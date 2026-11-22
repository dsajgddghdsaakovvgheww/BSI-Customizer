# Potato: reject unsupported resources, not valid immutable mip chains

## Latest installed result remains negative

The follow-up after the `ab8b27e` WGL-pbuffer/color-transfer candidate reports:

| Observation | Latest value |
| --- | ---: |
| OpenGL grading frames | 1,812 |
| Compatibility calls / sampler inspections / NoSampler | 3,624 / 3,624 / 3,624 |
| Prepared draws / private sampler bindings | 0 / 0 |
| Selected presentation-input draws / color bindings | 3,622 / 3,622 |
| Resource-qualified producer inspections / prepared / bindings | 2 / 0 / 0 |
| Qualified color transfer calls / inputs | 0 / 0 |
| Depth / blend / other / nested bypasses | all zero |

Automatic is requested; actual/configured host API is OpenGL. Qt bridge/host is
5.15.2/5.15.4. Vulkan has initialization only, no surfaces/swapchains/presents.
The last recorded sampler has one sampler2D slot, one binding and one combined
mip/color/protection rejection; descriptor-policy/private-sampler failures are
zero. See [renderer history](FREE-FIRE-RENDERER-ANALYSIS.md).

This is **not working game-texture Potato**. Unlike the preceding result, two
nonselected resource-linked draws reached sampler inspection, but neither
prepared a replacement. Most counted inspections are still on the selected
presentation route. The report does not identify what those two draws render,
prove a missing mip chain, or establish that every upstream material draw was
captured. Zero *qualified recorded* transfers is not proof that no copy/blit API
was called. No unsaved-setting, inactivity, Qt-patch, depth, stale-install or
GPU-vendor diagnosis follows from these counters.

## Why the supplied DLL can act while ours refuses

The [supplied-DLL experiment](POTATO-DLL-PROBE.md) independently executed all five
unique original identities on Windows software WGL. Their bind/draw hooks set
application texture/sampler LOD bias and, for some identities, minimum LOD.
These changes **persist after the draw**, including changes applied on the
WGL-resolved bind path before the first draw. They do not use our per-draw
complete-mip/color/resource-ownership classifier. The original also has measured
entry-point coverage gaps; its successful fixture is not universal coverage.

Our desktop bias-plus-floor math already matches the tested Strong mip pixels.
The remaining distinction is not simply "a smaller strength": our replacement
must first reach a resource-qualified draw and accept that bound color mip view,
then borrow a private sampler only until that draw returns. Persistent original
object mutation can affect a later use which our hook never reaches. Conversely,
it does not provide our exact in-place Off/source-state/concurrency contract.

That explains a **mechanism/coverage difference**, not the exact unobserved game
path. Blindly removing context/protection checks, adopting a share group or
mutating every process texture would not demonstrate a safe repair. The next
section fixes an independently reproduced *incorrect* rejection instead.

## Concrete bug: immutable storage does not require a 1x1 tail

The old `GlTextureOverride::mipLevels()` used the texture's stored
`GL_TEXTURE_MAX_LEVEL` (normally **1000**) as its only upper bound. For a 64x64
texture it queried every image down to 1x1. A valid
`glTexStorage2D(TEXTURE_2D, 4, RGBA8, 64, 64)` allocates exactly four images:
64x64, 32x32, 16x16, 8x8. OpenGL automatically restricts sampling to that immutable
storage; it **does not rewrite the stored MAX_LEVEL to 3**. The old code queried
unallocated level 4, found no matching image and returned NoSampler.

This was reproduced **before the production change** on actual GLES 3.1
ANGLE/SwiftShader: the native real shader sampled red, the driver reported
immutable levels=4 / MAX_LEVEL=1000, earlier direct-mip/Off controls passed, and
the new assertion requiring the production Strong draw to sample yellow failed.
It was not a missing driver, compile failure or fabricated sampler double.
After the repair the same production draw prepares one private binding, samples
yellow, and restores the entire native Off image exactly.

**This does not prove the user's two producer draws use immutable storage.** The
old combined diagnostic could not distinguish this false rejection from
unsupported formats, dimensions, protection or a genuinely unavailable chain.

### Standards checked

Pinned Khronos registry tree **6af574a14089ccfee87efe230ebcdd8742859813**:

