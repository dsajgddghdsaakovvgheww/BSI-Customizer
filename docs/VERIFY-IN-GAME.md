# Verifying the frame Smoothness in *your* game

Nothing in this repository can see your game. The tests here run on software
renderers (ANGLE/SwiftShader, Mesa llvmpipe, WARP) and prove the shader maths, the
constant-block layout and the schema migration. They cannot prove that your GPU
presented a processed frame. Only your process can report that.

This build reports it. Three independent signals, in order of how much they prove.

## 1. A frame counter that only moves when the picture is processed

`processedFrames` increments **only** when the production post-process pass actually
rendered on the identified game surface (`PresentationHooks`: `report(..., rendered,
...)`), not when hooks attach, not when a texture is inspected, and not when
Potato's texture path is armed. It is incremented on the same call that runs the
shader over your frame.

So after a game has been running for a while with Smoothness above 0:

* counter climbing → the presented frame went through the pass;
* counter stuck at 0 → the pass is bypassed on your route, and the token's `status`
  line says why.

The status file now carries it:

```json
{
  "frameSmoothness": 40,
  "framePassEnabled": true,
  "framePassNeutral": false,
  "processedFrames": 812,
  "renderer": "OpenGL",
  "active": true,
  "status": "..."
}
```

and the token is rewritten whenever that counter changes, so it cannot freeze at a
stale first value.

## 2. One command turns the file into an answer

```bash
python scripts/Analyze-PotatoStatus.py --token "<the path the app prints at startup>"
```

```
FRAME PASS: frame_active
  The frame pass rendered 812 game frame(s) on the identified game surface with
  Smoothness at 40. The picture you saw was processed; compare it against 0 to see
  the difference for yourself.
TEXTURE VERDICT: no_eligible_sampler
  ...
```

The two answers are deliberately separate:

| FRAME PASS key | meaning |
| --- | --- |
| `frame_active` | frames rendered **with** Smoothness above 0 |
| `frame_active_not_smoothing` | the pass works, Smoothness is 0 — raise it |
| `frame_off` | the post-process pass is off/neutral; nothing is drawn |
| `frame_bypassed` | enabled but never rendered; DirectX/Vulkan-only games keep native frames |
| unknown | the token predates these fields; nothing can be concluded |

`FRAME PASS: unknown` and a missing token are reported as absent measurements, never
as success.

## 3. The honest human check

Two captures of the same scene at Smoothness 0 and Smoothness 75 do differ in this
repository's own software-renderer tests (`1312629 -> 886955` detail energy), and at
0 the frame is byte-identical to native. Between those two extremes your eyes are the
only instrument that matters: if 75 does not look softer than 0 in your game, the
counter plus the token's `status` line will say whether the pass ran at all.

## What is still not provable from here

* Your physical GPU/driver.
* Frame rate or input latency on your hardware.
* That a DirectX/Vulkan-only game is affected — it keeps its original frames.
* Anything about account safety or a "detection" guarantee.
