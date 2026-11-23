// Actual Win32/MinHook on authored WGL state, NOT driver/player/game pixels.
// Reproduce a supported ordering gap separately from the installed v4 report:
// copy into a NEW output, then consume that output on the selected drawable.
#include "Runtime.h"
#include "PresentationHooks.h"
#include "GlDrawCalls.h"
#undef BSI_GL_DRAW_CALLS
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <thread>
using namespace BsiShaders;
namespace {
void require(bool yes,const char *message){if(!yes){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
struct Fixture {
    HMODULE module=nullptr;
    template<class T>T fn(const char *name){const auto p=GetProcAddress(module,name);require(p!=nullptr,"required authored WGL export");return reinterpret_cast<T>(reinterpret_cast<void*>(p));}
    template<class T>T proc(const char *name){const auto p=fn<PROC(WINAPI*)(LPCSTR)>("wglGetProcAddress")(name);require(p!=nullptr,"required authored WGL procedure");return reinterpret_cast<T>(p);}
    LONG count(const char *name){return fn<LONG(*)()>(name)();}
    void current(HDC dc,HGLRC context){require(fn<BOOL(WINAPI*)(HDC,HGLRC)>("wglMakeCurrent")(dc,context)!=FALSE,"make actual authored context current");}
    void resource(unsigned mode,unsigned output=50){fn<void(*)(unsigned,unsigned)>("bsiFixtureResourceState")(mode,output);}
    void drawFlags(unsigned flags,unsigned secondary=0){fn<void(*)(unsigned,unsigned)>("bsiFixtureDrawFlags")(flags,secondary);}
    void draw(LONG bindings,const char *message,unsigned primitive=4){
        const auto calls=count("bsiFixtureDrawCalls");
        fn<void(*)(unsigned,unsigned,unsigned)>("bsiFixtureDraw")(static_cast<unsigned>(GlDrawKind::ElementsBaseVertex),0,primitive);
        require(count("bsiFixtureDrawCalls")==calls+1&&count("bsiFixtureArgumentsIntact")&&count("bsiFixtureBindingsRestored")&&!count("bsiFixtureInvalidGl"),"exact once-only draw original and restored native state");
        const auto actual=count("bsiFixtureLastPrivateBindings");
        if(actual!=bindings)std::fprintf(stderr,"private bindings inside original: expected %ld, actual %ld\n",bindings,actual);
        require(actual==bindings,message);
    }
    void copy(unsigned output){
        fn<void(*)(unsigned,unsigned,unsigned)>("bsiFixtureTransferState")(0,0,output);
        const auto calls=count("bsiFixtureTransferCalls");
        SetLastError(0x424277);fn<void(*)(unsigned,unsigned)>("bsiFixtureTransfer")(1,0);const auto error=GetLastError();
        require(error==0x424288&&fn<DWORD(*)()>("bsiFixtureTransferIncomingError")()==0x424277,"copy incoming/outgoing LastError remains transparent");
        require(count("bsiFixtureTransferCalls")==calls+1&&count("bsiFixtureTransferArgumentsIntact")&&count("bsiFixtureBindingsRestored")&&!count("bsiFixtureInvalidGl"),"exact once-only copy original and untouched native arguments/state");
    }
};
Fixture *callbackFixture=nullptr;
HDC callbackPresentationDc=nullptr;
void WINAPI presentInOriginal(){
    auto &f=*callbackFixture;
    const auto dc=f.fn<HDC(WINAPI*)()>("wglGetCurrentDC")();
    const auto context=f.fn<HGLRC(WINAPI*)()>("wglGetCurrentContext")();
    f.current(callbackPresentationDc,context);
    require(f.fn<BOOL(WINAPI*)(HDC)>("wglSwapBuffers")(callbackPresentationDc)!=FALSE,"presentation can advance while native transfer holds no lock");
    f.current(dc,context);
}
void WINAPI overwriteInOriginal(){
    auto &f=*callbackFixture;f.resource(2,58);
    f.draw(0,"an intervening native-callback draw is not owned by an in-flight copy");f.resource(4);
}
void WINAPI resetInOriginal(){
    std::thread worker([]{auto &r=Runtime::instance();r.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});r.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});});
    worker.join();
}
}
int main(int argc,char **argv){
    require(argc==2,"pass explicitly named authored WGL fixture, never PF/system DLLs");Fixture f;f.module=LoadLibraryA(argv[1]);require(f.module!=nullptr,"load authored fixture");
    const auto instance=GetModuleHandleW(nullptr);
    const auto root=CreateWindowExW(0,L"STATIC",L"copy order player",WS_OVERLAPPEDWINDOW,0,0,900,600,nullptr,nullptr,instance,nullptr);
    const auto child=CreateWindowExW(0,L"STATIC",L"copy order game",WS_CHILD,0,0,640,360,root,nullptr,instance,nullptr);
    require(root&&child,"owned native child HWND");const auto dc=GetDC(child);
    callbackFixture=&f;callbackPresentationDc=dc;
    const auto context=f.fn<HGLRC(WINAPI*)(HDC)>("wglCreateContext")(dc);require(context!=nullptr,"authored native context");
    require(setWglProviderForTesting(f.module),"explicit test-only WGL provider injection");
    auto &runtime=Runtime::instance();runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.start(reinterpret_cast<std::uintptr_t>(root));
    f.current(dc,context);require(f.fn<BOOL(WINAPI*)(HDC)>("wglSwapBuffers")(dc)!=FALSE,"selected owned presentation");
    auto *pbuffer=f.fn<void*(*)(HWND)>("bsiFixturePbuffer")(nullptr);require(pbuffer!=nullptr,"create authored pbuffer");
    auto pbDc=f.proc<HDC(WINAPI*)(void*)>("wglGetPbufferDCARB")(pbuffer);require(pbDc&&!WindowFromDC(pbDc),"observe actual native pbuffer lifetime/DC");
    f.fn<void(*)(int)>("bsiFixtureTransferErrorProbe")(1);
    const auto reset=[&]{runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});};
    const auto consumer=[&](unsigned output){f.current(dc,context);f.resource(1,output);f.draw(0,"legal LINEAR presentation consumer never receives private samplers");};
    const auto producer=[&](LONG bindings,const char *message){f.current(pbDc,context);f.resource(4);f.draw(bindings,message);};
    const auto pending=[&]{const auto a=runtime.metrics().textureDeferredAudit;require(a.valid(),"actual hook audit snapshot must have balanced queue/source/retirement gauges");return a;};
    const auto retired=[&](GlDeferredCopyExit reason){
        const auto a=pending();const auto why=static_cast<std::size_t>(reason);
        if(a.queued!=1||a.pending||a.exits[why]!=1)std::fprintf(stderr,"pending ledger expected %s: queued=%llu pending=%u reason=%llu\n",glDeferredExitLabels[why],static_cast<unsigned long long>(a.queued),a.pending,static_cast<unsigned long long>(a.exits[why]));
        require(a.queued==1&&!a.pending&&a.exits[why]==1,"native callbacks publish the exact pending retirement reason without another GL query");
        std::printf("PASS ledger retirement: %s; queued=1 pending=0 balanced=yes\n",glDeferredExitLabels[why]);
    };

    // Existing positive control: the selected consumer precedes the copy.
    consumer(50);producer(0,"pbuffer existence/namespace alone is not ownership");f.copy(50);
    require(runtime.metrics().textureTransferCalls==1&&runtime.metrics().textureTransferInputs==1,"existing consumed-output copy path stays qualified");
    producer(2,"existing copy AFTER consumption reaches material bindings inside the original");
    std::puts("PASS control: consumer-before-copy, exact typed native pbuffer, native LINEAR compositor, transparent copy ABI");

    // Unconsumed sibling outputs must NEVER grant upstream ownership.
    reset();consumer(50);producer(0,"fresh settings revoke the old native copy proof");f.copy(51);
    producer(0,"unconsumed sibling copy output must stay native");
    require(!runtime.metrics().textureTransferCalls&&!runtime.metrics().textureBindings&&!runtime.metrics().textureDeferredCopies,"unlinked copy metadata is not a qualified transfer or private sampler");
    require(pending().queued==1&&pending().pending==1&&pending().sources[2]==1,"native copy completion publishes admitted pending metadata, not a qualified link");
    consumer(50);
    require(pending().consumers[static_cast<std::size_t>(GlDeferredConsumerStop::NoDestination)]==1,"a different selected input is explicitly NOT an exact pending-destination match");
    std::puts("PASS negative: a submitted copy into an unconsumed sibling is not ownership");
    const auto isolated=f.fn<HGLRC(WINAPI*)(HDC)>("wglCreateContext")(dc);require(isolated!=nullptr,"independent namespace, never guessed sharing");
    f.current(dc,isolated);f.resource(1,51);f.draw(0,"another namespace's equal name stays a native LINEAR input");
    producer(0,"equal-name consumption in an isolated namespace cannot reconcile this copy");
    require(!runtime.metrics().textureDeferredCopies,"unshared namespace names cannot manufacture asynchronous links");
    require(pending().consumers[static_cast<std::size_t>(GlDeferredConsumerStop::OtherNamespace)]==1,"other-namespace probe records no numeric-name comparison or ownership");

    // NEW missing case: this very output is sampled AFTER the native copy.
    consumer(51);
    producer(2,"a LATER exact selected consumer must reconcile the earlier native copy before inspecting its pbuffer material producer");
    require(runtime.metrics().textureProducerDraws==1&&runtime.metrics().textureProducerBindings==2
            &&runtime.metrics().textureDeferredCopies==1&&runtime.metrics().textureDeferredInputs==1
            &&!runtime.metrics().textureTransferCalls,"late actual consumption reaches only its resource-linked material producer, separately from native call totals");
    retired(GlDeferredCopyExit::Reconciled);
    require(pending().consumers[static_cast<std::size_t>(GlDeferredConsumerStop::ExactDestination)]==1,"the existing exact native consumer is the only reconciliation match");

    // Presentation elapsed inside the native original counts against the
    // ORIGINAL capture deadline, not a fresh lifetime starting at return.
    reset();consumer(50);producer(0,"in-flight frame boundary starts native");
    f.fn<void(*)(void(WINAPI*)())>("bsiFixtureOnTransfer")(presentInOriginal);f.copy(57);
    f.fn<void(*)(void(WINAPI*)())>("bsiFixtureOnTransfer")(nullptr);
    f.current(dc,context);require(f.fn<BOOL(WINAPI*)(HDC)>("wglSwapBuffers")(dc)!=FALSE,"reach the original copy's inclusive deadline");
    consumer(57);producer(2,"a returning copy can reconcile at its original inclusive two-frame deadline");
    f.current(dc,context);require(f.fn<BOOL(WINAPI*)(HDC)>("wglSwapBuffers")(dc)!=FALSE,"advance beyond entry-time deadline");
    producer(0,"time spent INSIDE the native copy must not extend the original capture's two-frame deadline");

    reset();consumer(50);producer(0,"in-flight overwrite control starts native");
    f.fn<void(*)(void(WINAPI*)())>("bsiFixtureOnTransfer")(overwriteInOriginal);f.copy(58);
    f.fn<void(*)(void(WINAPI*)())>("bsiFixtureOnTransfer")(nullptr);
    consumer(58);producer(0,"an intervening destination write during the original revokes in-flight copy metadata before it can be queued");
    require(!runtime.metrics().textureDeferredCopies,"post-original queuing cannot resurrect metadata invalidated during the original");

    reset();consumer(50);producer(0,"overwritten pending output control begins native");f.copy(56);
    f.resource(2,56);f.draw(0,"another unqualified FBO draw writes the pending destination without gaining ownership");
    retired(GlDeferredCopyExit::DestinationWrite);
    consumer(56);producer(0,"a later destination overwrite must cancel the obsolete copy predecessor");
    require(!runtime.metrics().textureDeferredCopies,"actual draw capture revokes an overwritten pending destination");

    // Reproduce a different, authored gap: draw eligibility is NOT an output
    // footprint. An unrelated blended/line/masked/no-program/no-buffer draw
    // must neither gain ownership nor revoke an already submitted sibling copy.
    for(unsigned flags:{1u,0u,2u,4u,16u}){
        reset();consumer(50);producer(0,"intervening unrelated-draw control begins native");f.copy(60);
        f.resource(2,61);f.drawFlags(flags);
        f.draw(0,"ineligible unrelated draws stay native",flags?4:1);
        require(pending().queued==1&&pending().pending==1&&!runtime.metrics().textureDeferredCopies
                &&!runtime.metrics().textureProducerDraws&&!runtime.metrics().textureBindings,
                "a captured unrelated color output must preserve pending metadata WITHOUT authorizing that draw or reconciling the copy");
        consumer(60);producer(2,"exact later consumption, not the unrelated draw, must authorize this material producer");
        retired(GlDeferredCopyExit::Reconciled);
        std::printf("PASS unrelated output: flags=%u, primitive=%u, native interposed draw, exact later consumer only\n",flags,flags?4:1);
    }
    // The same exclusions are NOT permission to ignore an actual alias. MRT
    // buffer 3 must revoke too, even with a zero buffer-0 color mask/program.
    for(unsigned flags:{0u,1u,2u,4u}){
        for(bool mrt:{false,true}){
            reset();consumer(50);producer(0,"intervening alias control begins native");f.copy(60);
            f.resource(2,mrt?61:60);f.drawFlags(flags,mrt?60:0);
            f.draw(0,"an excluded draw cannot authorize itself by writing a pending destination",1);
            retired(GlDeferredCopyExit::DestinationWrite);
            consumer(60);producer(0,"a possible overwritten destination cannot inherit the old copy predecessor");
            require(!runtime.metrics().textureDeferredCopies,"all enabled raster color outputs must be included in conservative overwrite invalidation");
        }
    }

    reset();consumer(50);producer(0,"uncaptured output control begins native");f.copy(59);
    f.resource(2,61);f.drawFlags(8);f.draw(0,"unqueryable/incomplete output still revokes the namespace, including for excluded primitives",1);
    retired(GlDeferredCopyExit::UnknownWrite);
    require(pending().discardSites[static_cast<std::size_t>(GlDeferredDiscardSite::DrawUsage)]==1,"unknown output is distinguished from an exact overwritten destination; primitive eligibility is not a footprint");
    consumer(59);producer(0,"a failed complete output capture must not retain metadata or manufacture ownership");

    reset();consumer(50);producer(0,"new interval starts native");
    f.fn<void(*)(void(WINAPI*)())>("bsiFixtureOnTransfer")(resetInOriginal);f.copy(52);
    f.fn<void(*)(void(WINAPI*)())>("bsiFixtureOnTransfer")(nullptr);
    consumer(52);producer(0,"a settings change on a JOINED native worker revokes the old captured copy without a lock/deadlock");
    require(!runtime.metrics().textureDeferredCopies&&!runtime.metrics().textureTransferCalls,"stale native completion cannot queue an old-epoch edge");

    reset();consumer(50);producer(0,"copy expiry control begins native");f.copy(53);
    for(unsigned i=0;i<3;++i){f.current(dc,context);require(f.fn<BOOL(WINAPI*)(HDC)>("wglSwapBuffers")(dc)!=FALSE,"advance real hooked presentation frames");}
    retired(GlDeferredCopyExit::Expired);
    consumer(53);producer(0,"an expired unconsumed copy cannot authorize a producer after delayed consumption");
    require(!runtime.metrics().textureDeferredCopies,"expired pending metadata is never replayed");

    reset();consumer(50);producer(0,"deletion control begins native");f.copy(54);
    const unsigned deleted=54;f.proc<void(WINAPI*)(int,const unsigned*)>("glDeleteTextures")(1,&deleted);
    consumer(54);producer(0,"deleting an unconsumed destination before numeric-name reuse revokes pending metadata");
    require(!runtime.metrics().textureDeferredCopies,"pending output deletion is covered by actual resource detours");
    retired(GlDeferredCopyExit::ResourceRetired);

    reset();consumer(50);producer(0,"native generation control begins native");f.copy(55);
    f.current(dc,context);require(f.proc<BOOL(WINAPI*)(void*)>("wglDestroyPbufferARB")(pbuffer)!=FALSE,"retire pending native source BEFORE consumption");
    pbuffer=f.fn<void*(*)(HWND)>("bsiFixturePbuffer")(nullptr);require(pbuffer!=nullptr,"recreate native fixture handle with a new lifetime");
    pbDc=f.proc<HDC(WINAPI*)(void*)>("wglGetPbufferDCARB")(pbuffer);require(pbDc!=nullptr,"observe replacement native pbuffer DC");
    consumer(55);producer(0,"a reused native pbuffer handle/DC cannot inherit the retired pending source");
    require(!runtime.metrics().textureDeferredCopies,"pending native-generation retirement is covered by actual lifecycle detours");
    retired(GlDeferredCopyExit::NativeSourceRetired);

    // No permanent adoption after an ordering repair.
    reset();producer(0,"Off/review/settings revision revokes the reconciled path");
    consumer(50);producer(0,"a different selected input cannot revive the old copy destination");
    runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});
    const auto queries=f.count("bsiFixtureGlQueries");f.copy(51);f.resource(4);f.draw(0,"Off preserves native samplers");
    require(f.count("bsiFixtureGlQueries")==queries&&!runtime.metrics().textureHookTransferCalls&&!runtime.metrics().textureProducerDraws
            &&!runtime.metrics().textureDeferredAudit.sequence,"Off makes no optional inspection and clears evidence, including pending audit snapshots");
    f.current(nullptr,nullptr);require(f.proc<BOOL(WINAPI*)(void*)>("wglDestroyPbufferARB")(pbuffer)!=FALSE,"retire authored pbuffer");
    require(f.fn<BOOL(WINAPI*)(HGLRC)>("wglDeleteContext")(isolated)!=FALSE,"retire independent namespace");
    require(f.fn<BOOL(WINAPI*)(HGLRC)>("wglDeleteContext")(context)!=FALSE,"retire authored context");runtime.stop();
    ReleaseDC(child,dc);DestroyWindow(child);DestroyWindow(root);
    std::puts("PASS: actual copy-before-consumer ordering, no pre-consumption adoption, exact late material binding, original entry-time TTL, in-flight overwrite revocation, native arguments/LastError/state, reset and Off");
    std::puts("LIMIT: authored native dispatch regression, not the installed game's proven resource graph or pixels");
}
