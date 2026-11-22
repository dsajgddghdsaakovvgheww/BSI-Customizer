// Authored GL/WGL state tables + actual Win32 MinHook, NOT player/driver pixels.
#include "Runtime.h"
#include "PresentationHooks.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
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
    runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});f.current(dc,context);
    const auto off=f.count("bsiFixtureGlQueries");for(unsigned api=0;api<3;++api)f.transfer(api);
    require(f.count("bsiFixtureGlQueries")==off&&!runtime.metrics().textureHookTransferCalls&&!runtime.metrics().textureTransferCalls,"Off clears evidence and performs no diagnostic GL query");
    f.current(nullptr,nullptr);f.fn<BOOL(WINAPI*)(HGLRC)>("wglDeleteContext")(context);runtime.stop();
    ReleaseDC(child,dc);ReleaseDC(foreign,other);DestroyWindow(foreign);DestroyWindow(child);DestroyWindow(root);
    std::puts("PASS: blit/copy-subimage/named-copy incoming+outgoing Win32 LastError, exact arguments/once-only originals, foreign/Off no-query controls");
}
