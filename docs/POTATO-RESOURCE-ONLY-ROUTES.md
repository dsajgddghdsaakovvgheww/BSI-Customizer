# Potato: named color resources do not need a native default image

## Installed game is still negative

The latest installed report has **1,030 presentation frames / 2,058 NoSampler
inspections and zero replacements/producers/transfers**; the
[GitHub follow-up](POTATO-GITHUB-RENDERING-CLUES.md) records the next investigation.
The preceding reports have **1,577 grading frames / 3,145 sampler
inspections** and **2,993 grading frames / 856 sampler inspections**. Every
inspection is **NoSampler**, with **zero prepared draws/private bindings,
zero linked producer inspections/bindings and zero qualified color transfers**.
Saved strength is 100%, policy `OwnedMipCompatibility`; the observed host API
is OpenGL. Qt bridge/host are 5.15.2/5.15.4. Vulkan has one loader/instance/device
but **no Win32 surface, swapchain or present**. These are separate intervals,
not evidence of partial texture reduction.

The new effective-filter diagnostic identifies the inspected consumer's actual
first blocker: **`GL_LINEAR` (`0x2601`), legal base-image-only sampling**. It is
mutable RGB `0x1907`, 256x256, stored/effective BASE/MAX 0/1000, storage/view -1/-1,
failed required level -1; `non-mip-filter=1`, all other mip-first-blockers zero.
This supersedes the older ambiguous level-1 refusal. It does **not** establish
that the unseen game materials lack mips. Forcing a mip filter/generating mips
or blurring this compositor input is not a solution.

## Concrete capture bug reproduced and repaired

The native `GameCandidate` prefilter previously rejected every WGL DC with no
mapped HWND unless its pbuffer lifetime had been observed. It also rejected a
real surfaceless EGL context. That is necessary when identifying a **default
native color image**, but incorrect for a **named GL FBO texture/renderbuffer**:
its identity is the typed object in its GL namespace, not the native drawable.
An FBO writing a recently consumed texture was unreachable even in the SAME
context, before the existing resource proof could run.

`GlProducerRoute.h` now shares the actual production prefilter and output
qualification with native policy tests and the real-pixel bridge:

| Current native route | Named GL color output | Default color output |
| --- | --- | --- |
| Exact selected game drawable | Existing checks | May seed selected input evidence |
| Observed unprotected native pbuffer | Exact resource proof | Only its observed lifetime and color aspect |
| Another captured drawable on the selected HWND | Exact resource proof | Refuse |
| Current WGL DC with no HWND and no observed pbuffer | Exact resource proof | Refuse |
| EGL context/display current with BOTH surfaces `EGL_NO_SURFACE` | Exact resource proof | Refuse |
| Unmapped non-null EGL surface, foreign/Qt HWND, missing WGL DC | Refuse | Refuse |

A resource-only candidate still needs an **already selected owned presentation**,
active reviewed compatibility, installed deletion/lifecycle tracing and recent
namespace-qualified dependencies **before optional GL initialization**. Then
read-only capture must find the exact typed output and production provenance
must authorize it before `begin()` can replace any material sampler. Sharing,
equal names or merely having an FBO cannot seed ownership. Same/observed-successful
sharing, provider/display separation, protection, color/blend/primitive/mip and
source-descriptor checks are unchanged. Native-image links still require their
exact observed pbuffer lifetime; this repair invents none.

For actual EGL surfaceless contexts, check the current display/context and both
null surfaces, legally query context protection when advertised, and **never
call `eglQuerySurface(EGL_NO_SURFACE, ...)`**. Unknown non-null surfaces are not
classified from their config. No foreign-context probe, extra GPU wait, context
migration, permanent namespace merge or persistent source mutation is added.

Only the upstream draw's qualifying existing material mips use private samplers.
Restore exact source bindings/active unit around the unchanged original. Off,
source/strength/review revisions, deletion, inherited two-frame TTL/eight-hop
bounds and lifecycle/concurrency rules remain. No new UI or preferences, frame
blur, generated mips, output-resolution change or allocation/FPS claim.

