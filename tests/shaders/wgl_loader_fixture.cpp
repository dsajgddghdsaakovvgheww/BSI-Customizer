// Authored WGL/public-GL state double for real MinHook/Win32 contracts.
// Explicitly named test DLL, NOT opengl32, a proxy, a driver or game pixels.
#include <windows.h>
#include "egl_gl_fixture.h"
#define API extern "C" __declspec(dllexport)
namespace {
struct Context {bool live=false;EglDrawFixture::State gl;} contexts[24];
struct Pbuffer {bool live=false;HDC dc=nullptr;HWND window=nullptr;unsigned leases=0;} pbuffers[16];
thread_local Context *currentContext=nullptr;
thread_local HDC drawDc=nullptr,readDc=nullptr;
volatile LONG swaps=0,gets=0,releases=0,destroys=0,creates=0,shares=0,imageBinds=0;
unsigned failures=0;
Context *newContext(){
    for(auto &context:contexts)if(!context.live){context={};context.live=true;context.gl.desktop=true;context.gl.protectionExtension=false;return &context;}
    return nullptr;
}
Pbuffer *validPbuffer(void *value){for(auto &p:pbuffers)if(&p==value&&p.live)return &p;return nullptr;}
void defaultBuffers(){
    auto &g=EglDrawFixture::state();g.defaultDrawBuffer=g.defaultReadBuffer=0x0405;
    for(const auto &p:pbuffers)if(p.live){if(p.dc==drawDc)g.defaultDrawBuffer=0x0400;if(p.dc==readDc)g.defaultReadBuffer=0x0400;}
}
}
namespace EglDrawFixture {
State &state(){static State unbound;return currentContext?currentContext->gl:unbound;}
bool protectedContext(){return false;}
}
API HGLRC WINAPI bsiWglGetCurrentContext(){return reinterpret_cast<HGLRC>(currentContext);}
API HDC WINAPI bsiWglGetCurrentDC(){return drawDc;}
API HDC WINAPI bsiWglGetCurrentReadDC(){return readDc;}
API HGLRC WINAPI bsiWglCreateContext(HDC){InterlockedIncrement(&creates);return reinterpret_cast<HGLRC>(newContext());}
API HGLRC WINAPI bsiWglCreateContextAttribs(HDC,HGLRC,const int*){InterlockedExchangeAdd(&creates,2);return failures&16?nullptr:reinterpret_cast<HGLRC>(newContext());}
API BOOL WINAPI bsiWglDeleteContext(HGLRC value){
    auto *context=reinterpret_cast<Context*>(value);if(!context||!context->live)return FALSE;
    context->live=false;if(currentContext==context){currentContext=nullptr;drawDc=readDc=nullptr;}return TRUE;
}
API BOOL WINAPI bsiWglShareLists(HGLRC a,HGLRC b){InterlockedIncrement(&shares);return a&&b&&!(failures&8);}
API BOOL WINAPI bsiWglMakeCurrent(HDC dc,HGLRC context){drawDc=readDc=dc;currentContext=reinterpret_cast<Context*>(context);defaultBuffers();return TRUE;}
API BOOL WINAPI bsiWglMakeContextCurrent(HDC draw,HDC read,HGLRC context){drawDc=draw;readDc=read;currentContext=reinterpret_cast<Context*>(context);defaultBuffers();return TRUE;}
API BOOL WINAPI bsiWglSwapBuffers(HDC){InterlockedIncrement(&swaps);return TRUE;}
API HDC WINAPI bsiWglGetPbufferDC(void *value){
    InterlockedIncrement(&gets);auto *p=validPbuffer(value);if(!p||(failures&1))return nullptr;++p->leases;return p->dc;
}
API int WINAPI bsiWglReleasePbufferDC(void *value,HDC dc){
    InterlockedIncrement(&releases);auto *p=validPbuffer(value);if(!p||p->dc!=dc||!p->leases||(failures&2))return 0;--p->leases;return 1;
}
API BOOL WINAPI bsiWglDestroyPbuffer(void *value){
    InterlockedIncrement(&destroys);auto *p=validPbuffer(value);if(!p||(failures&4))return FALSE;
    if(p->window)ReleaseDC(p->window,p->dc);else DeleteDC(p->dc);p->live=false;p->dc=nullptr;return TRUE;
}
API BOOL WINAPI bsiWglQueryPbuffer(void *value,int attribute,int *out){
    if(!validPbuffer(value)||!out||(failures&64))return FALSE;
    switch(attribute){
    case 0x2073:*out=failures&128?0x2078:0x207a;break;
    case 0x2072:*out=failures&256?0x2077:0x2076;break;
    case 0x207b:*out=failures&512?1:0;break;
    default:EglDrawFixture::bad();return FALSE;
    }
    return TRUE;
}
API BOOL WINAPI bsiWglBindTexImage(void *value,int buffer){
    InterlockedIncrement(&imageBinds);
    if(EglDrawFixture::onTransfer)EglDrawFixture::onTransfer();
    SetLastError(0x1a2b3c);
    return validPbuffer(value)&&buffer>=0x2083&&buffer<=0x2086&&!(failures&32);
}
API PROC WINAPI bsiWglGetProcAddress(LPCSTR name){
#define WGL(text,fn) if(std::strcmp(name,text)==0)return reinterpret_cast<PROC>(&fn)
    WGL("wglCreateContextAttribsARB",bsiWglCreateContextAttribs);
    WGL("wglBindTexImageARB",bsiWglBindTexImage);WGL("wglQueryPbufferARB",bsiWglQueryPbuffer);
    WGL("wglGetCurrentReadDCARB",bsiWglGetCurrentReadDC);WGL("wglGetCurrentReadDCEXT",bsiWglGetCurrentReadDC);
    WGL("wglMakeContextCurrentARB",bsiWglMakeContextCurrent);
    WGL("wglGetPbufferDCARB",bsiWglGetPbufferDC);WGL("wglGetPbufferDCEXT",bsiWglGetPbufferDC);
    WGL("wglReleasePbufferDCARB",bsiWglReleasePbufferDC);WGL("wglReleasePbufferDCEXT",bsiWglReleasePbufferDC);
    WGL("wglDestroyPbufferARB",bsiWglDestroyPbuffer);WGL("wglDestroyPbufferEXT",bsiWglDestroyPbuffer);
#undef WGL
    return reinterpret_cast<PROC>(EglDrawFixture::resolve(name));
}
API void *bsiFixturePbuffer(HWND hiddenWindow){
    for(auto &p:pbuffers)if(!p.live){p={};p.window=hiddenWindow;p.dc=hiddenWindow?GetDC(hiddenWindow):CreateCompatibleDC(nullptr);p.live=p.dc!=nullptr;return p.live?&p:nullptr;}
    return nullptr;
}
API HDC bsiFixtureNativePbufferDC(void *value){auto *p=validPbuffer(value);return p?p->dc:nullptr;}
API void bsiFixtureFailures(unsigned flags){failures=flags;}
API LONG bsiFixtureSwapCalls(){return swaps;}
API LONG bsiFixturePbufferGets(){return gets;}
API LONG bsiFixturePbufferReleases(){return releases;}
API LONG bsiFixturePbufferDestroys(){return destroys;}
API LONG bsiFixtureContextCreates(){return creates;}
API LONG bsiFixtureShares(){return shares;}
API void bsiFixtureDraw(unsigned kind,unsigned alias,unsigned mode){EglDrawFixture::invoke(kind,alias,mode);}
API LONG bsiFixtureDrawCalls(){return EglDrawFixture::draws;}
API LONG bsiFixturePrivateDraws(){return EglDrawFixture::privateDraws;}
API LONG bsiFixtureLastPrivateBindings(){return LONG(EglDrawFixture::lastPrivateBindings);}
API LONG bsiFixtureBindingsRestored(){return EglDrawFixture::restored();}
API LONG bsiFixtureArgumentsIntact(){return EglDrawFixture::lastHash==EglDrawFixture::expectedHash;}
API LONG bsiFixtureInvalidGl(){return EglDrawFixture::invalid;}
API void bsiFixtureResourceState(unsigned mode,unsigned output){
    auto &g=EglDrawFixture::state();g.readFramebuffer=g.framebuffer=(mode==2||mode==3)?7:0;g.output=output;
    g.outputType=mode==3?0x8d41:0x1702;
    g.textures=mode==1?std::array<unsigned,4>{{output,0,0,0}}:std::array<unsigned,4>{{1,2,0,0}};
    g.mips=mode!=1;g.depth=g.depthWrite=false;g.depthBits=0;g.blend=g.protectedTexture=false;g.colorWrite=true;
}
API void bsiFixtureTransferState(unsigned readKind,unsigned source,unsigned destination){
    auto &g=EglDrawFixture::state();g.readFramebuffer=readKind?8:0;g.framebuffer=7;
    g.readOutput=source;g.readOutputType=readKind==2?0x8d41:0x1702;g.output=destination;g.outputType=0x1702;
    g.textures[3]=destination;g.colorWrite=false;g.blend=true;
}
API void bsiFixtureTransfer(unsigned copy,unsigned alias){EglDrawFixture::invokeTransfer(copy!=0,alias);}
API LONG bsiFixtureTransferCalls(){return EglDrawFixture::transfers;}
API LONG bsiFixtureTransferArgumentsIntact(){return EglDrawFixture::lastTransferHash==EglDrawFixture::expectedTransferHash;}

API LONG bsiFixtureImageBinds(){return imageBinds;}
API void bsiFixtureNativeTextureTarget(unsigned name){EglDrawFixture::state().textures[3]=name;}
API void bsiFixtureOnTransfer(void(WINAPI *callback)()){EglDrawFixture::onTransfer=callback;}

API void bsiFixtureTransferToDefault(){EglDrawFixture::state().framebuffer=0;}
