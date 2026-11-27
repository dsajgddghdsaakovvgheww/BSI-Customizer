# Potato: a legal base-only filter is an eligible descriptor, not a refusal

## The user-visible defect

The player's own rendering status (capture `gl-draw-write-footprint-v7-edge2`,
6757 frames, Automatic/OpenGL) reports

```text
Prepared Potato sampler bindings: 0 across 0 draw calls
Texture inspection: armed; inspected calls: 7206
7206 x no eligible mip sampler; first blocker non-mip-filter=1
source: 1600x900; internal-format=0x1908; source-min-filter=0x2601;
        base/max=0/1000; immutable=0
sampler2D slots=1; bound textures=1
```

Every inspected draw stopped at the same predicate in
`src/shaders/GlTextureOverride.cpp`: a `GL_LINEAR`/`GL_NEAREST` **minification
filter** was treated as a fatal `MipRefusal::NonMipFilter`. `levels` was forced
to `0`, `changed` stayed empty, the reason stayed `TextureDrawReason::NoSampler`
and no private sampler was ever prepared. That is exactly "Potato does nothing".

The refusal conflated two independent facts:

1. **Storage** — does the resource actually store levels beyond its base image?
2. **Sampling** — does the *current* filter read them?

`GL_LINEAR` says "not today". It does not say "they do not exist". A renderer is
free to keep a complete chain and still sample base-only (mip generation is often
budgeted per asset, and a UI-ish or distance-clamped material keeps the identity
filter). A min-filter override in a **private** sampler is precisely the
mechanism that makes stored levels reachable — which is what the hash-pinned
reference DLLs do: in the recorded probe their source sampler was
`GL_NEAREST_MIPMAP_NEAREST` over a complete chain, and they selected mip 2 at
native lambda 0 while our floor-only plan did not
([probe](POTATO-DLL-PROBE.md#bias-versus-minimum-lod-floor-a-second-measured-difference)).

## What changed (no gate was deleted)

| Layer | Before | Now |
| --- | --- | --- |
| `inspectMips()` | `return refuse(NonMipFilter)` before walking levels | keeps walking the **stored** chain; a base-only filter is recorded, never fatal |
| `inspectMips()` undefined level | `Incomplete` for any filter | with a base-only filter an undefined level ends the chain (reported `single-level`); a **defined** wrong-size/format level is still `Incomplete` |
| `TextureSampling` | `mipmapped` only | new `storedChain` flag, set only by a caller that verified `visibleMipLevels>=2` |
| `planTextureQuality` | `!source.mipmapped` ⇒ `NoMipChain` | `!mipmapped && !storedChain` ⇒ `NoMipChain`; a single-level proof is still refused |
| `planGlTextureQuality` | descriptor never changed filters | for a stored chain with a positive planned floor, the **private** descriptor gains mip selection in the *same family*: `0x2601→0x2703`, `0x2600→0x2702`; an already mipmapped filter is preserved byte-for-byte |
| report | `non-mip-filter=N` | `base-only-filter=N (accepted when levels exist)`; a successful draw now reports how many base-only samplers qualified |

Everything else is unchanged: protection, level range, dimensions, color format,
draw-attachment feedback, blend, primitives, feedback loops, descriptor cloning
and the per-draw restore. No mipmap is generated, no source texture or source
sampler is edited, and no level is invented for a one-level resource.

The mip filter of the private sampler is **LINEAR** in both families because the
saved percentage is fractional (`1.5/3.0 × %`): a mip-*nearest* qualifier
quantized 50% and 75% onto the same level and made both frames byte-identical in
the measured run below. A base-only source has no mip interpolation to preserve,
so this changes nothing the application had; within-level filtering is preserved
exactly (`NEAREST` stays `NEAREST`).

## What is measured, on a real driver

Real ANGLE/SwiftShader GLES 3.1 (`egl-1.5`, Vulkan 1.3 SwiftShader), production
C++ (`GlTextureOverride`), via `tests/shaders_potato_emulator_game_test.py`:

| Asset descriptor | Native | 25% | 50% | 75% | 100% | Light 100% |
| --- | --- | --- | --- | --- | --- | --- |
| trilinear `0x2703` over chain (unchanged path) | 24735678 | 24137672 | 16861090 | 11648312 | 9713292 | 16861090 |
| **`GL_LINEAR 0x2601` over the same chain (player's shape)** | 24735678 | 24137672 | 16861090 | 11648312 | 9713292 | 16861090 |

Both shapes prepare 12/12 qualified scene draws per frame, leave the blended UI
pass and the game's own `MIN_LOD`/filter state untouched, apply live
100→25→100→Off on the next frame, and restore the native presented frame byte for
byte when Off. Before the change the same fixture returned **0 bindings at every
strength** (`ADAPTER-BEGIN count=0`), matching the player's report.

The same run keeps the negative control discriminating: with the production apply
path neutered, the harness exits non-zero at `expected 12 ... got 0`.

Single-level fixtures do **not** move. `texture_quality_gl_pixels.py` and
`native_texture_binding_gl_pixels.py` still assert `count == 0` for their legal
one-level compositor inputs (`MAX_LEVEL=0`), now via `base-only-filter=1 ...
single-level=1` rather than the old field name.

## What is still not proven

This is an authored game-shaped frame loop and a real software GL driver, not a
captured game, a physical GPU, a Windows detour or the player's own compositor.
It cannot show that the player's inspected 1600x900 input belongs to a texture
that stores more than one level — the captured status does not contain that
fact. If the game's asset really has a single level, this path correctly stays
native and the result would remain zero; the new report distinguishes the two
cases (`single-level=N` versus `base-only-filter=N ... qualified`), so the next
in-game status says which one it is without a new diagnostic build.

Off/source-change/Save still restore native sampling, and `No file was changed.`
is unchanged.
