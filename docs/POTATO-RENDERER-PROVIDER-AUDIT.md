# Potato: the supplied 5.12 graphics providers, followed into native code

## Result

The user's `both dll.zip` is accessible and has been downloaded, byte-verified
and inspected **without loading or running either DLL**. It contains exactly
`HD-Opengl-Native.dll` and `libOpenglRender.dll`, both Windows x64 with fixed
**FileVersion 5.12.120.6303**.

This advances the [four-EXE investigation](POTATO-HD-PLAYER-NATIVE-RENDERER-AUDIT.md)
past the previously missing `grApiInit` boundary. There are now concrete provider
instructions for interface construction, host GL resolution, private image
creation/destruction and a private-image-to-host-texture binding. This is no
longer an inference from a public gfxstream source tree or a DLL-name string.

**It does not yet diagnose the installed game's missing material capture.**
No production hook/sampler/ownership gate was changed, no new plugin/package was
built, and no game/FPS/Off acceptance is claimed. The last installed result is
still **3,154 presentation frames / 6,297 NoSampler / zero replacements**.
Private image implementations are a concrete investigation target, not proof
that all downstream host calls evade our existing hooks.

**PF follow-up:** the [subsequent program-files audit](POTATO-PLAYER-PF-AUDIT.md)
now includes a co-packaged 5.12.120.6303 `HD-Player.exe`, these exact same two
private providers and the public ANGLE EGL/GLES sidecars. The PF download and
checksums succeeded; no installer resend is needed. It also distinguishes public
export stubs from resolver-returned implementation targets, without claiming
that this is the launched game's branch or a newly verified Potato repair.

## Input identity and provenance

