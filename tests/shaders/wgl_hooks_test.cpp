// Actual MinHook/Win32 dispatch on an explicitly named authored WGL DLL.
// No opengl32 proxy, GL driver, HD-Player or physical GPU/game pixels execute.
#include "Runtime.h"
#include "PresentationHooks.h"
#include "GlDrawCalls.h"
#undef BSI_GL_DRAW_CALLS
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace BsiShaders;
namespace {
void require(bool value,const char *message){if(!value){std::fprintf(stderr,"FAIL: %s\n%s\n",message,Runtime::instance().metrics().textureStatus.c_str());std::exit(1);}}
LRESULT CALLBACK windowProc(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcW(w,m,a,b);}
struct Fixture {
    HMODULE module=nullptr;
    template<typename T>T function(const char *name){
        const auto entry=GetProcAddress(module,name);require(entry!=nullptr,"required WGL fixture export");return reinterpret_cast<T>(reinterpret_cast<void*>(entry));
    }
    template<typename T>T proc(const char *name){
        const auto entry=function<PROC(WINAPI*)(LPCSTR)>("wglGetProcAddress")(name);require(entry!=nullptr,"query native extension/GL address");return reinterpret_cast<T>(entry);
    }
    LONG count(const char *name){return function<LONG(*)()>(name)();}
    void failures(unsigned bits){function<void(*)(unsigned)>("bsiFixtureFailures")(bits);}
    HGLRC context(HDC dc,HGLRC share=nullptr){
        if(!share)return function<HGLRC(WINAPI*)(HDC)>("wglCreateContext")(dc);
        const int attributes[]{0x2091,4,0x2092,5,0};
        return proc<HGLRC(WINAPI*)(HDC,HGLRC,const int*)>("wglCreateContextAttribsARB")(dc,share,attributes);
    }
    BOOL share(HGLRC parent,HGLRC child){
        const auto before=count("bsiFixtureShares");const auto result=function<BOOL(WINAPI*)(HGLRC,HGLRC)>("wglShareLists")(parent,child);
        require(count("bsiFixtureShares")==before+1,"native sharing executes exactly once with its original result");return result;
    }
    void current(HDC dc,HGLRC context,HDC read=nullptr){
        if(read)require(proc<BOOL(WINAPI*)(HDC,HDC,HGLRC)>("wglMakeContextCurrentARB")(dc,read,context)!=FALSE,"bind distinct native WGL draw/read DCs");
        else require(function<BOOL(WINAPI*)(HDC,HGLRC)>("wglMakeCurrent")(dc,context)!=FALSE,"bind authored native WGL current route");
    }
    void swap(HDC dc){
        const auto before=count("bsiFixtureSwapCalls");require(function<BOOL(WINAPI*)(HDC)>("wglSwapBuffers")(dc)!=FALSE&&count("bsiFixtureSwapCalls")==before+1,"native WGL swap executes once with its original return");
    }
    void resource(unsigned mode,unsigned output=50){function<void(*)(unsigned,unsigned)>("bsiFixtureResourceState")(mode,output);}
    void draw(LONG bindings){
        const auto before=count("bsiFixtureDrawCalls"),privateBefore=count("bsiFixturePrivateDraws");
        function<void(*)(unsigned,unsigned,unsigned)>("bsiFixtureDraw")(static_cast<unsigned>(GlDrawKind::ElementsBaseVertex),0,4);
        require(count("bsiFixtureDrawCalls")==before+1&&count("bsiFixtureArgumentsIntact")&&count("bsiFixtureBindingsRestored")&&!count("bsiFixtureInvalidGl"),"draw original once, exact arguments/active unit/native source samplers, no illegal GL query");
        if(count("bsiFixtureLastPrivateBindings")!=bindings||count("bsiFixturePrivateDraws")!=privateBefore+(bindings?1:0))
            std::fprintf(stderr,"WGL draw #%ld: expected %ld private bindings, observed %ld; private-draw total %ld -> %ld\n",before+1,bindings,count("bsiFixtureLastPrivateBindings"),privateBefore,count("bsiFixturePrivateDraws"));
        require(count("bsiFixtureLastPrivateBindings")==bindings&&count("bsiFixturePrivateDraws")==privateBefore+(bindings?1:0),"private desktop bias+floor bindings must exist INSIDE the original, not merely diagnostic counters");
    }
    void *pbuffer(HWND hidden=nullptr){auto *p=function<void*(*)(HWND)>("bsiFixturePbuffer")(hidden);require(p!=nullptr,"create authored memory/hidden-window pbuffer");return p;}
    HDC nativeDc(void *p){return function<HDC(*)(void*)>("bsiFixtureNativePbufferDC")(p);}
    HDC get(void *p,bool ext=false){
        const auto before=count("bsiFixturePbufferGets");const auto dc=proc<HDC(WINAPI*)(void*)>(ext?"wglGetPbufferDCEXT":"wglGetPbufferDCARB")(p);
        require(count("bsiFixturePbufferGets")==before+1,"each native GetPbufferDC runs once");return dc;
    }
    void release(void *p,HDC dc,int expected=1,bool ext=false){
        const auto before=count("bsiFixturePbufferReleases");const auto result=proc<int(WINAPI*)(void*,HDC)>(ext?"wglReleasePbufferDCEXT":"wglReleasePbufferDCARB")(p,dc);
        require(result==expected&&count("bsiFixturePbufferReleases")==before+1,"each native release preserves exact handle/DC and failure return");
    }
    void destroy(void *p,BOOL expected=TRUE,bool ext=false){
        const auto before=count("bsiFixturePbufferDestroys");const auto result=proc<BOOL(WINAPI*)(void*)>(ext?"wglDestroyPbufferEXT":"wglDestroyPbufferARB")(p);
        require(result==expected&&count("bsiFixturePbufferDestroys")==before+1,"each native pbuffer destruction preserves result and runs exactly once");
    }
    void transferState(unsigned readKind,unsigned source,unsigned destination=50){function<void(*)(unsigned,unsigned,unsigned)>("bsiFixtureTransferState")(readKind,source,destination);}
    void transfer(bool copy,unsigned alias=0){
        const auto before=count("bsiFixtureTransferCalls");function<void(*)(unsigned,unsigned)>("bsiFixtureTransfer")(copy,alias);
        require(count("bsiFixtureTransferCalls")==before+1&&count("bsiFixtureTransferArgumentsIntact")&&count("bsiFixtureBindingsRestored")&&!count("bsiFixtureInvalidGl"),"native WGL-dispatched copy/resolve runs once with exact arguments and no borrowed sampler/state changes");
    }
    void nativeTarget(unsigned name){function<void(*)(unsigned)>("bsiFixtureNativeTextureTarget")(name);}
    void bindImage(void *p,BOOL expected=TRUE,int buffer=0x2083){
        const auto bind=proc<BOOL(WINAPI*)(void*,int)>("wglBindTexImageARB");const auto before=count("bsiFixtureImageBinds");
        SetLastError(0x667788);const auto result=bind(p,buffer);const auto nativeError=GetLastError();
        require(result==expected&&nativeError==0x1a2b3c&&count("bsiFixtureImageBinds")==before+1,
                "native wglBindTexImageARB forwards original pbuffer/buffer once, native return and Win32 LastError unchanged");
        require(count("bsiFixtureBindingsRestored")&&!count("bsiFixtureInvalidGl"),"native binding observation cannot borrow samplers/change active unit");
    }
    void remove(HGLRC context){require(function<BOOL(WINAPI*)(HGLRC)>("wglDeleteContext")(context)!=FALSE,"native WGL context teardown");}
};
}
int main(int argc,char **argv){
    require(argc==2,"pass the explicitly named authored WGL provider DLL");Fixture f;f.module=LoadLibraryA(argv[1]);require(f.module!=nullptr,"load WGL fixture, never a system proxy");
    HWND root=CreateWindowExW(0,L"STATIC",L"WGL owned player",WS_OVERLAPPEDWINDOW,0,0,900,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    HWND game=CreateWindowExW(0,L"STATIC",L"WGL game child",WS_CHILD,0,0,640,360,root,nullptr,GetModuleHandleW(nullptr),nullptr);
    HWND foreign=CreateWindowExW(0,L"STATIC",L"driver-hidden pbuffer window",WS_POPUP,0,0,640,360,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    WNDCLASSW cls{};cls.lpfnWndProc=windowProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"QtWglTestExcluded";
    require(RegisterClassW(&cls)!=0,"register excluded Qt-shaped native window");
    HWND ui=CreateWindowExW(0,cls.lpszClassName,L"WGL UI",WS_CHILD,0,0,640,360,root,nullptr,cls.hInstance,nullptr);
    require(root&&game&&foreign&&ui,"native WGL route fixtures");
    HDC gameDc=GetDC(game),foreignDc=GetDC(foreign),uiDc=GetDC(ui);require(gameDc&&foreignDc&&uiDc,"native owned/foreign/UI DCs");
    const auto presenter=f.context(gameDc);auto *pb=f.pbuffer();const auto pbDc=f.get(pb); // intentionally unobserved pre-start acquisition
    require(presenter&&pbDc&&WindowFromDC(pbDc)==nullptr,"pre-existing pbuffer has no inferable HWND");
    require(setWglProviderForTesting(f.module),"test-only provider injection exists only in native fixture build");
    auto &runtime=Runtime::instance();runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.start(reinterpret_cast<std::uintptr_t>(root));
    require(originalPresentationFunction(reinterpret_cast<void*>(GetProcAddress(f.module,"wglSwapBuffers")))!=nullptr,"actual WGL swap/lifecycle hooks attached");
    f.current(gameDc,presenter);f.swap(gameDc);f.resource(1);f.draw(0);
    require(runtime.metrics().textureConsumerInputs==1&&!runtime.metrics().textureDraws,"single-level selected desktop compositor is NoSampler, not Potato texture success");
    f.current(pbDc,presenter);f.resource(4);f.draw(0);
    require(!runtime.metrics().textureProducerInspected,"an unobserved pbuffer DC still cannot identify its default color output");
    f.resource(2);f.draw(2);f.resource(2,51);f.draw(0);
    f.current(foreignDc,presenter);f.resource(2);f.draw(0);f.current(uiDc,presenter);f.draw(0);
    require(runtime.metrics().textureProducerInspected==1&&runtime.metrics().textureProducerDraws==1,
            "a current no-HWND DC needs no pbuffer identity for an EXACT consumed SAME-context FBO output; unrelated outputs, foreign HWNDs and Qt stay native");
    runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});
    f.current(gameDc,presenter);f.resource(1);f.draw(0);
    require(f.get(pb)==pbDc,"successful native get can prove an existing pbuffer's EXACT current DC");
    f.current(pbDc,presenter);f.resource(2);f.draw(2);f.resource(2,51);f.draw(0);
    require(runtime.metrics().textureProducerDraws==1,"known pbuffer still requires the exact recently consumed FBO output, not merely the context");
    f.failures(16);const auto createsBefore=f.count("bsiFixtureContextCreates");
    require(!f.context(gameDc,presenter)&&f.count("bsiFixtureContextCreates")==createsBefore+2,"failed attributed creation preserves native return and cannot publish context/share evidence");f.failures(0);
    const auto scene=f.context(gameDc,presenter),isolated=f.context(gameDc);require(scene&&isolated,"observed attributed-sharing and isolated contexts");
    f.failures(8);require(!f.share(presenter,isolated),"authored failed native share");f.failures(0);
    const auto consume=[&]{
        runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});
        f.current(gameDc,presenter);f.resource(1);f.draw(0);
    };
    consume();f.current(pbDc,isolated);f.resource(2);f.draw(0);
    f.current(pbDc,scene);f.resource(2);f.draw(2);
    require(runtime.metrics().textureProducerDraws==1,"only SUCCESSFUL observed context sharing plus a fresh consumer qualifies the exact producer");
    require(f.share(presenter,isolated),"successful classic native sharing");consume();f.current(pbDc,isolated);f.resource(2);f.draw(2);
    require(runtime.metrics().textureProducerDraws==1,"observed wglShareLists success also requires fresh actual consumption, not an attributed-create-only shortcut");
    auto *hidden=f.pbuffer(foreign);const auto hiddenDc=f.get(hidden,true);require(hiddenDc&&WindowFromDC(hiddenDc)==foreign,"driver-like pbuffer DC can have a nonselected hidden HWND");
    f.current(hiddenDc,scene);f.resource(2);f.draw(2);
    require(runtime.metrics().textureProducerDraws==2,"positive native pbuffer evidence, not WindowFromDC's hidden HWND, classifies the exact resource-qualified route");
    auto *leased=f.pbuffer();const auto leasedDc=f.get(leased);require(leasedDc&&f.get(leased,true)==leasedDc,"multiple observed leases, both native aliases");
    f.current(leasedDc,scene);f.resource(2);f.draw(2);f.release(leased,leasedDc);f.draw(2);
    f.failures(2);f.release(leased,leasedDc,0,true);f.draw(2);f.failures(0);
    const auto beforeLastLease=runtime.metrics().textureProducerDraws;
    const auto transfersBeforeLastLease=runtime.metrics().textureTransferCalls;
    f.release(leased,leasedDc,1,true);f.draw(2);
    f.transferState(0,0);f.transfer(true);f.resource(2);
    require(runtime.metrics().textureProducerDraws==beforeLastLease+1&&runtime.metrics().textureTransferCalls==transfersBeforeLastLease,
            "last native DC lease removes DEFAULT-image copy proof, not the independently qualified named FBO in its surviving shared GL namespace");
    require(f.get(leased)==leasedDc,"refresh released DC");
    f.failures(4);f.destroy(leased,FALSE);f.draw(2);f.failures(0);
    f.current(nullptr,nullptr);f.destroy(leased,TRUE,true);
    auto *replacement=f.pbuffer();const auto replacementDc=f.get(replacement);require(replacementDc!=nullptr,"reused native pbuffer handle with fresh observed generation");

    consume();f.current(pbDc,scene);f.resource(4);f.draw(0);
    f.transferState(0,0);f.transfer(true);f.resource(4);f.draw(2);
    require(runtime.metrics().textureTransferCalls==1&&runtime.metrics().textureTransferInputs==1&&runtime.metrics().textureProducerDraws==1,"desktop default-pbuffer copy bridges the missing material producer; no frame blur or compositor replacement");
    consume();f.current(replacementDc,scene);f.resource(4);f.draw(0);f.transferState(0,0);f.transfer(true,1);f.resource(4);f.draw(2);
    f.current(nullptr,nullptr);f.destroy(replacement);
    auto *reused=f.pbuffer();const auto reusedDc=f.get(reused);require(reusedDc!=nullptr,"reacquire after destruction/handle reuse");
    f.current(reusedDc,scene);f.resource(4);f.draw(0);
    require(runtime.metrics().textureProducerDraws==1,"native pbuffer destruction revokes its inherited copy proof; reused handles are NOT the old default color buffer");
    auto *readPb=f.pbuffer(),*unknownRead=f.pbuffer();const auto readPbufferDc=f.get(readPb),unknownReadDc=f.nativeDc(unknownRead);
    f.failures(1);require(!f.get(unknownRead),"failed GetPbufferDC preserves native NULL and does not classify an unknown DC");f.failures(0);
    consume();f.current(pbDc,scene,unknownReadDc);f.transferState(0,0);f.transfer(true);
    require(!runtime.metrics().textureTransferCalls,"a known WGL DRAW pbuffer cannot stand in for an unobserved current READ DC");
    f.current(pbDc,scene,readPbufferDc);f.transferState(0,0);f.transfer(true);
    f.current(pbDc,scene);f.resource(4);f.draw(0);f.current(readPbufferDc,scene);f.resource(4);f.draw(2);
    require(runtime.metrics().textureProducerDraws==1,"wglGetCurrentReadDCARB identifies the actual read pbuffer, not the draw DC");
    consume();f.current(unknownReadDc,scene);f.resource(3,70);f.draw(0);
    f.transferState(2,70);f.transfer(false);f.resource(3,70);f.draw(2);
    f.resource(4);f.draw(0);
    require(runtime.metrics().textureTransferCalls==1&&runtime.metrics().textureProducerDraws==1,
            "unmapped no-HWND DC can resolve an exact named RBO into a consumed texture in an OBSERVED shared namespace, but never authorize its unknown default image");
    consume();f.current(pbDc,scene);f.resource(3,70);f.draw(0);
    for(unsigned alias=0;alias<4;++alias){f.transferState(2,70);f.transfer(false,alias);}
    f.resource(3,70);f.draw(2);
    require(runtime.metrics().textureTransferCalls==4&&runtime.metrics().textureTransferInputs==4&&runtime.metrics().textureProducerDraws==1,"desktop core/EXT/ANGLE/NV transfer detours follow a typed renderbuffer resolve, then override its material draw");
    // Render-to-texture binds the pbuffer image without a GL copy. Unlike GL
    // names, the observed native lifetime/color buffer is exact across separate
    // namespaces. Keep unknown/FBO/share-group-only routes rejected above.
    const auto nativeScene=f.context(gameDc);require(nativeScene!=nullptr,"independent nonsharing native pbuffer renderer");
    consume();f.nativeTarget(50);f.bindImage(pb);
    require(runtime.metrics().textureTransferCalls==1&&runtime.metrics().textureTransferInputs==1,
            "successful native WGL image binding publishes an exact pbuffer-color -> consumed-texture link");
    f.current(pbDc,nativeScene);f.resource(4);f.draw(2);f.resource(2,50);f.draw(0);
    require(runtime.metrics().textureProducerDraws==1,"nonsharing renderer can write the exact native default output, but never inherits texture/RBO names from the consumer");
    f.transferState(2,70);f.function<void(*)()>("bsiFixtureTransferToDefault")();f.transfer(false);
    f.resource(3,70);f.draw(2);
    require(runtime.metrics().textureTransferCalls==2&&runtime.metrics().textureProducerDraws==2,
            "a blit into the native output must preserve its DRAW color aspect and reach the exact preceding RBO material producer");
    for(unsigned flags:{32u,64u,128u,256u,512u}){
        consume();f.nativeTarget(50);f.failures(flags);f.bindImage(pb,flags==32?FALSE:TRUE);f.failures(0);
        f.current(pbDc,nativeScene);f.resource(4);f.draw(0);
        require(!runtime.metrics().textureTransferCalls&&!runtime.metrics().textureProducerDraws,
                "failed native bind/query, cubemap/noncolor and nonzero mip cannot create image-binding evidence");
    }
    consume();f.nativeTarget(50);f.bindImage(pb,TRUE,0x2085); // BACK_LEFT, not this single-buffer pbuffer's FRONT_LEFT
    f.current(pbDc,nativeScene);f.resource(4);f.draw(0);
    require(!runtime.metrics().textureProducerDraws,"different FRONT/BACK color buffers of the SAME native pbuffer are not the same output");
    consume();f.nativeTarget(50);f.bindImage(unknownRead);f.bindImage(pb,FALSE,0);
    require(!runtime.metrics().textureTransferCalls,"unobserved pbuffer lifetime/unsupported buffer cannot publish an image link");
    consume();f.nativeTarget(50);f.bindImage(pb);f.current(pbDc,nativeScene);f.resource(4);f.draw(2);
    f.current(gameDc,presenter);for(unsigned i=0;i<3;++i)f.swap(gameDc);
    f.current(pbDc,scene);f.resource(3,70);f.draw(0);
    runtime.setTexture({false,true,TextureDrawPolicy::OwnedMipCompatibility});f.draw(0);
    require(!runtime.metrics().textureInspected&&!runtime.metrics().textureTransferCalls&&!runtime.metrics().textureProducerDraws,"Off clears sampler and transfer evidence and leaves exact native bindings");
    runtime.setTexture({true,true,TextureDrawPolicy::OwnedMipCompatibility});f.current(gameDc,presenter);f.resource(1);f.draw(0);
    f.remove(presenter);f.current(pbDc,scene);f.resource(2);f.draw(0);
    require(!runtime.metrics().observed&&!runtime.metrics().textureInspected,"presenter destruction revokes WGL pbuffer producers; none may select presentation from a draw");
    f.current(nullptr,nullptr);f.remove(scene);f.remove(isolated);f.remove(nativeScene);f.destroy(pb);f.destroy(hidden);f.destroy(reused);f.destroy(readPb);f.destroy(unknownRead);
    runtime.setTexture({});runtime.stop();
    require(!f.count("bsiFixtureInvalidGl"),"no optional GL query/state failure in WGL fixture");
    ReleaseDC(game,gameDc);ReleaseDC(foreign,foreignDc);ReleaseDC(ui,uiDc);DestroyWindow(ui);DestroyWindow(foreign);DestroyWindow(game);DestroyWindow(root);UnregisterClassW(cls.lpszClassName,cls.hInstance);
    std::puts("PASS: actual WGL MinHook/Win32 contracts, observed exact pbuffer DCs including driver-like hidden HWNDs; named-only FBO/RBO producers on current no-HWND DCs without default-image guesses; unknown-default/Qt/foreign/failed-share exclusion; native leases/failure returns/refresh/destroy/reuse; default-read copy and RBO blit provenance, desktop bias+floor samplers INSIDE originals, once-only exact arguments, distinct READ DC, expiry, Off and source teardown");
    std::puts("LIMIT: authored WGL/GL dispatch doubles, not a system proxy, real GL driver/game pixels or physical GPU certification");
}
