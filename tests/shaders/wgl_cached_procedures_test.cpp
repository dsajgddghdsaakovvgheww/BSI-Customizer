// Authored Win32/WGL state double + actual MinHook, NOT a driver/game test.
// All application GL procedure lookups happen BEFORE Runtime::start().
#include "Runtime.h"
#include "PresentationHooks.h"
#include "GlDrawCalls.h"
#undef BSI_GL_DRAW_CALLS
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace BsiShaders;
namespace {
void require(bool value,const char *message){
    if(!value){std::fprintf(stderr,"FAIL: %s\n%s\n",message,Runtime::instance().metrics().textureStatus.c_str());std::exit(1);}
}
using Make=BOOL(WINAPI*)(HDC,HGLRC);
using MakeSeparate=BOOL(WINAPI*)(HDC,HDC,HGLRC);
struct Cached {PROC draw=nullptr,blit=nullptr,copy=nullptr,textures=nullptr,renderbuffers=nullptr;unsigned bank=0;};
struct Fixture {
    HMODULE module=nullptr;
    template<class T>T function(const char *name){
        const auto entry=GetProcAddress(module,name);require(entry!=nullptr,"required authored WGL fixture export");return reinterpret_cast<T>(reinterpret_cast<void*>(entry));
    }
    LONG count(const char *name){return function<LONG(*)()>(name)();}
    PROC query(const char *name){return function<PROC(WINAPI*)(LPCSTR)>("wglGetProcAddress")(name);}
    HGLRC context(HDC dc){return function<HGLRC(WINAPI*)(HDC)>("wglCreateContext")(dc);}
    void failures(unsigned flags){function<void(*)(unsigned)>("bsiFixtureFailures")(flags);}
    void variant(unsigned flags){function<void(*)(unsigned)>("bsiFixtureProcedureVariant")(flags);}
    void resource(unsigned mode,unsigned output=50){function<void(*)(unsigned,unsigned)>("bsiFixtureResourceState")(mode,output);}
    void current(HDC dc,HGLRC context,bool success=true){
        const auto make=function<Make>("wglMakeCurrent");const auto before=count("bsiFixtureMakeCalls");SetLastError(0x1357);
        const auto result=make(dc,context);const auto error=GetLastError();
        require((result!=FALSE)==success&&error==(success?0x442201u:0x4422f0u),"core make-current preserves exact native return and LastError");
        require(count("bsiFixtureMakeCalls")==before+1&&function<LONG(*)(HDC,HDC,HGLRC)>("bsiFixtureMakeArguments")(dc,dc,context),"core original executes once with unchanged DC/context arguments");
    }
    void separate(PROC entry,HDC draw,HDC read,HGLRC context,bool ext,bool success=true){
        const auto before=count("bsiFixtureMakeContextCalls");SetLastError(0x2468);
        const auto result=reinterpret_cast<MakeSeparate>(entry)(draw,read,context);const auto error=GetLastError();
        require((result!=FALSE)==success&&error==(success?(ext?0x442203u:0x442202u):0x4422f1u),"ARB/EXT make-current preserve their own native return and LastError");
        require(count("bsiFixtureMakeContextCalls")==before+1&&function<LONG(*)(HDC,HDC,HGLRC)>("bsiFixtureMakeArguments")(draw,read,context),"separate READ/DRAW/context reach the correct original once");
    }
    Cached cache(unsigned bank){
        variant(31|(bank<<16));Cached c;c.bank=bank;c.draw=query("glDrawElements");c.blit=query("glBlitFramebuffer");c.copy=query("glCopyTexSubImage2D");
        c.textures=query("glDeleteTextures");c.renderbuffers=query("glDeleteRenderbuffers");
        require(c.draw&&c.blit&&c.copy&&c.textures&&c.renderbuffers,"cache native draw/transfer/deletion addresses before bootstrap");return c;
    }
    void draw(const Cached &c,LONG bindings){
        const auto before=count("bsiFixtureDrawCalls"),privateBefore=count("bsiFixturePrivateDraws");
        function<void(*)(PROC,unsigned,unsigned)>("bsiFixtureCachedDraw")(c.draw,c.bank,4);
        require(count("bsiFixtureDrawCalls")==before+1&&count("bsiFixtureArgumentsIntact"),"cached draw original executes once with exact indices/type/mode/count");
        require(count("bsiFixtureLastPrivateBindings")==bindings&&count("bsiFixturePrivateDraws")==privateBefore+(bindings?1:0),"private bias+floor bindings exist only inside the proven material original");
        require(count("bsiFixtureBindingsRestored")&&!count("bsiFixtureInvalidGl"),"source samplers/active unit restored and no illegal GL calls");
    }
};
}
int main(int argc,char **argv){
    require(argc==2,"pass the explicitly named WGL state-double DLL");Fixture f;f.module=LoadLibraryA(argv[1]);require(f.module!=nullptr,"load authored fixture, never opengl32");
    const auto instance=GetModuleHandleW(nullptr);
    const auto root=CreateWindowExW(0,L"STATIC",L"cached WGL owner",WS_OVERLAPPEDWINDOW,0,0,900,600,nullptr,nullptr,instance,nullptr);
    const auto game=CreateWindowExW(0,L"STATIC",L"cached WGL game",WS_CHILD,0,0,640,360,root,nullptr,instance,nullptr);
    const auto foreign=CreateWindowExW(0,L"STATIC",L"unowned cached WGL",WS_POPUP,0,0,640,360,nullptr,nullptr,instance,nullptr);
    require(root&&game&&foreign,"real Win32 owned and foreign windows");const auto dc=GetDC(game),foreignDc=GetDC(foreign);require(dc&&foreignDc,"native DCs");
    const auto presenter=f.context(dc);std::array<HGLRC,4> scenes{};std::array<Cached,4> cached{};
    for(unsigned i=0;i<scenes.size();++i){scenes[i]=f.context(dc);require(scenes[i]&&scenes[i]!=presenter,"pre-existing nonsharing scene context");f.current(dc,scenes[i]);cached[i]=f.cache(i);}
    for(unsigned i=1;i<cached.size();++i)require(cached[i].draw!=cached[i-1].draw,"distinct native code targets per authored context table");
    const auto arb=f.query("wglMakeContextCurrentARB"),ext=f.query("wglMakeContextCurrentEXT");require(arb&&ext&&arb!=ext,"cache both distinct separate-current ABI aliases before bootstrap");
    const auto get=reinterpret_cast<HDC(WINAPI*)(void*)>(f.query("wglGetPbufferDCARB"));
    const auto bind=reinterpret_cast<BOOL(WINAPI*)(void*,int)>(f.query("wglBindTexImageARB"));
    const auto destroy=reinterpret_cast<BOOL(WINAPI*)(void*)>(f.query("wglDestroyPbufferARB"));require(get&&bind&&destroy,"cached native-image lifecycle entries");
    f.current(nullptr,nullptr);require(setWglProviderForTesting(f.module),"explicit test provider injection only");
    auto &runtime=Runtime::instance();runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.start(reinterpret_cast<std::uintptr_t>(root));

    // There are NO further application wglGetProcAddress calls. Code discovery
    // must happen on a successful native binding, before its first cached draw.
    f.current(dc,scenes[0]);
    require(originalPresentationFunction(reinterpret_cast<void*>(cached[0].draw))!=nullptr,"a pre-bootstrap cached draw must be intercepted AFTER make-current and BEFORE its first original, without another application lookup");
    for(const auto entry:{cached[0].blit,cached[0].copy,cached[0].textures,cached[0].renderbuffers,arb,ext})
        require(originalPresentationFunction(reinterpret_cast<void*>(entry))!=nullptr,"current-table discovery covers cached transfers, deletions and both separate-current aliases too");
    f.resource(4);f.draw(cached[0],0);require(!runtime.metrics().textureInspected,"discovering a current dispatch table grants NO presentation/resource ownership or GL inspection");
    const auto discovered=f.count("bsiFixtureProcedureQueries");f.current(dc,scenes[0]);
    require(f.count("bsiFixtureProcedureQueries")==discovered,"repeated known native binding does not rescan every procedure on every frame");

    f.current(dc,presenter);require(f.function<BOOL(WINAPI*)(HDC)>("wglSwapBuffers")(dc)!=FALSE,"select actual owned presentation");
    f.resource(1);f.function<void(*)(unsigned,unsigned,unsigned)>("bsiFixtureDraw")(static_cast<unsigned>(GlDrawKind::Arrays),0,4);
    require(runtime.metrics().textureConsumerInputs==1&&!runtime.metrics().textureDraws,"legal LINEAR consumer remains native and seeds only actual consumption");
    auto *pb=f.function<void*(*)(HWND)>("bsiFixturePbuffer")(nullptr);require(pb!=nullptr,"authored native pbuffer");
    const auto pbDc=get(pb);require(pbDc&&WindowFromDC(pbDc)==nullptr,"cached acquisition still observes exact no-HWND pbuffer lifetime");
    f.function<void(*)(unsigned)>("bsiFixtureNativeTextureTarget")(50);require(bind(pb,0x2083),"successful exact native image link, NOT guessed context sharing");
    f.current(pbDc,scenes[0]);f.resource(4);f.draw(cached[0],2);
    f.separate(arb,pbDc,dc,scenes[1],false);
    require(originalPresentationFunction(reinterpret_cast<void*>(cached[1].draw))!=nullptr,"cached ARB make-current discovers its distinct current-context draw target before return");
    f.resource(4);f.draw(cached[1],2);
    f.separate(ext,pbDc,dc,scenes[2],true);
    require(originalPresentationFunction(reinterpret_cast<void*>(cached[2].draw))!=nullptr,"cached EXT make-current discovers its own distinct draw target before return");
    f.resource(4);f.draw(cached[2],2);
    require(runtime.metrics().textureProducerDraws==3&&runtime.metrics().textureDraws==3,"all three cached dispatch routes reach only proven material producers");

    auto queries=f.count("bsiFixtureProcedureQueries");f.failures(1024);f.current(foreignDc,scenes[3],false);
    require(f.count("bsiFixtureProcedureQueries")==queries&&!originalPresentationFunction(reinterpret_cast<void*>(cached[3].draw)),"failed core binding cannot inspect a stale current context or discover the requested one");
    f.failures(2048);f.separate(arb,foreignDc,dc,scenes[3],false,false);f.separate(ext,foreignDc,dc,scenes[3],true,false);
    require(f.count("bsiFixtureProcedureQueries")==queries&&!originalPresentationFunction(reinterpret_cast<void*>(cached[3].draw)),"failed ARB/EXT bindings keep native behavior and perform no discovery");
    f.failures(0);f.separate(ext,nullptr,nullptr,nullptr,true);
    require(f.count("bsiFixtureProcedureQueries")==queries,"successful null unbind does not resolve GL procedures without a context");
    f.current(foreignDc,scenes[3]);require(originalPresentationFunction(reinterpret_cast<void*>(cached[3].draw))!=nullptr,"foreign current context grants code coverage, never ownership");
    f.resource(4);auto inspected=runtime.metrics().textureInspected;f.draw(cached[3],0);
    require(runtime.metrics().textureInspected==inspected&&runtime.metrics().textureProducerDraws==3,"foreign-window draws cannot borrow the selected route through a patched cached target");

    // Cached deletion must retire the exact link, not leave reused names live.
    f.current(dc,presenter);const unsigned texture=50;const auto deleted=f.count("bsiFixtureLateTextureDeletes");
    reinterpret_cast<void(WINAPI*)(int,const unsigned*)>(cached[0].textures)(1,&texture);
    require(f.count("bsiFixtureLateTextureDeletes")==deleted+1,"cached deletion forwards its own native original exactly once");
    f.current(pbDc,scenes[0]);f.resource(4);f.draw(cached[0],0);
    require(runtime.metrics().textureProducerDraws==3,"cached consumer deletion revokes material proof before numeric-name reuse");
    runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});
    f.current(pbDc,scenes[1]);f.resource(4);f.draw(cached[1],0);
    require(!runtime.metrics().textureInspected&&!runtime.metrics().textureDraws,"Off retains native source state and no capture evidence");

    // Discovery caches are code-only and bounded. A retired/reused HGLRC is a
    // new table, even when its numeric handle and DC equal an old observation.
    f.current(nullptr,nullptr);const auto remove=f.function<BOOL(WINAPI*)(HGLRC)>("wglDeleteContext");require(remove(scenes[3]),"delete pre-existing scene context");
    const auto reused=f.context(foreignDc);require(reused==scenes[3],"authored allocator deliberately reuses the numeric native handle");
    queries=f.count("bsiFixtureProcedureQueries");f.current(foreignDc,reused);
    require(f.count("bsiFixtureProcedureQueries")>queries,"retired/reused context handle must be rediscovered, not skipped by a stale code cache");
    queries=f.count("bsiFixtureProcedureQueries");f.current(foreignDc,reused);
    require(f.count("bsiFixtureProcedureQueries")==queries,"fresh repeated binding is cached again even with effects Off");
    f.current(nullptr,nullptr);queries=f.count("bsiFixtureProcedureQueries");f.current(nullptr,nullptr);
    require(f.count("bsiFixtureProcedureQueries")==queries,"core null unbind never runs a procedure scan");
    for(const auto context:scenes)require(remove(context),"native context teardown");require(remove(presenter)&&destroy(pb),"native presenter/pbuffer teardown");runtime.stop();
    require(!f.count("bsiFixtureInvalidGl"),"no illegal GL inspection on failed, null, unowned or foreign route");
    ReleaseDC(game,dc);ReleaseDC(foreign,foreignDc);DestroyWindow(foreign);DestroyWindow(game);DestroyWindow(root);
    std::puts("PASS: pre-bootstrap cached WGL draw/transfer/deletion coverage on core/ARB/EXT current binding; native results/errors/arguments/once-only originals; bounded refresh cache/reuse; proven producers only; LINEAR/foreign/Off unchanged");
    std::puts("LIMIT: authored DLL + actual Windows MinHook, not installed-game capture or driver pixels; a pre-existing context which never rebinds or queries after bootstrap remains outside this discovery path");
}
