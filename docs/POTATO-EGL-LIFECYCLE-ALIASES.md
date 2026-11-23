# Potato: split EGL lifecycle aliases — reproduced gap and narrow repair

## Status and scope

> The later v4 installed test also remains **not working**: `ebdf6c2…`,
> 1,392 frames, 186 NoSampler inspections, zero material bindings. See the
> [latest negative, independent copy-order regression and v5 candidate](POTATO-COPY-CONSUMER-ORDER.md).
> The v3/v4 package references below are historical, not the newest candidate.

This work follows the [verified PF archive inspection](POTATO-PLAYER-PF-AUDIT.md).
The PF files were inspected as data, **not loaded or executed**. Their public
EGL shim and resolver returned different code addresses. That observation alone
was not called a defect: a separate, authored Windows regression first reproduced
missing acquisition/presentation/retirement behavior in production hooks.

**This is a real public-dispatch coverage repair, not a diagnosed Free Fire fix.**
**The subsequent installed test confirms this candidate still does not apply
Potato:** module `00c1b7c…` / `gl-egl-lifecycle-alias-v3`, **706 frames / 330
NoSampler / zero replacements**. Pre-route activity is **19,379 draws / 329
transfers**, but there are still zero qualified transfers or linked producers.
This supersedes the earlier 3,154/6,297 report without blaming a stale install.
See the [confirmed failure and transfer-routing diagnostic follow-up](POTATO-TRANSFER-ROUTING-DIAGNOSTICS.md).
The exact DLL file hash and actual private/provider branch remain unproved.
No new private offsets, vendor ABI, EGLImage/native-buffer ownership or
permission to inspect foreign/protected resources is inferred from the archive.

Repair source: `ec75261a9406e269d2769e93e3a6768eb87fc5b6`.
The later candidate source includes additional tests and the durable static
inspection evidence. Its build/CI identity is recorded below, separately from
that production repair commit.

## 1. Before-repair Windows evidence

Before-source revision:
`28e21295eb9e91d10b50e94023aa1506f7a61033`.
Production `PresentationHooks.cpp` SHA-256:
`f9e43f6363c589018ec5d23eb271a9790e84eea4753d4019a97d44a938463a34`.
It is byte-identical to the previously delivered capture candidate's hooks.

[Before run 45036397728](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45036397728)
executes actual Windows MinHook, real owned/foreign HWNDs and **two authored
shim/backend DLL pairs**, without Qt or proprietary files. It checks each
public stub is a RIP-indirect jump through its actual imported backend pointer,
not a PE forwarder or two exported names for one address. A normal REX.W prefix
is allowed. The backend resolver returns the distinct implementation address.

| Contract | Before result / first failing behavior |
| --- | --- |
| `shader-egl-split-exports-contract` | **PASS** — ordinary public-export control. |
| `shader-egl-split-queried-contract` | **FAIL** — queried create/current/present does not select the owned drawable. |
| `shader-egl-split-cached-contract` | **FAIL** — pointers cached before bootstrap do not select the owned drawable. |
| `shader-egl-split-present-contract` | **FAIL** — an implementation-address swap bypasses presentation observation. |
| `shader-egl-split-retire-surface-contract` | **FAIL** — successful implementation-address surface teardown leaves the source selected; the native failure control is retained. |
| `shader-egl-split-retire-context-contract` | **FAIL** — successful implementation-address context teardown leaves the source selected; the native failure control is retained. |
| `shader-egl-split-resolver-contract` | **FAIL** — a cached implementation resolver returns a new GL target without coverage before its first original draw. |

The existing non-split native contracts pass in that focused run. These are
**authored dispatch/state doubles**, not EGL driver pixels or a game trace.
The precise missing behavior, rather than a distinct address alone, justified
changing production.

An earlier setup run `45035778852` rejected compiler-emitted REX-prefixed import
jumps and was superseded/cancelled. That fixture-decoding error is **not** a
production before failure. Likewise, the first added capacity test omitted
provider B's existing entry when counting a shared code pool; its expectation
was corrected, not the production pool limit. Neither setup issue is hidden as
a repaired game defect.

## 2. Narrow implementation

### Exact typed targets, not a replaced global original

The public per-provider callbacks remain. Each distinct lookup-returned address
for the following **public** contracts also receives its own bounded slot and
native trampoline:

- `eglCreateWindowSurface`, `eglCreatePlatformWindowSurface` and
  `eglCreatePlatformWindowSurfaceEXT`;
- `eglCreateContext`, `eglCreatePbufferSurface`;
- `eglDestroySurface`, `eglDestroyContext`;
- `eglSwapBuffers`, `eglBindTexImage`, `eglGetProcAddress`.

Core platform attributes retain their pointer-sized type; EXT/window attributes
retain `EGLint` width. An address cannot silently acquire an incompatible second
callback. A lookup returning an already-covered public export retains that
callback/original pair. The existing exact-target `eglMakeCurrent` and GL
procedure pools are reused, not replaced by guessed private dispatch slots.

