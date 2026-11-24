"""Static-only PF archive/ANGLE jump-edge tests. No PE is loaded or executed.

BSI_PLAYER_PF_ZIP opts into the original user archive and objdump comparisons.
These validate research tooling, not a game rendering or Potato repair.
"""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import warnings
import zipfile

from emulator_qt_report_test import fixture as qt_fixture

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("pf_audit", ROOT / "scripts/Inspect-PlayerProgramFiles.py")
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)
SYMBOL = "?MakeCurrent@egl@@YAIPEAX000@Z"


def sidecar(*, shim=False, dll=True, forwarded=False, data_export=False, zero_fill=False, embedded=False, symbol=SYMBOL):
    """Independent synthetic export and import tables, never executable."""
    data = bytearray(qt_fixture(dll=True))
    struct.pack_into("<H", data, 0x96, 0x2022 if dll else 0x22)
    data[0x1000:0x2000] = b"\xcc" * 0x1000
    struct.pack_into("<II", data, 0x108, 0x2800, 0x100)
    struct.pack_into("<IIHHIIIIIII", data, 0x2800, 0, 0, 0, 0, 0x2940, 1, 1, 1, 0x2840, 0x2850, 0x2860)
    target = 0x2890 if forwarded else 0x7ff0 if data_export else 0x1000
    struct.pack_into("<I", data, 0x2840, target)
    struct.pack_into("<I", data, 0x2850, 0x2900)
    struct.pack_into("<H", data, 0x2860, 0)
    data[0x2890:0x289d] = b"other.target\0"
    name = ("eglMakeCurrent" if shim else symbol).encode() + b"\0"
    data[0x2900:0x2900 + len(name)] = name
    data[0x2940:0x2949] = b"TestOnly\0"
    if zero_fill:
        struct.pack_into("<I", data, 0x188 + 40 + 16, 0x5000)
    if shim:
        struct.pack_into("<II", data, 0x110, 0x2000, 40)
        struct.pack_into("<IIIII", data, 0x2000, 0x3200, 0, 0, 0x3500, 0x3300)
        struct.pack_into("<Q", data, 0x3200, 0x3400)
        struct.pack_into("<Q", data, 0x3300, 0x3400)
        hint = b"\0\0" + symbol.encode() + b"\0"
        data[0x3400:0x3400 + len(hint)] = hint
        data[0x3500:0x350e] = b"libGLESv2.dll\0"
        jump = b"\xff\x25" + struct.pack("<i", 0x3300 - 0x1006)
        if embedded:
            jump = b"\x48\xb8" + jump + b"\0\0\xc3"  # FF25 hidden inside MOV's immediate.
    else:
        jump = b"\xe9" + struct.pack("<i", 0x1100 - 0x1005)
        data[0x1100] = 0xc3
    data[0x1000:0x1000 + len(jump)] = jump
    return bytes(data)


class ProgramFilesAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bsi-pf-static-only-")
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.path = self.folder / "PF.zip"

    def members(self):
        return {"HD-Player.exe": qt_fixture(), "HD-Opengl-Native.dll": qt_fixture(dll=True),
                "libOpenglRender.dll": qt_fixture(dll=True), "libEGL.dll": sidecar(shim=True), "libGLESv2.dll": sidecar()}

    def archive(self, members=None):
        with zipfile.ZipFile(self.path, "w", zipfile.ZIP_DEFLATED) as z:
            for name, data in (self.members() if members is None else members).items():
                z.writestr(name, data)
        return self.path

    def test_archive_is_crc_checked_without_extraction_or_changes(self):
        path = self.archive()
        before = path.read_bytes()
        files = set(self.folder.iterdir())
        result = audit.inspect(path)
        self.assertEqual(path.read_bytes(), before)
        self.assertEqual(set(self.folder.iterdir()), files)
        self.assertEqual(result["missingSelectedBasenames"], [])
        self.assertTrue(result["archive"]["allMemberCrcsValidated"])
        self.assertEqual(result["archive"]["archiveSha256"], hashlib.sha256(before).hexdigest())
        for key in ("archivePathsExtracted", "filesLoadedOrExecuted", "targetModified", "privateInterfacesCalled",
                    "runtimeLoaderBindingsOrActiveRendererProved", "materialOwnershipOrPotatoRepairProved"):
            self.assertFalse(result[key])

    def test_only_declared_import_jump_joins_exact_named_export(self):
        result = audit.inspect(self.archive())
        bridge, = result["publicEglImportBridges"]
        self.assertEqual((bridge["publicName"], bridge["shimExportRva"], bridge["importedSymbol"], bridge["targetExportRva"]),
                         ("eglMakeCurrent", 0x1000, SYMBOL, 0x1000))
        self.assertTrue(bridge["targetIsRawCode"])
        self.assertEqual(bridge["instruction"]["ripOperandTargetRvas"], [0x3300])
        gles = result["sidecars"]["libglesv2.dll"]["exports"][0]
        self.assertEqual(gles["entryJump"]["directTargetRva"], 0x1100)
        self.assertTrue(gles["entryJump"]["directTargetIsRawCode"])

    def test_mismatched_import_symbol_never_guesses_a_target(self):
        members = self.members(); members["libEGL.dll"] = sidecar(shim=True, symbol="?NotTheSame@egl@@YAXXZ")
        bridge, = audit.inspect(self.archive(members))["publicEglImportBridges"]
        self.assertIsNone(bridge["targetExportRva"])
        self.assertFalse(bridge["targetIsRawCode"])

    def test_data_and_zero_fill_are_not_callable_exports(self):
        for zero_fill in (False, True):
            with self.subTest(zero_fill=zero_fill):
                members = self.members(); members["libGLESv2.dll"] = sidecar(data_export=True, zero_fill=zero_fill)
                report = audit.inspect(self.archive(members))
                self.assertFalse(report["publicEglImportBridges"][0]["targetIsRawCode"])
                exported = report["sidecars"]["libglesv2.dll"]["exports"][0]
                self.assertIsNone(exported["entryJump"])
                self.assertEqual(exported["zeroFillIsNotFileBytes"], zero_fill)

    def test_forwarder_is_not_disassembled_as_a_jump(self):
        members = self.members(); members["libEGL.dll"] = sidecar(shim=True, forwarded=True)
        report = audit.inspect(self.archive(members))
        self.assertEqual(report["publicEglImportBridges"], [])
        self.assertIsNone(report["sidecars"]["libegl.dll"]["exports"][0]["entryJump"])

    def test_jump_pattern_inside_an_immediate_is_not_a_thunk(self):
        members = self.members(); members["libEGL.dll"] = sidecar(shim=True, embedded=True)
        self.assertEqual(audit.inspect(self.archive(members))["publicEglImportBridges"], [])

    def test_unknown_hash_gets_no_seeded_map_or_installed_identity(self):
        result = audit.inspect(self.archive())
        self.assertEqual(set(result["previousProviderWholeFileHashMatches"].values()), {False})
        for item in result["sidecars"].values():
            self.assertFalse(item["focused"]["knownWholeFileHashMatched"])
            self.assertEqual(item["focused"]["manuallySelectedMapEntries"], [])
            self.assertEqual(item["focused"]["manuallySelectedLookupInstructions"], [])
            self.assertFalse(item["signatureValidated"])

    def test_missing_inputs_are_reported_without_adjacent_file_search(self):
        self.folder.joinpath("libGLESv2.dll").write_bytes(sidecar())
        report = audit.inspect(self.archive({"HD-Player.exe": qt_fixture()}))
        self.assertIn("libglesv2.dll", report["missingSelectedBasenames"])
        self.assertEqual(report["sidecars"], {})
        self.assertIsNone(report["previousProviderWholeFileHashMatches"]["libopenglrender.dll"])

    def test_nonlinked_nested_names_and_unrelated_assets_are_inventoried_only(self):
        entries = {"PF/" + key: value for key, value in self.members().items()}
        entries["PF/Assets/readme.txt"] = b"not code"
        report = audit.inspect(self.archive(entries))
        self.assertEqual(report["missingSelectedBasenames"], [])
        self.assertEqual(report["archive"]["members"][-1]["sha256"], hashlib.sha256(b"not code").hexdigest())
        self.assertEqual(set(self.folder.iterdir()), {self.path})

    def test_traversal_absolute_drive_and_ambiguous_names_refuse(self):
        for name in ("../HD-Player.exe", "/HD-Player.exe", "C:\\HD-Player.exe", "./HD-Player.exe", "bad//file.txt", "bad./file.txt"):
            with self.subTest(name=name), self.assertRaises(audit.AuditError):
                audit.inspect(self.archive({name: b"ignored"}))
        entries = self.members(); entries["elsewhere/hd-player.EXE"] = qt_fixture()
        with self.assertRaisesRegex(audit.AuditError, "Ambiguous"):
            audit.inspect(self.archive(entries))

    def test_duplicate_case_and_linked_members_refuse(self):
        entries = self.members(); entries["hd-player.EXE"] = qt_fixture()
        with self.assertRaisesRegex(audit.AuditError, "ambiguous"):
            audit.inspect(self.archive(entries))
        with zipfile.ZipFile(self.path, "w") as z:
            info = zipfile.ZipInfo("HD-Player.exe"); info.create_system = 3; info.external_attr = 0o120777 << 16
            z.writestr(info, b"elsewhere")
        with self.assertRaisesRegex(audit.AuditError, "Linked"):
            audit.inspect(self.path)
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(self.path, "w") as z:
                z.writestr("same.txt", b"a"); z.writestr("same.txt", b"b")
        with self.assertRaisesRegex(audit.AuditError, "Duplicate"):
            audit.inspect(self.path)

    def test_too_many_and_oversized_members_refuse_before_payload_reads(self):
        self.archive({f"empty-{n}": b"" for n in range(513)})
        with self.assertRaisesRegex(audit.AuditError, "count"):
            audit.inspect(self.path)
        self.archive({"large": b"small"})
        raw = bytearray(self.path.read_bytes()); central = raw.index(b"PK\x01\x02")
        struct.pack_into("<I", raw, central + 24, 64 * 1024 * 1024 + 1); self.path.write_bytes(raw)
        with self.assertRaisesRegex(audit.AuditError, "Oversized"):
            audit.inspect(self.path)

    def test_corrupted_member_crc_is_rejected(self):
        with zipfile.ZipFile(self.path, "w", zipfile.ZIP_STORED) as z:
            z.writestr("asset.txt", b"unmodified payload")
        raw = bytearray(self.path.read_bytes()); raw[raw.index(b"unmodified payload")] ^= 1; self.path.write_bytes(raw)
        with self.assertRaises(zipfile.BadZipFile):
            audit.inspect(self.path)

    def test_encrypted_and_reparse_members_are_refused(self):
        self.archive({"asset.txt": b"data"})
        raw = bytearray(self.path.read_bytes()); central = raw.index(b"PK\x01\x02")
        struct.pack_into("<H", raw, central + 8, 1); self.path.write_bytes(raw)
        with self.assertRaisesRegex(audit.AuditError, "encrypted"):
            audit.inspect(self.path)
        with zipfile.ZipFile(self.path, "w") as z:
            info = zipfile.ZipInfo("asset.txt"); info.create_system = 0; info.external_attr = 0x400
            z.writestr(info, b"data")
        with self.assertRaisesRegex(audit.AuditError, "Linked"):
            audit.inspect(self.path)

    def test_corrupt_deflate_cli_failure_stays_machine_readable(self):
        self.archive({"asset.txt": b"x" * 4096})
        raw = bytearray(self.path.read_bytes())
        payload = 30 + len("asset.txt")
        raw[payload] = 7  # DEFLATE reserved block type, before any PE parser.
        self.path.write_bytes(raw)
        run = subprocess.run([sys.executable, str(ROOT / "scripts/Inspect-PlayerProgramFiles.py"), str(self.path)],
                             capture_output=True, text=True, timeout=30)
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("error", json.loads(run.stdout))
        self.assertNotIn("Traceback", run.stderr)
        self.assertFalse(json.loads(run.stdout)["targetModified"])

    def test_selected_sidecar_must_be_an_x64_dll(self):
        for bad in (sidecar(dll=False), b"not a PE"):
            members = self.members(); members["libEGL.dll"] = bad
            with self.subTest(kind=bad[:2]), self.assertRaises(audit.AuditError):
                audit.inspect(self.archive(members))
        bad = bytearray(sidecar()); struct.pack_into("<H", bad, 0x84, 0x14c)
        members = self.members(); members["libGLESv2.dll"] = bad
        with self.assertRaisesRegex(audit.AuditError, "x64"):
            audit.inspect(self.archive(members))

    def test_linked_path_and_machine_readable_cli_failure(self):
        self.archive(); link = self.folder / "linked.zip"
        link.symlink_to(self.path)
        with self.assertRaisesRegex(audit.AuditError, "Linked"):
            audit.inspect(link)
        self.path.write_bytes(b"not a zip")
        run = subprocess.run([sys.executable, str(ROOT / "scripts/Inspect-PlayerProgramFiles.py"), str(self.path)],
                             capture_output=True, text=True, timeout=30)
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("error", json.loads(run.stdout))
        self.assertFalse(json.loads(run.stdout)["filesLoadedOrExecuted"])

    @unittest.skipUnless(os.environ.get("BSI_PLAYER_PF_ZIP"), "Explicit original PF ZIP required")
    def test_original_pf_identity_and_angle_edges(self):
        path = Path(os.environ["BSI_PLAYER_PF_ZIP"])
        before = hashlib.sha256(path.read_bytes()).hexdigest()
        report = audit.inspect(path)
        self.assertEqual(before, "8922e6b30903d8f1552abb3a7a83477532bd699277fcbbdf222a24e73407b990")
        self.assertEqual(report["archive"]["archiveBytes"], 33609789)
        self.assertEqual(report["archive"]["entryCount"], 105)
        self.assertEqual(report["archive"]["uncompressedBytes"], 110824467)
        self.assertEqual(report["missingSelectedBasenames"], [])
        self.assertTrue(all(report["previousProviderWholeFileHashMatches"].values()))
        player = next(m["pe"] for m in report["archive"]["members"] if m["name"] == "HD-Player.exe")
        self.assertEqual(player["sha256"], "9138bf12725d389f2a2bd84495e2634428fafb292f23a5fa46e200cda986960d")
        self.assertEqual(player["fileVersion"], "5.12.120.6303")
        egl, gles = (report["sidecars"][key] for key in ("libegl.dll", "libglesv2.dll"))
        self.assertEqual((len(egl["exports"]), len(gles["exports"])), (65, 681))
        self.assertEqual(len(report["publicEglImportBridges"]), 65)
        self.assertTrue(all(b["targetIsRawCode"] for b in report["publicEglImportBridges"]))
        maps = {row["name"]: row["targetCodeRva"] for row in gles["focused"]["manuallySelectedMapEntries"]}
        self.assertEqual(maps, {"glBindTexture": 0x10820, "glDrawArrays": 0x13a30, "glDrawElements": 0x13b50,
                                "glEGLImageTargetTexture2DOES": 0x20350, "eglCreateWindowSurface": 0x8a50,
                                "eglGetProcAddress": 0x9e20, "eglMakeCurrent": 0xa370, "eglSwapBuffers": 0xbe50})
        for item in gles["exports"]:
            if item["name"] in maps:
                self.assertEqual(item["entryJump"]["directTargetRva"], maps[item["name"]])
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), before)

    @unittest.skipUnless(os.environ.get("BSI_PLAYER_PF_ZIP") and shutil.which("objdump"), "Explicit PF ZIP and objdump required")
    def test_original_selected_instructions_against_independent_objdump(self):
        path = Path(os.environ["BSI_PLAYER_PF_ZIP"])
        report = audit.inspect(path)
        _, members = audit.bundle(audit.qt.safe_path(path))
        totals = []
        for key, item in report["sidecars"].items():
            wanted = {}
            for exported in item["exports"]:
                if key == "libegl.dll" or exported["name"] in audit.MAP_WITNESSES:
                    ins = exported["entryJump"]
                    if ins:
                        wanted[ins["rva"]] = ins
            for row in item["focused"]["manuallySelectedMapEntries"]:
                wanted.update((i["rva"], i) for i in row["instructions"])
            wanted.update((i["rva"], i) for i in item["focused"]["manuallySelectedLookupInstructions"])
            copy = self.folder / f"independent-{key}"; copy.write_bytes(members[key][1])
            chunks = {tuple(ins["unwindChunk"]) if ins["unwindChunk"] else (rva, rva + len(bytes.fromhex(ins["bytes"]))) for rva, ins in wanted.items()}
            decoded = {}; base = item["imageBase"]
            for start, end in sorted(chunks):
                result = subprocess.run([shutil.which("objdump"), "-d", "-M", "intel", "--insn-width=16",
                                         f"--start-address={base + start:#x}", f"--stop-address={base + end:#x}", str(copy)],
                                        check=True, capture_output=True, text=True, timeout=30)
                for line in result.stdout.splitlines():
                    match = re.match(r"\s*([0-9a-f]+):\t([0-9a-f ]+)\t(.*)$", line)
                    if not match or (rva := int(match[1], 16) - base) not in wanted:
                        continue
                    assembly = re.sub(r"^rex(?:\.[A-Z]+)?\s+", "", match[3].strip())
                    decoded[rva] = (bytes.fromhex(match[2]).hex(), assembly)
            self.assertEqual(set(wanted), set(decoded))
            for rva, ins in wanted.items():
                raw, assembly = decoded[rva]
                self.assertEqual((raw, assembly.split()[0]), (ins["bytes"], ins["mnemonic"]), hex(rva))
                if ins["ripOperandTargetRvas"]:
                    comment = re.search(r"#\s*(?:0x)?([0-9a-f]+)", assembly)
                    self.assertIsNotNone(comment, hex(rva))
                    self.assertEqual([int(comment[1], 16) - base], ins["ripOperandTargetRvas"], hex(rva))
                if "directTargetRva" in ins:
                    target = re.search(r"(?:call|jmp)\s+0x([0-9a-f]+)", assembly)
                    self.assertIsNotNone(target, hex(rva))
                    self.assertEqual(int(target[1], 16) - base, ins["directTargetRva"], hex(rva))
            self.assertEqual(hashlib.sha256(copy.read_bytes()).hexdigest(), item["identity"]["sha256"])
            totals.append(len(wanted))
        self.assertEqual(sorted(totals), [65, 74])  # 139 distinct selected instructions, not complete functions.


if __name__ == "__main__":
    unittest.main()
