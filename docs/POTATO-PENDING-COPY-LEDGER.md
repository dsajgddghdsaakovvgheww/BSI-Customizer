# Potato: installed v6 still not working; pending-copy diagnostic history

## Latest installed result: v6 is also negative

The user has now tested **`2e5e6a0…` / `gl-pending-copy-ledger-v6`** in the game:
**5,950 processed frames / 11,663 NoSampler inspections / zero Potato bindings**.
Two deferred edges reconciled and two producer draws were inspected, but neither
prepared a binding. The latest producer input is base-only `960×540` RGBA8 with legal
`GL_NEAREST` (`0x2600`), BASE/MAX 0/0, still first-rejected as non-mip-filter.
This fourth installed negative supersedes the untested-v6 delivery status below.

The new interval balances: **11,630 queued = 11,361 uncaptured-output + 256
captured-destination-write + 10 missed/guarded/busy + one sharing change + two
reconciled**, pending zero, peak two. All 11,361 uncaptured removals are at
`draw-output-or-primitive-unavailable`; all queued sources are renderbuffers.
This identifies the dominant conservative **metadata removal code path**, not
a proven write, unsupported primitive, missing hook or game root cause.
No hop/resource-budget retries were reported. Counters are not game pixels.

[The transcribed installed report](evidence/potato-installed-v6.json) is kept
separate from the successful implementation tests below. At the user's request,
the next direction is the [exact tutorial DLL via the existing, explicitly
reviewed restart-only route](POTATO-ORIGINAL-DLL-USE.md), not another unchanged
v6 install or longer gameplay request. No new independent game repair is claimed.

## Previous installed result: v5 was not working

The user installed **`91f4efbe93c0360233d4e892d11949f3d8c9fc43`** /
**`gl-copy-consumer-order-v5`**. Its supported-path ordering and original-entry
lifetime repairs passed the [separate Windows/software tests](POTATO-COPY-CONSUMER-ORDER.md),
but **did not fix the user's game**. This is the third installed negative
(v3, v4, v5), not a reason to repeat an unchanged install or request more play time.

| Installed v5 observation, On interval | Result |
| --- | ---: |
| Requested / observed API | Automatic / OpenGL |
| Module Qt / host Qt | 5.15.2 / 5.15.4 |
| Processed presentation frames / strength | 268 / 100% |
| Policy / inspection | Owned mip compatibility / armed |
| Inspections / NoSampler | 162 / 162 |
| Prepared bindings / resource-linked producer inspections | **0 / 0** |
| Immediate qualified transfers / deferred reconciliations / deferred inputs | **0 / 0 / 0** |
| Selected presentation observations / color inputs | 162 / 162 |
| Raw draw / copy-subimage callbacks | 10,624 / 162 |
| Named copies / native image bindings | 0 / 0 |
| Terminal `awaiting-exact-copy-consumer` | **160** |
| Terminal `no-qualified-candidate` | 2 |
| Candidate qualified / no recent dependency | 160 / 2 |
| Descriptor refusals | 0 |

All 162 copy code targets were attributed WGL-only. This identifies the code-hook
provider mask, **not a resource namespace, DLL identity or ownership**. The 160
queued callback outcomes are **not 160 distinct, still-live, successful copies**.
They establish metadata admission at native return, not subsequent consumption
or a material effect. Legal `GL_LINEAR` on the mutable RGB `256×256` compositor
input (BASE/MAX 0/1000) must not be forced to use mips.

A second snapshot at 641 processed frames is Off, with interval counters reset
and saved strength 100. That reset is expected; it neither cancels the preceding
On failure nor proves image-level native restoration. Vulkan loader/instance/
device activity is not a Vulkan presentation: Win32 surfaces, swapchains and
presents remain zero. Neither actual game material images nor an installed-DLL
checksum were supplied. The missing checksum does **not** invalidate the failure.

**Unknown:** whether pending entries expire, are overwritten/revoked, lose a
lifecycle/namespace, encounter a graph bound, or simply lack an observed exact
consumer. The v5 counters cannot distinguish these. No new game root cause or
behavioral fix is claimed.

## Narrow diagnostic change

Source: **`2e5e6a0b726cf84dbc36c8ee5d5dcf8dce2c4d19`**.
Capture schema: **`gl-pending-copy-ledger-v6`**.

This change accounts for **already captured CPU metadata only**. It does not add
a GL query/readback, collect names/handles, upload a log, adopt a new route,
relax matching or extend any evidence lifetime.

- `queued`, `pending`, `peak`: cumulative admitted metadata and current/high-water
  gauges in one source/settings interval. The fixed pending capacity is still 32.
