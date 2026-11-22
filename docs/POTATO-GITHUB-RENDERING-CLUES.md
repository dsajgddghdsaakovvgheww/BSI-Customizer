# Potato: actionable GitHub rendering clues (2026-11-17)

## Acceptance has not changed

The latest installed report is **1,030 presentation frames / 2,058 inspections,
all NoSampler**, with **zero prepared draws/private bindings, linked producers
or captured transfers**. Strength is 100%. The inspected mutable RGB 256x256
consumer uses legal **GL_LINEAR (`0x2601`)**, not mipmapped filtering. Its
stored/effective BASE/MAX are 0/1000, storage/view -1/-1 and failed level -1.
That does not identify the unseen game-material renderer or prove absent mips.
An armed switch, grading frames, source research or a passing authored test is
not genuine installed-game texture reduction.

## 1. Strongest lead: lookup-returned driver entry points

| Evidence | What it actually establishes |
| --- | --- |
| [BlueStacks 5 Pie64 proxy issue](https://github.com/aurenex/opengl32/issues/1) and [reply](https://github.com/aurenex/opengl32/issues/1#issuecomment-4158495382) | A user reports helper calls working while `glDrawElements` is missed, and reports ReShade seeing draws on the same setup. The reply suggests driver addresses obtained through `wglGetProcAddress`. It is an unresolved third-party report/suggestion, not a verified diagnosis of this game. |
| [ReShade lookup handler, pinned source](https://github.com/crosire/reshade/blob/35347e8191026742f0e07e219137715fad0ed23d/source/opengl/opengl_hooks_wgl.cpp#L1386-L1508) | Its `wglGetProcAddress` handler explicitly routes returned `glDrawArrays`/`glDrawElements` and other core names through its hooks. This is concrete implementation evidence that a DLL-export-only hook is insufficient. No ReShade implementation is copied into BSI. |
| [gfxstream Windows resolver, pinned source](https://github.com/google/gfxstream/blob/0eb17af5d0216fce174aa63abe6098367931a20f/host/gl/glestranslator/egl/egl_os_api_wgl.cpp#L227-L236) | The emulator reference resolves GL functions with WGL lookup first and library-symbol fallback second. This is an emulator mechanism, not attribution to the installed BlueStacks binary. |
| [Microsoft's WGL lookup contract](https://github.com/MicrosoftDocs/sdk-api/blob/docs/sdk-api-src/content/wingdi/nf-wingdi-wglgetprocaddress.md#L65-L73) | Use the procedures returned for the current context. Extension addresses are specific to pixel format; contexts of the same pixel format share them. Do not assume all contexts have different pointers or that the presenter's address inventory is process-wide. Audited file blob: `54a8190f2d7f4b560f4223858faf96256774a556`. |

### Concrete BSI gap, separate from game attribution

At production baseline `0b07fe7`, BSI already resolves draw functions through
WGL on presenter initialization and has per-target trampolines. It is **not**
an export-only proxy. However, its existing WGL lookup interceptor observes
context creation and pbuffer lifecycle/bind entry points, **not newly returned
GL draw/transfer/deletion targets**. A distinct native procedure discovered
later can therefore remain unpatched. Its first draw cannot enter
`GameCandidate::initialize()` to discover that missing address. Initializing
sampler inspection only after entering that very hook cannot repair the miss.

The narrow repair registers the **exact returned address** for an existing
signature/alias in the same bounded target pools used by presenter setup. Each
address retains its own original trampoline. It does not substitute a different
context's original, intercept arbitrary names, invoke GL at lookup, hold the
render mutex at lookup, or turn a procedure lookup into ownership evidence.
The common procedure lock serializes lookup/presenter slot registration.
Native result/address and Win32 LastError survive optional attachment failures.

Draw, transfer, protection, resource-lifetime and selected-presentation checks
remain unchanged. No base-only compositor mips are fabricated, no source LODs
are persistently mutated and no foreign window/context is authorized. Deletion
lookups must be observed too: broader code coverage must not leave retired,
reused texture/RBO names trusted. Off still uses native state and clears proof.

### Before/after test boundary

- `e65a98f68d6a44656bedfccd41b45e8bb5e43bb8` is a **tests-only** regression commit.
  [Before-repair Windows workflow 45005286285](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/actions/runs/45005286285)
  executes the real production MinHook path against an explicitly named
  authored DLL. All **three Windows configurations** (Qt 5.15.2 and Qt 6.2.4
  against hosts 6.7.3/6.8.3) fail exactly the new test's assertion:
  **a returned draw procedure must be intercepted BEFORE its first original**.
  Each retains **26 passing previous tests / one intended new failure**. This
  is actual Windows execution, not an inference from cross-compilation.
- The new test resolves the presenter's target, then a genuinely distinct code
  address from a second authored context. It requires interception **before
  that original's first draw**. A lookup without consumed-resource proof must
  still perform zero sampler inspections/replacements.
- Only after an exact successful native-image link may that independent,
  nonsharing material producer receive private desktop bias+floor samplers
  **inside its own original**. The legal LINEAR compositor stays native.
- Follow-up coverage includes separately late blit/copy/deletion entry points,
  exact original arguments and once-only calls, native address/LastError,
  retirement/name reuse, foreign-window exclusion, Off and WGL failure sentinels.
- The production repair and extended fixtures pass strict Win64 compilation;
  native **9/9** and ASan/UBSan/leak **9/9** pass, including actual software
  Vulkan controls. These Linux tests do not execute the Windows hook. Exact-head
  **after-repair Windows execution/CI results are recorded separately on
  [draft PR121](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/pull/121)**,
  not inferred from compilation or the successful before-repair reproduction.

These are **authored public-API dispatch/state doubles plus real Windows
MinHook**, not BlueStacks execution, actual GL-driver pixels or physical GPU
certification. They do not cover every address cached before hook bootstrap,
prove this game's pixel format/lookup behavior, or identify another process.

## 2. Separate emulator lead: EGLImage aliases and private transfers

[gfxstream color-buffer source](https://github.com/google/gfxstream/blob/0eb17af5d0216fce174aa63abe6098367931a20f/host/gl/color_buffer_gl.cpp#L903-L927)
has a private `eglBlitFromCurrentReadBufferANDROID` fast path and a fallback
that creates a temporary texture and imports an EGLImage before copying. Other
parts create EGLImages from backing textures and recreate them after storage
orphaning. Thus an ordinary copied-to texture name may be a temporary alias,
not the later presentation texture's name.

BSI's native pbuffer bind support does **not** implement general EGLImage
aliasing/private transfers. This is a concrete coverage lead, **not evidence
that this game uses gfxstream's code**. Supporting it would need observed
create/import/destroy/storage generations and exact image/color relations;
equal numeric texture names or guessed share groups are not substitutes.
No such alias route is enabled by the WGL lookup repair.

Direct `glCopyImageSubData`/NV image copies are another unimplemented transfer
family; the existing descriptor supports framebuffer blits and
`CopyTexSubImage2D`. Their existence in Khronos specifications is not evidence
of use by this installation. They need their own valid-target/format/level/
sample/bounds and lifecycle reproduction before production changes.

## 3. Discarded or weak hits

- [RenderDoc's BlueStacks hit](https://github.com/baldurk/renderdoc/blob/5ad608809c9028da9f5c5082397cb6afe9c8dd17/renderdoc/driver/gl/gl_debug.cpp#L1092-L1109)
  is a GPU-vendor classification string. Keyword co-occurrence with GL texture
  functions is **not** a BlueStacks texture-reduction implementation.
- Repositories named PotatoGraphics included unrelated Lua/Web Unity projects
  and ZIP-only payloads. A browser modkit calling Unity quality settings is
  not a usable host-GL path for this native plugin. No unverified binary is
  downloaded/executed/deployed or described as an all-GPU fix.
- [OpenStacks' renderer notes](https://github.com/lasatre/OpenStacks/blob/4e5ec8d850d98c61b4fda3ff0a529eadaaaf5d96/docs/renderer_plan.md)
  describe an emugl-style BlueStacks guest/host stream and custom opcodes.
  These are third-party reverse-engineering claims, not independently audited
  source for the user's exact installed modules; useful orientation only.

**Conclusion:** GitHub yielded a specific lookup-coverage defect to reproduce
and repair, plus distinct image-alias/private-transfer leads. None changes the
still-negative installed-game acceptance. Keep PR121 draft/unmerged/unreleased;
no blind proxy deployment, protection/licensing bypass or universal-GPU/FPS claim.
