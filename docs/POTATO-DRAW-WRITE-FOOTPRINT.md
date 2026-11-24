# Live Potato controls: separate raster-write invalidation from effect eligibility

## Requirement and honest status

The user explicitly selected **live Settings On/Off and 0–100 strength**, keeping
native **Save/No/Discard**. The unchanged tutorial DLL has persistent mutations
and **no supported live-control ABI**. Its [restart-only local-use guide](POTATO-ORIGINAL-DLL-USE.md)
is a fallback, not this feature. There is no fake slider over that DLL, unsafe
live unload or simultaneous original/independent hook installation.

The independently authored engine already carries the saved integer percentage
to draw-scoped private samplers and restores the original bindings after each
original draw. This change retains that implementation and native Settings
semantics; it repairs a specific metadata defect in one supported producer path.
It does **not** establish the tutorial's appearance in the user's game.

**Latest actual installed result remains negative:** v6 source `2e5e6a0…`,
5,950 frames / 11,663 NoSampler inspections / zero bindings. Its
[11,361 uncaptured-output retirements](POTATO-PENDING-COPY-LEDGER.md) do not reveal
which individual predicate failed. No game trace proves that the reproduction
below is the cause. The latest producer input was itself legal base-only
`960×540 GL_NEAREST`, not an eligible mipmapped material. Do not force mips,
filters or output scaling to turn that observation into apparent success.

Source: **`eafff51bb12f9551336393ef8d308b13f3393faa`**.
Capture identity: **`gl-draw-write-footprint-v7`**.
[Machine-readable evidence](evidence/potato-draw-write-footprint.json) separates
installed failure, the RED reproduction, implementation tests and remaining gaps.
No new diagnostic-only package or game-install request accompanies this work.

The user's later pasted cloud-analysis report supplied no new DLL/hash mapping
or source/control ABI. Its bias-versus-filter distinction is useful, but the
[hash-pinned originals](POTATO-ANSHU-REFERENCE.md) are five identities, including
MSI minimum-LOD variants—not a verified universal two-build/bias-only result.
Its generated-mip, forced-filter and render-scaling suggestions are not adopted.

## Reproduced defect, rather than a guessed game diagnosis

Previously, `GameDraw` used the same Boolean for two different questions:

1. Can an opaque triangle draw supply qualified sampled-input/producer evidence?
2. Have we completely captured the color outputs that could overwrite a queued
   copy's destination?

A blended, line, zero-mask, no-program or buffer-zero-NONE draw failed question
1. Its output was then treated as unknown for question 2, revoking **all** queued
copies in that known namespace—even if it targeted a different named FBO image.
Conversely, the positive `usage()` capture listed only draw buffer zero. That
partial list is not sufficient for invalidating possible MRT destination writes.

A tests-only commit **`eba9b612e4353d8673fa273f4848d4ec2b3a2827`** reproduced the
first case through **actual Windows MinHook** on authored WGL state. Run
[45105294220](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45105294220),
job `137465071411`, built successfully and failed precisely at preservation of
pending metadata after an unrelated blended draw. The later new cases were not
reached in that RED test. Existing non-split tests were otherwise 23/24 passing;
all nine split modes and transfer ABI also reported zero exit status. This is
not supplied-player execution or the game's resource graph.

## Narrow implementation and unchanged safety boundaries

- `GlDrawWrites` is a **different type** from `GlTextureUsage`: no sampled inputs,
  native resource identity, producer authorization, graph observation or TTL
  renewal can be obtained from it. It is consumed only for pending invalidation.
- Read-only `drawWrites()` enumerates **every enabled `DRAW_BUFFERi`**, resolves
  its actual selected color attachment, and records a whole typed texture/RBO.
  It ignores blend, primitive, shader eligibility, depth and color masks for
  invalidation. These can suppress writes, but cannot justify missing another
  enabled MRT target. All-NONE is a complete empty raster-color footprint.
- A known possible destination write still removes the old copy predecessor.
  A complete unrelated footprint can preserve already queued metadata, but
  cannot qualify the interposed draw or reconcile the copy. Only the later exact
  selected consumer can do that, through the unchanged namespace/resource policy.
- Unknown/incomplete outputs, invalid limits, absent queries, protection-capable
  attachments, any bound shader image or advertised ARB/NV bindless-image
  capability retain **namespace-wide conservative revocation**. No partial
  buffer-zero fallback is accepted. Image queries are capability-gated, bounded
  to 256 units; output slots remain bounded to 32. These are query bounds, not
  enlarged graph/lifetime/ownership budgets. The actual ANGLE fixture exposes
  96 image units, all queried legally without consuming/adding an error.
- Default outputs still require the exact selected or observed live unprotected
  pbuffer route. No pbuffer generation/color aspect or cross-context share is
  invented. The native-output flag can only invalidate an in-flight token.