- Each removed queued edge contributes exactly **one** retirement reason:
  reconciled; original deadline expired; captured destination write; uncaptured
  callback output; missed/guarded/busy callback; context/provider/resource/native
  source retirement; successful sharing change; evidence-clock reset;
  namespace loss; or replacement by newer copy metadata.
- Uncaptured-output retirements are partitioned by their existing draw-candidate,
  draw-inspector, draw-output/primitive, transfer-candidate, transfer-inspector,
  transfer-descriptor or native-image-binding site. These are removed **edges**,
  not counts of every failed callback or proof those outputs actually changed.
- Selected default-FBO consumer checks distinguish no pending entries, pending
  only in other namespaces, same-namespace/no exact destination, and exact
  destination. Names are compared **only within the same/observed-share space**;
  another namespace is not tested for equal numeric names. One check may concern
  several pending entries, so these categories do not count distinct copies.
- Typed queued source counts distinguish textures, renderbuffers and native
  pbuffer lifetimes, not material semantics.
- Hop/resource-budget observations count reconciliation **retries**, which may
  repeat for one still-pending entry. They are not retirements or native calls.

Retirement reasons identify the existing code path that conservatively removed
metadata, not a driver-confirmed write or a causal trace of game textures.
Provider-wide resource/lifecycle invalidation may remove other pending entries,
and a captured destination write can itself be a later submitted copy. Do not
infer a missing hook, a driver failure or actual material semantics from a label.

Within one interval:

```
queued = pending + sum(all retirement reasons)
queued = sum(typed queued source counts)
uncaptured-output retirements = sum(uncaptured-output retirement sites)
```

The audit checks balance and bounds before publishing cumulative snapshots.
Runtime accepts only a current, nonzero GL source revision and strictly newer
snapshot sequence. Snapshots are **replaced, never summed**. Off, strength,
policy, review or selected-source changes reset the whole interval; old-source
retirements are not imported into the new interval. In-flight events can be
absent from a clipboard snapshot. Capture before Off/reset.

Publishing occurs under existing serialization and ends before a native copy
original. It holds no render/Runtime lock, session pointer, inspector guard or
sampler lease across that original. Existing native arguments/LastError,
provider/display/context/share/resource generation, protection/color and route
checks, **entry-time two-frame deadline**, eight hops, 64 contexts and 128
resources remain intact. Unknown/unsupported routes keep native behavior.
General EGLImage/native-buffer/private transfers remain unsupported/unproved.

## Validation and delivery status

At this source, local strict native CTest and GCC ASan/UBSan/leak CTest each pass
**11/11**, including SwiftShader Vulkan pixels and the new CPU audit contract.
Clang UBSan audit and strict optimized Win64 object compilation of hooks,
Runtime and the ordering test pass. Cross-compilation is not Windows execution.
The source-only public Qt boundary, redistribution notices, Python syntax and
patch whitespace checks pass.

The CPU tests cover queue/retirement/source conservation, all consumer
categories, each discard-site index, original TTL, overwritten/missed/lifecycle/
sharing/reset exits, capacity and hop/resource-budget refusals. Runtime tests
cover invalid/impossible snapshots, non-additive out-of-order/concurrent reports,
stale revisions, Off/zero strength, unobserved sources and non-GL renderers.
The expanded actual Windows hook test checks immediate queue publication,
consumer mismatch/exact reconciliation, overwrite/uncapturable-primitive/expiry/
resource/native-source retirement publication, native ABI and Off.

The independent local ANGLE software tests retain actual-copy material pixels,
zero LINEAR-compositor replacements, both sampler/texture source modes and
full-frame byte-exact Off, along with the existing native-image and material
controls. Six local copy-order PNGs independently pass CRC/decode, full-frame
native/Off byte equality, native/Strong inequality and exact alpha checks.
This is a regression check on authored software resources, **not the installed
game, a physical GPU or FPS acceptance**.

The CPU audit and concurrent Runtime/revision tests also each pass 100 repeated
executions. A separate deterministic CPU differential probe compares v5 and v6
provenance decisions over 128 traces / 65,536 metadata operations, with 2,055
admissions and 128 reconciliations: observable namespace/resource/producer
policy outputs remain identical after every mutation. This bounded authored
probe is not exhaustive or evidence of the game's graph. Diagnostic sequence
and accounting outputs are intentionally new.

