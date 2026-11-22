// Actual Win32/MinHook dispatch on an authored DLL, NOT a driver/game test.
// A second native lookup returns targets the presenter could not resolve.
#include "Runtime.h"
#include "PresentationHooks.h"
#include "GlDrawCalls.h"
#undef BSI_GL_DRAW_CALLS
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
using namespace BsiShaders;
namespace {
void require(bool value,const char *message){
    if(!value){std::fprintf(stderr,"FAIL: %s\n%s\n",message,Runtime::instance().metrics().textureStatus.c_str());std::exit(1);}
}
struct Fixture {
    HMODULE module=nullptr;
    template<class T>T function(const char *name){
        const auto entry=GetProcAddress(module,name);require(entry!=nullptr,"required WGL fixture export");return reinterpret_cast<T>(reinterpret_cast<void*>(entry));
    }
    LONG count(const char *name){return function<LONG(*)()>(name)();}
    PROC query(const char *name){
        SetLastError(0x2468);
        const auto result=function<PROC(WINAPI*)(LPCSTR)>("wglGetProcAddress")(name);const auto error=GetLastError();
        require(error==0x13579b,"lookup observation preserves the native Win32 LastError, not detour-installation errors");return result;
    }
    HGLRC context(HDC dc){return function<HGLRC(WINAPI*)(HDC)>("wglCreateContext")(dc);}
    void current(HDC dc,HGLRC context){require(function<BOOL(WINAPI*)(HDC,HGLRC)>("wglMakeCurrent")(dc,context)!=FALSE,"native current context/DC");}
    void resource(unsigned mode,unsigned output=50){function<void(*)(unsigned,unsigned)>("bsiFixtureResourceState")(mode,output);}
    void variant(unsigned bank){function<void(*)(unsigned)>("bsiFixtureProcedureVariant")(bank);}
    void draw(LONG bindings,bool late=false){
        const auto before=count("bsiFixtureDrawCalls"),privateBefore=count("bsiFixturePrivateDraws");
        if(late)function<void(*)(unsigned)>("bsiFixtureLateDraw")(4);
        else function<void(*)(unsigned,unsigned,unsigned)>("bsiFixtureDraw")(static_cast<unsigned>(GlDrawKind::Arrays),0,4);
        require(count("bsiFixtureDrawCalls")==before+1&&count("bsiFixtureArgumentsIntact"),"correct original draw target executes exactly once with every argument intact");
        if(count("bsiFixtureLastPrivateBindings")!=bindings)std::fprintf(stderr,"%s draw #%ld: expected %ld bindings, got %ld\n",late?"late":"early",before+1,bindings,count("bsiFixtureLastPrivateBindings"));
        require(count("bsiFixtureLastPrivateBindings")==bindings&&count("bsiFixturePrivateDraws")==privateBefore+(bindings?1:0),"expected private desktop bias+floor samplers must exist INSIDE that distinct original");
        require(count("bsiFixtureBindingsRestored")&&!count("bsiFixtureInvalidGl"),"exact source bindings/active unit restored, no illegal GL query");
    }
    void lateTransfer(bool copy){
        const auto before=count("bsiFixtureTransferCalls");function<void(*)(unsigned)>("bsiFixtureLateTransfer")(copy);
        require(count("bsiFixtureTransferCalls")==before+1&&count("bsiFixtureTransferArgumentsIntact"),"distinct transfer original executes once with exact arguments");
        require(count("bsiFixtureBindingsRestored")&&!count("bsiFixtureInvalidGl"),"lookup/transfer capture never borrows samplers or changes active unit");
    }
};
}
int main(int argc,char **argv){
    require(argc==2,"pass the explicitly named WGL state-double DLL");Fixture f;f.module=LoadLibraryA(argv[1]);require(f.module!=nullptr,"load authored WGL fixture, not opengl32");
    const auto instance=GetModuleHandleW(nullptr);
    const auto root=CreateWindowExW(0,L"STATIC",L"WGL lookup owner",WS_OVERLAPPEDWINDOW,0,0,900,600,nullptr,nullptr,instance,nullptr);
    const auto game=CreateWindowExW(0,L"STATIC",L"WGL lookup game",WS_CHILD,0,0,640,360,root,nullptr,instance,nullptr);
    const auto foreign=CreateWindowExW(0,L"STATIC",L"unowned WGL lookup",WS_POPUP,0,0,640,360,nullptr,nullptr,instance,nullptr);
    require(root&&game&&foreign,"actual Win32 owned and foreign windows");
    const auto dc=GetDC(game),foreignDc=GetDC(foreign);require(dc&&foreignDc,"native DCs");
    const auto presenter=f.context(dc),scene=f.context(dc);require(presenter&&scene&&presenter!=scene,"separate NONSHARING authored native contexts");
    require(setWglProviderForTesting(f.module),"explicit test provider injection only");
    auto &runtime=Runtime::instance();runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.start(reinterpret_cast<std::uintptr_t>(root));
    f.current(dc,presenter);
    require(f.function<BOOL(WINAPI*)(HDC)>("wglSwapBuffers")(dc)!=FALSE,"select presentation through actual native detour");
    const auto early=f.query("glDrawElements");require(early&&originalPresentationFunction(reinterpret_cast<void*>(early)),"presenter-resolved target is hooked");
    const auto earlyBlit=f.query("glBlitFramebuffer"),earlyCopy=f.query("glCopyTexSubImage2D");
    auto *pb=f.function<void*(*)(HWND)>("bsiFixturePbuffer")(nullptr);require(pb!=nullptr,"authored native pbuffer");
    const auto get=reinterpret_cast<HDC(WINAPI*)(void*)>(f.query("wglGetPbufferDCARB"));require(get!=nullptr,"native pbuffer acquisition entry");
    const auto pbDc=get(pb);require(pbDc&&WindowFromDC(pbDc)==nullptr,"observed exact no-HWND pbuffer DC");
    f.current(pbDc,scene);f.variant(1);f.resource(4);
    const auto late=f.query("glDrawElements");require(late&&late!=early,"later context returns a distinct native code target");
    require(originalPresentationFunction(reinterpret_cast<void*>(late))!=nullptr,"a returned draw procedure must be intercepted BEFORE its first original; presenter-only resolution misses this target");
    require(f.query("glDrawElements")==late,"repeated lookup preserves the exact native address, not another target's wrapper");
    f.draw(0,true);
    require(!runtime.metrics().textureInspected,"a lookup is code coverage, NEVER context/resource ownership or GL inspection permission");
    f.current(dc,presenter);f.resource(1);f.draw(0);
    require(runtime.metrics().textureConsumerInputs==1&&!runtime.metrics().textureDraws,"legal LINEAR compositor stays native and seeds only actual consumption");
    f.function<void(*)(unsigned)>("bsiFixtureNativeTextureTarget")(50);
    const auto bind=reinterpret_cast<BOOL(WINAPI*)(void*,int)>(f.query("wglBindTexImageARB"));require(bind&&bind(pb,0x2083),"successful exact native image link, NOT inferred GL sharing");
    f.current(pbDc,scene);f.resource(4);f.draw(2,true);
    require(runtime.metrics().textureProducerDraws==1&&runtime.metrics().textureDraws==1,"only the proven material producer changes; no compositor replacement");

    f.variant(1|2);
    const auto lateBlit=f.query("glBlitFramebuffer");require(lateBlit&&lateBlit!=earlyBlit&&originalPresentationFunction(reinterpret_cast<void*>(lateBlit)),"late color-transfer target is also intercepted without presenter reinitialization");
    f.function<void(*)(unsigned,unsigned,unsigned)>("bsiFixtureTransferState")(2,70,50);
    f.function<void(*)()>("bsiFixtureTransferToDefault")();const auto transfers=runtime.metrics().textureTransferCalls;f.lateTransfer(false);
    require(runtime.metrics().textureTransferCalls==transfers+1,"late blit follows the exact named RBO -> linked native default output");
    f.resource(3,70);f.draw(2,true);
    require(runtime.metrics().textureProducerDraws==2,"newly observed transfer reaches its typed upstream material draw");
    f.current(foreignDc,scene);f.resource(4);f.draw(0,true);
    require(runtime.metrics().textureProducerDraws==2,"the same patched function cannot authorize a foreign window");

    f.current(pbDc,scene);f.variant(1|2|16);
    const auto deleteRbo=reinterpret_cast<void(WINAPI*)(int,const unsigned*)>(f.query("glDeleteRenderbuffers"));
    require(deleteRbo&&originalPresentationFunction(reinterpret_cast<void*>(deleteRbo)),"a late deletion target must not leave reusable names trusted");
    const unsigned rbo=70;const auto deletedRbos=f.count("bsiFixtureLateRenderbufferDeletes");deleteRbo(1,&rbo);
    require(f.count("bsiFixtureLateRenderbufferDeletes")==deletedRbos+1,"correct distinct renderbuffer-deletion original runs once");
    f.resource(3,70);f.draw(0,true);
    require(runtime.metrics().textureProducerDraws==2,"late RBO deletion revokes inherited producer proof before numeric-name reuse");
    f.current(dc,presenter);f.resource(1);f.draw(0);f.function<void(*)(unsigned)>("bsiFixtureNativeTextureTarget")(50);require(bind(pb,0x2083),"fresh native image link after retirement");
    f.current(pbDc,scene);f.resource(4);f.draw(2,true);f.variant(1|2|8|16);
    const auto deleteTexture=reinterpret_cast<void(WINAPI*)(int,const unsigned*)>(f.query("glDeleteTextures"));
    require(deleteTexture&&originalPresentationFunction(reinterpret_cast<void*>(deleteTexture)),"late texture-deletion target also gets its own original");
    const unsigned texture=50;const auto deletedTextures=f.count("bsiFixtureLateTextureDeletes");deleteTexture(1,&texture);
    require(f.count("bsiFixtureLateTextureDeletes")==deletedTextures+1,"correct distinct texture-deletion original runs once");
    f.draw(0,true);require(runtime.metrics().textureProducerDraws==3,"late consumer deletion revokes its native-image ancestors too");

    runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});f.variant(1|2|4|8|16);
    const auto lateCopy=f.query("glCopyTexSubImage2D");require(lateCopy&&lateCopy!=earlyCopy&&originalPresentationFunction(reinterpret_cast<void*>(lateCopy)),"late copy target is captured even when effects are Off");
    f.function<void(*)(unsigned,unsigned,unsigned)>("bsiFixtureTransferState")(2,70,50);f.lateTransfer(true);f.resource(4);f.draw(0,true);
    require(!runtime.metrics().textureInspected&&!runtime.metrics().textureTransferCalls,"Off lookup/draw/copy retain exact native behavior and cannot retain evidence");
    require(!f.query("bsiUnsupportedProcedure")&&!f.query(nullptr),"unsupported and null lookup results stay native");
    for(const auto &entry:{std::pair<const char*,std::intptr_t>{"bsiInvalidOne",1},{"bsiInvalidTwo",2},{"bsiInvalidThree",3},{"bsiInvalidMinusOne",-1}}){
        const auto value=f.query(entry.first);require(reinterpret_cast<std::intptr_t>(value)==entry.second&&!originalPresentationFunction(reinterpret_cast<void*>(value)),"WGL failure sentinels are returned unchanged and never detoured");
    }
    unsigned failure=0;for(const std::intptr_t sentinel:{1,2,3,-1}){
        f.variant(31|(++failure<<8));const auto value=f.query("glDrawElements");
        require(reinterpret_cast<std::intptr_t>(value)==sentinel&&!originalPresentationFunction(reinterpret_cast<void*>(value)),"recognized GL names still return native WGL failure sentinels unchanged");
    }
    f.current(nullptr,nullptr);f.function<BOOL(WINAPI*)(HGLRC)>("wglDeleteContext")(scene);f.function<BOOL(WINAPI*)(HGLRC)>("wglDeleteContext")(presenter);
    const auto destroy=reinterpret_cast<BOOL(WINAPI*)(void*)>(f.query("wglDestroyPbufferARB"));require(destroy&&destroy(pb),"native pbuffer teardown");runtime.stop();
    require(!f.count("bsiFixtureInvalidGl"),"all lookup, source, lifetime and error contracts stayed legal");
    ReleaseDC(game,dc);ReleaseDC(foreign,foreignDc);DestroyWindow(foreign);DestroyWindow(game);DestroyWindow(root);
    std::puts("PASS: late WGL-resolved draw/blit/copy targets, exact native address/LastError/arguments/once-only originals, unowned/foreign/Off exclusion, exact native-image-linked material samplers inside distinct originals");
    std::puts("LIMIT: authored DLL + real Windows MinHook; not BlueStacks, actual driver/game pixels or a claim about pre-bootstrap cached procedures");
}
