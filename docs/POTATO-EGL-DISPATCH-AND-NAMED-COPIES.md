# Potato: EGL procedure discovery and binding-free named image copies

## Installed-game acceptance has not changed

The latest user report remains **3,154 presentation frames / 6,297 NoSampler
inspections**, with **zero prepared/private bindings, producers and qualified
transfers**. Its mutable RGB 256x256 presentation input uses legal **GL_LINEAR**.
Neither its unseen material renderer nor the installed module hash was proved.
This is not texture success, absent material mips proved by inspection, or a
reason to generate mips/change the finished image's sampler. The repairs below
are independently reproduced capture gaps, **not an installed-game diagnosis**.

## Four before-repair assertions, not speculative attribution

Tests-only commit `634335e34e9093a0644f0f41e0dd3d559d5e875d` extends the native
sampler test and adds three independently registered Windows contracts.
[Baseline 45013999597](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45013999597)
completes with exactly these failures in **all three Windows configurations**:

| Contract | Intended first failure |
| --- | --- |
| `shader-egl-cached-procedures-contract` | Pre-bootstrap cached target is not discovered after successful make-current, before its first draw. |
| `shader-egl-late-procedures-contract` | A later EGL lookup returns a different target which is not patched before its first draw. |
| `shader-wgl-existing-current-contract` | Presenter already current at bootstrap swaps, but cached ARB/EXT binding targets remain unpatched. |
| Extended `shader-gl-texture-safety` | Valid named RBO-to-texture image copy produces no typed resource observation. |

Each Windows configuration has **27 passing tests / these four failures out of
31**, including a passing previously repaired cached-WGL contract. Native Linux
has **19 passing tests / the named-copy assertion failing out of 20**; the two
other Linux jobs pass. The pixel-artifact upload also fails because execution
stops before pixel generation; it is not an additional production assertion.

The Windows fixtures use **actual production MinHook and real Win32 HWND/DC
ownership checks**, but their EGL/WGL/GL dispatch tables/resources are authored
DLL doubles. They are not an EGL driver, BlueStacks, Free Fire or GPU evidence.
The separate real software-driver regression first verifies the raw image copy
and unchanged composition, then fails at the production RBO-to-texture capture
assertion when compiled against the before-repair inspector. An earlier fixture
setup attempt failed because the compositor intentionally unbound its sampler;
restoring the source sampler before the next producer draw fixed the test setup.
That setup failure is **not** the recorded production before failure.

## Narrow production repairs

### EGL cached and later-returned procedures

- Observe native `eglMakeCurrent` only **after its original succeeds**. Verify
  the actual provider/display/context and exact DRAW/READ surfaces, then resolve
  the known draw, transfer and deletion inventory on that same native thread.
  No GL state query, render-mutex lock, context migration or ownership claim is
  part of this code discovery. Failed binds and null unbinds do not sweep.
- Observe actual `eglGetProcAddress` return addresses. Each distinct known GL
  target gets its own bounded slot/native trampoline; the application receives
  its original pointer and native Win32 LastError. A returned `eglMakeCurrent`
  target is also covered. Internal discovery uses the native lookup trampoline,
  not recursively the detoured lookup function.
- The shared exact-target pools retain provider masks. A bounded **64-entry
  thread-local provider/display/context code cache** avoids repeated sweeps;
  observed creation/deletion revisions invalidate it. Deletion observation is
  required; failed attachment retries rather than caching incomplete coverage.
  Overflow rescans rather than authorizes a resource.
- The tests assert interception **before the first original**, once-only exact
  arguments/results/errors, no queries before ownership/protection, independent
  provider tables, repeat-bind caching, lifecycle invalidation, failed/null
  binding, foreign/protected/Off exclusion and an unchanged LINEAR consumer.

### Already-current WGL presenter

The existing owned presentation path now invokes the same code-only discovery
used after WGL make-current. This covers a presenter already current before
bootstrap, so a cached separate-binding alias can be patched at its owned swap
without requiring a later core bind/application lookup. It does not claim that
a context which never binds, queries or presents through any known path is
captured. No initialization is moved into `DllMain` or PE imports.

### Named image-copy dependencies

Observe `glCopyImageSubData` and ABI-compatible EXT/OES/NV spellings using exact
native originals and all **15 unchanged arguments**. Optional code coverage is
not feature support: read-only capture requires **advertised desktop core/ARB
DSA**, the required actual query procedures and an advertised copy-image
contract matching the observed target. A non-null driver stub is insufficient.

The implemented descriptor subset is deliberately narrow:

