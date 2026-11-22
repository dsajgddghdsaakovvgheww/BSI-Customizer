# Potato: the supplied PF archive is now downloaded and inspected

## Result

**The transfer succeeded.** The user's `PF.zip` was retrieved through the
connected Drive download tool, not a browser listing or a failed direct link.
Its complete bytes match Drive's SHA-256 and MD5. All **105 ZIP entries** were
read with CRC validation; the archive expands to **110,824,467 bytes**.
No supplied executable, DLL, service, driver or bundled extraction program was
loaded or run. **No installer, Program Data, Android virtual disk or account
upload is needed for this inspection.**

The important advance over the [two-provider audit](POTATO-RENDERER-PROVIDER-AUDIT.md)
is a **co-packaged 5.12.120.6303 player EXE and public ANGLE EGL/GLES sidecars**.
The two private renderer DLLs are byte-for-byte identical to the pair already
analyzed. We can now distinguish private provider dispatch, public EGL export
stubs, actual EGL implementations and lookup-returned GL targets using this
archive's own code—not an assumed public source version.

**This static audit was not a working Potato fix.** Archive identity/co-location
does not prove which path the launched game executes. At this audit stage,
production was not changed and no new shader package was built. The subsequent
repair/package is documented below; the installed failure still remains
**3,154 frames / 6,297 NoSampler / zero replacements**. A transfer success or a
static test pass must not be presented as material-texture, Off, driver or FPS
acceptance.

**Subsequent fix/testing:** the [split-EGL lifecycle regression and repair](POTATO-EGL-LIFECYCLE-ALIASES.md)
now follows this audit's concrete stub/implementation topology. It first records
before-repair Windows failures, rather than treating a distinct address as a
missing hook. This audit's static-input identities and limitations are unchanged;
the later authored/software tests are not installed-game acceptance.

## 1. Exact inputs

