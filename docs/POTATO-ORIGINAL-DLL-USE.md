# Use the exact tutorial Potato DLL: separate, restart-only, explicitly reviewed

## Decision and current result

**Fallback only under the current requirement.** After requesting the original
tutorial DLL, the user explicitly selected **live Settings On/Off and 0–100
strength** (`live_required`). This unchanged DLL has no supported live-control
ABI: the restart-only guide below does **not** fulfill that choice. The selected
work is the [independently controllable native path](POTATO-DRAW-WRITE-FOOTPRINT.md),
with Save/Discard and draw-scoped Off restoration; its game effect is unproved.
Do not stack the two hook modules or present DLL swapping as a live slider.

The independent v6 path is confirmed **not working in the game**: 5,950 processed presentation frames,
11,663 NoSampler inspections, **zero Potato bindings**. There were two deferred
reconciliations and two producer inspections, still with zero bindings. The
[installed v6 report](evidence/potato-installed-v6.json) is separate from all
historical software/CI successes.

The v6 metadata ledger balances: 11,630 queued = 11,361 uncaptured-output
retirements + 256 captured-destination writes + 10 missed/guarded/busy + one
sharing change + two reconciliations, pending zero, peak two. Every reported
uncaptured-output retirement is at `draw-output-or-primitive-unavailable`.
This explains the dominant **conservative metadata removal path**, not actual
writes, the exact unsupported primitive, missing-hook identity or the causal
material graph. Both producer inspections yielded zero bindings; the latest
reported producer input is `960×540` base-only `GL_NEAREST` RGBA8. No mip forcing,
broader ownership guess or longer play request follows from this report.

**Use of the original is an opt-in alternative, not an improvement to the BSI
Potato slider.** It has process-wide persistent hooks/state and no supported
live strength/Off interface. Installing/removing it requires closing all player
processes. Never unload it beneath live hooks. No perfect compatibility,
anti-cheat/account safety, FPS benefit or NVIDIA-profile fix is claimed.

## Exact file, not another similarly named download

