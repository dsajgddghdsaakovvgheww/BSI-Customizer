// Authored EGL-shaped implementation + GL state double. No driver/pixels run.
#include "egl_split_fixture_api.h"
#include <array>
#include <cstring>
#include <type_traits>

#define BSI_SPLIT_DECLARE(Result,Name,Params,Args) extern "C" __declspec(dllexport) Result WINAPI bsiImpl_##Name Params;
BSI_EGL_SPLIT_APIS(BSI_SPLIT_DECLARE)
#undef BSI_SPLIT_DECLARE
namespace {
unsigned splitCreateVariant=0;
void *variedCreateProcedure(unsigned variant);
void *splitProcedure(const char *name){
    if(splitCreateVariant&&std::strcmp(name,"eglCreateWindowSurface")==0)return variedCreateProcedure(splitCreateVariant);
#define BSI_SPLIT_RESOLVE(Result,Name,Params,Args) if(std::strcmp(name,#Name)==0)return reinterpret_cast<void*>(&bsiImpl_##Name);
    BSI_EGL_SPLIT_APIS(BSI_SPLIT_RESOLVE)
#undef BSI_SPLIT_RESOLVE
    return nullptr;
}
}
// Reuse the existing independent native state model, without any public EGL
// exports in this backend: the production scanner must discover the SHIM.
#define eglGetCurrentContext bsiRaw_eglGetCurrentContext
#define eglGetCurrentDisplay bsiRaw_eglGetCurrentDisplay
#define eglGetCurrentSurface bsiRaw_eglGetCurrentSurface
#define eglQueryAPI bsiRaw_eglQueryAPI
#define eglQueryString bsiRaw_eglQueryString
#define eglCreateWindowSurface bsiRaw_eglCreateWindowSurface
#define eglCreatePlatformWindowSurface bsiRaw_eglCreatePlatformWindowSurface
#define eglCreatePlatformWindowSurfaceEXT bsiRaw_eglCreatePlatformWindowSurfaceEXT
#define eglCreateContext bsiRaw_eglCreateContext
#define eglCreatePbufferSurface bsiRaw_eglCreatePbufferSurface
#define eglDestroySurface bsiRaw_eglDestroySurface
#define eglDestroyContext bsiRaw_eglDestroyContext
#define eglQuerySurface bsiRaw_eglQuerySurface
#define eglQueryContext bsiRaw_eglQueryContext
#define eglGetProcAddress bsiRaw_eglGetProcAddress
#define eglSwapBuffers bsiRaw_eglSwapBuffers
#define eglBindTexImage bsiRaw_eglBindTexImage
#define eglMakeCurrent bsiRaw_eglMakeCurrent
#define BSI_EGL_FIXTURE_EXTRA_PROCEDURE(name) splitProcedure(name)
#include "egl_loader_fixture.cpp"
#undef BSI_EGL_FIXTURE_EXTRA_PROCEDURE
#undef eglGetCurrentContext
#undef eglGetCurrentDisplay
#undef eglGetCurrentSurface
#undef eglQueryAPI
#undef eglQueryString
#undef eglCreateWindowSurface
#undef eglCreatePlatformWindowSurface
#undef eglCreatePlatformWindowSurfaceEXT
#undef eglCreateContext
#undef eglCreatePbufferSurface
#undef eglDestroySurface
#undef eglDestroyContext
#undef eglQuerySurface
#undef eglQueryContext
#undef eglGetProcAddress
#undef eglSwapBuffers
#undef eglBindTexImage
#undef eglMakeCurrent

namespace {
struct NativeCall {
    std::array<std::uintptr_t,4> arguments{};
    std::uintptr_t result=0;
    DWORD incomingError=0;
};
thread_local std::array<NativeCall,EglSplitFixture::Count> nativeCalls{};
std::array<volatile LONG,EglSplitFixture::Count> nativeCounts{};
std::array<bool,EglSplitFixture::Count> nativeFailures{};
template<typename T>std::uintptr_t value(T item){
    if constexpr(std::is_pointer_v<T>)return reinterpret_cast<std::uintptr_t>(item);
    else return static_cast<std::uintptr_t>(item);
}
template<unsigned Index,typename Function,typename... Args>auto invokeNative(Function function,Args... args){
    const auto incoming=GetLastError();InterlockedIncrement(&nativeCounts[Index]);
    NativeCall record;record.incomingError=incoming;
    const std::array<std::uintptr_t,sizeof...(Args)> arguments{{value(args)...}};
    for(unsigned i=0;i<arguments.size();++i)record.arguments[i]=arguments[i];
    const bool failed=nativeFailures[Index];SetLastError(incoming);
    const auto result=failed?decltype(function()){}:function();record.result=value(result);
    nativeCalls[Index]=record;SetLastError(EglSplitFixture::nativeError(Index,failed));return result;
}
}
// Empty argument lists need no special preprocessor extension in C++17.
// A tuple-free lambda records its arguments through a separate generic caller.
#define BSI_SPLIT_IMPLEMENT(Result,Name,Params,Args) \
    extern "C" __declspec(dllexport) __declspec(noinline) Result WINAPI bsiImpl_##Name Params { \
        const auto call=[&](auto... values){return invokeNative<EglSplitFixture::Name>([&](){return bsiRaw_##Name Args;},values...);}; \
        return call Args; \
    }
BSI_EGL_SPLIT_APIS(BSI_SPLIT_IMPLEMENT)
#undef BSI_SPLIT_IMPLEMENT
namespace {
std::array<volatile LONG,16> variantCalls{};
template<unsigned Variant> __declspec(noinline) void *WINAPI variedCreate(void *d,void *c,void *w,const int *a){
    InterlockedIncrement(&variantCalls[Variant]);return bsiImpl_eglCreateWindowSurface(d,c,w,a);
}
template<std::size_t... Variant>auto createProcedures(std::index_sequence<Variant...>){
    return std::array<void*,sizeof...(Variant)>{{reinterpret_cast<void*>(&variedCreate<Variant>)...}};
}
void *variedCreateProcedure(unsigned variant){
    static const auto entries=createProcedures(std::make_index_sequence<16>{});
    return variant&&variant<=entries.size()?entries[variant-1]:nullptr;
}
}
extern "C" __declspec(dllexport) void bsiSplitCreateVariant(unsigned variant){splitCreateVariant=variant;}
extern "C" __declspec(dllexport) LONG bsiSplitCalls(unsigned entry){return entry<EglSplitFixture::Count?nativeCounts[entry]:-1;}
extern "C" __declspec(dllexport) void bsiSplitFailure(unsigned entry,int fail){if(entry<EglSplitFixture::Count)nativeFailures[entry]=fail!=0;}
extern "C" __declspec(dllexport) std::uintptr_t bsiSplitResult(unsigned entry){return entry<EglSplitFixture::Count?nativeCalls[entry].result:0;}
extern "C" __declspec(dllexport) DWORD bsiSplitIncomingError(unsigned entry){return entry<EglSplitFixture::Count?nativeCalls[entry].incomingError:0;}
extern "C" __declspec(dllexport) int bsiSplitArguments(unsigned entry,const std::uintptr_t *arguments,unsigned count){
    if(entry>=EglSplitFixture::Count||count>4)return 0;
    for(unsigned i=0;i<count;++i)if(nativeCalls[entry].arguments[i]!=arguments[i])return 0;
    return 1;
}
