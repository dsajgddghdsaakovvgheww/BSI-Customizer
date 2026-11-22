# Potato: follow successful native pbuffer image bindings

## Latest installed result is still negative, now with a specific inspected image

The slider/mip-storage candidate's installed report is **2,107 OpenGL grading
frames, 1,456 sampler inspections/all NoSampler, 1,456 presentation inputs,
zero prepared draws/bindings, zero linked producer inspections and zero captured
color transfers**. Saved strength is **100%**, driver review has passed, depth
and blended bypasses are zero. Qt bridge/host are 5.15.2/5.15.4; Vulkan has no
surfaces/swapchains/presents. This is **not working texture Potato**, not a weak
strength, Qt failure, inactive game or a reason to force Vulkan/resolution.

The inspected image is **mutable, RGB 0x1907, 256x256, base/max 0/1000**, with
level **1** first undefined/mismatched under the old completeness test. It is
**not** the shortened immutable-storage case fixed previously. The old report
queried the mip chain before the effective minification filter, so it cannot
establish whether that higher level is required by actual sampling. With a
LINEAR/NEAREST filter a base-only image is legal, irrespective of stored
MAX_LEVEL=1000. With a mip filter a genuinely incomplete mutable chain must
still be refused. No game producer's blocker was captured in this report.

