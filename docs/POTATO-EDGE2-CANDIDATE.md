# Potato edge-refresh edge2 candidate

This source candidate is based on packaged v7 source `b450acc5304829b095052eec2751432c135ab7e7`. Its bridge capture identity is `gl-draw-write-footprint-v7-edge2`; the prior v7 control package is unchanged.

## Changes in this candidate

- Applies the v2 bounded producer-edge refresh policy.
- On a missed/busy/guarded transfer callback, retires provider-scoped pending evidence and invalidates the affected provider namespaces.
- On a rejected transfer descriptor, discards pending writes and invalidates the qualified namespace.
- Adds transfer refusal diagnostics and outermost handler-entry versus `observe()`-reached counts. These counters are diagnostic snapshots and do not participate in provenance or draw-selection decisions.
- Adds CPU-only copy/presentation correlation counters: typed resource names are compared only within the same tracked namespace and a two-provenance-frame window. Distinct presented inputs are tracked in a fixed 32-name table and distinct copy outputs in a fixed 16-name table; overflow is reported. The report contains aggregate counts only, never names or handles. No extra GL queries or locks are added.
- Retains the pending-copy-only draw-write inspection gate. It does not use a broad `hasPendingCopies() || hasEdges()` gate.

## Limits and evidence boundary

Unknown/skipped draw footprints remain an accepted limit: without an inspected footprint, no namespace-wide draw invalidation is inferred. The policy test verifies that any renewal is bounded by the original capture deadline and hop limit.

Same-target invalidation remains a production-path known limit. The isolated API test can explicitly call known-write invalidation, but the production draw-exclusion point has not been proven to have the exact destination identity needed to make that call. No same-target epoch bump is wired on that assumption.

Strict and ASan/UBSan tests and the v2-versus-candidate randomized differential trace are CPU/source checks only. They do not establish installed-player behavior, visual detail, GL timing, or player safety. The required installed comparison is untouched packaged v7 control versus this candidate, using the same machine, route, scene, and Off → Strong → Off sequence.
