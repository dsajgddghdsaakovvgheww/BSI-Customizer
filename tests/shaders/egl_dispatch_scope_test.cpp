#include "EglDispatchScope.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>
using namespace BsiShaders;
namespace {
void require(bool ok,const char *message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
}
int main(){
    constexpr std::array<std::uintptr_t,4> args{{1,2,3,4}},other{{1,2,3,5}};
    {
        EglDispatchScope publicShim(EglDispatchOp::WindowSurface,2,args);
        require(publicShim.observes()&&publicShim.provider()==1,"one public provider is exact code attribution only");
        {
            EglDispatchScope implementation(EglDispatchOp::WindowSurface,2,args);
            require(!implementation.observes()&&!implementation.retirementCandidates(),"same-provider same-call import/body nesting deduplicated");
        }
        {
            EglDispatchScope sharedImplementation(EglDispatchOp::WindowSurface,6,args);
            require(!sharedImplementation.observes(),"overlapping shared implementation cannot duplicate a known public observation");
        }
        {
            EglDispatchScope independent(EglDispatchOp::WindowSurface,4,args);
            require(independent.provider()==2,"disjoint provider does not inherit outer namespace even with equal handle values");
        }
        {
            EglDispatchScope distinctArgs(EglDispatchOp::WindowSurface,2,other);
            require(distinctArgs.provider()==1,"truly separate native call remains observable");
            EglDispatchScope distinctContract(EglDispatchOp::PlatformSurface,2,other);
            require(distinctContract.provider()==1,"pointer-width contracts never folded together");
        }
    }
    {
        EglDispatchScope directShared(EglDispatchOp::Context,6,args);
        require(!directShared.observes()&&directShared.retirementCandidates()==6,"ambiguous code cannot create ownership/sharing; successful retirement can only revoke");
        EglDispatchScope duplicate(EglDispatchOp::Context,2,args);
        require(!duplicate.observes()&&!duplicate.retirementCandidates(),"nested ambiguity is not promoted to new resource proof");
    }
    {
        EglDispatchScope invalid(EglDispatchOp::Swap,1,args);
        require(!invalid.observes(),"WGL bit zero cannot be reinterpreted as EGL");
    }
    {
        std::vector<std::unique_ptr<EglDispatchScope>> nested;
        for(unsigned i=0;i<33;++i)nested.push_back(std::make_unique<EglDispatchScope>(EglDispatchOp::Context,2,std::array<std::uintptr_t,4>{{i,0,0,0}}));
        {
            EglDispatchScope overflow(EglDispatchOp::Swap,2,args);
            require(!overflow.observes(),"bounded nesting overflow stays native, not a guessed attribution");
        }
        // Restore the actual LIFO dispatch stack, as native callbacks do.
        while(!nested.empty())nested.pop_back();
    }
    {
        EglDispatchScope restored(EglDispatchOp::Swap,2,args);
        require(restored.provider()==1,"scope exit restores attribution after nested overflow");
        bool isolated=false;std::thread worker([&]{EglDispatchScope own(EglDispatchOp::Swap,4,args);isolated=own.provider()==2;});worker.join();
        require(isolated,"dispatch attribution is native-thread-local");
        require(!EglInternalLookup::active(),"no internal lookup outside discovery");
        {EglInternalLookup outer;require(EglInternalLookup::active(),"internal lookup suppresses recursive application observation");{EglInternalLookup inner;require(EglInternalLookup::active(),"nested internal lookup remains suppressed");}require(EglInternalLookup::active(),"nested exit retains outer lookup guard");}
        require(!EglInternalLookup::active(),"lookup exit restores normal application observation");
    }
    std::puts("PASS: bounded exact EGL alias nesting, provider ambiguity/negative retirement, contract/argument/thread separation and internal lookup recursion; no resource ownership inferred");
}
