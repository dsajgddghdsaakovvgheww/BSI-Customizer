// Metadata accounting only: no GL context, player, driver success or pixels.
#include "GlTextureProvenance.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
using namespace BsiShaders;
namespace {
void require(bool yes,const char *why){if(!yes){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
GlTextureUsage usage(bool selected,std::initializer_list<GlColorResource> reads,std::initializer_list<GlColorResource> writes={}){
    GlTextureUsage u;u.defaultFramebuffer=selected;
    for(auto r:reads)u.reads[u.readCount++]=r;
    for(auto r:writes)u.writes[u.writeCount++]=r;
    return u;
}
constexpr GlContextIdentity presenter{1,10,100},isolated{1,10,200};
constexpr auto front=GlColorResource::pbuffer(800,0x0400);
auto index(GlDeferredCopyExit x){return static_cast<std::size_t>(x);}
auto index(GlDeferredConsumerStop x){return static_cast<std::size_t>(x);}
auto index(GlDeferredDiscardSite x){return static_cast<std::size_t>(x);}
void balanced(const GlTextureProvenance &p){require(p.deferredCopyAudit().valid(),"pending gauges/source/exit/site accounting must balance after every mutation");}
void onlyExit(const GlTextureProvenance &p,GlDeferredCopyExit reason){
    balanced(p);const auto &a=p.deferredCopyAudit();
    require(a.queued==1&&!a.pending&&a.peak==1&&a.exits[index(reason)]==1,"one queued edge must retire once for the observed reason");
}
}
int main(){
    const auto root=usage(true,{50}),copy=usage(false,{front},{51}),producer=usage(true,{1},{front});
    {
        GlTextureProvenance p;GlDeferredCopyAudit snapshot;std::uint64_t revision=99;
        require(!p.takeDeferredCopyAudit(&snapshot,&revision),"an untouched graph has no fabricated audit interval");
        p.synchronize(123,7);
        require(p.takeDeferredCopyAudit(&snapshot,&revision)&&revision==7&&snapshot.valid()&&!snapshot.queued,"source revision publishes one empty fresh snapshot");
        require(!p.takeDeferredCopyAudit(&snapshot,&revision),"reading unchanged diagnostics cannot refresh a graph or count another event");
        p.observe(presenter,root,true);
        require(p.deferCopy(presenter,copy)==GlDeferredCopyResult::Queued&&!p.producer(presenter,producer),"queuing metadata alone never adopts its source");
        balanced(p);auto a=p.deferredCopyAudit();
        require(a.queued==1&&a.pending==1&&a.peak==1&&a.sources[2]==1,"native-pbuffer source and live pending entry are distinct from resolution");
        p.observe(presenter,root,true);
        require(p.deferredCopyAudit().consumers[index(GlDeferredConsumerStop::NoDestination)]==1,"same namespace, wrong exact destination is observable without querying anything");
        p.observe(isolated,usage(true,{51}),true);
        require(p.deferredCopyAudit().consumers[index(GlDeferredConsumerStop::OtherNamespace)]==1&&!p.producer(presenter,producer),"other-namespace consumer category never compares numeric names or merges ownership");
        GlDeferredCopyResolution resolved;p.observe(presenter,usage(true,{51}),true,&resolved);
        onlyExit(p,GlDeferredCopyExit::Reconciled);
        require(resolved.copies==1&&p.deferredCopyAudit().consumers[index(GlDeferredConsumerStop::ExactDestination)]==1
                &&p.deferredCopyAudit().consumers[index(GlDeferredConsumerStop::NoPending)]==1,"only exact subsequent same-space consumption reconciles once");
        require(p.takeDeferredCopyAudit(&snapshot,&revision)&&snapshot.queued==1&&snapshot.valid()&&revision==7,"published audit is one cumulative snapshot, not an additive delta");
        require(!p.takeDeferredCopyAudit(&snapshot,&revision),"snapshot readback cannot replay the last copy retirement");
        p.synchronize(123,8);a=p.deferredCopyAudit();
        require(a.valid()&&!a.queued&&!a.pending&&!a.peak&&a.exits==decltype(a.exits){}&&a.consumers==decltype(a.consumers){},"source/settings change clears the WHOLE old audit, including its gauges and probes");
        require(p.takeDeferredCopyAudit(&snapshot,&revision)&&revision==8&&snapshot.sequence==1,"a reset frame clock cannot publish an old interval's audit");
    }
    {
        GlTextureProvenance p;unsigned revision=0;
        const auto fresh=[&]{p.synchronize(123,++revision);p.observe(presenter,root,true);require(p.deferCopy(presenter,copy)==GlDeferredCopyResult::Queued,"queue fresh control");balanced(p);};
        fresh();for(unsigned i=0;i<3;++i)p.nextFrame();onlyExit(p,GlDeferredCopyExit::Expired);
        p.observe(presenter,usage(true,{51}),true);require(!p.producer(presenter,producer),"accounting does not preserve an expired edge for a late consumer");
        fresh();const auto write=usage(false,{1},{51});p.discardPendingWrites(presenter,&write);onlyExit(p,GlDeferredCopyExit::DestinationWrite);
        fresh();p.discardPendingWrites(presenter,nullptr,GlDeferredDiscardSite::DrawWrites);onlyExit(p,GlDeferredCopyExit::UnknownWrite);
        require(p.deferredCopyAudit().discardSites[index(GlDeferredDiscardSite::DrawWrites)]==1,"retired-edge sites identify which existing capture path lacked a safe output");
        fresh();auto invalid=write;invalid.writeCount=33;p.discardPendingWrites(presenter,&invalid,static_cast<GlDeferredDiscardSite>(999));onlyExit(p,GlDeferredCopyExit::UnknownWrite);
        require(p.deferredCopyAudit().discardSites[0]==1,"invalid diagnostic indices remain bounded WITHOUT preventing the existing conservative discard");
        fresh();p.discardPendingProvider(2);require(p.deferredCopyAudit().pending==1,"another provider's revocation must not affect the pending edge");
        p.discardPendingProvider(1);onlyExit(p,GlDeferredCopyExit::MissedCallback);
        fresh();p.forget(presenter);onlyExit(p,GlDeferredCopyExit::ContextRetired);
        fresh();p.forgetProvider(1);onlyExit(p,GlDeferredCopyExit::ProviderRetired);
        fresh();unsigned texture=51;p.deleted(1,1,&texture);onlyExit(p,GlDeferredCopyExit::ResourceRetired);
        fresh();p.retiredPbuffer(1,800);onlyExit(p,GlDeferredCopyExit::NativeSourceRetired);
        fresh();require(p.shared(presenter,isolated),"observed native sharing control");onlyExit(p,GlDeferredCopyExit::ShareChanged);
        fresh();p.clearEvidence();onlyExit(p,GlDeferredCopyExit::EvidenceReset);
        fresh();p.clearEvidence(static_cast<GlDeferredCopyExit>(999));onlyExit(p,GlDeferredCopyExit::EvidenceReset);
        fresh();p.deferCopy(presenter,usage(false,{GlColorResource::pbuffer(801,0x0400)},{51}));balanced(p);
        auto a=p.deferredCopyAudit();require(a.queued==2&&a.pending==1&&a.peak==1&&a.exits[index(GlDeferredCopyExit::Superseded)]==1,"a newer queued copy retires the overwritten predecessor, not the occupied slot twice");
        p.observe(presenter,usage(true,{51}),true);balanced(p);a=p.deferredCopyAudit();
        require(a.queued==2&&!a.pending&&a.exits[index(GlDeferredCopyExit::Reconciled)]==1,"one superseded plus one resolved entry balances two submitted metadata records");
    }
    {
        GlTextureProvenance p;unsigned revision=100;
        const auto fresh=[&]{p.synchronize(123,++revision);p.observe(presenter,root,true);require(p.deferCopy(presenter,copy)==GlDeferredCopyResult::Queued,"queue draw-footprint control");};
        GlDrawWrites unrelated;unrelated.writes[unrelated.writeCount++]=52;
        fresh();p.discardPendingDrawWrites(presenter,&unrelated);
        require(p.deferredCopyAudit().pending==1&&!p.producer(presenter,producer),"an unrelated known raster write retains queued metadata without adopting a producer");
        p.observe(presenter,usage(true,{51}),true);onlyExit(p,GlDeferredCopyExit::Reconciled);
        GlDrawWrites alias;alias.writes[alias.writeCount++]=51;
        fresh();p.discardPendingDrawWrites(presenter,&alias);onlyExit(p,GlDeferredCopyExit::DestinationWrite);
        GlDrawWrites rbo;rbo.writes[rbo.writeCount++]=GlColorResource::renderbuffer(51);
        fresh();p.discardPendingDrawWrites(presenter,&rbo);
        require(p.deferredCopyAudit().pending==1,"a same-number renderbuffer is not a texture destination alias");
        require(p.remember(isolated),"known independent context space");p.discardPendingDrawWrites(isolated,&alias);
        require(p.deferredCopyAudit().pending==1,"equal-name writes in an isolated space cannot retire another namespace's queued destination");
        for(unsigned i=0;i<3;++i){p.discardPendingDrawWrites(presenter,&unrelated);p.nextFrame();}
        onlyExit(p,GlDeferredCopyExit::Expired);
        fresh();auto invalid=alias;invalid.writeCount=GlDrawWrites::capacity+1;
        p.discardPendingDrawWrites(presenter,&invalid);onlyExit(p,GlDeferredCopyExit::UnknownWrite);
        fresh();p.discardPendingDrawWrites(presenter,nullptr);onlyExit(p,GlDeferredCopyExit::UnknownWrite);
        // The new precision is ONLY for already submitted/queued destinations.
        // Any possible interleaved write still invalidates an in-flight capture.
        for(unsigned kind=0;kind<3;++kind){
            p.synchronize(123,++revision);p.observe(presenter,root,true);const auto token=p.captureDeferredCopy();
            GlDrawWrites writes=kind==0?unrelated:GlDrawWrites{};
            if(kind==1)writes.defaultFramebuffer=writes.nativeColorWrite=true;
            p.discardPendingDrawWrites(presenter,kind==2?nullptr:&writes);
            require(p.deferCopy(presenter,copy,token)==GlDeferredCopyResult::Refused,
                    "unrelated/native/unknown writes revoke an in-flight token BEFORE later completion can queue stale metadata");
        }
        p.synchronize(123,++revision);p.observe(presenter,root,true);const auto token=p.captureDeferredCopy();
        GlDrawWrites none;p.discardPendingDrawWrites(presenter,&none);
        require(p.deferCopy(presenter,copy,token)==GlDeferredCopyResult::Queued,"a complete genuinely empty raster footprint need not invent a write");balanced(p);
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);p.observe(presenter,root,true);
        p.deferCopy(presenter,usage(false,{70},{51}));p.deferCopy(presenter,usage(false,{GlColorResource::renderbuffer(70)},{52}));
        p.deferCopy(presenter,usage(false,{front},{53}));balanced(p);
        require(p.deferredCopyAudit().sources==std::array<std::uint64_t,4>{{1,1,1,0}},"queued input kinds remain typed; equal numeric texture/RBO names are not aliased");
        const auto write=usage(false,{1},{53});p.discardPendingWrites(presenter,&write);
        for(unsigned i=0;i<3;++i)p.nextFrame();
        balanced(p);
        require(p.deferredCopyAudit().queued==3&&!p.deferredCopyAudit().pending
                &&p.deferredCopyAudit().exits[index(GlDeferredCopyExit::DestinationWrite)]==1
                &&p.deferredCopyAudit().exits[index(GlDeferredCopyExit::Expired)]==2,"mixed retirement reasons partition the captured metadata, not process-wide native calls");
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);p.observe(presenter,root,true);
        for(unsigned i=0;i<GlTextureProvenance::pendingCopyCapacity;++i)
            require(p.deferCopy(presenter,usage(false,{front},{1000+i}))==GlDeferredCopyResult::Queued,"fill exact pending budget");
        require(p.deferCopy(presenter,usage(false,{front},{2000}))==GlDeferredCopyResult::Capacity,"diagnostics never enlarge or evict the pending budget");
        balanced(p);require(p.deferredCopyAudit().queued==32&&p.deferredCopyAudit().pending==32&&p.deferredCopyAudit().peak==32,"refused capacity is not counted as an admitted pending copy");
        p.observe(presenter,usage(true,{1000}),true);balanced(p);
        require(p.deferredCopyAudit().pending==31&&p.deferredCopyAudit().peak==32,"peak is a gauge, not another admission count");
    }
    {
        // Exact destination seen, but its proof cannot enter the full graph.
        GlTextureProvenance p;p.synchronize(123,1);p.observe(presenter,root,true);
        p.deferCopy(presenter,usage(false,{front},{1000}));
        for(unsigned i=0;i<127;++i)require(p.observe(presenter,usage(true,{2000+i}),true)==1,"fill proof budget without consuming queued output");
        require(!p.observe(presenter,usage(true,{1000}),true)&&!p.hasNativeOutput(presenter,front),"matching consumption cannot evade the existing resource budget");
        balanced(p);require(p.deferredCopyAudit().resourceBlocked==1&&p.deferredCopyAudit().pending==1
                &&p.deferredCopyAudit().consumers[index(GlDeferredConsumerStop::ExactDestination)]==1,"distinguish exact consumer/proof-budget failure from missing destination");
    }
    {
        // Destination proof fits, but its predecessor has no remaining slot.
        GlTextureProvenance p;p.synchronize(123,1);p.observe(presenter,root,true);
        p.deferCopy(presenter,usage(false,{front},{1000}));
        for(unsigned i=0;i<126;++i)p.observe(presenter,usage(true,{2000+i}),true);
        require(p.observe(presenter,usage(true,{1000}),true)==1&&!p.hasNativeOutput(presenter,front),"input proof insertion may also reach the unchanged resource budget");
        balanced(p);require(p.deferredCopyAudit().resourceBlocked==1&&p.deferredCopyAudit().pending==1,"a reconciliation retry is NOT an edge retirement or ownership");
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);p.observe(presenter,root,true);
        for(unsigned i=0;i<GlTextureProvenance::maximumHops;++i)p.deferCopy(presenter,usage(false,{101+i},{100+i}));
        p.observe(presenter,usage(true,{100}),true);balanced(p);
        require(p.deferredCopyAudit().pending==1&&p.deferredCopyAudit().hopBlocked>0
                &&p.deferredCopyAudit().exits[index(GlDeferredCopyExit::Reconciled)]==7
                &&!p.producer(presenter,usage(false,{1},{108})),"retry accounting cannot relax the original eight-hop limit");
    }
    {
        GlTextureProvenance p;
        for(unsigned site=0;site<glDeferredDiscardLabels.size();++site){
            p.synchronize(123,site+1);p.observe(presenter,root,true);p.deferCopy(presenter,copy);
            p.discardPendingWrites(presenter,nullptr,static_cast<GlDeferredDiscardSite>(site));
            onlyExit(p,GlDeferredCopyExit::UnknownWrite);
            require(p.deferredCopyAudit().discardSites[site]==1,"each existing uncaptured-output site partitions removed metadata, not all callback attempts");
        }
    }
    for(const auto *label:glDeferredExitLabels)require(label&&*label,"every retirement reason has a stable label");
    for(const auto *label:glDeferredConsumerLabels)require(label&&*label,"every existing consumer probe has a stable label");
    for(const auto *label:glDeferredDiscardLabels)require(label&&*label,"every uncaptured-output site has a stable label");
    std::puts("PASS: pending-copy queue/retirement balance, typed sources, exact/sibling/other-namespace consumer probes, original TTL, overwrite/uncaptured/missed/lifecycle/share/reset reasons, cumulative snapshots and unchanged capacity/hop refusals");
    std::puts("LIMIT: CPU-only metadata accounting; no game graph, native driver success, material pixels or permission to relax provenance");
}
