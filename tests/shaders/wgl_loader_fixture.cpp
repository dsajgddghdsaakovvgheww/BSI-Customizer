// Authored WGL/public-GL state double for real MinHook/Win32 contracts.
// Explicitly named test DLL, NOT opengl32, a proxy, a driver or game pixels.
#include <windows.h>
#include "egl_gl_fixture.h"
#define API extern "C" __declspec(dllexport)
namespace {
struct Context {bool live=false;unsigned procedures=0;EglDrawFixture::State gl;} contexts[24];
struct Pbuffer {bool live=false;HDC dc=nullptr;HWND window=nullptr;unsigned leases=0;} pbuffers[16];
thread_local Context *currentContext=nullptr;
thread_local HDC drawDc=nullptr,readDc=nullptr;
volatile LONG swaps=0,gets=0,releases=0,destroys=0,creates=0,shares=0,imageBinds=0,lateTextureDeletes=0,lateRenderbufferDeletes=0;
volatile LONG procedureQueries=0,makeCalls=0,makeContextCalls=0;
HDC lastMakeDraw=nullptr,lastMakeRead=nullptr;HGLRC lastMakeContext=nullptr;
unsigned failures=0;
PROC lateElements(unsigned bank){
    static const PROC entries[]{
        reinterpret_cast<PROC>(&EglDrawFixture::drawElements<BsiShaders::glDrawAliases>),
        reinterpret_cast<PROC>(&EglDrawFixture::drawElements<BsiShaders::glDrawAliases+1>),
        reinterpret_cast<PROC>(&EglDrawFixture::drawElements<BsiShaders::glDrawAliases+2>),
        reinterpret_cast<PROC>(&EglDrawFixture::drawElements<BsiShaders::glDrawAliases+3>)};
    return entries[bank&3];
}
void WINAPI lateDeleteTextures(int count,const unsigned*){InterlockedExchangeAdd(&lateTextureDeletes,count);}
void WINAPI lateDeleteRenderbuffers(int count,const unsigned*){InterlockedExchangeAdd(&lateRenderbufferDeletes,count);}
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
API BOOL WINAPI bsiWglMakeCurrent(HDC dc,HGLRC context){
    InterlockedIncrement(&makeCalls);lastMakeDraw=lastMakeRead=dc;lastMakeContext=context;
    if(failures&1024){SetLastError(0x4422f0);return FALSE;}
    drawDc=readDc=dc;currentContext=reinterpret_cast<Context*>(context);defaultBuffers();SetLastError(0x442201);return TRUE;
}
API BOOL WINAPI bsiWglMakeContextCurrent(HDC draw,HDC read,HGLRC context){
    InterlockedIncrement(&makeContextCalls);lastMakeDraw=draw;lastMakeRead=read;lastMakeContext=context;
    if(failures&2048){SetLastError(0x4422f1);return FALSE;}
    drawDc=draw;readDc=read;currentContext=reinterpret_cast<Context*>(context);defaultBuffers();SetLastError(0x442202);return TRUE;
}
API BOOL WINAPI bsiWglMakeContextCurrentExt(HDC draw,HDC read,HGLRC context){
    InterlockedIncrement(&makeContextCalls);lastMakeDraw=draw;lastMakeRead=read;lastMakeContext=context;
    if(failures&2048){SetLastError(0x4422f1);return FALSE;}
    drawDc=draw;readDc=read;currentContext=reinterpret_cast<Context*>(context);defaultBuffers();SetLastError(0x442203);return TRUE;
}
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
    InterlockedIncrement(&procedureQueries);SetLastError(0x13579b);
    if(!name)return nullptr;
    // The presenter cannot resolve these distinct entry points. A later native
    // lookup on another dispatch table must be observed before its first draw.
    if(currentContext){
        const auto variant=currentContext->procedures;
        if(std::strcmp(name,"glDrawElements")==0){
            static constexpr std::intptr_t invalid[]{1,2,3,-1};const auto failure=variant>>8;
            if(failure>=1&&failure<=4)return reinterpret_cast<PROC>(invalid[failure-1]);
            if(variant&1)return lateElements(variant>>16);
        }
        if((variant&2)&&std::strcmp(name,"glBlitFramebuffer")==0)return reinterpret_cast<PROC>(&EglDrawFixture::blitFramebuffer<4>);
        if((variant&4)&&std::strcmp(name,"glCopyTexSubImage2D")==0)return reinterpret_cast<PROC>(&EglDrawFixture::copyTexSubImage2D<2>);
        if((variant&8)&&std::strcmp(name,"glDeleteTextures")==0)return reinterpret_cast<PROC>(&lateDeleteTextures);
        if((variant&16)&&std::strcmp(name,"glDeleteRenderbuffers")==0)return reinterpret_cast<PROC>(&lateDeleteRenderbuffers);
    }
    if(std::strcmp(name,"bsiInvalidOne")==0)return reinterpret_cast<PROC>(std::intptr_t(1));
    if(std::strcmp(name,"bsiInvalidTwo")==0)return reinterpret_cast<PROC>(std::intptr_t(2));
    if(std::strcmp(name,"bsiInvalidThree")==0)return reinterpret_cast<PROC>(std::intptr_t(3));
    if(std::strcmp(name,"bsiInvalidMinusOne")==0)return reinterpret_cast<PROC>(std::intptr_t(-1));
#define WGL(text,fn) if(std::strcmp(name,text)==0)return reinterpret_cast<PROC>(&fn)
    WGL("wglCreateContextAttribsARB",bsiWglCreateContextAttribs);
    WGL("wglBindTexImageARB",bsiWglBindTexImage);WGL("wglQueryPbufferARB",bsiWglQueryPbuffer);
    WGL("wglGetCurrentReadDCARB",bsiWglGetCurrentReadDC);WGL("wglGetCurrentReadDCEXT",bsiWglGetCurrentReadDC);
    WGL("wglMakeContextCurrentARB",bsiWglMakeContextCurrent);WGL("wglMakeContextCurrentEXT",bsiWglMakeContextCurrentExt);
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
    g.samplers[1].min=mode==1?0x2601:0x2703; // legal LINEAR consumer, existing trilinear material source otherwise
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

API void bsiFixtureProcedureVariant(unsigned variant){if(currentContext)currentContext->procedures=variant;}
API LONG bsiFixtureLateTextureDeletes(){return lateTextureDeletes;}
API LONG bsiFixtureLateRenderbufferDeletes(){return lateRenderbufferDeletes;}
API LONG bsiFixtureProcedureQueries(){return procedureQueries;}
API LONG bsiFixtureMakeCalls(){return makeCalls;}
API LONG bsiFixtureMakeContextCalls(){return makeContextCalls;}
API LONG bsiFixtureMakeArguments(HDC draw,HDC read,HGLRC context){return lastMakeDraw==draw&&lastMakeRead==read&&lastMakeContext==context;}
API void bsiFixtureCachedDraw(PROC entry,unsigned bank,unsigned mode){
    constexpr unsigned kind=static_cast<unsigned>(BsiShaders::GlDrawKind::Elements);
    constexpr int count=13;constexpr unsigned type=0x1403;const auto *indices=reinterpret_cast<void*>(0x123400);
    EglDrawFixture::expectedHash=EglDrawFixture::hash(kind,BsiShaders::glDrawAliases+(bank&3),mode,count,type,indices);
    reinterpret_cast<void(WINAPI*)(unsigned,int,unsigned,const void*)>(entry)(mode,count,type,indices);
}
API void bsiFixtureLateDraw(unsigned mode){
    constexpr unsigned kind=static_cast<unsigned>(BsiShaders::GlDrawKind::Elements),alias=BsiShaders::glDrawAliases;
    constexpr int count=13;constexpr unsigned type=0x1403;const auto *indices=reinterpret_cast<void*>(0x123400);
    EglDrawFixture::expectedHash=EglDrawFixture::hash(kind,alias,mode,count,type,indices);
    using Function=void(WINAPI*)(unsigned,int,unsigned,const void*);
    void *volatile target=reinterpret_cast<void*>(&EglDrawFixture::drawElements<alias>);
    reinterpret_cast<Function>(target)(mode,count,type,indices);
}
API void bsiFixtureLateTransfer(unsigned copy){
    if(copy){
        constexpr unsigned target=0x0de1;constexpr int level=0,dx=3,dy=4,x=1,y=2,width=57,height=56;
        EglDrawFixture::expectedTransferHash=EglDrawFixture::hash(101,2,target,level,dx,dy,x,y,width,height);
        using Function=void(WINAPI*)(unsigned,int,int,int,int,int,int,int);
        void *volatile entry=reinterpret_cast<void*>(&EglDrawFixture::copyTexSubImage2D<2>);
        reinterpret_cast<Function>(entry)(target,level,dx,dy,x,y,width,height);
    }else{
        constexpr int sx0=1,sy0=2,sx1=61,sy1=62,dx0=3,dy0=4,dx1=59,dy1=60;constexpr unsigned mask=0x4000,filter=0x2600;
        EglDrawFixture::expectedTransferHash=EglDrawFixture::hash(100,4,sx0,sy0,sx1,sy1,dx0,dy0,dx1,dy1,mask,filter);
        using Function=void(WINAPI*)(int,int,int,int,int,int,int,int,unsigned,unsigned);
        void *volatile entry=reinterpret_cast<void*>(&EglDrawFixture::blitFramebuffer<4>);
        reinterpret_cast<Function>(entry)(sx0,sy0,sx1,sy1,dx0,dy0,dx1,dy1,mask,filter);
    }
}

API LONG bsiFixtureGlQueries(){return EglDrawFixture::stateQueries;}
API void bsiFixtureImageCopy(unsigned alias,unsigned invalid){
    unsigned src=70,st=0x8d41;int sl=0,sx=1,sy=2,sz=0;constexpr unsigned dst=50,dt=0x0de1;constexpr int dl=0,dx=3,dy=4,dz=0,height=56,depth=1;int width=57;
    if(invalid==1)st=0x8513;if(invalid==2)sl=1;if(invalid==3)width=65;if(invalid==4)src=0;
    EglDrawFixture::expectedTransferHash=EglDrawFixture::hash(102,alias,src,st,sl,sx,sy,sz,dst,dt,dl,dx,dy,dz,width,height,depth);
    using Function=void(WINAPI*)(unsigned,unsigned,int,int,int,int,unsigned,unsigned,int,int,int,int,int,int,int);
    void *volatile entry=alias?reinterpret_cast<void*>(&EglDrawFixture::copyImage<1>):reinterpret_cast<void*>(&EglDrawFixture::copyImage<0>);
    reinterpret_cast<Function>(entry)(src,st,sl,sx,sy,sz,dst,dt,dl,dx,dy,dz,width,height,depth);
}
