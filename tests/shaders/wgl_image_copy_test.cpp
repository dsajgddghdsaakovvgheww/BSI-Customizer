// Authored GL/WGL tables + actual Win32 MinHook, NOT a driver or game test.
// Real software-driver image-copy/material pixels execute in the pixel matrix.
#include "Runtime.h"
#include "PresentationHooks.h"
#include "GlDrawCalls.h"
#undef BSI_GL_DRAW_CALLS
#include <windows.h>
#include <cstdio>
#include <cstdlib>
using namespace BsiShaders;
namespace {
void require(bool value,const char *message){if(!value){std::fprintf(stderr,"FAIL: %s\n%s\n",message,Runtime::instance().metrics().textureStatus.c_str());std::exit(1);}}
struct Fixture {
    HMODULE module=nullptr;
    template<class T>T fn(const char *name){const auto p=GetProcAddress(module,name);require(p!=nullptr,"required named-copy fixture export");return reinterpret_cast<T>(reinterpret_cast<void*>(p));}
    LONG count(const char *name){return fn<LONG(*)()>(name)();}
    void current(HDC dc,HGLRC c){require(fn<BOOL(WINAPI*)(HDC,HGLRC)>("wglMakeCurrent")(dc,c)!=FALSE,"native current binding");}
    HGLRC context(HDC dc){return fn<HGLRC(WINAPI*)(HDC)>("wglCreateContext")(dc);}
    void resource(unsigned mode,unsigned output=50){fn<void(*)(unsigned,unsigned)>("bsiFixtureResourceState")(mode,output);}
    void draw(LONG bindings){
        const auto before=count("bsiFixtureDrawCalls");
        fn<void(*)(unsigned,unsigned,unsigned)>("bsiFixtureDraw")(static_cast<unsigned>(GlDrawKind::ElementsBaseVertex),0,4);
        require(count("bsiFixtureDrawCalls")==before+1&&count("bsiFixtureArgumentsIntact")&&count("bsiFixtureLastPrivateBindings")==bindings
                &&count("bsiFixtureBindingsRestored")&&!count("bsiFixtureInvalidGl"),"only proven material originals borrow samplers, with exact arguments/once-only forwarding and full source restoration");
    }
    void copy(unsigned alias=0,unsigned invalid=0){
        const auto before=count("bsiFixtureTransferCalls");fn<void(*)(unsigned,unsigned)>("bsiFixtureImageCopy")(alias,invalid);
        require(count("bsiFixtureTransferCalls")==before+1&&count("bsiFixtureTransferArgumentsIntact")&&count("bsiFixtureBindingsRestored")&&!count("bsiFixtureInvalidGl"),
                "every named-copy original executes once with all 15 arguments unchanged, no bound images/FBOs/sampler edits or illegal queries");
    }
    void callback(void(WINAPI *p)()){fn<void(*)(void(WINAPI*)())>("bsiFixtureOnTransfer")(p);}
    void erase(bool rbo,unsigned name){
        const auto p=fn<PROC(WINAPI*)(LPCSTR)>("wglGetProcAddress")(rbo?"glDeleteRenderbuffers":"glDeleteTextures");require(p!=nullptr,"observed exact native retirement procedure");
        reinterpret_cast<void(WINAPI*)(int,const unsigned*)>(p)(1,&name);
    }
} *active=nullptr;
std::uint64_t unpublished=0;
unsigned callbackMode=0;
void WINAPI insideCopy(){
    auto &r=Runtime::instance();require(r.metrics().textureTransferCalls==unpublished,"a named-copy dependency must not publish BEFORE its native original returns");
    if(callbackMode==1){r.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});r.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});}
    if(callbackMode==2)active->erase(false,50); // re-enters without a held render mutex
}
}
int main(int argc,char **argv){
    require(argc==2,"pass the explicitly named authored WGL provider");Fixture f;active=&f;f.module=LoadLibraryA(argv[1]);require(f.module!=nullptr,"never load a system proxy");
    const auto instance=GetModuleHandleW(nullptr);
    const auto root=CreateWindowExW(0,L"STATIC",L"image-copy owner",WS_OVERLAPPEDWINDOW,0,0,900,600,nullptr,nullptr,instance,nullptr);
    const auto game=CreateWindowExW(0,L"STATIC",L"image-copy game",WS_CHILD,0,0,640,360,root,nullptr,instance,nullptr);
    const auto foreign=CreateWindowExW(0,L"STATIC",L"foreign image-copy",WS_POPUP,0,0,640,360,nullptr,nullptr,instance,nullptr);
    require(root&&game&&foreign,"real owned and foreign Win32 windows");const auto dc=GetDC(game),foreignDc=GetDC(foreign),memoryDc=CreateCompatibleDC(nullptr);
    require(dc&&foreignDc&&memoryDc&&!WindowFromDC(memoryDc),"native selected/foreign/no-HWND DCs; no invented default pbuffer image");
    const auto presenter=f.context(dc),isolated=f.context(dc);require(presenter&&isolated,"independent pre-existing contexts");
    require(setWglProviderForTesting(f.module),"explicit testing-only WGL provider injection");
    auto &r=Runtime::instance();r.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});r.start(reinterpret_cast<std::uintptr_t>(root));
    f.current(dc,presenter);require(f.fn<BOOL(WINAPI*)(HDC)>("wglSwapBuffers")(dc)!=FALSE,"select owned presenter");
    const auto consume=[&]{
        r.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});r.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});
        f.current(dc,presenter);f.resource(1);f.draw(0);
        require(r.metrics().textureConsumerInputs==1&&!r.metrics().textureDraws,"legal LINEAR presentation input remains native, not a fabricated reducible material");
    };
    for(unsigned alias=0;alias<2;++alias){
        consume();f.current(memoryDc,presenter);f.resource(3,70);f.draw(0);
        f.current(dc,presenter);f.resource(4);unpublished=r.metrics().textureTransferCalls;f.callback(insideCopy);f.copy(alias);f.callback(nullptr);
        require(r.metrics().textureTransferCalls==1&&r.metrics().textureTransferInputs==1&&r.metrics().textureHookImageCopyCalls==1,
                "exact core/ARB or NV copy entry publishes its typed RBO->consumed-texture dependency AFTER original completion");
        f.current(memoryDc,presenter);f.resource(3,70);f.draw(2);
        require(r.metrics().textureProducerDraws==1,"named-copy link, NOT a current FBO relationship, reaches its material RBO producer");
        f.erase(true,70);f.draw(0);require(r.metrics().textureProducerDraws==1,"observed RBO retirement revokes its producer proof before numeric-name reuse");
    }
    for(unsigned invalid=1;invalid<=4;++invalid){
        consume();f.resource(4);f.copy(0,invalid);f.current(memoryDc,presenter);f.resource(3,70);f.draw(0);
        require(!r.metrics().textureTransferCalls&&!r.metrics().textureProducerDraws&&r.metrics().textureHookImageCopyCalls==1,
                "unsupported target/nonzero mip/out-of-bounds/zero name still forwards natively; raw hook coverage is NOT qualified copy evidence");
    }
    consume();f.current(foreignDc,presenter);const auto gl=f.count("bsiFixtureGlQueries");f.copy();
    require(f.count("bsiFixtureGlQueries")==gl&&!r.metrics().textureTransferCalls&&r.metrics().textureHookImageCopyCalls==1,
            "a foreign-window named copy is counted as raw coverage but rejected BEFORE any optional GL query");
    f.current(memoryDc,isolated);const auto independent=f.count("bsiFixtureGlQueries");f.copy();f.resource(3,70);f.draw(0);
    require(f.count("bsiFixtureGlQueries")==independent&&!r.metrics().textureProducerDraws,"equal numeric GL names in an isolated context cannot borrow ownership or trigger state queries");
    for(callbackMode=1;callbackMode<=2;++callbackMode){
        consume();f.resource(4);unpublished=r.metrics().textureTransferCalls;f.callback(insideCopy);f.copy();f.callback(nullptr);
        f.current(memoryDc,presenter);f.resource(3,70);f.draw(0);
        require(!r.metrics().textureTransferCalls&&!r.metrics().textureProducerDraws,"settings revision/deletion INSIDE a named-copy original rejects stale capture without locking out native workers");
    }
    callbackMode=0;consume();f.resource(4);f.copy();f.erase(false,50);f.current(memoryDc,presenter);f.resource(3,70);f.draw(0);
    require(r.metrics().textureProducerDraws==0,"consumer texture retirement also revokes named-copy producer evidence");
    r.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});const auto off=f.count("bsiFixtureGlQueries");f.copy(1);f.draw(0);
    require(f.count("bsiFixtureGlQueries")==off&&!r.metrics().textureHookDrawCalls&&!r.metrics().textureHookTransferCalls&&!r.metrics().textureTransferCalls,
            "Off makes no optional GL query and clears BOTH qualified evidence and active raw counters");
    f.current(nullptr,nullptr);f.fn<BOOL(WINAPI*)(HGLRC)>("wglDeleteContext")(isolated);f.fn<BOOL(WINAPI*)(HGLRC)>("wglDeleteContext")(presenter);r.stop();
    DeleteDC(memoryDc);ReleaseDC(game,dc);ReleaseDC(foreign,foreignDc);DestroyWindow(foreign);DestroyWindow(game);DestroyWindow(root);
    std::puts("PASS: actual Windows named-copy MinHook originals/15 arguments/once-only forwarding; typed RBO lineage after native completion; bound-image independence; invalid/foreign/unshared/retirement/reentrant-revision/Off guards and separate raw coverage");
    std::puts("LIMIT: authored dispatch tables, NOT driver image-copy legality, installed game textures, hardware or FPS evidence");
}