The new diagnostic reads the effective **bound sampler's** minification filter
(or the texture's filter if no sampler object is bound), distinguishes
`non-mip-filter` from `incomplete-required-level`, and records `source-min-filter`.
It does not generate mips, promote an incomplete image, replace its filter,
coarsen the final compositor or count any new binding just to make status green.

## A missing native route independently reproduced and implemented

Native render-to-texture can transfer a pbuffer's color image to the current
texture **without calling glCopyTexSubImage2D or glBlitFramebuffer**:

```text
existing material mips -> native pbuffer default color
                                   |
                    successful eglBindTexImage / wglBindTexImageARB
                                   v
                      base-only texture -> selected game compositor
```

The production hooks now follow **successful** `eglBindTexImage` (GLES) and
`wglBindTexImageARB` (desktop GL). The original native call always executes once
with untouched arguments/return; WGL's original LastError also restores exactly.
No render mutex, session pointer, private sampler or GL-state lease spans it.

- Only a previously observed live unprotected EGL pbuffer or a WGL pbuffer with
  an observed successful DC acquisition supplies a native lifetime. Unknown,
  protected, foreign and unobserved handles do not become evidence.
- The consumer route must already be the selected owned source or an exact
  proven producer; an image binding cannot claim presentation. Before the call,
  capture the current **2D texture name and active unit**, even if its image is
  not yet defined. Only native success permits further capture.
- Query the native target/format/mip level legally: **ordinary 2D, RGB/RGBA,
  level zero** only. Afterward require the **same unit/name**, a defined bounded
  normalized color image and a legally queried unprotected texture. No source
  texture, parameter, sampler, framebuffer or pixel is changed by observation;
  no GL/EGL error queue is consumed.
- Publish only after the original and only if selected source/settings revision,
  current context/drawable and the **drained lifecycle serial** still match.
  Capture the native source generation **and its epoch together under the
  surface lock**; require that exact epoch after draining. A retired generation
  cannot be paired with a newer stable serial. Native driver queries execute
  outside that surface lock. Failure, unknown metadata, contention, deletion,
  source replacement, binding change or revision change cannot create a stale
  resource link.
- GL texture/RBO names still require the same or observed-successfully-shared
  namespace. A **native pbuffer lifetime and exact color buffer** is not a GL
  object name. A positive recent binding/copy link can therefore reach that
  exact native output in a **nonsharing** rendering context. This does not join
  the contexts' GL namespaces: inherited material texture names stay local to
  the producer. Pbuffer existence, sharing or equal texture numbers alone never
  authorize a draw. WGL FRONT/BACK/LEFT/RIGHT aspects remain distinct; a copy's
  actual READ aspect and a draw's actual DRAW aspect are preserved too. An
  upstream blit **to default DRAW** also carries its destination aspect through
  native-output qualification, rather than silently dropping that dependency.
  A new unit assertion reproduced that omission before the repair; the Windows
  WGL fixture additionally follows native binding -> default-DRAW blit -> RBO
  material producer, with separate GL namespaces.
- Native release makes the buffer available for the next render; it is not
  pbuffer retirement. Evidence is a previous successful submitted image link,
  not a permanent alias table. It inherits the selected consumer's **two-frame
  expiry/eight-hop bound** and cannot refresh itself. Pbuffer destruction/reuse,
  tracked resource retirement, source/review/strength/Off revisions revoke it.
  Context/dependency/native registries remain bounded; no random name probe,
  guessed sharing, extra GPU wait or context migration is introduced.

The actual material draw still needs existing complete color mips, supported
source state, driver review and the user's saved strength. Only private
samplers are borrowed around that draw and restored before returning. The
base-only compositor remains **zero replacements**, at unchanged resolution.
Opaque mipmapped HUD/RGB-packed data may also change under the disclosed policy.

## Executed evidence and remaining limits

Local execution for this change:

- **8/8 native CTest** and **8/8 ASan/UBSan/leak checks**, including required
  real software Vulkan controls. Policy doubles include nonsharing native
  output links with separate material namespaces, front/back distinction,
  unrelated provider/display/generation rejection, TTL/revision and retirement.
- **Actual ANGLE/SwiftShader GLES 3.1**, two genuinely **nonsharing EGL contexts**,
  a real texture-capable RGBA pbuffer and real `eglBindTexImage`/release calls.
  First, an **unmodified raw native-image round trip**, before initializing
  either production inspector, must reproduce the entire source frame exactly.
  Then the new production metadata/provenance/private samplers reach the upstream
  material: **0/50/100%** produce **(255,0,0,173)** / **(0,128,128,173)** /
  **(255,255,0,173)** in both texture-object and existing-sampler source modes.
  The unchanged one-level compositor has **0** replacements. The **entire Off
  frame** equals native byte-for-byte; source LODs, bindings, active unit and
  pending errors restore exactly; expiry/revision revoke the native output.
- **Negative control:** disabling only cross-namespace native-pbuffer matching
  in a disposable source copy passes the earlier direct/immutable/FBO/RBO/copy/
  five-strength controls, then fails the intended new **material** assertion at
  50% (zero replacements/red pixels). It is not a compiler/API failure, and no
  tracked source is modified for that negative run.
- Strict Win64 objects compile for production hooks/override and extended real
  MinHook EGL/WGL/native-image fixtures. Those authored Windows tests check
  private samplers **inside** the originals, exact native results/arguments,
  failed binding/query/unsupported target/format/level/buffer, distinct aspects,
  unobserved/protected routes, nonsharing namespaces, no pre-original publish,
  target/revision/retirement changes inside originals, exact Off and expiry.
  **Compilation is not Windows execution**; exact-head CI conclusions and the
  Windows package's uploaded-ZIP checks belong on draft PR121.

### Explicit graphics-driver matrix, not a skipped assertion

The first Windows matrix at `572ea30` passed all three native hook/Qt jobs,
Linux Qt, inventory, Chromium and Windows package/uploaded-ZIP checks. Its
Mesa job **failed** at the native-image pixels; that run is not a shader-matrix
pass. The existing five-strength, immutable/view, source-restoration, transfer
and material controls had passed before that failure.

The same failure was then reproduced on a locally built **Mesa 25.2.8 softpipe**.
A separate raw EGL/GLES program, **without loading any BSI code**, gets a
successful `eglBindTexImage`, a defined 64x64 RGBA image and **transparent black
(0,0,0,0)** instead of its red **(255,0,0,173)** source. Fresh independent runs
with application `glFlush`, `glFinish`, `eglWaitClient` and `eglSwapBuffers` all
fail the same raw full-frame comparison. This is not a production ownership or
sampler refusal, and no extra wait/copy is added to the deployed hooks for it.

CI therefore keeps **all existing Mesa GLES/desktop pixel assertions** and adds
a separate **mandatory ANGLE/SwiftShader native-image step**, using the pinned
runtime already installed for the independent sanitizer ICD. Enable that
specific fixture with `BSI_TEST_NATIVE_IMAGE_BINDING=1`,
`BSI_TEST_TEXTURE_QUALITY=1`, `BSI_SHADERS_EGL_CLIENT_VERSION=3` and
`BSI_SHADERS_EGL_PLATFORM=angle-swiftshader`, with `BSI_SHADERS_EGL_LIBRARY`
pointing to that runtime's `libEGL.so`. Requesting the fixture on desktop GL,
GLES 2 or without texture-quality tests fails rather than silently bypassing it.
There is **no driver-name conditional, runtime-dependent skip or relaxed
pixel/Off/error assertion**. Explicitly requesting it on Mesa still fails at
the raw comparison **before production observation**. Mesa and ANGLE artifacts
remain in separate folders, and the native-image report names its actual
renderer/version and raw baseline. **This does not claim Mesa native-pbuffer
export support or physical-driver certification.**

This independently executed native route is **not established as the game's
root cause**. EGLImage/private/external-memory aliases, unobserved native
lifetimes/sharing, unsupported shader/texture kinds, another API/process and
complete physical-driver coverage remain outside this proof. No generated
mipmaps, source mutation, final-frame blur, resolution reduction, allocation/FPS
claim, account-safety promise, original DLL redistribution, merge, tag or public
release. The installed game stays **not working** until actual texture/Off
behavior proves otherwise; authored pixels and prepared counters cannot supply
that acceptance.
