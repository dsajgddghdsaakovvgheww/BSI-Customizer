# Potato: WGL dispatch tables cached before bootstrap

## Installed-game result remains negative

After delivery of the `81509f1` WGL lookup candidate, the user reports **3,154
presentation frames / 6,297 sampler inspections, all NoSampler**, with **zero
prepared draws/private bindings, linked producer inspections or color transfers**.
The observed host API is OpenGL; Qt bridge/host are 5.15.2/5.15.4. The inspected
mutable RGB 256x256 input still uses legal **GL_LINEAR (`0x2601`)**. All 6,297
inspections reject non-mip filtering; other mip-first-blockers are zero.
Strength 100% does not change that eligibility rule. This is **no Potato texture
success**, not partial success or a reason to manufacture compositor mipmaps.

The report followed candidate delivery, but no installed-module hash accompanied
it. Its binary identity is not independently certified; neither a stale install
nor successful game capture is inferred from that absence. The older 1,030 /
2,058 report is not the latest observation.

## What the startup inspection actually establishes

- Production `ShaderSettingsSource.cpp` inserts the optional dynamic Bootstrap
  creation at the **start of the root `Component.onCompleted` handler**.
  `ShaderBridge::attach()` configures the runtime and starts presentation hooks.
  `ShaderPlugin::registerTypes()` registers types; it is not an earlier renderer
  hook startup point. These are our own implementation facts.
- The independently fetched [player QML reference](https://github.com/husainprime20/src-code/blob/ee557954ee9e1a251f20fe667d2bbcc1f711d26b/plr.qml#L150-L155)
  has root `ApplicationWindow` id `iAppwin`; its completion handler calls
  `fSetUiParams` and sets `pCompLoaded`. That is **not a native renderer's startup
  trace** or proof that rendering/function caching begins only afterward.
  Audited QML blob: `08aca98778589e8c168ed2a02959c5202ea68239`, 178,834 bytes,
  SHA-256 `d4337078459fa60cd99a0953250e7489007bb984b67f0b367064b31ca32a2974`.
- `81509f1` observes newly returned WGL procedures, but does not detour native
  current-context binding. If a renderer already cached a distinct draw address
  before bootstrap and subsequently only binds its context and calls that
  pointer, no new application lookup reaches the repaired lookup handler.
  Presenter-only resolution cannot discover another context's different table.

**This is a concrete coverage defect, not attribution to the installed game.**
Moving Qt/plugin initialization into `DllMain`, patching more PE imports or
assuming all child completion/native startup order is unnecessary to repair
this narrower case. No such startup relocation is made.

## Independently executed before-repair regression

Tests-only commit **`9b23fd007d209349b82f2f31bc29fb7ef2f95110`** adds the
`shader-wgl-cached-procedures-contract` CTest and extends the explicitly named
`BsiWglFixture` state double. All application GL procedure lookups, including
four different draw targets, happen **before `Runtime::start()`**. The test
then binds a pre-existing context and immediately requires its cached draw
address to be patched before executing that original, with no application
lookup after bootstrap.

[Before-repair workflow 45011346015](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45011346015)
completed with **all three Windows configurations failing exactly that new
assertion**, each with **27 previous tests passing / one new failure**:

> a pre-bootstrap cached draw must be intercepted AFTER make-current and BEFORE
> its first original, without another application lookup

The three Linux jobs pass. The Windows cases use the real production MinHook
path, not a policy-only simulation. However, their GL/WGL functions and contexts
are **authored DLL/state doubles**, not actual driver/BlueStacks/game execution.

## Narrow repair and acceptance boundary

After successful native `wglMakeCurrent`, `wglMakeContextCurrentARB` or
`wglMakeContextCurrentEXT`, discover the current table on **that same thread**.
Verify that the requested non-null context/DRAW DC is actually current, then
resolve the existing draw/blit/copy/deletion signature inventory into the same
bounded exact-target pools used by lookup/presenter setup. Discover both
separate-current aliases as well, so previously cached alias pointers can be
patched after a standard binding. Late returned aliases are also observed.

- Never substitute another context's original or modify cached application
  pointers. Code at each observed exact address has its own native trampoline.
- Forward each binding's original once with the exact READ/DRAW/context
  arguments; preserve its BOOL return and native Win32 LastError.
- Failed native binds and successful null unbinds perform **no discovery**.
- Discovery does not call GL state functions, hold the render mutex, select a
  game presentation, infer context sharing or publish resource dependencies.
  Draw-time ownership/protection/provenance checks remain mandatory.
- A bounded **64-entry thread-local code cache** avoids resolving every procedure
  on every repeated binding. Observed WGL creation and revision changes around
  native deletion invalidate it, including numeric context-handle reuse. It
  stores no GL resources/ownership. Overflow rescans rather than grants proof.
- Native deletion observation is required before caching tables. No loader-lock
  callback, process injection, driver replacement or game-memory scan is added.

The new Windows test continues beyond the failing-before assertion to cover
cached core/ARB/EXT paths, different context targets, native arguments/errors
and once-only originals, repeat-bind caching and handle reuse, failed/null
bindings, exact native-image-linked nonsharing material producers, consumer
retirement, foreign-window exclusion and Off. The legal LINEAR presentation
consumer remains unmodified. Strict Win64 compilation and local native and
ASan/UBSan/leak suites pass with an explicitly selected software Vulkan ICD;
Linux suites **do not execute this Windows repair**. Initial local runs without
that ICD failed the mandatory loader check (8/9), not a production regression;
with the ICD configured both suites pass 9/9.

**After-repair Windows execution is tracked on [draft PR121](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/pull/121).
Do not infer runtime success from compilation or the intended before failure.**
No new packaged candidate has been delivered for this repair yet; the previously
delivered `81509f1` ZIPs do not contain it.

## Still unsupported or unproved

A context that never binds or queries again after bootstrap can still be missed.
A previously cached separate-current alias cannot be discovered from a native
thread on which no known binding/lookup/presentation path ever executes.
General EGL procedure caching is not repaired here. General EGLImage backing
aliases, private copies, external-memory paths, `CopyImageSubData`, another API
or another process remain distinct leads, not enabled routes or installed-game
diagnoses. See the [GitHub rendering clues](POTATO-GITHUB-RENDERING-CLUES.md).

In particular, gfxstream's texture-image path can reuse translator global backing
instead of issuing a visible host copy. Implementing such a route requires exact
observed image/storage generation, protection, provider/display and ownership
relations; equal GL names, pbuffer existence or source-code resemblance do not
suffice. No image alias or ownership shortcut is added to raise counters.

**There is still no verified installed-game material-pixel change or FPS gain.**
Keep PR121 draft/open/unmerged and do not publish a release on this evidence.
