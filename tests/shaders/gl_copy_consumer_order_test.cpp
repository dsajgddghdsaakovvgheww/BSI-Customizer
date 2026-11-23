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
    void draw(LONG bindings,const char *message){
        const auto calls=count("bsiFixtureDrawCalls");
        fn<void(*)(unsigned,unsigned,unsigned)>("bsiFixtureDraw")(static_cast<unsigned>(GlDrawKind::ElementsBaseVertex),0,4);
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
}
int main(int argc,char **argv){
    require(argc==2,"pass explicitly named authored WGL fixture, never PF/system DLLs");Fixture f;f.module=LoadLibraryA(argv[1]);require(f.module!=nullptr,"load authored fixture");
    const auto instance=GetModuleHandleW(nullptr);
    const auto root=CreateWindowExW(0,L"STATIC",L"copy order player",WS_OVERLAPPEDWINDOW,0,0,900,600,nullptr,nullptr,instance,nullptr);
    const auto child=CreateWindowExW(0,L"STATIC",L"copy order game",WS_CHILD,0,0,640,360,root,nullptr,instance,nullptr);
    require(root&&child,"owned native child HWND");const auto dc=GetDC(child);
    const auto context=f.fn<HGLRC(WINAPI*)(HDC)>("wglCreateContext")(dc);require(context!=nullptr,"authored native context");
    require(setWglProviderForTesting(f.module),"explicit test-only WGL provider injection");
    auto &runtime=Runtime::instance();runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.start(reinterpret_cast<std::uintptr_t>(root));
    f.current(dc,context);require(f.fn<BOOL(WINAPI*)(HDC)>("wglSwapBuffers")(dc)!=FALSE,"selected owned presentation");
    auto *pbuffer=f.fn<void*(*)(HWND)>("bsiFixturePbuffer")(nullptr);require(pbuffer!=nullptr,"create authored pbuffer");
    const auto pbDc=f.proc<HDC(WINAPI*)(void*)>("wglGetPbufferDCARB")(pbuffer);require(pbDc&&!WindowFromDC(pbDc),"observe actual native pbuffer lifetime/DC");
    f.fn<void(*)(int)>("bsiFixtureTransferErrorProbe")(1);
    const auto reset=[&]{runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});};
    const auto consumer=[&](unsigned output){f.current(dc,context);f.resource(1,output);f.draw(0,"legal LINEAR presentation consumer never receives private samplers");};
    const auto producer=[&](LONG bindings,const char *message){f.current(pbDc,context);f.resource(4);f.draw(bindings,message);};

    // Existing positive control: the selected consumer precedes the copy.
    consumer(50);producer(0,"pbuffer existence/namespace alone is not ownership");f.copy(50);
    require(runtime.metrics().textureTransferCalls==1&&runtime.metrics().textureTransferInputs==1,"existing consumed-output copy path stays qualified");
    producer(2,"existing copy AFTER consumption reaches material bindings inside the original");
    std::puts("PASS control: consumer-before-copy, exact typed native pbuffer, native LINEAR compositor, transparent copy ABI");

    // Unconsumed sibling outputs must NEVER grant upstream ownership.
    reset();consumer(50);producer(0,"fresh settings revoke the old native copy proof");f.copy(51);
    producer(0,"unconsumed sibling copy output must stay native");
    require(!runtime.metrics().textureTransferCalls&&!runtime.metrics().textureBindings,"unlinked copy metadata is not a qualified transfer or private sampler");
    std::puts("PASS negative: a submitted copy into an unconsumed sibling is not ownership");

    // NEW missing case: this very output is sampled AFTER the native copy.
    consumer(51);
    producer(2,"a LATER exact selected consumer must reconcile the earlier native copy before inspecting its pbuffer material producer");
    require(runtime.metrics().textureProducerDraws==1&&runtime.metrics().textureProducerBindings==2,"late actual consumption reaches only its resource-linked material producer");

    // No permanent adoption after an ordering repair.
    reset();producer(0,"Off/review/settings revision revokes the reconciled path");
    consumer(50);producer(0,"a different selected input cannot revive the old copy destination");
    runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});
    const auto queries=f.count("bsiFixtureGlQueries");f.copy(51);f.resource(4);f.draw(0,"Off preserves native samplers");
    require(f.count("bsiFixtureGlQueries")==queries&&!runtime.metrics().textureHookTransferCalls&&!runtime.metrics().textureProducerDraws,"Off makes no optional inspection and clears evidence");
    f.current(nullptr,nullptr);require(f.proc<BOOL(WINAPI*)(void*)>("wglDestroyPbufferARB")(pbuffer)!=FALSE,"retire authored pbuffer");
    require(f.fn<BOOL(WINAPI*)(HGLRC)>("wglDeleteContext")(context)!=FALSE,"retire authored context");runtime.stop();
    ReleaseDC(child,dc);DestroyWindow(child);DestroyWindow(root);
    std::puts("PASS: actual copy-before-consumer ordering, no pre-consumption adoption, material bindings inside original only after exact late consumption, native arguments/LastError/state, reset and Off");
    std::puts("LIMIT: authored native dispatch regression, not the installed game's proven resource graph or pixels");
}
