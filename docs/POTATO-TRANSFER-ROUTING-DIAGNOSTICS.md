# Potato: v4 transfer-routing diagnostics and native ABI repair (historical)

> **Superseded by an installed v4 negative:** `ebdf6c2…` /
> `gl-transfer-route-ledger-v4` produced zero material bindings in 1,392 frames.
> Its ledger reports 184/185 copies passing candidate/descriptor checks but
> failing exact recent-output matching. This page preserves the v3 report,
> v4 ABI/diagnostic validation and old package identities; its downloads and
> "next check" below are historical, not a request to repeat that failed build.
> The later installed v5 also has zero bindings/reconciliations despite 160
> queued callback outcomes. See the [latest negative and v6 pending-copy lifetime diagnostics](POTATO-PENDING-COPY-LEDGER.md),
> and the [separately reproduced copy-order repair and v5 validation](POTATO-COPY-CONSUMER-ORDER.md).

## Installed v3 result that motivated v4: not working

The user tested module build **`00c1b7c1d4caf5ce1ac7e0ddaf08494f0aa6c45d`**,
capture **`gl-egl-lifecycle-alias-v3`**. The copied module identity confirms that
the [lifecycle-alias candidate](POTATO-EGL-LIFECYCLE-ALIASES.md) was reached. Do not
blame an older download, stale settings, insufficient play time or strength.
The exact installed DLL file hash was not supplied; that does not invalidate
the report or establish a stale installation.

| Observation | User's result |
| --- | --- |
| Observed game API / bridge / host | OpenGL / Qt 5.15.2 / Qt 5.15.4 |
| Processed presentation frames | 706 |
| Potato strength / inspection | 100% / armed |
| Prepared Potato draws / bindings | **0 / 0** |
| Sampler inspections / NoSampler | **330 / 330** |
| Presentation observations / color inputs | 330 / 330 |
| Linked producer inspections / bindings | **0 / 0** |
| Qualified transfer observations / linked inputs | **0 / 0** |
| Pre-route draw / transfer callbacks | 19,379 / 329 |
| Pre-route named copies / native image binds | 0 / 0 |

The inspected texture is mutable RGB `256×256`, internal format `0x1907`,
BASE/MAX `0/1000`, effective minification **`GL_LINEAR` (`0x2601`)**. Its first
blocker is non-mip filtering, not missing required mips or depth. This legal
base-only input remains unchanged. Presentation grading being active does not
mean Potato was applied, and the large pre-route count is not an upstream
material-ownership trace.

The 329 counted transfer callbacks are in the existing blit/subimage family,
not the separately counted named-copy/native-image-binding categories. They
can include nested aliases or unrelated contexts. Their count does **not**
establish 329 distinct successful game copies, which entry point was used,
why capture failed, or a missing private route. Vulkan has loader/instance/device
activity but **zero Win32 surfaces, swapchains or presents**; it is not a newly
observed Vulkan game route.

## 1. What can be fixed and what is still unknown

There is no proved game-resource repair from this report. Existing code stops
before publishing a transfer for several independent reasons, but the old raw
counter does not distinguish them. Silently treating all pre-route callbacks as
owned material work, merging provider namespaces or forcing mips on the
presentation texture would manufacture scope, not diagnose it.

The follow-up adds **bounded, CPU-only routing diagnostics** to the already
hooked GL blit/subimage/named-copy paths. It does not add entry-point discovery,
GL queries, image readback, new texture eligibility or a private ABI. Native
image bindings retain their separate existing counters and are explicitly
excluded from this ledger.

### One terminal outcome and separate provider-candidate checks

The copied status now contains:

- **APIs:** blit, copy-subimage, named-copy;
- **terminal outcomes:** nested alias/inspection guard, lock contention,
  policy/source, resource epoch, no candidate, unavailable inspector/retirement
  hooks, descriptor/output/read-pbuffer/feedback refusal, unlinked output,
  changed native current/route, or metadata recorded with/without inputs;
- **provider-candidate checks:** no current context, renderer mismatch, no selected
  source for that provider, lifecycle not ready/faulted, policy, unavailable
  retirement coverage, foreign/unknown drawable, no recent resource dependency,
  unproved protection or context budget;
- **descriptor refusals:** existing completeness/color-buffer/protection/
  attachment/component/destination/argument/named-query/feedback checks;
- **code-target provider masks:** WGL-only, EGL-only or mixed/unattributed. These
  masks identify hook-code attribution, **not resource ownership or actual DLL
  identity**, and never merge GL namespaces.

Several provider candidates can be tried for one shared code address. Their
checks are recorded separately, not summed as separate copies or reduced to a
misleading arbitrary last rejection. A qualified candidate is only a prefilter:
its descriptor and resource path must still pass. One callback outcome is
recorded after the native call returns; in-flight and reset-stale callbacks can
be absent. These totals are not a permanent partition of all process activity.

