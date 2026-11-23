#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace BsiShaders {
// CPU-only accounting of already captured metadata. No GL query, name, handle,
// pixel, ownership decision or permission to inspect a new route lives here.
enum class GlDeferredCopyExit : unsigned { Reconciled, Expired, DestinationWrite, UnknownWrite,
    MissedCallback, ContextRetired, ProviderRetired, ResourceRetired, NativeSourceRetired,
    ShareChanged, EvidenceReset, NamespaceLost, Superseded, Count };
enum class GlDeferredConsumerStop : unsigned { NoPending, OtherNamespace, NoDestination, ExactDestination, Count };
enum class GlDeferredDiscardSite : unsigned { Unspecified, DrawCandidate, DrawInspector, DrawWrites,
    TransferCandidate, TransferInspector, TransferDescriptor, NativeImageBinding, Count };
inline constexpr std::array<const char*,static_cast<std::size_t>(GlDeferredCopyExit::Count)> glDeferredExitLabels{{
    "reconciled", "original-deadline-expired", "captured-destination-write", "uncaptured-callback-output",
    "missed-guarded-or-busy-callback", "context-retired", "provider-retired", "resource-retirement-or-lost-coverage",
    "native-source-retired", "observed-share-change", "evidence-clock-reset", "namespace-lost", "newer-copy-replaced"}};
inline constexpr std::array<const char*,static_cast<std::size_t>(GlDeferredConsumerStop::Count)> glDeferredConsumerLabels{{
    "no-pending-copy", "pending-only-in-other-namespaces", "same-namespace-no-destination-match", "exact-destination-match"}};
inline constexpr std::array<const char*,static_cast<std::size_t>(GlDeferredDiscardSite::Count)> glDeferredDiscardLabels{{
    "unspecified", "draw-candidate-refused", "draw-inspector-unavailable", "complete-draw-color-output-unavailable",
    "transfer-candidate-refused", "transfer-inspector-or-hooks-unavailable", "transfer-descriptor-refused", "native-image-binding"}};
inline constexpr std::array<const char*,4> glDeferredSourceLabels{{"texture", "renderbuffer", "native-pbuffer", "other"}};
struct GlDeferredCopyAudit {
    // One cumulative snapshot per source/review interval, not additive deltas.
    // sequence orders snapshots; only pending/peak are gauges. Source/settings
    // reset discards the entire old interval, as do the existing Potato totals.
    std::uint64_t sequence=0,queued=0;
    unsigned pending=0,peak=0;
    std::array<std::uint64_t,glDeferredExitLabels.size()> exits{};
    std::array<std::uint64_t,glDeferredConsumerLabels.size()> consumers{};
    std::array<std::uint64_t,glDeferredDiscardLabels.size()> discardSites{};
    std::array<std::uint64_t,glDeferredSourceLabels.size()> sources{};
    // Candidate-only CPU name correlation. Names/handles are never serialized;
    // only bounded aggregate counts leave the provenance owner.
    std::uint64_t presentedInputObservations=0,presentedInputDistinct=0,presentedInputUntracked=0;
    std::uint64_t copyObservations=0,copyInputRecentPresentedMatches=0,copyOutputRecentPresentedMatches=0,copyInputUntracked=0;
    std::uint64_t presentationInputsRecentCopyInputMatches=0,presentationInputsRecentCopyOutputMatches=0;
    std::uint64_t copyOutputDistinct=0,copyOutputUntracked=0,copyOutputFrames=0,copyOutputMaxFramesPerName=0;
    // Retry observations, NOT retired edges: an edge can remain pending and
    // encounter a bound on more than one reconciliation sweep.
    std::uint64_t hopBlocked=0,resourceBlocked=0;
    bool balanced()const{
        std::uint64_t retired=0,origins=0,discarded=0;
        for(const auto count:exits)retired+=count;
        for(const auto count:sources)origins+=count;
        for(const auto count:discardSites)discarded+=count;
        return queued==retired+pending&&queued==origins
            &&discarded==exits[static_cast<std::size_t>(GlDeferredCopyExit::UnknownWrite)];
    }
    bool valid()const{return sequence&&pending<=peak&&peak<=32&&peak<=queued&&balanced();}
};
} // namespace BsiShaders
