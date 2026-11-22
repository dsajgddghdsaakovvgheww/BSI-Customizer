# Potato: follow native pbuffer outputs through color copies/resolves

## Newer installed result and eligibility follow-up

The `ab8b27e` implementation below passed all six shader CI jobs, actual Mesa
desktop/GLES transfer pixels and Windows package verification. Its subsequent
installed report nevertheless has **1,812 grading frames, 3,624 sampler
inspections/all NoSampler, 3,622 selected inputs, TWO resource-linked producer
inspections but zero replacements, and zero qualified transfer observations**.
The [immutable-mip qualification follow-up](POTATO-MIP-QUALIFICATION.md) repairs
another independently reproduced false rejection and retains the producer's own
reason. It does not diagnose this game as using that storage or relabel the
negative result as working Potato. This document retains the transfer mechanism
and its original validation scope; newer exact-head evidence belongs on PR121.

## Previous installed failure that motivated the transfer repair

The user reported **1,794 working OpenGL grading frames, 1,657 compatibility
sampler inspections/NoSampler, 1,657 presentation-input draws/color bindings,
and ZERO linked producer inspections/preparations/bindings** after delivery of
the first resource-producer candidate. The latest sampler has one bound
sampler2D, one combined mip/color/protection rejection, and no feedback,
descriptor-policy or private-sampler failures. Depth/blend/other/nested counts
are zero. See the [full evidence and history](FREE-FIRE-RENDERER-ANALYSIS.md).

That establishes working presentation/input observation, not genuine Potato
textures. Missing mipmaps, unsaved settings, inactivity, a Qt mismatch, a depth
failure or a particular offscreen implementation are **not** diagnosed. No
installed module hash or actual texture/Off/FPS acceptance was supplied. These
changes repair independently reproduced production gaps; they do not relabel
this negative game result as a success.

## Missing paths reproduced and implemented

The first repair represented only texture-backed draw-FBO outputs. It could not
follow any of these paths, even with an observed shared context namespace:

```text
material sampler2D + existing mips -> native pbuffer DEFAULT color
                                             | CopyTexSubImage2D
                                             v
                                  one-level texture T -> selected compositor

material sampler2D + existing mips -> renderbuffer R
                                             | BlitFramebuffer color resolve
                                             v
                                  one-level texture T -> selected compositor
```

It also excluded every WGL pbuffer DC before optional GL inspection. The new
implementation follows an exact recent output dependency back through these
submitted color transfers, then applies the existing private sampler at the
material draw, **not the one-level compositor or final frame**. Default-pbuffer
and renderbuffer identities are typed separately from texture names.

### Why these are viable paths, not a game-specific diagnosis

Pinned Android gfxstream source demonstrates native WGL pbuffer acquisition and
shared-context creation, plus default-read-surface copies and multisample
resolves. The inspected tree is **0eb17af5d0216fce174aa63abe6098367931a20f**:

