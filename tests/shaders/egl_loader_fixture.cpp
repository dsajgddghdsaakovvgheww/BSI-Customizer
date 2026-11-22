// Authored EGL lifecycle double, NOT an EGL/GL driver. No GPU calls execute.
// Two differently named DLLs exercise the production provider/HWND detours.
#include <windows.h>
#include <cstdint>
#include "egl_gl_fixture.h"

#define API extern "C" __declspec(dllexport)
namespace {
struct Surface { HWND window=nullptr;bool protectedContent=false; } surfaces[32];
struct Context { bool protectedContent=false;unsigned variant=0;EglDrawFixture::State gl; } contexts[16];
unsigned surfaceCount=0,contextCount=0;
int displayToken=0;
thread_local Surface *currentSurface=nullptr,*currentReadSurface=nullptr;
thread_local Context *currentContext=nullptr;
volatile LONG calls=0,glQueries=0,protectionQueries=0,illegalQueries=0,housekeeping=0;
bool extensions=true;unsigned bindingFailures=0;
volatile LONG imageBinds=0,makeCalls=0;
bool failMake=false;void *lastMakeDisplay=nullptr,*lastMakeDraw=nullptr,*lastMakeRead=nullptr,*lastMakeContext=nullptr;
template<typename Attribute>void *create(void *window,const Attribute *attrs){
    if(!IsWindow(reinterpret_cast<HWND>(window))||surfaceCount==32)return nullptr;
    auto &surface=surfaces[surfaceCount++];surface.window=reinterpret_cast<HWND>(window);
    if(attrs)for(unsigned i=0;i<128&&attrs[i]!=0x3038;i+=2)if(attrs[i]==0x32c0)surface.protectedContent=attrs[i+1]!=0;
    return &surface;
}
void *pbuffer(const int *attributes){
    if(surfaceCount==32)return nullptr;
    auto &surface=surfaces[surfaceCount++];surface.window=nullptr;
    if(attributes)for(unsigned i=0;i<128&&attributes[i]!=0x3038;i+=2)
        if(attributes[i]==0x32c0)surface.protectedContent=attributes[i+1]!=0;
    return &surface;
}
void *context(bool protectedContent){
    if(contextCount==16)return nullptr;
    auto &result=contexts[contextCount++];result.protectedContent=protectedContent;return &result;
}
}
namespace EglDrawFixture {
State &state(){static State unbound;return currentContext?currentContext->gl:unbound;}
bool protectedContext(){return currentContext&&currentContext->protectedContent;}
}
API void *WINAPI eglGetCurrentContext(){return currentContext;}
API void *WINAPI eglGetCurrentDisplay(){return currentContext?&displayToken:nullptr;}
API void *WINAPI eglGetCurrentSurface(int which){return which==0x305a?currentReadSurface:currentSurface;}
API unsigned WINAPI eglQueryAPI(){return 0x30a0;} // actual GLES, irrespective of configuration hint
API const char *WINAPI eglQueryString(void*,int){return extensions?"EGL_EXT_protected_content EGL_EXT_protected_surface":"";}
API void *WINAPI eglCreateWindowSurface(void*,void*,void *window,const int *attrs){InterlockedExchangeAdd(&housekeeping,1);return create(window,attrs);}
API void *WINAPI eglCreatePlatformWindowSurface(void*,void*,void *window,const std::intptr_t *attrs){InterlockedExchangeAdd(&housekeeping,2);return create(window,attrs);}
API void *WINAPI eglCreatePlatformWindowSurfaceEXT(void*,void*,void *window,const int *attrs){InterlockedExchangeAdd(&housekeeping,3);return create(window,attrs);}
API void *WINAPI eglCreateContext(void*,void*,void*,const int *attributes){
    bool protectedContent=false;
    if(attributes)for(unsigned i=0;i<128&&attributes[i]!=0x3038;i+=2)
        if(attributes[i]==0x32c0)protectedContent=attributes[i+1]!=0;
    return context(protectedContent);
}
API void *WINAPI eglCreatePbufferSurface(void*,void*,const int *attributes){return pbuffer(attributes);}
API unsigned WINAPI eglDestroySurface(void*,void *value){
    InterlockedExchangeAdd(&housekeeping,4);if(currentSurface==value)currentSurface=nullptr;if(currentReadSurface==value)currentReadSurface=nullptr;return value!=nullptr;
}
API unsigned WINAPI eglDestroyContext(void*,void *value){
    InterlockedExchangeAdd(&housekeeping,5);if(currentContext==value){currentContext=nullptr;currentSurface=currentReadSurface=nullptr;}return value!=nullptr;
}
API unsigned WINAPI eglQuerySurface(void*,void *value,int name,int *out){
    if(!value){InterlockedIncrement(&illegalQueries);return 0;} // EGL_NO_SURFACE is NEVER queryable
    if(!out)return 0;const auto &surface=*static_cast<Surface*>(value);
    if(name==0x32c0){InterlockedIncrement(&protectionQueries);if(!extensions)InterlockedIncrement(&illegalQueries);*out=surface.protectedContent;return 1;}
    if(!surface.window){
        if(bindingFailures&2)return 0;
        if(name==0x3081){*out=bindingFailures&4?0x305c:0x305f;return 1;}
        if(name==0x3080){*out=bindingFailures&8?0x305c:0x305e;return 1;}
        if(name==0x3083){*out=bindingFailures&16?1:0;return 1;}
    }
    if(name==0x3057){*out=640;return 1;}if(name==0x3056){*out=360;return 1;}return 0;
}
API unsigned WINAPI eglQueryContext(void*,void *value,int name,int *out){
    if(!value||!out||name!=0x32c0)return 0;InterlockedIncrement(&protectionQueries);
    if(!extensions)InterlockedIncrement(&illegalQueries);*out=static_cast<Context*>(value)->protectedContent;return 1;
}
API void *WINAPI eglGetProcAddress(const char *name){
    InterlockedIncrement(&glQueries);SetLastError(0x13579b);
    if(!name)return nullptr;
#ifdef BSI_EGL_FIXTURE_EXTRA_PROCEDURE
    if(const auto entry=BSI_EGL_FIXTURE_EXTRA_PROCEDURE(name))return entry;
#endif
    if(currentContext&&currentContext->variant&&std::strcmp(name,"glDrawElements")==0){
        if(currentContext->variant==1)return reinterpret_cast<void*>(&EglDrawFixture::drawElements<BsiShaders::glDrawAliases>);
        if(currentContext->variant==2)return reinterpret_cast<void*>(&EglDrawFixture::drawElements<BsiShaders::glDrawAliases+1>);
        return reinterpret_cast<void*>(&EglDrawFixture::drawElements<BsiShaders::glDrawAliases+2>);
    }
    return EglDrawFixture::resolve(name);
}
API unsigned WINAPI eglSwapBuffers(void*,void*){InterlockedIncrement(&calls);return 1;}
API unsigned WINAPI eglBindTexImage(void *display,void *surface,int buffer){
    InterlockedIncrement(&imageBinds);
    if(display!=&displayToken||!surface||static_cast<Surface*>(surface)->window||buffer!=0x3084||(bindingFailures&1))return 0;
    if(EglDrawFixture::onTransfer)EglDrawFixture::onTransfer();
    return 1;
}
API unsigned WINAPI eglMakeCurrent(void *display,void *draw,void *read,void *context){
    InterlockedIncrement(&makeCalls);lastMakeDisplay=display;lastMakeDraw=draw;lastMakeRead=read;lastMakeContext=context;
    if(failMake){SetLastError(0x4422f0);return 0;}
    currentSurface=static_cast<Surface*>(draw);currentReadSurface=static_cast<Surface*>(read);currentContext=static_cast<Context*>(context);SetLastError(0x442201);return 1;
}
API void *bsiFixtureDisplay(){return &displayToken;}
API void *bsiFixtureContext(int protectedContent){return context(protectedContent!=0);}
API void *bsiFixtureUnknownPbuffer(){return pbuffer(nullptr);}
API LONG bsiFixtureSwapCalls(){return calls;}
API LONG bsiFixtureGlQueries(){return EglDrawFixture::stateQueries;}
API LONG bsiFixtureProcedureQueries(){return glQueries;}
API LONG bsiFixtureProtectionQueries(){return protectionQueries;}
API LONG bsiFixtureIllegalQueries(){return illegalQueries;}
API void bsiFixtureExtensions(int available){extensions=available!=0;InterlockedExchange(&protectionQueries,0);}
API void bsiFixtureDraw(unsigned kind,unsigned alias,unsigned mode){EglDrawFixture::invoke(kind,alias,mode);}
API void bsiFixtureNestedDraw(unsigned mode){
    using namespace EglDrawFixture;
    nestedMode=mode;nestedPrivateBindings=0;nestedArgumentsIntact=true;
    invoke(static_cast<U>(BsiShaders::GlDrawKind::Arrays),0,4);nestedMode=0;
}
API LONG bsiFixtureNestedPrivateBindings(){return LONG(EglDrawFixture::nestedPrivateBindings);}
API LONG bsiFixtureDrawCalls(){return EglDrawFixture::draws;}
API LONG bsiFixturePrivateDraws(){return EglDrawFixture::privateDraws;}
API LONG bsiFixtureTextureDeletes(){return EglDrawFixture::textureDeletes;}
API void bsiFixtureResourceState(unsigned mode,unsigned output){
    auto &g=EglDrawFixture::state();g.readFramebuffer=g.framebuffer=(mode==2||mode==3)?7:0;g.output=output;
    g.outputType=mode==3?0x8d41:0x1702;
    g.textures=mode==1?std::array<unsigned,4>{{output,0,0,0}}:std::array<unsigned,4>{{1,2,0,0}};
    g.samplers[1].min=mode==1?0x2601:0x2703; // legal LINEAR consumer, existing trilinear material source otherwise
    g.mips=mode!=1;g.depth=false;g.depthWrite=false;g.depthBits=0;
    g.blend=g.protectedTexture=false;g.colorWrite=true;
}
API void bsiFixtureGlProtection(int available){EglDrawFixture::state().protectionExtension=available!=0;}
API void bsiFixtureTransferState(unsigned readKind,unsigned source,unsigned destination){
    auto &g=EglDrawFixture::state();g.readFramebuffer=readKind?8:0;g.framebuffer=7;
    g.readOutput=source;g.readOutputType=readKind==2?0x8d41:0x1702;g.output=destination;g.outputType=0x1702;
    g.textures[3]=destination;g.readComplete=g.drawComplete=true;g.colorWrite=false;g.blend=true;
}
API void bsiFixtureTransfer(unsigned copy,unsigned alias){EglDrawFixture::invokeTransfer(copy!=0,alias);}
API LONG bsiFixtureTransferCalls(){return EglDrawFixture::transfers;}
API LONG bsiFixtureTransferArgumentsIntact(){return EglDrawFixture::lastTransferHash==EglDrawFixture::expectedTransferHash;}
API void bsiFixtureOnTransfer(void(WINAPI *callback)()){EglDrawFixture::onTransfer=callback;}
API void bsiFixtureExpectedFloor(float value){EglDrawFixture::expectedMinimumLod=value;}
API LONG bsiFixtureInvalidGl(){return EglDrawFixture::invalid;}
API LONG bsiFixtureLastPrivateBindings(){return LONG(EglDrawFixture::lastPrivateBindings);}
API LONG bsiFixtureArgumentsIntact(){return EglDrawFixture::nestedArgumentsIntact&&EglDrawFixture::lastHash==EglDrawFixture::expectedHash;}
API LONG bsiFixtureBindingsRestored(){return EglDrawFixture::restored();}
API void bsiFixtureSceneState(unsigned flags){
    auto &g=EglDrawFixture::state();g.depth=!(flags&1);g.depthWrite=!(flags&2);
    g.depthFunction=flags&4?0x0207:0x0203;g.depthBits=flags&8?0:24;
    g.blend=(flags&16)!=0;g.mips=!(flags&32);g.protectedTexture=(flags&64)!=0;
}