Counters reset on the same source/review/strength/Off changes as existing Potato
evidence. Entry and report share a revision token; old callbacks cannot populate
a new settings interval. Only fixed enum counts are stored. Clipboard formatting
stays on the Qt/UI side; render callbacks store no Qt objects, raw handles,
texture names, module paths, pixel data, persistent logs or uploads. No additional
query is made merely to explain a foreign or unowned route.

### Separately reproduced native ABI repair

Before production changes, tests-only source
**`ac9dc3c4acad34fa1ed35b00a9613d0cc9c20abb`** keeps
`PresentationHooks.cpp` byte-identical to the delivered lifecycle candidate:
SHA-256 `784845177c6cfdf97bee4a6f1a018b3dcf94d641d7e1521fa704431402671c41`.

[Windows before run 45048102298](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45048102298)
has all nine split-EGL modes passing and **21/22 non-split tests passing**. The
only new failure is:

> transfer inspection must preserve the native original's INCOMING LastError

The authored fixture deliberately changes Win32 LastError during allowed state
queries, independently records the incoming original value and sets an outgoing
sentinel. This demonstrates an actual callback transparency bug; it is **not**
a demonstration that the game or its driver relies on LastError or that this
caused the missing links.

The three GL copy wrappers now restore the exact incoming LastError immediately
before their own native original. They save the exact outgoing value and restore
it after capture **and diagnostic destruction**. Every native argument and
once-only call remains unchanged. No render/Runtime lock, context/session pointer
or sampler lease is kept across the native original. Resource/route/current/
revision checks still precede optional publication after native completion.

## 2. Validation

Initial repair/diagnostic source:
**`46ce6cf96911c7a1991ce38c4b0e6a3eb85c52a5`**.
The corrected candidate is
**`ebdf6c219864c5f7352a1b0bb3fc1ea0ad0ca486`**.
Capture schema: **`gl-transfer-route-ledger-v4`**.

- [Focused Windows run 45048681508](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45048681508):
  **SUCCESS**. Dedicated transfer ABI/ledger contract, all nine split modes and
  existing non-split suite pass. Other matrix job groups are intentionally
  skipped, not full-matrix successes. The new contract checks all three transfer
  families, exact arguments/once-only originals, incoming/outgoing LastError,
  positive typed resource links, foreign/no-current no-query refusals, legal
  LINEAR preservation, joined-native-worker settings invalidation and Off.
- Strict local native CTest **9/9**, and ASan/UBSan/leak CTest **9/9** pass.
  Vulkan pixel execution is **off** in these local runs; this is not the previous
  Vulkan-on 10/10 result. Portable tests cover bounded enum aggregation, separate
  candidate reasons, concurrent reports, descriptor refusal categories,
  allocation-free metadata, state preservation and stale-epoch/Off rejection.
  The corrected routing test also passes an independent local Clang UBSan run
  with `-fno-sanitize-recover=all` and `UBSAN_OPTIONS=halt_on_error=1`.
- Strict Win64 C++17 `-O2 -Wall -Wextra -Werror` object compilation passes for
  production hooks, runtime, inspector, new Windows test and fixture. This is
  cross-compilation, not Windows execution. Source-only public-Qt/no-private-
  renderer boundaries and redistribution-notice checks pass.
- The first full attempt
  [45048873578](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45048873578)
  is **FAILURE**, not a full success: five jobs, including all three Windows
  native/Qt/UI configurations, pass. Mandatory Clang UBSan catches the new
  diagnostic bounds test constructing native `GlColorTransfer::Kind(999)`, an
  out-of-range non-fixed enum, before it can be validated. Pixel generation is
  skipped after that failure; its missing upload is a consequence, not another
  game-renderer diagnosis. In `ebdf6c2`, the diagnostic API index is stored as
  unsigned and validated before indexing. The `999` negative test remains;
  UBSan is not disabled and native GL operation contracts are not widened.
  Package attempt `45048876964` was cancelled as superseded, not counted as a
  completed package or offered for download.
- Corrected [full matrix 45049696867](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45049696867)
  at **`ebdf6c2…`**: **completed / SUCCESS, six full-matrix jobs pass**; the
  optional focused-only duplicate is intentionally skipped. All registered
  native CTests, including the new transfer contract and nine split modes, run
  in the successful unfiltered Windows steps on jobs `137291887302` (Qt
  5.15.2 → independent Qt 5 host), `137291887228` (Qt 6.2.4 → 6.7.3) and
  `137291887377` (Qt 6.2.4 → 6.8.3). The actual loaded-plugin/physical UI,
  Save/Discard, native theme and selected-tab-glass tests pass. Linux independent
  Qt 5, pinned-player source/inventory and software graphics jobs pass too.
