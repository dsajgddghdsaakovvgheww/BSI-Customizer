#pragma once
// Authored split EGL loader contract, NOT vendor code or a driver ABI probe.
#include <windows.h>
#include <cstdint>

// Each public function in the shim calls a dllimport in the separate backend.
// The resolver returns backend implementations, never the shim's public stubs.
#define BSI_EGL_SPLIT_APIS(X) \
    X(void*, eglGetCurrentContext, (), ()) \
    X(void*, eglGetCurrentDisplay, (), ()) \
    X(void*, eglGetCurrentSurface, (int which), (which)) \
    X(unsigned, eglQueryAPI, (), ()) \
    X(const char*, eglQueryString, (void *display,int name), (display,name)) \
    X(void*, eglCreateWindowSurface, (void *display,void *config,void *window,const int *attrs), (display,config,window,attrs)) \
    X(void*, eglCreatePlatformWindowSurface, (void *display,void *config,void *window,const std::intptr_t *attrs), (display,config,window,attrs)) \
    X(void*, eglCreatePlatformWindowSurfaceEXT, (void *display,void *config,void *window,const int *attrs), (display,config,window,attrs)) \
    X(void*, eglCreateContext, (void *display,void *config,void *share,const int *attrs), (display,config,share,attrs)) \
    X(void*, eglCreatePbufferSurface, (void *display,void *config,const int *attrs), (display,config,attrs)) \
    X(unsigned, eglDestroySurface, (void *display,void *surface), (display,surface)) \
    X(unsigned, eglDestroyContext, (void *display,void *context), (display,context)) \
    X(unsigned, eglQuerySurface, (void *display,void *surface,int name,int *out), (display,surface,name,out)) \
    X(unsigned, eglQueryContext, (void *display,void *context,int name,int *out), (display,context,name,out)) \
    X(void*, eglGetProcAddress, (const char *name), (name)) \
    X(unsigned, eglSwapBuffers, (void *display,void *surface), (display,surface)) \
    X(unsigned, eglBindTexImage, (void *display,void *surface,int buffer), (display,surface,buffer)) \
    X(unsigned, eglMakeCurrent, (void *display,void *draw,void *read,void *context), (display,draw,read,context))

namespace EglSplitFixture {
enum Entry {
#define BSI_SPLIT_ENUM(Result,Name,Params,Args) Name,
    BSI_EGL_SPLIT_APIS(BSI_SPLIT_ENUM)
#undef BSI_SPLIT_ENUM
    Count
};
inline DWORD nativeError(unsigned entry,bool failed=false){return 0x71dd0000u+entry+(failed?0xf000u:0u);}
}
