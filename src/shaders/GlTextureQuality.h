#pragma once
#include "TextureQuality.h"

namespace BsiShaders {
// Relevant fields from a caller-owned sampler, not arbitrary texture state.
// A private replacement must also copy the caller's full wrap/border/other state.
// GL >= 3.3 / GLES >= 3 supplies sampler objects and MIN_LOD. GL 2 / GLES 2
// must pass privateReplacementSupported=false and leave their state untouched.
struct GlQualitySampler {
    int minimumFilter=0x2601; // GL_LINEAR (no mip chain by default)
    int comparisonMode=0;   // GL_NONE
    float minimumLod=-1000,maximumLod=1000,lodBias=0,maximumAnisotropy=1;
};
struct GlQualityCapabilities {
    // Only a queried desktop GL_MAX_TEXTURE_LOD_BIAS enables additive bias.
    // Zero keeps the original portable floor-only plan. GLES must use zero:
    // it has no sampler TEXTURE_LOD_BIAS state, even when entry points exist.
    float maximumLodBias=0;
};
// filtersControlLevels: the caller has verified that the bound resource really
// stores the levels this plan can reach. A base-only min filter then names an
// ELIGIBLE descriptor rather than a missing chain, so the plan may also switch
// the PRIVATE sampler's min filter to the matching mipmapped variant. The
// caller must have completed every protection/format/attachment/ownership
// check first; this flag never replaces them and is never set for a
// single-level resource.
inline TextureSamplerPlan<GlQualitySampler> planGlTextureQuality(
    TextureQuality mode,const TextureQualityProof &proof,const GlQualitySampler &source,
    const GlQualityCapabilities &capabilities={},int strength=100,bool filtersControlLevels=false){
    TextureSamplerPlan<GlQualitySampler> result{source,TextureQualityDecision::UnsupportedDescriptor};
    if(mode==TextureQuality::Native){result.decision=TextureQualityDecision::Disabled;return result;}
    if(proof.renderer!=Renderer::OpenGL&&proof.renderer!=Renderer::OpenGLES){result.decision=TextureQualityDecision::UnidentifiedRenderer;return result;}
    if(!std::isfinite(capabilities.maximumLodBias)||capabilities.maximumLodBias<0
       ||(proof.renderer==Renderer::OpenGLES&&capabilities.maximumLodBias!=0))return result;
    // Keep unrecognized filters, comparison modes and invalid AF unchanged.
    if((source.minimumFilter!=0x2600&&source.minimumFilter!=0x2601
        &&(source.minimumFilter<0x2700||source.minimumFilter>0x2703))
       ||!std::isfinite(source.maximumAnisotropy)||source.maximumAnisotropy<1)return result;
    if(source.minimumLod<0&&source.minimumLod!=-1000.f&&std::isfinite(source.minimumLod)){
        result.decision=TextureQualityDecision::ExistingLodOverride;return result;
    }
    TextureSampling sampling;
    // GL/GLES sampler objects start at MIN_LOD=-1000, not the zero used by
    // Vulkan/DirectX. Treat only that exact API default as an unrestricted
    // floor for planning. Other nonzero floors are preserved overrides.
    // The source/default is still returned byte-for-byte on every bypass.
    sampling.minimumLod=source.minimumLod==-1000.f?0.f:source.minimumLod;
    sampling.maximumLod=source.maximumLod;sampling.lodBias=source.lodBias;
    sampling.mipmapped=source.minimumFilter>=0x2700&&source.minimumFilter<=0x2703;
    sampling.storedChain=filtersControlLevels&&proof.visibleMipLevels>=2;
    sampling.comparison=source.comparisonMode!=0;sampling.anisotropic=source.maximumAnisotropy>1;
    const auto plan=planTextureQuality(mode,proof,sampling,strength);
    result.decision=plan.decision;
    if(plan.changed()){
        result.descriptor.minimumLod=plan.sampling.minimumLod;
        // A minimum floor alone does nothing when native lambda is already
        // above it. The independently tested desktop path also shifts lambda,
        // in a PRIVATE sampler only, bounded by view/max LOD and the queried
        // device limit. The shared proof/refusals ran first: never stack an
        // existing source bias/floor or authorize a resource by capability.
        if(proof.renderer==Renderer::OpenGL&&capabilities.maximumLodBias>0)
            result.descriptor.lodBias=(std::min)(plan.sampling.minimumLod,capabilities.maximumLodBias);
        // Only a policy-planned mip floor may reach a stored level below the
        // base image. Keep the source's WITHIN-LEVEL filter (NEAREST stays
        // NEAREST, LINEAR stays LINEAR) and add mipmap selection. A base-only
        // source has no mip-interpolation choice to preserve, so the mip
        // filter is LINEAR: the saved percentage is fractional (1.5/3.0 x %),
        // and a NEAREST mip filter would quantize 50% and 75% onto one level.
        // Existing mipmapped filters are left exactly as authored.
        if(sampling.storedChain&&plan.sampling.minimumLod>0&&source.minimumFilter==0x2600)
            result.descriptor.minimumFilter=0x2702; // NEAREST within level, LINEAR between mips
        else if(sampling.storedChain&&plan.sampling.minimumLod>0&&source.minimumFilter==0x2601)
            result.descriptor.minimumFilter=0x2703; // LINEAR within level, LINEAR between mips
    }
    return result;
}
} // namespace BsiShaders