Internal discovery queries this fixed public name list during attachment,
covering the demonstrated pre-bootstrap cached-pointer pattern. Later native
lookup returns are observed too. Internal lookups have a separate thread-local
code-discovery guard, so a public resolver trampoline which enters a separately
hooked implementation cannot recursively register its own discovery calls.
There is no new per-frame sweep, GL state query or context migration here.

### Imported stub plus hooked implementation is one observation

A shim trampoline can call its imported implementation after **both** have been
hooked. Every callback must invoke its **own** exact original. A bounded,
thread-local `EglDispatchScope` deduplicates the same operation and exact
arguments on that native stack. Different calls, signatures and disjoint
provider candidates do not borrow an outer attribution. Nested-scan overflow
stays native rather than guessing.

This prevents duplicate creation/share events, pbuffer generations, swaps and
image-link publication. Native return values, arguments and incoming/outgoing
Win32 LastError are retained. No render lock, sampler lease or resource/session
pointer is held across a native image-binding original. Successful teardown
revokes evidence; failed native teardown must not release live ownership.

### Provider and lifetime gates remain fail-closed

- No acquired surface/pbuffer/share or processed swap is published before
  attachment completes. The externally visible public-swap hook is enabled
  last, avoiding a late-provider readiness race.
- There are **16 exact alias slots per lifecycle operation, shared across all
  providers**, not 16 resource namespaces. Null/unsupported results stay native.
  A known non-null lifecycle target that cannot be attached faults that provider's
  optional work, rather than retaining proof with an uncovered retirement path.
  Native calls/pointers/results still pass through; restart is required to retry
  that faulted provider. Counters from earlier calls are not current-pixel proof.
- Shared code addresses do **not** prove shared providers/resources. Ambiguous
  direct lifecycle calls cannot acquire ownership or publish share/image links.
  Successful ambiguous teardown may only conservatively revoke matching candidate
  evidence. The portable attribution test also covers this negative policy.
- Resource provider/display/context/namespace/share generations, object and
  pbuffer lifetimes, protection, color/type, selected-route revision and inherited
  TTL checks are unchanged. Existing native pbuffer binding is still only its
  previously qualified public subset, **not general EGLImage/native-buffer
  interoperability**.
- The mip-selection/texture-inspection policy is unchanged. A legal one-level
  **GL_LINEAR** presentation texture is not forced into mip sampling, rescaled,
  blurred, allocated new mips or counted as an override.

## 3. Executed regression evidence

### Local

- Strict Win64 C++17 `-Wall -Wextra -Werror` cross-compilation of production hooks
  and new fixture/test sources. Cross-compilation is **not Windows execution**.
- Native CTest **10/10** and ASan/UBSan/leak CTest **10/10**, including a deliberately
  selected SwiftShader Vulkan ICD. The new portable dispatch-attribution test
  checks duplicate/independent/ambiguous calls, differing argument and contract
  types, bounded nesting, thread isolation and internal-lookup recursion.
- Actual ANGLE/SwiftShader **GLES 3.1** software-driver pixels pass before and
  after this repair. Existing material mips select native red `[255,0,0,173]`,
  half-strength `[0,128,128,173]` and Strong yellow `[255,255,0,173]`; the entire
  authored Off images equal native bytes. Both texture/sampler source modes,
  unchanged source/binding/pending-error state, resource-only/default-pbuffer/
  RBO-copy producers, bounded expiry and nonsharing native pbuffer image binding
  pass. The one-level LINEAR compositor receives **zero** private bindings.
  These software pixel tests exercise production texture code, **not the new
  Win32 detour path**; the Windows contracts above test that separate boundary.
- The supplied PF regression suite reruns **19/19, zero skips**, including all
  archive CRCs and 139 independent selected-instruction objdump comparisons.
  The original private providers remain unmodified and are not executed.

### Windows repair progression

[First repair run 45036727720](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45036727720)
at `ec75261a9406e269d2769e93e3a6768eb87fc5b6` passes **all seven split contracts**
and the existing non-split native suite. This is a focused run; its four other
job groups are intentionally **skipped**, not full Qt/GPU matrix successes.

The additional native-image contract passes in run `45037065640`: distinct
queried context creation records explicit sharing once; equal isolated texture
names do not become shared; queried pbuffer/image binding links a nonsharing
upstream material only after native success; failed/protected, TTL, deletion,
LINEAR and Off controls pass. That run's capacity assertion exposed the test's
two-provider count error described above, corrected in `fd396c7`.

Candidate source:
**`00c1b7c1d4caf5ce1ac7e0ddaf08494f0aa6c45d`**.