API LONG bsiFixtureImageBinds(){return imageBinds;}
API void bsiFixtureImageBindingFailures(unsigned flags){bindingFailures=flags;}
API void bsiFixtureNativeTextureTarget(unsigned name){EglDrawFixture::state().textures[3]=name;}

API void bsiFixtureProcedureVariant(unsigned variant){if(currentContext)currentContext->variant=variant;}
API void bsiFixtureMakeFailure(int fail){failMake=fail!=0;}
API LONG bsiFixtureMakeCalls(){return makeCalls;}
API LONG bsiFixtureMakeArguments(void *d,void *draw,void *read,void *c){return d==lastMakeDisplay&&draw==lastMakeDraw&&read==lastMakeRead&&c==lastMakeContext;}
API void bsiFixtureCachedDraw(void *entry,unsigned variant){
    constexpr unsigned mode=4,kind=static_cast<unsigned>(BsiShaders::GlDrawKind::Elements),type=0x1403;
    constexpr int count=13;const auto *indices=reinterpret_cast<void*>(0x123400);
    EglDrawFixture::expectedHash=EglDrawFixture::hash(kind,BsiShaders::glDrawAliases+variant-1,mode,count,type,indices);
    reinterpret_cast<void(WINAPI*)(unsigned,int,unsigned,const void*)>(entry)(mode,count,type,indices);
}