- Existing normalized-color **2D textures / renderbuffers** only; validate names
  with legal `glIsTexture`/`glIsRenderbuffer` before named queries. A texture's
  actual established `GL_TEXTURE_TARGET` must match. Do not bind unknown names,
  FBOs, RBOs, textures or samplers to inspect/"repair" them.
- Level zero, Z zero, depth one, positive bounded extents, valid source and
  destination rectangles, identical sized normalized internal formats and equal
  sample counts. No format reinterpretation, multisample resolve, compressed,
  integer/depth/stencil, cube/array/3D or nonzero-level inference.
- Qualify texture completeness using its actual native filter, BASE/MAX and
  required images. Immutable storage/view bounds use legal supported tokens,
  including **`GL_TEXTURE_VIEW_NUM_LEVELS = 0x82DC`**, never a nonexistent tail
  down to 1x1 or a modified MAX_LEVEL. Non-mip LINEAR copies need no extra mips.
- Contexts where texture protection is possible stay native for this named-copy
  route. GLES has no implemented binding-free named-query route here, even if
  its driver returns copy-image pointers. These conservative exclusions do not
  disable the existing qualified draw path.

Metadata goes through the existing typed texture/RBO provenance and is published
**only after the native original returns**, with unchanged revision, resource
serial, current context/drawable and selected route. It is a submitted,
descriptor-qualified dependency, **not a general driver-success oracle**. Deletion,
source/review/strength changes, two-frame inherited TTL/eight-hop bounds and
namespace/provider/display separation still revoke proof. The new Windows
named-copy contract checks exact original arguments, no borrowed sampler/state,
reentrant settings/deletion inside the original, no premature publication,
foreign/isolated/invalid/retirement/Off guards and a real upstream private binding.

### Useful next-test diagnostics without manufactured success

The clipboard output now identifies the **native shader module's build Git
revision** and capture schema `gl-dispatch-named-copy-v2`, independently of the
Customizer EXE version. Builds with modified tracked shader sources are marked
`-modified`; source archives without Git honestly say `unversioned`. No private
module/process path is copied.

Separate **pre-route hook-entry counters** report draws and transfers, with
named-copy/native-image-binding subsets, while Potato and a selected GL game
source are active. They can include foreign routes/nested aliases; they are
**not inspected game draws, qualified transfers, resource ownership, successful
pixels or FPS**. Existing presentation/private-binding recursion guards still
exclude their guarded internals; this is not a complete process/API trace. A
positive raw count with zero qualified transfers proves the detour saw activity
which did not become accepted transfer evidence, not why every path was refused.
Off/zero strength and source/settings changes clear these counters. No diagnostic
GPU query, persistent log, upload or setting is added.

## Executed local evidence for repair `b807653`

- Strict Win64 C++17/Werror compilation of production hooks/inspector/runtime and
  affected fixture/test sources; source-only public Qt and notices guards pass.
  **Cross-compilation does not execute Windows contracts.**
- Native CTest **9/9**, plus ASan/UBSan/leak CTest **9/9**, with an explicitly
  selected software Vulkan ICD. Diagnostics-runner tests **6/6** and artifact
  integrity/download tests **19/19** pass.
- Actual Mesa 25.2.8 desktop **3.3 core**, advertising ARB DSA/copy-image (no fake
  version override), passes core and NV named RBO-to-texture material paths.
  Native red `[255,0,0,173]` becomes Strong yellow `[255,255,0,173]`; **entire
  Off equals raw native bytes**, one producer binding / **zero LINEAR compositor
  bindings**, with source/FBO/RBO/bindings and application pending errors intact.
  A missing observed copy cannot invent a producer; copies cannot renew TTL.
- Real named queries/copies also qualify **four-level immutable storage** and a
  **three-level view of seven-level storage** without source mutation or added/
  consumed errors. This exercises the view token separately from authored doubles.
- Existing actual Mesa GLES/default-client grading, GLES strength/resource/copy/
  storage/view/Off controls, desktop controls and mandatory ANGLE/SwiftShader
  nonsharing native-image controls pass. The default client is not asserted to
  be strict GLES2. The known Mesa raw native-image export failure is not hidden
  or relabelled supported.
- **8 complete local JSON reports / 78 CRC-verified, decoded 64x64 RGBA PNGs**;
  alpha 173 throughout, Strong differs and each entire native/Off pair matches.
  Authored software resources are not installed-game/GPU/FPS acceptance.

## Independently verified fresh test candidate

