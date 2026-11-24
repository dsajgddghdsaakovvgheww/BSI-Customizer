# Potato: deep static audit of the four HD-Player references

## Answer to the user's question

**Yes: the actual player binaries are more useful than another speculative hook
change. This audit follows native code, imports and registered resources, not
just a search for renderer-related words.** All four supplied reference EXEs
implement the same useful boundary: choose a graphics-library basename, load
it, resolve **`grApiInit`**, then call the resolved pointer and use a private
renderer interface.

The EXEs also construct native context/image callback tables. One observed
context-callback slot changes between the older and newer samples. A further
four-version code/data pass follows a **separate parented QWindow's native window
ID into graphics initialization and the private renderer's first dispatch slot**.
This is concrete evidence for following the **matching graphics DLL next**, not
for blindly hooking a guessed private table offset.

**This audit does not prove the cause of the installed game's zero replacements.
It is not a new Potato fix or new game/FPS acceptance.** The last game report remains
3,154 presentation frames / 6,297 NoSampler / zero replacements. The packaged
`b807653` test candidate has not been confirmed in-game. No renderer/ownership/
protection gate is changed by this research.

**Provider follow-up (2026-11-17):** the user has now supplied the two graphics
DLLs in a separate ZIP. The [new provider audit](POTATO-RENDERER-PROVIDER-AUDIT.md)
verifies their 5.12.120.6303 identities and follows `grApiInit` into real GL
resolution and internal image code. This supersedes the missing-input limitation,
not the installed-game/ABI/ownership limitations of this EXE-only report.
The [later PF archive audit](POTATO-PLAYER-PF-AUDIT.md) also verifies a fifth,
5.12.120.6303 EXE supplied alongside those exact providers and ANGLE EGL/GLES
sidecars. The four-reference identities and historical validation below are
not relabelled as that newer input or as installed-game acceptance.

## Inputs and identity

