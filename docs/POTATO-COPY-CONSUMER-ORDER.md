# Potato: copy-before-consumer repair — game acceptance still required

## 1. Latest installed game result: v4 is not working

The user tested **`ebdf6c219864c5f7352a1b0bb3fc1ea0ad0ca486`** /
**`gl-transfer-route-ledger-v4`**. This is a second confirmed negative, not an
assumption about a stale install. The earlier `00c1b7c…` / v3 negative and the
v4 download/ABI history remain in the
[transfer-routing report](POTATO-TRANSFER-ROUTING-DIAGNOSTICS.md).
No installed DLL file checksum or material-image triplet was supplied; neither
absence invalidates the reported failure.

| Installed v4 observation | Result |
| --- | ---: |
| Game API / bridge Qt / host Qt | OpenGL / 5.15.2 / 5.15.4 |
| Processed presentation frames | 1,392 |
| Strength / sampler inspections / NoSampler | 100% / 186 / 186 |
| Prepared Potato bindings / producer inspections / qualified transfers | **0 / 0 / 0** |
| Presentation observations / color inputs | 186 / 186 |
| Raw draw / copy-subimage callbacks | 10,708 / 185 |
| Named copies / native image bindings | 0 / 0 |
| Copy callbacks passing candidate checks | 184 |
| Terminal `output-not-recently-consumed` | **184** |
| Terminal `no-qualified-candidate` | 1 |
| Candidate `no-recent-resource-dependency` | 1 |
| Descriptor refusals | 0 |

All 185 copy code addresses were attributed WGL-only. That is **code-target
attribution, not ownership**, a module-identity proof, 185 distinct successful
native copies, or evidence of a missing hook. Candidate qualification is only a
prefilter. The inspected mutable RGB `256×256` input still uses legal
`GL_LINEAR` (`0x2601`), BASE/MAX `0/1000`; it must not be forced to use mips.
The different capture intervals are not performance measurements.

**What the game report proves:** the dominant refusal is exact recent-output
matching, not the existing descriptor checks. **What it does not prove:** that
those destinations are later consumed, that they contain material textures, or
that copy/consumer ordering is their actual cause. The game's resource mismatch
remains unproved.

## 2. Independently reproduced defect

A separately authored Windows/MinHook regression uses a selected native child
window, a known pbuffer lifetime, legal LINEAR compositor input and mip-filtered
material samplers. It does not load PF binaries or represent real driver pixels.

Tests-only **`52bfa99886d82c2d20c21ef44f8fcf32844382ad`** leaves production
unchanged. `PresentationHooks.cpp` SHA-256 is
`d5d4dc1adcae623bb16020d21d0581c72f38500990c755505824b262e42d8f97`.
[Baseline run 45090182850](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45090182850),
Windows job `137417740822`, is **expected RED**:

- Consumer → copy into the already-consumed output → material producer: two
  private bindings inside the native draw original (existing positive control).
- Copy into a new, unconsumed sibling → material producer: zero bindings
  (required no-adoption control).
- Later consumption of that **exact new destination** → material producer:
  expected two bindings, actual **zero**. The earlier copy was discarded.

The nine split-dispatch modes, transfer ABI contract and 22 other non-split
contracts pass. Assertions after the failure were not reached. This reproduces
an ordering defect in the supported authored path; it does **not** establish
why the user's 184 destinations missed matching.

## 3. Bounded repair, not broader texture eligibility

Candidate source: **`91f4efbe93c0360233d4e892d11949f3d8c9fc43`**.
Capture schema: **`gl-copy-consumer-order-v5`**.

Only an already-supported base-2D `CopyTexSubImage2D` can retain pending
metadata. Existing current-provider/display/context, route, descriptor,
protection, color, pbuffer-generation and resource-retirement checks still
apply. Sampling/descriptor qualification and native candidate qualification
remain unchanged from the RED baseline.

1. Capture value identities plus the **entry-time** deadline/revocation stamp.
   Release the inspection guard and lock before the native original.
2. Call that original exactly once with unchanged arguments. Preserve incoming
   and outgoing Win32 LastError. Do not keep a Runtime/render lock, session
   pointer or private sampler lease across it.
3. After return, revalidate native current/drawable, source/review revision and
   resource serial. A still-unconsumed output may occupy one of **32 fixed
   pending slots**. This creates no namespace, producer ownership, private
   binding, or qualified-transfer count.
4. Only an exact subsequent consumer in the same or observed-successfully-shared
   namespace may reconcile that submitted predecessor. Keep the lesser of the
   consumer and original-copy deadlines, and the existing **eight-hop bound**.
   Context capacity 64, resource capacity 128 and two-frame lifetime are not
   enlarged. Capacity/unsupported/unproved paths retain native behavior.
