# Free Fire: host presentation is not scene-texture capture

## Latest installed-player follow-up: depth-independent inspection, 2,652 NoSampler

The user's **"still not working"** report supersedes the old depth-gate failure:

```text
Requested API: Automatic; configured / observed host API: OpenGL / OpenGL
Processed presentation-shader frames: 1326
Texture policy: Owned mip compatibility (depth independent); inspection armed: yes
Texture calls / sampler-inspection calls: 2652 / 2652
NoSampler: 2652; depth / blended / other bypass: 0 / 0 / 0
Prepared draws / overridden bindings: 0 / 0
All first-depth-blocker subcounts: 0; nested inspected / prepared: 0 / 0
Vulkan: 1 loader, 1 instance, 1 device; 0 surfaces/swapchains/presents
Qt bridge / host: 5.15.2 / 5.15.4
```

**Presentation grading works; Potato textures still do not.** All counted calls
now reach sampler inspection. Do not diagnose this run as TestDisabled, an
unsaved toggle, inactive gameplay or a Qt patch-version failure. `NoSampler`
combined shader layout/type/unit/binding, feedback, protection, mip/color,
descriptor policy and replacement failures; it does **not** prove no mipmaps.
The frame/call ratio does not prove complete scene coverage or two draws per
frame because their evidence intervals differ. The report establishes neither
a particular producer/API/process nor permission to adopt unknown resources.

## Current repair: follow a bound presentation input to its exact FBO producer

Saved On still uses [depth-independent owned-mip compatibility](POTATO-DEPTH-COMPATIBILITY.md).
A new [resource-linked producer path](POTATO-RESOURCE-PRODUCERS.md) repairs an
independently reproduced gap: a one-level compositor correctly has no reducible
mips, while the material draw that fills its input may use another drawable.
Previously even that **same context's** nonpresenting producer was never
inspected. The new path seeds resource evidence only from the selected
presentation's active color bindings, then requires the candidate FBO to write
the exact recently referenced texture in the same or **observed successfully
shared** namespace. A provider/share group or equal numeric names alone never
qualifies. Known unprotected EGL pbuffers are candidates, not new presentation
sources; unknown/foreign/Qt/protected routes remain excluded. The original
sampler/protection/mip/Off and nested/concurrency safeguards still apply.

Real software pixels reproduce **compositor bindings 0 -> linked material
producer bindings 1**, a changed full presented image and byte-exact full-frame
Off. A negative control disabling production producer qualification fails the
expected material-pixel assertion. This is a behavioral repair, not merely more
status text or a blur/resolution substitute. The new NoSampler detail also
separates discovered slots/bindings from mip/color/protection, descriptor and
private-sampler failures; producer/input observations have separate counters.

**This new repair has not been deployed to the reported game and is not its
established root cause or texture/FPS acceptance.** Unsupported shader/sampler
kinds, unobserved sharing, other processes/APIs and nontexture transfer paths
remain native. No repeat Save/gameplay/report request or blind DLL deployment
is proposed. Historical gate/guard statements below describe earlier revisions,
not this latest installed-player result.

## Previous installed-player follow-up: all 4,506 inspected calls had depth testing disabled

The saved/armed-On follow-up after the nested-wrapper repair remains a **failed
texture result**, despite working host OpenGL grading:

```text
Requested API: Automatic; configured / observed host API: OpenGL / OpenGL
Processed presentation-shader frames: 2375; texture inspected calls: 4506
First depth blocker: TestDisabled=4506; all other depth subcounts=0
Eligible / prepared calls / overridden bindings: 0 / 0 / 0
Nested inspected / nested prepared: 0 / 0
Qt bridge / host: 5.15.2 / 5.15.4
Vulkan: 1 loader, 1 instance, 1 device; 0 surfaces/swapchains/presents
```

The preceding 1,235-frame / 2,468-call follow-up had the same exact first blocker.
Every counted call stops **before** shader/sampler qualification. Zero later
blended/no-sampler buckets do not prove an absence of blending or mip chains;
those properties were not queried after the first rejection. Zero nested
inspected calls does not describe excluded/unhooked activity. This result does
not prove an offscreen producer, another renderer/process, or a safe resource
boundary to adopt. It also does not justify repeating the already answered
Save/gameplay/Settings-closed questions or asking for the same report again.

