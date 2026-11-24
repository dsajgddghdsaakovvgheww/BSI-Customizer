#!/usr/bin/env python3
"""Bounded STATIC inventory of explicitly supplied x64 graphics-provider DLLs.

Requires pefile==2024.8.26 and capstone==5.0.6, plus the sibling read-only
Inspect-PlayerRenderer.py / Inspect-EmulatorQt.py utilities. Accept DLLs or a
ZIP containing exactly HD-Opengl-Native.dll and libOpenglRender.dll as DATA.
No network, installation discovery, adjacent-file traversal, extraction, PE
loading/execution, process attachment, private ABI call or patching occurs.

Focused instruction addresses are manually selected research witnesses for TWO
exact hashes, not signatures/offsets used by the product. Unknown hashes receive
inventory/export/anchor evidence only. Neither output proves a live renderer,
GL resource ownership, shader compatibility, the complete CFG or a stable ABI.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
from io import BytesIO
import json
from pathlib import Path
import re
import stat
import struct
import zipfile

SPEC = importlib.util.spec_from_file_location("bsi_provider_pe", Path(__file__).with_name("Inspect-PlayerRenderer.py"))
native = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(native)
qt, AuditError, require = native.qt, native.AuditError, native.require
MEM, REG = native.X86_OP_MEM, native.X86_OP_REG
NAMES = {"hd-opengl-native.dll", "libopenglrender.dll"}
ANCHORS = (
    "grApiInit", "opengl32", "opengl32.dll", "libGLESv2", "libGLESv2.dll", "libEGL.dll",
    "wglGetProcAddress", "wglMakeCurrent", "eglGetProcAddress", "eglMakeCurrent",
    "glDrawArrays", "glDrawElements", "glBindTexture", "glTexImage2D",
    "glCopyTexSubImage2D", "glBlitFramebuffer", "eglCreateImageKHR", "eglDestroyImageKHR",
    "glEGLImageTargetTexture2DOES", "pga_glEGLImageTargetTexture2DOES",
    "PgaCtlPostFramebuffer", "PgaEglMakeCurrent", "PgaEglSwapBuffers",
)
# Addresses here are HUMAN-SEEDED witnesses, enabled only by complete-file SHA256.
# A range in .pdata is an unwind chunk, NOT necessarily a complete logical function.
FOCUSED = {
    "8af770ea79f47551b2f8ab88acdf568745046ff08b47b88860a06965de34fe5d": {
        "tableHelpers": [],
        "leafRanges": [(0x88c50, 0x88c5a)],
        "instructions": {
            "slot-zero-window-argument-and-native-worker": [0xa6354, 0xa64e4, 0xa64f9, 0xa650a],
            "backend-bit-test": [0x88c50, 0x88c56],
            "internal-or-external-egl-lookup": [0x8cbea, 0x8cbf1, 0x8cbf8, 0x8cbff, 0x8cc06],
            "core-gl-name-and-destination-slots": [0x92da8, 0x92daf, 0x92db6, 0x92dbb, 0x92dc2, 0x92dc9, 0x9322f, 0x93236, 0x9323d],
            "core-gl-loader": [0x92627, 0x92633, 0x9263c, 0x926b9, 0x926be, 0x926c5, 0x926ca, 0xbf000, 0xbf002, 0xbf005, 0xbf00b],
            "elements-call-through-resolved-slot": [0x999eb, 0x999f2, 0x999f5, 0x999f8, 0x999fa],
            "private-image-type-and-host-texture-alias": [0x9a7c3, 0x9a7c6, 0x9a7d1, 0x9a879, 0x9a98d, 0x9a994, 0x9a99d, 0x9a9a0, 0x9a9b2],
            "post-framebuffer-linear-input": [0x88075, 0x8807f, 0x88085, 0x8808a, 0x88094, 0x8809a, 0x881da],
            "dummy-window-bootstrap": [0xb45dd, 0xb45e8, 0xb45f4, 0xb4628, 0xb54dc, 0xb54e3, 0xb54ea],
            "native-current-and-native-present": [0xb5ba6, 0xb5bb2, 0xb5bc0, 0xb5bd4, 0xb5db4, 0xb5db8],
        },
        "tables": [],
    },
    "d4b50c270d1f7ed1afed4fbc4e963177f747b0ee1c155a2e7112c59924212afc": {
        "tableHelpers": [0x325e20],
        "leafRanges": [(0x113660, 0x1136ea), (0x2de780, 0x2de80a), (0x1d32d0, 0x1d32e7)],
        "instructions": {
            "slot-zero-retains-native-window": [0x130e71, 0x131096, 0x1310a0, 0x131109],
            "callback-value-copies": [0x113665, 0x113668, 0x1136db, 0x1136e1, 0x2de785, 0x2de788, 0x2de7fb, 0x2de801],
            "internal-egl-dispatch-and-image-resolution": [0x197f17, 0x197f1e, 0x19822d, 0x198234, 0x198239, 0x19825a, 0x198261, 0x19826d, 0x198274, 0x198279, 0x19829a, 0x1982a1],
            "internal-egl-resolver-chunks": [0x2dc926, 0x2dc92c, 0x2dc936, 0x2dc94c, 0x2dc95b, 0x2dc962, 0x2dc98d, 0x2dc9cb],
            "image-client-uses-texture-target-and-retains-images": [0xfa513, 0xfa51a, 0xfa529, 0xfa52c, 0xfa539, 0xfa53b, 0xfa548, 0xfa561, 0xfa56e, 0xfa570, 0xf98ec, 0xf98ff],
            "internal-image-validation-backing-and-retirement": [0x2d9416, 0x2d94a2, 0x2d94dc, 0x2d957e, 0x2d9585, 0x2d95ae, 0x2d9819, 0x2d9842, 0x2d984f, 0x2d9856, 0x2d9860],
            "cached-host-gl-dispatch-data-not-code-export": [0x1f31f4, 0x1f31fb, 0x1f32ac],
            "wgl-library-and-resolver-initialization": [0x2f091d, 0x2f0927, 0x2f092e, 0x2f0953, 0x2f096b, 0x2f0970, 0x2f09f7, 0x2ee9dd, 0x2ee9ea, 0x2ee9ef, 0x1d32dd],
            "alternative-host-egl-procedure-cache": [0x2f0d3c, 0x2f0d43, 0x2f0d48, 0x2f0dca, 0x2f0dd4, 0x2f0de1, 0x2f0de8],
        },
        "tables": [{"rva": 0x50cbe0, "entries": 13, "label": "internal-egl-extension-name-to-code-pairs"}],
    },
}


def inputs(path):
    data = qt.read(path)
    if path.suffix.lower() != ".zip":
        return None, [(path.name, data)]
    require(len(data) <= 32 * 1024 * 1024, "Provider ZIP exceeds 32 MiB")
    selected, total = {}, 0
    with zipfile.ZipFile(BytesIO(data)) as archive:
        require(len(archive.infolist()) <= 64, "Too many provider ZIP entries")
        for entry in archive.infolist():
            require(not entry.flag_bits & 1 and not stat.S_ISLNK(entry.external_attr >> 16), "Encrypted/linked provider ZIP entry")
            if entry.is_dir():
                continue
            name = entry.filename.replace("\\", "/").split("/")[-1].lower()
            require(name in NAMES and name not in selected, "Provider ZIP must contain the two unique expected DLL basenames only")
            total += entry.file_size
            require(0 < entry.file_size <= qt.LIMIT and total <= 2 * qt.LIMIT, "Oversized provider ZIP member")
            with archive.open(entry) as stream:
                member = stream.read(qt.LIMIT + 1)  # ZIP CRC checked; no archive path is extracted.
            require(len(member) == entry.file_size, "Truncated/oversized provider ZIP member")
            selected[name] = (entry.filename, member)
    require(set(selected) == NAMES, "Provider ZIP must contain both expected DLLs")
    return hashlib.sha256(data).hexdigest(), [selected[name] for name in sorted(selected)]


def section_kind(image, rva):
    for section in image.sections:
        delta = rva - section.VirtualAddress
        if 0 <= delta < max(section.Misc_VirtualSize, section.SizeOfRawData):
            backed = delta < section.SizeOfRawData and section.PointerToRawData + delta < len(image.data)
            return {"section": section.Name.rstrip(b"\0").decode("ascii"), "rawFileBacked": backed,
                    "executable": bool(section.Characteristics & 0x20000000),
                    "zeroFillIsNotFileBytes": not backed}
    return {"section": None, "rawFileBacked": False, "executable": False, "zeroFillIsNotFileBytes": False}


def instruction(image, ins):
    result = {"rva": ins.address - image.base, "bytes": ins.bytes.hex(), "mnemonic": ins.mnemonic, "operands": ins.op_str}
    result["ripOperandTargetRvas"] = [r for op in ins.operands if (r := image.rip_target(ins, op)) is not None]
    if api := image.import_at_call(ins):
        result["import"] = api
    if ins.mnemonic in ("call", "jmp") and len(ins.operands) == 1 and ins.operands[0].type == native.X86_OP_IMM:
        result["directTargetRva"] = ins.operands[0].imm - image.base
    scope = image.owner(result["rva"])
    result["unwindChunk"] = list(scope) if scope else None
    return result


def pointer_stores(image, entry):
    """Only decoded ADJACENT LEA-code / MOV-memory pairs, not an ABI inference."""
    rows = []
    scope = image.owner(entry)
    require(not scope or scope[0] == entry, "Initializer/helper must begin at its unwind chunk")
    code = image.decode(entry)
    if not scope:
        # A leaf tail jump ends this observed entry; do not attribute the next
        # function's stores to an initializer which merely jumps elsewhere.
        for index, ins in enumerate(code):
            if ins.mnemonic == "jmp" or ins.mnemonic.startswith("ret"):
                code = code[:index + 1]
                break
    for left, right in zip(code, code[1:]):
        if left.mnemonic != "lea" or right.mnemonic != "mov" or len(left.operands) != 2 or len(right.operands) != 2:
            continue
        destination, value = right.operands
        if (left.operands[0].type != REG or destination.type != MEM or value.type != REG
                or left.operands[0].reg != value.reg or destination.size != 8
                or not destination.mem.base or destination.mem.base == native.X86_REG_RIP or destination.mem.index):
            continue
        target = image.rip_target(left, left.operands[1])
        if target is None or not section_kind(image, target)["executable"]:
            continue
        image.raw(target, 1, execute=True)
        require(len(rows) < 256, "Too many adjacent code-pointer stores")
        rows.append({"destinationRegister": right.reg_name(destination.mem.base), "destinationOffset": destination.mem.disp,
                     "targetCodeRva": target, "lea": instruction(image, left), "store": instruction(image, right)})
    return rows


def anchors(image):
    found, targets = {}, {}
    for text in ANCHORS:
        values, at = [], 0
        needle = text.encode("ascii") + b"\0"
        while (at := image.data.find(needle, at)) >= 0:
            require(len(values) < 64, "Too many repeated provider anchors")
            rva = image.pe.get_rva_from_offset(at)
            image.raw(rva, len(needle))
            values.append(rva)
            targets[rva] = text
            at += len(needle)
        found[text] = {"literalRvas": values, "decodedReferences": []}
    scopes = set()
    # Candidate byte matches are credited only at instruction starts decoded
    # from a validated bounded unwind chunk. Not an exhaustive xref/CFG scanner.
    pattern = rb"(?=([\x48\x4c][\x8d\x8b][\x05\x0d\x15\x1d\x25\x2d\x35\x3d].{4}))"
    for begin, data in image.code_sections():
        for match in re.finditer(pattern, data, re.S):
            rva = begin + match.start()
            target = rva + 7 + struct.unpack_from("<i", match[1], 3)[0]
            if target in targets and (scope := image.owner(rva)):
                scopes.add(scope)
    require(len(scopes) <= 512, "Too many anchor-reference unwind chunks")
    for begin, _ in sorted(scopes):
        for ins in image.decode(begin):
            for op in ins.operands:
                if (target := image.rip_target(ins, op)) in targets:
                    found[targets[target]]["decodedReferences"].append(instruction(image, ins))
    return found


def focused(image, selected, exports):
    if not selected:
        return {"knownHashMatched": False, "manuallySeededInstructions": {}, "extraTableStoreBuilders": [], "nameCodeTables": []}
    result = {"knownHashMatched": True, "manuallySeededInstructions": {}, "extraTableStoreBuilders": [], "nameCodeTables": []}
    # Known leaf helpers must actually be direct calls of this exported initializer.
    direct = {target for _, target in image.direct_calls(image.decode(exports["grApiInit"]["rva"]))}
    for entry in selected["tableHelpers"]:
        require(entry in direct, "Focused table helper is not an initializer direct call")
        result["extraTableStoreBuilders"].append({"entryRva": entry, "adjacentStores": pointer_stores(image, entry)})
    for label, addresses in selected["instructions"].items():
        rows = []
        for address in addresses:
            scope = image.owner(address)
            if scope:
                decoded = image.decode(scope[0])
            else:
                # These leaf starts are human-seeded for the exact whole-file
                # hash too. Decode FROM that start, never from an interior byte.
                leaves = [leaf for leaf in selected["leafRanges"] if leaf[0] <= address < leaf[1]]
                require(len(leaves) == 1, "Missing/ambiguous focused leaf range")
                decoded = [ins for ins in image.decode(leaves[0][0]) if ins.address - image.base < leaves[0][1]]
            ins = next((ins for ins in decoded if ins.address - image.base == address), None)
            require(ins is not None, f"Focused address {address:x} is not a decoded instruction boundary")
            row = instruction(image, ins)
            if not scope:
                row["manuallySeededLeafRange"] = list(leaves[0])
            rows.append(row)
        result["manuallySeededInstructions"][label] = rows
    for table in selected["tables"]:
        rows = []
        require(0 < table["entries"] <= 256, "Oversized name/code table")
        for index in range(table["entries"]):
            at = table["rva"] + index * 16
            name, target = struct.unpack_from("<QQ", image.data, image.raw(at, 16))
            name -= image.base
            target -= image.base
            text = bytearray()
            for byte in range(256):
                value = image.data[image.raw(name + byte)]
                if not value:
                    break
                require(32 <= value < 127, "Non-ASCII focused table name")
                text.append(value)
            else:
                raise AuditError("Unterminated focused table name")
            image.raw(target, 1, execute=True)
            rows.append({"pairRva": at, "nameRva": name, "name": text.decode("ascii"), "targetCodeRva": target})
        result["nameCodeTables"].append({**table, "pairs": rows})
    return result


def provider(data, name):
    image = native.Image(data)
    try:
        require(image.metadata["dll"], "Expected a DLL, not a player EXE")
        image.pe.parse_data_directories(directories=[0, 2])
        version_strings = []
        for group in getattr(image.pe, "FileInfo", []):
            for block in group:
                for table in getattr(block, "StringTable", []):
                    require(len(version_strings) < 16 and len(table.entries) <= 64, "Too many version string records")
                    require(all(len(key) <= 128 and len(value) <= 4096 for key, value in table.entries.items()), "Oversized version string")
                    version_strings.append({key.decode("utf-8", errors="replace"): value.decode("utf-8", errors="replace")
                                            for key, value in table.entries.items()})
        symbols = getattr(getattr(image.pe, "DIRECTORY_ENTRY_EXPORT", None), "symbols", [])
        require(len(symbols) <= 4096, "Too many provider exports")
        rows, named = [], {}
        for symbol in symbols:
            text = symbol.name.decode("ascii") if symbol.name else None
            require(text is None or 0 < len(text) <= 2048 and all(32 <= ord(c) < 127 for c in text), "Invalid export name")
            row = {"name": text, "ordinal": symbol.ordinal, "rva": symbol.address,
                   "forwarder": symbol.forwarder.decode("ascii") if symbol.forwarder else None,
                   **section_kind(image, symbol.address)}
            if text:
                require(text not in named, "Duplicate provider export name")
                named[text] = row
            rows.append(row)
        entry = named.get("grApiInit")
        require(entry and not entry["forwarder"] and entry["executable"] and entry["rawFileBacked"], "Missing/non-code/forwarded grApiInit export")
        stores = pointer_stores(image, entry["rva"])
        metadata = {key: value for key, value in image.metadata.items() if key != "sections"}
        return {"inputMember": name, "identity": metadata, "versionStringTables": version_strings, "imageBase": image.base,
                "authenticode": {"certificateDirectoryPresent": bool(image.pe.OPTIONAL_HEADER.DATA_DIRECTORY[4].Size), "signatureValidated": False},
                "exports": rows, "publicEglExports": [row for row in rows if (row["name"] or "").startswith("egl")],
                "imports": [{"iatRva": rva, **value} for rva, value in sorted(image.imports.items())],
                "grApiInit": {"rva": entry["rva"], "adjacentCodePointerStores": stores,
                              "completePrivateAbiInferred": False},
                "exactAsciiAnchors": anchors(image), "focused": focused(image, FOCUSED.get(metadata["sha256"]), named),
                "dllLoadedOrExecuted": False, "targetModified": False, "privateInterfaceCalled": False,
                "liveRendererOrResourceOwnershipProved": False}
    finally:
        image.pe.close()


def inspect(path):
    path = qt.safe_path(path)
    archive, members = inputs(path)
    return {"input": path.name, "archiveSha256": archive, "providers": [provider(data, name) for name, data in members],
            "archivePathsExtracted": False, "completeControlFlowOrStableAbiProved": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="+", type=Path, help="Explicit provider DLLs or the two-DLL ZIP; never an install directory")
    args = parser.parse_args()
    reports, failed = [], False
    for path in args.inputs:
        try:
            reports.append(inspect(path))
        except (AuditError, OSError, RuntimeError, ValueError, zipfile.BadZipFile, struct.error) as error:
            reports.append({"input": path.name, "error": str(error), "dllLoadedOrExecuted": False, "targetModified": False})
            failed = True
    print(json.dumps({"schema": "bsi.renderer-provider-static-audit.v1", "reports": reports}, indent=2))
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
