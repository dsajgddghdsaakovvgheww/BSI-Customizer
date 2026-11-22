// Native-success events only. This portable fixture does not implement WGL,
// prove a route belongs to a game, or authorize any texture substitution.
#include "WglPbufferRegistry.h"
#include <cstdio>
#include <cstdlib>
using namespace BsiShaders;
namespace {
void require(bool value,const char *message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
}
int main(){
    WglPbufferRegistry p;
    require(!p.resource(100)&&!p.acquired(0,100)&&!p.acquired(1,0),"unknown/null DCs are not inferred to be pbuffers");
    require(p.acquired(1,100),"successful native acquisition records exactly that pbuffer/DC");
    const auto first=p.resource(100);require(first!=0&&!p.resource(101),"no neighboring/unknown DC adoption");
    require(p.acquired(1,100)&&p.resource(100)==first,"repeated successful gets retain the same framebuffer generation");
    require(p.released(1,100)&&p.resource(100)==first,"one release cannot revoke another live native DC lease");
    require(!p.released(2,100)&&!p.released(1,101)&&p.resource(100)==first,"wrong-handle/DC releases do not affect a live route");
    require(p.released(1,100)&&!p.resource(100),"last release excludes all draws on that DC until another observed get");
    require(p.acquired(1,101)&&p.resource(101)==first&&!p.resource(100),"refreshing a released DC preserves pbuffer storage identity, not the stale HDC");
    require(p.destroyed(1)==first&&!p.resource(101),"destruction revokes even outstanding leases and returns the retired generation");
    require(p.acquired(1,101)&&p.resource(101)!=first,"reusing BOTH native handles must still get a fresh generation");
    const auto reused=p.resource(101);
    require(!p.acquired(2,101)&&!p.resource(101),"ambiguous DC reuse without release revokes the old mapping rather than guessing");
    require(p.acquired(2,101)&&p.resource(101)!=reused,"a fresh successful get can relearn only a new generation");
    require(!p.acquired(2,102)&&!p.resource(101)&&!p.resource(102),"changing the DC with a live lease fails closed for that pbuffer");
    WglPbufferRegistry full;
    for(std::size_t i=0;i<WglPbufferRegistry::capacity;++i)require(full.acquired(1+i,1000+i),"fixed pbuffer capacity");
    const auto live=full.resource(1000);
    require(!full.acquired(10000,20000)&&full.resource(1000)==live&&!full.resource(20000),"capacity overflow does not evict/adopt a live proof");
    require(full.destroyed(1)==live&&full.acquired(10000,20000)&&full.resource(20000)!=live,"a retired slot is reused only with a new identity");
    WglPbufferRegistry leases;
    for(unsigned i=0;i<WglPbufferRegistry::maximumLeases;++i)require(leases.acquired(1,100),"bounded repeated native leases");
    require(!leases.acquired(1,100)&&!leases.resource(100),"lease overflow cannot wrap into a falsely live mapping");
    std::puts("PASS: exact observed pbuffer/DC leases, refreshed DC identity, destruction/handle reuse, ambiguity, bounded capacity/lease overflow; no game ownership inferred");
}