- All **eight complete software-pixel JSON notices** are parsed and checked:
  production ANGLE GLES / Mesa desktop GL / Mesa GLES, actual surfaceless
  producers, named image copies and the independent ANGLE native-pbuffer-image
  control. Material strengths 0/50/100 retain red/cyan/yellow at alpha173;
  existing legal LINEAR compositors get zero replacements. Full-frame Off,
  source state, pending-error and revision/expiry assertions remain required.
  These are authored software-driver scenes, **not the Windows detours or
  installed game**. New hosted PNGs and full per-case Windows CTest transcripts
  were not downloaded into this Linux workspace; complete check notices and
  successful unfiltered test steps are retained instead.
- [Nonpublishing package 45049699287](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45049699287)
  at the **same corrected source**: **completed / SUCCESS**, Windows job
  `137291894740`. Both Qt module baselines, application/portable tests, manifest,
  updater acceptance, real installer install/upgrade/uninstall and both actual
  uploaded-ZIP download/CRC/hash/Windows-extraction checks pass. Public release
  and Discord publication are **skipped**. No public release/tag is created.

Machine-readable installed report, before/after assertions, source hashes,
validation stages and package identities:
[`evidence/potato-transfer-routing-diagnostics.json`](evidence/potato-transfer-routing-diagnostics.json).

Sampling/mip-selection policy, provider/resource/share/lifetime/protection/color/
route-revision/TTL boundaries are not relaxed. The new descriptor reporting
reuses existing checks. Unchanged authored/software behavior is not installed-
game texture or physical-GPU/FPS acceptance.

## 3. Verified v4 test downloads

The version label remains **4.0.1**. These artifacts are built from
**`ebdf6c219864c5f7352a1b0bb3fc1ea0ad0ca486`**, run **`45049699287`**, attempt 1;
older v3 downloads do not contain this ledger. Identify the package by
**source/run/artifact**, not by the version alone. Subsequent documentation-only
commits are not the build source.

| Build | Verified artifact | ZIP bytes | ZIP SHA-256 |
| --- | --- | ---: | --- |
| Portable x64 | [BSI-Customizer-4.0.1-win64-45049699287-r1](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45049699287/artifacts/14766155722) | 43,754,697 | `d76f833e185a3353571f78ca6bffab70087c773876063e2496973ed63719809c` |
| Optional setup | [BSI-Customizer-4.0.1-setup-45049699287-r1](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45049699287/artifacts/14766190567) | 46,049,748 | `4abc4f135d0be3c1170ea88f0f419a783c90d8e4fd66076307008a6420492085` |

Exact single EXE SHA-256 after Windows extraction:

- `BSI-Customizer-4.0.1-win64.exe`:
  `8def7bd5d57c1fcda47e28314412b83e94d4ed858dd7b72af3512c2256a50771`.
- `BSI-Customizer-4.0.1-setup.exe`:
  `03c60b1bfd27af0fe034b16530bdb962e54a50b970a6c216538bf90ef4ca6746`.

Both actual uploaded ZIPs were downloaded, CRC/full-ZIP/exact single-EXE hashes
checked and extracted/hash-checked again with Windows `Expand-Archive` **in the
hosted Windows build job**. Artifact metadata, source, byte sizes, ZIP digests
and complete readback/extraction notices were independently reconciled here;
neither ZIP nor EXE was downloaded/executed in this Linux workspace. GitHub
requires artifact access and retains these test artifacts briefly (configured
retention three days; reported expiry `2026-11-25T17:48:04Z` portable /
`2026-11-25T17:48:07Z` setup, as returned by GitHub). Package integrity and
software pixels are not game acceptance. PR 121 remains **draft, open,
unmerged and unreleased**.

## 4. Next useful installed check

This is a **diagnostic candidate plus native ABI repair, not a claimed Potato
material-effect fix**. Close all instances in a backed-up authorized test
installation and use:

**Inspect compatibility → 2. Review update → Install native shaders.**

Updating the Customizer EXE alone does not replace the installed native DLL.
The copied header must identify this source and `gl-transfer-route-ledger-v4`.
At 100% strength, reproduce the same scene and **copy diagnostics before changing
strength or turning Off**: those actions reset the interval. Return the header
and complete **Copy/blit routing ledger**, including provider-candidate and
descriptor rows. For material acceptance, compare the same scene **Off → Strong
→ Off** and retain the actual material images; a saved value or a grading change
is not sufficient. No extra installer, PD/Android disk/account upload, lower
resolution, driver override or protection change is requested. Do not force an
incompatible module or disable anti-cheat.

If the new ledger cannot establish a supported resource path, keep it native and
keep the game result **not working**. Even a recorded input is only metadata;
completion still requires actual material-texture change and correct native Off,
not counters, status, grading or a saved strength value.