## Executed regression and pixel evidence

- **Before repair:** the new production-route unit assertion fails specifically
  because the valid no-window named FBO cannot reach resource proof. An actual
  ANGLE/SwiftShader surfaceless EGL run passes all earlier controls, then fails
  at **50% material strength: zero replacements, red `[255,0,0,173]`** instead
  of the expected coarser mip. An initial fixture setup error (setting the
  consumer filter on the wrong active unit) was corrected before recording
  this intended failing production regression; it is not counted as that proof.
- **After repair:** actual ANGLE GLES 3.1, Mesa 25.2.8 softpipe GLES 3.1 and
  Mesa desktop core 3.3 pass the new mandatory surfaceless pixels and all
  preceding assertions. Both texture-parameter and existing-sampler sources at
  **0/50/100%** produce **red `[255,0,0,173]` / cyan `[0,128,128,173]` /
  yellow `[255,255,0,173]`**. The real current context/display and BOTH null EGL
  surfaces are asserted, not inferred from a pbuffer fixture.
- **One material binding; zero LINEAR compositor replacements.** The changed
  producer propagates through that unchanged compositor at 64x64; the **entire
  Off frame** exactly equals native. Source LODs/bindings, active unit, pending
  errors, no-consumer/default-output rejection, revision and expiry are checked.
  A separate bounded JSON/annotation and optional PNGs retain this proof without
  truncating the existing five-strength/immutable/material report.
- **Native CTest 9/9 and ASan/UBSan/leak checks 9/9**, including required real
  software Vulkan controls. New policy tests retain foreign/unmapped-EGL/default
  exclusions, isolated/provider/display/typed-name collisions, successful sharing
  plus fresh consumption, unrelated FBOs, TTL/revision/deletion/source revocation.
- Strict Win64 compilation passes for the production hooks and five extended
  native-route/EGL/WGL fixture sources. The Windows tests now assert private
  samplers INSIDE original draws on a pre-existing no-HWND DC (same context)
  and a surfaceless observed-shared context, plus named RBO resolves, protected
  context refusal before GL, null-surface query prohibition, mismatched READ/DRAW
  and unknown default-image exclusion. **Compilation is not Windows execution**;
  exact-head Windows CI/package outcomes are recorded on draft PR121.
- Existing Mesa GLES2 grading, all Mesa GLES/desktop strength/storage/view/copy/
  RBO/Off/error controls, and the mandatory ANGLE **raw-before-production**
  nonsharing native-image fixture pass. Mesa's independent raw native-pbuffer
  export failure remains documented; this does not claim that route works there.

### First Windows attempt and fixture-contract correction

The first candidate `9ff81da` failed the WGL/EGL hook assertions in the Windows
shader/package runs; it is **not** a successful Windows candidate. Review found
an old WGL expectation still treating last-DC-lease release as loss of named GL
FBO proof (only its native DEFAULT-image identity is lost), and a new positive
surfaceless RBO case using the protected-texture-capable context, whose RBO
protection cannot be queried without changing bindings. Production correctly
keeps that RBO path native. The fixture correction retains an explicit protected
RBO negative, uses the existing qualified no-protection transfer context for the
positive RBO case, and checks that last-lease release refuses default-image
copies while the separately qualified named FBO still works. No production
protection/lifetime guard is relaxed for either correction. Draw mismatches now
print expected/actual inside-original bindings and the original-call number.
Follow-up exact-head Windows execution belongs on draft PR121, not inferred
from cross-compilation or this explanation.

This is an independently demonstrated missing producer path, **not a proven
installed-game root cause or success**. Actual game material capture/texture
changes/exact Off remain unverified. Unobserved sharing and EGLImage/private/
external-memory aliases remain unresolved leads. Keep draft/unmerged/unreleased;
no supplied binary deployment, licensing/protection bypass or universal-GPU claim.
