#pragma once
#include "GlPipeline.h"
#include "TextureQuality.h"
#include "GlTextureProvenance.h"
#include <memory>

namespace BsiShaders {
// Draw-scoped PRIVATE sampler substitution, not a final-image blur. The caller
// must first prove the CURRENT drawable belongs to the selected owned game
// presentation, OR prove the current color-FBO output is a recently consumed
// presentation dependency in the same/explicitly shared object namespace.
// Sharing, a loaded provider or equal numeric texture names alone never prove
// ownership. Foreign window routes remain excluded. No source object is edited.
// GL 3.3 / GLES 3.1, opaque triangle draws, ordinary sampler2D uniforms and
// complete normalized-color mip chains. The legacy policy additionally needs
// qualified depth; explicit owned-mip compatibility does NOT require depth.
// Compatibility can affect opaque mipmapped in-game HUD; it is not semantic
// scene classification. Blended draws, separate Qt surfaces, protected/foreign
// routes and unsupported depth/integer/HDR formats stay native. Normalized RGB
// data maps cannot be distinguished from albedo by format alone.
// Desktop GL uses a queried, bounded private LOD bias plus floor; GLES retains
// its floor-only policy and never queries/writes desktop bias.
class GlTextureOverride {
public:
    GlTextureOverride();
    ~GlTextureOverride();
    GlTextureOverride(const GlTextureOverride &) = delete;
    GlTextureOverride &operator=(const GlTextureOverride &) = delete;
    bool initialize(ResolveGl resolve,void *user,std::string *error,bool externalProtectionPossible=false);
    // Read-only dependency capture, including single-level compositor inputs.
    // No sampler replacement, texture binding, pixel readback or GL error read.
    // Restores ACTIVE_TEXTURE on every path; caller still proves the route.
    bool usage(GlTextureUsage *out);
    // Returns the number of bindings temporarily replaced for this draw.
    // Call end() AFTER the original draw; begin() already restores ACTIVE_TEXTURE
    // and rolls back every binding before propagating an inspection exception.
    unsigned begin(TextureQuality mode,bool externalOverridesReviewed,unsigned primitive,std::string *detail,
                   TextureDrawReason *reason=nullptr,TextureDepthFailure *depthFailure=nullptr,
                   TextureDrawPolicy policy=TextureDrawPolicy::DepthQualifiedScene);
    void end()noexcept;
    // Only with the owning context CURRENT. Destruction otherwise abandons
    // names to that context's driver teardown, never deletes in another context.
    void release();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace BsiShaders