5. Expiry, source/settings reset, sharing changes, context/provider retirement,
   typed input/output deletion, destination overwrite or native pbuffer
   generation retirement revoke the relevant pending metadata. An interleaved
   callback write or missed guarded/busy callback also conservatively revokes
   an **in-flight, not-yet-queued** capture. No extra GL query is made to infer
   a missed output.

The CPU ledger distinguishes `awaiting-exact-copy-consumer` and
`pending-copy-budget` from qualified transfers. Separate epoch-gated
`textureDeferredCopies` / `textureDeferredInputs` report reconciliations, not
additional native calls, driver success, material changes or FPS. The immediate
transfer counts retain their meaning. No names/handles/pixels are uploaded.

### Additional entry-time lifetime regression

Initial source `60e2939…` passed its focused Windows contracts in
[45091783744](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45091783744).
Before packaging, an added boundary test deliberately advances a presentation
frame **inside** the native copy, then consumes the destination at the original
inclusive deadline. Tests-only **`b20fbe4502e854ae4a1b2a6481b2d5386799f5db`**,
[run 45092285585](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45092285585),
job `137424320571`, is **RED**: one frame beyond the original deadline, expected
zero private bindings, actual **two**. This is a genuine intermediate repair
bug: queuing at return had given the capture a fresh lifetime. The initial full
run `45092062850` was cancelled as superseded, not counted as validation or a
package. The later in-flight-overwrite assertion was not reached in the RED run.

The corrected candidate takes its deadline/stamp before the original and rejects
expired or interleaved captures at completion. A reset frame clock cannot alias
an old stamp. Stamp wrap disables new deferrals instead of reviving old ones.
No private bindings or queued metadata may survive a failed boundary check.

## 4. Validation status

- Strict local Linux Debug/Vulkan-OFF CTest: **9/9 PASS**.
- GCC ASan/UBSan with leak detection, Vulkan-OFF: **9/9 PASS**. Separate Clang
  UBSan provenance test passes with nonrecovering errors.
- A separate strict local Debug/Vulkan-ON build and its ASan/UBSan/leak build
  each pass **10/10**, using the independently supplied SwiftShader Vulkan
  library for real API pixels. These are software results, not physical GPUs.
- Strict Win64 C++17 `-O2 -Wall -Wextra -Werror` object compilation passes for
  modified production hooks/runtime and the expanded ordering regression.
  Cross-compilation is not native Windows execution.
- Source-only public-Qt boundary, notices, Python syntax and `git diff --check`
  pass. Actual Qt plugin/UI and full-matrix results are recorded separately.
- Independent ANGLE/SwiftShader GLES 3.1 pixels pass on the candidate. Actual GL
  copies occur between capture and completion. In both sampler-object and
  texture-state source modes, queued metadata alone and unrelated consumption
  leave the producer unchanged. Exact late consumption enables one upstream
  material binding, **zero** LINEAR compositor replacements, red native /
  yellow Strong pixels at alpha 173, and **byte-exact entire-frame Off**.
  Source state, pending API error, revision reset and original TTL checks pass.
  Existing material, surfaceless and nonsharing native-image controls also pass.
  This is an authored independent software driver, not the Win32 detours, PF,
  the installed game, physical hardware or FPS acceptance.

- Corrected [focused Windows run 45092557605](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45092557605),
  job `137425163647`: **SUCCESS**, including the expanded ordering contract,
  original entry-time deadline, in-flight and later overwrite controls, isolated
  namespace, settings reset on a joined native worker, deletion/reuse, native
  generation retirement, exact native ABI and Off. All nine split modes,
  the transfer ABI contract and the non-split suite pass. Other matrix groups
  are intentionally skipped in this focused run, not certified by it.
- [Full matrix 45092754471](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45092754471)
  at the **same candidate source**: **SUCCESS, six jobs pass**; the optional
  focused-only duplicate is skipped. All registered Windows native contracts
  run in successful unfiltered CTest steps on Qt 5.15.2 → independent Qt 5 host
  (`137425784085`), Qt 6.2.4 → 6.7.3 (`137425783956`) and Qt 6.2.4 → 6.8.3
  (`137425783948`). Actual loaded-plugin/physical controls, Save/No/Discard,
  native Settings/theme and selected-tab-only glass tests pass. Independent
  Linux Qt 5 and pinned-player static source/inventory jobs pass too.
