// Production native-route prefilter + output qualification + resource policy.
// No HWND, native driver, GL calls or installed-player semantics are simulated
// as pixels here; Windows detours and actual surfaceless pixels are separate.
#include "GlProducerRoute.h"
#include <cstdio>
#include <cstdlib>
using namespace BsiShaders;
namespace {
void require(bool value,const char *message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
GlTextureUsage fbo(GlColorResource output=50){
    GlTextureUsage usage;usage.reads[usage.readCount++]=1;usage.writes[usage.writeCount++]=output;return usage;
}
GlTextureUsage nativeOutput(){
    GlTextureUsage usage;usage.defaultFramebuffer=true;usage.nativeColorBuffer=0x0402;
    usage.reads[usage.readCount++]=1;return usage;
}
}
int main(){
    const auto selected=glProducerRoute(true,false,true,true,false);
    const auto sameWindow=glProducerRoute(false,false,true,true,false);
    const auto nonWindow=glProducerRoute(false,false,false,false,true);
    const auto pbuffer=glProducerRoute(false,true,false,false,true);
    const auto hiddenPbuffer=glProducerRoute(false,true,true,false,false);
    require(selected==GlProducerRoute::Selected&&sameWindow==GlProducerRoute::NamedResourceOnly,
            "only the selected drawable has a selected default output; another drawable on its HWND is resource-only");
    require(pbuffer==GlProducerRoute::NativePbuffer&&hiddenPbuffer==pbuffer
            &&glProducerRoute(true,true,true,true,false)==pbuffer,
            "an observed pbuffer lease remains explicit native evidence even with its driver's hidden HWND or selected association");
    require(glProducerRoute(false,false,true,false,true)==GlProducerRoute::Refuse
            &&glProducerRoute(false,false,false,false,false)==GlProducerRoute::Refuse,
            "foreign/Qt windows, unknown non-null EGL surfaces and missing WGL DCs cannot become resource routes");
    auto output=fbo();
    require(nonWindow==GlProducerRoute::NamedResourceOnly&&qualifyGlProducerOutput(nonWindow,0,output)
            &&output.writeCount==1&&output.writes[0]==50,
            "a current non-window WGL DC or surfaceless EGL context must reach EXACT named-FBO proof, without requiring a native pbuffer");
    auto native=nativeOutput();
    require(!qualifyGlProducerOutput(nonWindow,0,native)&&!qualifyGlProducerOutput(sameWindow,0,native)
            &&native.defaultFramebuffer&&!native.writeCount&&native.nativeColorBuffer==0x0402,
            "resource-only routes cannot authorize, invent or change an unknown default framebuffer/pbuffer output");
    require(qualifyGlProducerOutput(selected,0,native)&&!native.writeCount,
            "the exact selected default output seeds only its actual inputs, not a fake pbuffer identity");
    require(!qualifyGlProducerOutput(pbuffer,0,native)&&qualifyGlProducerOutput(pbuffer,800,native)
            &&native.writeCount==1&&native.writes[0]==GlColorResource::pbuffer(800,0x0402),
            "a default native output requires its nonzero observed lifetime and exact color aspect");
    output=fbo();require(!qualifyGlProducerOutput(GlProducerRoute::Refuse,800,output)&&output.writeCount==1&&output.writes[0]==50,
            "even exact resource names cannot bypass a foreign native route");
    const GlContextIdentity presenter{1,10,100},shared{1,10,200},isolated{1,10,300};
    GlTextureProvenance dependencies;dependencies.synchronize(123,1);
    output=fbo();
    require(qualifyGlProducerOutput(nonWindow,0,output)&&!dependencies.producer(presenter,output),
            "drawable-independent FBO inspection is NOT ownership without actual recent selected consumption");
    auto consume=nativeOutput();consume.reads[0]=50;
    require(dependencies.shared(presenter,shared)&&dependencies.observe(presenter,consume,true)==1
            &&dependencies.producer(presenter,output)&&dependencies.producer(shared,output),
            "the SAME or successfully-shared namespace still needs the exact consumed texture output");
    require(!dependencies.producer(isolated,output)&&!dependencies.producer({2,10,100},output)
            &&!dependencies.producer({1,11,100},output)&&!dependencies.producer(shared,fbo(51))
            &&!dependencies.producer(shared,fbo(GlColorResource::renderbuffer(50))),
            "isolated/provider/display/sibling/typed-name collisions remain native, even on resource-only routes");
    dependencies.observe(shared,output,false);
    for(unsigned i=0;i<=GlTextureProvenance::frameLifetime;++i)dependencies.nextFrame();
    require(!dependencies.producer(shared,output),"a resource-only producer cannot renew its own selected-consumer TTL");
    dependencies.observe(presenter,consume,true);dependencies.synchronize(123,2);
    require(!dependencies.producer(shared,output),"Off/settings revision revokes resource-only producers too");
    dependencies.observe(presenter,consume,true);unsigned name=50;dependencies.deleted(1,1,&name);
    require(!dependencies.producer(shared,output),"texture deletion/name reuse revokes the entire resource-only path");
    dependencies.observe(presenter,consume,true);dependencies.synchronize(0,2);
    require(!dependencies.producer(shared,output),"source teardown cannot be replaced by an offscreen FBO becoming its own presentation");
    std::puts("PASS: production resource-only native-route prefilter/output qualification, exact typed same/observed-share consumption, no guessed default output/foreign route, namespace/TTL/revision/deletion/source boundaries");
    std::puts("LIMIT: native-route/resource policy unit fixture, not Windows execution, GL pixels or installed-game success");
}
