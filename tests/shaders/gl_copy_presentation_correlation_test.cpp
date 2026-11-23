#include "GlTextureProvenance.h"
#include <cstdio>
#include <cstdlib>
using namespace BsiShaders;
namespace {
void require(bool okay,const char *message){if(!okay){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
}
int main(){
    const GlContextIdentity a{1,10,100},b{1,10,200};
    GlTextureProvenance p;p.synchronize(77,1);p.remember(a);p.remember(b);
    GlTextureUsage root;root.defaultFramebuffer=true;root.reads[0]=GlColorResource(11);root.reads[1]=GlColorResource(99);root.readCount=2;
    p.observe(a,root,true);
    GlTextureUsage copy;copy.reads[0]=GlColorResource(11);copy.readCount=1;copy.writes[0]=GlColorResource(99);copy.writeCount=1;
    p.observe(a,copy,false,nullptr,true);
    auto audit=p.deferredCopyAudit();
    require(audit.presentedInputObservations==2&&audit.presentedInputDistinct==2,"presented-input observations and distinct count");
    require(audit.copyObservations==1&&audit.copyInputRecentPresentedMatches==1&&audit.copyOutputRecentPresentedMatches==1,"same-namespace recent input/output correlations");
    require(audit.copyOutputDistinct==1&&audit.copyOutputFrames==1&&audit.copyOutputMaxFramesPerName==1,"bounded output name and frame counts");
    p.nextFrame();p.observe(a,root,true);p.observe(a,copy,false,nullptr,true);
    audit=p.deferredCopyAudit();
    require(audit.copyOutputFrames==2&&audit.copyOutputMaxFramesPerName==2&&audit.presentedInputDistinct==2,"same output persists across frames");
    p.nextFrame();GlTextureUsage other=root;other.reads[0]=GlColorResource(12);p.observe(a,other,true);
    audit=p.deferredCopyAudit();require(audit.presentedInputDistinct==3,"changing presented input is counted");
    // Equal numeric resource names in a separate, unshared namespace do not match.
    GlTextureUsage foreign;foreign.reads[0]=GlColorResource(11);foreign.readCount=1;foreign.writes[0]=GlColorResource(99);foreign.writeCount=1;
    p.observe(b,foreign,false,nullptr,true);audit=p.deferredCopyAudit();
    require(audit.copyInputRecentPresentedMatches==2&&audit.copyOutputRecentPresentedMatches==2,"unrelated namespace does not match by numeric name");
    p.nextFrame();p.nextFrame();p.nextFrame();p.observe(a,copy,false,nullptr,true);audit=p.deferredCopyAudit();
    require(audit.copyInputRecentPresentedMatches==2&&audit.copyOutputRecentPresentedMatches==2,"recent-match window ends after two provenance frames");
    const auto reverseInputs=audit.presentationInputsRecentCopyInputMatches,reverseOutputs=audit.presentationInputsRecentCopyOutputMatches;
    p.observe(a,root,true);audit=p.deferredCopyAudit();
    require(audit.presentationInputsRecentCopyInputMatches>reverseInputs&&audit.presentationInputsRecentCopyOutputMatches>reverseOutputs,"presentation after copy is correlated within the frame window");
    for(unsigned i=0;i<20;++i){GlTextureUsage many;many.reads[0]=GlColorResource(11);many.readCount=1;many.writes[0]=GlColorResource(200+i);many.writeCount=1;p.observe(a,many,false,nullptr,true);}
    audit=p.deferredCopyAudit();require(audit.copyOutputDistinct==GlTextureProvenance::copyOutputCapacity&&audit.copyOutputUntracked>0,"copy-output name tracking is bounded and reports overflow");
    GlTextureUsage manyInputs;manyInputs.defaultFramebuffer=true;manyInputs.readCount=GlTextureUsage::capacity;
    for(std::size_t i=0;i<manyInputs.readCount;++i)manyInputs.reads[i]=GlColorResource(300+static_cast<unsigned>(i));
    p.observe(a,manyInputs,true);audit=p.deferredCopyAudit();
    require(audit.presentedInputDistinct==GlTextureProvenance::correlationCapacity&&audit.presentedInputUntracked>0,"presented-input name tracking is bounded and reports overflow");
    std::puts("PASS: bounded, same-namespace copy/presentation correlation, stability, order, and expiry");
}