Source: [the four-file HD-Player repository at pinned commit
`6e4dc894eb1b6956060b3f76d13fc438e5178d03`](https://github.com/MrPcGamerYT/HD-Player/tree/6e4dc894eb1b6956060b3f76d13fc438e5178d03).
The Git blob SHA-1 of every downloaded input and the complete EXE SHA-256 were
checked. The two ZIPs contain the EXE, **not its adjacent graphics providers**.
These are the supplied references: filenames saying “latest” are not a current
release database, and a pinned hash is not independent vendor-authenticity or
installed-player identity verification.

| EXE FileVersion | EXE bytes | Complete EXE SHA-256 |
| --- | ---: | --- |
| 5.9.300.6315 | 10,068,960 | `1f473ca2ec1e684cf85dbf1cacd47ed0cb33c27c6fe298ada1775f2a3414f2ed` |
| 5.10.150.1016 | 11,991,040 | `bcec63030f032fef60850559b5a5c40fdcce44f3b129aa5ccee05e990957467b` |
| 5.22.75.6322 | 23,858,608 | `b373ad8ce5dbc6b1e605b3c153d20b69bf5769b7ff82f896291e4844ab418efd` |
| 5.22.285.1013 | 33,149,872 | `d2f8509d025f90bd23604aa1c8f6981c456468bceab4db4a30bb380806826f39` |

[Machine-readable evidence](evidence/potato-hd-player-native-renderer.json)
contains input filenames/archive hashes/Git blobs, imports, exact RVA witnesses,
registered-resource hashes, auditor identity and validation boundaries. Original
EXEs/ZIPs, recovered QML and disassemblies stay outside Git in ignored research
storage. **No original player or DLL was loaded, executed, patched or shipped.**

## 1. Native graphics loading, not merely a DLL-name string

The decoded graphics initialization path contains both **`HD-Opengl-Native`**
and **`libOpenglRender`** as basename operands. Its loader helper reaches the
actual `KERNEL32!LoadLibraryW` import; its symbol helper reaches the actual
`KERNEL32!GetProcAddress` import. The string `grApiInit` is constructed for that
symbol lookup. Its return is saved in the x64 nonvolatile RBX register and is
subsequently called through RBX after a null check.

```text
HD-Player's native graphics initialization
    -> select graphics-library basename
    -> loader helper -> LoadLibraryW
    -> symbol helper -> GetProcAddress for grApiInit
    -> call the returned function pointer
    -> private renderer dispatch / callback interface
```

This describes implemented code paths, **not a successful load in the installed
session**. Neither an absolute DLL path nor which selector branch the game takes
is established by these EXE-only inputs. Windows DLL loading convention makes
`HD-Opengl-Native.dll` / `libOpenglRender.dll` the concrete companion-file leads;
a different installed build still requires its own identity and dependency audit.

All addresses below are **file-image RVAs**, not live addresses or patch offsets.

| Version | Graphics-init range begins | LoadLibraryW API call | GetProcAddress API call | Call through resolved pointer |
| --- | --- | --- | --- | --- |
| 5.9.300.6315 | `0x10C2A0` | `0x2A1322` | `0x2A1585` | `0x10C631` |
| 5.10.150.1016 | `0x13B5C0` | `0x2D8222` | `0x2D8485` | `0x13B95F` |
| 5.22.75.6322 | `0x2DF990` | `0x4B3D72` | `0x4B3FD5` | `0x2DFD21` |
| 5.22.285.1013 | `0x3E9FE0` | `0x5E9DD2` | `0x5EA035` | `0x3EA371` |

These EXEs have **no normal or delay imports of OpenGL/EGL/GLES/Vulkan DLLs**.
They do import D3D11/DXGI device/factory APIs.
Neither observation identifies the active game API: dynamic providers and
private dispatch are exactly why an EXE import list is not a renderer trace.
Likewise, an “OpenGL” DLL basename need not mean every supported engine mode in
that DLL is OpenGL. Keep Qt/interface rendering separate from game rendering.

## 2. Native context callbacks and a real version boundary

Immediately before the graphics-init call, the native startup routine calls two
builders which store code pointers into separate callback structures. Their
pointed-to functions contain the labels **`imgdOnEglCreateContext`** and
**`tmapOnEglCreateContext`**. The evidence records each adjacent LEA/code-pointer
store, its destination displacement, the builder call and its later graphics
initialization call. This is more specific than seeing “EGL” somewhere in an EXE.

| Version | Image callback builder | Observed adjacent pointer stores | Its context callback displacement | Texture-map callback builder |
| --- | --- | ---: | --- | --- |
| 5.9.300.6315 | `0x1142A0` | 9 | `+0x10` | `0x23C6B0` |
| 5.10.150.1016 | `0x144460` | 9 | `+0x10` | `0x26FE70` |
| 5.22.75.6322 | `0x2E92E0` | 15 | `+0x18` | `0x452560` |
| 5.22.285.1013 | `0x3F4CD0` | 15 | `+0x18` | `0x582B60` |

The texture-map builder has **11 observed adjacent pointer stores**, with the
context-labelled callback at **`+0x8`** in all four samples. In contrast, the
image callback moves from **`+0x10` to `+0x18`**, alongside **9 to 15 observed
pointer assignments**. These are observed assignments, **not a recovered complete
struct declaration or stable private API**. Do not transplant a table layout
from another version or call these callbacks with guessed parameters.

Other inspected EXE strings refer to image detection, package-specific texture
names and texture-map/mipmap handling. They can help navigate native code but do
not prove which game images are owned, unprotected or active. HST/HPP shared-memory
transport strings also do **not** establish an EGLImage or color-backing alias.
No private context callback is treated as permission to query/change its GL state.

## 3. Startup evidence is now binary-specific, but is still not a trace

For all four references, a common decoded native startup range has a UI-init call
before its explicit graphics-init call in instruction order. The UI initializer
contains the actual imported **`QQmlApplicationEngine::load(QUrl)`** call.
The older MSI normal branch was also manually followed from successful UI init
to graphics init; the evidence does not merely compare unrelated string offsets.

| Version | Native UI-init call → target | QML-engine load call inside UI initializer | Later graphics-init call → target |
| --- | --- | --- | --- |
| 5.9.300.6315 | `0x637E1` → `0x8B4B0` | `0x8BA9A` | `0x63FBB` → `0x10C2A0` |
| 5.10.150.1016 | `0x6EE2E` → `0x9A000` | `0x9A6CD` | `0x6F601` → `0x13B5C0` |
| 5.22.75.6322 | `0xA818D` → `0xF7FF0` | `0xF8D9F` | `0xA8A9F` → `0x2DF990` |
| 5.22.285.1013 | `0xBC8D1` → `0x11F2C0` | `0x1203EB` | `0xBD245` → `0x3E9FE0` |

### The native-window handoff was followed, not left as an unknown pointer

A focused additional pass resolved the previously undecoded QString/QUrl and
small window-ID getter. All four UI initializers construct the URL
**`qrc:/plr.qml`**. The Qt5 immutable QString header and Qt6 UTF-16 data are read
only from initialized file bytes, not from arbitrary BSS annotations.

All four also implement this native path:

```text
UI initializer constructs a separate QWindow with an existing UI window as parent
    -> save that child object's pointer in the UI-state structure
    -> child QWindow::winId() -> save native ID in a different structure member
    -> small getter reads that exact native-ID member
    -> native startup passes getter's return as graphics init's first argument
    -> graphics init retains it and passes it in RCX to private dispatch slot 0
```

| Version | Parented QWindow constructor | Child winId call | Native-ID getter | Private slot-0 call receiving the ID |
| --- | --- | --- | --- | --- |
| 5.9.300.6315 | `0x8BDA5` | `0x8BE30` | `0x86B50` | `0x10C748` |
| 5.10.150.1016 | `0x9A9D8` | `0x9AA63` | `0x94600` | `0x13BAF0` |
| 5.22.75.6322 | `0xF9183` | `0xF920F` | `0xF1740` | `0x2DFF32` |
| 5.22.285.1013 | `0x120B4B` | `0x120C35` | `0x1185A0` | `0x3EA582` |

The stored native-ID member displacement changes across these versions:
**`+0xE8`, `+0xF8`, `+0x100`, `+0x110`**. These offsets are evidence only.
The supplement in the JSON records the constructor parent/member operands,
child-pointer and ID stores, same-singleton getter, return-to-argument flow and
private-table load/call. **93 additional GNU objdump instruction checks** corroborate
these sites; these are check counts, with some overlap with the primary audit,
not 93 claimed additional unique functions.

This establishes a distinct native rendering-target handoff in the supplied code;
it is stronger than calling every Qt texture a game scene texture. It **does not**
identify the live game's surface/context/API, prove whether contexts share, recover
the called DLL's implementation or establish that our current hooks miss it.

**Do not turn this into “our bootstrap definitely ran first” or “it definitely ran
too late.”** Unwind ranges can be split and are not complete source functions;
instruction order alone does not prove control-flow dominance, active branches,
Qt asynchronous/child/thread timing or the installed player's startup. A QML
source completion handler is not a native renderer trace either. The new facts
argue against treating speculative relocation into `DllMain`/PE imports as the
answer. The cached-target repairs remain independently reproduced fixes, not a
game diagnosis retroactively established by this audit.

## 4. Actual QML resources, rather than assuming a GitHub source copy matches

Recovered resource identities come from decoded x64 argument setup into an
actual **`qRegisterResourceData` import/thunk**, with versioned array bases,
connected-tree traversal, node/name hashes, raw-section bounds and complete
bounded decompression. Repeated registrations of the same three arrays are
**deduplicated**, not counted as extra resource pools.

There are **three distinct mapped pools in each EXE**. “File entries” below means
connected file nodes, not unique logical resources, source streams or a guarantee
that a resource wins Qt's runtime registration/cache resolution.

| Version | Connected file entries | `/plr.qml` decoded bytes | Root completion source line | `/plr.qml` SHA-256 |
| --- | ---: | ---: | ---: | --- |
| 5.9.300.6315 | 550 | 34,712 | 149 | `e69d8d184d63199f6d29889fde7d5c2330de8bd7f09c6e4180cc9298e436a1db` |
| 5.10.150.1016 | 582 | 61,269 | 153 | `9608157ad9438714cae687913838d4f3ecaa16f7ac5b0e2a385c70872460beb7` |
| 5.22.75.6322 | 736 | 169,480 | 150 | `8ea15086f63141adb87cb7b4f31f97a22bed01760ed9a4c1537af40729142af9` |
| 5.22.285.1013 | 880 | 199,974 | 207 | `75937ed0893a1b96359e06647f04c194840920156b98ae3c846ecce19ff8c57c` |

Each recovered root declares its completion handler and calls the backend's
`setBackendUiParams` layout function through `fSetUiParams`. The recovered
`/UiGraphicsSetting.qml` hashes are recorded separately too. These exact sources
are **not byte-identical** to the previously consulted 178,834-byte public
`plr.qml` blob `08aca98778589e8c168ed2a02959c5202ea68239`. Source resemblance
must not stand in for an exact installed binary/resource identity. The older
Qt5 samples also contain compiled QML: **source presence does not prove which
compiled/source representation executes**.

## What this answers, and what it cannot answer yet

Confirmed in the supplied files:

- A concrete native graphics-library loading/symbol/indirect-call boundary in
  all four references, independently decoded—not only the Qt UI layer.
- EXE-side context/image callback structures, including a version-dependent
  pointer displacement. There is real native renderer coordination to inspect.
- The separate native child-window ID's code path into the private renderer
  dispatch, without inventing a complete signature or touching a live context.
- Exact binary-specific QML resource identities, actual static root URL and
  narrower startup evidence than the earlier external-source-only observation.

Not proved by this static EXE audit:

- Which graphics provider/version/path the installed Free Fire session loaded;
  its exact EXE/module hashes were not supplied in the game report.
- Whether material GL calls resolve through standard exports, cached pointers,
  another private dispatch table, another process/API or unobserved contexts.
- Whether EGLImage/native-buffer/shared backing/private copies are used, and
  their exact protection, lifecycle, namespace and resource relationships.
- Which gap caused zero replacements, whether the new candidate fixes it, or
  material pixels/exact game Off/FPS. The legal LINEAR consumer is not changed.

The original Potato DLL's independently measured persistent bind/draw mutations
remain a mechanism difference from our bounded draw-scoped sampler path. This
new audit does not rerun that original DLL or magically establish full parity.

## Provider follow-up: the requested ZIP has now been inspected

**Do not resend the four identical reference EXEs or the same provider ZIP.**
The [separate provider audit](POTATO-RENDERER-PROVIDER-AUDIT.md) records the supplied
`HD-Opengl-Native.dll` and `libOpenglRender.dll`, both 5.12.120.6303, their exact
hashes and code-bearing renderer/image evidence. No account/config data or entire
installation was needed. Their identity with the launched installation remains
unverified; an EXE or Qt display version alone cannot establish that match.

The investigation order remains:

1. Statically inspect those actual DLLs' exports/imports/delay imports, verify
   `grApiInit`, follow its table construction and its dependencies. Request an
   additional provider only if that code/dependency chain identifies it.
2. Trace the actual GL resolver/dispatch and image creation/import/destruction/
   storage paths. Separate guest/context identity from proven selected-game
   resources; retain provider/display/generation/protection boundaries.
3. Compare only demonstrated paths against our hook inventory. Reproduce a
   specific missed entry or typed dependency in a negative fixture and legal
   driver-pixel test **before** changing production hooks.
4. If static dispatch cannot establish which branch runs, use a narrowly scoped
   read-only runtime observation/new existing diagnostics and say what is still
   unknown. Do not manufacture success or turn private offsets into a blind hook.

Thus the EXEs narrowed the investigation, and the newly supplied providers now
supply additional instructions in their own report. The EXE-only samples do not
themselves reveal those implementations or establish a matching live renderer.

## Reproducibility and executed checks

New research-only tool: [`scripts/Inspect-PlayerRenderer.py`](../scripts/Inspect-PlayerRenderer.py).
It reuses the existing protected local-file/ZIP and PE metadata reader, and adds
bounded x64/import, literal-to-instruction, helper-call, callback-store and
registered-QML evidence. It does not load an EXE/DLL, attach to a process, call a
private ABI, scan an installation, access the network, extract files or patch a
player. JSON explicitly keeps runtime/game/support certification **false**.

```sh
python -m pip install pefile==2024.8.26 capstone==5.0.6
python scripts/Inspect-PlayerRenderer.py "/path/to/HD-Player.exe"
# EXE-only reference ZIPs are read as bytes, never extracted/executed:
python scripts/Inspect-PlayerRenderer.py "/path/to/HD-Player MSI Latest version.zip"

python tests/player_renderer_audit_test.py -v
# Optional means explicit supplied references, not an optional pass claimed here:
BSI_RENDERER_REFERENCE_DIR=/path/to/pinned-HD-Player-repo \
  python tests/player_renderer_audit_test.py -v
```

Executed locally for this audit:

- **14/14 renderer-auditor tests**, including actually reading **all four pinned
  references**, and **11/11 retained Qt-inventory tests**, also with all four
  reference inputs supplied. None executes their code.
- Negative controls distinguish strings-only / a fake LEA inside an immediate
  from actual decoded references; a partial-register write revokes the saved
  pointer witness. Leaf callback builders, duplicate registrations, raw/zlib
  equality, corrupt names/cycles/payloads, bounded/trailing decompression,
  virtual-zero-fill, overlapping unwind ranges and no-extraction ZIPs are checked.
- Independent **GNU objdump 2.40** crosschecks all normal/delay import module
  names and **52 / 52 / 64 / 64 = 232 instruction witnesses**, covering loader/
  lookup, resolved-pointer calls, native UI/QML/graphics calls and callback pointer
  assignments. The small JSON evidence is retained; proprietary disassemblies are
  not added to Git. Example check with the first pinned sample:

  ```sh
  objdump -p "HD-Player MSI v5.9.exe"
  objdump -d -M intel --start-address=0x14010c2a0 \
    --stop-address=0x14010ca78 "HD-Player MSI v5.9.exe"
  # Other RVA ranges and API/store witnesses are in the JSON report.
  ```

- The focused window/URL supplement is a separate manually seeded code/data
  pass, not extra coverage claimed for the generic CLI. **23 / 23 / 23 / 24 = 93**
  further objdump checks verify the getter/constructor/stores/argument/private-call
  instruction sites. Its version-matched numeric witnesses are retained in the
  same JSON under `focusedNativeWindowAudit`. For example:

  ```sh
  objdump -d -M intel --start-address=0x140086b50 \
    --stop-address=0x140086b5f "HD-Player MSI v5.9.exe"
  objdump -d -M intel --start-address=0x14008bd94 \
    --stop-address=0x14008be44 "HD-Player MSI v5.9.exe"
  ```

- The auditor's complete PE-derived output matches the retained golden reports
  for both original named inputs/ZIPs and verified standalone EXEs. Every original
  player SHA-256 is unchanged after the audits; Python syntax and diff-whitespace
  checks pass.

**Production native shader source, UI, installer and tested `b807653` payload
remain unchanged.** These local static-tool tests are not a new Windows shader/
package CI result. PR121 remains draft/open/unmerged/unreleased; no new binary
candidate, generalized shared-image support or installed-game success is claimed.
