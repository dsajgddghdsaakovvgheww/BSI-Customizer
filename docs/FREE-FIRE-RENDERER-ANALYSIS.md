# Free Fire: host presentation is not scene-texture capture

## Latest installed-player follow-up: 3,154 frames / 6,297 NoSampler, still zero replacements

The newest report, supplied after delivery of the `81509f1` WGL lookup candidate,
has **3,154 presentation frames / 6,297 sampler inspections, all NoSampler**,
with **zero prepared draws/private bindings, zero linked producers and zero
captured transfers**. Strength is 100%, policy remains `OwnedMipCompatibility`;
observed host API is OpenGL, Qt bridge/host are 5.15.2/5.15.4 and Vulkan has no
WSI surface/swapchain/present. The inspected mutable RGB 256x256 consumer uses
legal **GL_LINEAR (`0x2601`)**, with all 6,297 inspections rejecting non-mip
filtering and other first blockers zero. This supersedes the 1,030/2,058 report.
No installed hash was supplied, so exact candidate identity is not independently
certified; neither a stale install nor game success is inferred. This is not
partial Potato success, absent material mips proved by inspection, or a reason
to force a compositor mip filter.

The [GitHub rendering-clue follow-up](POTATO-GITHUB-RENDERING-CLUES.md) records the
previous newly returned WGL-target repair and its separate limits. Its delivered
candidate has not established installed-game acceptance. The
[cached-dispatch investigation](POTATO-CACHED-WGL-DISPATCH.md) confirms our hooks
start in root QML completion and independently reproduces another gap:
pre-bootstrap cached draw addresses can bypass the lookup handler. All three
Windows before-repair configurations pass 27 earlier tests and fail the intended
new assertion. The narrow cached-WGL repair `bba981e` subsequently passes all six
jobs, including all three Windows native contract steps, in run 45012097459.
The [next EGL/named-copy batch](POTATO-EGL-DISPATCH-AND-NAMED-COPIES.md) separately
reproduces cached/late EGL targets, an already-current WGL presenter and missing
named image-copy observations. It retains the legal LINEAR consumer and adds
native-module build identity and raw-versus-qualified hook coverage diagnostics.
No hooks are moved into `DllMain`, no ownership is inferred from context binding,
and no general EGLImage/private-transfer route is guessed.
**Neither source similarity nor an authored regression diagnoses this game's
unobserved material path.** Installed-game acceptance remains negative.

## Previous installed-player follow-ups: legal LINEAR consumer, still zero producers

The reports have **1,577 grading frames / 3,145 NoSampler inspections** and
**2,993 grading frames / 856 NoSampler inspections**. Both have **zero prepared
draws/private bindings, zero linked producer inspections/bindings and zero
captured color transfers**, with saved strength 100% and depth-independent
compatibility. Observed API is OpenGL, Qt bridge/host 5.15.2/5.15.4; Vulkan has
one loader/instance/device but no WSI surface/swapchain/present.

The inspected mutable RGB 256x256 sampler now definitively reports **MIN_FILTER
`GL_LINEAR` (`0x2601`), `non-mip-filter=1`**, with all other first blockers zero.
Stored/effective BASE/MAX are 0/1000, storage/view -1/-1, failed level -1. This
is legal base-image-only sampling, **not missing required mip 1**. It does not
identify the unseen material renderer or establish that its textures lack mips.
Presentation grading and saved strength are not texture reduction.

The [resource-only route repair](POTATO-RESOURCE-ONLY-ROUTES.md) independently
reproduces and fixes a different production capture gap: a named FBO/RBO output
needs exact recent namespace-qualified resource proof, **not a native default
pbuffer identity**. Current no-HWND WGL DCs and truly surfaceless unprotected EGL
contexts can now reach that proof; foreign windows, unmapped non-null EGL
surfaces and unknown default images remain excluded. A before-repair actual
surfaceless pixel test fails at the intended 50% material assertion; the repair
passes on Mesa desktop/GLES and ANGLE with zero compositor replacements and
full-frame exact Off. **This does not diagnose the game's route or change its
still-negative acceptance.**

## Previous installed-player follow-up: ambiguous mutable RGB level 1 refusal

The saved-strength candidate reports **2,107 grading frames, 1,456 sampler
inspections/all NoSampler and 1,456 presentation-input draws/bindings**, with
**zero prepared bindings/draws, zero linked producer inspections and zero
captured color transfers**. Saved strength is 100%, review passed; depth/blend/
other/nested bypasses are zero. Observed/configured host API is OpenGL, Qt is
5.15.2/5.15.4 and Vulkan still has no WSI surfaces/swapchains/presents.