- [Focused Windows 45097584475](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45097584475),
  job `137440749209`: **SUCCESS** at the exact source above. Its pinned workflow
  runs all nine split-dispatch modes, transfer ABI, expanded ordering contract,
  new CPU audit and the non-split suite, propagating failures. The API exposes
  explicit zero-exit notices for the nine split modes and transfer ABI. Later
  individual success notices were not returned. The successful pinned step and
  its source were read back; the log-storage redirect returned EOF, so no full
  per-case successful Windows transcript is claimed here.
- [Full matrix 45097773932](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45097773932):
  **SUCCESS, six jobs pass**, optional focused duplicate skipped. Three Windows
  Qt 5/Qt 6 configurations run all registered native contracts in successful
  unfiltered CTest steps and load the actual plugin/UI. The v6 metric keys,
  zero-initialized gauges, labels, clipboard caveats and capture identity pass the
  real bridge test. Native Settings/Save/No/Discard, independent Linux Qt 5,
  pinned-player static inventory/source and software graphics controls pass.
  Eleven complete hosted software-pixel JSON notices were parsed, including
  three copy-before-consumer paths (Mesa GLES, desktop GL, ANGLE) in both source
  modes. They preserve producer binding 1, compositor 0 and full-frame exact Off.
  Hosted PNGs and successful per-case Windows transcripts were not downloaded
  into this workspace; complete bounded notices/step metadata were read back.

- [Nonpublishing Windows package 45097774113](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45097774113),
  job `137441326644`: **SUCCESS**. Genuine Qt 5/Qt 6 native payloads,
  portable/application tests, administrator manifest, updater, actual setup
  install/upgrade/uninstall and both uploaded-ZIP download/CRC/hash/Windows
  extraction checks pass. Release and Discord publication steps are **skipped**.

[Machine-readable evidence](evidence/potato-pending-copy-ledger.json) separates
the reported installed-v5 failure from diagnostic implementation validation.
It records exact source hashes, completed CI job/step/annotation readback,
eleven hosted software-pixel notices, six independently decoded local PNGs and
reconciled package identities. Historical v5 evidence remains unchanged.
No public release/tag is created; PR 121 remains draft/open/unmerged.

## Verified v6 diagnostic downloads

Version remains **4.0.1**. Identify **source `2e5e6a0…` / run `45097774113` / r1**,
not version alone. These are diagnostic candidates, **not a promised effect fix**.
Both actual uploaded ZIPs were downloaded, CRC/hash-verified and extracted in
the hosted Windows package job. Artifact metadata/digests and complete bounded
verification notices were independently reconciled here. ZIPs/EXEs were not
downloaded or executed in this Linux workspace.

| Package | Exact artifact | ZIP bytes | GitHub-reported expiry |
| --- | --- | ---: | --- |
| [Portable x64](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45097774113/artifacts/14784423538) | `BSI-Customizer-4.0.1-win64-45097774113-r1` | 43,784,869 | `2026-11-26T02:19:41Z` |
| [Optional setup](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45097774113/artifacts/14784742432) | `BSI-Customizer-4.0.1-setup-45097774113-r1` | 46,096,051 | `2026-11-26T02:19:43Z` |

| File | SHA256 |
| --- | --- |
| Portable ZIP | `86196fc9ad54380ecc688553d27870f6a7f090f350d556a88af5b8623bf79aa1` |
| Portable EXE | `9aa394b5e0d88a416ad3d9b36eb71b0eec73bd5e6c4480ea04e1ec3bff639ea6` |
| Setup ZIP | `e2df4611a960762f9a191c4eda5aacd1ecf08aa0f25c505264cde0f116130f02` |
| Setup EXE | `93a4bc98493e937f1844b678b6e2f8f8600453264457d4b7736a76fcc178935d` |

## Superseded diagnostic-install request

The v6 package above was validated as a diagnostic implementation, then tested
by the user with the negative result at the top of this page. **Do not reinstall
unchanged v6 or request longer play.** Existing implementation/package evidence
remains historical and unchanged; it does not establish game acceptance.
Do not loosen TTLs, force LINEAR mips, merge guessed namespaces or substitute
presentation blur/resolution on the basis of this ledger.

The user's requested alternative is documented in
[exact original DLL use](POTATO-ORIGINAL-DLL-USE.md), using the already available
DLL patcher with unchanged local original bytes and explicit startup-risk review.
It is separate from the BSI sampler feature and does not provide its live slider
or Off/restoration contract. No new diagnostic package is needed for that route.

Acceptance of the independent sampler feature still requires actual same-scene
material **Off → Strong → Off** with native restoration. Original-DLL process
exit/restart is a separate comparison, not an in-place Off test. Neither a
balanced ledger, a reconciliation, a private binding, authored pixels nor a
successful package can substitute for actual game material changes.