The previously supplied [ANSHU archive](https://drive.google.com/file/d/1Ex-E3YCwkq5wWschqaFMdNv9abB0MV9-/view)
contains the exact six members/five byte identities already compared and
[executed in the isolated software-WGL host](POTATO-DLL-PROBE.md). The tutorial
reference is [BOBBY LIVE 1M](https://www.youtube.com/watch?v=WD1vwPoxt64).
No new video-frame inspection is claimed. The
[earlier reference comparison](POTATO-ANSHU-REFERENCE.md) established full-byte
identity, not just matching names or code sections.

For the tutorial's **BS/MSI 5 / Strong `version.dll`** variant (identity D):

- Archive: `POTATO FF DLL BY ANSHU.rar`, 345,058 bytes.
- Archive SHA256: `ed56050e890d9bf9efa8df08cd5eee2f143041cb49b983760c231e2c1fbca449`.
- Password supplied with that reference: `ANSHUXPOTATO`.
- Member, omitting the repeated wrapper directory:
  `Nvidia  AMD/BS MSI 5/STRONG/version.dll`.
- DLL: **245,248 bytes**, SHA256
  `7440db72c09af59e8a3249e17fff0a1e964e4409bc6ec7b9c1686b35368fac22`.
- Existing native import anchor: **`proxy_VerQueryValueW`**, exact case.
- The independently tested original changes desktop WGL texture LOD bias and
  minimum LOD to 3.0 on authored resources, without requiring depth testing.

These archive folder labels are **not** a hardware/driver compatibility test.
The optional BS/MSI 5 Light file is identity E, SHA256
`b68e5b3586ffd2a57c680a0426818e71b06eadc3ce60b80507f42679892b3558`,
same size/anchor, bias/minimum 1.5. Intel/MSI 4 variants instead use
`proxy_DirectInput8Create`; do not guess an export or interchange files silently.
Do not use the generic `zlag_init` default for any of these files.

No original DLL, archive, extracted bytes, disassembly or patched commercial
player is bundled or redistributed here. The archive has no included source or
redistribution license; a known hash is identity, **not a safety certificate**.
Use only a local copy you are authorized to use and deliberately trust.

## Existing app route — no new Customizer build needed

The already-tested package at source
`2e5e6a0b726cf84dbc36c8ee5d5dcf8dce2c4d19` contains the generic
**PATCHER → DLL patcher**. No new game-fix build or new download is required for
this route. It adds a normal, hash-reviewed startup import using a uniquely
named local copy of the **unchanged** original DLL, rather than replacing a
system-library filename. Renaming does not remove the original's internal proxy
initialization or persistent-hook risks. This is not identical to the video's
original-name `version.dll` search-order installation.

### Prerequisites and isolating the comparison

1. Use an authorized, backed-up disposable installation. Preserve an independent
   known-good copy of the **whole installation** and your current EXE/recovery
   files. Do not distribute the patched player. An EXE import edit invalidates
   its Authenticode signature and may conflict with updates or product terms.
2. Close **all** player instances and keep updaters/other editing tools stopped.
   Select the actual `HD-Player.exe` normally launched, not another installation.
3. If another BSI startup-DLL transaction is already installed, **stop** and
   review/restore it before any further EXE edits. The importer intentionally
   supports one selected payload; do not overwrite its journal or chain a second
   import around it. Unrelated later edits/updates must not be discarded to make
   restore succeed.
4. In the disposable test installation, remove the independently authored native
   shader integration first: **PATCHER → Player interface → Shaders → Inspect
   compatibility → Remove shaders only**, then acknowledge/confirm. Exit all
   player processes again before proceeding. **Turning the Potato slider Off is
   not enough to prove the BSI hook module is absent.** Do not run both hook
   implementations together for this comparison. Removal preserves unrelated
   Settings customization/preferences; it does not restore native Display size.
5. Do not overwrite an existing `version.dll`, `dinput8.dll`, renderer DLL,
   GIF-RPC proxy or other mod. Use an isolated known baseline, and stop if there
   is an unexplained existing hook/proxy. Do not reset a driver profile, change
   renderer, disable protection/anti-cheat or modify system directories.

### Install the selected unchanged original through the existing tool

1. Extract the selected **Strong identity-D** file locally. Make a local copy
   named **`BsiPotatoOriginal.dll`** beside the selected `HD-Player.exe`.
   Do not modify any DLL bytes or replace any existing file of that name.
2. Verify its SHA256 equals the identity-D hash above. One read-only PowerShell
   command is:

   ```powershell
   (Get-FileHash -LiteralPath 'C:\path\to\BsiPotatoOriginal.dll' -Algorithm SHA256).Hash
   ```

   Hashing is not loading. Stop on a mismatch. No Python/SDK/script runtime is
   needed for normal recipient startup.
3. Open **PATCHER → DLL patcher** and enter:

   | Field | Exact value |
   | --- | --- |
   | DLL filename | `BsiPotatoOriginal.dll` |
   | Existing native function export | `proxy_VerQueryValueW` |

4. Click **1. Inspect EXE + DLL**. Check the DLL hash, EXE identity, direct import
   list and warnings. This is static parsing, **not** a safety or complete
   transitive-dependency check. If inspection refuses the actual EXE/layout,
   stop; do not force a patch or choose an arbitrary export.
5. Click **2. Review & add import**, explicitly acknowledge the startup/signature
   risks, then **Add startup import**. BSI rechecks both hashes and creates a
   unique original EXE backup/recovery record before committing. Keep both.
6. A successful file operation is **not a successful game test**. If you choose
   to execute the trusted original in the disposable installation, launch the
   player normally. Windows resolves the anchor but the patcher never calls it;
   the DLL's ordinary process-attach initialization starts the original hooks.
   No BSI app/launcher is required to remain running. Successful game startup,
   actual material pixels and driver behavior remain unverified here.

The existing BSI sampler-binding diagnostics and strength slider do **not**
control or measure this original DLL. Do not add an always-green “active” label
or interpret zero BSI bindings as a probe of the original's behavior.

### Stop / remove / restore

This original has no supported in-process Off. **Close all player processes**,
then use **DLL patcher → Restore original EXE**. Restore checks the recorded
patched/current/original hashes and preserves its unique backup. If a later edit
or update changed the EXE, stop and reconcile; do not overwrite that work.

**Restore the EXE dependency before moving/deleting the required DLL.** Deleting
it first can prevent startup. Once restoration succeeds and all old processes
have exited, the local DLL can be moved out. Do not use `FreeLibrary`, a live
unhook or an injected initializer call. A fresh launch of the restored isolated
baseline is the native comparison; merely closing one child window is not proof
all original hooks exited. Keep the original backup and recovery record.

For this restart-only alternative, the visual check is the same scene across
**native launch → original-Strong launch → restored-native launch**. That is
separate from the independent module's stronger live Off/restoration contract;
process exit is not an in-place Off test. No benefit is claimed without actual
material images. Do not repeat a long gameplay/counter collection as a remedy.

## What was checked now, and what was not

The ANSHU and PF archives were downloaded again through the connected Drive,
with sizes and complete hashes matched to the prior evidence. All six bounded
ANSHU DLL members were read as data using the pinned upstream UnRAR tool; none
was loaded. The PF player was read as data with its selected ZIP member CRC
checked. **No proprietary EXE/DLL/service/driver/tool was executed.**

The **existing production import planner** was compiled as a native Linux
*data-test adapter* and run on this exact PF `5.12.120.6303` player plus each
original DLL as non-executable `.bin` data. All six plans/five unique identities
passed, each changing 43 import descriptors to 44. Independent `pefile` checks
confirm original sections/overlay/entry point/descriptors preserved, a writable
non-executable added section, real terminated named-import thunks and repaired
PE checksum. Repeat output is byte-identical; wrong `zlag_init` exports fail
without creating output. Source inputs retain exact bytes. This is **structural
compatibility of these data**, not successful loading of the installed player.

[Exact data-plan evidence](evidence/potato-original-dll-startup-plan.json)
records DLL/input/output hashes and limits. The shipping startup implementation
and UI are byte-unchanged from the package already available to the user; no
new production code, binary, original execution, tag, release or merge is needed
for these instructions. The native parser guards also pass, including 2,000
bounded input-mutation/idempotence controls.

Historical original execution is recorded separately in
[POTATO-DLL-PROBE.md](POTATO-DLL-PROBE.md): five unique originals loaded as
`BsiPotatoOriginal.dll` in contained Windows software-WGL children and changed
authored mip pixels. They were **not** loaded into this PF player or a game.
Generic BSI apply/restore/loader/recipient tests do not establish that missing
original/player/physical-driver acceptance either. No fresh original execution
was scheduled just to repeat identical authored software evidence.