- [ARB_texture_storage](https://github.com/KhronosGroup/OpenGL-Registry/blob/6af574a14089ccfee87efe230ebcdd8742859813/extensions/ARB/ARB_texture_storage.txt):
  immutable sampling clamps effective base/max levels to storage, without
  rewriting their stored texture parameter values.
- [ARB_texture_view](https://github.com/KhronosGroup/OpenGL-Registry/blob/6af574a14089ccfee87efe230ebcdd8742859813/extensions/ARB/ARB_texture_view.txt),
  particularly issue 11: a view's `TEXTURE_IMMUTABLE_LEVELS` is inherited from
  its **parent**; `TEXTURE_VIEW_NUM_LEVELS` describes its exposed subset.
  `TEXTURE_IMMUTABLE_LEVELS` is introduced by texture_view, not by the older
  desktop texture_storage extension alone.
- [EXT_texture_storage](https://github.com/KhronosGroup/OpenGL-Registry/blob/6af574a14089ccfee87efe230ebcdd8742859813/extensions/EXT/EXT_texture_storage.txt):
  the corresponding immutable storage/base/max behavior and API dependencies.

These are public API rules, not evidence of this game's storage implementation.
No third-party implementation, source code or original DLL is incorporated.

## Production repair and limits

- Qualify optional metadata by the actual GL version/extension list. Core GLES
  supports immutable format/count; desktop core 4.2 or texture_storage supplies
  immutable format, while core 4.3 / texture_view supplies count/view queries.
  **Never issue texture_view-only enums merely because storage is supported.**
- Clamp effective immutable base/max to the queried storage/view, including
  legal stored base values beyond that range. A texture view is bounded by its
  exposed level count, **not the parent's larger inherited storage count**.
- On desktop GL 4.2 / texture_storage without count queries, validate the
  contiguous defined immutable prefix and stop at its undefined terminal image.
  Immutability guarantees contiguous storage. A missing/mismatched required
  image on a **mutable** texture still fails; this is not permission to accept
  arbitrary incomplete chains.
- Validate every exposed required level's dimensions and matching color format.
  Keep the existing supported-dimensions, RGB/RGBA-format, feedback, protected
  resource, native-route, descriptor/review, blend and private-allocation rules.
  One effective level is not reducible. No new mipmaps, texture allocation,
  minification filter, source base/max or source sampler values are generated
  or changed.
- Bound image queries by the actual `GL_MAX_TEXTURE_SIZE`. A stored mutable
  BASE_LEVEL above its queryable image limit is rejected **before** an illegal
  image query can inject a GL error. Dependency observation uses the same bound.
- Preserve queried/bounded desktop private additive bias plus minimum floor and
  legal GLES floor-only. Both are clamped to actual **visible** mip levels.
  Restore exact borrowed bindings before any Off draw. The separately tested
  depth-qualified policy remains unchanged.
- Split the combined mip/color/protection bucket into first refusal counts and
  read-only metadata (protection, range, dimensions, format, required-level
  mismatch, one effective level, unqualified storage). Unqueried values are
  marked, not passed off as absent resources. These remain query observations,
  not a shader-consumption or GL-error success oracle.
- Keep the most recent actual **producer** inspection detail independently of
  the last presentation refusal. Thousands of later compositor calls must not
  overwrite the reason for the two upstream candidates. Settings/source/Off
  resets and stale-revision rejection also cover this retained detail.

Qualifying an already bound texture view's mip count **does not infer storage
alias ownership between different texture names**. EGLImage/private interop,
unobserved sharing and other API/process paths remain unsupported. The previous
[typed native pbuffer and color-transfer proof](POTATO-COLOR-TRANSFERS.md) is not
replaced by blanket context/share-group adoption.

## Execution evidence

Locally executed for this repair:

- Before-fix real-driver regression failed the intended immutable-material pixel
  assertion, after earlier native sampling/API controls passed.
- Actual GLES 3.1 ANGLE/SwiftShader after-fix pixels pass **five cases in both
  source modes** (existing sampler / texture-object parameters): four-level
  storage, two-level storage, nonzero base, explicit max level, and one-level
  refusal. Four levels change red -> yellow; two levels clamp Strong to floor 1
  and change red -> green. Entire Off frames are byte-exact native; source
  base/max/storage/LOD state and a pending application error are preserved.
- A real texture-view case additionally runs when the driver's public extension
  exists; the local ANGLE driver does **not** expose it, so local view pixels are
  not claimed. Portable production doubles test exposed view versus inherited
  parent counts on both APIs, old desktop capability boundaries and failures.
- Existing real direct/RBO-blit/default-pbuffer-copy producer/compositor pixels,
  128 exact Off/source-state cycles, depth-independent controls and full authored
  material-detail reduction remain passing.
- Non-Qt native CTest **8/8**, ASan/UBSan/leaks **8/8**, each including mandatory
  actual SwiftShader Vulkan pixels. Expanded production doubles include legal
  partial storage versus incomplete mutable chains, views, nonzero/extreme
  base/max, metadata failures and exact first blockers. Runtime tests retain the
  producer refusal across later presentation calls, then revoke it on Off,
  source destruction and stale reports.
- Seven strict Windows x64 objects compile: production hooks/override/runtime,
  EGL loader/hooks and WGL loader/hooks. Compilation is not execution. There is
  no local Qt SDK/Windows runtime claim.

The existing exact-head workflow must also execute Windows native/Qt 5/6 tests
and actual Mesa desktop-core/GLES pixels, including real texture views where
supported. Follow-up conclusions and Windows-verified candidate downloads are
recorded on [draft PR121](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/pull/121),
separately from the preceding successful `ab8b27e` CI and now-negative game report.
A pending workflow is not a pass.

**Installed Free Fire remains unresolved.** No exact in-game texture/Off,
physical-GPU matrix, FPS/allocated-memory saving, semantic-albedo guarantee,
universal API support or account safety is claimed. Opaque mipmapped HUD/RGB
data may change under compatibility. No stock QML/theme/navigation/Save/Discard,
callback identity, Shaders-before-About order or no-growth rule was edited. No
blind original-DLL deployment/redistribution, merge, tag or public release.
