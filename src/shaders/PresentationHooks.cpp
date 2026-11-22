#include "Runtime.h"
#ifdef _WIN32
#include "GlPipeline.h"
#include "GlTextureOverride.h"
#include "GlDrawCalls.h"
#include "GlDrawScope.h"
#include "PresentationHooks.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include "MinHook.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace BsiShaders {
namespace {
using WglSwap=BOOL(WINAPI*)(HDC);
using WglCurrent=HGLRC(WINAPI*)();
using WglCurrentDc=HDC(WINAPI*)();
using WglProc=PROC(WINAPI*)(LPCSTR);
using WglDelete=BOOL(WINAPI*)(HGLRC);
using WglCreate=HGLRC(WINAPI*)(HDC);
using WglShare=BOOL(WINAPI*)(HGLRC,HGLRC);
using WglCreateAttributes=HGLRC(WINAPI*)(HDC,HGLRC,const int*);
using EglSwap=unsigned(WINAPI*)(void*,void*);
using EglCreate=void*(WINAPI*)(void*,void*,void*,const int*);
using EglPlatformCreate=void*(WINAPI*)(void*,void*,void*,const std::intptr_t*);
using EglPbufferCreate=void*(WINAPI*)(void*,void*,const int*);
using EglDestroy=unsigned(WINAPI*)(void*,void*);
using EglCurrent=void*(WINAPI*)();
using EglCurrentSurface=void*(WINAPI*)(int);
using EglQuery=unsigned(WINAPI*)(void*,void*,int,int*);
using EglProc=void*(WINAPI*)(const char*);
using EglQueryApi=unsigned(WINAPI*)();
using EglString=const char*(WINAPI*)(void*,int);
constexpr unsigned maximumEglProviders=4,drawKinds=glDrawKinds,drawSlots=32;
static_assert(drawSlots>=(maximumEglProviders+1)*glDrawAliases,"bound all independent provider/alias targets");
using DrawableKey=std::pair<std::uintptr_t,std::uintptr_t>; // display + surface (or 0 + WGL DC)
using ContextKey=std::tuple<unsigned,std::uintptr_t,std::uintptr_t>; // provider + display + context
struct EglProvider {
    HMODULE module=nullptr;
    EglSwap swap=nullptr;EglCreate create=nullptr,createPlatformExt=nullptr,createContext=nullptr;
    EglPlatformCreate createPlatform=nullptr;EglPbufferCreate createPbuffer=nullptr;
    EglDestroy destroySurface=nullptr,destroyContext=nullptr;
    EglCurrent current=nullptr,currentDisplay=nullptr;EglCurrentSurface currentSurface=nullptr;
    EglQuery query=nullptr,queryContext=nullptr;EglProc proc=nullptr;EglQueryApi api=nullptr;EglString strings=nullptr;
    std::map<DrawableKey,HWND> surfaces;
    std::set<DrawableKey> pbuffers; // only successful, observed unprotected pbuffer creations
};
struct GameContext {
    GlPipeline pipeline;
    GlTextureOverride potato;
    Renderer renderer=Renderer::Unknown;
    HWND window=nullptr;
    DrawableKey drawable{};
    bool drawHooks=false,resourceHooks=false;
};
struct DrawHook {void *target=nullptr;std::atomic<void*> original{nullptr};std::atomic<unsigned> providers{0};};
struct ResourceEvent {
    enum Kind { Deleted, Created, CreatedShared, Shared } kind=Deleted;
    unsigned providers=0,count=0;
    GlContextIdentity parent{},child{};
    std::array<unsigned,64> names{};
};
struct Hooks {
    std::mutex mutex,renderMutex,surfaceMutex,hookMutex,resourceMutex,procedureMutex;
    std::atomic<bool> stopping{false};
    std::thread worker;
    bool started=false;
    HMODULE gl=nullptr;
    WglSwap swaps[2]{};
    WglCurrent currentWgl=nullptr;WglCurrentDc currentDc=nullptr;WglProc wglProc=nullptr;WglDelete deleteWgl=nullptr;
    WglCreate createWgl=nullptr;WglShare shareWgl=nullptr;WglProc nativeWglProc=nullptr;
    GlTextureProvenance provenance;
    std::array<ResourceEvent,32> resources{};unsigned resourceCount=0;std::atomic<unsigned> resourceReset{0};
    std::atomic<std::uint64_t> resourceSerial{0};
    std::array<DrawHook,12> textureDeletes{};
    std::array<DrawHook,16> attributedCreates{};
    std::array<EglProvider,maximumEglProviders> egl;
    unsigned eglCount=0;
    // 0 = WGL; each EGL provider has its own identity, procedures and lifecycle.
    std::map<ContextKey,std::unique_ptr<GameContext>> contexts;
    std::array<std::array<DrawHook,drawSlots>,drawKinds> draws;
    struct Detour {void *replacement;void *original;};
    std::map<void*,Detour> targets;
};
Hooks &hooks(){static auto *state=new Hooks;return *state;}
template<class T> T exportFunction(HMODULE module,const char *name){return reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(module,name)));}
thread_local bool presenting=false;
struct Recursion {bool previous;Recursion():previous(presenting){presenting=true;}~Recursion(){presenting=previous;}};
bool install(void *target,void *replacement,void **original);
bool installPublished(void *target,void *replacement,void **original,std::atomic<void*> *published);
void *resolve(const char *name,void *user){
    auto &h=hooks();void *address=nullptr;
    if(user){auto &p=*static_cast<EglProvider*>(user);if(p.proc)address=p.proc(name);if(!address&&p.module)address=reinterpret_cast<void*>(GetProcAddress(p.module,name));}
    else {
        if(h.wglProc)address=reinterpret_cast<void*>(h.wglProc(name));
        const auto value=reinterpret_cast<std::intptr_t>(address);
        if(!address||value==1||value==2||value==3||value==-1)address=h.gl?reinterpret_cast<void*>(GetProcAddress(h.gl,name)):nullptr;
    }
    return address;
}
// Creation/deletion callbacks never wait for renderMutex: a driver may join a
// worker while a draw holds it. Fixed events are consumed under that mutex.
// Contention/overflow invalidates the provider's evidence, never guesses it.
void resourceEvent(const ResourceEvent &event)noexcept{
    auto &h=hooks();if(h.stopping.load())return;
    try{
        std::unique_lock<std::mutex> lock(h.resourceMutex,std::try_to_lock);
        if(lock&&h.resourceCount<h.resources.size()){
            h.resources[h.resourceCount++]=event;h.resourceSerial.fetch_add(1);return;
        }
    }catch(...){}
    h.resourceReset.fetch_or(event.providers);h.resourceSerial.fetch_add(1);
}
void drainResources(){
    auto &h=hooks();std::array<ResourceEvent,32> pending{};unsigned count=0,reset=0;
    {
        std::lock_guard<std::mutex> lock(h.resourceMutex);count=h.resourceCount;
        std::copy_n(h.resources.begin(),count,pending.begin());h.resourceCount=0;
        reset=h.resourceReset.exchange(0);
    }
    for(unsigned provider=0;provider<=maximumEglProviders;++provider)
        if(reset&(1u<<provider))h.provenance.forgetProvider(provider);
    for(unsigned i=0;i<count;++i){const auto &event=pending[i];
        if(event.providers&reset)continue;
        if(event.kind==ResourceEvent::Deleted){
            for(unsigned provider=0;provider<=maximumEglProviders;++provider)
                if(event.providers&(1u<<provider))h.provenance.deleted(provider,int(event.count),event.names.data());
        }else{
            if(event.kind!=ResourceEvent::Shared)h.provenance.forget(event.child);
            if(event.kind==ResourceEvent::Shared||event.kind==ResourceEvent::CreatedShared)
                h.provenance.shared(event.parent,event.child);
            else h.provenance.remember(event.child);
        }
    }
}
void contextCreated(unsigned provider,std::uintptr_t display,std::uintptr_t context,std::uintptr_t share)noexcept{
    if(!context)return;
    ResourceEvent event;event.providers=1u<<provider;
    event.kind=share?ResourceEvent::CreatedShared:ResourceEvent::Created;
    event.parent={provider,display,share};event.child={provider,display,context};resourceEvent(event);
}
std::uintptr_t contextFor(unsigned provider){
    auto &h=hooks();return provider?reinterpret_cast<std::uintptr_t>(h.egl[provider-1].current()):h.currentWgl?reinterpret_cast<std::uintptr_t>(h.currentWgl()):0;
}
HWND drawableFor(unsigned provider,DrawableKey *key){
    auto &h=hooks();
    if(!provider){
        const auto dc=h.currentDc?h.currentDc():nullptr;
        *key={0,reinterpret_cast<std::uintptr_t>(dc)};return dc?WindowFromDC(dc):nullptr;
    }
    auto &p=h.egl[provider-1];if(!p.currentSurface||!p.currentDisplay)return nullptr;
    *key={reinterpret_cast<std::uintptr_t>(p.currentDisplay()),reinterpret_cast<std::uintptr_t>(p.currentSurface(0x3059))};
    // EGL_DRAW on the exact display, not an arbitrary context or another
    // surface which happens to use the same native HWND.
    std::lock_guard<std::mutex> lock(h.surfaceMutex);const auto it=p.surfaces.find(*key);
    return it==p.surfaces.end()?nullptr:it->second;
}
bool installDrawHooks(unsigned provider,void *user);
bool installResourceHooks(unsigned provider,void *user);
bool extension(const char *list,const char *name);
Renderer rendererFor(unsigned provider){
    if(!provider)return Renderer::OpenGL;
    auto &p=hooks().egl[provider-1];const auto api=p.api?p.api():0;
    return api==0x30a0u?Renderer::OpenGLES:api==0x30a2u?Renderer::OpenGL:Renderer::Unknown;
}
bool unprotectedEgl(EglProvider &p,DrawableKey drawable,std::uintptr_t context,bool *externalProtectionPossible){
    const auto display=reinterpret_cast<void*>(drawable.first),surface=reinterpret_cast<void*>(drawable.second);
    if(p.currentDisplay()!=display||p.currentSurface(0x3059)!=surface
       ||reinterpret_cast<std::uintptr_t>(p.current())!=context)return false;
    const char *extensions=p.strings(display,0x3055); // EGL_EXTENSIONS
    if(!extensions)return false;
    const bool content=extension(extensions,"EGL_EXT_protected_content");
    const bool surfaces=content||extension(extensions,"EGL_EXT_protected_surface");
    int protectedSurface=0,protectedContext=0;
    if(externalProtectionPossible)*externalProtectionPossible=surfaces;
    return (!surfaces||(p.query(display,surface,0x32c0,&protectedSurface)&&!protectedSurface))
        &&(!content||(p.queryContext(display,reinterpret_cast<void*>(context),0x32c0,&protectedContext)&&!protectedContext));
}
// A draw can use the exact selected drawable, or (compatibility only) write a
// recently presented color-texture dependency in its same/observed-share
// namespace. Another window/default framebuffer, unknown pbuffer, mere sharing
// and equal numeric object names never select or authorize a presentation.
struct GameDraw {
    GlDrawScope scope;
    GameContext *selected=nullptr;
    GameDraw(unsigned providers,unsigned primitive)noexcept:scope(hooks().renderMutex,presenting){
        try {
            auto &h=hooks();auto &runtime=Runtime::instance();
            if(!scope.mayInspect()||h.stopping.load()||!runtime.texture().enabled)return;
            if(!scope.tryLock())return;
            drainResources();const auto route=runtime.textureRoute();const auto quality=route.configuration;
            h.provenance.synchronize(route.object,route.revision);
            if(!quality.enabled||!route.object||!runtime.eligible(route.window)){scope.releaseBypass();return;}
            for(unsigned provider=0;provider<=maximumEglProviders;++provider){
                if(!(providers&(1u<<provider)))continue;
                const auto context=contextFor(provider);if(!context)continue;
                const auto renderer=rendererFor(provider);if(renderer!=route.renderer)continue;
                GameContext *source=nullptr;
                for(const auto &entry:h.contexts)
                    if(std::get<0>(entry.first)==provider&&reinterpret_cast<std::uintptr_t>(entry.second.get())==route.object){source=entry.second.get();break;}
                if(!source||!source->drawHooks||!runtime.isPresentation(renderer,route.object,route.window))continue;
                DrawableKey drawable{};const HWND window=drawableFor(provider,&drawable);
                const bool exact=source->window==window&&source->drawable==drawable;
                const GlContextIdentity identity{provider,drawable.first,context};
                if(!exact){
                    if(quality.drawPolicy!=TextureDrawPolicy::OwnedMipCompatibility||!quality.externalOverridesReviewed
                       ||!source->resourceHooks||!h.provenance.hasDependencies(identity))continue;
                    if(window){if(window!=source->window)continue;}
                    else {
                        // Only an observed unprotected EGL pbuffer is a known
                        // non-window route. Unmapped/pre-existing/foreign
                        // windows, surfaceless and unknown WGL DCs stay native.
                        if(!provider)continue;
                        std::lock_guard<std::mutex> lock(h.surfaceMutex);
                        if(!h.egl[provider-1].pbuffers.count(drawable))continue;
                    }
                }
                auto *egl=provider?&h.egl[provider-1]:nullptr;bool protectionPossible=false;
                if(egl&&!unprotectedEgl(*egl,drawable,context,&protectionPossible))continue;
                const ContextKey key{provider,drawable.first,context};auto found=h.contexts.find(key);
                if(found==h.contexts.end()){
                    if(h.contexts.size()>=16)continue;
                    found=h.contexts.emplace(key,std::make_unique<GameContext>()).first;
                }
                auto &session=*found->second;
                // A producer never overwrites the presenter's window/drawable
                // association, even when one context performs both roles.
                if(exact){session.window=window;session.drawable=drawable;}
                session.renderer=renderer;selected=&session;scope.guardInspection();
                std::string detail;
                if(!session.drawHooks){
                    session.drawHooks=session.potato.initialize(resolve,egl,&detail,protectionPossible)&&installDrawHooks(provider,egl);
                    if(!session.drawHooks){
                        runtime.textureReport(0,detail.empty()?"This game context has no safe sampler-inspection path; native sampling retained.":detail,false,
                                              TextureDrawReason::NotInspected,TextureDepthFailure::None,scope.nested(),!exact,route.revision);
                        selected=nullptr;scope.releaseBypass();return;
                    }
                }
                if(quality.drawPolicy==TextureDrawPolicy::OwnedMipCompatibility&&quality.externalOverridesReviewed){
                    if(!session.resourceHooks)session.resourceHooks=installResourceHooks(provider,egl);
                    const auto serial=h.resourceSerial.load();GlTextureUsage usage;
                    const bool captured=session.resourceHooks&&(primitive==4||primitive==5||primitive==6)&&session.potato.usage(&usage)
                        &&serial==h.resourceSerial.load();
                    if(!exact&&(!captured||!h.provenance.producer(identity,usage))){selected=nullptr;scope.releaseBypass();return;}
                    if(captured){
                        const auto inputs=h.provenance.observe(identity,usage,exact);
                        if(exact&&usage.defaultFramebuffer)runtime.textureConsumerReport(inputs,route.revision);
                    }
                }
                if(runtime.textureRoute().revision!=route.revision){selected=nullptr;scope.releaseBypass();return;}
                TextureDrawReason reason=TextureDrawReason::NotInspected;TextureDepthFailure depth=TextureDepthFailure::None;
                const auto changed=selected->potato.begin(quality.quality(),quality.externalOverridesReviewed,primitive,&detail,&reason,&depth,quality.drawPolicy);
                if(!exact)detail="Presentation-linked color-FBO producer: "+detail;
                runtime.textureReport(changed,detail,true,reason,depth,scope.nested(),!exact,route.revision);
                if(!changed){
                    // Zero-binding wrappers release their leases BEFORE the
                    // original, allowing its later real nested draw inspection.
                    selected=nullptr;scope.releaseBypass();
                }
                return;
            }
            scope.releaseBypass();
        }catch(...){
            // Even an allocation failure BEFORE selecting/initializing a
            // context must fail closed around the original. Idempotent when
            // the inspection/diagnostic that failed already owns the guard.
            scope.guardInspection();
            // A failed inspection must not change the original draw. begin()
            // already rolls back; also handle an exception in diagnostics.
            if(selected)selected->potato.end();
            try{Runtime::instance().textureReport(0,"Potato inspection failed; native game sampler bindings retained.",false);}catch(...){}
        }
    }
    ~GameDraw(){if(selected)selected->potato.end();} // restore bindings BEFORE scope releases its guard/lock
};
// Preserve every argument and call the original once, including bypasses.
// The RAII guard brackets a whole multi/indirect API call; it never reads or
// rewrites a CPU/GPU command buffer, index offset, instance count or base vertex.
#define BSI_DRAW_CALLBACK(id,params,args,...) \
    template<unsigned Index> void WINAPI draw##id params { \
        auto &d=hooks().draws[static_cast<unsigned>(GlDrawKind::id)][Index]; \
        GameDraw guard(d.providers.load(std::memory_order_acquire),mode); \
        using Function=void(WINAPI*)params; \
        reinterpret_cast<Function>(d.original.load(std::memory_order_acquire)) args; \
    } \
    template<std::size_t... Index> std::array<void*,drawSlots> callbacks##id(std::index_sequence<Index...>){ \
        return {{reinterpret_cast<void*>(&draw##id<Index>)...}}; \
    }
BSI_GL_DRAW_CALLS(BSI_DRAW_CALLBACK)
#undef BSI_DRAW_CALLBACK
const std::array<std::array<void*,drawSlots>,drawKinds> drawCallbacks{{
#define BSI_DRAW_CALLBACK_ROW(id,params,args,...) callbacks##id(std::make_index_sequence<drawSlots>{}),
    BSI_GL_DRAW_CALLS(BSI_DRAW_CALLBACK_ROW)
#undef BSI_DRAW_CALLBACK_ROW
}};
#undef BSI_GL_DRAW_CALLS
template<unsigned Index> void WINAPI deletedTextures(int count,const unsigned *names){
    auto &h=hooks();auto &hook=h.textureDeletes[Index];const auto providers=hook.providers.load(std::memory_order_acquire);
    if(count>0&&!h.stopping.load()){
        if(!names||count>64){h.resourceReset.fetch_or(providers);h.resourceSerial.fetch_add(1);}
        else {ResourceEvent event;event.providers=providers;event.count=unsigned(count);std::copy_n(names,count,event.names.begin());resourceEvent(event);}
    }
    using Function=void(WINAPI*)(int,const unsigned*);
    reinterpret_cast<Function>(hook.original.load(std::memory_order_acquire))(count,names);
}
template<std::size_t... Index> auto deleteCallbacks(std::index_sequence<Index...>){
    return std::array<void*,sizeof...(Index)>{{reinterpret_cast<void*>(&deletedTextures<Index>)...}};
}
template<std::size_t N> bool resourceHook(std::array<DrawHook,N> &slots,const std::array<void*,N> &callbacks,void *target,unsigned providers){
    if(!target)return false;
    std::lock_guard<std::mutex> lock(hooks().procedureMutex);
    for(std::size_t i=0;i<N;++i){auto &entry=slots[i];
        if(entry.target==target){entry.providers.fetch_or(providers,std::memory_order_release);return entry.original.load()!=nullptr;}
        if(entry.target)continue;
        entry.target=target;entry.providers.store(providers,std::memory_order_release);
        if(installPublished(target,callbacks[i],nullptr,&entry.original))return true;
        entry.providers.store(0);entry.target=nullptr;return false;
    }
    return false;
}
bool installResourceHooks(unsigned provider,void *user){
    static const auto callbacks=deleteCallbacks(std::make_index_sequence<12>{});
    return resourceHook(hooks().textureDeletes,callbacks,resolve("glDeleteTextures",user),1u<<provider);
}
template<unsigned Index> HGLRC WINAPI createAttributedWgl(HDC dc,HGLRC share,const int *attributes){
    auto &entry=hooks().attributedCreates[Index];
    const auto context=reinterpret_cast<WglCreateAttributes>(entry.original.load(std::memory_order_acquire))(dc,share,attributes);
    contextCreated(0,0,reinterpret_cast<std::uintptr_t>(context),reinterpret_cast<std::uintptr_t>(share));return context;
}
template<std::size_t... Index> auto attributedCallbacks(std::index_sequence<Index...>){
    return std::array<void*,sizeof...(Index)>{{reinterpret_cast<void*>(&createAttributedWgl<Index>)...}};
}
PROC WINAPI queriedWgl(LPCSTR name){
    const auto result=hooks().nativeWglProc(name);
    if(result&&name&&std::strcmp(name,"wglCreateContextAttribsARB")==0)try{
        const auto value=reinterpret_cast<std::intptr_t>(result);
        if(value!=1&&value!=2&&value!=3&&value!=-1){
            static const auto callbacks=attributedCallbacks(std::make_index_sequence<16>{});
            resourceHook(hooks().attributedCreates,callbacks,reinterpret_cast<void*>(result),1);
        }
    }catch(...){} // a failed optional detour never changes the returned native address
    return result;
}
HGLRC WINAPI createdWgl(HDC dc){
    const auto context=hooks().createWgl(dc);contextCreated(0,0,reinterpret_cast<std::uintptr_t>(context),0);return context;
}
BOOL WINAPI sharedWgl(HGLRC parent,HGLRC child){
    const auto result=hooks().shareWgl(parent,child);
    if(result){ResourceEvent event;event.kind=ResourceEvent::Shared;event.providers=1;
        event.parent={0,0,reinterpret_cast<std::uintptr_t>(parent)};event.child={0,0,reinterpret_cast<std::uintptr_t>(child)};resourceEvent(event);}
    return result;
}
bool installDrawHooks(unsigned provider,void *user){
    bool any=false;auto &h=hooks();
    for(unsigned kind=0;kind<drawKinds;++kind){
        for(const auto *name:glDrawNames[kind]){
            if(!name)continue;
            void *target=resolve(name,user);if(!target)continue;
            for(unsigned slot=0;slot<drawSlots;++slot){
                auto &d=h.draws[kind][slot];
                if(d.target==target){d.providers.fetch_or(1u<<provider,std::memory_order_release);any=d.original.load()!=nullptr||any;break;}
                if(d.target)continue;
                d.target=target;d.providers.store(1u<<provider,std::memory_order_release);
                if(installPublished(target,drawCallbacks[kind][slot],nullptr,&d.original))any=true;
                else {d.providers.store(0);d.target=nullptr;}
                break;
            }
        }
    }
    return any;
}
void process(unsigned provider,std::uintptr_t context,HWND window,DrawableKey drawable,int width,int height,bool externalProtectionPossible=false){
    auto &h=hooks();if(!context||presenting||h.stopping.load())return;
    auto *egl=provider?&h.egl[provider-1]:nullptr;
    const auto api=egl&&egl->api?egl->api():0x30a2u;
    if(egl&&api!=0x30a0u&&api!=0x30a2u)return;
    const auto renderer=api==0x30a0u?Renderer::OpenGLES:Renderer::OpenGL;
    if(!Runtime::instance().accepts(renderer))return;
    std::unique_lock<std::mutex> lock(h.renderMutex,std::try_to_lock);if(!lock.owns_lock())return;
    Recursion guard;
    try {
        drainResources();
        const ContextKey key{provider,drawable.first,context};auto it=h.contexts.find(key);
        if(it==h.contexts.end()){
            if(h.contexts.size()>=16){Runtime::instance().report(true,true,false,"Too many game contexts; shaders safely bypassed.",false,renderer);return;}
            it=h.contexts.emplace(key,std::make_unique<GameContext>()).first;
        }
        auto &session=*it->second;
        const auto token=reinterpret_cast<std::uintptr_t>(&session);
        if((session.window!=window||session.drawable!=drawable)
           &&Runtime::instance().isPresentation(session.renderer,token,reinterpret_cast<std::uintptr_t>(session.window)))
            Runtime::instance().releasePresentation(session.renderer,token);
        session.window=window;session.drawable=drawable;session.renderer=renderer;
        if(!Runtime::instance().claimPresentation(renderer,token,reinterpret_cast<std::uintptr_t>(window)))return;
        std::string error;
        const auto route=Runtime::instance().textureRoute();const auto quality=route.configuration;
        h.provenance.synchronize(route.object,route.revision);
        h.provenance.remember({provider,drawable.first,context});h.provenance.nextFrame();
        if(quality.enabled){
            if(!session.drawHooks){
                const bool ready=session.potato.initialize(resolve,egl,&error,externalProtectionPossible);
                session.drawHooks=ready&&installDrawHooks(provider,egl);
                const char *armed=quality.drawPolicy==TextureDrawPolicy::OwnedMipCompatibility
                    ?"Depth-independent Potato compatibility is armed on the selected game drawable. Waiting for eligible mip-color bindings; no texture change is confirmed yet."
                    :"Potato is armed for subsequent qualified scene draws on the identified game context. No sampler override has been counted yet.";
                Runtime::instance().textureReport(0,session.drawHooks?armed:error.empty()?"Potato draw hooks could not be attached; native texture sampling retained.":error,session.drawHooks);
            }
            if(session.drawHooks&&quality.externalOverridesReviewed&&quality.drawPolicy==TextureDrawPolicy::OwnedMipCompatibility&&!session.resourceHooks)
                session.resourceHooks=installResourceHooks(provider,egl);
        }
        const auto p=Runtime::instance().settings();
        if(p.neutral()){Runtime::instance().report(true,true,false,"Neutral bypass",false,renderer);return;}
        const bool ready=session.pipeline.initialize(resolve,egl,&error);
        const bool rendered=ready&&session.pipeline.render(width,height,p,&error);
        Runtime::instance().report(true,true,rendered,rendered?"Shaders active on the identified native OpenGL/GLES game surface.":error,rendered,renderer,rendered);
    }catch(...){try{Runtime::instance().report(true,true,false,"Shader processing failed; original presentation continues.",false,renderer);}catch(...){}}
}
void processWgl(HDC dc){
    auto &h=hooks();if(presenting||h.stopping.load()||!h.currentWgl||!h.currentDc||h.currentDc()!=dc)return;
    const HWND window=WindowFromDC(dc);if(!Runtime::instance().eligible(reinterpret_cast<std::uintptr_t>(window)))return;
    RECT rect{};if(GetClientRect(window,&rect))process(0,reinterpret_cast<std::uintptr_t>(h.currentWgl()),window,{0,reinterpret_cast<std::uintptr_t>(dc)},rect.right,rect.bottom);
}
template<int Index> BOOL WINAPI swapWgl(HDC dc){try{processWgl(dc);}catch(...){}Recursion guard;return hooks().swaps[Index](dc);}
void removeContextState(unsigned provider,std::uintptr_t display,std::uintptr_t context){
    auto &h=hooks();std::lock_guard<std::mutex> lock(h.renderMutex);drainResources();
    h.provenance.forget({provider,display,context});const auto it=h.contexts.find({provider,display,context});
    if(it==h.contexts.end())return;
    Runtime::instance().releasePresentation(it->second->renderer,reinterpret_cast<std::uintptr_t>(it->second.get()));h.contexts.erase(it);
}
BOOL WINAPI removeWgl(HGLRC context){const BOOL okay=hooks().deleteWgl(context);if(okay)try{removeContextState(0,0,reinterpret_cast<std::uintptr_t>(context));}catch(...){}return okay;}
template<typename Attribute> void trackSurface(EglProvider &p,void *display,void *surface,void *window,const Attribute *attrs)noexcept{
    try {
        bool protectedContent=false,terminated=attrs==nullptr;
        if(attrs)for(unsigned i=0;i<128;i+=2){
            if(attrs[i]==0x3038){terminated=true;break;} // EGL_NONE
            if(attrs[i]==0x32c0&&attrs[i+1])protectedContent=true;
        }
        if(surface&&window&&terminated&&!protectedContent&&IsWindow(reinterpret_cast<HWND>(window))){
            std::lock_guard<std::mutex> lock(hooks().surfaceMutex);
            if(p.surfaces.size()<128)p.surfaces[{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(surface)}]=reinterpret_cast<HWND>(window);
        }
    }catch(...){} // Native creation succeeded; untracked is safer than guessing.
}
template<unsigned Index> void *WINAPI createSurface(void *display,void *config,void *window,const int *attrs){
    auto &p=hooks().egl[Index];void *surface=p.create(display,config,window,attrs);trackSurface(p,display,surface,window,attrs);return surface;
}
template<unsigned Index> void *WINAPI createPlatformSurface(void *display,void *config,void *window,const std::intptr_t *attrs){
    auto &p=hooks().egl[Index];void *surface=p.createPlatform(display,config,window,attrs);trackSurface(p,display,surface,window,attrs);return surface;
}
template<unsigned Index> void *WINAPI createPlatformSurfaceExt(void *display,void *config,void *window,const int *attrs){
    auto &p=hooks().egl[Index];void *surface=p.createPlatformExt(display,config,window,attrs);trackSurface(p,display,surface,window,attrs);return surface;
}
template<unsigned Index> void *WINAPI createdEglContext(void *display,void *config,void *share,const int *attributes){
    auto &p=hooks().egl[Index];const auto context=p.createContext(display,config,share,attributes);
    contextCreated(Index+1,reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(context),reinterpret_cast<std::uintptr_t>(share));return context;
}
template<unsigned Index> void *WINAPI createdPbuffer(void *display,void *config,const int *attributes){
    auto &h=hooks();auto &p=h.egl[Index];const auto surface=p.createPbuffer(display,config,attributes);
    if(surface)try{
        bool unprotected=true,terminated=attributes==nullptr;
        if(attributes)for(unsigned i=0;i<128;i+=2){
            if(attributes[i]==0x3038){terminated=true;break;}
            if(attributes[i]==0x32c0&&attributes[i+1])unprotected=false;
        }
        if(unprotected&&terminated){std::lock_guard<std::mutex> lock(h.surfaceMutex);
            if(p.pbuffers.size()<128)p.pbuffers.emplace(reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(surface));}
    }catch(...){}
    return surface;
}
template<unsigned Index> unsigned WINAPI removeSurface(void *display,void *surface){
    auto &h=hooks();auto &p=h.egl[Index];const auto okay=p.destroySurface(display,surface);
    if(okay){try{
        const DrawableKey drawable{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(surface)};
        {std::lock_guard<std::mutex> lock(h.surfaceMutex);p.surfaces.erase(drawable);p.pbuffers.erase(drawable);}
        std::lock_guard<std::mutex> lock(h.renderMutex);for(const auto &entry:h.contexts)
            if(std::get<0>(entry.first)==Index+1&&entry.second->drawable==drawable){
                Runtime::instance().releasePresentation(entry.second->renderer,reinterpret_cast<std::uintptr_t>(entry.second.get()));
                entry.second->window=nullptr;entry.second->drawable={};
            }
    }catch(...){}}
    return okay;
}
template<unsigned Index> unsigned WINAPI removeContext(void *display,void *context){
    const auto okay=hooks().egl[Index].destroyContext(display,context);
    if(okay)try{removeContextState(Index+1,reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(context));}catch(...){}return okay;
}
bool extension(const char *list,const char *name){
    if(!list)return false;
    const auto size=std::strlen(name);const char *found=list;
    while((found=std::strstr(found,name))){
        if((found==list||found[-1]==' ')&&(found[size]==' '||found[size]=='\0'))return true;
        found+=size;
    }return false;
}
template<unsigned Index> unsigned WINAPI swapEgl(void *display,void *surface){
    auto &h=hooks();auto &p=h.egl[Index];
    if(!presenting&&!h.stopping.load())try{
        HWND window=nullptr;
        const DrawableKey drawable{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(surface)};
        {std::lock_guard<std::mutex> lock(h.surfaceMutex);const auto it=p.surfaces.find(drawable);if(it!=p.surfaces.end())window=it->second;}
        const auto context=p.current();
        if(Runtime::instance().eligible(reinterpret_cast<std::uintptr_t>(window))&&context
           &&p.currentDisplay()==display&&p.currentSurface(0x3059)==surface){
            // Apply the SAME legal protection/current-drawable checks to swap
            // and alternate scene contexts before any optional GL inspection.
            bool protectionPossible=false;int width=0,height=0;
            if(unprotectedEgl(p,drawable,reinterpret_cast<std::uintptr_t>(context),&protectionPossible)
               &&p.query(display,surface,0x3057,&width)&&p.query(display,surface,0x3056,&height))
                process(Index+1,reinterpret_cast<std::uintptr_t>(context),window,drawable,width,height,protectionPossible);
        }
    }catch(...){} // Optional effects must never prevent the native present.
    Recursion guard;return p.swap(display,surface);
}
bool install(void *target,void *replacement,void **original){return installPublished(target,replacement,original,nullptr);}
bool installPublished(void *target,void *replacement,void **original,std::atomic<void*> *published){
    if(!target||!replacement)return false;
    auto &h=hooks();std::lock_guard<std::mutex> lock(h.hookMutex);if(h.stopping.load())return false;
    const auto found=h.targets.find(target);
    if(found!=h.targets.end()){if(found->second.replacement!=replacement)return false;if(original)*original=found->second.original;if(published)published->store(found->second.original,std::memory_order_release);return true;}
    // A host may unload/reload its renderer. Keep installed code modules pinned
    // rather than leaving a trampoline into unmapped provider/callback code.
    HMODULE owner=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(target),&owner))return false;
    void *trampoline=nullptr;if(MH_CreateHook(target,replacement,&trampoline)!=MH_OK)return false;
    try{h.targets.emplace(target,Hooks::Detour{replacement,trampoline});}catch(...){MH_RemoveHook(target);throw;}
    if(original)*original=trampoline;
    if(published)published->store(trampoline,std::memory_order_release); // BEFORE enabling a draw callback.
    if(MH_EnableHook(target)!=MH_OK){h.targets.erase(target);MH_RemoveHook(target);if(original)*original=nullptr;if(published)published->store(nullptr);return false;}return true;
}
template<unsigned Index> void attachEgl(HMODULE module){
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(module),&pinned))return;
    auto &p=hooks().egl[Index];p.module=pinned;
    p.current=exportFunction<EglCurrent>(module,"eglGetCurrentContext");p.currentSurface=exportFunction<EglCurrentSurface>(module,"eglGetCurrentSurface");
    p.currentDisplay=exportFunction<EglCurrent>(module,"eglGetCurrentDisplay");p.queryContext=exportFunction<EglQuery>(module,"eglQueryContext");
    p.query=exportFunction<EglQuery>(module,"eglQuerySurface");p.proc=exportFunction<EglProc>(module,"eglGetProcAddress");p.api=exportFunction<EglQueryApi>(module,"eglQueryAPI");
    p.strings=exportFunction<EglString>(module,"eglQueryString");
    // Publish a complete stable provider table before enabling any lifecycle
    // callback. A partial provider is never retargeted to another DLL.
    const bool created=install(reinterpret_cast<void*>(GetProcAddress(module,"eglCreateWindowSurface")),reinterpret_cast<void*>(&createSurface<Index>),reinterpret_cast<void**>(&p.create));
    const bool surfaces=install(reinterpret_cast<void*>(GetProcAddress(module,"eglDestroySurface")),reinterpret_cast<void*>(&removeSurface<Index>),reinterpret_cast<void**>(&p.destroySurface));
    const bool contexts=install(reinterpret_cast<void*>(GetProcAddress(module,"eglDestroyContext")),reinterpret_cast<void*>(&removeContext<Index>),reinterpret_cast<void**>(&p.destroyContext));
    if(created&&surfaces&&contexts){
        install(reinterpret_cast<void*>(GetProcAddress(module,"eglCreateContext")),reinterpret_cast<void*>(&createdEglContext<Index>),reinterpret_cast<void**>(&p.createContext));
        install(reinterpret_cast<void*>(GetProcAddress(module,"eglCreatePbufferSurface")),reinterpret_cast<void*>(&createdPbuffer<Index>),reinterpret_cast<void**>(&p.createPbuffer));
        const auto optional=[&](const char *name){void *entry=reinterpret_cast<void*>(GetProcAddress(module,name));return entry?entry:p.proc?p.proc(name):nullptr;};
        install(optional("eglCreatePlatformWindowSurface"),reinterpret_cast<void*>(&createPlatformSurface<Index>),reinterpret_cast<void**>(&p.createPlatform));
        install(optional("eglCreatePlatformWindowSurfaceEXT"),reinterpret_cast<void*>(&createPlatformSurfaceExt<Index>),reinterpret_cast<void**>(&p.createPlatformExt));
        install(reinterpret_cast<void*>(GetProcAddress(module,"eglSwapBuffers")),reinterpret_cast<void*>(&swapEgl<Index>),reinterpret_cast<void**>(&p.swap));
    }
}
void scan(){
    auto &h=hooks();
    if(!h.gl){h.gl=GetModuleHandleW(L"opengl32.dll");if(h.gl){h.currentWgl=exportFunction<WglCurrent>(h.gl,"wglGetCurrentContext");h.currentDc=exportFunction<WglCurrentDc>(h.gl,"wglGetCurrentDC");h.wglProc=exportFunction<WglProc>(h.gl,"wglGetProcAddress");}}
    if(h.gl&&h.currentWgl&&h.currentDc&&h.wglProc){
        if(!h.nativeWglProc)install(reinterpret_cast<void*>(GetProcAddress(h.gl,"wglGetProcAddress")),reinterpret_cast<void*>(&queriedWgl),reinterpret_cast<void**>(&h.nativeWglProc));
        if(!h.createWgl)install(reinterpret_cast<void*>(GetProcAddress(h.gl,"wglCreateContext")),reinterpret_cast<void*>(&createdWgl),reinterpret_cast<void**>(&h.createWgl));
        if(!h.shareWgl)install(reinterpret_cast<void*>(GetProcAddress(h.gl,"wglShareLists")),reinterpret_cast<void*>(&sharedWgl),reinterpret_cast<void**>(&h.shareWgl));
        if(!h.deleteWgl)install(reinterpret_cast<void*>(GetProcAddress(h.gl,"wglDeleteContext")),reinterpret_cast<void*>(&removeWgl),reinterpret_cast<void**>(&h.deleteWgl));
        void *candidates[]{reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"gdi32.dll"),"SwapBuffers")),reinterpret_cast<void*>(GetProcAddress(h.gl,"wglSwapBuffers"))};
        void *callbacks[]{reinterpret_cast<void*>(&swapWgl<0>),reinterpret_cast<void*>(&swapWgl<1>)};
        if(h.deleteWgl)for(int i=0;i<2;++i)if(!h.swaps[i])install(candidates[i],callbacks[i],reinterpret_cast<void**>(&h.swaps[i]));
    }
    // Qt's first EGL DLL is often NOT the game's provider. Discover each
    // complete public contract; never stop scanning after a UI-only provider.
    HMODULE modules[512]{};DWORD required=0;
    if(EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&required)){
        const auto count=(std::min)(std::size_t(required/sizeof(HMODULE)),std::size_t(512));
        for(std::size_t m=0;m<count&&h.eglCount<maximumEglProviders;++m){
            const auto module=modules[m];const auto swap=GetProcAddress(module,"eglSwapBuffers");if(!swap)continue;
            bool known=false;for(unsigned i=0;i<h.eglCount;++i)if(h.egl[i].module==module||GetProcAddress(h.egl[i].module,"eglSwapBuffers")==swap)known=true;
            if(known)continue;
            const char *names[]{"eglCreateWindowSurface","eglGetCurrentContext","eglQuerySurface","eglGetProcAddress","eglDestroySurface","eglDestroyContext","eglQueryAPI","eglGetCurrentSurface","eglGetCurrentDisplay","eglQueryContext","eglQueryString"};
            bool complete=true;for(const auto *name:names)complete=GetProcAddress(module,name)&&complete;if(!complete)continue;
            switch(h.eglCount){case 0:attachEgl<0>(module);break;case 1:attachEgl<1>(module);break;case 2:attachEgl<2>(module);break;case 3:attachEgl<3>(module);break;default:break;}++h.eglCount;
        }
    }
    const bool dxgi=scanDxgiHooks(install),vulkan=scanVulkanHooks(install);
    bool attached=h.swaps[0]||h.swaps[1]||dxgi||vulkan;for(const auto &p:h.egl)attached=p.swap||attached;
    Runtime::instance().waiting(attached,attached?"Automatically waiting for the owned game's actual WGL/EGL, DirectX or Vulkan presentation, regardless of the emulator renderer setting.":"Waiting for the game's presentation API; no eligible backend has been observed.");
}
}
void *originalPresentationFunction(void *target){auto &h=hooks();std::lock_guard<std::mutex> lock(h.hookMutex);const auto it=h.targets.find(target);return it==h.targets.end()?nullptr:it->second.original;}
void startPresentationHooks(){
    auto &h=hooks();std::lock_guard<std::mutex> lock(h.mutex);if(h.started)return;h.started=true;
    HMODULE self=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&startPresentationHooks),&self)){Runtime::instance().report(false,false,false,"Could not pin shader callback code; hooks not installed.");return;}
    if(MH_Initialize()!=MH_OK){Runtime::instance().report(false,false,false,"Could not initialize presentation hooks.");return;}
    // Prime all supported public host API loaders before root bootstrap returns.
    // Loading is NOT observed-game evidence and never selects a processing API.
    // This closes the config-mismatch/LoadLibrary-to-next-poll lifecycle gap.
    // Honor application/user DLL directories for Vulkan, like the native player;
    // OS GL/DX loaders stay System32-only. Never search cwd/replace any system DLL.
    const auto requested=Runtime::instance().metrics().requested;
    if(requested==RendererFamily::Automatic||requested==RendererFamily::Vulkan)LoadLibraryExW(L"vulkan-1.dll",nullptr,LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(requested==RendererFamily::Automatic||requested==RendererFamily::DirectX){LoadLibraryExW(L"dxgi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);LoadLibraryExW(L"d3d11.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);}
    if(requested==RendererFamily::Automatic||requested==RendererFamily::OpenGL)LoadLibraryExW(L"opengl32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    try {
        scan();h.worker=std::thread([]{while(!hooks().stopping.load()){try{scan();}catch(...){}for(int i=0;i<15&&!hooks().stopping.load();++i)std::this_thread::sleep_for(std::chrono::milliseconds(100));}});
    }catch(...){Runtime::instance().report(false,false,false,"Presentation-hook initialization was incomplete; unsupported/unidentified surfaces remain untouched.");}
}
void stopPresentationHooks(){
    auto &h=hooks();std::lock_guard<std::mutex> lock(h.mutex);h.stopping.store(true);if(h.worker.joinable())h.worker.join();
    // Pinned trampolines are never freed while a host thread could return through
    // one. Resource ownership is released by native context/device teardown.
    stopDxgiHooks();stopVulkanHooks();std::lock_guard<std::mutex> hooksLock(h.hookMutex);for(const auto &entry:h.targets)MH_DisableHook(entry.first);
}
} // namespace BsiShaders
#endif