- [`egl_os_api_wgl.cpp`](https://github.com/google/gfxstream/blob/0eb17af5d0216fce174aa63abe6098367931a20f/host/gl/glestranslator/egl/egl_os_api_wgl.cpp):
  WGL pbuffer Get/Release/Destroy, refreshed DCs, attributed context sharing and
  successful `wglShareLists`. A driver pbuffer can have a hidden native window.
- [`color_buffer_gl.cpp`](https://github.com/google/gfxstream/blob/0eb17af5d0216fce174aa63abe6098367931a20f/host/gl/color_buffer_gl.cpp),
  `ColorBufferGl::blitFromCurrentReadBuffer`: default READ framebuffer,
  `glCopyTexSubImage2D`, a multisample `glBlitFramebuffer` resolve, and a later
  texture draw. It also illustrates EGLImage/private fast paths **not** covered
  by this patch.
- [`EXT_protected_textures`](https://github.com/KhronosGroup/OpenGL-Registry/blob/main/extensions/EXT/EXT_protected_textures.txt):
  public texture-protection/context requirements; a public numeric object name
  is not a binding-free per-attachment protection query.

This is research into a plausible host-rendering pattern, **not evidence that
the user's BlueStacks/Free Fire uses this revision, code or exact path**. No
third-party implementation or original Potato DLL binary/code is incorporated.

## Native routing and resource proof

- **No presentation adoption from a draw, pbuffer or share group.** A qualified
  owned swap still selects the exact provider/display/game child/source. The
  old depth-qualified policy retains its exact-drawable control behavior.
- **Positive WGL pbuffer classification only.** An actual successful
  `wglGetPbufferDCARB/EXT` identifies its exact pbuffer/DC after that get family's
  release/destroy hooks are installed. This works for a pre-existing pbuffer
  only after an observed successful get; arbitrary pre-existing DCs remain
  excluded. Successful gets/releases maintain bounded leases. Last release
  excludes that DC until another observed get; refresh preserves the same
  pbuffer lifetime. Successful destruction retires its generation. Reusing
  either/both native handles cannot inherit the old resource proof. Failed
  native calls preserve their results and do not publish false lifecycle events.
- **A hidden HWND is not a foreign window exception.** Some driver pbuffer DCs
  have a hidden HWND. Only the exact DC returned by the real native pbuffer get
  is classified; an arbitrary DC on that HWND is not adopted. The pbuffer still
  needs a same/observed-successfully-shared namespace AND a matching recent
  output dependency. Ordinary Qt/Chrome/foreign windows stay excluded.
- **EGL boundaries stay explicit.** Only observed unprotected EGL pbuffer
  creations qualify. Legal current-surface/context protection queries precede
  optional GL initialization. The actual READ surface/DC is obtained separately
  (`eglGetCurrentSurface(EGL_READ)` / WGL current-read-DC extension), not inferred
  from DRAW. Unknown/protected/default foreign READ routes cannot be a copy's
  upstream pbuffer identity.
- **Typed resource dependency, not numeric coincidence.** A texture, RBO and
  pbuffer generation with equal numbers are distinct. A pbuffer/default draw or
  RBO draw cannot seed its own ownership. Only its exact observed color transfer
  into an already consumed dependency (or a selected default-output blit) can
  extend the path. Unrelated destinations/outputs remain native. Other MRT
  outputs are not authorized using buffer zero's write mask.

## Transfer capture is read-only and bounded

Core/EXT/ANGLE/NV `glBlitFramebuffer` and core/EXT `glCopyTexSubImage2D` targets
preserve every original argument and call their original **once**. Capture
requires supported GL 3.3 / GLES 3.1, complete READ/DRAW FBOs as relevant, actual
read/draw buffer selection, normalized RGB color metadata, bounded nonzero
rectangles and distinct typed resources. Copy capture accepts only ordinary
2D level-zero subimage destinations with matching base/dimensions/format and
legal per-bound-texture protection inspection. Mixed depth/stencil, integer/HDR,
unsupported targets/levels, zero/invalid dimensions, feedback and incomplete
framebuffers do not become evidence.

Copy/blit operations ignore draw blend state and color write masks by API
contract, so those **do not incorrectly suppress transfer tracing**. They still
suppress the actual material draw where required by the existing draw policy.
No source binding, source parameter, shader, framebuffer, texture storage or
image pixel is changed for observation. No `glGetError`, readback, generated
mipmaps, new game texture allocation, private sampler on a copy, resize or blur
is introduced. Default attachments use legal GLES `BACK` versus desktop core
`FRONT/BACK_LEFT/RIGHT` metadata queries.

Framebuffer-attached textures/RBOs have no binding-free core GLES per-object
protection query. Where protected textures are supported, **those transfer
attachments and newly introduced RBO draw outputs fail closed**, rather than
being presumed unprotected from their names. A default-pbuffer copy to an
actually bound, queried unprotected texture can still qualify. Native protected
contexts/surfaces and per-sampled-texture protection exclusions remain intact.

Only value identities are saved across the original transfer. No `renderMutex`,
private sampler or GL-state lease is held across that native call; a driver may
join workers or dispatch a later real draw. Nested transfer aliases do not
publish duplicate evidence. After the original returns, publication requires
unchanged selected-source/settings revision, current native context/drawable
and lifecycle serial. No session pointer survives the unlocked original. A
revision change or object retirement during the original rejects the capture.

These are **binding/submitted-copy observations**, not proof a shader sampled a
texel, a draw rasterized, a void GL operation succeeded or game pixels changed.
Queries do not consume errors to pretend to establish success. Root expiry is
two selected-present frames; each of eight bounded backwards hops inherits
that expiry and cannot renew itself. Storage remains fixed at 64 contexts and
128 dependencies; WGL classification has 128 lifetime entries and bounded
leases. Unknown sharing is never reconstructed from names or share membership.

### Object deletion overflow must not permanently destroy known sharing

The old queue fallback forgot a provider's namespaces even for a deletion list
above the capture bound or object-deletion queue overflow. A one-off large
batch could therefore erase observed sharing permanently, even after fresh
presentation consumption, until another native context/share call occurred.

The fallback now distinguishes **objects** from **context topology**. Lost or
oversized texture/RBO retirement details invalidate all resource evidence,
including inherited ancestors, but retain known context identities/sharing.
Fresh selected consumption/copy can relearn. Lost context creation/share events
still forget namespaces too, because handle reuse could have been missed.
Captures compare against the last **drained** lifecycle serial, so merely seeing
a newer but unprocessed event generation is not permission to reuse old proof.
Callbacks never wait on the render mutex; fixed-queue contention stays fail
closed. This relies on normal host shared-object synchronization, not a universal
interop/driver-internal lifetime oracle or extra cross-context GPU fences.

## Validation and limits

Executed locally for this patch:

- **8/8 non-Qt native CTest**, including mandatory real SwiftShader Vulkan;
  **8/8 ASan/UBSan/leak detection** on the same required driver. These are not
  the full Qt/Windows matrix and are not a claim of local Qt SDK availability.
- Actual **production GLES 3.1 ANGLE/SwiftShader** pixels for both new paths:
  material replacement **1**, unchanged one-level compositor replacement **0**,
  native **(255,0,0,173)** -> Strong **(255,255,0,173)**; the **entire Off frame
  equals native byte-for-byte**, at unchanged 64x64 resolution. Both paths test
  expiry, absent native-READ proof, ignored copy blend/mask state and preservation
  of a pending application error. Existing 128 exact Off/source-LOD cycles and
  authored spatial-material reduction still pass.
- **Two independent negative controls** in disposable source copies disable only
  RBO or default-pbuffer producer authorization. Each passes earlier direct
  material/metadata/API controls, then fails the intended new upstream material
  mip/pixel assertion. Missing APIs or compilation failure are not accepted as
  the negative result.
- Portable metadata/policy/registry doubles cover invalid arguments, typed
  identity, attachment format/protection/completeness, no allocation/binding
  changes, native leases, handle/DC reuse, ambiguity/capacity/overflow, TTL,
  revisions and inherited retirement.
- Strict Windows x64 compilation of production hooks/override/runtime and both
  EGL/WGL lifecycle/draw/transfer tests passes. The explicitly named WGL fixture
  DLL also links with verified WGL exports. **Compilation/linking is not
  execution.** The new Windows native regression must execute on exact-head CI:
  same/shared versus isolated/failed sharing, hidden-window DCs, successful and
  failed native lifecycle calls, live leases/refresh/reuse, default-pbuffer copy,
  RBO resolve, distinct READ drawable, original arguments/results, private
  desktop bias+floor samplers inside draws, no pre-original publication,
  revision/deletion changes inside copies, oversized/overflowed deletion
  relearning, expiry, Off and source teardown. The WGL provider override is
  test-only, compiled out of the deployed module, never a system DLL proxy.

The existing workflow additionally requires actual Mesa **desktop core AND
GLES** software pixels, Windows hooks and independent Qt 5/6 plugin/Settings
execution. Exact-head conclusions and retrieved pixel evidence belong on draft
[PR121](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/pull/121); pending
CI is not a pass. No stock Settings UI, native callback identity, Save/Discard,
Shaders-before-About order, restart dialog or About/no-growth rule is changed.

**Installed Free Fire remains unresolved.** This does not cover EGLImage/private
fast-copy aliases, native interop, another API/process, unobserved sharing,
unsupported shader/sampler kinds or every attachment/protection configuration.
Opaque mipmapped HUD/RGB-packed data can still change under the disclosed
compatibility contract. Existing-mip sampling does not reduce allocation size.
No FPS improvement, physical GPU matrix, universal API/GPU support or account
safety is claimed. No original DLL redistribution, blind deployment, merge,
tag or public release is performed.
