// Authored GL/WGL state tables + actual Win32 MinHook, NOT player/driver pixels.
#include "Runtime.h"
#include "PresentationHooks.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <thread>
using namespace BsiShaders;
namespace {
void require(bool yes,const char *message){if(!yes){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
struct Fixture {
    HMODULE module=nullptr;
    template<class T>T fn(const char *name){auto p=GetProcAddress(module,name);require(p!=nullptr,"required transfer fixture export");return reinterpret_cast<T>(reinterpret_cast<void*>(p));}
    LONG count(const char *name){return fn<LONG(*)()>(name)();}
    void current(HDC dc,HGLRC context){require(fn<BOOL(WINAPI*)(HDC,HGLRC)>("wglMakeCurrent")(dc,context)!=FALSE,"native binding succeeds");}
    void resource(unsigned mode){fn<void(*)(unsigned,unsigned)>("bsiFixtureResourceState")(mode,50);}
    void transfer(unsigned api){
        const auto invoke=fn<void(*)(unsigned,unsigned)>(api==2?"bsiFixtureImageCopy":"bsiFixtureTransfer");
        const auto before=count("bsiFixtureTransferCalls");
        SetLastError(0x424277);invoke(api==2?0:api,0);const auto outgoing=GetLastError();
        const auto incoming=fn<DWORD(*)()>("bsiFixtureTransferIncomingError")();
        require(incoming==0x424277,"transfer inspection must preserve the native original's INCOMING LastError");
        require(outgoing==0x424288,"post-original transfer observations must preserve OUTGOING LastError");
        require(count("bsiFixtureTransferCalls")==before+1&&count("bsiFixtureTransferArgumentsIntact")
            &&count("bsiFixtureBindingsRestored")&&!count("bsiFixtureInvalidGl"),"all transfer arguments/once-only calls/state survive native/diagnostic work");
    }
};
void WINAPI changeSettingsInOriginal(){
    // The native original is allowed to join another rendering/settings worker.
    // An audit must not retain renderMutex or Runtime's mutex across this call.
    std::thread worker([]{auto &r=Runtime::instance();r.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});r.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});});
    worker.join();
}
}
int main(int argc,char **argv){
    require(argc==2,"pass explicitly named authored WGL fixture");Fixture f;f.module=LoadLibraryA(argv[1]);require(f.module!=nullptr,"load only authored fixture");
    auto instance=GetModuleHandleW(nullptr);
    auto root=CreateWindowExW(0,L"STATIC",L"transfer audit owner",WS_OVERLAPPEDWINDOW,0,0,900,600,nullptr,nullptr,instance,nullptr);
    auto child=CreateWindowExW(0,L"STATIC",L"transfer audit game",WS_CHILD,0,0,640,360,root,nullptr,instance,nullptr);
    auto foreign=CreateWindowExW(0,L"STATIC",L"unrelated transfer",WS_POPUP,0,0,640,360,nullptr,nullptr,instance,nullptr);
    require(root&&child&&foreign,"owned and unrelated live HWNDs");auto dc=GetDC(child),other=GetDC(foreign);
    auto context=f.fn<HGLRC(WINAPI*)(HDC)>("wglCreateContext")(dc);require(context!=nullptr,"native fixture context");
    require(setWglProviderForTesting(f.module),"explicit WGL provider injection");
    auto &runtime=Runtime::instance();runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.start(reinterpret_cast<std::uintptr_t>(root));
    f.current(dc,context);require(f.fn<BOOL(WINAPI*)(HDC)>("wglSwapBuffers")(dc)!=FALSE,"owned presentation selects source");
    f.fn<void(*)(int)>("bsiFixtureTransferErrorProbe")(1);
    for(unsigned api=0;api<3;++api){
        f.current(dc,context);f.resource(4);f.transfer(api);
        f.current(other,context);const auto queries=f.count("bsiFixtureGlQueries");f.transfer(api);
        require(f.count("bsiFixtureGlQueries")==queries,"foreign transfer never triggers optional GL queries");
    }
    auto audit=runtime.metrics().textureTransferAudit;
    require(audit.calls==std::array<std::uint64_t,3>{{2,2,2}}&&audit.codeProviders[0]==6
            &&audit.stops[static_cast<std::size_t>(GlTransferStop::NoCandidate)]==3
            &&audit.candidates[static_cast<std::size_t>(GlCandidateStop::Drawable)]==3,
            "each transfer family and each foreign route has a bounded exact diagnostic, not qualified ownership");
    f.current(nullptr,nullptr);const auto unboundQueries=f.count("bsiFixtureGlQueries");
    for(unsigned api=0;api<3;++api)f.transfer(api);
    audit=runtime.metrics().textureTransferAudit;
    require(f.count("bsiFixtureGlQueries")==unboundQueries&&audit.candidates[static_cast<std::size_t>(GlCandidateStop::NoContext)]==3
            &&audit.calls==std::array<std::uint64_t,3>{{3,3,3}},"missing current contexts are diagnosed without GL inspection or adoption");
    auto memory=CreateCompatibleDC(nullptr);require(memory&&!WindowFromDC(memory),"real no-window resource route");
    const auto consume=[&]{
        runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});
        f.current(dc,context);f.resource(1);f.fn<void(*)(unsigned,unsigned,unsigned)>("bsiFixtureDraw")(0,0,4);
        require(runtime.metrics().textureConsumerInputs==1&&!runtime.metrics().textureBindings,"legal LINEAR presentation remains unchanged");
        f.current(memory,context);f.resource(4);f.fn<void(*)(unsigned,unsigned,unsigned)>("bsiFixtureTransferState")(2,70,50);
    };
    for(unsigned api=0;api<3;++api){
        consume();f.transfer(api);audit=runtime.metrics().textureTransferAudit;
        require(audit.calls[api]==1&&audit.stops[static_cast<std::size_t>(GlTransferStop::RecordedInputs)]==1
                &&runtime.metrics().textureTransferCalls==1&&runtime.metrics().textureTransferInputs==1
                &&!runtime.metrics().textureBindings,
                "all three APIs retain exact post-original resource metadata; diagnostic counts cannot manufacture private bindings");
    }
    consume();f.fn<void(*)(void(WINAPI*)())>("bsiFixtureOnTransfer")(changeSettingsInOriginal);f.transfer(0);
    f.fn<void(*)(void(WINAPI*)())>("bsiFixtureOnTransfer")(nullptr);
    require(runtime.metrics().textureTransferAudit.calls==decltype(audit.calls){}&&!runtime.metrics().textureTransferCalls,
            "settings changed by a joined native worker revoke both old metadata and old diagnostic epochs without deadlock");
    runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});f.current(dc,context);
    const auto off=f.count("bsiFixtureGlQueries");for(unsigned api=0;api<3;++api)f.transfer(api);
    require(f.count("bsiFixtureGlQueries")==off&&!runtime.metrics().textureHookTransferCalls&&!runtime.metrics().textureTransferCalls
            &&runtime.metrics().textureTransferAudit.calls==decltype(audit.calls){},"Off clears evidence and every ledger counter, with no diagnostic GL query");
    f.current(nullptr,nullptr);f.fn<BOOL(WINAPI*)(HGLRC)>("wglDeleteContext")(context);runtime.stop();
    DeleteDC(memory);ReleaseDC(child,dc);ReleaseDC(foreign,other);DestroyWindow(foreign);DestroyWindow(child);DestroyWindow(root);
    std::puts("PASS: blit/copy-subimage/named-copy incoming+outgoing Win32 LastError, exact arguments/once-only originals; exact foreign/no-context/descriptor/post-native routing ledger; positive typed links; joined-worker reset and Off no-query controls");
}
