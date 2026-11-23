// Allocation-free resource-dependency policy, not a driver/player trace.
#include "GlTextureProvenance.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
using namespace BsiShaders;
namespace {
void require(bool value,const char *message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
GlTextureUsage usage(bool root,std::initializer_list<GlColorResource> reads,std::initializer_list<GlColorResource> writes={}){
    GlTextureUsage result;result.defaultFramebuffer=root;
    for(auto name:reads)result.reads[result.readCount++]=name;
    for(auto name:writes)result.writes[result.writeCount++]=name;
    return result;
}
const GlContextIdentity presenter{1,10,100},sharedScene{1,10,200},unknown{1,10,300};
}
int main(){
    const auto composite=usage(true,{50}),scene=usage(false,{1,2},{50}),intermediate=usage(false,{70},{50});
    {
        GlTextureProvenance p;p.remember(presenter);
        require(!p.producer(presenter,scene)&&!p.observe(presenter,composite,true),"no presentation source means no seeded ownership");
        p.synchronize(123,1);
        require(!p.producer(presenter,scene),"a known context alone is not resource provenance");
        require(p.observe(presenter,composite,true)==1&&p.producer(presenter,scene),
                "a single-level presentation input can identify its SAME-context color-FBO producer");
        require(!p.producer(presenter,usage(true,{1,2})),"an offscreen DEFAULT framebuffer is not a texture producer");
        require(!p.producer(presenter,usage(false,{1,2},{51})),"a different FBO output in the same context stays native");
        require(!p.producer(unknown,scene),"equal names in an unknown context do not establish sharing");
        require(p.remember(unknown)&&!p.producer(unknown,scene),"separately observed context names are still a distinct namespace");
        require(p.observe(presenter,intermediate,false)==1&&p.producer(presenter,usage(false,{3},{70})),
                "a proven producer propagates its actual input dependency one bounded hop");
        require(!p.producer(presenter,usage(false,{3},{71})),"an unrelated sibling target remains unqualified after propagation");
        require(!p.observe(unknown,composite,false),"foreign defaults cannot seed themselves");
        p.synchronize(123,2);require(!p.producer(presenter,scene),"Off/On or policy/review revision clears dependencies");
        p.observe(presenter,composite,true);p.synchronize(124,2);
        require(!p.producer(presenter,scene),"presentation source replacement clears previous resource evidence");
        p.observe(presenter,composite,true);p.synchronize(0,2);
        require(!p.producer(presenter,scene),"destroyed/unselected source never authorizes a producer");
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);p.observe(presenter,composite,true);
        require(p.shared(presenter,sharedScene)&&!p.producer(sharedScene,scene),
                "successful sharing alone is not ownership; a namespace merge clears old name evidence");
        require(p.observe(presenter,composite,true)==1&&p.producer(sharedScene,scene),
                "a successful native share plus fresh actual consumption authorizes the exact shared producer");
        require(!p.shared(presenter,{2,10,200})&&!p.shared(presenter,{1,11,200}),
                "never merge another provider or EGL display");
        require(!p.producer({2,10,100},scene)&&!p.producer({1,11,100},scene),
                "same numeric context/texture identities across providers/displays stay unrelated");
        p.invalidate(1);require(!p.producer(sharedScene,scene),"lost deletion details clear all resource evidence");
        p.observe(presenter,composite,true);require(p.producer(sharedScene,scene),
                "object deletion/overflow does not change observed context sharing; fresh consumption can relearn without a new native share");
        p.forget(sharedScene);require(!p.producer(sharedScene,scene),"context deletion/reuse revokes its namespace identity");
        p.remember(sharedScene);require(!p.producer(sharedScene,scene),"a reused handle is isolated until a new successful share");
        require(p.producer(presenter,scene),"deleting a nonpresenting shared peer does not erase the surviving object's namespace");
        p.forget(presenter);p.remember(presenter);
        require(!p.producer(presenter,scene),"a destroyed last namespace cannot revive through a reused context handle");
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);p.observe(presenter,composite,true);
        p.nextFrame();require(p.producer(presenter,scene),"one-frame producer lag can use recent actual consumption");
        p.observe(presenter,intermediate,false);p.nextFrame();
        require(p.producer(presenter,usage(false,{1},{70})),"second-frame inherited proof keeps the root's original expiry");
        p.observe(presenter,usage(false,{80},{70}),false);p.nextFrame();
        require(!p.producer(presenter,scene)&&!p.producer(presenter,usage(false,{1},{70}))
                &&!p.producer(presenter,usage(false,{1},{80})),
                "producer cycles cannot refresh their own TTL without another selected presentation consumer");
        require(p.observe(presenter,composite,true)==1&&p.producer(presenter,scene),"a new real consumer refreshes dependency evidence");
        p.observe(presenter,intermediate,false);
        unsigned name=50;p.deleted(1,1,&name);
        require(!p.producer(presenter,scene)&&!p.producer(presenter,usage(false,{1},{70})),
                "texture deletion revokes the deleted root AND its inherited ancestors before numeric-name reuse");
        p.observe(presenter,composite,true);p.deleted(2,1,&name);
        require(p.producer(presenter,scene),"a different provider's deletion does not remove this namespace");
        p.deleted(1,257,nullptr);require(!p.producer(presenter,scene),"oversized/null deletion lists fail closed without reading caller memory");
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);
        require(!p.observe(presenter,usage(false,{50},{99}),true),"a same-window FBO does not seed an arbitrary disconnected render graph");
        p.observe(presenter,composite,true);
        for(unsigned i=1;i<GlTextureProvenance::maximumHops;++i)
            require(p.observe(presenter,usage(false,{50+i},{49+i}),false)==1,"bounded backwards dependency propagation");
        require(!p.observe(presenter,usage(false,{58},{57}),false),"maximum dependency depth is a hard bound");
        require(!p.producer(presenter,usage(false,{1},{58})),"a refused depth cannot authorize a deeper producer");
        require(!p.observe(presenter,usage(false,{50},{50}),false),"feedback cannot self-seed or refresh a texture's evidence");
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);
        for(std::size_t i=0;i<GlTextureProvenance::contextCapacity;++i)
            require(p.remember({0,0,1000+i}),"bounded context registry accepts its fixed capacity");
        require(!p.remember(presenter)&&!p.observe(presenter,composite,true),"context overflow cannot invent/evict a namespace identity");
        p.forget({0,0,1000});require(p.remember(presenter),"deleted context slots can be reused with a fresh namespace ID");
        for(unsigned n=1;n<=GlTextureProvenance::textureCapacity;++n)
            require(p.observe(presenter,usage(true,{n}),true)==1,"bounded dependency storage accepts its fixed capacity");
        require(!p.observe(presenter,usage(true,{1000}),true),"dependency overflow fails closed without evicting live proofs");
        auto malformed=composite;malformed.readCount=33;
        require(!p.observe(presenter,malformed,true),"invalid capture counts never index past fixed arrays");
        malformed=scene;malformed.writeCount=33;
        require(!p.producer(presenter,malformed),"invalid output counts are not authorization");
        for(unsigned i=0;i<=GlTextureProvenance::frameLifetime;++i)p.nextFrame();
        require(p.observe(presenter,usage(true,{1000}),true)==1,"expired fixed slots are reusable without unbounded growth");
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);p.observe(presenter,composite,true);
        const auto pb=GlColorResource::pbuffer(800),rb=GlColorResource::renderbuffer(50);
        const auto pbufferDraw=usage(true,{1,2},{pb}),copy=usage(false,{pb},{50});
        require(!p.producer(presenter,pbufferDraw)&&!p.producer(presenter,usage(false,{1},{rb})),
                "neither known pbuffer nor same numeric renderbuffer/texture name is an upstream producer without a transfer");
        require(p.observe(presenter,copy,false)==1&&p.producer(presenter,pbufferDraw),
                "an observed copy of the exact default-pbuffer output into a consumed texture reaches its material producer");
        require(!p.producer(presenter,usage(true,{1},{GlColorResource::pbuffer(801)}))
                &&!p.producer({2,10,300},pbufferDraw)&&!p.producer({1,11,300},pbufferDraw),
                "another pbuffer lifetime, provider or display never inherits an observed native output link");
        require(p.hasNativePbuffer(unknown,800)&&p.producer(unknown,pbufferDraw)&&!p.known(unknown),
                "an exact native pbuffer output is not a GL object name: a real copy/binding reaches its nonsharing renderer without a guessed namespace merge");
        require(p.observe(unknown,pbufferDraw,false)==2&&p.known(unknown)
                &&p.producer(unknown,usage(false,{3},{1}))&&!p.producer(presenter,usage(false,{3},{1})),
                "material inputs inherited from a proven native output stay in the producer's SEPARATE namespace");
        p.observe(presenter,usage(false,{rb},{50}),false);
        require(p.producer(presenter,usage(false,{1},{rb})),"texture/RBO resolves cross typed resources, not global numeric names");
        unsigned unrelated=800;p.deleted(1,1,&unrelated);
        require(p.producer(presenter,pbufferDraw),"deleting a numerically equal TEXTURE does not retire a native pbuffer generation");
        p.retiredPbuffer(1,801);require(p.producer(presenter,pbufferDraw),"another native pbuffer generation's teardown cannot revoke this one");
        p.retiredPbuffer(1,800);
        require(!p.producer(presenter,pbufferDraw)&&!p.producer(presenter,scene),"native pbuffer retirement revokes the entire inherited transfer path");
        p.observe(presenter,composite,true);p.observe(presenter,usage(false,{rb},{50}),false);
        unsigned name=50;p.deleted(1,1,&name,GlColorResource::Renderbuffer);
        require(!p.producer(presenter,scene),"renderbuffer deletion is separately tracked and invalidates inherited resolve ancestors");
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);p.observe(presenter,composite,true);
        const auto front=GlColorResource::pbuffer(800,0x0400),back=GlColorResource::pbuffer(800,0x0402);
        require(!p.hasNativeOutput(unknown,front)&&!p.hasNativePbuffer(unknown,800),"native lifetime alone cannot create provenance");
        require(p.observe(presenter,usage(false,{front},{50}),false)==1,"successful native image binding of the consumed texture records its exact source buffer");
        require(p.hasNativeOutput(unknown,front)&&!p.hasNativeOutput(unknown,back)
                &&p.producer(unknown,usage(true,{1},{front}))&&!p.producer(unknown,usage(true,{1},{back})),
                "front/back color aspects of the SAME pbuffer remain different dependencies");
        p.observe(unknown,usage(true,{1},{front}),false);
        p.nextFrame();p.observe(unknown,usage(true,{1},{front}),false);p.nextFrame();p.nextFrame();
        require(!p.hasNativeOutput(unknown,front)&&!p.producer(unknown,usage(false,{2},{1})),
                "nonsharing native producer cannot renew either its pbuffer or inherited material-input TTL");
        p.observe(presenter,composite,true);p.observe(presenter,usage(false,{front},{50}),false);
        p.synchronize(123,2);require(!p.hasNativeOutput(unknown,front),"a settings/source revision revokes native bindings too");
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);p.observe(presenter,composite,true);
        const auto front=GlColorResource::pbuffer(800,0x0400),back=GlColorResource::pbuffer(800,0x0402);
        const auto copy=usage(false,{front},{51}),nativeDraw=usage(true,{1},{front});
        require(p.deferCopy(presenter,copy)==GlDeferredCopyResult::Queued&&!p.producer(presenter,nativeDraw),
                "copy-before-consumer retains bounded submitted metadata, NEVER pbuffer ownership");
        GlDeferredCopyResolution resolved;
        p.observe(presenter,composite,true,&resolved);
        require(!resolved.copies&&!p.producer(presenter,nativeDraw),"a different consumed output must not reconcile a sibling copy");
        p.observe(unknown,usage(true,{51}),true,&resolved);
        require(!resolved.copies&&!p.producer(presenter,nativeDraw),"same numeric destination in an unrelated namespace cannot consume pending copy metadata");
        require(p.observe(presenter,usage(true,{51}),true,&resolved)==1&&resolved.copies==1&&resolved.inputs==1
                &&p.producer(presenter,nativeDraw)&&!p.producer(presenter,usage(true,{1},{back})),
                "only a LATER exact actual consumer reconciles the native lifetime/color aspect, without merging GL names");
        p.observe(presenter,usage(true,{51}),true,&resolved);
        require(!resolved.copies&&!resolved.inputs,"each submitted pending edge is consumed once, not replayed every frame");
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);p.shared(presenter,sharedScene);p.observe(presenter,composite,true);
        const auto copy=usage(false,{GlColorResource::renderbuffer(70)},{51});
        require(p.deferCopy(sharedScene,copy)==GlDeferredCopyResult::Queued,"an observed shared namespace can retain an unconsumed copy");
        require(p.deferCopy(unknown,copy)==GlDeferredCopyResult::Refused
                &&p.deferCopy({2,10,100},copy)==GlDeferredCopyResult::Refused
                &&p.deferCopy({1,11,100},copy)==GlDeferredCopyResult::Refused,"pending copies cannot create context/provider/display dependencies");
        GlDeferredCopyResolution resolved;p.observe(presenter,usage(true,{51}),true,&resolved);
        require(resolved.copies==1&&p.producer(sharedScene,usage(false,{1},{GlColorResource::renderbuffer(70)}))
                &&!p.producer(presenter,usage(false,{1},{70})),"late shared consumption still separates TEXTURE and RENDERBUFFER numeric names");
    }
    {
        GlTextureProvenance p;const auto front=GlColorResource::pbuffer(800,0x0400);const auto copy=usage(false,{front},{51});
        const auto reset=[&]{p.synchronize(0,1);p.synchronize(123,1);p.observe(presenter,composite,true);require(p.deferCopy(presenter,copy)==GlDeferredCopyResult::Queued,"queue fresh exact typed copy");};
        GlDeferredCopyResolution resolved;
        reset();p.nextFrame();p.nextFrame();p.observe(presenter,usage(true,{51}),true,&resolved);
        require(resolved.copies==1&&p.hasNativeOutput(unknown,front),"pending deadline remains inclusive at two frames");
        p.observe(presenter,usage(true,{51}),true);p.nextFrame();
        require(!p.hasNativeOutput(unknown,front),"late consumption cannot extend the ORIGINAL copy deadline or replay its removed edge");
        reset();for(unsigned i=0;i<3;++i)p.nextFrame();p.observe(presenter,usage(true,{51}),true,&resolved);
        require(!resolved.copies&&!p.hasNativeOutput(unknown,front),"expired unconsumed copies cannot revive from a later destination-name match");
        reset();unsigned name=51;p.deleted(1,1,&name);p.observe(presenter,usage(true,{51}),true,&resolved);
        require(!resolved.copies&&!p.hasNativeOutput(unknown,front),"deleting an UNCONSUMED pending destination revokes it before name reuse");
        reset();p.retiredPbuffer(1,800);p.observe(presenter,usage(true,{51}),true,&resolved);
        require(!resolved.copies&&!p.hasNativeOutput(unknown,front),"retirement of an UNLINKED native source generation clears pending metadata too");
        reset();p.forget(presenter);p.remember(presenter);p.observe(presenter,usage(true,{51}),true,&resolved);
        require(!resolved.copies&&!p.hasNativeOutput(unknown,front),"context handle reuse never reuses a pending namespace generation");
        reset();p.shared(presenter,sharedScene);p.observe(presenter,usage(true,{51}),true,&resolved);
        require(!resolved.copies&&!p.hasNativeOutput(unknown,front),"a late namespace merge revokes pending copies, rather than retroactively sharing them");
        reset();p.synchronize(123,2);p.observe(presenter,usage(true,{51}),true,&resolved);
        require(!resolved.copies&&!p.hasNativeOutput(unknown,front),"Off/review/strength/source revision clears unconsumed copy metadata");
        reset();p.forgetProvider(1);p.observe(presenter,usage(true,{51}),true,&resolved);
        require(!resolved.copies&&!p.hasNativeOutput(unknown,front),"provider retirement cannot leave a pending native link");
        reset();p.invalidate(1);p.observe(presenter,usage(true,{51}),true,&resolved);
        require(!resolved.copies&&!p.hasNativeOutput(unknown,front),"lost retirement coverage drops pending and qualified dependencies together");
    }
    {
        GlTextureProvenance p;const auto front=GlColorResource::pbuffer(800,0x0400);const auto copy=usage(false,{front},{51});
        const auto reset=[&]{p.synchronize(0,1);p.synchronize(123,1);p.observe(presenter,composite,true);return p.captureDeferredCopy();};
        GlDeferredCopyResolution resolved;auto capture=reset();p.nextFrame();p.nextFrame();
        require(p.deferCopy(presenter,copy,capture)==GlDeferredCopyResult::Queued,"time inside the original still permits its ORIGINAL inclusive two-frame deadline");
        p.observe(presenter,usage(true,{51}),true,&resolved);require(resolved.copies==1,"original-deadline completion can reconcile once");
        p.nextFrame();require(!p.hasNativeOutput(unknown,front),"native completion cannot give a fresh deadline to an older captured copy");
        capture=reset();for(unsigned i=0;i<3;++i)p.nextFrame();p.observe(presenter,composite,true);
        require(p.deferCopy(presenter,copy,capture)==GlDeferredCopyResult::Refused,"a fresh unrelated consumer cannot revive an in-flight copy past its capture deadline");
        capture=reset();const auto write=usage(false,{1},{51});p.discardPendingWrites(presenter,&write);
        require(p.deferCopy(presenter,copy,capture)==GlDeferredCopyResult::Refused,"an intervening write revokes an IN-FLIGHT copy even when no pending slot exists yet");
        capture=reset();p.discardPendingProvider(1);
        require(p.deferCopy(presenter,copy,capture)==GlDeferredCopyResult::Refused,"a missed guarded/busy callback revokes an unqueued capture without deleting consumer proof");
        capture=reset();p.synchronize(123,2);p.observe(presenter,composite,true);
        require(p.deferCopy(presenter,copy,capture)==GlDeferredCopyResult::Refused,"source/review reset prevents old in-flight stamps aliasing a reset frame clock");
        capture=reset();unsigned name=51;p.deleted(1,1,&name);
        require(p.deferCopy(presenter,copy,capture)==GlDeferredCopyResult::Refused,"deletion revokes an unqueued destination before numeric name reuse");
        capture=reset();p.retiredPbuffer(1,800);
        require(p.deferCopy(presenter,copy,capture)==GlDeferredCopyResult::Refused,"native-source retirement revokes an unqueued capture too");
        capture=reset();++capture.expires;
        require(p.deferCopy(presenter,copy,{})==GlDeferredCopyResult::Refused
                &&p.deferCopy(presenter,copy,capture)==GlDeferredCopyResult::Refused,"empty/future deadline stamps cannot enlarge the capture lifetime");
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);p.observe(presenter,composite,true);
        const auto a=GlColorResource::pbuffer(800,0x0400),b=GlColorResource::pbuffer(801,0x0400);
        require(p.deferCopy(presenter,usage(false,{a},{51}))==GlDeferredCopyResult::Queued
                &&p.deferCopy(presenter,usage(false,{b},{51}))==GlDeferredCopyResult::Queued,"latest submitted copy replaces an overwritten pending predecessor");
        GlDeferredCopyResolution resolved;p.observe(presenter,usage(true,{51}),true,&resolved);
        require(resolved.copies==1&&!p.hasNativeOutput(unknown,a)&&p.hasNativeOutput(unknown,b),"a later consumer cannot revive an obsolete overwritten native source");
        p.synchronize(123,2);p.observe(presenter,composite,true);
        const auto rb=GlColorResource::renderbuffer(70);
        require(p.deferCopy(presenter,usage(false,{rb},{51}))==GlDeferredCopyResult::Queued,"typed RBO source queued");
        unsigned name=70;p.deleted(1,1,&name);p.observe(presenter,usage(true,{51}),true,&resolved);
        require(resolved.copies==1,"a numerically equal texture deletion is not an RBO retirement");
        p.synchronize(123,3);p.observe(presenter,composite,true);p.deferCopy(presenter,usage(false,{rb},{51}));
        p.deleted(1,1,&name,GlColorResource::Renderbuffer);p.observe(presenter,usage(true,{51}),true,&resolved);
        require(!resolved.copies,"retiring the actual typed pending input revokes the path");
    }
    {
        GlTextureProvenance p;const auto front=GlColorResource::pbuffer(800,0x0400);
        const auto reset=[&]{p.synchronize(0,1);p.synchronize(123,1);p.observe(presenter,composite,true);p.deferCopy(presenter,usage(false,{front},{51}));};
        GlDeferredCopyResolution resolved;const auto same=usage(false,{1},{51}),other=usage(false,{1},{52});
        reset();p.discardPendingWrites(presenter,&same);p.observe(presenter,usage(true,{51}),true,&resolved);
        require(!resolved.copies&&!p.hasNativeOutput(unknown,front),"an observed later draw/copy into the pending DESTINATION replaces the old edge");
        reset();p.discardPendingWrites(presenter,&other);p.observe(presenter,usage(true,{51}),true,&resolved);
        require(resolved.copies==1,"an exact captured sibling write need not revoke unrelated pending metadata");
        reset();p.discardPendingWrites(presenter,nullptr);p.observe(presenter,usage(true,{51}),true,&resolved);
        require(!resolved.copies,"uncapturable callback output revokes pending metadata in that namespace, without querying more GL state");
        reset();p.discardPendingWrites(unknown,nullptr);p.discardPendingWrites({2,10,100},nullptr);p.discardPendingWrites({1,11,100},nullptr);
        p.observe(presenter,usage(true,{51}),true,&resolved);
        require(resolved.copies==1,"another isolated context/provider/display cannot collide with the pending namespace");
        reset();p.discardPendingProvider(1);p.observe(presenter,usage(true,{51}),true,&resolved);
        require(!resolved.copies&&p.hasDependencies(presenter),"missed-callback revocation drops pending metadata only, not existing namespace/consumer proof");
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);p.observe(presenter,composite,true);
        for(unsigned i=0;i<GlTextureProvenance::pendingCopyCapacity;++i)
            require(p.deferCopy(presenter,usage(false,{GlColorResource::pbuffer(800+i,0x0400)},{1000+i}))==GlDeferredCopyResult::Queued,"fixed pending-copy budget accepts its exact capacity");
        require(p.deferCopy(presenter,usage(false,{GlColorResource::renderbuffer(70)},{2000}))==GlDeferredCopyResult::Capacity,"pending overflow never evicts a live proof or grows the budget");
        GlDeferredCopyResolution resolved;p.observe(presenter,usage(true,{1000}),true,&resolved);
        require(resolved.copies==1&&p.deferCopy(presenter,usage(false,{GlColorResource::renderbuffer(70)},{2000}))==GlDeferredCopyResult::Queued,"resolved metadata frees exactly one bounded pending slot");
        require(p.deferCopy(presenter,usage(false,{1},{1}))==GlDeferredCopyResult::Refused
                &&p.deferCopy(presenter,usage(false,{1},{GlColorResource::renderbuffer(60)}))==GlDeferredCopyResult::Refused
                &&p.deferCopy(presenter,usage(false,{1,2},{60}))==GlDeferredCopyResult::Refused
                &&p.deferCopy(presenter,usage(false,{GlColorResource::pbuffer(800)},{60}))==GlDeferredCopyResult::Refused,
                "feedback, nontexture destination, non-copy arity and unknown native color aspects are not queued");
    }
    {
        GlTextureProvenance p;p.synchronize(123,1);p.observe(presenter,composite,true);
        for(unsigned i=0;i<GlTextureProvenance::maximumHops;++i)
            require(p.deferCopy(presenter,usage(false,{101+i},{100+i}))==GlDeferredCopyResult::Queued,"queue a bounded reverse-order copy chain");
        GlDeferredCopyResolution resolved;p.observe(presenter,usage(true,{100}),true,&resolved);
        require(resolved.copies==GlTextureProvenance::maximumHops-1
                &&p.producer(presenter,usage(false,{1},{107}))&&!p.producer(presenter,usage(false,{1},{108})),"deferred resolution retains the SAME eight-hop limit");
        p.synchronize(123,2);p.observe(presenter,composite,true);
        p.deferCopy(presenter,usage(false,{52},{51}));p.deferCopy(presenter,usage(false,{51},{52}));
        require(!p.producer(presenter,usage(false,{1},{51})),"a queued cycle cannot seed its own ownership");
        p.observe(presenter,usage(true,{51}),true,&resolved);require(resolved.copies==2,"real consumption can resolve a cycle only once per captured edge");
        for(unsigned i=0;i<3;++i)p.nextFrame();
        require(!p.producer(presenter,usage(false,{1},{51}))&&!p.producer(presenter,usage(false,{1},{52})),"resolved copies never renew a cycle's TTL");
    }
    std::puts("PASS: copy-before-consumer bounded metadata, exact later same/observed-share consumption only, no pre-consumption ownership; original two-frame deadline/eight hops, typed input/output retirement, latest copy, namespace/source/review reset and capacity controls");
    std::puts("PASS: actual-consumer -> same/explicit-share typed texture/RBO/pbuffer producer dependency, observed color transfers not share-group guessing; TTL/hop/capacity bounds, unrelated providers/displays/contexts/outputs, revision/source/resource teardown and handle reuse");
    std::puts("LIMIT: resource-policy unit fixture, not installed-game pixels or physical-driver certification");
}
