#include "Runtime.h"
#ifdef _WIN32
#include "GlPipeline.h"
#include "GlTextureOverride.h"
#include "GlDrawCalls.h"
#include "GlDrawScope.h"
#include "WglPbufferRegistry.h"
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
using WglPbufferGet=HDC(WINAPI*)(void*);
using WglPbufferRelease=int(WINAPI*)(void*,HDC);
using WglPbufferDestroy=BOOL(WINAPI*)(void*);
using WglBindTexture=BOOL(WINAPI*)(void*,int);
using WglPbufferQuery=BOOL(WINAPI*)(void*,int,int*);
using EglSwap=unsigned(WINAPI*)(void*,void*);
using EglBindTexture=unsigned(WINAPI*)(void*,void*,int);
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
    EglSwap swap=nullptr;EglBindTexture bindTexture=nullptr;EglCreate create=nullptr,createPlatformExt=nullptr,createContext=nullptr;
    EglPlatformCreate createPlatform=nullptr;EglPbufferCreate createPbuffer=nullptr;
    EglDestroy destroySurface=nullptr,destroyContext=nullptr;
    EglCurrent current=nullptr,currentDisplay=nullptr;EglCurrentSurface currentSurface=nullptr;
    EglQuery query=nullptr,queryContext=nullptr;EglProc proc=nullptr;EglQueryApi api=nullptr;EglString strings=nullptr;
    std::map<DrawableKey,HWND> surfaces;
    std::map<DrawableKey,std::uintptr_t> pbuffers; // observed unprotected pbuffer creation -> unique lifetime
    std::uintptr_t nextPbuffer=1;
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
    enum Kind { Deleted, RenderbuffersDeleted, PbufferRetired, Created, CreatedShared, Shared } kind=Deleted;
    unsigned providers=0,count=0;
    std::uintptr_t pbuffer=0;
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
    std::array<ResourceEvent,32> resources{};unsigned resourceCount=0;
    std::atomic<unsigned> resourceReset{0},resourceInvalidate{0};
    std::atomic<std::uint64_t> resourceSerial{0};
    std::uint64_t drainedResourceSerial=0; // renderMutex: evidence includes events only through this serial
    std::array<DrawHook,12> textureDeletes{},renderbufferDeletes{};
    std::array<DrawHook,32> blits{},copies{};
    std::array<DrawHook,16> attributedCreates{},pbufferGets{},pbufferReleases{},pbufferDestroys{},pbufferBinds{};
    WglPbufferRegistry wglPbuffers;
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
    // A missed object retirement invalidates resource evidence, NOT an
    // already observed context-share relationship. Losing a creation/share
    // event can hide handle reuse, so THAT still forgets namespaces as well.
    const bool objects=event.kind==ResourceEvent::Deleted||event.kind==ResourceEvent::RenderbuffersDeleted||event.kind==ResourceEvent::PbufferRetired;
    (objects?h.resourceInvalidate:h.resourceReset).fetch_or(event.providers);h.resourceSerial.fetch_add(1);
}
void drainResources(){
    auto &h=hooks();std::array<ResourceEvent,32> pending{};unsigned count=0,reset=0,invalidate=0;std::uint64_t serial=0;
    {
        std::lock_guard<std::mutex> lock(h.resourceMutex);serial=h.resourceSerial.load();count=h.resourceCount;
        std::copy_n(h.resources.begin(),count,pending.begin());h.resourceCount=0;
        reset=h.resourceReset.exchange(0);invalidate=h.resourceInvalidate.exchange(0);
    }
    for(unsigned provider=0;provider<=maximumEglProviders;++provider){
        if(reset&(1u<<provider))h.provenance.forgetProvider(provider);
        else if(invalidate&(1u<<provider))h.provenance.invalidate(provider);
    }
    for(unsigned i=0;i<count;++i){const auto &event=pending[i];
        if(event.providers&reset)continue;
        if(event.kind==ResourceEvent::Deleted||event.kind==ResourceEvent::RenderbuffersDeleted){
            const auto kind=event.kind==ResourceEvent::Deleted?GlColorResource::Texture:GlColorResource::Renderbuffer;
            for(unsigned provider=0;provider<=maximumEglProviders;++provider)
                if(event.providers&(1u<<provider))h.provenance.deleted(provider,int(event.count),event.names.data(),kind);
        }else if(event.kind==ResourceEvent::PbufferRetired){
            for(unsigned provider=0;provider<=maximumEglProviders;++provider)
                if(event.providers&(1u<<provider))h.provenance.retiredPbuffer(provider,event.pbuffer);
        }else{
            if(event.kind!=ResourceEvent::Shared)h.provenance.forget(event.child);
            if(event.kind==ResourceEvent::Shared||event.kind==ResourceEvent::CreatedShared)
                h.provenance.shared(event.parent,event.child);
            else h.provenance.remember(event.child);
        }
    }
    h.drainedResourceSerial=serial;
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
void *nativeWglProcedure(const char *name){
    auto &h=hooks();const auto proc=h.nativeWglProc?h.nativeWglProc:h.wglProc;if(!proc)return nullptr;
    const auto value=reinterpret_cast<std::intptr_t>(proc(name));
    return !value||value==1||value==2||value==3||value==-1?nullptr:reinterpret_cast<void*>(value);
}
HWND drawableFor(unsigned provider,DrawableKey *key,bool read=false){
    auto &h=hooks();
    if(!provider){
        auto dc=h.currentDc?h.currentDc():nullptr;
        if(read){
            auto currentRead=reinterpret_cast<WglCurrentDc>(nativeWglProcedure("wglGetCurrentReadDCARB"));
            if(!currentRead)currentRead=reinterpret_cast<WglCurrentDc>(nativeWglProcedure("wglGetCurrentReadDCEXT"));
            if(currentRead)dc=currentRead();
        }
        *key={0,reinterpret_cast<std::uintptr_t>(dc)};return dc?WindowFromDC(dc):nullptr;
    }
    auto &p=h.egl[provider-1];if(!p.currentSurface||!p.currentDisplay)return nullptr;
    *key={reinterpret_cast<std::uintptr_t>(p.currentDisplay()),reinterpret_cast<std::uintptr_t>(p.currentSurface(read?0x305a:0x3059))};
    // EGL_DRAW on the exact display, not an arbitrary context or another
    // surface which happens to use the same native HWND.
    std::lock_guard<std::mutex> lock(h.surfaceMutex);const auto it=p.surfaces.find(*key);
    return it==p.surfaces.end()?nullptr:it->second;
}
bool installDrawHooks(unsigned provider,void *user);
bool installResourceHooks(unsigned provider,void *user);
bool installTransferHooks(unsigned provider,void *user);
void installWglPbufferHooks();
std::uintptr_t pbufferFor(unsigned provider,DrawableKey drawable){
    auto &h=hooks();std::lock_guard<std::mutex> lock(h.surfaceMutex);
    if(!provider)return h.wglPbuffers.resource(drawable.second);
    const auto &pbuffers=h.egl[provider-1].pbuffers;const auto it=pbuffers.find(drawable);
    return it==pbuffers.end()?0:it->second;
}
bool extension(const char *list,const char *name);
Renderer rendererFor(unsigned provider){
    if(!provider)return Renderer::OpenGL;
    auto &p=hooks().egl[provider-1];const auto api=p.api?p.api():0;
    return api==0x30a0u?Renderer::OpenGLES:api==0x30a2u?Renderer::OpenGL:Renderer::Unknown;
}
bool unprotectedEgl(EglProvider &p,DrawableKey drawable,std::uintptr_t context,bool *externalProtectionPossible,bool read=false){
    const auto display=reinterpret_cast<void*>(drawable.first),surface=reinterpret_cast<void*>(drawable.second);
    if(p.currentDisplay()!=display||p.currentSurface(read?0x305a:0x3059)!=surface
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
// Shared native routing for draw AND color-transfer observations. Merely
// recognizing a pbuffer (including a driver's hidden-window DC) is not proof:
// a nonselected route must still match a recently consumed exact output.
struct GameCandidate {
    unsigned provider=0;
    std::uintptr_t context=0,pbuffer=0;
    DrawableKey drawable{};
    GlContextIdentity identity{};
    EglProvider *egl=nullptr;
    GameContext *session=nullptr;
    bool exact=false,protectionPossible=false;
    bool find(unsigned index,const TextureRoute &route){
        provider=index;auto &h=hooks();auto &runtime=Runtime::instance();
        context=contextFor(provider);if(!context)return false;
        const auto renderer=rendererFor(provider);if(renderer!=route.renderer)return false;
        GameContext *source=nullptr;
        for(const auto &entry:h.contexts)
            if(std::get<0>(entry.first)==provider&&reinterpret_cast<std::uintptr_t>(entry.second.get())==route.object){source=entry.second.get();break;}
        if(!source||!source->drawHooks||!runtime.isPresentation(renderer,route.object,route.window))return false;
        const auto window=drawableFor(provider,&drawable);
        exact=source->window==window&&source->drawable==drawable;
        identity={provider,drawable.first,context};pbuffer=pbufferFor(provider,drawable);
        const auto &quality=route.configuration;
        if(!exact&&(quality.drawPolicy!=TextureDrawPolicy::OwnedMipCompatibility||!quality.externalOverridesReviewed
           ||!source->resourceHooks||(!h.provenance.hasDependencies(identity)&&!h.provenance.hasNativePbuffer(identity,pbuffer))
           ||(!pbuffer&&(!window||window!=source->window))))return false;
        egl=provider?&h.egl[provider-1]:nullptr;
        if(egl&&!unprotectedEgl(*egl,drawable,context,&protectionPossible))return false;
        const ContextKey key{provider,drawable.first,context};auto found=h.contexts.find(key);
        if(found==h.contexts.end()){
            if(h.contexts.size()>=16)return false;
            found=h.contexts.emplace(key,std::make_unique<GameContext>()).first;
        }
        session=found->second.get();
        // Never overwrite the presenter's association when the same context
        // visits an offscreen producer, even if that pbuffer has a hidden HWND.
        if(exact){session->window=window;session->drawable=drawable;}
        session->renderer=renderer;return true;
    }
    bool initialize(std::string *detail){
        if(!session->drawHooks)
            session->drawHooks=session->potato.initialize(resolve,egl,detail,protectionPossible)&&installDrawHooks(provider,egl);
        return session->drawHooks;
    }
    bool qualifyOutput(GlTextureUsage &usage)const{
        if(!usage.defaultFramebuffer)return true;
        if(pbuffer){usage.writes[0]=GlColorResource::pbuffer(pbuffer,usage.nativeColorBuffer);usage.writeCount=1;return true;}
        if(exact){usage.writeCount=0;return true;} // selected default output can seed a root, not a pbuffer identity
        return false;
    }
    bool qualifyTransferInput(GlTextureUsage &usage)const{
        for(std::size_t i=0;i<usage.readCount;++i)if(usage.reads[i].kind==GlColorResource::Pbuffer){
            DrawableKey read{};drawableFor(provider,&read,true);const auto resource=pbufferFor(provider,read);
            if(!resource||(egl&&!unprotectedEgl(*egl,read,context,nullptr,true)))return false;
            usage.reads[i]=GlColorResource::pbuffer(resource,usage.reads[i].colorBuffer);
        }
        for(std::size_t i=0;i<usage.readCount;++i)for(std::size_t j=0;j<usage.writeCount;++j)
            if(usage.reads[i]==usage.writes[j])return false;
        return true;
    }
};
// Exact selected drawable, or compatibility-only proven color-resource output
// in its same/observed-share namespace. No foreign/unknown route is adopted.
struct GameDraw {
    GlDrawScope scope;
    GameContext *selected=nullptr;
    GameDraw(unsigned providers,unsigned primitive)noexcept:scope(hooks().renderMutex,presenting){
        try {
            auto &h=hooks();auto &runtime=Runtime::instance();
            if(!scope.mayInspect()||h.stopping.load()||!runtime.texture().active())return;
            if(!scope.tryLock())return;
            drainResources();const auto route=runtime.textureRoute();const auto quality=route.configuration;
            h.provenance.synchronize(route.object,route.revision);
            if(!quality.active()||!route.object||!runtime.eligible(route.window)
               ||h.drainedResourceSerial!=h.resourceSerial.load()){scope.releaseBypass();return;}
            for(unsigned provider=0;provider<=maximumEglProviders;++provider){
                if(!(providers&(1u<<provider)))continue;
                GameCandidate candidate;if(!candidate.find(provider,route))continue;
                const auto exact=candidate.exact;const auto identity=candidate.identity;
                auto &session=*candidate.session;selected=&session;scope.guardInspection();
                std::string detail;
                if(!candidate.initialize(&detail)){
                    runtime.textureReport(0,detail.empty()?"This game context has no safe sampler-inspection path; native sampling retained.":detail,false,
                                          TextureDrawReason::NotInspected,TextureDepthFailure::None,scope.nested(),!exact,route.revision);
                    selected=nullptr;scope.releaseBypass();return;
                }
                if(quality.drawPolicy==TextureDrawPolicy::OwnedMipCompatibility&&quality.externalOverridesReviewed){
                    if(!session.resourceHooks)session.resourceHooks=installResourceHooks(provider,candidate.egl);
                    const auto serial=h.drainedResourceSerial;GlTextureUsage usage;
                    const bool captured=session.resourceHooks&&(primitive==4||primitive==5||primitive==6)&&session.potato.usage(&usage)
                        &&candidate.qualifyOutput(usage)&&serial==h.resourceSerial.load();
                    if(!exact&&(!captured||!h.provenance.producer(identity,usage))){selected=nullptr;scope.releaseBypass();return;}
                    if(captured){
                        const auto inputs=h.provenance.observe(identity,usage,exact);
                        if(exact&&usage.defaultFramebuffer)runtime.textureConsumerReport(inputs,route.revision);
                    }
                }
                if(runtime.textureRoute().revision!=route.revision){selected=nullptr;scope.releaseBypass();return;}
                TextureDrawReason reason=TextureDrawReason::NotInspected;TextureDepthFailure depth=TextureDepthFailure::None;
                const auto changed=selected->potato.begin(quality.quality(),quality.externalOverridesReviewed,primitive,&detail,&reason,&depth,quality.drawPolicy,quality.strength);
                if(!exact)detail="Presentation-linked color-resource producer: "+detail;
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
// Transfer callbacks never borrow samplers or hold renderMutex across a native
// copy/resolve (a driver may join workers). Keep only value identities; publish
// after the original returns and only if revision/lifecycle/current route still
// match. Nested aliases are not duplicate evidence; nested DRAW hooks remain
// allowed, unlike a live private-sampler override.
struct GameTransfer {
    GlDrawScope scope;
    inline static thread_local unsigned depth=0;
    const bool outer;
    GlTextureUsage usage;
    GlContextIdentity identity{};
    DrawableKey drawable{};
    std::uint64_t revision=0,serial=0;
    std::uintptr_t object=0;
    bool captured=false,exact=false;
    GameTransfer(unsigned providers,const GlColorTransfer &call)noexcept
        :scope(hooks().renderMutex,presenting),outer(++depth==1){
        try{
            auto &h=hooks();auto &runtime=Runtime::instance();
            if(!outer||!scope.mayInspect()||h.stopping.load()||!runtime.texture().active()||!scope.tryLock())return;
            drainResources();const auto route=runtime.textureRoute();const auto &quality=route.configuration;
            h.provenance.synchronize(route.object,route.revision);
            if(quality.active()&&quality.externalOverridesReviewed&&quality.drawPolicy==TextureDrawPolicy::OwnedMipCompatibility
               &&route.object&&runtime.eligible(route.window)&&h.drainedResourceSerial==h.resourceSerial.load()){
                for(unsigned provider=0;provider<=maximumEglProviders;++provider){
                    if(!(providers&(1u<<provider)))continue;
                    GameCandidate candidate;if(!candidate.find(provider,route))continue;
                    scope.guardInspection();std::string detail;
                    if(!candidate.initialize(&detail))break;
                    auto &session=*candidate.session;
                    if(!session.resourceHooks)session.resourceHooks=installResourceHooks(provider,candidate.egl);
                    serial=h.drainedResourceSerial;
                    if(!session.resourceHooks||!session.potato.transfer(call,&usage)||!candidate.qualifyOutput(usage)
                       ||!candidate.qualifyTransferInput(usage)||serial!=h.resourceSerial.load())break;
                    if(!candidate.exact&&!h.provenance.producer(candidate.identity,usage))break;
                    identity=candidate.identity;drawable=candidate.drawable;exact=candidate.exact;
                    revision=route.revision;object=route.object;captured=true;break;
                }
            }
        }catch(...){} // Metadata failure cannot modify/prevent the native call.
        scope.releaseBypass();
    }
    void complete()noexcept{
        if(!captured)return;
        try{
            auto &h=hooks();if(h.stopping.load()||serial!=h.resourceSerial.load())return;
            std::unique_lock<std::mutex> lock(h.renderMutex,std::try_to_lock);if(!lock.owns_lock())return;
            drainResources();if(serial!=h.resourceSerial.load())return;
            auto &runtime=Runtime::instance();const auto route=runtime.textureRoute();
            if(route.revision!=revision||route.object!=object||!route.configuration.active()
               ||contextFor(identity.provider)!=identity.context)return;
            DrawableKey current{};drawableFor(identity.provider,&current);
            if(current!=drawable||!runtime.eligible(route.window)||serial!=h.resourceSerial.load())return;
            const auto inputs=h.provenance.observe(identity,usage,exact);
            runtime.textureTransferReport(inputs,revision);
        }catch(...){} // Optional observations never throw through a native ABI.
    }
    ~GameTransfer(){--depth;}
};
// Successful native render-to-texture is a resource link without a GL
// copy/blit. The native pbuffer lifetime + color aspect is explicit, NOT an
// inferred GL share group. Hold no render lock, session pointer or sampler
// lease across that original. Nothing is published on a failed native call.
struct GameImageBinding {
    GlDrawScope scope;
    inline static thread_local unsigned depth=0;
    const bool outer;
    GlContextIdentity identity{};DrawableKey drawable{};
    GlColorResource source{};GlTextureBinding binding{};
    std::uint64_t revision=0,serial=0;std::uintptr_t object=0;
    bool captured=false;
    GameImageBinding(unsigned provider,GlColorResource nativeSource,std::uint64_t nativeSerial)noexcept
        :scope(hooks().renderMutex,presenting),outer(++depth==1),source(nativeSource){
        try{
            auto &h=hooks();auto &runtime=Runtime::instance();
            if(!source||source.kind!=GlColorResource::Pbuffer||!source.colorBuffer||!outer||!scope.mayInspect()
               ||h.stopping.load()||!runtime.texture().active()||!scope.tryLock())return;
            drainResources();const auto route=runtime.textureRoute();const auto &quality=route.configuration;
            h.provenance.synchronize(route.object,route.revision);
            if(quality.active()&&quality.externalOverridesReviewed&&quality.drawPolicy==TextureDrawPolicy::OwnedMipCompatibility
               &&route.object&&runtime.eligible(route.window)&&h.drainedResourceSerial==nativeSerial
               &&h.drainedResourceSerial==h.resourceSerial.load()){
                // The source generation AND epoch were read together under
                // surfaceMutex. Draining a newer retirement must not pair the
                // OLD lifetime with a newly captured otherwise-stable serial.
                GameCandidate candidate;
                if(candidate.find(provider,route)){
                    scope.guardInspection();std::string detail;
                    if(candidate.initialize(&detail)){
                        auto &session=*candidate.session;
                        if(!session.resourceHooks)session.resourceHooks=installResourceHooks(provider,candidate.egl);
                        serial=h.drainedResourceSerial;
                        if(session.resourceHooks&&session.potato.textureBinding(&binding)&&serial==h.resourceSerial.load()){
                            identity=candidate.identity;drawable=candidate.drawable;
                            revision=route.revision;object=route.object;captured=true;
                        }
                    }
                }
            }
        }catch(...){} // Native rendering always continues unchanged.
        scope.releaseBypass();
    }
    void complete()noexcept{
        if(!captured)return;
        try{
            auto &h=hooks();if(h.stopping.load()||serial!=h.resourceSerial.load())return;
            std::unique_lock<std::mutex> lock(h.renderMutex,std::try_to_lock);if(!lock.owns_lock())return;
            drainResources();if(serial!=h.resourceSerial.load())return;
            auto &runtime=Runtime::instance();const auto route=runtime.textureRoute();
            if(route.object!=object||route.revision!=revision||!route.configuration.active()||!runtime.eligible(route.window))return;
            GameCandidate candidate;
            if(!candidate.find(identity.provider,route)||!(candidate.identity==identity)||candidate.drawable!=drawable)return;
            GlTextureBinding now;
            if(!candidate.session->potato.textureBinding(&now,true)||!(now==binding)||serial!=h.resourceSerial.load())return;
            GlTextureUsage usage;usage.reads[usage.readCount++]=source;usage.writes[usage.writeCount++]=binding.texture;
            if(!candidate.exact&&!h.provenance.producer(identity,usage))return;
            const auto inputs=h.provenance.observe(identity,usage,candidate.exact);
            runtime.textureTransferReport(inputs,revision);
        }catch(...){} // No metadata exception crosses the native ABI.
    }
    ~GameImageBinding(){--depth;}
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
template<unsigned Index> void WINAPI blitFramebuffer(int sx0,int sy0,int sx1,int sy1,int dx0,int dy0,int dx1,int dy1,unsigned mask,unsigned filter){
    auto &hook=hooks().blits[Index];
    GameTransfer transfer(hook.providers.load(std::memory_order_acquire),GlColorTransfer::blit(sx0,sy0,sx1,sy1,dx0,dy0,dx1,dy1,mask,filter));
    using Function=void(WINAPI*)(int,int,int,int,int,int,int,int,unsigned,unsigned);
    reinterpret_cast<Function>(hook.original.load(std::memory_order_acquire))(sx0,sy0,sx1,sy1,dx0,dy0,dx1,dy1,mask,filter);
    transfer.complete();
}
template<unsigned Index> void WINAPI copyTexSubImage2D(unsigned target,int level,int dx,int dy,int x,int y,int width,int height){
    auto &hook=hooks().copies[Index];
    GameTransfer transfer(hook.providers.load(std::memory_order_acquire),GlColorTransfer::copy(target,level,dx,dy,x,y,width,height));
    using Function=void(WINAPI*)(unsigned,int,int,int,int,int,int,int);
    reinterpret_cast<Function>(hook.original.load(std::memory_order_acquire))(target,level,dx,dy,x,y,width,height);
    transfer.complete();
}
template<std::size_t... Index> auto blitCallbacks(std::index_sequence<Index...>){return std::array<void*,sizeof...(Index)>{{reinterpret_cast<void*>(&blitFramebuffer<Index>)...}};}
template<std::size_t... Index> auto copyCallbacks(std::index_sequence<Index...>){return std::array<void*,sizeof...(Index)>{{reinterpret_cast<void*>(&copyTexSubImage2D<Index>)...}};}
template<bool Renderbuffers,unsigned Index> void WINAPI deletedObjects(int count,const unsigned *names){
    auto &h=hooks();auto &hook=Renderbuffers?h.renderbufferDeletes[Index]:h.textureDeletes[Index];const auto providers=hook.providers.load(std::memory_order_acquire);
    if(count>0&&!h.stopping.load()){
        if(!names||count>64){h.resourceInvalidate.fetch_or(providers);h.resourceSerial.fetch_add(1);}
        else {ResourceEvent event;event.kind=Renderbuffers?ResourceEvent::RenderbuffersDeleted:ResourceEvent::Deleted;
            event.providers=providers;event.count=unsigned(count);std::copy_n(names,count,event.names.begin());resourceEvent(event);}
    }
    using Function=void(WINAPI*)(int,const unsigned*);
    reinterpret_cast<Function>(hook.original.load(std::memory_order_acquire))(count,names);
}
template<bool Renderbuffers,std::size_t... Index> auto deleteCallbacks(std::index_sequence<Index...>){
    return std::array<void*,sizeof...(Index)>{{reinterpret_cast<void*>(&deletedObjects<Renderbuffers,Index>)...}};
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
bool installTransferHooks(unsigned provider,void *user){
    static const auto blits=blitCallbacks(std::make_index_sequence<32>{}),copies=copyCallbacks(std::make_index_sequence<32>{});
    bool any=false;
    for(const auto *name:{"glBlitFramebuffer","glBlitFramebufferEXT","glBlitFramebufferANGLE","glBlitFramebufferNV"})
        any=resourceHook(hooks().blits,blits,resolve(name,user),1u<<provider)||any;
    for(const auto *name:{"glCopyTexSubImage2D","glCopyTexSubImage2DEXT"})
        any=resourceHook(hooks().copies,copies,resolve(name,user),1u<<provider)||any;
    return any;
}
bool installResourceHooks(unsigned provider,void *user){
    static const auto textures=deleteCallbacks<false>(std::make_index_sequence<12>{}),renderbuffers=deleteCallbacks<true>(std::make_index_sequence<12>{});
    const bool deletedTextures=resourceHook(hooks().textureDeletes,textures,resolve("glDeleteTextures",user),1u<<provider);
    const bool deletedRenderbuffers=resourceHook(hooks().renderbufferDeletes,renderbuffers,resolve("glDeleteRenderbuffers",user),1u<<provider);
    installTransferHooks(provider,user);if(!provider)installWglPbufferHooks();
    return deletedTextures&&deletedRenderbuffers;
}
template<unsigned Index> HDC WINAPI getPbufferDc(void *pbuffer){
    auto &h=hooks();auto &entry=h.pbufferGets[Index];
    const auto dc=reinterpret_cast<WglPbufferGet>(entry.original.load(std::memory_order_acquire))(pbuffer);
    if(dc&&!h.stopping.load()&&entry.providers.load(std::memory_order_acquire))try{
        std::lock_guard<std::mutex> lock(h.surfaceMutex);
        if(!h.wglPbuffers.acquired(reinterpret_cast<std::uintptr_t>(pbuffer),reinterpret_cast<std::uintptr_t>(dc)))h.resourceInvalidate.fetch_or(1);
        h.resourceSerial.fetch_add(1);
    }catch(...){}
    return dc;
}
template<unsigned Index> int WINAPI releasePbufferDc(void *pbuffer,HDC dc){
    auto &h=hooks();auto &entry=h.pbufferReleases[Index];
    const auto result=reinterpret_cast<WglPbufferRelease>(entry.original.load(std::memory_order_acquire))(pbuffer,dc);
    if(result>0&&!h.stopping.load())try{
        ResourceEvent event;event.kind=ResourceEvent::PbufferRetired;event.providers=1;
        {
            std::lock_guard<std::mutex> lock(h.surfaceMutex);h.resourceSerial.fetch_add(1);
            if(!h.wglPbuffers.released(reinterpret_cast<std::uintptr_t>(pbuffer),reinterpret_cast<std::uintptr_t>(dc)))
                event.pbuffer=h.wglPbuffers.destroyed(reinterpret_cast<std::uintptr_t>(pbuffer));
        }
        if(event.pbuffer)resourceEvent(event);
    }catch(...){}
    return result;
}
template<unsigned Index> BOOL WINAPI destroyPbuffer(void *pbuffer){
    auto &h=hooks();auto &entry=h.pbufferDestroys[Index];
    const auto result=reinterpret_cast<WglPbufferDestroy>(entry.original.load(std::memory_order_acquire))(pbuffer);
    if(result&&!h.stopping.load())try{
        ResourceEvent event;event.kind=ResourceEvent::PbufferRetired;event.providers=1;
        {std::lock_guard<std::mutex> lock(h.surfaceMutex);h.resourceSerial.fetch_add(1);
            event.pbuffer=h.wglPbuffers.destroyed(reinterpret_cast<std::uintptr_t>(pbuffer));}
        resourceEvent(event);
    }catch(...){}
    return result;
}
template<std::size_t... Index> auto pbufferGetCallbacks(std::index_sequence<Index...>){return std::array<void*,sizeof...(Index)>{{reinterpret_cast<void*>(&getPbufferDc<Index>)...}};}
template<std::size_t... Index> auto pbufferReleaseCallbacks(std::index_sequence<Index...>){return std::array<void*,sizeof...(Index)>{{reinterpret_cast<void*>(&releasePbufferDc<Index>)...}};}
template<std::size_t... Index> auto pbufferDestroyCallbacks(std::index_sequence<Index...>){return std::array<void*,sizeof...(Index)>{{reinterpret_cast<void*>(&destroyPbuffer<Index>)...}};}
template<unsigned Index> BOOL WINAPI bindPbufferTexture(void *pbuffer,int buffer){
    const auto incoming=GetLastError();auto &h=hooks();auto &entry=h.pbufferBinds[Index];
    std::uintptr_t generation=0;std::uint64_t nativeSerial=0;
    try{
        std::lock_guard<std::mutex> lock(h.surfaceMutex);
        generation=h.wglPbuffers.generation(reinterpret_cast<std::uintptr_t>(pbuffer));nativeSerial=h.resourceSerial.load();
    }catch(...){}
    const unsigned color=buffer>=0x2083&&buffer<=0x2086?0x0400u+unsigned(buffer-0x2083):0;
    GameImageBinding capture(0,GlColorResource::pbuffer(generation,color),nativeSerial);
    SetLastError(incoming);
    const auto result=reinterpret_cast<WglBindTexture>(entry.original.load(std::memory_order_acquire))(pbuffer,buffer);
    const auto nativeError=GetLastError();
    if(result&&capture.captured)try{
        const auto query=reinterpret_cast<WglPbufferQuery>(nativeWglProcedure("wglQueryPbufferARB"));
        int target=0,format=0,level=-1;
        // These are legal ARB_render_texture pbuffer queries. Do NOT ask
        // QueryPbuffer for pixel-format attributes such as DOUBLE_BUFFER.
        if(query&&query(pbuffer,0x2073,&target)&&target==0x207a
           &&query(pbuffer,0x2072,&format)&&(format==0x2075||format==0x2076)
           &&query(pbuffer,0x207b,&level)&&level==0)capture.complete();
    }catch(...){}
    SetLastError(nativeError);return result;
}
template<std::size_t... Index> auto pbufferBindCallbacks(std::index_sequence<Index...>){return std::array<void*,sizeof...(Index)>{{reinterpret_cast<void*>(&bindPbufferTexture<Index>)...}};}
void installWglPbufferHooks(){
    static const auto gets=pbufferGetCallbacks(std::make_index_sequence<16>{}),releases=pbufferReleaseCallbacks(std::make_index_sequence<16>{}),destroys=pbufferDestroyCallbacks(std::make_index_sequence<16>{});
    static const auto binds=pbufferBindCallbacks(std::make_index_sequence<16>{});
    auto &h=hooks();
    resourceHook(h.pbufferBinds,binds,nativeWglProcedure("wglBindTexImageARB"),1);
    for(const auto *suffix:{"ARB","EXT"}){
        const std::string family(suffix);
        const auto get=nativeWglProcedure(("wglGetPbufferDC"+family).c_str());
        const auto release=nativeWglProcedure(("wglReleasePbufferDC"+family).c_str());
        const auto destroy=nativeWglProcedure(("wglDestroyPbuffer"+family).c_str());
        // Enable acquisition LAST. A partially installed lifecycle family must
        // not publish a pbuffer/DC which can later disappear unobserved.
        if(get&&release&&destroy&&resourceHook(h.pbufferReleases,releases,release,1)&&resourceHook(h.pbufferDestroys,destroys,destroy,1))
            resourceHook(h.pbufferGets,gets,get,1);
    }
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
    if(result&&name)try{
        const auto value=reinterpret_cast<std::intptr_t>(result);
        if(value!=1&&value!=2&&value!=3&&value!=-1){
            if(std::strcmp(name,"wglCreateContextAttribsARB")==0){
                static const auto callbacks=attributedCallbacks(std::make_index_sequence<16>{});
                resourceHook(hooks().attributedCreates,callbacks,reinterpret_cast<void*>(result),1);
            }
            if(std::strncmp(name,"wglGetPbufferDC",15)==0||std::strncmp(name,"wglReleasePbufferDC",19)==0
               ||std::strncmp(name,"wglDestroyPbuffer",17)==0||std::strcmp(name,"wglBindTexImageARB")==0)installWglPbufferHooks();
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
        if(quality.active()){
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
        ResourceEvent retired;retired.kind=ResourceEvent::PbufferRetired;retired.providers=1u<<(Index+1);
        {
            std::lock_guard<std::mutex> lock(h.surfaceMutex);h.resourceSerial.fetch_add(1);
            const DrawableKey key{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(surface)};
            const auto old=p.pbuffers.find(key);if(old!=p.pbuffers.end()){retired.pbuffer=old->second;p.pbuffers.erase(old);}
            if(unprotected&&terminated&&p.pbuffers.size()<128&&p.nextPbuffer)p.pbuffers.emplace(key,p.nextPbuffer++);
        }
        if(retired.pbuffer)resourceEvent(retired);
    }catch(...){}
    return surface;
}
template<unsigned Index> unsigned WINAPI bindEglTexture(void *display,void *surface,int buffer){
    auto &h=hooks();auto &p=h.egl[Index];std::uintptr_t generation=0;std::uint64_t nativeSerial=0;
    try{
        const bool current=p.currentDisplay()==display&&rendererFor(Index+1)==Renderer::OpenGLES;
        std::lock_guard<std::mutex> lock(h.surfaceMutex);
        const auto found=p.pbuffers.find({reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(surface)});
        if(current&&found!=p.pbuffers.end())generation=found->second;
        nativeSerial=h.resourceSerial.load();
    }catch(...){}
    GameImageBinding capture(Index+1,GlColorResource::pbuffer(generation,buffer==0x3084?0x0405u:0),nativeSerial);
    const auto result=p.bindTexture(display,surface,buffer);
    if(result&&capture.captured)try{
        int target=0,format=0,level=-1;
        if(p.query(display,surface,0x3081,&target)&&target==0x305f
           &&p.query(display,surface,0x3080,&format)&&(format==0x305d||format==0x305e)
           &&p.query(display,surface,0x3083,&level)&&level==0)capture.complete();
    }catch(...){}
    return result;
}
template<unsigned Index> unsigned WINAPI removeSurface(void *display,void *surface){
    auto &h=hooks();auto &p=h.egl[Index];const auto okay=p.destroySurface(display,surface);
    if(okay){try{
        const DrawableKey drawable{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(surface)};
        ResourceEvent retired;retired.kind=ResourceEvent::PbufferRetired;retired.providers=1u<<(Index+1);
        {std::lock_guard<std::mutex> lock(h.surfaceMutex);h.resourceSerial.fetch_add(1);p.surfaces.erase(drawable);
            const auto old=p.pbuffers.find(drawable);if(old!=p.pbuffers.end()){retired.pbuffer=old->second;p.pbuffers.erase(old);}}
        if(retired.pbuffer)resourceEvent(retired);
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
        install(reinterpret_cast<void*>(GetProcAddress(module,"eglBindTexImage")),reinterpret_cast<void*>(&bindEglTexture<Index>),reinterpret_cast<void**>(&p.bindTexture));
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
#ifdef BSI_SHADER_HOOK_TESTING
bool setWglProviderForTesting(void *module){
    auto &h=hooks();std::lock_guard<std::mutex> lock(h.mutex);if(h.started||h.gl||!module)return false;
    h.gl=static_cast<HMODULE>(module);h.currentWgl=exportFunction<WglCurrent>(h.gl,"wglGetCurrentContext");
    h.currentDc=exportFunction<WglCurrentDc>(h.gl,"wglGetCurrentDC");h.wglProc=exportFunction<WglProc>(h.gl,"wglGetProcAddress");
    return h.currentWgl&&h.currentDc&&h.wglProc;
}
#endif
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
