// Actual production MinHook + Win32 windows on AUTHORED split EGL/GL doubles.
// There is no proprietary player, EGL driver, GPU memory or pixel test here.
#include "Runtime.h"
#include "PresentationHooks.h"
#include "egl_split_fixture_api.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <set>
#include <thread>
#include <type_traits>
using namespace BsiShaders;
namespace {
void require(bool ok,const char *message){
    if(!ok){std::fprintf(stderr,"FAIL: %s\n%s\n",message,Runtime::instance().metrics().textureStatus.c_str());std::exit(1);}
}
template<typename T>std::uintptr_t value(T item){
    if constexpr(std::is_pointer_v<T>)return reinterpret_cast<std::uintptr_t>(item);
    else return static_cast<std::uintptr_t>(item);
}
struct Fixture {
    HMODULE shim=nullptr,backend=nullptr;void *display=nullptr;bool queried=false,cached=false;
    std::array<void*,EglSplitFixture::Count> early{};
    template<typename T>T fn(HMODULE module,const char *name){
        const auto address=GetProcAddress(module,name);require(address!=nullptr,"explicit authored fixture export");
        return reinterpret_cast<T>(reinterpret_cast<void*>(address));
    }
    LONG count(const char *name){return fn<LONG(*)()>(backend,name)();}
    LONG calls(unsigned entry){return fn<LONG(*)(unsigned)>(backend,"bsiSplitCalls")(entry);}
    void fail(unsigned entry,bool failure){fn<void(*)(unsigned,int)>(backend,"bsiSplitFailure")(entry,failure);}
    void *implementation(const char *name){return reinterpret_cast<void*>(GetProcAddress(backend,(std::string("bsiImpl_")+name).c_str()));}
    void open(const char *shimPath,const char *backendPath){
        backend=LoadLibraryA(backendPath);shim=LoadLibraryA(shimPath);require(shim&&backend,"load explicitly named import shim and implementation DLL");
        require(!GetProcAddress(backend,"eglSwapBuffers"),"backend cannot be independently classified as another public EGL provider");
        // In this Release fixture each public entry is a real import jump,
        // not a PE forwarder or two names for one implementation address.
#define BSI_CHECK_SHIM(Result,Name,Params,Args) { \
        const auto *code=reinterpret_cast<const unsigned char*>(GetProcAddress(shim,#Name)); \
        const auto *jump=code&&code[0]==0x48?code+1:code; \
        require(jump&&jump[0]==0xff&&jump[1]==0x25,"public shim begins with a RIP-indirect imported-code jump"); \
        std::int32_t offset=0;std::memcpy(&offset,jump+2,4);void *imported=nullptr;std::memcpy(&imported,jump+6+offset,sizeof(imported)); \
        require(imported==implementation(#Name)&&imported!=code,"declared shim import and resolver implementation live at distinct addresses"); \
        }
        BSI_EGL_SPLIT_APIS(BSI_CHECK_SHIM)
#undef BSI_CHECK_SHIM
        display=fn<void*(*)()>(backend,"bsiFixtureDisplay")();
#define BSI_CACHE_ENTRY(Result,Name,Params,Args) early[EglSplitFixture::Name]=query(#Name);
        BSI_EGL_SPLIT_APIS(BSI_CACHE_ENTRY)
#undef BSI_CACHE_ENTRY
    }
    void ready(){
        for(unsigned i=0;i<100&&!originalPresentationFunction(reinterpret_cast<void*>(GetProcAddress(shim,"eglSwapBuffers")));++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        require(originalPresentationFunction(reinterpret_cast<void*>(GetProcAddress(shim,"eglSwapBuffers")))!=nullptr,"complete public shim attached by production scanner");
    }
    template<unsigned Entry,typename Function,typename... Args>auto invoke(Function function,Args... args){
        const auto before=calls(Entry);const bool expectedFailure=forcedFailure==Entry;
        SetLastError(0x424277);const auto result=function(args...);const auto error=GetLastError();
        require(error==EglSplitFixture::nativeError(Entry,expectedFailure),"exact native Win32 LastError survives optional observation");
        require(calls(Entry)==before+1,"each exact native implementation executes ONCE through shim/implementation nesting");
        require(fn<DWORD(*)(unsigned)>(backend,"bsiSplitIncomingError")(Entry)==0x424277,"optional pre-observation preserves the native original's incoming LastError too");
        const std::array<std::uintptr_t,sizeof...(Args)> arguments{{value(args)...}};
        require(fn<int(*)(unsigned,const std::uintptr_t*,unsigned)>(backend,"bsiSplitArguments")(Entry,arguments.data(),unsigned(arguments.size()))!=0,"all native EGL arguments arrive unchanged");
        require(fn<std::uintptr_t(*)(unsigned)>(backend,"bsiSplitResult")(Entry)==value(result),"caller receives native implementation result unchanged");
        return result;
    }
    unsigned forcedFailure=EglSplitFixture::Count;
    template<typename T>T entry(unsigned kind,const char *name){
        if(cached)return reinterpret_cast<T>(early[kind]);
        if(queried)return reinterpret_cast<T>(query(name));
        return fn<T>(shim,name);
    }
    void *query(const char *name,bool viaCachedResolver=false){
        auto resolver=viaCachedResolver?reinterpret_cast<void*(WINAPI*)(const char*)>(early[EglSplitFixture::eglGetProcAddress]):fn<void*(WINAPI*)(const char*)>(shim,"eglGetProcAddress");
        return invoke<EglSplitFixture::eglGetProcAddress>(resolver,name);
    }
    void *context(bool secret=false,void *share=nullptr){
        const int attrs[]{0x32c0,secret,0x3038};
        return invoke<EglSplitFixture::eglCreateContext>(entry<void*(WINAPI*)(void*,void*,void*,const int*)>(EglSplitFixture::eglCreateContext,"eglCreateContext"),display,reinterpret_cast<void*>(0x1230),share,attrs);
    }
    void *surface(HWND window,unsigned family=0,bool secret=false){
        const int attrs[]{0x32c0,secret,0x3038};
        if(family==1){const std::intptr_t wide[]{0x32c0,secret,0x3038};return invoke<EglSplitFixture::eglCreatePlatformWindowSurface>(entry<void*(WINAPI*)(void*,void*,void*,const std::intptr_t*)>(EglSplitFixture::eglCreatePlatformWindowSurface,"eglCreatePlatformWindowSurface"),display,reinterpret_cast<void*>(0x2340),reinterpret_cast<void*>(window),wide);}
        if(family==2)return invoke<EglSplitFixture::eglCreatePlatformWindowSurfaceEXT>(entry<void*(WINAPI*)(void*,void*,void*,const int*)>(EglSplitFixture::eglCreatePlatformWindowSurfaceEXT,"eglCreatePlatformWindowSurfaceEXT"),display,reinterpret_cast<void*>(0x3450),reinterpret_cast<void*>(window),attrs);
        return invoke<EglSplitFixture::eglCreateWindowSurface>(entry<void*(WINAPI*)(void*,void*,void*,const int*)>(EglSplitFixture::eglCreateWindowSurface,"eglCreateWindowSurface"),display,reinterpret_cast<void*>(0x4560),reinterpret_cast<void*>(window),attrs);
    }
    void current(void *surface,void *context,void *read=nullptr){
        require(invoke<EglSplitFixture::eglMakeCurrent>(entry<unsigned(WINAPI*)(void*,void*,void*,void*)>(EglSplitFixture::eglMakeCurrent,"eglMakeCurrent"),display,surface,read?read:surface,context)!=0,"successful exact native bind");
    }
    void swap(void *surface){require(invoke<EglSplitFixture::eglSwapBuffers>(entry<unsigned(WINAPI*)(void*,void*)>(EglSplitFixture::eglSwapBuffers,"eglSwapBuffers"),display,surface)!=0,"native swap succeeds");}
    unsigned remove(void *object,bool context=false,bool failure=false){
        const auto kind=context?EglSplitFixture::eglDestroyContext:EglSplitFixture::eglDestroySurface;
        const auto name=context?"eglDestroyContext":"eglDestroySurface";fail(kind,failure);forcedFailure=failure?unsigned(kind):unsigned(EglSplitFixture::Count);
        const auto result=context?invoke<EglSplitFixture::eglDestroyContext>(entry<unsigned(WINAPI*)(void*,void*)>(kind,name),display,object)
            :invoke<EglSplitFixture::eglDestroySurface>(entry<unsigned(WINAPI*)(void*,void*)>(kind,name),display,object);
        fail(kind,false);forcedFailure=EglSplitFixture::Count;require((result!=0)!=failure,"failed/successful native teardown retained");return result;
    }
    void *pbuffer(bool secret=false){
        const int attrs[]{0x32c0,secret,0x3038};
        return invoke<EglSplitFixture::eglCreatePbufferSurface>(entry<void*(WINAPI*)(void*,void*,const int*)>(EglSplitFixture::eglCreatePbufferSurface,"eglCreatePbufferSurface"),display,reinterpret_cast<void*>(0x5670),attrs);
    }
    void bind(void *surface,bool failure=false){
        fail(EglSplitFixture::eglBindTexImage,failure);forcedFailure=failure?unsigned(EglSplitFixture::eglBindTexImage):unsigned(EglSplitFixture::Count);
        const auto result=invoke<EglSplitFixture::eglBindTexImage>(entry<unsigned(WINAPI*)(void*,void*,int)>(EglSplitFixture::eglBindTexImage,"eglBindTexImage"),display,surface,0x3084);
        fail(EglSplitFixture::eglBindTexImage,false);forcedFailure=EglSplitFixture::Count;require((result!=0)!=failure,"exact native image-bind SUCCESS/FAILURE retained");
    }
    void nativeTarget(unsigned name){fn<void(*)(unsigned)>(backend,"bsiFixtureNativeTextureTarget")(name);}
    void resource(unsigned mode){fn<void(*)(unsigned,unsigned)>(backend,"bsiFixtureResourceState")(mode,50);}
    void draw(LONG expected){
        const auto before=count("bsiFixtureDrawCalls"),privateBefore=count("bsiFixturePrivateDraws");
        fn<void(*)(unsigned,unsigned,unsigned)>(backend,"bsiFixtureDraw")(0,0,4);
        require(count("bsiFixtureDrawCalls")==before+1&&count("bsiFixtureArgumentsIntact"),"exact GL original and arguments once");
        require(count("bsiFixtureLastPrivateBindings")==expected&&count("bsiFixturePrivateDraws")==privateBefore+(expected?1:0),"private material samplers exist INSIDE native draw only on authorized route");
        require(count("bsiFixtureBindingsRestored")&&!count("bsiFixtureInvalidGl"),"all bindings restored, no invalid GL query");
    }
};
}
int main(int argc,char **argv){
    require(argc==6,"pass two shim/backend pairs and an explicit contract mode");const std::string mode=argv[5];
    require(mode=="exports"||mode=="queried"||mode=="cached"||mode=="present"||mode=="retire-surface"||mode=="retire-context"||mode=="resolver"||mode=="native-image"||mode=="capacity","recognized split-dispatch contract mode");
    const auto instance=GetModuleHandleW(nullptr);
    const auto root=CreateWindowExW(0,L"STATIC",L"Split EGL root",WS_OVERLAPPEDWINDOW,0,0,900,600,nullptr,nullptr,instance,nullptr);
    const auto game=CreateWindowExW(0,L"STATIC",L"Split EGL game",WS_CHILD,0,0,640,360,root,nullptr,instance,nullptr);
    const auto foreign=CreateWindowExW(0,L"STATIC",L"Split EGL foreign",WS_POPUP,0,0,640,360,nullptr,nullptr,instance,nullptr);require(root&&game&&foreign,"real owned/foreign native HWNDs");
    Fixture a,b;a.open(argv[1],argv[2]);b.open(argv[3],argv[4]);
    auto &runtime=Runtime::instance();runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.start(value(root));a.ready();b.ready();
    a.queried=mode=="queried"||mode=="native-image";a.cached=mode=="cached";
    const auto scene=a.context();const auto target=a.surface(game);require(scene&&target,"native context and owned window surface");a.current(target,scene);
    if(mode=="resolver"){
        a.fn<void(*)(unsigned)>(a.backend,"bsiFixtureProcedureVariant")(2);const auto exact=a.query("glDrawElements",true);
        require(exact&&originalPresentationFunction(exact),"cached implementation resolver must cover a NEW returned GL target before its first native draw");
    }
    if(mode=="present")a.queried=true;
    a.swap(target);
    require(runtime.metrics().observed&&runtime.metrics().renderer==Renderer::OpenGLES,"owned create/current/present through split dispatch must select the exact game drawable");
    if(mode=="retire-surface"||mode=="retire-context"){
        a.queried=true;const bool context=mode=="retire-context";const auto object=context?scene:target;
        a.remove(object,context,true);require(runtime.metrics().observed,"FAILED implementation teardown must not release live presentation");
        a.remove(object,context);require(!runtime.metrics().observed,"SUCCESSFUL implementation teardown must revoke selected presentation immediately");
        a.resource(4);a.draw(0);require(!runtime.metrics().textureInspected&&!runtime.metrics().textureDraws,"retired ownership cannot authorize later draws");
        runtime.stop();DestroyWindow(foreign);DestroyWindow(game);DestroyWindow(root);
        std::printf("PASS: %s exact original/results/errors and native retirement across split import/queried aliases\n",mode.c_str());return 0;
    }
    if(mode=="native-image"){
        const auto isolated=a.context(),shared=a.context(false,scene),offscreen=a.pbuffer(),protectedPbuffer=a.pbuffer(true);
        require(isolated&&shared&&offscreen&&protectedPbuffer,"queried typed context/share/pbuffer implementations");
        const auto consume=[&]{
            runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});
            a.current(target,scene);a.resource(1);a.draw(0);a.nativeTarget(50);
        };
        consume();a.current(offscreen,shared);a.resource(2);a.draw(2);
        require(runtime.metrics().textureProducerDraws==1,"queried context creation records ONE explicit share namespace through nested import/body calls");
        a.current(offscreen,isolated);a.resource(2);a.draw(0);require(runtime.metrics().textureProducerDraws==1,"equal isolated texture names do not borrow that explicit sharing");
        consume();a.bind(offscreen,true);a.current(offscreen,isolated);a.resource(4);a.draw(0);
        require(!runtime.metrics().textureTransferCalls&&!runtime.metrics().textureProducerDraws,"failed queried native binding publishes no image/resource proof");
        consume();a.bind(offscreen);
        require(runtime.metrics().textureTransferCalls==1&&runtime.metrics().textureTransferInputs==1&&!runtime.metrics().textureProducerDraws,"nested public/implementation image binding publishes ONCE, only after native SUCCESS");
        a.current(offscreen,isolated);a.resource(4);a.draw(2);
        require(runtime.metrics().textureProducerDraws==1&&runtime.metrics().textureProducerBindings==2,"queried native pbuffer link reaches NONSHARING upstream material, not the LINEAR compositor");
        a.resource(2);a.draw(0);require(runtime.metrics().textureProducerDraws==1,"native image link cannot merge isolated FBO texture names");
        a.current(target,scene);for(unsigned i=0;i<3;++i)a.swap(target);
        a.current(offscreen,isolated);a.resource(4);a.draw(0);require(runtime.metrics().textureProducerDraws==1,"queried alias discovery does not renew resource TTL");
        consume();a.bind(protectedPbuffer);require(!runtime.metrics().textureTransferCalls,"protected pbuffer creation cannot publish a native image source");
        consume();a.bind(offscreen);a.remove(offscreen);a.current(offscreen,isolated);a.resource(4);a.draw(0);
        require(!runtime.metrics().textureProducerDraws,"queried pbuffer retirement revokes the exact native lifetime, even if a raw double later binds its stale handle");
        runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});a.current(target,scene);a.resource(4);a.draw(0);
        require(!runtime.metrics().textureInspected&&!runtime.metrics().textureDraws,"Off remains native after queried image/share activity");
        a.remove(target);a.remove(protectedPbuffer);a.remove(isolated,true);a.remove(shared,true);a.remove(scene,true);
        runtime.stop();DestroyWindow(foreign);DestroyWindow(game);DestroyWindow(root);
        std::puts("PASS: queried split EGL typed sharing/pbuffer/image binding, exact native arguments/results/incoming+outgoing LastError, once-only publication, isolated namespaces, protected/failure/TTL/retirement/LINEAR/Off controls");return 0;
    }
    a.resource(1);a.draw(0);require(!runtime.metrics().textureDraws,"legal one-level LINEAR compositor remains native");
    a.resource(4);a.draw(2);require(runtime.metrics().textureDraws==1,"existing material mips reach private samplers through split provider");
    if(mode=="capacity"){
        std::set<void*> targets;
        // BOTH providers' normal bodies already occupy the global typed pool.
        // Fourteen extra targets fill its sixteen slots. Further targets must
        // remain native, NOT retain ownership with uncovered future lifetimes.
        targets.insert(a.implementation("eglCreateWindowSurface"));targets.insert(b.implementation("eglCreateWindowSurface"));
        require(targets.size()==2,"two independently discovered normal implementation targets count toward the SAME bounded code pool, not one provider namespace");
        for(unsigned variant=1;variant<=16;++variant){
            a.fn<void(*)(unsigned)>(a.backend,"bsiSplitCreateVariant")(variant);const auto entry=a.query("eglCreateWindowSurface");
            require(entry&&targets.insert(entry).second,"authored unmerged independent lifecycle code targets");
            const bool attached=originalPresentationFunction(entry)!=nullptr;
            if(attached!=(variant<=14))std::fprintf(stderr,"capacity variant %u: attached=%u; two normal targets already present\n",variant,unsigned(attached));
            require(attached==(variant<=14),"bounded typed alias pool fails explicitly at capacity without changing returned address");
        }
        runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});
        auto queries=a.count("bsiFixtureGlQueries");a.resource(4);a.draw(0);a.swap(target);
        require(!runtime.metrics().textureInspected&&!runtime.metrics().textureDraws&&a.count("bsiFixtureGlQueries")==queries,"uncovered known lifecycle alias disables optional provider work BEFORE GL inspection");
        a.queried=true;const auto nativeOnly=a.surface(game);require(nativeOnly!=nullptr,"pool exhaustion preserves successful native creation through the uncovered returned target");a.current(nativeOnly,scene);a.swap(nativeOnly);a.resource(4);a.draw(0);
        require(!runtime.metrics().textureInspected&&!runtime.metrics().textureDraws,"new native success cannot re-enable a faulted lifecycle provider or fabricate ownership");
        a.remove(nativeOnly);a.remove(target);a.remove(scene,true);runtime.stop();DestroyWindow(foreign);DestroyWindow(game);DestroyWindow(root);
        std::puts("PASS: sixteen exact lifecycle alias slots, explicit overflow, unchanged native pointer/result/args/errors and fail-closed optional work; no GL/resource authorization on exhaustion");return 0;
    }
    const auto inspected=runtime.metrics().textureInspected;
    const auto unowned=a.surface(foreign),secret=a.context(true),secretSurface=a.surface(game,0,true);
    a.current(unowned,scene);auto queries=a.count("bsiFixtureGlQueries");a.resource(4);a.draw(0);require(a.count("bsiFixtureGlQueries")==queries,"foreign drawable rejected before GL queries");
    a.current(target,secret);queries=a.count("bsiFixtureGlQueries");a.resource(4);a.draw(0);require(a.count("bsiFixtureGlQueries")==queries,"protected context rejected before GL queries");
    a.current(secretSurface,scene);queries=a.count("bsiFixtureGlQueries");a.resource(4);a.draw(0);require(a.count("bsiFixtureGlQueries")==queries&&runtime.metrics().textureInspected==inspected,"protected surface cannot borrow selected ownership");
    b.queried=true;const auto other=b.context(),otherTarget=b.surface(game);b.current(otherTarget,other);queries=b.count("bsiFixtureGlQueries");b.resource(4);b.draw(0);
    require(b.count("bsiFixtureGlQueries")==queries&&runtime.metrics().textureDraws==1,"second provider cannot borrow selected namespace/ownership from identical resource names or HWND");
    a.current(target,scene);runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});a.resource(4);a.draw(0);
    require(!runtime.metrics().textureDraws&&!runtime.metrics().textureInspected,"Off clears evidence and leaves all samplers native");
    runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});a.resource(4);a.draw(2);require(runtime.metrics().textureDraws==1,"re-enable only restores authorized existing material route");
    a.remove(target);require(!runtime.metrics().observed,"surface retirement releases selected source in every dispatch mode");
    // Both platform attribute widths use their own exact typed native original.
    for(unsigned family:{1u,2u}){
        const auto platform=a.surface(game,family);require(platform!=nullptr,"typed platform surface");a.current(platform,scene);a.swap(platform);require(runtime.metrics().observed,"core/EXT platform-created drawable tracked");
        a.remove(platform);require(!runtime.metrics().observed,"platform lifetime revoked");
    }
    a.remove(scene,true);a.remove(secret,true);b.remove(other,true);a.remove(unowned);a.remove(secretSurface);b.remove(otherTarget);
    require(!a.query("bsiUnsupportedProcedure")&&!a.query(nullptr),"unknown/null lookup results unchanged");
    runtime.stop();DestroyWindow(foreign);DestroyWindow(game);DestroyWindow(root);
    std::printf("PASS: %s split-import/implementation dispatch, exact native calls/args/results/LastError, actual MinHook private binding DURING authorized original, LINEAR/protected/foreign/provider/Off/retirement controls\n",mode.c_str());
    std::puts("LIMIT: authored EGL/GL doubles; not Windows EGL driver pixels, game material/FPS or hardware certification");
}