The [actual original-Potato-DLL experiment and separate reversible known-resource
comparison](POTATO-DLL-PROBE.md) have now executed successfully on Windows:
all five unique originals changed known mip pixels with depth disabled, and
the independent private-sampler alternative restored exact fixture Off pixels
and checked state over 64 cycles. A further independent private desktop
bias+floor sampler matches the original Strong pixels at native and minified
gradients and restores Off without mutating source objects; the floor-only plan
does not match that minified case. The subsequent [bounded desktop integration](POTATO-DESKTOP-BIAS.md)
used an actually queried device limit behind the existing classifier; it could
not remove this report's earlier depth rejection. The current compatibility
follow-up at the top of this page now explicitly addresses that mechanism
difference with a broader disclosed owned-mip contract, rather than merely
changing bias behind the same gate. Neither software experiment establishes
resource semantics, authorizes a foreign producer or supplies installed-game
texture acceptance; neither is a blind player deployment/retest request.
Original loading, detours and test-owned mip pixels must be measured separately
from actual game pixels, exact in-game Off restoration and FPS.

## Earlier report: confirmed active gameplay, working presentation, zero texture draws

The user supplied a full report and separately confirmed an advancing 3D scene
with Settings closed, not merely a lobby or paused Settings window:

```text
Configured hint / observed host API: OpenGL / OpenGL
Processed game frames: 13416; observed: yes; presentation processing: yes
Texture inspection armed: yes
Inspected calls: 27620; eligible scene calls: 0
Depth/UI bypass: 27620; blended / no-sampler / other bypass: 0 / 0 / 0
Prepared Potato draws / bindings: 0 / 0
Qt bridge / host: 5.15.2 / 5.15.4
Vulkan initialization: 1 loader, 1 instance, 1 device; 0 WSI surfaces/presents
```

**Texture Potato did not work in this active-gameplay run.** The earlier draw
signature/alternate-context repair did not resolve this case. This is not an
unsaved/off toggle, a native-resolution no-op, a failed Qt import, a missing
mip chain proved by inspection, or a reason to force Vulkan. Every inspected
call stopped at the *combined depth gate*, before program/texture/sampler
qualification. The old report does not distinguish disabled depth testing,
disabled depth writes, NEVER/ALWAYS, a missing attachment or insufficient depth
precision. It also does not prove that all actual game geometry reached this
hook or that a separate offscreen producer is safe to adopt. Presentation and
texture counters have different reset intervals; their ratio is not a measured
number of game draws per frame.

The follow-up repairs an independently testable guard gap: a normally bypassed
outer wrapper retained the render mutex and recursion guard until *after* its
original returned, suppressing any state-setting inner draw entry it called.
A zero-binding bypass now releases those leases **before the original**. A live
private override, an inspection exception, or presentation-internal work keeps
its guard; nested inspection is bounded to eight API levels. Native call/argument
forwarding and the exact owned-drawable/protection/depth/UI/resource gates stay
unchanged. The regression includes a depth-disabled wrapper whose original
establishes legal depth then calls another real detoured entry, and negatives
for two depth-disabled calls and a still-live outer override.

This gap is **not established as this player's root cause**. The follow-up also
reports each exact first depth blocker and nested inspected/prepared API calls,
without additional resource adoption or an appearance change. A continued zero
result must remain an unresolved/unsupported texture path, not be relabelled a
working effect. A real same-scene image change and exact off restoration are
still required; software/dispatch tests cannot supply that acceptance.

## Earlier user report: working host OpenGL presentation under a Vulkan hint

The user subsequently reported that the effects now work with Vulkan selected
and supplied these runtime facts:

```text
Requested API: Automatic
Configured emulator renderer (hint only): Vulkan
Observed game API: OpenGL
Processed game frames: 3704
Game surface observed: yes; processing active: yes
loaders=1; instances=1; devices=1; Win32 surfaces=0;
swapchains=0; presents=0; missing queues=0; excluded windows=0
Last hooked loader: vulkan-1.dll
Qt bridge: 5.15.2; host: 5.15.4
```

This is **user-reported owned GL/GLES presentation processing**, despite the
Vulkan configuration hint. Keep the actual API label: it is not a captured
Vulkan WSI path, identification of the Android guest API, Vulkan texture
processing, a measured FPS gain or universal player/GPU acceptance. It does
supersede the zero-frame symptom for that later session; do not keep claiming
that the user has no working effect/capture evidence.

