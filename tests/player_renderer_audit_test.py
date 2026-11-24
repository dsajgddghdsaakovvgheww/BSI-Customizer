"""Static renderer-audit tests. All PE instruction fixtures are synthetic DATA.

No PE, DLL, imported function or private interface is ever executed.
"""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

from emulator_qt_report_test import fixture as qt_fixture

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("player_renderer_audit", ROOT / "scripts/Inspect-PlayerRenderer.py")
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


def fixture(*, strings_only=False, compressed=False, clobber_result=False, fake_instruction=False, register=True):
    data = bytearray(qt_fixture())
    data[0x1000:0x2000] = b"\xcc" * 0x1000
    data[0x2000:0x7000] = b"\0" * 0x5000
    pool, slots = 0x3000, {}

    def append(value):
        nonlocal pool
        at = pool
        data[at:at + len(value)] = value
        pool += len(value)
        return at

    modules = (("KERNEL32.dll", ("LoadLibraryW", "GetProcAddress")),
               ("Qt5Qml.dll", ("?load@QQmlApplicationEngine@@QEAAXAEBVQUrl@@@Z",)),
               ("Qt5Core.dll", ("?qRegisterResourceData@@YA_NHPEBE00@Z",)))
    for index, (module, symbols) in enumerate(modules):
        name = append(module.encode() + b"\0")
        pool = (pool + 7) & ~7
        table = pool
        iat = table + 8 * (len(symbols) + 1)
        pool = iat + 8 * (len(symbols) + 1)
        for n, symbol in enumerate(symbols):
            at = append(b"\0\0" + symbol.encode() + b"\0")
            struct.pack_into("<Q", data, table + n * 8, at)
            struct.pack_into("<Q", data, iat + n * 8, at)
            slots[symbol] = iat + n * 8
        struct.pack_into("<IIIII", data, 0x2000 + index * 20, table, 0, 0, name, iat)
    struct.pack_into("<II", data, 0x110, 0x2000, 80)
    strings, cursor = {}, 0x4000
    for text in audit.LITERALS:
        strings[text] = cursor
        encoded = text.encode() + b"\0"
        data[cursor:cursor + len(encoded)] = encoded
        cursor += len(encoded)

    ranges = []

    def emit(begin, content, unwind=True):
        data[begin:begin + len(content)] = content
        if unwind:
            ranges.append((begin, begin + len(content)))

    def lea(start, target, prefix=b"\x48\x8d\x15"):
        return prefix + struct.pack("<i", target - start - 7)

    def call(start, target):
        return b"\xe8" + struct.pack("<i", target - start - 5)

    def imported(start, slot, opcode=b"\xff\x15"):
        return opcode + struct.pack("<i", slot - start - 6)

    if strings_only:
        emit(0x1000, b"\xc3")
    elif fake_instruction:
        # A byte-pattern LEA inside MOV's immediate must NOT count as an
        # instruction-boundary-validated reference to the grApiInit literal.
        fake = lea(0x1002, strings["grApiInit"])
        emit(0x1000, b"\x48\xb8" + fake + b"\0\xc3")
    else:
        code = lea(0x1000, strings["Initializing ui ..."], b"\x48\x8d\x05")
        for target in (0x1100, 0x1600, 0x1200):
            code += call(0x1000 + len(code), target)
        emit(0x1000, code + b"\xc3")
        emit(0x1100, imported(0x1100, slots["?load@QQmlApplicationEngine@@QEAAXAEBVQUrl@@@Z"]) + b"\xc3")
        code = lea(0x1200, strings["HD-Opengl-Native"])
        code += lea(0x1200 + len(code), strings["libOpenglRender"])
        code += lea(0x1200 + len(code), strings["grApiInit"])
        for target in (0x1300, 0x1320):
            code += call(0x1200 + len(code), target)
        code += b"\x48\x8b\xd8"  # mov rbx,rax
        if clobber_result:
            code += b"\x31\xdb"  # xor ebx,ebx also kills the RBX pointer
        emit(0x1200, code + b"\xff\xd3\xc3")
        emit(0x1300, imported(0x1300, slots["LoadLibraryW"]) + b"\xc3")
        emit(0x1320, imported(0x1320, slots["GetProcAddress"]) + b"\xc3")
        emit(0x1600, lea(0x1600, 0x1700, b"\x48\x8d\x05") + b"\x48\x89\x42\x10\xc3", unwind=False)
        emit(0x1700, lea(0x1700, strings["imgdOnEglCreateContext"]) + b"\xc3")
    if register:
        slot = slots["?qRegisterResourceData@@YA_NHPEBE00@Z"]
        emit(0x1400, imported(0x1400, slot, b"\xff\x25"), unwind=False)
        for start in (0x1500, 0x1540):
            code = lea(start, 0x5800, b"\x4c\x8d\x0d")
            code += lea(start + len(code), 0x5400, b"\x4c\x8d\x05")
            code += lea(start + len(code), 0x5000)
            code += b"\xb9\x03\0\0\0"
            code += b"\xe9" + struct.pack("<i", 0x1400 - start - len(code) - 5)
            emit(start, code, unwind=False)
    struct.pack_into("<II", data, 0x120, 0x7000, len(ranges) * 12)
    for n, (begin, end) in enumerate(sorted(ranges)):
        struct.pack_into("<III", data, 0x7000 + n * 12, begin, end, 0x7300)
    data[0x7300:0x7304] = b"\x01\0\0\0"
    struct.pack_into(">IHII", data, 0x5000, 0, 2, 1, 1)
    struct.pack_into(">IH", data, 0x5016, 0, 1 if compressed else 0)
    struct.pack_into(">I", data, 0x5020, 0)
    name, h = "plr.qml", 0
    for ch in name:
        h = (h << 4) + ord(ch)
        h ^= (h & 0xF0000000) >> 23
        h &= 0x0FFFFFFF
    struct.pack_into(">HI", data, 0x5400, len(name), h)
    data[0x5406:0x5414] = name.encode("utf-16be")
    source = (b"import QtQuick 2.15\nApplicationWindow {\n"
              b"    Component.onCompleted: { plrBackend.setBackendUiParams(1) }\n}\n")
    payload = source
    if compressed:
        import zlib
        payload = struct.pack(">I", len(source)) + zlib.compress(source)
    struct.pack_into(">I", data, 0x5800, len(payload))
    data[0x5804:0x5804 + len(payload)] = payload
    return bytes(data)


class RendererAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bsi-renderer-audit-")
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "HD-Player.exe"

    def report(self, data):
        self.path.write_bytes(data)
        before = hashlib.sha256(self.path.read_bytes()).hexdigest()
        result = audit.inspect(self.path)
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).hexdigest(), before)
        self.assertFalse(result["playerOrDllExecuted"] or result["targetModified"]
                         or result["privateRendererAbiCalled"] or result["runtimeRendererOrGameRouteProved"])
        return result

    def test_decoded_native_loader_lookup_and_saved_pointer_call(self):
        report = self.report(fixture())
        boundary, = report["rendererBoundaries"]
        self.assertEqual(boundary["unwindRangeBegin"], 0x1200)
        self.assertEqual({x["api"]["symbol"] for x in boundary["directHelperImportWitnesses"]},
                         {"LoadLibraryW", "GetProcAddress"})
        self.assertEqual(len(boundary["resolvedPointerCallWitnesses"]), 1)
        self.assertFalse(boundary["privateAbiInferred"])
        startup, = report["startupInstructionEvidence"]
        self.assertLess(startup["uiInitCallRva"], startup["graphicsInitCallRva"])
        self.assertEqual(startup["qmlEngineLoadCalls"][0]["instructionRva"], 0x1100)

    def test_partial_register_write_revokes_return_pointer_evidence(self):
        report = self.report(fixture(clobber_result=True))
        self.assertEqual(report["rendererBoundaries"][0]["resolvedPointerCallWitnesses"], [])

    def test_named_strings_without_code_are_not_loader_evidence(self):
        report = self.report(fixture(strings_only=True))
        self.assertTrue(report["exactAsciiAnchorEvidence"]["grApiInit"]["literalRvas"])
        self.assertEqual(report["rendererBoundaries"], [])
        self.assertEqual(report["startupInstructionEvidence"], [])

    def test_pattern_inside_an_immediate_is_not_a_decoded_reference(self):
        report = self.report(fixture(fake_instruction=True))
        self.assertEqual(report["exactAsciiAnchorEvidence"]["grApiInit"]["decodedCodeReferences"], [])
        self.assertEqual(report["rendererBoundaries"], [])

    def test_leaf_callback_builder_observed_at_native_call_target(self):
        report = self.report(fixture())
        builder, = report["nativeCallbackBuilderEvidence"]
        self.assertEqual(builder["builderRva"], 0x1600)
        store, = builder["adjacentCodePointerStores"]
        self.assertEqual(store["targetCodeRva"], 0x1700)
        self.assertEqual(store["destinationOffset"], 0x10)
        self.assertEqual(store["namedContextCallback"], "imgdOnEglCreateContext")

    def test_registered_connected_resource_not_repeated_registration_count(self):
        report = self.report(fixture())
        resource = report["registeredQml"]
        self.assertEqual(resource["uniquePoolCount"], 1)
        self.assertEqual(resource["connectedFileEntryCount"], 1)
        source, = resource["selectedSources"]
        self.assertEqual(source["path"], "/plr.qml")
        self.assertEqual(len(source["registrationCallRvas"]), 2)
        self.assertEqual(source["rootCompletionLines"], [3])
        self.assertTrue(source["setBackendUiParamsPresent"])
        self.assertTrue(resource["sourcePresenceIsNotCompiledCacheActivationProof"])

    def test_compressed_source_matches_raw_full_hash(self):
        a = self.report(fixture())["registeredQml"]["selectedSources"][0]
        b = self.report(fixture(compressed=True))["registeredQml"]["selectedSources"][0]
        self.assertEqual((a["sha256"], a["decodedBytes"]), (b["sha256"], b["decodedBytes"]))

    def test_unregistered_resource_bytes_do_not_gain_provenance(self):
        report = self.report(fixture(register=False))
        self.assertEqual(report["registeredQml"]["uniquePoolCount"], 0)
        self.assertEqual(report["registeredQml"]["selectedSources"], [])

    def test_corrupt_name_hash_cycles_and_unmapped_payload_refuse(self):
        for offset, fmt, value in ((0x5402, ">I", 0), (0x500A, ">I", 0), (0x5020, ">I", 0xFFFFFFFF)):
            with self.subTest(offset=offset):
                data = bytearray(fixture())
                struct.pack_into(fmt, data, offset, value)
                self.path.write_bytes(data)
                with self.assertRaises(audit.AuditError):
                    audit.inspect(self.path)

    def test_compression_bound_and_trailing_bytes_refuse(self):
        for mode in ("oversize", "trailing"):
            with self.subTest(mode=mode):
                data = bytearray(fixture(compressed=True))
                if mode == "oversize":
                    struct.pack_into(">I", data, 0x5804, audit.MAX_TEXT + 1)
                else:
                    length = struct.unpack_from(">I", data, 0x5800)[0]
                    struct.pack_into(">I", data, 0x5800, length + 1)
                    data[0x5804 + length] = 0
                self.path.write_bytes(data)
                with self.assertRaises(audit.AuditError):
                    audit.inspect(self.path)

    def test_virtual_zero_fill_is_not_raw_data(self):
        image = audit.Image(fixture())
        self.addCleanup(image.pe.close)
        image.sections[-1].Misc_VirtualSize = 0x4000
        with self.assertRaises(audit.AuditError):
            image.raw(0xA000, 1)

    def test_overlapping_unwind_ranges_refuse(self):
        data = bytearray(fixture())
        struct.pack_into("<I", data, 0x700C, 0x1001)
        self.path.write_bytes(data)
        with self.assertRaisesRegex(audit.AuditError, "Overlapping"):
            audit.inspect(self.path)

    def test_zip_reads_one_member_without_extraction(self):
        path = self.path.with_suffix(".zip")
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
            archive.writestr("../../HD-Player.exe", fixture())
        before = set(path.parent.iterdir())
        report = audit.inspect(path)
        self.assertEqual(len(report["rendererBoundaries"]), 1)
        self.assertEqual(set(path.parent.iterdir()), before)
        self.assertTrue(report["archiveSha256"])

    @unittest.skipUnless(os.environ.get("BSI_RENDERER_REFERENCE_DIR"), "Optional four pinned EXEs, never executed")
    def test_four_pinned_samples(self):
        folder = Path(os.environ["BSI_RENDERER_REFERENCE_DIR"])
        manifest = json.loads((ROOT / "tests/fixtures/emulator-qt-evidence.json").read_text())
        evidence = json.loads((ROOT / "docs/evidence/potato-hd-player-native-renderer.json").read_text())
        golden = {report["image"]["sha256"]: report for report in evidence["reports"]}
        for n, expected in enumerate(manifest["samples"], 1):
            with self.subTest(version=expected["playerFileVersion"]):
                path = folder / expected["input"]
                if not path.exists():
                    path = folder / f"sample-{n}" / "HD-Player.exe"
                before = hashlib.sha256(path.read_bytes()).hexdigest()
                report = audit.inspect(path)
                self.assertEqual(report["image"]["sha256"], expected["playerSha256"])
                self.assertEqual(report["image"]["fileVersion"], expected["playerFileVersion"])
                # Either the original input ZIP/name or a verified standalone
                # EXE can be supplied; PE-derived evidence must match exactly.
                self.assertEqual({key: value for key, value in report.items() if key not in ("input", "archiveSha256")},
                                 {key: value for key, value in golden[expected["playerSha256"]].items()
                                  if key not in ("input", "archiveSha256")})
                self.assertEqual(len(report["rendererBoundaries"]), 1)
                boundary = report["rendererBoundaries"][0]
                self.assertTrue(all(boundary["moduleOperandReferences"].values()))
                self.assertEqual({x["api"]["symbol"] for x in boundary["directHelperImportWitnesses"]},
                                 {"LoadLibraryW", "GetProcAddress"})
                self.assertEqual(len(boundary["resolvedPointerCallWitnesses"]), 1)
                self.assertEqual(len(report["startupInstructionEvidence"]), 1)
                self.assertEqual(len(report["nativeCallbackBuilderEvidence"]), 2)
                self.assertEqual(report["registeredQml"]["uniquePoolCount"], 3)
                self.assertEqual(len(report["registeredQml"]["selectedSources"]), 2)
                self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), before)
                self.assertFalse(report["playerOrDllExecuted"] or report["privateRendererAbiCalled"])


if __name__ == "__main__":
    unittest.main()
