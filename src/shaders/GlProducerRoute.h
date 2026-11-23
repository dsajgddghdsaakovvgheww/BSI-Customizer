#pragma once
#include "GlTextureProvenance.h"

namespace BsiShaders {
// Native drawable identity is needed for a DEFAULT color output, not for an
// independently qualified GL texture/RBO attachment. Classification is ONLY a
// prefilter: current/protection checks, exact recent consumption and the same
// or observed-successfully-shared namespace remain mandatory at the caller.
// A foreign HWND and an unknown non-null EGL surface never become discovery
// routes. A resource-only route must never stand in for a native pbuffer.
enum class GlProducerRoute { Refuse, Selected, NativePbuffer, NamedResourceOnly };
constexpr GlProducerRoute glProducerRoute(bool selected,bool pbuffer,bool hasWindow,bool selectedWindow,bool resourceOnlyDrawable){
    return pbuffer?GlProducerRoute::NativePbuffer:selected?GlProducerRoute::Selected
        :hasWindow?(selectedWindow?GlProducerRoute::NamedResourceOnly:GlProducerRoute::Refuse)
        :resourceOnlyDrawable?GlProducerRoute::NamedResourceOnly:GlProducerRoute::Refuse;
}
// Invalidation-only footprints cannot create a native identity or ownership.
// A resource-only drawable still cannot stand in for an unknown default image.
inline bool qualifyGlDrawWrites(GlProducerRoute route,std::uintptr_t pbuffer,const GlDrawWrites &writes){
    if(route==GlProducerRoute::Refuse)return false;
    return !writes.defaultFramebuffer||route==GlProducerRoute::Selected
        ||(route==GlProducerRoute::NativePbuffer&&pbuffer);
}
// No GL calls or allocations; leave a refused capture untouched. Texture/RBO
// identities are still checked against live namespace-qualified dependencies
// AFTER this function. Only an observed pbuffer lifetime can name its default
// output; only the exact selected default output can seed a new consumer root.
inline bool qualifyGlProducerOutput(GlProducerRoute route,std::uintptr_t pbuffer,GlTextureUsage &usage){
    if(route==GlProducerRoute::Refuse)return false;
    if(!usage.defaultFramebuffer)return true;
    if(route==GlProducerRoute::NativePbuffer&&pbuffer){
        usage.writes[0]=GlColorResource::pbuffer(pbuffer,usage.nativeColorBuffer);usage.writeCount=1;return true;
    }
    if(route==GlProducerRoute::Selected){usage.writeCount=0;return true;}
    return false;
}
} // namespace BsiShaders