The accompanying checked-Potato screenshot showed a required driver-review
validation refusal and **saved/runtime off**. Zero prepared draws while disabled
cannot diagnose sampler incompatibility. The UI repair reveals the explicit
review beside the toggle and distinguishes unsaved draft from saved/runtime
state; it does not auto-acknowledge the review or invent successful draws.
The separate resolution error said the selected profile was already at/below
its cap. Actual native dimensions were **not** supplied, so they are not inferred
from that message. Current-size no-ops and eligible-mode filtering address that
flow without forcing upscaling or unsupported sub-540 sizes.

## Earlier zero-frame report (historical, not the later result)

The earlier diagnostic was not a successful shader result:

```text
Requested API: Vulkan
Observed game API: Not detected
Processed game frames: 0
Game surface observed: no; processing active: no
loaders=1; instances=1; devices=1; Win32 surfaces=0;
swapchains=0; presents=0; missing queues=0; excluded windows=0
Last hooked loader: vulkan-1.dll
Qt bridge: 5.15.2; host: 5.15.4
```

This establishes **captured Vulkan initialization**, but no captured Win32 WSI
presentation chain. It does **not** establish any of the following:

- that Free Fire's Android client is using native Vulkan;
- that Free Fire never supports Vulkan;
- that its final Windows presentation is OpenGL, EGL, DXGI or Vulkan;
- that a shader compiled, received a game image or changed a frame;
- that a window-ownership/queue rejection caused this particular zero-frame run.
  Neither rejection counter increased in this report.

The public Qt bridge is intentionally built against the older same-major API
baseline. Seeing 5.15.2 versus 5.15.4 is not by itself a compatibility failure,
and replacing Qt would neither prove game ownership nor provide a missing WSI
swapchain. Actual module/provider preflight is a separate check.

## Research findings — keep three layers separate