Source: [user-supplied archive, pinned to repository commit
`c4af1bd2584bbb8884ab3d34657ba35a32747c81`](https://github.com/dsajgddghdsaakovvgheww/BSI-Customizer/blob/c4af1bd2584bbb8884ab3d34657ba35a32747c81/both%20dll.zip).
GitHub's blob API supplied the bytes; the pinned contents API independently
confirms blob identity and length. ZIP member CRCs were checked before analysis.

- Archive: **5,674,937 bytes**.
- Git blob SHA-1: `fadd26c4893a171b8eab8b0f24f1f3c4b2aa9c41`.
- Archive SHA-256: `9b4bd78b3f5a6e4ad545e8946f5c0e423e20635b822e7f187582fcbd6fbac25d`.

| Member | Bytes | Complete-file SHA-256 |
| --- | ---: | --- |
| `HD-Opengl-Native.dll` | 6,157,688 | `8af770ea79f47551b2f8ab88acdf568745046ff08b47b88860a06965de34fe5d` |
| `libOpenglRender.dll` | 11,115,896 | `d4b50c270d1f7ed1afed4fbc4e963177f747b0ee1c155a2e7112c59924212afc` |

The fixed file versions and displayed FileVersion/ProductVersion strings say
5.12.120.6303; the **fixed ProductVersion is 0.0.0.0 in both**. Those fields are
preserved separately, not silently reconciled. Both have certificate directories;
Authenticode trust/signatures were **not** validated. The proposed installer's
version is consistent with the files, but an independently verified extraction
from that installer was not performed. Identity with the **actually launched
installation** is also unverified. These DLLs are not version-matched companions
of any of the four differently versioned reference EXEs merely because they
implement the same export name.

[Machine-readable evidence](evidence/potato-renderer-provider-static-audit.json)
contains the full export/import inventory, version fields, instruction bytes,
RIP/direct targets, manually selected witness boundaries and the 13-entry private
EGL extension-name table. Original binaries and scratch disassembly remain in
ignored research storage, not this change set.

All addresses below are **hexadecimal RVAs**, not loaded addresses or offsets
to install into production. Preferred image base is `0x180000000` in both.

## 1. `grApiInit` really constructs different private interfaces

| Evidence | `HD-Opengl-Native.dll` | `libOpenglRender.dll` |
| --- | --- | --- |
| Total exports | 2 | 916 |
| Code export `grApiInit` | `a65f0` | `1312d0` |
| Adjacent code-pointer stores in exported initializer | 25 | 18 |
| Additional directly called table builder | None credited | `325e20`: 8 more stores |
| Observed first-slot implementation | `a6330`, stored at `a670a` | `130e50`, stored at `1313c5` |
| Public `egl*` exports | None | None |

These are **observed stores**, not declarations of a complete compatible ABI.
For example, offset `0x60` has no credited assignment in the native provider's
initializer but does in the other provider. Do not treat the table as a vtable
with interchangeable, guessed signatures or copy these layouts into a hook.

The native initializer saves its incoming first argument in R14 and populates
that object. It copies 80 bytes from the second input and 88 from the third when
non-null. The other provider copies the same extents in internal helpers
`113660`, `325e20` and `2de780`. These fit the previously observed callback-table
construction boundary; they do **not** establish every callback's argument or
lifetime contract across versions.

The first-slot native implementation retains incoming RCX at `a6354`, puts it
in a worker parameter object at `a64e4`, then passes a native worker entry to
`_beginthreadex` at `a650a`. The other implementation retains RCX at `130e71`,
passes it onward in RDX at `131096` before the subwindow helper call `1310a0`,
and saves it at `131109`. The separate native-window handoff found in the EXEs
therefore has a concrete provider-side continuation to investigate; the child
window alone is still not evidence of a missing presentation hook.

## 2. Native host GL calls are real, but private dispatch is not automatically a gap

### `HD-Opengl-Native.dll`

The backend predicate at `88c50` reads a flag word and masks it with `6`.
Loader `92600` chooses between the strings `opengl32` and `libGLESv2` using that
predicate. Its symbol helper reaches **`KERNEL32!GetProcAddress` at `bf005`**.
The actual runtime flag value and loaded module are not proved by this code.

The GL setup has decoded name/destination/call triples, including:

| Procedure | Name operand | Destination slot | Loader call |
| --- | --- | --- | --- |
| `glDrawArrays` | `92da8` | `639a20` | `92db6` |
| `glDrawElements` | `92dbb` | `639a28` | `92dc9` |
| `glTexImage2D` | `9322f` | `639b50` | `9323d` |

The real indexed-draw path calls through `639a28` at **`999fa`**. WGL bootstrap
resolves `wglGetProcAddress` at `b54dc..b54ea`; native current binding calls its
cached WGL target at `b5bd4`. Native presentation reaches the actual
**`GDI32!SwapBuffers` import at `b5db8`**. The bootstrap also creates a `PgaDummy`
window, but that window's name/existence grants no game-resource ownership.

The current product already observes public WGL creation/sharing/current/
presentation and resolves core GL through the `opengl32` export fallback when
`wglGetProcAddress` returns an invalid sentinel. It is therefore **not** justified
to label these core-export/cached-dispatch/presentation mechanisms a newly
reproduced missing hook just because we can now see the provider's implementation.

### `libOpenglRender.dll`

This provider has a host WGL path that loads `opengl32.dll` or the explicit
`ANDROID_GL_LIB=mesa` alternative (`2f091d..2f0970`). `WglBaseDispatch` resolves
`wglGetProcAddress` through helper `1d32d0`, whose tail jump at `1d32dd` reaches
`KERNEL32!GetProcAddress`, and saves the result at `2ee9ef`. A separate host-EGL
setup resolves/caches the native `eglGetProcAddress` at `2f0dca..2f0de8`.
Neither path's presence identifies the active game branch.

**Many `GLDispatch` exports are data, not callable GL functions.** For example,
`?glDrawElements@GLDispatch@@2P6AXIHIPEBX@ZEA` exports the writable, initially
zero-filled pointer slot **`ac9f30`**. Code obtains a target by name at
`1f31f4..1f31fb` and writes the pointer at `1f32ac`. Reading file bytes as a live
pointer there, or installing a code detour on the exported variable's address,
would be wrong. The auditor distinguishes executable raw bytes from data and
virtual zero-fill explicitly.

## 3. A concrete internal image path, not just an `EGLImage` string

### Internal EGL in `libOpenglRender.dll`

At `197f17..197f1e`, code places internal resolver **`2dc920`** in dispatch slot
`a48040`. Its extension lookup loop references the actual name/code-pair table
**`50cbe0..50ccb0`**, with 16-byte stride. The 13 entries include:

| Name | Pair RVA | Internal implementation |
| --- | --- | --- |
| `eglCreateImageKHR` | `50cbe0` | `2d9350` |
| `eglDestroyImageKHR` | `50cbf0` | `2d9780` |
| `eglBlitFromCurrentReadBufferANDROID` | `50cc50` | `2d9a90` |
| `eglSetImageFenceANDROID` | `50cc60` | `2d9ab0` |
| `eglWaitImageFenceANDROID` | `50cc70` | `2d9b90` |

The initializer asks that resolver for image creation at `19822d..198239`,
retaining the result in **`a48048`**, with a direct internal fallback at
`19825a..198261`. Destruction similarly populates **`a48050`**. This demonstrates
an EGL implementation inside a DLL with **no public `egl*` export contract**;
scanning loaded modules for public `eglSwapBuffers` cannot discover that internal
interface as a separate EGL provider.

An internal image client at `fa100` reads `a48048`, supplies target `0x30b1`,
calls the saved function at `fa539` / `fa56e` and retains two returned image
handles at object offsets `0x18` / `0x20`. Retirement code calls `a48050` at
`f98ec` / `f98ff`. The implementation checks the texture-image target, retains
backing objects and has its own validation/destruction code. A complete alias,
protection, context/display, fence and numeric-handle-reuse contract has **not**
been reconstructed or observed live by this audit.

### Private image binding in `HD-Opengl-Native.dll`

The log-labelled **`pga_glEGLImageTargetTexture2DOES`** implementation is the
code at `9a730`, not a public export in this DLL. It handles different internal
image types. In its `0x30b1` branch (`9a98d`):

1. Read a 32-bit native texture value from the image object at `9a99d`.
2. Associate it with the mapped texture object at `9a9a0`.
3. Tail-jump at **`9a9b2`** through the resolved host **`glBindTexture` slot
   `639978`**, with target `GL_TEXTURE_2D`.

That branch performs a **name/backing alias**, not an explicit framebuffer copy.
Treating the opaque incoming image object as a GLuint, or manufacturing a
copy-image dependency from equal dimensions/names, would be incorrect. Conversely,
it eventually binds a real host texture, so the existence of this private mapper
alone does not prove that our host draw/resource observations miss its output.

The `PgaCtlPostFramebuffer`-labelled chunk also explicitly sets a texture's
minification/magnification to legal **LINEAR** (`88075..8809a`). That is evidence
for a provider presentation/composition input, **not identification of the
installed report's particular RGB 256x256 texture**. No policy is changed to make
one-level composition qualify as scene mip sampling.

## What this means for a repair

The evidence now separates two questions which must not be conflated:

- **Code coverage:** which actual host/native draw, resolver, current binding and
  transfer targets run? Several demonstrated paths already have product hooks.
- **Resource proof:** how does this provider's internal image/backing/lifetime
  connect a material producer to the selected game's consumer, without merging
  unrelated GL namespaces or trusting a hidden/foreign window?

The next useful work is to close that exact native image/context lifetime chain
and verify which branch and provider identity the launched player actually uses.
Then reproduce a **specific** missing entry or typed alias in a negative contract
and legal driver-pixel test before changing production. A private table offset,
source-code similarity or the presence of internal EGL is not enough.

Any eventual implementation must retain provider/display/context/share-generation,
resource type/generation, protection, selected-route revision and bounded-TTL
checks; revocation on destruction/reuse/source change; unchanged native arguments/
results; and exact Off behavior. Do not turn all process GL draws or every private
image into owned game content. No extra DLL/account/config upload is requested
by this audit, and the four reference EXEs do not need resending.

**There is no newly verified Potato repair/build in this change.** The previous
candidate `b807653` is not being repackaged or relabelled as a provider fix. A new
Customizer EXE alone would not replace an already installed native shader DLL.

## Reproduction and executed validation

New read-only tool: [`Inspect-RendererProviders.py`](../scripts/Inspect-RendererProviders.py).
It accepts explicit DLLs or the two-DLL ZIP; it does not auto-discover an install,
load the providers or follow adjacent imports. Focused witnesses are enabled
only for the two complete-file hashes above. Unknown hashes get a generic
inventory/export/anchor report, **not replayed private offsets**.

```bash
python3 -m venv .venv
.venv/bin/python -m pip install pefile==2024.8.26 capstone==5.0.6
.venv/bin/python scripts/Inspect-RendererProviders.py 'path/to/both dll.zip'

# Requires GNU objdump for the independent selected-instruction comparisons.
BSI_RENDERER_PROVIDER_ZIP='path/to/both dll.zip' \
  .venv/bin/python tests/renderer_provider_audit_test.py -v
```

Executed in this audit:

- **17/17 provider tests, zero skips**, including this exact original ZIP and
  independent GNU objdump comparisons for **243 distinct selected instructions**
  (**115 native / 128 libOpenglRender**): bytes, normalized mnemonic, RIP targets
  and direct branch targets where present. This is not 243 functions or a complete
  CFG. Synthetic cases cover instruction-boundary false matches, data/zero-fill
  exports, intervening writes, leaf tail jumps, forwarded/non-x64/non-DLL inputs,
  duplicate/unrelated/linked ZIP members, linked paths and nonzero CLI failures.
- Direct named-DLL reports equal the ZIP-member reports. Complete archive/DLL
  hashes remain unchanged. The new auditor/test compile with `py_compile`.
- Existing renderer suite: **13 passed / one original-EXE golden skipped**;
  Qt inventory: **10 passed / one original-reference golden skipped**. This
  environment's previous EXE cache is absent; their historical all-four passes
  are not relabelled as new original-EXE validation.
- Existing renderer/Qt auditor sources and production presentation/sampler sources
  are byte-identical to their before-analysis hashes. No proprietary PE was loaded,
  executed, patched or added by this change. No Windows/game/driver-pixel run was
  performed, and no build, commit, push, tag, release or merge was performed here.
