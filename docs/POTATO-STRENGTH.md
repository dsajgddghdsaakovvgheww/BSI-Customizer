# Potato graphic strength: actual sampling, not a weaker-success indicator

## What the installed status means

The latest supplied game report has **3,624 NoSampler inspections, two linked
producer inspections and zero prepared draws/private bindings**. That is **no
applied Potato texture reduction**, not a small/weak effect. The 1,812 processed
frames refer to independent presentation grading. Neither those frames nor an
armed/saved switch establishes changed game textures.

The [immutable mip/view repair](POTATO-MIP-QUALIFICATION.md) independently fixes
an incorrect rejection and passes real desktop/GLES storage/view pixels. It is
not yet established as the cause of the game's two producer refusals. Increasing
strength cannot make an unsupported or unqualified resource eligible.

## Control and saved behavior

- **Potato strength** uses the player's stock `UiSlider`, from **0 to 100%** in
  integer steps, beneath the independent Potato graphic switch. Its native
  pointer/keyboard/theme behavior is retained; it is not a hand-painted control.
- **0% = native sampling**, **50% = former Light**, **100% = former Strong**.
  The previous On behavior is retained at 100%; this is not a newly increased
  maximum or an additional presentation filter.
- Changes remain a **draft until native Save changes**. Native No/Discard
  restores the saved percentage. Visual presets, Enhance and Reset visuals
  preserve it. The visual-effects checkbox and optional native Display size
  do not control the texture slider.
- A texture-only strength Save does not write Display, request a restart,
  generate mipmaps, resize a frame, reset a driver profile or edit a source
  texture/sampler. The existing explicit driver-review requirement remains.
- The UI and copied status distinguish saved strength, zero/native, no eligible
  override and prepared bindings. **Prepared bindings are still not a
  measurement of changed game texture pixels, memory savings or FPS.**

## Render-thread implementation

The saved integer reaches `TextureConfiguration`, its mutex-consistent route
snapshot, the real Windows draw hook and `GlTextureOverride::begin()`. For a
qualified texture, requested minimum LOD is `3 * strength / 100`, bounded by
actual exposed storage/view levels and the source sampler's maximum LOD.
Desktop OpenGL also uses the existing queried/bounded private additive bias;
GLES retains legal minimum-LOD-only behavior. The source descriptors/bindings
restore exactly after the original draw. A higher percentage can have the same
result on a short mip chain, a mip-quantizing filter or an already minified GLES
sample; percent is not percent allocation/FPS reduction.

Zero is an early native bypass, including dependency observations. A strength
change revokes old producer detail, provenance and counters through the existing
revision mechanism. Old in-flight results cannot be credited to the new strength.
Obsolete private sampler variants are released only in their current owning
context, after borrowed bindings restore, so repeated saves do not fill the
bounded 128-entry cache with the slider's history. Resource/protection/route,
blend, format, descriptor and source-override checks are unchanged.

## Preferences and migration

`bsi.shaders.v4` adds only `potatoStrength` (integer 0..100) to saved values,
including durable native Display transactions. Schemas v1/v2/v3 remain readable;
missing legacy strength becomes 100 **in memory**, preserving previous On/Off
and native-resolution recovery. Opening does not rewrite preferences. Explicit
Save writes v4 atomically with the existing external-edit/lock/journal checks.
Invalid, fractional, nonfinite, string, boolean or out-of-range percentages are
refused rather than coerced/clamped. Older binaries that cannot read v4 must not
be used to overwrite its retained native Display recovery evidence.

## Validation and limits

Executed locally for the slider:

- Native **8/8 CTest** and ASan/UBSan/leaks **8/8**, including actual SwiftShader
  Vulkan controls. Policy tests cover every integer percentage, zero, invalid
  values, old defaults, view/device limits and unchanged source descriptors.
- Actual production sampler doubles sweep **three complete 0..100 ranges** on
  each API, with bounded live private samplers, exact source/binding restoration
  and explicit release. Runtime tests reject stale old-strength evidence.
- Actual GLES 3.1 ANGLE/SwiftShader **0/25/50/75/100% pixels**, both source modes
  and native gradients 0/4, with full-frame exact Off, unchanged source LOD and
  preserved pending application error. At native gradient 0 all five outputs
  differ. Existing immutable/direct/transfer/material and grading pixels pass.
- Physical native Qt slider, keyboard, Save/Discard, zero, invalid bridge input,
  visual-reset independence, v1/v2/v3 migration/reopen/journal and actual Windows
  hook-to-original percentage assertions are added to the exact-head CI tests.
  Their execution conclusions and candidate package verification are recorded
  on [draft PR121](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/pull/121),
  not inferred from source or a local cross-compile.

These are authored software/resources/contracts, not installed Free Fire or
physical-GPU acceptance. DirectX/Vulkan game texture replacement is still not
implemented. Opaque mipmapped HUD/RGB data maps may also change. No allocated-
memory, FPS, universal GPU/API or account-safety promise; no original DLL/code
redistribution, public release, merge or tag.
