#include "Runtime.h"
#ifdef _WIN32
#include "GlPipeline.h"
#include "GlTextureOverride.h"
#include "GlDrawCalls.h"
#include "GlDrawScope.h"
#include "GlProducerRoute.h"
#include "WglPbufferRegistry.h"
#include "EglDispatchScope.h"
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
using WglMake=BOOL(WINAPI*)(HDC,HGLRC);
using WglMakeSeparate=BOOL(WINAPI*)(HDC,HDC,HGLRC);
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
using EglMake=unsigned(WINAPI*)(void*,void*,void*,void*);
using EglQueryApi=unsigned(WINAPI*)();
using EglString=const char*(WINAPI*)(void*,int);
constexpr unsigned maximumEglProviders=4,drawKinds=glDrawKinds,drawSlots=32,eglAliasSlots=16;
static_assert(drawSlots>=(maximumEglProviders+1)*glDrawAliases,"bound all independent provider/alias targets");
using DrawableKey=std::pair<std::uintptr_t,std::uintptr_t>; // display + surface (or 0 + WGL DC)
using ContextKey=std::tuple<unsigned,std::uintptr_t,std::uintptr_t>; // provider + display + context
struct EglProvider {
    HMODULE module=nullptr;
    EglSwap swap=nullptr;EglBindTexture bindTexture=nullptr;EglCreate create=nullptr,createPlatformExt=nullptr,createContext=nullptr;
    EglPlatformCreate createPlatform=nullptr;EglPbufferCreate createPbuffer=nullptr;
    EglDestroy destroySurface=nullptr,destroyContext=nullptr;
    EglCurrent current=nullptr,currentDisplay=nullptr;EglCurrentSurface currentSurface=nullptr;
    EglQuery query=nullptr,queryContext=nullptr;EglProc proc=nullptr,nativeProc=nullptr;
    std::atomic<std::uint64_t> dispatchRevision{1};EglQueryApi api=nullptr;EglString strings=nullptr;
    std::atomic<bool> lifecycleReady{false},attaching{true},lifecycleFault{false}; // fail closed on any known uncovered lifecycle target
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
struct DrawHook {void *target=nullptr;std::atomic<void*> original{nullptr};std::atomic<unsigned> providers{0},contracts{0};};
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
    WglCreate createWgl=nullptr;WglShare shareWgl=nullptr;WglProc nativeWglProc=nullptr;WglMake makeWgl=nullptr;
    std::atomic<std::uint64_t> wglDispatchRevision{1}; // code discovery only, NEVER resource/ownership proof
    GlTextureProvenance provenance;
    std::array<ResourceEvent,32> resources{};unsigned resourceCount=0;
    std::atomic<unsigned> resourceReset{0},resourceInvalidate{0},pendingCopyInvalidate{0};
    std::atomic<std::uint64_t> resourceSerial{0};
    std::uint64_t drainedResourceSerial=0; // renderMutex: evidence includes events only through this serial
    std::array<DrawHook,12> textureDeletes{},renderbufferDeletes{};
    std::array<DrawHook,32> blits{},copies{},imageCopies{},eglCurrents{};
    std::array<std::array<DrawHook,eglAliasSlots>,eglDispatchNames.size()> eglAliases{};
    std::array<DrawHook,16> attributedCreates{},separateCurrents{},pbufferGets{},pbufferReleases{},pbufferDestroys{},pbufferBinds{};
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
void *nativeEglProcedure(EglProvider &p,const char *name){
    // The public resolver trampoline can itself enter a separately detoured
    // implementation. Internal CODE discovery is never an application lookup
    // and must not recursively register procedures through that nested alias.
    EglInternalLookup guard;const auto lookup=p.nativeProc?p.nativeProc:p.proc;
    return lookup?lookup(name):nullptr;
}
void *resolve(const char *name,void *user){
    auto &h=hooks();void *address=nullptr;
    if(user){auto &p=*static_cast<EglProvider*>(user);address=nativeEglProcedure(p,name);if(!address&&p.module)address=reinterpret_cast<void*>(GetProcAddress(p.module,name));}
    else {
        const auto lookup=h.nativeWglProc?h.nativeWglProc:h.wglProc;if(lookup)address=reinterpret_cast<void*>(lookup(name));
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
    const auto pendingInvalidate=h.pendingCopyInvalidate.exchange(0);
    for(unsigned provider=0;provider<=maximumEglProviders;++provider){
        if(reset&(1u<<provider))h.provenance.forgetProvider(provider);
        else if(invalidate&(1u<<provider))h.provenance.invalidate(provider);
        if(pendingInvalidate&(1u<<provider))h.provenance.discardPendingProvider(provider);
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
    if(!provider)hooks().wglDispatchRevision.fetch_add(1);
    else hooks().egl[provider-1].dispatchRevision.fetch_add(1);
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
bool installDrawHooks(unsigned provider,void *user,bool *complete=nullptr);
bool installResourceHooks(unsigned provider,void *user,bool *complete=nullptr);
bool installTransferHooks(unsigned provider,void *user,bool *complete=nullptr);
bool installWglPbufferHooks();
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
    if(!display||!context||p.currentDisplay()!=display||p.currentSurface(read?0x305a:0x3059)!=surface
       ||reinterpret_cast<std::uintptr_t>(p.current())!=context)return false;
    // A real surfaceless current context has NEITHER native drawable. Never
    // call eglQuerySurface(EGL_NO_SURFACE, ...): it is not a queryable image.
    // The context/protection check still precedes every optional GL call.
    if(!surface&&p.currentSurface(read?0x3059:0x305a))return false;
    const char *extensions=p.strings(display,0x3055); // EGL_EXTENSIONS
    if(!extensions)return false;
    const bool content=extension(extensions,"EGL_EXT_protected_content");
    const bool surfaces=content||extension(extensions,"EGL_EXT_protected_surface");
    int protectedSurface=0,protectedContext=0;
    if(externalProtectionPossible)*externalProtectionPossible=surfaces;
    return (!surface||!surfaces||(p.query(display,surface,0x32c0,&protectedSurface)&&!protectedSurface))
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
    GlProducerRoute producerRoute=GlProducerRoute::Refuse;
    bool find(unsigned index,const TextureRoute &route,GlCandidateStop *stop=nullptr){
        const auto refuse=[&](GlCandidateStop reason){if(stop)*stop=reason;return false;};
        if(stop)*stop=GlCandidateStop::Qualified;
        provider=index;auto &h=hooks();auto &runtime=Runtime::instance();
        if(provider&&(!h.egl[provider-1].lifecycleReady.load(std::memory_order_acquire)||h.egl[provider-1].lifecycleFault.load(std::memory_order_acquire)))return refuse(GlCandidateStop::Lifecycle);
        context=contextFor(provider);if(!context)return refuse(GlCandidateStop::NoContext);
        const auto renderer=rendererFor(provider);if(renderer!=route.renderer)return refuse(GlCandidateStop::Renderer);
        GameContext *source=nullptr;
        for(const auto &entry:h.contexts)
            if(std::get<0>(entry.first)==provider&&reinterpret_cast<std::uintptr_t>(entry.second.get())==route.object){source=entry.second.get();break;}
        if(!source||!source->drawHooks||!runtime.isPresentation(renderer,route.object,route.window))return refuse(GlCandidateStop::Source);
        const auto window=drawableFor(provider,&drawable);
        exact=source->window==window&&source->drawable==drawable;
        identity={provider,drawable.first,context};pbuffer=pbufferFor(provider,drawable);
        // WGL needs a current non-window DC; EGL needs an actually surfaceless
        // current context/display. An UNMAPPED non-null EGL surface is not
        // inferred to be offscreen from its config or from equal GL names.
        const bool resourceOnlyDrawable=provider?(drawable.first&&!drawable.second):(!window&&drawable.second);
        producerRoute=glProducerRoute(exact,pbuffer!=0,window!=nullptr,window==source->window,resourceOnlyDrawable);
        const auto &quality=route.configuration;
        if(!exact){
            if(quality.drawPolicy!=TextureDrawPolicy::OwnedMipCompatibility||!quality.externalOverridesReviewed)return refuse(GlCandidateStop::Policy);
            if(!source->resourceHooks)return refuse(GlCandidateStop::ResourceHooks);
            if(producerRoute==GlProducerRoute::Refuse)return refuse(GlCandidateStop::Drawable);
            if(!h.provenance.hasDependencies(identity)&&!h.provenance.hasNativePbuffer(identity,pbuffer))return refuse(GlCandidateStop::Dependency);
        }
        egl=provider?&h.egl[provider-1]:nullptr;
        if(egl&&!unprotectedEgl(*egl,drawable,context,&protectionPossible))return refuse(GlCandidateStop::Protection);
        const ContextKey key{provider,drawable.first,context};auto found=h.contexts.find(key);
        if(found==h.contexts.end()){
            if(h.contexts.size()>=16)return refuse(GlCandidateStop::Capacity);
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
        return qualifyGlProducerOutput(producerRoute,pbuffer,usage);
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
            if(h.stopping.load()||presenting||!runtime.textureHookEntry(TextureHookKind::Draw))return;
            // A missed application write cannot leave an older unconsumed copy
            // as lineage. Atomic-only revocation never waits for renderMutex.
            if(!scope.mayInspect()||!scope.tryLock()){h.pendingCopyInvalidate.fetch_or(providers);return;}
            drainResources();const auto route=runtime.textureRoute();const auto quality=route.configuration;
            h.provenance.synchronize(route.object,route.revision);
            if(!quality.active()||!route.object||!runtime.eligible(route.window)
               ||h.drainedResourceSerial!=h.resourceSerial.load()){scope.releaseBypass();return;}
            for(unsigned provider=0;provider<=maximumEglProviders;++provider){
                if(!(providers&(1u<<provider)))continue;
                GameCandidate candidate;
                if(!candidate.find(provider,route)){h.provenance.discardPendingWrites(candidate.identity,nullptr);continue;}
                const auto exact=candidate.exact;const auto identity=candidate.identity;
                auto &session=*candidate.session;selected=&session;scope.guardInspection();
                std::string detail;
                if(!candidate.initialize(&detail)){
                    h.provenance.discardPendingWrites(identity,nullptr);
                    runtime.textureReport(0,detail.empty()?"This game context has no safe sampler-inspection path; native sampling retained.":detail,false,
                                          TextureDrawReason::NotInspected,TextureDepthFailure::None,scope.nested(),!exact,route.revision);
                    selected=nullptr;scope.releaseBypass();return;
                }
                if(quality.drawPolicy==TextureDrawPolicy::OwnedMipCompatibility&&quality.externalOverridesReviewed){
                    if(!session.resourceHooks)session.resourceHooks=installResourceHooks(provider,candidate.egl);
                    const auto serial=h.drainedResourceSerial;GlTextureUsage usage;
                    const bool captured=session.resourceHooks&&(primitive==4||primitive==5||primitive==6)&&session.potato.usage(&usage)
                        &&candidate.qualifyOutput(usage)&&serial==h.resourceSerial.load();
                    h.provenance.discardPendingWrites(identity,captured?&usage:nullptr);
                    if(!exact&&(!captured||!h.provenance.producer(identity,usage))){selected=nullptr;scope.releaseBypass();return;}
                    if(captured){
                        GlDeferredCopyResolution resolved;
                        const auto inputs=h.provenance.observe(identity,usage,exact,&resolved);
                        if(resolved.copies)runtime.textureDeferredCopyReport(resolved.copies,resolved.inputs,route.revision);
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
    GlTransferAttempt audit;
    GlTextureUsage usage;
    GlContextIdentity identity{};
    DrawableKey drawable{};
    std::uint64_t revision=0,serial=0;
    std::uintptr_t object=0;
    bool captured=false,exact=false,mayDefer=false;
    GameTransfer(unsigned providers,const GlColorTransfer &call)noexcept
        :scope(hooks().renderMutex,presenting),outer(++depth==1){
        audit.api=call.kind;audit.providers=providers;
        try{
            auto &h=hooks();auto &runtime=Runtime::instance();
            if(h.stopping.load()||presenting||!runtime.textureHookEntry(call.kind==GlColorTransfer::CopyImage?TextureHookKind::NamedImageCopy:TextureHookKind::ColorTransfer,&audit.revision))return;
            if(!outer){h.pendingCopyInvalidate.fetch_or(providers);audit.stop=GlTransferStop::Nested;return;}
            if(!scope.mayInspect()){h.pendingCopyInvalidate.fetch_or(providers);audit.stop=GlTransferStop::Guarded;return;}
            if(!scope.tryLock()){h.pendingCopyInvalidate.fetch_or(providers);audit.stop=GlTransferStop::Busy;return;}
            const auto capture=[&]{
                drainResources();const auto route=runtime.textureRoute();const auto &quality=route.configuration;
                h.provenance.synchronize(route.object,route.revision);
                audit.stop=GlTransferStop::Policy;
                if(!quality.active()||!quality.externalOverridesReviewed||quality.drawPolicy!=TextureDrawPolicy::OwnedMipCompatibility
                   ||!route.object||!runtime.eligible(route.window))return;
                audit.stop=GlTransferStop::ResourceChanged;
                if(h.drainedResourceSerial!=h.resourceSerial.load())return;
                for(unsigned provider=0;provider<=maximumEglProviders;++provider){
                    if(!(providers&(1u<<provider)))continue;
                    GameCandidate candidate;GlCandidateStop reason=GlCandidateStop::Qualified;
                    const bool qualified=candidate.find(provider,route,&reason);
                    ++audit.candidates[static_cast<std::size_t>(reason)];
                    if(!qualified){h.provenance.discardPendingWrites(candidate.identity,nullptr);continue;}
                    scope.guardInspection();std::string detail;
                    audit.stop=GlTransferStop::Initialize;
                    if(!candidate.initialize(&detail)){h.provenance.discardPendingWrites(candidate.identity,nullptr);return;}
                    auto &session=*candidate.session;
                    if(!session.resourceHooks)session.resourceHooks=installResourceHooks(provider,candidate.egl);
                    serial=h.drainedResourceSerial;
                    audit.stop=GlTransferStop::ResourceHooks;
                    if(!session.resourceHooks){h.provenance.discardPendingWrites(candidate.identity,nullptr);return;}
                    audit.stop=GlTransferStop::Descriptor;
                    if(!session.potato.transfer(call,&usage,&audit.descriptor)){h.provenance.discardPendingWrites(candidate.identity,nullptr);return;}
                    h.provenance.discardPendingWrites(candidate.identity,&usage);
                    audit.stop=GlTransferStop::Output;if(!candidate.qualifyOutput(usage))return;
                    audit.stop=GlTransferStop::Input;if(!candidate.qualifyTransferInput(usage))return;
                    audit.stop=GlTransferStop::ResourceChanged;if(serial!=h.resourceSerial.load())return;
                    audit.stop=GlTransferStop::UnlinkedOutput;
                    if(!candidate.exact&&!h.provenance.producer(candidate.identity,usage)){
                        // Still NOT ownership. Retain only a supported base-2D
                        // copy's value metadata after its native completion; a
                        // later exact consumer must independently prove it.
                        if(call.kind!=GlColorTransfer::CopyTexSubImage2D)return;
                        mayDefer=true;
                    }
                    identity=candidate.identity;drawable=candidate.drawable;exact=candidate.exact;
                    revision=route.revision;object=route.object;captured=true;audit.stop=GlTransferStop::NativePending;return;
                }
                audit.stop=GlTransferStop::NoCandidate;
            };
            capture();
        }catch(...){audit.stop=GlTransferStop::Exception;} // Diagnostics cannot modify/prevent the native call.
        scope.releaseBypass(); // NEVER hold a render lock or guard across the native original.
    }
    void complete()noexcept{
        if(!captured)return;
        try{
            auto &h=hooks();audit.stop=GlTransferStop::ResourceChanged;
            if(h.stopping.load()||serial!=h.resourceSerial.load())return;
            audit.stop=GlTransferStop::Busy;
            std::unique_lock<std::mutex> lock(h.renderMutex,std::try_to_lock);if(!lock.owns_lock())return;
            drainResources();audit.stop=GlTransferStop::ResourceChanged;if(serial!=h.resourceSerial.load())return;
            auto &runtime=Runtime::instance();const auto route=runtime.textureRoute();
            audit.stop=GlTransferStop::RouteChanged;
            if(route.revision!=revision||route.object!=object||!route.configuration.active()||!runtime.eligible(route.window))return;
            audit.stop=GlTransferStop::CurrentChanged;
            if(contextFor(identity.provider)!=identity.context)return;
            DrawableKey current{};drawableFor(identity.provider,&current);if(current!=drawable)return;
            audit.stop=GlTransferStop::ResourceChanged;if(serial!=h.resourceSerial.load())return;
            if(mayDefer&&!h.provenance.producer(identity,usage)){
                const auto queued=h.provenance.deferCopy(identity,usage);
                audit.stop=queued==GlDeferredCopyResult::Queued?GlTransferStop::DeferredOutput
                    :queued==GlDeferredCopyResult::Capacity?GlTransferStop::DeferredCapacity:GlTransferStop::UnlinkedOutput;
                return; // A queued callback is NOT a qualified transfer/link.
            }
            GlDeferredCopyResolution resolved;
            const auto inputs=h.provenance.observe(identity,usage,exact,&resolved);
            if(resolved.copies)runtime.textureDeferredCopyReport(resolved.copies,resolved.inputs,revision);
            runtime.textureTransferReport(inputs,revision);
            audit.stop=inputs?GlTransferStop::RecordedInputs:GlTransferStop::RecordedNoInputs;
        }catch(...){audit.stop=GlTransferStop::Exception;}
    }
    ~GameTransfer(){
        --depth;
        try{if(audit.revision)Runtime::instance().textureTransferAuditReport(audit);}catch(...){}
    }
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
            if(h.stopping.load()||presenting||!runtime.textureHookEntry(TextureHookKind::NativeImageBinding))return;
            if(!source||source.kind!=GlColorResource::Pbuffer||!source.colorBuffer||!outer||!scope.mayInspect()||!scope.tryLock()){
                h.pendingCopyInvalidate.fetch_or(1u<<provider);return;
            }
            drainResources();const auto route=runtime.textureRoute();const auto &quality=route.configuration;
            h.provenance.synchronize(route.object,route.revision);
            if(quality.active()&&quality.externalOverridesReviewed&&quality.drawPolicy==TextureDrawPolicy::OwnedMipCompatibility
               &&route.object&&runtime.eligible(route.window)&&h.drainedResourceSerial==nativeSerial
               &&h.drainedResourceSerial==h.resourceSerial.load()){
                // The source generation AND epoch were read together under
                // surfaceMutex. Draining a newer retirement must not pair the
                // OLD lifetime with a newly captured otherwise-stable serial.
                GameCandidate candidate;
                const bool qualified=candidate.find(provider,route);
                h.provenance.discardPendingWrites(candidate.identity,nullptr);
                if(qualified){
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
            GlDeferredCopyResolution resolved;
            const auto inputs=h.provenance.observe(identity,usage,candidate.exact,&resolved);
            if(resolved.copies)runtime.textureDeferredCopyReport(resolved.copies,resolved.inputs,revision);
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
// Declared before the audit guard, so its destructor restores the native
// outgoing error AFTER that guard records its CPU-only diagnostics.
struct NativeTransferError {
    DWORD value=GetLastError();
    void incoming()const{SetLastError(value);}
    void returned(){value=GetLastError();}
    ~NativeTransferError(){SetLastError(value);}
};
template<unsigned Index> void WINAPI blitFramebuffer(int sx0,int sy0,int sx1,int sy1,int dx0,int dy0,int dx1,int dy1,unsigned mask,unsigned filter){
    NativeTransferError error;
    auto &hook=hooks().blits[Index];
    GameTransfer transfer(hook.providers.load(std::memory_order_acquire),GlColorTransfer::blit(sx0,sy0,sx1,sy1,dx0,dy0,dx1,dy1,mask,filter));
    error.incoming();
    using Function=void(WINAPI*)(int,int,int,int,int,int,int,int,unsigned,unsigned);
    reinterpret_cast<Function>(hook.original.load(std::memory_order_acquire))(sx0,sy0,sx1,sy1,dx0,dy0,dx1,dy1,mask,filter);
    error.returned();transfer.complete();
}
template<unsigned Index> void WINAPI copyTexSubImage2D(unsigned target,int level,int dx,int dy,int x,int y,int width,int height){
    NativeTransferError error;
    auto &hook=hooks().copies[Index];
    GameTransfer transfer(hook.providers.load(std::memory_order_acquire),GlColorTransfer::copy(target,level,dx,dy,x,y,width,height));
    error.incoming();
    using Function=void(WINAPI*)(unsigned,int,int,int,int,int,int,int);
    reinterpret_cast<Function>(hook.original.load(std::memory_order_acquire))(target,level,dx,dy,x,y,width,height);
    error.returned();transfer.complete();
}
template<unsigned Index> void WINAPI copyImage(unsigned src,unsigned srcTarget,int srcLevel,int sx,int sy,int sz,
                                               unsigned dst,unsigned dstTarget,int dstLevel,int dx,int dy,int dz,int width,int height,int depth){
    NativeTransferError error;
    auto &hook=hooks().imageCopies[Index];
    auto call=GlColorTransfer::image(src,srcTarget,srcLevel,sx,sy,sz,dst,dstTarget,dstLevel,dx,dy,dz,width,height,depth);
    call.imageApis=hook.contracts.load(std::memory_order_acquire);
    GameTransfer transfer(hook.providers.load(std::memory_order_acquire),call);
    error.incoming();
    using Function=void(WINAPI*)(unsigned,unsigned,int,int,int,int,unsigned,unsigned,int,int,int,int,int,int,int);
    reinterpret_cast<Function>(hook.original.load(std::memory_order_acquire))(src,srcTarget,srcLevel,sx,sy,sz,dst,dstTarget,dstLevel,dx,dy,dz,width,height,depth);
    error.returned();transfer.complete();
}
template<std::size_t... Index> auto imageCopyCallbacks(std::index_sequence<Index...>){return std::array<void*,sizeof...(Index)>{{reinterpret_cast<void*>(&copyImage<Index>)...}};}
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
template<std::size_t N> bool resourceHook(std::array<DrawHook,N> &slots,const std::array<void*,N> &callbacks,void *target,unsigned providers,unsigned contracts=0){
    if(!target)return false;
    std::lock_guard<std::mutex> lock(hooks().procedureMutex);
    for(std::size_t i=0;i<N;++i){auto &entry=slots[i];
        if(entry.target==target){entry.providers.fetch_or(providers,std::memory_order_release);entry.contracts.fetch_or(contracts,std::memory_order_release);return entry.original.load()!=nullptr;}
        if(entry.target)continue;
        entry.target=target;entry.providers.store(providers,std::memory_order_release);entry.contracts.store(contracts,std::memory_order_release);
        if(installPublished(target,callbacks[i],nullptr,&entry.original))return true;
        entry.providers.store(0);entry.target=nullptr;return false;
    }
    return false;
}
// Register the exact native code address, not one global trampoline per GL
// name. WGL procedures can differ between pixel formats/contexts. A lookup
// grants code coverage ONLY: these callbacks still need existing route/resource
// proof before any GL inspection, and always forward their own original once.
// Shared bounded slots/lock also serialize lookups against presenter setup.
bool installGlProcedure(unsigned provider,const char *name,void *target){
    if(!target||!name)return false;
    auto &h=hooks();const auto providers=1u<<provider;
    for(unsigned kind=0;kind<drawKinds;++kind)for(const auto *alias:glDrawNames[kind])
        if(alias&&std::strcmp(name,alias)==0)return resourceHook(h.draws[kind],drawCallbacks[kind],target,providers);
    static const auto blits=blitCallbacks(std::make_index_sequence<32>{}),copies=copyCallbacks(std::make_index_sequence<32>{});
    for(const auto *alias:{"glBlitFramebuffer","glBlitFramebufferEXT","glBlitFramebufferANGLE","glBlitFramebufferNV"})
        if(std::strcmp(name,alias)==0)return resourceHook(h.blits,blits,target,providers);
    for(const auto *alias:{"glCopyTexSubImage2D","glCopyTexSubImage2DEXT"})
        if(std::strcmp(name,alias)==0)return resourceHook(h.copies,copies,target,providers);
    static const auto images=imageCopyCallbacks(std::make_index_sequence<32>{});
    const char *imageNames[]{"glCopyImageSubData","glCopyImageSubDataEXT","glCopyImageSubDataOES","glCopyImageSubDataNV"};
    for(unsigned api=0;api<4;++api)if(std::strcmp(name,imageNames[api])==0)return resourceHook(h.imageCopies,images,target,providers,1u<<api);
    static const auto textures=deleteCallbacks<false>(std::make_index_sequence<12>{}),renderbuffers=deleteCallbacks<true>(std::make_index_sequence<12>{});
    if(std::strcmp(name,"glDeleteTextures")==0)return resourceHook(h.textureDeletes,textures,target,providers);
    if(std::strcmp(name,"glDeleteRenderbuffers")==0)return resourceHook(h.renderbufferDeletes,renderbuffers,target,providers);
    return false;
}
bool installTransferHooks(unsigned provider,void *user,bool *complete){
    bool any=false;
    for(const auto *name:{"glBlitFramebuffer","glBlitFramebufferEXT","glBlitFramebufferANGLE","glBlitFramebufferNV","glCopyTexSubImage2D","glCopyTexSubImage2DEXT","glCopyImageSubData","glCopyImageSubDataEXT","glCopyImageSubDataOES","glCopyImageSubDataNV"}){
        const auto target=resolve(name,user);const bool attached=installGlProcedure(provider,name,target);
        if(target&&!attached&&complete)*complete=false;
        any=attached||any;
    }
    return any;
}
bool installResourceHooks(unsigned provider,void *user,bool *complete){
    const auto textures=resolve("glDeleteTextures",user),renderbuffers=resolve("glDeleteRenderbuffers",user);
    const bool deletedTextures=installGlProcedure(provider,"glDeleteTextures",textures);
    const bool deletedRenderbuffers=installGlProcedure(provider,"glDeleteRenderbuffers",renderbuffers);
    if(complete&&((textures&&!deletedTextures)||(renderbuffers&&!deletedRenderbuffers)))*complete=false;
    installTransferHooks(provider,user,complete);
    if(!provider&&!installWglPbufferHooks()&&complete)*complete=false;
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
bool installWglPbufferHooks(){
    static const auto gets=pbufferGetCallbacks(std::make_index_sequence<16>{}),releases=pbufferReleaseCallbacks(std::make_index_sequence<16>{}),destroys=pbufferDestroyCallbacks(std::make_index_sequence<16>{});
    static const auto binds=pbufferBindCallbacks(std::make_index_sequence<16>{});
    auto &h=hooks();bool complete=true;
    const auto bind=nativeWglProcedure("wglBindTexImageARB");
    if(bind&&!resourceHook(h.pbufferBinds,binds,bind,1))complete=false;
    for(const auto *suffix:{"ARB","EXT"}){
        const std::string family(suffix);
        const auto get=nativeWglProcedure(("wglGetPbufferDC"+family).c_str());
        const auto release=nativeWglProcedure(("wglReleasePbufferDC"+family).c_str());
        const auto destroy=nativeWglProcedure(("wglDestroyPbuffer"+family).c_str());
        // Enable acquisition LAST. A partially installed lifecycle family must
        // not publish a pbuffer/DC which can later disappear unobserved.
        if(get&&(!release||!destroy||!resourceHook(h.pbufferReleases,releases,release,1)
           ||!resourceHook(h.pbufferDestroys,destroys,destroy,1)||!resourceHook(h.pbufferGets,gets,get,1)))complete=false;
    }
    return complete;
}
template<unsigned Index> HGLRC WINAPI createAttributedWgl(HDC dc,HGLRC share,const int *attributes){
    auto &entry=hooks().attributedCreates[Index];
    const auto context=reinterpret_cast<WglCreateAttributes>(entry.original.load(std::memory_order_acquire))(dc,share,attributes);
    contextCreated(0,0,reinterpret_cast<std::uintptr_t>(context),reinterpret_cast<std::uintptr_t>(share));return context;
}
template<std::size_t... Index> auto attributedCallbacks(std::index_sequence<Index...>){
    return std::array<void*,sizeof...(Index)>{{reinterpret_cast<void*>(&createAttributedWgl<Index>)...}};
}
void discoverCurrentWgl(HDC dc,HGLRC context)noexcept;
template<unsigned Index> BOOL WINAPI madeSeparateCurrentWgl(HDC draw,HDC read,HGLRC context){
    const auto &entry=hooks().separateCurrents[Index];
    const auto result=reinterpret_cast<WglMakeSeparate>(entry.original.load(std::memory_order_acquire))(draw,read,context);
    const auto nativeError=GetLastError();if(result)discoverCurrentWgl(draw,context);
    SetLastError(nativeError);return result;
}
template<std::size_t... Index> auto separateCurrentCallbacks(std::index_sequence<Index...>){
    return std::array<void*,sizeof...(Index)>{{reinterpret_cast<void*>(&madeSeparateCurrentWgl<Index>)...}};
}
bool installWglCurrentProcedure(const char *name,void *target){
    if(std::strcmp(name,"wglMakeContextCurrentARB")!=0&&std::strcmp(name,"wglMakeContextCurrentEXT")!=0)return false;
    static const auto callbacks=separateCurrentCallbacks(std::make_index_sequence<16>{});
    return resourceHook(hooks().separateCurrents,callbacks,target,1);
}
// Renderer dispatch tables may predate the QML bootstrap and never perform
// another application lookup. Resolve the existing signature inventory on the
// thread AFTER its successful native bind, not on the polling worker (which
// has no current context). This installs code only: NO GL state query, render
// mutex, context/share inference, presentation selection or resource evidence.
void discoverCurrentWgl(HDC dc,HGLRC context)noexcept{
    auto &h=hooks();if(!dc||!context||h.stopping.load()||!h.deleteWgl||!h.currentWgl||!h.currentDc)return;
    struct Cache {
        std::uint64_t revision=0;unsigned count=0;bool discovering=false;
        std::array<std::pair<HDC,HGLRC>,64> tables{};
    };
    static thread_local Cache cache;
    if(cache.discovering||h.currentWgl()!=context||h.currentDc()!=dc)return;
    const auto revision=h.wglDispatchRevision.load();
    if(cache.revision!=revision){cache.revision=revision;cache.count=0;}
    const auto key=std::make_pair(dc,context);
    for(unsigned i=0;i<cache.count;++i)if(cache.tables[i]==key)return;
    cache.discovering=true;bool completed=false;
    try{
        bool complete=true;installResourceHooks(0,nullptr,&complete);installDrawHooks(0,nullptr,&complete);
        for(const auto *name:{"wglMakeContextCurrentARB","wglMakeContextCurrentEXT"}){
            const auto target=nativeWglProcedure(name);
            if(target&&!installWglCurrentProcedure(name,target))complete=false;
        }
        completed=complete; // Unsupported/null is fine; a failed attachment is retried on the next binding.
    }catch(...){} // Optional code discovery never changes native binding success.
    cache.discovering=false;
    // Avoid a per-frame resolver sweep. This bounded thread-local CODE cache
    // is invalidated around native deletion / on observed creation, including
    // handle reuse. Overflow simply rescans; it cannot authorize any resource.
    if(completed&&revision==h.wglDispatchRevision.load()&&cache.count<cache.tables.size()
       &&h.currentWgl()==context&&h.currentDc()==dc)cache.tables[cache.count++]=key;
}
BOOL WINAPI madeCurrentWgl(HDC dc,HGLRC context){
    const auto result=hooks().makeWgl(dc,context);const auto nativeError=GetLastError();
    if(result)discoverCurrentWgl(dc,context);
    SetLastError(nativeError);return result;
}
PROC WINAPI queriedWgl(LPCSTR name){
    const auto result=hooks().nativeWglProc(name);const auto nativeError=GetLastError();
    if(result&&name)try{
        const auto value=reinterpret_cast<std::intptr_t>(result);
        if(value!=1&&value!=2&&value!=3&&value!=-1){
            if(name[0]=='g'&&name[1]=='l')installGlProcedure(0,name,reinterpret_cast<void*>(result));
            installWglCurrentProcedure(name,reinterpret_cast<void*>(result));
            if(std::strcmp(name,"wglCreateContextAttribsARB")==0){
                static const auto callbacks=attributedCallbacks(std::make_index_sequence<16>{});
                resourceHook(hooks().attributedCreates,callbacks,reinterpret_cast<void*>(result),1);
            }
            if(std::strncmp(name,"wglGetPbufferDC",15)==0||std::strncmp(name,"wglReleasePbufferDC",19)==0
               ||std::strncmp(name,"wglDestroyPbuffer",17)==0||std::strcmp(name,"wglBindTexImageARB")==0)installWglPbufferHooks();
        }
    }catch(...){} // a failed optional detour never changes the returned native address
    SetLastError(nativeError);return result;
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
bool installDrawHooks(unsigned provider,void *user,bool *complete){
    bool any=false;
    for(const auto &names:glDrawNames)for(const auto *name:names)if(name){
        const auto target=resolve(name,user);const bool attached=installGlProcedure(provider,name,target);
        if(target&&!attached&&complete)*complete=false;
        any=attached||any;
    }
    return any;
}
void discoverCurrentEgl(unsigned provider,void *display,void *draw,void *read,void *context)noexcept{
    auto &h=hooks();if(!provider||!display||!context||h.stopping.load())return;
    auto &p=h.egl[provider-1];
    struct Entry {unsigned provider=0;void *display=nullptr,*context=nullptr;std::uint64_t revision=0;};
    struct Cache {std::array<Entry,64> entries{};unsigned count=0;bool discovering=false;};
    static thread_local Cache cache;if(cache.discovering)return;
    if(!p.destroyContext||p.current()!=context||p.currentDisplay()!=display||p.currentSurface(0x3059)!=draw||p.currentSurface(0x305a)!=read)return;
    const auto revision=p.dispatchRevision.load();unsigned slot=cache.count;
    for(unsigned i=0;i<cache.count;++i)if(cache.entries[i].provider==provider&&cache.entries[i].display==display&&cache.entries[i].context==context){
        if(cache.entries[i].revision==revision)return;slot=i;break;
    }
    cache.discovering=true;bool complete=true;
    try{installResourceHooks(provider,&p,&complete);installDrawHooks(provider,&p,&complete);}catch(...){complete=false;}
    cache.discovering=false;
    if(complete&&revision==p.dispatchRevision.load()&&slot<cache.entries.size()
       &&p.current()==context&&p.currentDisplay()==display&&p.currentSurface(0x3059)==draw&&p.currentSurface(0x305a)==read){
        cache.entries[slot]={provider,display,context,revision};if(slot==cache.count)++cache.count;
    }
}
template<unsigned Index> unsigned WINAPI madeCurrentEgl(void *display,void *draw,void *read,void *context){
    auto &hook=hooks().eglCurrents[Index];
    const auto result=reinterpret_cast<EglMake>(hook.original.load(std::memory_order_acquire))(display,draw,read,context);
    const auto error=GetLastError();
    if(result)for(unsigned provider=1;provider<=maximumEglProviders;++provider)
        if(hook.providers.load(std::memory_order_acquire)&(1u<<provider))discoverCurrentEgl(provider,display,draw,read,context);
    SetLastError(error);return result;
}
template<std::size_t... Index> auto eglCurrentCallbacks(std::index_sequence<Index...>){return std::array<void*,sizeof...(Index)>{{reinterpret_cast<void*>(&madeCurrentEgl<Index>)...}};}
bool installEglCurrentProcedure(unsigned provider,void *target){
    static const auto callbacks=eglCurrentCallbacks(std::make_index_sequence<32>{});
    return resourceHook(hooks().eglCurrents,callbacks,target,1u<<provider);
}
bool installEglAlias(unsigned provider,const char *name,void *target);
void disableEglTracking(unsigned provider)noexcept{
    // A non-null known lifecycle target which could not be attached must not
    // leave resource proof enabled. Never block a resolver on renderMutex.
    auto &h=hooks();auto &p=h.egl[provider-1];p.lifecycleFault.store(true,std::memory_order_release);p.lifecycleReady.store(false,std::memory_order_release);
    p.dispatchRevision.fetch_add(1);h.resourceReset.fetch_or(1u<<provider);h.resourceSerial.fetch_add(1);
}
void *queriedEglAt(unsigned providers,EglProc native,const char *name){
    EglDispatchScope scope(EglDispatchOp::Lookup,providers,{{reinterpret_cast<std::uintptr_t>(name),0,0,0}});
    const auto result=native(name);const auto error=GetLastError();
    if(result&&name&&!EglInternalLookup::active()&&scope.observes())try{
        const auto provider=scope.provider();
        if(name[0]=='g'&&name[1]=='l')installGlProcedure(provider,name,result);
        else if(std::strcmp(name,"eglMakeCurrent")==0)installEglCurrentProcedure(provider,result);
        else for(const auto *known:eglDispatchNames)if(std::strcmp(name,known)==0){
            if(!installEglAlias(provider,name,result))disableEglTracking(provider);break;
        }
    }catch(...){if(scope.observes())disableEglTracking(scope.provider());}
    SetLastError(error);return result;
}
template<unsigned Index> void *WINAPI queriedEgl(const char *name){
    return queriedEglAt(1u<<(Index+1),hooks().egl[Index].nativeProc,name);
}
void process(unsigned provider,std::uintptr_t context,HWND window,DrawableKey drawable,int width,int height,bool externalProtectionPossible=false){
    auto &h=hooks();if(!context||presenting||h.stopping.load())return;
    auto *egl=provider?&h.egl[provider-1]:nullptr;
    const auto api=egl&&egl->api?egl->api():0x30a2u;
    if(egl&&api!=0x30a0u&&api!=0x30a2u)return;
    const auto renderer=api==0x30a0u?Renderer::OpenGLES:Renderer::OpenGL;
    if(!Runtime::instance().accepts(renderer))return;
    if(!provider)discoverCurrentWgl(h.currentDc?h.currentDc():nullptr,reinterpret_cast<HGLRC>(context));
    else discoverCurrentEgl(provider,reinterpret_cast<void*>(drawable.first),reinterpret_cast<void*>(drawable.second),egl->currentSurface(0x305a),reinterpret_cast<void*>(context));
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
BOOL WINAPI removeWgl(HGLRC context){
    auto &h=hooks();h.wglDispatchRevision.fetch_add(1);
    const auto okay=h.deleteWgl(context);const auto nativeError=GetLastError();h.wglDispatchRevision.fetch_add(1);
    if(okay)try{removeContextState(0,0,reinterpret_cast<std::uintptr_t>(context));}catch(...){}
    SetLastError(nativeError);return okay;
}
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
EglProvider *observedEgl(const EglDispatchScope &scope)noexcept{
    const auto provider=scope.provider();if(!provider||provider>maximumEglProviders)return nullptr;
    auto &p=hooks().egl[provider-1];return p.lifecycleReady.load(std::memory_order_acquire)&&!p.lifecycleFault.load(std::memory_order_acquire)?&p:nullptr;
}
template<class Native,class Attribute> void *createSurfaceAt(EglDispatchOp operation,unsigned providers,Native native,void *display,void *config,void *window,const Attribute *attrs){
    EglDispatchScope scope(operation,providers,{{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(config),reinterpret_cast<std::uintptr_t>(window),reinterpret_cast<std::uintptr_t>(attrs)}});
    const auto surface=native(display,config,window,attrs);const auto error=GetLastError();
    if(auto *p=observedEgl(scope))trackSurface(*p,display,surface,window,attrs);
    SetLastError(error);return surface;
}
template<unsigned Index> void *WINAPI createSurface(void *display,void *config,void *window,const int *attrs){
    return createSurfaceAt(EglDispatchOp::WindowSurface,1u<<(Index+1),hooks().egl[Index].create,display,config,window,attrs);
}
template<unsigned Index> void *WINAPI createPlatformSurface(void *display,void *config,void *window,const std::intptr_t *attrs){
    return createSurfaceAt(EglDispatchOp::PlatformSurface,1u<<(Index+1),hooks().egl[Index].createPlatform,display,config,window,attrs);
}
template<unsigned Index> void *WINAPI createPlatformSurfaceExt(void *display,void *config,void *window,const int *attrs){
    return createSurfaceAt(EglDispatchOp::PlatformSurfaceExt,1u<<(Index+1),hooks().egl[Index].createPlatformExt,display,config,window,attrs);
}
void *createdEglContextAt(unsigned providers,EglCreate native,void *display,void *config,void *share,const int *attributes){
    EglDispatchScope scope(EglDispatchOp::Context,providers,{{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(config),reinterpret_cast<std::uintptr_t>(share),reinterpret_cast<std::uintptr_t>(attributes)}});
    const auto context=native(display,config,share,attributes);const auto error=GetLastError();
    if(observedEgl(scope))contextCreated(scope.provider(),reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(context),reinterpret_cast<std::uintptr_t>(share));
    SetLastError(error);return context;
}
template<unsigned Index> void *WINAPI createdEglContext(void *display,void *config,void *share,const int *attributes){
    return createdEglContextAt(1u<<(Index+1),hooks().egl[Index].createContext,display,config,share,attributes);
}
void *createdPbufferAt(unsigned providers,EglPbufferCreate native,void *display,void *config,const int *attributes){
    EglDispatchScope scope(EglDispatchOp::Pbuffer,providers,{{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(config),reinterpret_cast<std::uintptr_t>(attributes),0}});
    auto &h=hooks();const auto surface=native(display,config,attributes);const auto error=GetLastError();
    if(auto *provider=surface?observedEgl(scope):nullptr)try{
        auto &p=*provider;bool unprotected=true,terminated=attributes==nullptr;
        if(attributes)for(unsigned i=0;i<128;i+=2){
            if(attributes[i]==0x3038){terminated=true;break;}
            if(attributes[i]==0x32c0&&attributes[i+1])unprotected=false;
        }
        ResourceEvent retired;retired.kind=ResourceEvent::PbufferRetired;retired.providers=1u<<scope.provider();
        {
            std::lock_guard<std::mutex> lock(h.surfaceMutex);h.resourceSerial.fetch_add(1);
            const DrawableKey key{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(surface)};
            const auto old=p.pbuffers.find(key);if(old!=p.pbuffers.end()){retired.pbuffer=old->second;p.pbuffers.erase(old);}
            if(unprotected&&terminated&&p.pbuffers.size()<128&&p.nextPbuffer)p.pbuffers.emplace(key,p.nextPbuffer++);
        }
        if(retired.pbuffer)resourceEvent(retired);
    }catch(...){}
    SetLastError(error);return surface;
}
template<unsigned Index> void *WINAPI createdPbuffer(void *display,void *config,const int *attributes){
    return createdPbufferAt(1u<<(Index+1),hooks().egl[Index].createPbuffer,display,config,attributes);
}
unsigned bindEglTextureAt(unsigned providers,EglBindTexture native,void *display,void *surface,int buffer){
    const auto incoming=GetLastError();
    EglDispatchScope scope(EglDispatchOp::BindTexture,providers,{{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(surface),static_cast<std::uintptr_t>(buffer),0}});
    auto *provider=observedEgl(scope);if(!provider)return native(display,surface,buffer);
    auto &h=hooks();auto &p=*provider;std::uintptr_t generation=0;std::uint64_t nativeSerial=0;
    try{
        const bool current=p.currentDisplay()==display&&rendererFor(scope.provider())==Renderer::OpenGLES;
        std::lock_guard<std::mutex> lock(h.surfaceMutex);
        const auto found=p.pbuffers.find({reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(surface)});
        if(current&&found!=p.pbuffers.end())generation=found->second;
        nativeSerial=h.resourceSerial.load();
    }catch(...){}
    GameImageBinding capture(scope.provider(),GlColorResource::pbuffer(generation,buffer==0x3084?0x0405u:0),nativeSerial);
    SetLastError(incoming);const auto result=native(display,surface,buffer);const auto error=GetLastError();
    if(result&&capture.captured)try{
        int target=0,format=0,level=-1;
        if(p.query(display,surface,0x3081,&target)&&target==0x305f
           &&p.query(display,surface,0x3080,&format)&&(format==0x305d||format==0x305e)
           &&p.query(display,surface,0x3083,&level)&&level==0)capture.complete();
    }catch(...){}
    SetLastError(error);return result;
}
template<unsigned Index> unsigned WINAPI bindEglTexture(void *display,void *surface,int buffer){
    return bindEglTextureAt(1u<<(Index+1),hooks().egl[Index].bindTexture,display,surface,buffer);
}
void retireEglSurface(unsigned provider,void *display,void *surface)noexcept{
    auto &h=hooks();auto &p=h.egl[provider-1];try{
        const DrawableKey drawable{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(surface)};
        ResourceEvent retired;retired.kind=ResourceEvent::PbufferRetired;retired.providers=1u<<provider;
        {std::lock_guard<std::mutex> lock(h.surfaceMutex);h.resourceSerial.fetch_add(1);p.surfaces.erase(drawable);
            const auto old=p.pbuffers.find(drawable);if(old!=p.pbuffers.end()){retired.pbuffer=old->second;p.pbuffers.erase(old);}}
        if(retired.pbuffer)resourceEvent(retired);
        std::lock_guard<std::mutex> lock(h.renderMutex);for(const auto &entry:h.contexts)
            if(std::get<0>(entry.first)==provider&&entry.second->drawable==drawable){
                Runtime::instance().releasePresentation(entry.second->renderer,reinterpret_cast<std::uintptr_t>(entry.second.get()));
                entry.second->window=nullptr;entry.second->drawable={};
            }
    }catch(...){}
}
unsigned removeSurfaceAt(unsigned providers,EglDestroy native,void *display,void *surface){
    EglDispatchScope scope(EglDispatchOp::DestroySurface,providers,{{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(surface),0,0}});
    const auto okay=native(display,surface);const auto error=GetLastError();
    if(okay)for(unsigned provider=1;provider<=maximumEglProviders;++provider)
        if(scope.retirementCandidates()&(1u<<provider))retireEglSurface(provider,display,surface);
    SetLastError(error);return okay;
}
template<unsigned Index> unsigned WINAPI removeSurface(void *display,void *surface){
    return removeSurfaceAt(1u<<(Index+1),hooks().egl[Index].destroySurface,display,surface);
}
unsigned removeContextAt(unsigned providers,EglDestroy native,void *display,void *context){
    EglDispatchScope scope(EglDispatchOp::DestroyContext,providers,{{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(context),0,0}});
    auto &h=hooks();const auto candidates=scope.retirementCandidates();
    for(unsigned provider=1;provider<=maximumEglProviders;++provider)if(candidates&(1u<<provider))h.egl[provider-1].dispatchRevision.fetch_add(1);
    const auto okay=native(display,context);const auto error=GetLastError();
    for(unsigned provider=1;provider<=maximumEglProviders;++provider)if(candidates&(1u<<provider)){
        h.egl[provider-1].dispatchRevision.fetch_add(1);
        if(okay)try{removeContextState(provider,reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(context));}catch(...){}
    }
    SetLastError(error);return okay;
}
template<unsigned Index> unsigned WINAPI removeContext(void *display,void *context){
    return removeContextAt(1u<<(Index+1),hooks().egl[Index].destroyContext,display,context);
}
bool extension(const char *list,const char *name){
    if(!list)return false;
    const auto size=std::strlen(name);const char *found=list;
    while((found=std::strstr(found,name))){
        if((found==list||found[-1]==' ')&&(found[size]==' '||found[size]=='\0'))return true;
        found+=size;
    }return false;
}
unsigned swapEglAt(unsigned providers,EglSwap native,void *display,void *surface){
    const auto incoming=GetLastError();
    EglDispatchScope scope(EglDispatchOp::Swap,providers,{{reinterpret_cast<std::uintptr_t>(display),reinterpret_cast<std::uintptr_t>(surface),0,0}});
    auto &h=hooks();auto *provider=observedEgl(scope);
    if(provider&&!presenting&&!h.stopping.load())try{
        auto &p=*provider;HWND window=nullptr;
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
                process(scope.provider(),reinterpret_cast<std::uintptr_t>(context),window,drawable,width,height,protectionPossible);
        }
    }catch(...){} // Optional effects must never prevent the native present.
    Recursion guard;SetLastError(incoming);return native(display,surface);
}
template<unsigned Index> unsigned WINAPI swapEgl(void *display,void *surface){
    return swapEglAt(1u<<(Index+1),hooks().egl[Index].swap,display,surface);
}
// Each returned lifecycle address keeps its own TYPED native trampoline.
// Never overwrite a provider's public-stub original with a body original: an
// ordinary imported call can execute BOTH detours on the same native stack.
template<EglDispatchOp Operation,unsigned Slot> struct EglAliasCallback;
#define BSI_EGL_ALIAS_CALLBACK(Kind,Result,Native,Params,Expression) \
    template<unsigned Slot> struct EglAliasCallback<EglDispatchOp::Kind,Slot> { \
        static Result WINAPI call Params { \
            auto &entry=hooks().eglAliases[static_cast<unsigned>(EglDispatchOp::Kind)][Slot]; \
            const auto providers=entry.providers.load(std::memory_order_acquire); \
            const auto native=reinterpret_cast<Native>(entry.original.load(std::memory_order_acquire)); \
            return Expression; \
        } \
    };
BSI_EGL_ALIAS_CALLBACK(WindowSurface,void*,EglCreate,(void *d,void *c,void *w,const int *a),(createSurfaceAt(EglDispatchOp::WindowSurface,providers,native,d,c,w,a)))
BSI_EGL_ALIAS_CALLBACK(PlatformSurface,void*,EglPlatformCreate,(void *d,void *c,void *w,const std::intptr_t *a),(createSurfaceAt(EglDispatchOp::PlatformSurface,providers,native,d,c,w,a)))
BSI_EGL_ALIAS_CALLBACK(PlatformSurfaceExt,void*,EglCreate,(void *d,void *c,void *w,const int *a),(createSurfaceAt(EglDispatchOp::PlatformSurfaceExt,providers,native,d,c,w,a)))
BSI_EGL_ALIAS_CALLBACK(Context,void*,EglCreate,(void *d,void *c,void *s,const int *a),(createdEglContextAt(providers,native,d,c,s,a)))
BSI_EGL_ALIAS_CALLBACK(Pbuffer,void*,EglPbufferCreate,(void *d,void *c,const int *a),(createdPbufferAt(providers,native,d,c,a)))
BSI_EGL_ALIAS_CALLBACK(DestroySurface,unsigned,EglDestroy,(void *d,void *s),(removeSurfaceAt(providers,native,d,s)))
BSI_EGL_ALIAS_CALLBACK(DestroyContext,unsigned,EglDestroy,(void *d,void *c),(removeContextAt(providers,native,d,c)))
BSI_EGL_ALIAS_CALLBACK(Swap,unsigned,EglSwap,(void *d,void *s),(swapEglAt(providers,native,d,s)))
BSI_EGL_ALIAS_CALLBACK(BindTexture,unsigned,EglBindTexture,(void *d,void *s,int b),(bindEglTextureAt(providers,native,d,s,b)))
BSI_EGL_ALIAS_CALLBACK(Lookup,void*,EglProc,(const char *n),(queriedEglAt(providers,native,n)))
#undef BSI_EGL_ALIAS_CALLBACK
template<EglDispatchOp Operation,std::size_t... Slot> auto eglAliasCallbacks(std::index_sequence<Slot...>){
    return std::array<void*,sizeof...(Slot)>{{reinterpret_cast<void*>(&EglAliasCallback<Operation,Slot>::call)...}};
}
bool installEglAlias(unsigned provider,const char *name,void *target){
    if(!target||!name||!provider||provider>maximumEglProviders)return false;
    auto &h=hooks();auto &p=h.egl[provider-1];
    // If lookup returns the ALREADY covered public stub, preserve that exact
    // callback/native pair instead of attempting an incompatible second hook.
    if(target==reinterpret_cast<void*>(GetProcAddress(p.module,name))){
        if(originalPresentationFunction(target))return true;
        // Startup installs each public callback itself and the public swap
        // LAST. A simultaneous lookup must not replace that pending typed
        // callback; pre-ready calls remain native/unowned until it is enabled.
        if(p.attaching.load(std::memory_order_acquire))return true;
    }
    static const std::array<std::array<void*,eglAliasSlots>,eglDispatchNames.size()> callbacks{{
        eglAliasCallbacks<EglDispatchOp::WindowSurface>(std::make_index_sequence<eglAliasSlots>{}),
        eglAliasCallbacks<EglDispatchOp::PlatformSurface>(std::make_index_sequence<eglAliasSlots>{}),
        eglAliasCallbacks<EglDispatchOp::PlatformSurfaceExt>(std::make_index_sequence<eglAliasSlots>{}),
        eglAliasCallbacks<EglDispatchOp::Context>(std::make_index_sequence<eglAliasSlots>{}),
        eglAliasCallbacks<EglDispatchOp::Pbuffer>(std::make_index_sequence<eglAliasSlots>{}),
        eglAliasCallbacks<EglDispatchOp::DestroySurface>(std::make_index_sequence<eglAliasSlots>{}),
        eglAliasCallbacks<EglDispatchOp::DestroyContext>(std::make_index_sequence<eglAliasSlots>{}),
        eglAliasCallbacks<EglDispatchOp::Swap>(std::make_index_sequence<eglAliasSlots>{}),
        eglAliasCallbacks<EglDispatchOp::BindTexture>(std::make_index_sequence<eglAliasSlots>{}),
        eglAliasCallbacks<EglDispatchOp::Lookup>(std::make_index_sequence<eglAliasSlots>{})
    }};
    for(unsigned kind=0;kind<eglDispatchNames.size();++kind)if(std::strcmp(name,eglDispatchNames[kind])==0)
        return resourceHook(h.eglAliases[kind],callbacks[kind],target,1u<<provider);
    return false;
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
        bool complete=install(reinterpret_cast<void*>(GetProcAddress(module,"eglGetProcAddress")),reinterpret_cast<void*>(&queriedEgl<Index>),reinterpret_cast<void**>(&p.nativeProc));
        const auto current=reinterpret_cast<void*>(GetProcAddress(module,"eglMakeCurrent"));
        if(current&&!installEglCurrentProcedure(Index+1,current))complete=false;
        const auto optional=[&](const char *name,void *callback,void **original){
            const auto entry=reinterpret_cast<void*>(GetProcAddress(module,name));
            if(entry&&!install(entry,callback,original))complete=false;
        };
        optional("eglCreateContext",reinterpret_cast<void*>(&createdEglContext<Index>),reinterpret_cast<void**>(&p.createContext));
        optional("eglCreatePbufferSurface",reinterpret_cast<void*>(&createdPbuffer<Index>),reinterpret_cast<void**>(&p.createPbuffer));
        optional("eglBindTexImage",reinterpret_cast<void*>(&bindEglTexture<Index>),reinterpret_cast<void**>(&p.bindTexture));
        optional("eglCreatePlatformWindowSurface",reinterpret_cast<void*>(&createPlatformSurface<Index>),reinterpret_cast<void**>(&p.createPlatform));
        optional("eglCreatePlatformWindowSurfaceEXT",reinterpret_cast<void*>(&createPlatformSurfaceExt<Index>),reinterpret_cast<void**>(&p.createPlatformExt));
        // Cached lifecycle/resolver pointers can predate bootstrap just like
        // cached GL tables. Query only this bounded PUBLIC name/signature list;
        // do not inspect private offsets, execute GL or infer resource sharing.
        // Acquisition/presentation callbacks stay dormant until every non-null
        // known lifecycle alias has its retirement-safe native pair installed.
        for(const auto *name:eglDispatchNames){
            const auto target=nativeEglProcedure(p,name);
            if(target&&!installEglAlias(Index+1,name,target))complete=false;
        }
        const auto target=nativeEglProcedure(p,"eglMakeCurrent");
        if(target&&!installEglCurrentProcedure(Index+1,target))complete=false;
        p.lifecycleReady.store(complete,std::memory_order_release);
        // Publish the externally visible public-swap hook only after alias
        // closure. Late-loaded providers cannot look attached while their
        // newly created window surfaces are still intentionally untracked.
        if(!install(reinterpret_cast<void*>(GetProcAddress(module,"eglSwapBuffers")),reinterpret_cast<void*>(&swapEgl<Index>),reinterpret_cast<void**>(&p.swap)))disableEglTracking(Index+1);
    }
    p.attaching.store(false,std::memory_order_release);
}
void scan(){
    auto &h=hooks();
    if(!h.gl){h.gl=GetModuleHandleW(L"opengl32.dll");if(h.gl){h.currentWgl=exportFunction<WglCurrent>(h.gl,"wglGetCurrentContext");h.currentDc=exportFunction<WglCurrentDc>(h.gl,"wglGetCurrentDC");h.wglProc=exportFunction<WglProc>(h.gl,"wglGetProcAddress");}}
    if(h.gl&&h.currentWgl&&h.currentDc&&h.wglProc){
        if(!h.nativeWglProc)install(reinterpret_cast<void*>(GetProcAddress(h.gl,"wglGetProcAddress")),reinterpret_cast<void*>(&queriedWgl),reinterpret_cast<void**>(&h.nativeWglProc));
        if(!h.createWgl)install(reinterpret_cast<void*>(GetProcAddress(h.gl,"wglCreateContext")),reinterpret_cast<void*>(&createdWgl),reinterpret_cast<void**>(&h.createWgl));
        if(!h.shareWgl)install(reinterpret_cast<void*>(GetProcAddress(h.gl,"wglShareLists")),reinterpret_cast<void*>(&sharedWgl),reinterpret_cast<void**>(&h.shareWgl));
        if(!h.deleteWgl)install(reinterpret_cast<void*>(GetProcAddress(h.gl,"wglDeleteContext")),reinterpret_cast<void*>(&removeWgl),reinterpret_cast<void**>(&h.deleteWgl));
        // Lifetime observation precedes cached-table discovery; never cache a
        // context handle whose native retirement could go unobserved.
        if(h.deleteWgl&&!h.makeWgl)install(reinterpret_cast<void*>(GetProcAddress(h.gl,"wglMakeCurrent")),reinterpret_cast<void*>(&madeCurrentWgl),reinterpret_cast<void**>(&h.makeWgl));
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
