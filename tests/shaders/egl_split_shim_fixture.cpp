// Authored import-jump shim, NOT libEGL.dll and never installed as a proxy.
#include "egl_split_fixture_api.h"
#define BSI_SPLIT_IMPORT(Result,Name,Params,Args) extern "C" __declspec(dllimport) Result WINAPI bsiImpl_##Name Params;
BSI_EGL_SPLIT_APIS(BSI_SPLIT_IMPORT)
#undef BSI_SPLIT_IMPORT
#define BSI_SPLIT_EXPORT(Result,Name,Params,Args) extern "C" __declspec(dllexport) __declspec(noinline) Result WINAPI Name Params {return bsiImpl_##Name Args;}
BSI_EGL_SPLIT_APIS(BSI_SPLIT_EXPORT)
#undef BSI_SPLIT_EXPORT
