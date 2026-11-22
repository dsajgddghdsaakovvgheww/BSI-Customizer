// Actual Win32 MinHook on authored EGL DLLs, NOT a GL driver/game test.
// Address discovery is code coverage, never permission to query GL state.
#include "Runtime.h"
#include "PresentationHooks.h"
#include <windows.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
using namespace BsiShaders;
namespace {
void require(bool b,const char *m){if(!b){std::fprintf(stderr,"FAIL: %s\n%s\n",m,Runtime::instance().metrics().textureStatus.c_str());std::exit(1);}}
struct Fixture {
    HMODULE module=nullptr;void *display=nullptr;
    template<class T>T fn(const char *name){auto p=GetProcAddress(module,name);require(p!=nullptr,"authored EGL export");return reinterpret_cast<T>(reinterpret_cast<void*>(p));}
    LONG count(const char *name){return fn<LONG(*)()>(name)();}
    void open(const char *path){module=LoadLibraryA(path);require(module!=nullptr,"explicit EGL fixture name");display=fn<void*(*)()>("bsiFixtureDisplay")();}
    void ready(){
        for(unsigned i=0;i<100&&!originalPresentationFunction(reinterpret_cast<void*>(GetProcAddress(module,"eglSwapBuffers")));++i)std::this_thread::sleep_for(std::chrono::milliseconds(50));
        require(originalPresentationFunction(reinterpret_cast<void*>(GetProcAddress(module,"eglSwapBuffers")))!=nullptr,"complete provider attached");
    }
    void *context(bool secret=false){const int a[]{0x32c0,secret,0x3038};return fn<void*(WINAPI*)(void*,void*,void*,const int*)>("eglCreateContext")(display,nullptr,nullptr,a);}
    void *surface(HWND w){const int a[]{0x3038};return fn<void*(WINAPI*)(void*,void*,void*,const int*)>("eglCreateWindowSurface")(display,nullptr,w,a);}
    void current(void *draw,void *context,bool success=true,void *read=nullptr){
        auto make=fn<unsigned(WINAPI*)(void*,void*,void*,void*)>("eglMakeCurrent");const auto before=count("bsiFixtureMakeCalls");SetLastError(0x77);
        const auto r=make(display,draw,read?read:draw,context);const auto error=GetLastError();
        require((r!=0)==success&&error==(success?0x442201u:0x4422f0u),"native EGL bind result and Win32 LastError unchanged");
        require(count("bsiFixtureMakeCalls")==before+1&&fn<LONG(*)(void*,void*,void*,void*)>("bsiFixtureMakeArguments")(display,draw,read?read:draw,context),"EGL original runs once with exact display/DRAW/READ/context");
    }
    void variant(unsigned v){fn<void(*)(unsigned)>("bsiFixtureProcedureVariant")(v);}
    void *query(const char *name){SetLastError(0x55);auto p=fn<void*(WINAPI*)(const char*)>("eglGetProcAddress")(name);require(GetLastError()==0x13579b,"EGL lookup retains native Win32 LastError");return p;}
    void resource(unsigned mode){fn<void(*)(unsigned,unsigned)>("bsiFixtureResourceState")(mode,50);}
    void draw(void *p,unsigned variant,LONG bindings){
        const auto calls=count("bsiFixtureDrawCalls"),priv=count("bsiFixturePrivateDraws");fn<void(*)(void*,unsigned)>("bsiFixtureCachedDraw")(p,variant);
        require(count("bsiFixtureDrawCalls")==calls+1&&count("bsiFixtureArgumentsIntact"),"exact cached native draw original and arguments once");
        require(count("bsiFixtureLastPrivateBindings")==bindings&&count("bsiFixturePrivateDraws")==priv+(bindings?1:0),"private samplers only inside authorized original");
        require(count("bsiFixtureBindingsRestored")&&!count("bsiFixtureInvalidGl"),"all source state restored with legal GL inspection only");
    }
    void remove(void *context){require(fn<unsigned(WINAPI*)(void*,void*)>("eglDestroyContext")(display,context)!=0,"native context teardown");}
};
}
int main(int argc,char **argv){
    require(argc==4,"pass two explicitly named providers and cached/late mode");const bool cached=std::strcmp(argv[3],"cached")==0;
    auto instance=GetModuleHandleW(nullptr);
    auto root=CreateWindowExW(0,L"STATIC",L"EGL procedure owner",WS_OVERLAPPEDWINDOW,0,0,900,600,nullptr,nullptr,instance,nullptr);
    auto game=CreateWindowExW(0,L"STATIC",L"EGL procedure game",WS_CHILD,0,0,640,360,root,nullptr,instance,nullptr);
    auto foreign=CreateWindowExW(0,L"STATIC",L"unowned EGL procedure",WS_POPUP,0,0,640,360,nullptr,nullptr,instance,nullptr);require(root&&game&&foreign,"native owned/foreign HWNDs");
    Fixture a,b;a.open(argv[1]);b.open(argv[2]);const auto scene=a.context(),presenter=a.context(),other=b.context();require(scene&&presenter&&other,"pre-existing independent contexts");
    a.current(nullptr,scene);a.variant(1);auto *early=a.query("glDrawElements");require(early!=nullptr,"cache draw BEFORE bootstrap");a.current(nullptr,nullptr);
    b.current(nullptr,other);b.variant(1);auto *otherEntry=b.query("glDrawElements");b.current(nullptr,nullptr);require(otherEntry&&otherEntry!=early,"separate providers expose distinct native addresses");
    auto &r=Runtime::instance();r.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});r.start(reinterpret_cast<std::uintptr_t>(root));a.ready();b.ready();
    a.current(nullptr,scene);void *entry=early;unsigned variant=1;
    if(!cached){a.variant(2);variant=2;entry=a.query("glDrawElements");require(entry&&entry!=early,"later query returns another exact code target");}
    require(originalPresentationFunction(entry)!=nullptr,cached?"EGL pre-bootstrap cached target must be discovered after successful make-current BEFORE its first draw":"EGL lookup-returned distinct GL target must be hooked BEFORE its first draw");
    a.resource(4);a.draw(entry,variant,0);require(!r.metrics().textureInspected&&!a.count("bsiFixtureGlQueries"),"code lookup/current binding cannot inspect GL or invent ownership");
    auto queries=a.count("bsiFixtureProcedureQueries");a.current(nullptr,scene);require(a.count("bsiFixtureProcedureQueries")==queries,"known repeated EGL context binding avoids per-frame procedure sweeps");
    const auto target=a.surface(game),unowned=a.surface(foreign);require(target&&unowned,"post-attach tracked drawables");
    a.current(target,presenter);require(a.fn<unsigned(WINAPI*)(void*,void*)>("eglSwapBuffers")(a.display,target)!=0,"select owned presentation only through native swap");
    a.resource(1);a.fn<void(*)(unsigned,unsigned,unsigned)>("bsiFixtureDraw")(0,0,4);require(r.metrics().textureConsumerInputs==1&&!r.metrics().textureDraws,"LINEAR compositor stays native");
    a.current(target,scene);a.resource(4);a.draw(entry,variant,2);require(r.metrics().textureDraws==1,"pre-existing material context on EXACT selected drawable reaches its original with private samplers");
    a.current(unowned,scene);auto gl=a.count("bsiFixtureGlQueries");auto inspected=r.metrics().textureInspected;a.draw(entry,variant,0);
    require(a.count("bsiFixtureGlQueries")==gl&&r.metrics().textureInspected==inspected,"foreign drawable rejected before optional GL state query");
    const auto secret=a.context(true);require(secret!=nullptr,"protected context double");a.current(target,secret);a.variant(3);auto *secretEntry=a.query("glDrawElements");
    gl=a.count("bsiFixtureGlQueries");a.draw(secretEntry,3,0);require(a.count("bsiFixtureGlQueries")==gl&&r.metrics().textureInspected==inspected,"protected current context may discover code, but NEVER GL state or material samplers");
    b.current(nullptr,other);require(originalPresentationFunction(otherEntry)!=nullptr,"cached procedure discovery is independent for the second provider");b.resource(4);b.draw(otherEntry,1,0);
    require(r.metrics().textureDraws==1&&!b.count("bsiFixtureGlQueries"),"a second provider cannot borrow selected-provider ownership or names");
    queries=a.count("bsiFixtureProcedureQueries");gl=a.count("bsiFixtureGlQueries");a.fn<void(*)(int)>("bsiFixtureMakeFailure")(1);a.current(target,scene,false);a.fn<void(*)(int)>("bsiFixtureMakeFailure")(0);
    require(a.count("bsiFixtureProcedureQueries")==queries&&a.count("bsiFixtureGlQueries")==gl,"failed native EGL binding performs no discovery and cannot probe a stale context");
    a.current(nullptr,nullptr);queries=a.count("bsiFixtureProcedureQueries");a.current(nullptr,nullptr);require(a.count("bsiFixtureProcedureQueries")==queries,"null EGL unbind has no dispatch discovery");
    r.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});a.current(target,scene);a.resource(4);a.draw(entry,variant,0);require(!r.metrics().textureInspected&&!r.metrics().textureDraws,"Off state remains native");
    require(!a.query("bsiUnsupportedProcedure")&&!a.query(nullptr),"unknown/null lookup results retained");
    a.current(nullptr,nullptr);b.current(nullptr,nullptr);a.remove(scene);a.remove(presenter);a.remove(secret);b.remove(other);r.stop();DestroyWindow(foreign);DestroyWindow(game);DestroyWindow(root);
    std::printf("PASS: %s EGL exact-target discovery; cached/late provider separation; native results/errors/once-only arguments; real GL-query exclusion before ownership/protection; LINEAR/foreign/Off unchanged\n",cached?"cached":"late");
    std::puts("LIMIT: authored EGL/GL DLL + actual Windows MinHook, not driver/game/pixel/FPS certification");
}