- Graphics job `137425783669` passes native Mesa Vulkan validation, SwiftShader
  sanitizer Vulkan, Mesa GLES/desktop GL and independent ANGLE controls.
  **Eleven complete hosted software-pixel JSON notices** are parsed and checked.
  Three are the new copy-order path (Mesa GLES, desktop GL and ANGLE GLES), each
  in both source modes with producer binding 1, compositor 0 and exact Off.
  The other eight retain material strengths, actual surfaceless producers,
  named copies and actual nonsharing native-image controls. Full per-case
  successful Windows transcripts and hosted PNG artifacts were not retrieved
  into this Linux workspace; successful explicit/unfiltered steps and complete
  bounded JSON notices were read back. Local generated PNGs were independently
  CRC-checked, decoded and compared byte-for-byte.

- [Nonpublishing package 45092757076](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45092757076)
  at the **same candidate source**: **SUCCESS**, Windows job `137425791549`.
  Both genuine Qt module baselines, application/portable tests, administrator
  manifest, updater acceptance and actual setup install/upgrade/uninstall pass.
  Both actual uploaded ZIPs pass hosted download, CRC/full-ZIP/exact single-EXE
  hashes and Windows extraction checks. Source/artifact metadata, digests and
  complete notices were independently reconciled here. Public release and
  Discord publication are **skipped**; no release/tag is created.

[Machine-readable evidence](evidence/potato-copy-consumer-order.json) retains
source hashes, both RED assertions, the superseded/cancelled intermediate run,
exact hosted conclusions, all eleven complete software JSON notices, six locally
decoded software PNGs and their hashes, package identities and scope limits.
Additional offline integrity-verifier tests pass 19/19; CI-diagnostic and
independent-host-path guards each pass 6/6.

## 5. Verified v5 test downloads

The application version label is still **4.0.1**. Identify these artifacts by
**`91f4efb…` / run `45092757076` / attempt 1**, not by the version alone. Older
v4 downloads do not contain this ordering repair. Later documentation-only
commits are not the binary build source.

| Build | Verified artifact | ZIP bytes | ZIP SHA-256 |
| --- | --- | ---: | --- |
| Portable x64 | [BSI-Customizer-4.0.1-win64-45092757076-r1](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45092757076/artifacts/14781959380) | 43,775,209 | `bbb692e9d56a67e1b646dd5644428fe682a0199ab0192c62b8892d089cae29fb` |
| Optional setup | [BSI-Customizer-4.0.1-setup-45092757076-r1](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45092757076/artifacts/14782388773) | 46,092,251 | `ab02be297ca1a3a28d3a233f58a0be42984d11e66e594ac0c8c32a1ab0ed8279` |

Exact EXE SHA-256 after hosted Windows extraction:

- `BSI-Customizer-4.0.1-win64.exe`:
  `c875e9cf6fac3c82930537c807f1711e9d87f1bd44d41e10f8fc0199ff6fbaf1`.
- `BSI-Customizer-4.0.1-setup.exe`:
  `fa18259965ead203426b0dcba28162c37b0874a6056a84a2666537c2fe0238e7`.

The actual uploaded ZIPs were verified and extracted **in the hosted Windows
package job**, not downloaded or executed in this Linux workspace. The separate
local integrity verifier tests use authored archives only. GitHub artifact
access is required and these are temporary test downloads, not releases.
GitHub reports expiry `2026-11-26T01:23:31Z` portable /
`2026-11-26T01:23:33Z` setup. Artifact integrity is not game acceptance.
PR 121 remains **draft, open, unmerged and unreleased**.

## 6. Installed check and acceptance

Do not re-test the old v4 download expecting this repair. A source commit is not
an installed module. Use the verified v5 package above, close all player instances
in a backed-up authorized test installation, and use:

**Inspect compatibility → 2. Review update → Install native shaders.**

Replacing the Customizer EXE alone does not replace the native DLL. Check that
copied diagnostics identify the candidate source and
**`gl-copy-consumer-order-v5`**. At 100% strength, copy the complete diagnostics
**before changing strength or turning Off**, since those reset the interval.
Retain the routing ledger **and** deferred-reconciliation line.

Actual material acceptance remains the same scene **Off → Strong → Off**, with
native material detail restored exactly on the final Off. Queued/reconciled
counters, a saved slider, a grading effect or a verified package are not success.
If exact supported provenance is not established, keep native behavior and keep
the game result **not working**.

No new installer/PF/PD/account upload is needed. PF means Program Files; PD means
Program Data. The supplied PF archive remains static-analysis-only. Loader
handoff via `grApiInit` and the QML/native child-window private-slot-zero path is
already resolved; it is not restarted as a missing hook. General EGLImage,
native-buffer and private transfers remain unsupported/unproved. No forced
LINEAR mips, mip generation, output blur/rescaling, guessed sharing, foreign or
protected adoption, speculative PE/DllMain hook, public release, tag or merge.