The inspected image is now specifically **mutable RGB 0x1907, 256x256,
base/max=0/1000, failed-level=1**. The old inspector says that required image is
undefined/mismatched, but queried completeness **before the effective sampler
filter**. Thus it cannot distinguish a legal non-mipmapped base-only image from
a genuinely incomplete mip-filtered resource. It is not the immutable-storage
bug repaired previously. Neither that image nor an increased strength justifies
inventing mipmaps or reducing the finished presentation image.

**Texture Potato is still not working, not working a little.** The
[native-image binding follow-up](POTATO-NATIVE-IMAGE-BINDINGS.md) fixes this
misleading classifier detail and independently reproduces/implements another
missing upstream route: successful native pbuffer render-to-texture bindings
without GL copies/blits. Exact native lifetime/color-output links can reach a
nonsharing renderer without merging its GL texture/RBO names. The actual game
is not diagnosed as using this path; no unsupported/foreign/protected route is
adopted just to raise counters, and no repeated Save/gameplay prerequisite is
being substituted for a production repair.

## Previous installed-player follow-up: 3,622 inputs, two producers, zero replacements

After the `ab8b27e` native-pbuffer/color-transfer candidate the user again reports
that the original DLL works while our Potato does not:

```text
Requested API: Automatic; configured / observed host API: OpenGL / OpenGL
Processed presentation-shader frames: 1812
Texture calls / sampler-inspection calls / NoSampler: 3624 / 3624 / 3624
Prepared draws / overridden bindings: 0 / 0
Depth / blended / other / nested: all 0
Latest sampler2D slots / bound textures: 1 / 1
Feedback / sampler-policy / private-sampler failures: 0 / 0 / 0
Ineligible mip/color/protection: 1
Presentation-input observations: 3622 draws / 3622 color bindings
Resource-linked FBO/pbuffer producers: 2 inspected / 0 prepared / 0 bindings
Qualified color transfers: 0 calls / 0 inputs
Vulkan: 1 loader, 1 instance, 1 device; 0 surfaces/swapchains/presents
Qt bridge / host: 5.15.2 / 5.15.4
```

**Still no genuine game-texture change.** Two producer candidates now reach
sampler inspection, unlike the preceding report, but neither qualifies. Most
counted draws are on the selected presentation route. The final combined
mip/color/protection rejection does not reveal the two producers' own first
blockers, prove absent mips, or establish complete material-draw coverage. Zero
qualified transfer observations is not proof that no copy/blit API was called.
This is not an unsaved-setting, depth, inactivity, Qt-patch or GPU-vendor diagnosis.
No installed hash, exact texture/Off or FPS acceptance was supplied; a stale
installation is not inferred either.

## Current repair: correct valid immutable storage/view qualification

The original DLL's measured bind/draw mutations persist and do not use our
complete-mip/resource classifier. Its broader reach can affect later sampling
that our draw-scoped path never captures. This is a real mechanism difference,
not permission to delete protection/ownership boundaries.

An additional **incorrect eligibility check** has now been independently
reproduced on an actual driver: a valid 64x64 immutable texture with four mip
levels keeps stored MAX_LEVEL=1000. Native OpenGL clamps sampling to those four
images, but our code demanded the nonexistent tail to 1x1 and refused it. The
new real-driver regression fails the intended material-mip assertion before
the change and passes afterward. The [mip qualification repair](POTATO-MIP-QUALIFICATION.md)
uses actual immutable storage/view bounds, preserves complete mutable-chain
checks, queries only supported enums, clamps private LODs to exposed levels and
restores exact Off/source state. It also retains the real producer's reason
separately from later presentation refusals and splits the combined mip bucket.
**The report does not prove immutable storage is this game's cause.** Capturing
the remaining upstream path and actual game texture acceptance remain unproved.

## Previous installed-player follow-up: 1,657 inputs, zero linked producers

This preceding **still-not-working** result was supplied after delivery of the
`e68e030` resource-producer candidate. It superseded the 1,326/2,652 report and
is now superseded by the 1,812/3,624 result above:

