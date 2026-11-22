// Allocation-free resource-dependency policy, not a driver/player trace.
#include "GlTextureProvenance.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
using namespace BsiShaders;
namespace {
void require(bool value,const char *message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
GlTextureUsage usage(bool root,std::initializer_list<unsigned> reads,std::initializer_list<unsigned> writes={}){
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
    std::puts("PASS: actual-consumer -> same/explicit-share FBO producer dependency, not share-group guessing; TTL/hop/capacity bounds, unrelated providers/displays/contexts/outputs, revision/source/texture/context teardown and handle reuse");
    std::puts("LIMIT: resource-policy unit fixture, not installed-game pixels or physical-driver certification");
}