Build source is **`b80765330bae1164f1fe595d2dadd90c58e8b952`**, not `81509f1`.
It includes this batch and the previously Windows-validated cached-WGL repair
`bba981e`. The older downloads contain neither; they are not offered as this fix.
Any following documentation-only commit is not relabelled as the workflow head.

- [Shader/API matrix **45014823385**](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45014823385):
  **all six jobs complete/success**, including the native API contract step in
  **all three Windows configurations**, with the new EGL/previous-current-WGL/
  named-copy contracts. Windows jobs **137184788276 / 137184788296 /
  137184788378**. The actual Qt5/Qt6 plugin/UI tests, including the new diagnostics
  fields, also pass. **14 complete annotations / zero failures.** A complete
  successful Windows CTest transcript is not claimed to have been downloaded.
- [Package **45014858298**](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45014858298):
  **complete/success**, including both regular shader baselines/native tests,
  portable and full application self-tests, actual installer install/upgrade/
  uninstall, updater acceptance and both uploaded-ZIP download/extraction checks.
  Both publishing steps are **skipped**, not release successes.
- [Chromium **45014862421**](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45014862421):
  **both jobs complete/success**.
- [Broader application **45014860461**](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45014860461):
  **not yet complete** at the delivery readback: **five successful jobs**, including
  both older Windows reference-player self-tests; **two latest-package Windows
  reference jobs still running** (**137184906051 / 137184906087**). The exact three
  default-off jobs `Opt-in original Potato DLL test`, `Opt-in supplied DLL test`
  and `Refuse simultaneous unrelated payload tests` are **skipped**, not passed.
  No all-four-workflows success is claimed. Draft PR121 tracks this separately;
  neither the earlier build's success nor the new package's success completes it.

All three completed primary workflow heads/jobs/checks and complete paginated
annotations were independently re-fetched; **zero failure annotations**. All
**eight complete remote JSON pixel notices** were parsed/asserted, including
actual named-copy and immutable/view metadata cases, not just a green job.
The remote PNG archive was uploaded successfully; it was not downloaded here.
The separate local 78-PNG decoding described above is not labelled remote proof.

### New downloads, not unchanged older EXEs

Both exact uploaded archives were **re-downloaded on Windows** and checked for
ZIP structure/CRC/whole-archive SHA-256, equality with the tested inner EXE and
Windows Expand-Archive equality. Artifact run/source/name/ID/bytes/digest were
independently matched to those complete notices. Archive integrity is not game
acceptance. These are short-retention test artifacts, **not a public release**.

| Build | Verified download | ZIP bytes | ZIP SHA-256 |
| --- | --- | ---: | --- |
| Portable x64 | [BSI-Customizer-4.0.1-win64-45014858298-r1](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45014858298/artifacts/14751900674) | 43,740,797 | `c81b5509adaef69936e9247cb9ccedf64ccd2f3ecb5096740ecf493b67b8644a` |
| Setup x64 | [BSI-Customizer-4.0.1-setup-45014858298-r1](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45014858298/artifacts/14752245033) | 46,058,935 | `06ea72ff203a90f91096215d444ebddf163f5971195c056cd395b34fbcab0778` |

Inner EXEs after actual Windows extraction:

- `win64` EXE SHA-256: `f8625568136ea6d7cb21043744f5c844e3db702ce54f81542215fd258974c47b`
- `setup` EXE SHA-256: `82d8d478e6051b22e7a2ca8025a5372c401da580a176aaf7c8f2ffe0224d45ca`

The Customizer EXE and the installed native shader module are separate. Use the
fresh Customizer's native shader compatibility/review/update flow to deploy the
new owned module; merely replacing the Customizer EXE does not replace an already
installed DLL. New copied diagnostics identify module build `b807653...` and
capture schema `gl-dispatch-named-copy-v2`. This is a new delivery instruction,
not an inference that the previous negative game test had a stale installation.

## Remaining boundary

General EGLImage/native-buffer/external-memory backing aliases, private transfers,
unobserved namespaces, another process/API and paths which never encounter a
known lookup/binding/presentation callback remain unimplemented or unproved.
[ARB DSA specification](https://github.com/KhronosGroup/OpenGL-Registry/blob/main/extensions/ARB/ARB_direct_state_access.txt)
(audited file blob `b779b391f2429ccba03f648c41dde089958be70d`) was consulted
for named-query/TEXTURE_TARGET semantics; emulator source/reference
similarity is not evidence that the installed game uses this route. No alias is
invented from equal names, dimensions, a pbuffer or a context/share group.

**No verified installed-game material texture change or FPS gain is claimed.**
Keep PR121 open/draft/unmerged; no public tag/release is warranted by these tests.