- [Full shader/API matrix 45037613555](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45037613555):
  **completed / SUCCESS; all six full-matrix jobs pass**. The optional
  `focused-egl` duplicate job is intentionally skipped, not counted as a pass.
  All nine split contracts, including native-image lifetime and corrected
  global-capacity/fail-closed checks, are registered in the unfiltered native
  CTest steps, which pass in all three Windows configurations:

  | Windows job | Module build baseline → independent host |
  | --- | --- |
  | `137254747069` | Qt 5.15.2 → PyQt5-bundled Qt 5.15 |
  | `137254747119` | Qt 6.2.4 → Qt 6.8.3 |
  | `137254747279` | Qt 6.2.4 → Qt 6.7.3 |

  Their remaining loaded-plugin, physical controls/Save/Discard and selected-tab
  glass/negative-control steps also finish successfully. The independent Linux
  Qt 5 host, pinned-player Qt inventory/Phone-source and software Vulkan/GLES/GL
  jobs pass. All **eight complete software-pixel JSON notices** were parsed;
  software rendering is not physical-GPU or installed-game certification.
  Source, job steps and complete paginated check annotations were retrieved;
  a per-case full Windows CTest log was not downloaded into this workspace.
- [Nonpublishing test package 45037616353](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45037616353):
  **completed / SUCCESS** at the same candidate source. Both Qt shader bundles,
  application/portable self-tests, administrator manifest, updater acceptance
  and actual installer install/upgrade/uninstall checks pass. Both uploaded ZIPs
  are re-downloaded, CRC/hash checked, matched to their single tested EXE and
  extracted/hash-checked again with Windows `Expand-Archive` in the hosted job.
  Release and Discord publication steps are **skipped**; no tag or public release
  is created.

Machine-readable before/after source, checks, authored-pixel evidence and
verified download identities:
[`evidence/potato-egl-lifecycle-aliases.json`](evidence/potato-egl-lifecycle-aliases.json).
The subsequent documentation/evidence commit is **not** the package's build
source. Production, tests, packaging and workflows remain at the candidate
identified above.

## 4. Historical verified v3 downloads — superseded for the next test

These artifacts contain the lifecycle-alias repair, **not the new v4 transfer
ledger**. The user reached this v3 build and reported the negative result above;
the next diagnostic test uses the [identified v4 package](POTATO-TRANSFER-ROUTING-DIAGNOSTICS.md#3-verified-v4-test-downloads).
The older artifacts and hashes below are retained as historical evidence, not
reoffered as a new game fix. The version label is still **4.0.1**, so use the
**run/source/artifact identity**, not the version number alone.

| Build | Verified artifact | ZIP bytes | ZIP SHA-256 |
| --- | --- | ---: | --- |
| Portable x64 | [BSI-Customizer-4.0.1-win64-45037616353-r1](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45037616353/artifacts/14760093940) | 43,739,903 | `c4b533b66ef9a6193e5a4afef03c86032daf92302ab079e92b2e99c372bab66f` |
| Setup | [BSI-Customizer-4.0.1-setup-45037616353-r1](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45037616353/artifacts/14760925895) | 46,034,392 | `bcd05bf347962ccf1e3c34c1d5a6cd854e31a07ef0c0663bdb70ecbbc59a1955` |

Inner EXE SHA-256 after Windows extraction:

- `BSI-Customizer-4.0.1-win64.exe`:
  `10d58b2134a07be0811d177ce66247cf1f958ab0904bd6e1887f050ea3b8c640`.
- `BSI-Customizer-4.0.1-setup.exe`:
  `e33098850d92ee48bdce5411e17c94bf1c73cdb238ad677a4d8eed97ace486f2`.

The exact artifact IDs, names, run/head, sizes and ZIP digests were independently
matched against complete Windows readback/extraction check notices. The ZIPs
were verified **on Windows CI**, not downloaded or executed in this Linux
workspace. The local archive-verifier regression separately passes **19/19**
without executing an EXE. GitHub requires artifact access and retains these test
artifacts only briefly (configured retention: three days); the run's artifact
metadata, including GitHub-reported expiry, is saved in the evidence. These are
not release downloads or a claim of installed-game success.

## 5. Actual-player acceptance remains separate

A successful test artifact must be deployed using:

**Inspect compatibility → 2. Review update → Install native shaders.**

Replacing the Customizer EXE alone does not replace the installed shader DLL.
The historical v3 check required capture schema **`gl-egl-lifecycle-alias-v3`**;
the user returned it with source `00c1b7c…`, confirming the candidate was reached.
An unknown earlier file checksum does not prove that installation was stale.
Use the [v4 follow-up protocol](POTATO-TRANSFER-ROUTING-DIAGNOSTICS.md#4-next-useful-installed-check)
for the next test. Keep the usual full backup/test-installation and
protection/anti-cheat restrictions in [the user test guide](SHADERS-USER-TEST.md).

The relevant comparison is a repeatable **material-texture image with Potato
Off versus On/Strong**, then Off again returning the native image—not grading,
status text, a saved slider, counters or a forced sampler on the final RGB
presentation image. The subsequent zero-replacement result confirms that this
public alias repair has **not** fixed the user's game. It does not establish
which private/provider/resource boundary is still missing, nor authorize
adopting unowned/protected content as a workaround.