- **In-flight writes remain unordered and conservative**, including unrelated
  and native writes. With no queued copy to match, the hook revokes any possible
  in-flight token directly, without the new output/image GL queries. No render
  lock or sampler lease is held across a transfer original.
- Same/observed-share namespace, provider/display/context, foreign-window and
  protection exclusions, resource serials/generations, selected source/settings
  revisions, original-entry two-frame TTL, eight hops and existing capacities
  remain. Unknown routes are not adopted; unobserved resource/storage aliases
  or general image/private-transfer graphs are not reconstructed.
- `usage()`/`begin()` retain their original positive qualification: this change
  does not let a line, blend or rejected material gain an override. Legal base-only
  LINEAR/NEAREST textures stay native. No generated mips, forced minification
  filter, source descriptor edits, final-image blur or downscaling is introduced.

The old v6 discard label `draw-output-or-primitive-unavailable` stays in historical
evidence. Current v7 uses `complete-draw-color-output-unavailable`; it indicates
failure of the **invalidation footprint**, not an identified primitive or proof
of a real GPU write. Ledger arrays/balance rules remain unchanged.

## Executed validation

- Strict local native CTest **11/11** and ASan/UBSan/leaks **11/11**, including
  software Vulkan controls. New cases cover every color slot, permuted slot-to-
  attachment mapping, typed name collisions, zero-mask MRT, all-NONE, partial
  failure, incomplete/protected/bindless/image/query-limit refusal and allocation-
  free/state-neutral capture. Separate CPU tests check queued retention versus
  adoption, destination/unknown removal, isolated namespaces, original expiry,
  and conservative in-flight revocation for unrelated/native/unknown writes.
- A deterministic CPU differential probe still matches the old policy over
  **128 traces / 65,536 operations / 2,055 admissions / 128 reconciliations** for
  the unchanged `GlTextureUsage` API. The new footprint API and hook behavior
  are intentionally additional and are **not** called equivalent by that probe.
- Actual local **ANGLE GLES 3.1 / SwiftShader** pixels: both sampler-object and
  texture-object sources, eight blend/line/mask/NONE interpositions, 0/50/100
  strength, unchanged one-level compositor, full-frame byte-exact Off, original
  source LOD/binding/state and pending-error preservation. Four buffer0/MRT3
  alias cases revoke instead of resurrecting an old predecessor. Existing
  spatial-material, 0/25/50/75/100, immutable/view, copy, surfaceless, nonsharing
  native-image and ten presentation-effect controls also pass. Six new local
  PNGs are independently decoded/CRC/hash checked in the evidence record.
- Optimized strict **Win64 object compilation** of production hooks/observer and
  authored fixture/ordering tests passes. This is not Windows execution.
- Focused Windows [45106272297](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45106272297),
  job `137468187403`: **SUCCESS** at the exact source above. The pinned successful
  step runs the expanded ordering/footprint, original in-flight overwrite/TTL/
  reset/Off, transfer ABI, audit, all nine split modes and existing contracts,
  propagating every failure. The API exposes nine split and transfer-ABI zero-
  exit notices, not a complete successful per-case transcript. RED failure text
  is available separately in check annotations; a log-storage fetch returned EOF.
- Full shader/Qt/native Settings/software-API matrix
  [45106460327](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45106460327):
  **SUCCESS, six jobs pass** at the same exact source; optional focused duplicate
  skipped. Actual Qt 5/Qt 6 plugin/host, physical native Settings Save/No/Discard,
  live percentage/revision, recovery and existing Windows contracts pass their
  failure-propagating steps. Three complete new software-pixel notices were
  independently parsed: **Mesa GLES, Mesa desktop GL and ANGLE**, each with
  eight excluded-draw interpositions, four conservative buffer0/MRT3 aliases and
  full-frame exact Off. This is not player execution or physical-GPU evidence.
  No packaging/release workflow was dispatched for this repair.

These tests do not execute supplied PF/player/provider/driver/tool binaries,
establish real game material pixels, measure a physical GPU/FPS improvement or
certify anti-cheat/account safety. The new observation is bounded GL state queries, not pixel
readback, telemetry upload or proprietary-binary instrumentation.

## Remaining acceptance

Keep the native shader update flow **Inspect compatibility → 2. Review update →
Install native shaders**; no bypass of review, unrelated proxy/EXE edits or
Settings Save/Discard is part of this repair. Original-DLL use remains separate.

Actual acceptance still requires repeatable same-scene **material Off → Strong
→ Off/native restoration**, with intermediate saved strength behavior. None of
an armed switch, repaired counter, reconciled edge, prepared private binding,
authored software image or successful CI/package substitutes for those game
pixels. Do not repeat the unchanged v6 diagnostic install or ask for more play
on it. No public release, merge, tag or proprietary redistribution is performed.
