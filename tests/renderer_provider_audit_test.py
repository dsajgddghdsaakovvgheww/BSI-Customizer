"""Read-only provider research tests; synthetic PEs are NEVER executed.

Set BSI_RENDERER_PROVIDER_ZIP to explicitly validate the supplied original pair.
That optional test is static byte evidence, not installed-player acceptance.
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
import zipfile

from emulator_qt_report_test import fixture as qt_fixture

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("provider_audit", ROOT / "scripts/Inspect-RendererProviders.py")
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


def fixture(*, dll=True, fake_reference=False, noncode_store=False, clobber=False, forwarder=False, zero_fill_export=False):
    data = bytearray(qt_fixture(dll=True, release="5.12.120.6303"))
    struct.pack_into("<H", data, 0x96, 0x2022 if dll else 0x22)
    data[0x1000:0x2000] = b"\xcc" * 0x1000
    data[0x3000:0x300f] = b"glDrawElements\0"
    struct.pack_into("<II", data, 0x108, 0x2800, 0x100)
    struct.pack_into("<IIHHIIIIIII", data, 0x2800, 0, 0, 0, 0, 0x2940, 1, 2, 2, 0x2840, 0x2850, 0x2860)
    struct.pack_into("<II", data, 0x2840, 0x2890 if forwarder else 0x1000, 0x7ff0)
    struct.pack_into("<II", data, 0x2850, 0x2900, 0x2920)
    struct.pack_into("<HH", data, 0x2860, 0, 1)
    data[0x2890:0x289d] = b"other.target\0"
    data[0x2900:0x290a] = b"grApiInit\0"
    name = b"?glDrawElements@GLDispatch\0"
    data[0x2920:0x2920 + len(name)] = name
    data[0x2940:0x294a] = b"TestOnly\0\0"

    def lea(start, target, prefix=b"\x48\x8d\x05"):
        return prefix + struct.pack("<i", target - start - 7)

    if fake_reference:
        # A fake LEA byte pattern hidden inside MOV's immediate is not code evidence.
        code = b"\x48\xb8" + lea(0x1002, 0x3000, b"\x48\x8d\x15") + b"\0\xc3"
    else:
        code = b"\x48\x8b\xd9"  # mov rbx,rcx
        code += lea(0x1000 + len(code), 0x3000 if noncode_store else 0x1100)
        if clobber:
            code += b"\x31\xc0"  # An intervening write breaks adjacent-store evidence.
        code += b"\x48\x89\x43\x08"
        code += lea(0x1000 + len(code), 0x3000, b"\x48\x8d\x15") + b"\xc3"
    data[0x1000:0x1000 + len(code)] = code
    data[0x1100] = 0xc3
    struct.pack_into("<II", data, 0x120, 0x6200, 24)
    struct.pack_into("<IIIIII", data, 0x6200, 0x1000, 0x1000 + len(code), 0x6300, 0x1100, 0x1101, 0x6300)
    data[0x6300:0x6304] = b"\x01\0\0\0"
    if zero_fill_export:
        # .rdata virtual tail includes the data export but has NO file backing.
        struct.pack_into("<I", data, 0x188 + 40 + 16, 0x5000)
    return bytes(data)


class ProviderAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bsi-provider-data-only-")
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.path = self.folder / "HD-Opengl-Native.dll"

    def report(self, **options):
        self.path.write_bytes(fixture(**options))
        before = hashlib.sha256(self.path.read_bytes()).hexdigest()
        result = audit.inspect(self.path)
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).hexdigest(), before)
        self.assertFalse(result["archivePathsExtracted"] or result["completeControlFlowOrStableAbiProved"])
        provider, = result["providers"]
        for flag in ("dllLoadedOrExecuted", "targetModified", "privateInterfaceCalled", "liveRendererOrResourceOwnershipProved"):
            self.assertFalse(provider[flag])
        return provider

    def archive(self, names=("HD-Opengl-Native.dll", "libOpenglRender.dll")):
        path = self.folder / "both.zip"
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
            for name in names:
                archive.writestr(name, fixture())
        return path

    def test_export_is_code_and_adjacent_store_is_not_a_complete_abi(self):
        report = self.report()
        self.assertEqual(report["grApiInit"]["rva"], 0x1000)
        store, = report["grApiInit"]["adjacentCodePointerStores"]
        self.assertEqual((store["destinationRegister"], store["destinationOffset"], store["targetCodeRva"]), ("rbx", 8, 0x1100))
        self.assertFalse(report["grApiInit"]["completePrivateAbiInferred"])
        self.assertFalse(report["focused"]["knownHashMatched"])

    def test_unknown_hash_cannot_gain_manually_seeded_offsets(self):
        focused = self.report()["focused"]
        self.assertEqual(focused["manuallySeededInstructions"], {})
        self.assertEqual(focused["nameCodeTables"], [])
        self.assertEqual(focused["extraTableStoreBuilders"], [])

    def test_data_export_is_never_a_callable_gl_entry(self):
        data = self.report()["exports"][1]
        self.assertIn("GLDispatch", data["name"])
        self.assertFalse(data["executable"])
        self.assertTrue(data["rawFileBacked"])

    def test_export_virtual_zero_fill_is_not_read_as_a_file_pointer(self):
        data = self.report(zero_fill_export=True)["exports"][1]
        self.assertTrue(data["zeroFillIsNotFileBytes"])
        self.assertFalse(data["rawFileBacked"] or data["executable"])

    def test_intervening_register_write_revokes_adjacent_pointer_store(self):
        self.assertEqual(self.report(clobber=True)["grApiInit"]["adjacentCodePointerStores"], [])

    def test_data_address_is_not_a_code_pointer_store(self):
        self.assertEqual(self.report(noncode_store=True)["grApiInit"]["adjacentCodePointerStores"], [])

    def test_leaf_tail_jump_does_not_borrow_an_adjacent_functions_stores(self):
        data = bytearray(fixture())
        struct.pack_into("<II", data, 0x120, 0, 0)  # Explicitly no unwind table.
        following = bytes(data[0x1000:0x10fb])
        data[0x1000:0x1100] = b"\xe9" + struct.pack("<i", 0x1100 - 0x1005) + following
        self.path.write_bytes(data)
        report, = audit.inspect(self.path)["providers"]
        self.assertEqual(report["grApiInit"]["adjacentCodePointerStores"], [])

    def test_anchor_reference_requires_decoded_instruction_boundary(self):
        self.assertEqual(len(self.report()["exactAsciiAnchors"]["glDrawElements"]["decodedReferences"]), 1)
        self.assertEqual(self.report(fake_reference=True)["exactAsciiAnchors"]["glDrawElements"]["decodedReferences"], [])

    def test_forwarded_initializer_is_not_disassembled_as_code(self):
        with self.assertRaisesRegex(audit.AuditError, "grApiInit"):
            self.report(forwarder=True)

    def test_exe_and_x86_cannot_masquerade_as_x64_provider(self):
        with self.assertRaisesRegex(audit.AuditError, "DLL"):
            self.report(dll=False)
        data = bytearray(fixture())
        struct.pack_into("<H", data, 0x84, 0x14c)
        self.path.write_bytes(data)
        with self.assertRaisesRegex(audit.AuditError, "x64"):
            audit.inspect(self.path)

    def test_zip_members_are_read_without_path_extraction(self):
        path = self.archive(("../../HD-Opengl-Native.dll", "C:\\ignored\\libOpenglRender.dll"))
        before = set(self.folder.iterdir())
        report = audit.inspect(path)
        self.assertEqual(set(self.folder.iterdir()), before)
        self.assertEqual(len(report["providers"]), 2)
        self.assertEqual(report["archiveSha256"], hashlib.sha256(path.read_bytes()).hexdigest())
        self.assertFalse(report["archivePathsExtracted"])

    def test_zip_duplicate_missing_and_unrelated_members_refuse(self):
        for names in (("HD-Opengl-Native.dll",), ("HD-Opengl-Native.dll", "nested/hd-opengl-native.DLL"),
                      ("HD-Opengl-Native.dll", "libOpenglRender.dll", "unexpected.exe")):
            with self.subTest(names=names), self.assertRaises(audit.AuditError):
                audit.inspect(self.archive(names))

    def test_zip_link_is_refused(self):
        path = self.folder / "linked.zip"
        with zipfile.ZipFile(path, "w") as archive:
            entry = zipfile.ZipInfo("HD-Opengl-Native.dll")
            entry.create_system = 3
            entry.external_attr = 0o120777 << 16
            archive.writestr(entry, b"somewhere")
        with self.assertRaisesRegex(audit.AuditError, "linked"):
            audit.inspect(path)

    def test_linked_input_path_is_refused(self):
        self.path.write_bytes(fixture())
        link = self.folder / "link.dll"
        try:
            link.symlink_to(self.path)
        except (OSError, NotImplementedError):
            self.skipTest("Host does not permit symlink creation")
        with self.assertRaisesRegex(audit.AuditError, "Linked"):
            audit.inspect(link)

    def test_cli_failure_is_nonzero_machine_readable_and_never_executes(self):
        self.path.write_bytes(b"not a PE")
        result = subprocess.run([sys.executable, str(ROOT / "scripts/Inspect-RendererProviders.py"), str(self.path)],
                                capture_output=True, text=True, check=False, timeout=30)
        self.assertEqual(result.returncode, 1)
        report, = json.loads(result.stdout)["reports"]
        self.assertIn("error", report)
        self.assertFalse(report["dllLoadedOrExecuted"] or report["targetModified"])

    @unittest.skipUnless(os.environ.get("BSI_RENDERER_PROVIDER_ZIP"), "Explicit original provider ZIP not requested")
    def test_explicit_original_pair_static_golden(self):
        path = Path(os.environ["BSI_RENDERER_PROVIDER_ZIP"])
        before = hashlib.sha256(path.read_bytes()).hexdigest()
        report = audit.inspect(path)
        self.assertEqual(report["archiveSha256"], "9b4bd78b3f5a6e4ad545e8946f5c0e423e20635b822e7f187582fcbd6fbac25d")
        first, second = report["providers"]
        self.assertEqual([p["identity"]["sha256"] for p in (first, second)], list(audit.FOCUSED))
        self.assertEqual([len(p["exports"]) for p in (first, second)], [2, 916])
        for p, count, entry in ((first, 25, 0xa65f0), (second, 18, 0x1312d0)):
            self.assertEqual(p["identity"]["fileVersion"], "5.12.120.6303")
            self.assertEqual(p["identity"]["productVersion"], "0.0.0.0")
            self.assertEqual(p["versionStringTables"][0]["ProductVersion"], "5.12.120.6303")
            self.assertTrue(p["authenticode"]["certificateDirectoryPresent"])
            self.assertFalse(p["authenticode"]["signatureValidated"])
            self.assertEqual(p["publicEglExports"], [])
            self.assertEqual(len(p["grApiInit"]["adjacentCodePointerStores"]), count)
            self.assertEqual(p["grApiInit"]["rva"], entry)
            self.assertTrue(p["focused"]["knownHashMatched"])
            self.assertFalse(p["liveRendererOrResourceOwnershipProved"])
        helpers, = second["focused"]["extraTableStoreBuilders"]
        self.assertEqual(len(helpers["adjacentStores"]), 8)
        table, = second["focused"]["nameCodeTables"]
        self.assertEqual(table["pairs"][0]["name"], "eglCreateImageKHR")
        self.assertEqual(table["pairs"][0]["targetCodeRva"], 0x2d9350)
        self.assertEqual(table["pairs"][1]["targetCodeRva"], 0x2d9780)
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), before)

    @unittest.skipUnless(os.environ.get("BSI_RENDERER_PROVIDER_ZIP") and shutil.which("objdump"),
                         "Explicit original ZIP and independent objdump required")
    def test_explicit_original_objdump_bytes_mnemonics_and_rip_targets(self):
        path = audit.qt.safe_path(Path(os.environ["BSI_RENDERER_PROVIDER_ZIP"]))
        report = audit.inspect(path)
        _, members = audit.inputs(path)
        totals = []
        for index, (provider, (_, data)) in enumerate(zip(report["providers"], members), 1):
            self.assertTrue(provider["focused"]["knownHashMatched"])
            # Write a fixed, test-owned DATA filename for objdump; NEVER load a
            # PE or extract an untrusted archive path. Hash the copy afterward.
            copy = self.folder / f"provider-{index}.dll"
            copy.write_bytes(data)
            wanted = {}
            for group in provider["focused"]["manuallySeededInstructions"].values():
                wanted.update((ins["rva"], ins) for ins in group)
            builders = [provider["grApiInit"]["adjacentCodePointerStores"]]
            builders += [row["adjacentStores"] for row in provider["focused"]["extraTableStoreBuilders"]]
            for stores in builders:
                for pair in stores:
                    wanted.update((pair[key]["rva"], pair[key]) for key in ("lea", "store"))
            chunks = set()
            for rva, ins in wanted.items():
                if ins["unwindChunk"]:
                    chunks.add(tuple(ins["unwindChunk"]))
                else:
                    # Hash-pinned leaf starts observed as direct call targets.
                    leaves = ((0x88c50, 0x88c5a), (0x113660, 0x1136ea), (0x2de780, 0x2de80a),
                              (0x325e20, 0x325f1a), (0x1d32d0, 0x1d32e7))
                    matching = [leaf for leaf in leaves if leaf[0] <= rva < leaf[1]]
                    self.assertEqual(len(matching), 1, hex(rva))
                    chunks.add(matching[0])
            decoded = {}
            base = provider["imageBase"]
            for begin, end in sorted(chunks):
                output = subprocess.run([shutil.which("objdump"), "-d", "-M", "intel", "--insn-width=16",
                                         f"--start-address={base + begin:#x}", f"--stop-address={base + end:#x}", str(copy)],
                                        capture_output=True, text=True, check=True, timeout=30).stdout
                for line in output.splitlines():
                    match = re.match(r"\s*([0-9a-f]+):\t([0-9a-f ]+)\t(.*)$", line)
                    if not match:
                        continue
                    address = int(match[1], 16) - base
                    if address not in wanted:
                        continue
                    assembly = re.sub(r"^rex(?:\.[A-Z]+)?\s+", "", match[3].strip())
                    decoded[address] = (bytes.fromhex(match[2]).hex(), assembly.split()[0], assembly)
            self.assertEqual(set(decoded), set(wanted))
            for rva, ins in wanted.items():
                raw, mnemonic, assembly = decoded[rva]
                self.assertEqual((raw, mnemonic), (ins["bytes"], ins["mnemonic"]), hex(rva))
                if ins["ripOperandTargetRvas"]:
                    comment = re.search(r"#\s*(?:0x)?([0-9a-f]+)", assembly)
                    self.assertIsNotNone(comment, hex(rva))
                    self.assertEqual([int(comment[1], 16) - base], ins["ripOperandTargetRvas"], hex(rva))
                if "directTargetRva" in ins:
                    target = re.search(r"(?:call|jmp)\s+0x([0-9a-f]+)", assembly)
                    self.assertIsNotNone(target, hex(rva))
                    self.assertEqual(int(target[1], 16) - base, ins["directTargetRva"], hex(rva))
            self.assertEqual(hashlib.sha256(copy.read_bytes()).hexdigest(), provider["identity"]["sha256"])
            totals.append(len(wanted))
        self.assertEqual(totals, [115, 128])


if __name__ == "__main__":
    unittest.main()