| Layer | What its setting/evidence actually means |
| --- | --- |
| Android game API | The API selected by this exact Free Fire or Free Fire MAX client, its build/device rules and runtime environment. Not observable from the supplied Windows device-creation counts. |
| Emulator graphics renderer | BlueStacks' configured graphics-library path. Its current documentation offers **Vulkan & OpenGL** or **OpenGL Only** on newer supported Android instances, not a guarantee that every app creates a Win32 Vulkan swapchain. [1](https://support.bluestacks.com/hc/en-us/articles/360057389932-How-to-change-the-graphics-settings-on-BlueStacks-5) |
| Windows presentation / interface | The host API and surface finally presented to a Windows HWND. BlueStacks documents **Interface renderer** separately from the game graphics renderer. Capturing Qt Settings is not capturing the game. [1](https://support.bluestacks.com/hc/en-us/articles/360057389932-How-to-change-the-graphics-settings-on-BlueStacks-5) |

BlueStacks' version-specific documentation says Nougat 32/64-bit has
OpenGL/DirectX choices; newer Android Pie/11/13 versions have different choices,
including combined Vulkan/OpenGL. The player version and Android instance are
therefore needed; Qt 5.15.4 alone does not identify either.
[2](https://support.bluestacks.com/hc/en-us/articles/360059893151-How-to-use-a-graphics-renderer-on-BlueStacks-5)

The official Free Fire recommendation page specifies CPU, memory and
performance-mode settings, **not a mandatory Vulkan renderer**. It does not
provide the exact client's runtime API. The Free Fire MAX recommendation is a
separate guide, so the two clients should not be silently treated as identical.
[3](https://support.bluestacks.com/hc/en-us/articles/360057784811-Recommended-settings-for-Free-Fire-on-BlueStacks-5)
[4](https://support.bluestacks.com/hc/en-us/articles/4410423736333-Recommended-settings-for-Free-Fire-MAX-on-BlueStacks-5)

Unity's Android documentation supports multiple graphics APIs, including Vulkan
and GLES. Engine capability is **not proof that an individual game shipped,
selected or used a particular backend**. No exact Free Fire engine build/API
configuration was extracted or established in this investigation.
[5](https://docs.unity3d.com/2022.2/Documentation/Manual/android-requirements-and-compatibility.html)

API translation also makes DLL-based inference unsafe. Android documents ANGLE
as an implementation of OpenGL ES on Vulkan: Vulkan activity can exist below a
GLES app. This is an architectural example, **not evidence that this older
BlueStacks session is using Android 15 ANGLE or that changing ANGLE fixes it**.
[6](https://developer.android.com/games/develop/vulkan/overview)

## What the current hook implementation can and cannot say

The in-repository code was inspected, not just the label in Settings:

- In the reported older build, `ShaderBridge::attach()` mapped native `gl` /
  `dx` / `vlcn` into a requested-family gate. Current source records this as a
  **configuration hint** and uses automatic owned-host-presentation routing.
  Neither value measures the running Android app's original API.
- `VulkanHooks.cpp` tracks Vulkan loader/instance/device lifetimes, captured
  `vkCreateWin32SurfaceKHR` HWNDs, swapchains and presentation queues.
- `Runtime::eligible()` requires an identified same-process player **child**
  window, excludes the Qt/Chrome UI, and requires sufficient client bounds.
- Current `Runtime` accepts the first actual owned source independently of the
  configured hint, holds its API/object/window claim to avoid translation-layer
  double processing, and releases it on teardown. EGL uses independent providers
  and exact current display/drawable identity; Qt/UI-first or loaded-device
  evidence is insufficient. A bypass remains a bypass, not a Vulkan success.
- No Android game-process API identification, offscreen Vulkan image ownership,
  shared-image interop chain or external-process presentation adoption is
  implemented by these counters. The shader must not touch such images on a guess.

Plausible explanations still to distinguish are **a different game/presentation
API**, **offscreen/interop presentation**, and **a missed surface/provider/process
lifecycle**. These remain hypotheses for the reported session. The automatic
routing change removes the configuration gate without changing the emulator's
renderer or adopting UI/offscreen images. Authored Windows loader/lifecycle tests
and software pipelines alone are not affected-player evidence; the later report
above separately supplies working GL presentation counters. It does not explain
or retroactively turn the historical zero-frame run into a success. Record the
actual observed API; do not count another API as Vulkan acceptance.

## Historical evidence checklist (not a repeated request for the reported case)

The reported case has since supplied active gameplay and the exact first depth
blocker above. Do not resend this checklist as a prerequisite for trying the
original DLL or the independently owned-resource experiments. The version/API
items remain useful when evaluating a genuinely different installation.

1. Record exact MSI/BlueStacks version, Android instance (Nougat/Pie/11/13),
   Free Fire versus MAX and app version, and both native Graphics and Interface
   renderer settings. Do not infer them from the bridge version.
2. After a normal full player restart, launch the same game/scene and copy
   rendering status again. No game files, startup arguments, global profiles,
   protection settings or DLL proxies need changing.
3. If ADB was **already enabled** and the user elects to inspect it, use the
   installed emulator's ADB to read the game's own graphics-initialization log.
   Select the actual connected instance from `devices`, get the PID of the
   actual package (`com.dts.freefireth` or `com.dts.freefiremax`, only after
   confirming the installed client), and filter a read-only `logcat -d --pid=...`
   for its initialization/API lines. A retained Vulkan/GLES initialization line
   belonging to that app is useful; an unrelated SurfaceFlinger EGL string or
   a supported-device-feature list is **not** proof of the app's chosen API.
   Missing/rolled-over/restricted logs are inconclusive. Share only relevant
   graphics lines after reviewing for personal data, not an entire device log.
4. The missing Windows presentation path needs a supported host trace or
   bounded read-only presentation diagnostics in an authorized test environment.
   Do not attach intrusive tools to a protected online match or disable anti-cheat.
   No RenderDoc/ANGLE forcing, APK modification or game injection was performed.

The later session needs no forced renderer change merely to relabel its
working host GL path. If the user elects to compare another native renderer,
record it as a separate run, not a Vulkan acceptance shortcut. The reported
case has already supplied saved/armed-On state and the latest precise negative
result; another Save or resolution change is not a repair. The original-DLL
and reversible owned-resource experiments above proceed without replacing that
negative game result with a fixture success. Current native Display dimensions
belong in the separate size test. Game texture appearance, real Vulkan
presentation and FPS gains remain unconfirmed.
