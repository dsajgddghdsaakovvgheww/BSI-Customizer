#pragma once
// Public EGL dispatch bookkeeping only. No addresses, handles or equal argument
// values constitute context/resource ownership or a private provider ABI.
#include <array>
#include <cstdint>

namespace BsiShaders {
enum class EglDispatchOp : unsigned {
    WindowSurface,PlatformSurface,PlatformSurfaceExt,Context,Pbuffer,
    DestroySurface,DestroyContext,Swap,BindTexture,Lookup,Count
};
inline constexpr std::array<const char*,static_cast<unsigned>(EglDispatchOp::Count)> eglDispatchNames{{
    "eglCreateWindowSurface","eglCreatePlatformWindowSurface","eglCreatePlatformWindowSurfaceEXT",
    "eglCreateContext","eglCreatePbufferSurface","eglDestroySurface","eglDestroyContext",
    "eglSwapBuffers","eglBindTexImage","eglGetProcAddress"
}};
class EglDispatchScope {
    inline static thread_local EglDispatchScope *active=nullptr;
    EglDispatchScope *previous=active;
    const EglDispatchOp operation;
    const std::array<std::uintptr_t,4> arguments;
    const unsigned candidates;
    unsigned selected=0;
    bool repeated=false,overflow=false;
public:
    EglDispatchScope(EglDispatchOp op,unsigned providers,std::array<std::uintptr_t,4> args)noexcept
        :operation(op),arguments(args),candidates(providers){
        // An import shim's trampoline can enter a separately hooked resolver
        // implementation. Identical operation/arguments + overlapping provider
        // candidates are ONE observation, not another generation/share/swap.
        unsigned budget=32;auto *parent=previous;
        for(;parent&&budget;parent=parent->previous,--budget)
            if(parent->operation==op&&parent->arguments==args&&(parent->candidates&providers)){
                repeated=true;break;
            }
        overflow=parent&&!budget;
        // Shared code is not proof that two providers share a resource space.
        // Ambiguous direct calls may retire candidate evidence, NEVER acquire
        // ownership or publish a share/image link. An outer public shim can
        // still provide its one exact provider; the nested body is deduplicated.
        if(!repeated&&!overflow&&providers&&!(providers&(providers-1))){
            unsigned bit=providers;while((bit>>=1)!=0)++selected;
            // A lone WGL bit leaves selected == 0, which observes() refuses.
        }
        active=this;
    }
    ~EglDispatchScope(){active=previous;}
    EglDispatchScope(const EglDispatchScope&)=delete;
    EglDispatchScope &operator=(const EglDispatchScope&)=delete;
    unsigned provider()const noexcept{return selected;}
    bool observes()const noexcept{return selected!=0;}
    // Ambiguous SUCCESSFUL teardown may conservatively revoke all candidates;
    // it cannot publish/transfer any ownership to them. Nested aliases do none.
    unsigned retirementCandidates()const noexcept{return repeated?0:candidates;}
};
class EglInternalLookup {
    inline static thread_local unsigned depth=0;
public:
    EglInternalLookup()noexcept{++depth;}
    ~EglInternalLookup(){--depth;}
    static bool active()noexcept{return depth!=0;}
    EglInternalLookup(const EglInternalLookup&)=delete;
    EglInternalLookup &operator=(const EglInternalLookup&)=delete;
};
} // namespace BsiShaders
