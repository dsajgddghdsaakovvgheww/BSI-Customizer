#pragma once
#include "GlColorTransfer.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace BsiShaders {
// CPU-only diagnostic values. Never GL names, native handles, pixels or a
// permission to inspect a route. A shared code target may try several providers.
enum class GlCandidateStop { Qualified, Lifecycle, NoContext, Renderer, Source, Policy, ResourceHooks,
    Drawable, Dependency, Protection, Capacity, Count };
enum class GlTransferStop { Exception, Nested, Guarded, Busy, Policy, ResourceChanged, NoCandidate,
    Initialize, ResourceHooks, Descriptor, Output, Input, UnlinkedOutput, NativePending,
    RouteChanged, CurrentChanged, RecordedNoInputs, RecordedInputs, Count };
enum class GlTransferDescriptor { None, NotReady, Api, Arguments, MissingQuery, Framebuffer,
    ColorBuffer, Protection, Attachment, ColorFormat, Destination, NamedImage, Feedback, Count };
inline constexpr std::array<const char*,static_cast<std::size_t>(GlCandidateStop::Count)> glCandidateLabels{{
    "qualified", "lifecycle-not-ready-or-faulted", "no-current-context", "renderer-mismatch",
    "no-selected-source-for-provider", "policy", "source-retirement-hooks-unavailable",
    "foreign-or-unknown-drawable", "no-recent-resource-dependency", "protection-unproved", "context-budget"}};
inline constexpr std::array<const char*,static_cast<std::size_t>(GlTransferStop::Count)> glTransferLabels{{
    "exception", "nested-copy-alias", "inspection-guard", "render-lock-busy", "policy-or-source",
    "resource-epoch-changed", "no-qualified-candidate", "inspector-unavailable", "retirement-hooks-unavailable",
    "descriptor-refused", "output-unqualified", "read-pbuffer-or-feedback-unqualified", "output-not-recently-consumed",
    "native-completion-not-recorded", "route-revision-changed", "native-current-changed", "recorded-zero-inputs", "recorded-inputs"}};
inline constexpr std::array<const char*,static_cast<std::size_t>(GlTransferDescriptor::Count)> glDescriptorLabels{{
    "none", "not-initialized", "unsupported-api-or-protection", "unsupported-arguments", "missing-completeness-query",
    "incomplete-framebuffer", "unsupported-color-buffer", "protection-unproved", "unsupported-attachment",
    "non-normalized-or-missing-rgb", "destination-image-unqualified", "named-image-descriptor", "feedback"}};
inline constexpr std::array<const char*,3> glTransferApiLabels{{"blit", "copy-subimage", "named-copy"}};
struct GlTransferAttempt {
    GlColorTransfer::Kind api=GlColorTransfer::BlitFramebuffer;
    GlTransferStop stop=GlTransferStop::Exception;
    GlTransferDescriptor descriptor=GlTransferDescriptor::None;
    std::array<unsigned,glCandidateLabels.size()> candidates{};
    unsigned providers=0;
    std::uint64_t revision=0;
};
struct GlTransferAudit {
    std::array<std::uint64_t,glTransferApiLabels.size()> calls{};
    std::array<std::uint64_t,glTransferLabels.size()> stops{};
    std::array<std::uint64_t,glCandidateLabels.size()> candidates{};
    std::array<std::uint64_t,glDescriptorLabels.size()> descriptors{};
    // CODE attribution only: WGL-only, EGL-only, shared WGL+EGL/unknown mask.
    std::array<std::uint64_t,3> codeProviders{};
    void note(const GlTransferAttempt &attempt){
        const auto api=static_cast<std::size_t>(attempt.api),stop=static_cast<std::size_t>(attempt.stop);
        const auto descriptor=static_cast<std::size_t>(attempt.descriptor);
        if(api>=calls.size()||stop>=stops.size()||descriptor>=descriptors.size())return;
        ++calls[api];++stops[stop];
        for(std::size_t i=0;i<candidates.size();++i)candidates[i]+=attempt.candidates[i];
        if(attempt.stop==GlTransferStop::Descriptor)++descriptors[descriptor];
        ++codeProviders[attempt.providers==1?0:attempt.providers&&!(attempt.providers&1)?1:2];
    }
};
} // namespace BsiShaders