```text
Requested API: Automatic; configured / observed host API: OpenGL / OpenGL
Processed presentation-shader frames: 1794
Texture policy: Owned mip compatibility (depth independent)
Texture calls / sampler-inspection calls / NoSampler: 1657 / 1657 / 1657
Prepared draws / overridden bindings: 0 / 0
Depth / blended / other / nested: all 0
Latest sampler2D slots / bound textures: 1 / 1
Feedback / sampler-policy / private-sampler failures: 0 / 0 / 0
Ineligible mip/color/protection: 1
Presentation-input observations: 1657 draws / 1657 color bindings
Resource-linked FBO producers: 0 inspected / 0 prepared / 0 private bindings
Vulkan: 1 loader, 1 instance, 1 device; 0 surfaces/swapchains/presents
Qt bridge / host: 5.15.2 / 5.15.4
```

**Presentation grading and input tracking work; Potato textures still do not.**
The bound presentation input fails before sampler descriptor planning. Its
combined mip/color/protection bucket does **not** establish missing mipmaps.
The resource-aware path has not matched an upstream producer. Do not call this
a depth failure, unsaved setting, inactive gameplay, Qt mismatch or successful
texture reduction. No installed module hash, exact in-game Off, texture-change
or FPS acceptance was supplied; neither a stale installation nor a particular
unobserved game renderer is inferred from that absence.

## Previous repair: native pbuffer outputs and color-transfer dependencies

The previous implementation could only trace sampler2D draw inputs to
texture-backed `DRAW_BUFFER0` FBO outputs. It excluded **all WGL pbuffers**, did
not represent native default-pbuffer/RBO outputs, and intercepted **no framebuffer
copy/blit transfers**. A scene that renders to a pbuffer and copies/resolves its
color into a one-level presentation texture therefore remained unreachable,
even with working input tracking and an observed shared namespace.

The [color-transfer repair](POTATO-COLOR-TRANSFERS.md) independently reproduces
and fixes these supported gaps. It adds exact successful native WGL pbuffer DC
acquisition/release/destruction tracking, typed texture/renderbuffer/pbuffer
identities, and read-only core/alias color blit plus 2D subimage-copy observations.
Only an exact recent selected-presentation dependency can qualify an upstream
draw; recognizing a pbuffer or share group is never ownership. The actual current
**READ** drawable is qualified separately from DRAW. Unknown/foreign/Qt/protected
routes remain native. Object-deletion overflow clears resource evidence without
permanently discarding already observed context sharing; missed context events
still fail closed by forgetting namespaces.

Android's pinned open-source host renderer demonstrates both WGL pbuffers and
copy/resolve paths. That establishes a viable reproduction, **not that this
BlueStacks/Free Fire build uses that implementation or that this is its root
cause**. No source from it or original Potato DLL binary/code is incorporated.

Actual production GLES/SwiftShader pixels now cover **material -> renderbuffer
blit OR default-pbuffer copy -> unchanged one-level compositor**. Each path has
zero compositor replacements, one material replacement, a changed full output
and byte-exact full-frame Off at unchanged resolution. Independent controls
disabling only each new typed producer fail the expected material-mip assertion.
Windows native-hook regressions additionally cover leases, hidden-window DCs,
separate READ pbuffer, exact original arguments/results, lifecycle/revision
changes during originals, and deletion-overflow relearning. Executed/pending
validation is separated in the repair document and draft PR121.

This is a behavioral capture change, not another diagnostics-only stop, a
final-frame blur or a resolution/FPS claim. **Installed Free Fire remains
unresolved until actual texture behavior is established.** Unobserved sharing,
EGLImage/private interop, unsupported shader kinds and other APIs/processes
remain outside this resource proof. No repeated Save/gameplay/report request
or blind original-DLL deployment is proposed.

## Previous report: depth-independent inspection, 2,652 NoSampler

The preceding run had **1,326 grading frames / 2,652 sampler inspections,
all NoSampler / zero prepared bindings**, no depth/blend/other/nested activity,
Automatic requested and configured/actual OpenGL, Qt bridge/host 5.15.2/5.15.4.
It established neither absent mipmaps nor a safe upstream boundary. The first
[resource-producer repair](POTATO-RESOURCE-PRODUCERS.md) extended
[owned-mip compatibility](POTATO-DEPTH-COMPATIBILITY.md) to an exact recently
consumed texture-backed FBO output in the same or observed-successfully-shared
namespace. It passed independent software pixels and Windows hook/Qt CI, but
the newer report above still has **zero linked producers**. Candidate delivery
and authored tests did not establish installed-game success.

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