[User's Drive file](https://drive.google.com/file/d/10OyJS1LgefEal8AvwW0w3UjvAMg0eX4z/view),
ID `10OyJS1LgefEal8AvwW0w3UjvAMg0eX4z`, returned as `PF.zip`:

- Archive bytes: **33,609,789** (about 32.1 MiB, below the connector limit).
- Archive SHA-256: `8922e6b30903d8f1552abb3a7a83477532bd699277fcbbdf222a24e73407b990`.
- Archive MD5: `446e908249e53c3bb90374c4450d82b3`.

| Member | Bytes | Complete-file SHA-256 |
| --- | ---: | --- |
| `HD-Player.exe` | 12,098,936 | `9138bf12725d389f2a2bd84495e2634428fafb292f23a5fa46e200cda986960d` |
| `HD-Opengl-Native.dll` | 6,157,688 | `8af770ea79f47551b2f8ab88acdf568745046ff08b47b88860a06965de34fe5d` |
| `libOpenglRender.dll` | 11,115,896 | `d4b50c270d1f7ed1afed4fbc4e963177f747b0ee1c155a2e7112c59924212afc` |
| `libEGL.dll` | 30,584 | `5e287e7e884504b524dc4610bebe79e013f0bc6f87fe788dd1f5562b70a6dd65` |
| `libGLESv2.dll` | 3,395,448 | `5b0acf994cd091c5c07d707219a33de7d5d9ce2038bf93644a7c3d8d64de48d5` |

All five are Windows x64. The player and private providers have fixed
FileVersion **5.12.120.6303** and fixed ProductVersion **0.0.0.0**. Both public
sidecars have fixed versions **2.1.0.0**; their version strings separately say
**`2.1.0.c1172b2147ff`**, with FileDescription identifying ANGLE. These fields are
not a signature-authenticity check or independent source-commit attribution.

The complete archive/member inventory, selected instruction bytes and exact
import/export joins are saved in
[machine-readable evidence](evidence/potato-player-program-files.json).
Original binaries remain in ignored research storage, not this change set.
No public temporary download token or account data is stored in that evidence.

The new EXE is a **fifth sample**, not a relabelled copy of one of the four
[earlier EXE references](POTATO-HD-PLAYER-NATIVE-RENDERER-AUDIT.md). Neither the
archive nor a version field authenticates its extraction from the proposed
installer or proves the hash of a currently running installation.

## 2. This exact 5.12 EXE reaches the same private boundary

The existing bounded native auditor successfully inspected this EXE. Its
renderer-initialization chunk begins at **RVA `153d90`**:

- Basename operands select `HD-Opengl-Native` or `libOpenglRender`.
- Call **`154028`** reaches the loader helper, whose **`319772`** instruction
  calls `KERNEL32!LoadLibraryW`.
- The symbol helper call **`154065`** reaches
  **`KERNEL32!GetProcAddress` at `3199d5`** for `grApiInit`.
- The returned pointer is saved in RBX at **`15406a`** and called at
  **`15412f`**.

Those are static instruction witnesses, not runtime-selected module paths.
The private-provider reports generated from these PF members exactly equal the
previous named-DLL reports. The already-established private-image paths and
ownership/lifetime limitations therefore still apply; they do not need to be
rediscovered from provider-name strings.

The EXE's connected resource mapping also yields **631 file entries in three
unique pools** and the registered `/plr.qml` and `/UiGraphicsSetting.qml` source
hashes in the evidence. It has **119 QML cache headers compiled with Qt 5.15.4**.
That is **not** its exact installed Qt runtime version: the archive does not
contain the required Qt runtime DLLs. No additional Qt upload is requested for
this renderer inspection, and a QML compiler version does not authorize a
shader installation.

## 3. `libEGL.dll` is a real public shim into `libGLESv2.dll`

The 30-KiB EGL DLL has **65 named exports**. Every one begins with a decoded
RIP-indirect jump through an import of a decorated `egl` function in
`libGLESv2.dll`. Each exact imported name resolves to an executable, file-backed
export in the supplied GLES sidecar. These are **code stubs**, not PE export
forwarder strings or initialized live pointers read from the file's IAT.

Representative **hex RVAs**, with each address interpreted in its own DLL:

| Public EGL name | `libEGL.dll` export stub | Matching `libGLESv2.dll` implementation |
| --- | --- | --- |
| `eglCreateWindowSurface` | `1110` | `8a50` |
| `eglGetProcAddress` | `1210` | `9e20` |
| `eglMakeCurrent` | `1240` | `a370` |
| `eglSwapBuffers` | `13a0` | `be50` |

For example, the import at the resolver stub is named
`?GetProcAddress@egl@@YAP6AXXZPEBD@Z`, and that exact symbol is exported at
`9e20` in the GLES DLL. The auditor joins the declared import name to that
export; it never resolves or calls a Windows DLL.

This establishes a **static same-archive dependency**. It does not establish
which real path Windows loaded, whether the game uses ANGLE at all, or whether
these sidecars are instead serving another part of the player/UI. Do not merge
their provider/resource namespaces with the private EGL implementation inside
`libOpenglRender.dll`.

## 4. A procedure lookup can return a different address from a public export

The supplied GLES library has **681 exports**, including decorated EGL
implementations and ordinary GL entry stubs. Its EGL resolver at `9e20` lazily
calls a map constructor at `19b0` (call `a079`), searches the constructed map,
reads the value at `a019` and returns it at `a026`.

Hash-pinned decoded name construction, map-entry access, code-pointer LEA and
adjacent MOV witnesses establish these selected map assignments:

| Lookup name | Ordinary GL export stub in `libGLESv2.dll` | Code address stored in the resolver map |
| --- | --- | --- |
| `glBindTexture` | `2f330` | `10820` |
| `glDrawArrays` | `2f700` | `13a30` |
| `glDrawElements` | `2f750` | `13b50` |
| `glEGLImageTargetTexture2DOES` | `2f7a0` | `20350` |

The ordinary GL stubs jump to those same implementation bodies. The selected
`eglCreateWindowSurface`, `eglGetProcAddress`, `eglMakeCurrent` and
`eglSwapBuffers` map values are likewise the implementation addresses in the
previous table, **not the public stubs in the separate EGL DLL**.

This is now observed in the exact sidecars, not merely a possible loader
pattern. But it is **not automatically a missing production hook**:

- Production already observes lookup-returned GL targets and alternate
  `eglMakeCurrent` targets, including discovery after a successful native bind.
- A hook on an export stub alone must not be assumed to cover a distinct
  resolver-returned implementation address. Existing fixtures do not reproduce
  this complete split-DLL public-shim/implementation topology for every EGL
  lifecycle operation.
- Actual use of a distinct lookup-returned create/destroy/present target must be
  demonstrated before calling that an installed-player defect. The native
  provider's normal WGL/core-export route remains a separate possibility, with
  already-present public hook coverage.

This gives a **specific next regression target**: an authored two-DLL provider
with the observed public-import stubs and distinct queried implementations,
including lifecycle/protection/retirement and exact native-result checks. First
show the precise missed behavior; only then design a narrow production change.
Static evidence and an authored reproduction must still be distinguished from
this game's real resource/branch acceptance. No private offsets from this report
are to be called or installed as detours.

## 5. Reproduction and executed checks

New read-only CLI:
[`scripts/Inspect-PlayerProgramFiles.py`](../scripts/Inspect-PlayerProgramFiles.py).
It accepts one explicit ZIP, bounds entry counts/expansion, CRC-checks all entries,
retains only five selected PE byte arrays and writes a JSON report. It refuses
ambiguous selected basenames, unsafe paths, symlinks/reparse/special members,
encryption and oversized inputs. It does not extract archive paths or search an
installation for dependencies. Unknown hashes receive generic export/import
inventory, not the manually selected ANGLE map offsets.

```bash
.venv/bin/python scripts/Inspect-PlayerProgramFiles.py path/to/PF.zip
BSI_PLAYER_PF_ZIP=path/to/PF.zip \
  .venv/bin/python tests/player_program_files_audit_test.py -v

# The existing native auditor can select the player bytes without extraction.
.venv/bin/python scripts/Inspect-PlayerRenderer.py path/to/PF.zip
```

Dependencies remain `pefile==2024.8.26`, `capstone==5.0.6`; GNU objdump is needed
for the optional independent original-byte test. Executed here:

- **19/19 PF tests, zero skips**, including this original archive and **139
  distinct selected instruction comparisons against GNU objdump**: 65 EGL shim
  jumps and 74 GLES export/map/resolver instructions. Bytes, normalized mnemonic,
  RIP targets and direct branch targets where present match. This is not 139
  functions or complete control-flow/dataflow proof.
- Both Drive checksums match; all 105 entry CRCs were read/validated. New/old
  native reports ran on the actual 5.12 EXE/private provider members. Provider
  output equals the previous named-pair output.
- **17/17 previous provider tests, zero skips**, including the original pair's
  golden and its separate 243 selected-instruction objdump comparisons.
- Earlier renderer suite: **13 pass / one old-original golden skipped**. Qt
  inventory suite: **10 pass / one old-original golden skipped**. The four old
  reference EXEs are still absent; this fifth EXE is not substituted into those
  golden cases. Both new Python files compile with `py_compile`.
- Existing player/Qt auditor and production presentation/sampler hashes remain
  unchanged. No new Windows/driver/game/pixel run, shader package build, commit,
  push, workflow dispatch, merge, tag or public release was performed.

Resource provider/display/context/share generations, color/protection boundaries,
source and handle retirement, route revision/TTL and exact Off behavior remain
mandatory. Nothing here permits inspecting foreign resources, forcing legal
LINEAR composition into mip sampling or reporting success from counters alone.
