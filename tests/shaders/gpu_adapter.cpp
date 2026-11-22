#include "GlPipeline.h"
#include "GlTextureQuality.h"
#include "GlTextureOverride.h"
#include "GlProducerRoute.h"
#include <cstring>
#include <memory>

// Test-only C bridge to the production pipeline, not a reimplementation.
namespace {
std::unique_ptr<BsiShaders::GlPipeline> pipeline;
std::unique_ptr<BsiShaders::GlTextureOverride> quality,nativeQuality;
BsiShaders::GlTextureBinding pendingImageBinding;
bool imageCaptured=false;
constexpr BsiShaders::GlContextIdentity nativeContext{1,1,2};
constexpr auto nativePbuffer=BsiShaders::GlColorResource::pbuffer(1000002,0x0405);
BsiShaders::GlTextureProvenance dependencies;
constexpr BsiShaders::GlContextIdentity authoredContext{1,1,1};
constexpr auto authoredPbuffer=BsiShaders::GlColorResource::pbuffer(1000001);
BsiShaders::GlTextureUsage pendingTransfer;
bool transferCaptured=false;
unsigned producerBegin(int mode,bool pbuffer,bool namedOnly=false,int strength=100){
    // Python verifies the actual current EGL context/display and BOTH null
    // surfaces before requesting namedOnly. This fixture-only bridge supplies
    // the native fact, never the final resource proof or a fake pbuffer ID.
    const auto route=BsiShaders::glProducerRoute(false,false,false,false,namedOnly);
    if(namedOnly&&route==BsiShaders::GlProducerRoute::Refuse)return 0;
    BsiShaders::GlTextureUsage usage;
    if(!quality->usage(&usage))return 0;
    if(namedOnly&&!BsiShaders::qualifyGlProducerOutput(route,0,usage))return 0;
    if(pbuffer&&usage.defaultFramebuffer){usage.writes[0]=authoredPbuffer;usage.writeCount=1;}
    if(!dependencies.producer(authoredContext,usage))return 0;
    dependencies.observe(authoredContext,usage,false);std::string detail;
    return quality->begin(static_cast<BsiShaders::TextureQuality>(mode),true,4,&detail,nullptr,nullptr,
                          BsiShaders::TextureDrawPolicy::OwnedMipCompatibility,strength);
}
}
extern "C" {
int bsi_test_initialize(BsiShaders::ResolveGl resolve,void *user,char *message,int capacity){
    pipeline=std::make_unique<BsiShaders::GlPipeline>();std::string error;
    const bool okay=pipeline->initialize(resolve,user,&error);
    if(capacity>0){std::strncpy(message,error.c_str(),capacity-1);message[capacity-1]=0;}return okay;
}
int bsi_test_render(int width,int height,int enabled,const int *values,char *message,int capacity){
    BsiShaders::Parameters p;p.enabled=enabled!=0;
    for(std::size_t i=0;i<p.values.size();++i)p.values[i]=values[i];
    std::string error;const bool okay=pipeline&&pipeline->render(width,height,p,&error);
    if(capacity>0){std::strncpy(message,error.c_str(),capacity-1);message[capacity-1]=0;}return okay;
}
void bsi_test_reference(const float *input,const int *values,float *output){
    BsiShaders::Parameters p;p.enabled=true;
    for(std::size_t i=0;i<p.values.size();++i)p.values[i]=values[i];
    const auto result=BsiShaders::grade({input[0],input[1],input[2]},p);
    for(int i=0;i<3;++i)output[i]=result[i];
}
// Test-owned mip resources supply explicit qualification. This bridge is not
// linked into the plugin and cannot assert ownership/profile safety in a game.
int bsi_test_quality_minimum(int mode,int es,int supported,unsigned mips,float minimum,float maximum,float bias,int reviewed,float *out){
    using namespace BsiShaders;
    TextureQualityProof proof;proof.renderer=es?Renderer::OpenGLES:Renderer::OpenGL;proof.requested=RendererFamily::OpenGL;
    proof.use=TextureUse::SampledColor;proof.gameSurfaceIdentified=proof.resourceOwnershipProven=true;
    proof.privateReplacementSupported=supported!=0;proof.externalOverridesReviewed=reviewed!=0;proof.visibleMipLevels=mips;
    GlQualitySampler source;source.minimumFilter=0x2703;source.minimumLod=minimum;source.maximumLod=maximum;source.lodBias=bias;
    const auto plan=planGlTextureQuality(static_cast<TextureQuality>(mode),proof,source);
    *out=plan.descriptor.minimumLod;return plan.changed();
}
int bsi_test_quality_initialize(BsiShaders::ResolveGl resolve,void *user,char *message,int capacity){
    quality=std::make_unique<BsiShaders::GlTextureOverride>();std::string error;
    const bool okay=quality->initialize(resolve,user,&error);
    if(capacity>0){std::strncpy(message,error.c_str(),capacity-1);message[capacity-1]=0;}return okay;
}
unsigned bsi_test_quality_begin(int mode,int reviewed,unsigned primitive,char *message,int capacity){
    std::string detail;
    const auto count=quality->begin(static_cast<BsiShaders::TextureQuality>(mode),reviewed!=0,primitive,&detail);
    if(capacity>0){std::strncpy(message,detail.c_str(),capacity-1);message[capacity-1]=0;}return count;
}
unsigned bsi_test_quality_begin_compatibility(int mode,int reviewed,unsigned primitive,char *message,int capacity){
    std::string detail;
    const auto count=quality->begin(static_cast<BsiShaders::TextureQuality>(mode),reviewed!=0,primitive,&detail,
                                    nullptr,nullptr,BsiShaders::TextureDrawPolicy::OwnedMipCompatibility);
    if(capacity>0){std::strncpy(message,detail.c_str(),capacity-1);message[capacity-1]=0;}return count;
}
unsigned bsi_test_quality_begin_strength(int mode,int reviewed,unsigned primitive,int compatibility,int strength,char *message,int capacity){
    std::string detail;
    const auto policy=compatibility?BsiShaders::TextureDrawPolicy::OwnedMipCompatibility:BsiShaders::TextureDrawPolicy::DepthQualifiedScene;
    const auto count=quality->begin(static_cast<BsiShaders::TextureQuality>(mode),reviewed!=0,primitive,&detail,nullptr,nullptr,policy,strength);
    if(capacity>0){std::strncpy(message,detail.c_str(),capacity-1);message[capacity-1]=0;}return count;
}
// Actual API observations + production dependency policy. The caller owns this
// fixture context; these exports are NEVER part of the deployed module.
void bsi_test_dependency_reset(unsigned revision){dependencies.synchronize(1,revision);transferCaptured=false;}
void bsi_test_dependency_frame(){dependencies.nextFrame();}
unsigned bsi_test_dependency_consumer(){
    BsiShaders::GlTextureUsage usage;
    return quality->usage(&usage)&&usage.defaultFramebuffer?dependencies.observe(authoredContext,usage,true):0;
}
unsigned bsi_test_dependency_producer_begin(int mode){return producerBegin(mode,false);}
unsigned bsi_test_dependency_pbuffer_begin(int mode){return producerBegin(mode,true);}
unsigned bsi_test_dependency_resource_only_begin(int mode,int strength){return producerBegin(mode,false,true,strength);}
// Authored native-pbuffer qualification, not a Windows lifecycle assertion.
// Python invokes the real GL original BETWEEN capture and complete.
unsigned bsi_test_dependency_transfer_capture(int copy,int qualifyNativeRead){
    using namespace BsiShaders;transferCaptured=false;
    const auto call=copy?GlColorTransfer::copy(0x0de1,0,0,0,0,0,64,64)
                        :GlColorTransfer::blit(0,0,64,64,0,0,64,64,0x4000,0x2600);
    if(!quality->transfer(call,&pendingTransfer))return 0;
    for(std::size_t i=0;i<pendingTransfer.readCount;++i)if(pendingTransfer.reads[i].kind==GlColorResource::Pbuffer){
        if(!qualifyNativeRead)return 0;
        pendingTransfer.reads[i]=authoredPbuffer;
    }
    if(pendingTransfer.defaultFramebuffer)pendingTransfer.writeCount=0;
    transferCaptured=true;return 1;
}
unsigned bsi_test_dependency_transfer_complete(){
    if(!transferCaptured)return 0;
    transferCaptured=false;return dependencies.observe(authoredContext,pendingTransfer,pendingTransfer.defaultFramebuffer);
}
// The real EGL fixture creates a NONSHARING producer context and a native
// texture-capable pbuffer, calls the actual native bind between these exports,
// and runs the actual production observers/policy/private draw samplers.
unsigned bsi_test_native_binding_capture(){imageCaptured=quality->textureBinding(&pendingImageBinding);return imageCaptured;}
unsigned bsi_test_native_binding_complete(int nativeSuccess){
    if(!imageCaptured)return 0;
    imageCaptured=false;
    BsiShaders::GlTextureBinding now;
    if(!nativeSuccess||!quality->textureBinding(&now,true)||!(now==pendingImageBinding))return 0;
    BsiShaders::GlTextureUsage usage;usage.reads[usage.readCount++]=nativePbuffer;usage.writes[usage.writeCount++]=now.texture;
    return dependencies.observe(authoredContext,usage,false);
}
int bsi_test_native_producer_initialize(BsiShaders::ResolveGl resolve,void *user){
    nativeQuality=std::make_unique<BsiShaders::GlTextureOverride>();std::string error;
    return nativeQuality->initialize(resolve,user,&error);
}
unsigned bsi_test_native_producer_begin(int mode,int strength){
    BsiShaders::GlTextureUsage usage;
    if(!nativeQuality->usage(&usage)||!usage.defaultFramebuffer||usage.nativeColorBuffer!=nativePbuffer.colorBuffer)return 0;
    usage.writes[0]=nativePbuffer;usage.writeCount=1;
    if(!dependencies.producer(nativeContext,usage))return 0;
    dependencies.observe(nativeContext,usage,false);std::string detail;
    return nativeQuality->begin(static_cast<BsiShaders::TextureQuality>(mode),true,4,&detail,nullptr,nullptr,
                               BsiShaders::TextureDrawPolicy::OwnedMipCompatibility,strength);
}
void bsi_test_native_producer_end(){nativeQuality->end();}
void bsi_test_native_producer_release(){if(nativeQuality)nativeQuality->release();nativeQuality.reset();}
void bsi_test_quality_end(){quality->end();}
void bsi_test_quality_release(){if(quality)quality->release();quality.reset();}
void bsi_test_release(){if(pipeline)pipeline->release();pipeline.reset();}
}
