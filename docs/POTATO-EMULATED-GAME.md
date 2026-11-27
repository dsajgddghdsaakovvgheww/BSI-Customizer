# Potato in an emulated game loop (GitHub Actions proof)

## What this answers

The question was: *can a CI environment prove Potato works the way it has to
work inside an emulator playing a game?* The answer in this change is **yes for
the player-facing behaviour, no for a captured Android game**:

- `tests/shaders_potato_emulator_game_test.py` composes a game-shaped GLES 3
  frame loop and runs the **production** `BsiShaders::GlTextureOverride` and
  `BsiShaders::GlTextureProvenance` between the draws, in the same call order the
  player's OpenGL hooks use.
- The `emulated-game` job in `.github/workflows/shaders.yml` runs it on **two
  independent software drivers** (Mesa llvmpipe and ANGLE/SwiftShader), then
  re-runs it against a broken copy of production and requires that run to FAIL.
- A missing GL driver fails the job. Nothing here is skipped, and the job
  summary repeats the scope limits.

## The emulated game

| Element | Detail |
| --- | --- |
| Asset | 256×256 colour material with a real `glGenerateMipmap` chain (83 levels worth of storage, 9 present), high-frequency bands/checker/gradient |
| Scene draw | indexed `glDrawElements` of a full-frame triangle, `GL_TRIANGLES`, depth test `LEQUAL` + depth write, `gl_FragCoord` 1:1 asset read |
| HUD pass | blended (`GL_BLEND`, SRC_ALPHA/ONE_MINUS_SRC_ALPHA) draw of the same material |
| Frames | 6 presented frames per setting, 2 qualified scene draws per frame |
| Sampler sources | (a) the game's own sampler object, (b) only the texture's parameters — both must work |
| Settings | Off/0 and 25/50/75/100 % in **Strong**, plus **Light (Natural)** at 100 %, each rendered and read back |
| Live change | the slider moves 100 → 25 → 100 → Off while frames keep rendering; each change must land on the next frame |

Per strength the harness asserts, not merely prints:

1. a private sampler is bound for the draw (never the game's object) with the
   expected floor `3 × strength / 100` (Strong mode), while the game's own
   `MIN_LOD` stays `-1000` **during** the draw;
2. exactly `frames × sceneDraws` qualified draws are prepared, and **zero**
   blended HUD draws are touched;
3. the presented frame differs from native for every non-zero strength and is
   **byte-identical** for Off;
4. presented detail energy falls monotonically with strength and every saved
   strength produces a distinct frame (a binding count alone is never accepted);
5. Off after use restores the previous presented frame exactly;
6. the depth-qualified policy still refuses a depth-disabled draw at 100 %;
7. Light/Natural is visible but stays lighter than Strong (floor 1.5 vs 3);
8. live setting changes apply on the next presented frame and Off returns the
   byte-identical native frame;
9. the game's texture/sampler LOD state is unchanged at the end and the driver
   reports no GL error.

Measured locally with ANGLE/SwiftShader (same binary as CI):

```
native 24 735 678 -> 0 %: 24 735 678 | 25 %: 24 137 672 | 50 %: 16 861 090
75 %: 11 648 312 | 100 %: 9 713 292        (presented detail energy, sampler mode)
```

(The texture-parameter source mode produces the same numbers here; that is the
expected equivalence, not a duplicated test.) Light/Natural at 100 % presents
`16 861 090`, i.e. the lighter floor and not Strong's 9.7 M.

CI evidence for this job: run [45930829803](https://github.com/MrPcGamerYT/BSI-Customizer/actions/runs/45930829803)
— steps *Emulated game on Mesa*, *Emulated game on ANGLE/SwiftShader* and
*Negative control* all succeeded.

## Negative control (why a green run means something)

The harness accepts `BSI_POTATO_ADAPTER_SOURCES` **only** so it can be compiled
against a neutered copy of `src/shaders/GlTextureOverride.cpp`. CI disables the
apply path in a temporary copy and requires exit status ≠ 0:

```
AssertionError: strength 25: expected 12 qualified scene draws with a private sampler, got 0
```

The repository file is checked again with `git diff --exit-code` after the
negative control, so a green job cannot come from edited production code.

## Where this sits relative to the supplied players

The four uploaded HD-Player samples (`MrPcGamerYT/HD-Player`) were statically
audited again for this change (PE parsing only; no execution, no extraction):

- All four samples — `HD-Player MSI v5.9.exe` 5.9.300.6315 and
  `HD-Player v.5.10 bluestacks.exe` 5.10.150.1016 (x64, Qt 5), plus the two ZIPs
  `HD-Player MSI Latest version.zip` 5.22.75.6322 and
  `HD-Player latest version Bluestacks 5.zip` 5.22.285.1013 (x64, Qt 6) — are
  launchers that import `d3d11.dll`/`dxgi.dll` only for their own UI and reach
  the guest renderer through `LoadLibraryW` + `GetProcAddress`-resolved
  pointers: `HD-Opengl-Native` and `libOpenglRender`, with the
  `imgdOnEglCreateContext` / `tmapOnEglCreateContext` callbacks registered at
  startup. Every uploaded version therefore feeds the game through the OpenGL
  renderer, which is exactly the layer this harness exercises — the launcher's
  D3D11 import is not a game-render path and no version exposes a second one.
- The renderer providers themselves (`both dll.zip`: `HD-Opengl-Native.dll`
  5.12.120.6303, `libOpenglRender.dll`) expose `grApiInit` and draw through
  WGL/GDI (`SwapBuffers`, `ChoosePixelFormat`), which is why the sampler/LOD
  contract is the right place to prove game-visible behaviour.

What that static evidence does **not** prove, and this harness therefore does
not claim: which provider path a launched game takes, Windows detour/presentation
correctness, hardware driver behaviour, or FPS.

## Reproduce locally

```bash
# any real software GLES driver; SwiftShader example
npm install --prefix /tmp/icd --no-fund --no-audit --save-exact @sparticuz/chromium@133.0.0
node -e "const fs=require('node:fs'),z=require('node:zlib');fs.writeFileSync('/tmp/icd/swiftshader.tar',z.brotliDecompressSync(fs.readFileSync('/tmp/icd/node_modules/@sparticuz/chromium/bin/swiftshader.tar.br')));"
mkdir -p /tmp/icd/sw && tar -xf /tmp/icd/swiftshader.tar -C /tmp/icd/sw

BSI_SHADERS_EGL_LIBRARY=/tmp/icd/sw/libEGL.so BSI_SHADERS_EGL_PLATFORM=angle-swiftshader \
BSI_TEXTURE_PIXEL_ARTIFACT_DIR=/tmp/potato-frames \
  python3 tests/shaders_potato_emulator_game_test.py
```

Artifacts: `emulated-game-<mode>-native.png`, `emulated-game-<mode>-strength-<n>.png`
and `emulated-game-evidence.json` (per-mode native and per-strength detail
energy). PNG frames are written by the test itself (zlib only, no dependency).

## Remaining limits (deliberate)

- A software renderer stands in for the Android guest; it proves API/pixel
  behaviour, not GPU/vendor quirks or performance.
- Hook installation, MinHook detours, EGL/WGL dispatch attribution, window
  ownership and presentation are covered by their own Windows tests, not here.
- DirectX 11 and Vulkan have the strength plumbing and policy tests only; this
  harness does not claim in-process texture binding for those APIs.
