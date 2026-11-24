#!/usr/bin/env python3
"""Read-only inventory of an explicitly supplied player program-files ZIP.

Requires pefile==2024.8.26 and capstone==5.0.6 and the sibling static auditors.
No network, installation discovery, archive-path extraction, PE loading,
process attachment, private-interface invocation or target modification occurs.
Export jump/import edges are static file evidence, NOT runtime module bindings.
Human-selected ANGLE map witnesses are enabled by ONE complete-file SHA256 only.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
from io import BytesIO
import json
from pathlib import Path, PureWindowsPath
import stat
import struct
import zipfile
import zlib

SPEC = importlib.util.spec_from_file_location("bsi_pf_provider", Path(__file__).with_name("Inspect-RendererProviders.py"))
provider = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(provider)
native, qt, require, AuditError = provider.native, provider.qt, provider.require, provider.AuditError
SELECTED = {"hd-player.exe", "hd-opengl-native.dll", "libopenglrender.dll", "libegl.dll", "libglesv2.dll"}
KNOWN_PROVIDERS = {
    "hd-opengl-native.dll": "8af770ea79f47551b2f8ab88acdf568745046ff08b47b88860a06965de34fe5d",
    "libopenglrender.dll": "d4b50c270d1f7ed1afed4fbc4e963177f747b0ee1c155a2e7112c59924212afc",
}
GLES_HASH = "5b0acf994cd091c5c07d707219a33de7d5d9ce2038bf93644a7c3d8d64de48d5"
# MANUALLY SELECTED name-setup / code-pointer-LEA / adjacent-store witnesses.
# These are never used by production or applied to a different binary hash.
MAP_WITNESSES = {
    "glBindTexture": (0x1fdd, 0x2005, 0x200c),
    "glDrawArrays": (0x2e02, 0x2e20, 0x2e27),
    "glDrawElements": (0x2e34, 0x2e52, 0x2e59),
    "glEGLImageTargetTexture2DOES": (0x4b46, 0x4b68, 0x4b6f),
    "eglCreateWindowSurface": (0x670e, 0x6730, 0x6737),
    "eglGetProcAddress": (0x6906, 0x6928, 0x692f),
    "eglMakeCurrent": (0x6976, 0x6998, 0x699f),
    "eglSwapBuffers": (0x6a56, 0x6a78, 0x6a7f),
}


def bundle(path):
    """Bound names/expansion, CRC-check EVERY member; retain five explicit PEs."""
    data = qt.read(path)
    require(path.suffix.lower() == ".zip", "Expected an explicit program-files ZIP")
    rows, selected, seen = [], {}, set()
    total = 0
    with zipfile.ZipFile(BytesIO(data)) as archive:
        require(0 < len(archive.infolist()) <= 512, "Invalid/oversized program-files entry count")
        for entry in archive.infolist():
            name = entry.filename.replace("\\", "/")
            parts = name.rstrip("/").split("/")
            require(name and not name.startswith("/") and not PureWindowsPath(name).drive
                    and ":" not in name and all(part and part not in (".", "..") and part[-1] not in ". " for part in parts),
                    "Unsafe program-files member name")
            require(name.casefold() not in seen, "Duplicate/case-ambiguous program-files member")
            seen.add(name.casefold())
            mode = stat.S_IFMT(entry.external_attr >> 16)
            require(mode in (0, stat.S_IFREG, stat.S_IFDIR) and not entry.external_attr & 0x400
                    and not entry.flag_bits & 1, "Linked/special/encrypted program-files member")
            require(0 <= entry.file_size <= 64 * 1024 * 1024, "Oversized program-files member")
            total += entry.file_size
            require(total <= 256 * 1024 * 1024, "Program-files expansion exceeds audit limit")
            row = {"name": entry.filename, "bytes": entry.file_size, "compressedBytes": entry.compress_size,
                   "crc32": f"{entry.CRC:08x}", "directory": entry.is_dir()}
            with archive.open(entry) as stream:
                payload = stream.read(64 * 1024 * 1024 + 1)
            require(len(payload) == entry.file_size and (not entry.is_dir() or not payload), "Truncated/invalid program-files member")
            if not entry.is_dir():
                row["sha256"] = hashlib.sha256(payload).hexdigest()
                if payload[:2] == b"MZ":
                    row["pe"] = {key: value for key, value in qt.image(payload).items() if key != "sections"}
                key = parts[-1].lower()
                if key in SELECTED:
                    require(key not in selected, "Ambiguous selected PE basename across folders")
                    require("pe" in row, "Selected program file is not a PE")
                    selected[key] = (entry.filename, payload)
            rows.append(row)
    return {"archiveSha256": hashlib.sha256(data).hexdigest(), "archiveBytes": len(data),
            "entryCount": len(rows), "uncompressedBytes": total, "allMemberCrcsValidated": True, "members": rows}, selected


def ascii_at(image, rva):
    text = bytearray()
    for offset in range(256):
        byte = image.data[image.raw(rva + offset)]
        if not byte:
            return text.decode("ascii")
        require(32 <= byte < 127, "Non-ASCII selected procedure name")
        text.append(byte)
    raise AuditError("Unterminated selected procedure name")


def focused_gles(image):
    result = {"knownWholeFileHashMatched": image.metadata["sha256"] == GLES_HASH,
              "manuallySelectedMapEntries": [], "manuallySelectedLookupInstructions": []}
    if not result["knownWholeFileHashMatched"]:
        return result
    code = image.decode(0x19b0)
    indexed = {ins.address - image.base: i for i, ins in enumerate(code)}
    for name, (name_at, pointer_at, store_at) in MAP_WITNESSES.items():
        left, right = code[indexed[pointer_at]], code[indexed[store_at]]
        label = code[indexed[name_at]]
        require(label.mnemonic == "lea" and label.reg_name(label.operands[0].reg) == "rdx", "Changed selected name setup")
        name_rva = image.rip_target(label, label.operands[1])
        require(name_rva is not None and ascii_at(image, name_rva) == name, "Changed selected procedure name")
        require(left.mnemonic == "lea" and left.reg_name(left.operands[0].reg) == "rcx"
                and right.mnemonic == "mov" and right.op_str == "qword ptr [rax], rcx"
                and left.address + left.size == right.address, "Changed selected adjacent pointer store")
        target = image.rip_target(left, left.operands[1])
        require(target is not None, "Selected map value is not RIP-relative code")
        image.raw(target, execute=True)
        result["manuallySelectedMapEntries"].append({"name": name, "nameRva": name_rva, "targetCodeRva": target,
            "instructions": [provider.instruction(image, ins) for ins in code[indexed[name_at]:indexed[store_at] + 1]]})
    getter = {ins.address - image.base: ins for ins in image.decode(0x9e20)}
    result["manuallySelectedLookupInstructions"] = [provider.instruction(image, getter[at])
        for at in (0x9f16, 0xa019, 0xa026, 0xa079)]
    return result


def sidecar(data, name):
    image = native.Image(data)
    try:
        require(image.metadata["dll"], "Selected sidecar is not a DLL")
        image.pe.parse_data_directories(directories=[0, 2])
        symbols = getattr(getattr(image.pe, "DIRECTORY_ENTRY_EXPORT", None), "symbols", [])
        require(len(symbols) <= 4096, "Too many sidecar exports")
        exports, seen = [], set()
        for symbol in symbols:
            text = symbol.name.decode("ascii") if symbol.name else None
            require(text is None or 0 < len(text) <= 2048 and all(32 <= ord(c) < 127 for c in text), "Unsafe export name")
            require(text is None or text not in seen, "Duplicate named sidecar export")
            seen.add(text)
            row = {"name": text, "rva": symbol.address, "ordinal": symbol.ordinal,
                   "forwarder": symbol.forwarder.decode("ascii") if symbol.forwarder else None,
                   **provider.section_kind(image, symbol.address), "entryJump": None}
            if not row["forwarder"] and row["executable"] and row["rawFileBacked"]:
                # This is a declared export ENTRY, not a byte-pattern candidate.
                # Decode ONE instruction only, never spill into the next export.
                section = next(s for s in image.sections if s.VirtualAddress <= symbol.address < s.VirtualAddress + s.SizeOfRawData)
                size = min(15, section.VirtualAddress + section.SizeOfRawData - symbol.address)
                at = image.raw(symbol.address, size, execute=True)
                ins = next(image.md.disasm(data[at:at + size], image.base + symbol.address, count=1), None)
                if ins and ins.mnemonic == "jmp":
                    jump = provider.instruction(image, ins)
                    if "directTargetRva" in jump:
                        jump["directTargetIsRawCode"] = all(provider.section_kind(image, jump["directTargetRva"])[k]
                                                                   for k in ("rawFileBacked", "executable"))
                    row["entryJump"] = jump
            exports.append(row)
        strings = []
        for group in getattr(image.pe, "FileInfo", []):
            for block in group:
                for table in getattr(block, "StringTable", []):
                    require(len(strings) < 16 and len(table.entries) <= 64, "Too many version strings")
                    require(all(len(k) <= 128 and len(v) <= 4096 for k, v in table.entries.items()), "Oversized version string")
                    strings.append({k.decode("utf-8", errors="replace"): v.decode("utf-8", errors="replace") for k, v in table.entries.items()})
        return {"inputMember": name, "identity": {k: v for k, v in image.metadata.items() if k != "sections"},
                "imageBase": image.base, "versionStringTables": strings, "exports": exports,
                "focused": focused_gles(image), "signatureValidated": False}
    finally:
        image.pe.close()


def egl_bridges(egl, gles):
    """Join only exact declared import names to this archive's named exports."""
    targets = {row["name"]: row for row in gles["exports"] if row["name"]}
    result = []
    for row in egl["exports"]:
        jump = row["entryJump"]
        imported = (jump or {}).get("import")
        if not row["name"] or not row["name"].startswith("egl") or not imported or imported["module"].lower() != "libglesv2.dll":
            continue
        target = targets.get(imported["symbol"])
        result.append({"publicName": row["name"], "shimExportRva": row["rva"], "instruction": jump,
                       "importedSymbol": imported["symbol"], "targetModule": gles["inputMember"],
                       "targetExportRva": target["rva"] if target else None,
                       "targetIsRawCode": bool(target and not target["forwarder"] and target["executable"] and target["rawFileBacked"])})
    return result


def inspect(path):
    path = qt.safe_path(path)
    inventory, selected = bundle(path)
    sidecars = {name: sidecar(data, member) for name, (member, data) in selected.items() if name in ("libegl.dll", "libglesv2.dll")}
    pairs = egl_bridges(sidecars["libegl.dll"], sidecars["libglesv2.dll"]) if len(sidecars) == 2 else []
    return {"schema": "bsi.player-program-files-static-audit.v1", "input": path.name, "archive": inventory,
            "missingSelectedBasenames": sorted(SELECTED - selected.keys()),
            "previousProviderWholeFileHashMatches": {name: hashlib.sha256(selected[name][1]).hexdigest() == digest if name in selected else None
                                                      for name, digest in KNOWN_PROVIDERS.items()},
            "sidecars": sidecars, "publicEglImportBridges": pairs, "archivePathsExtracted": False,
            "filesLoadedOrExecuted": False, "targetModified": False, "privateInterfacesCalled": False,
            "runtimeLoaderBindingsOrActiveRendererProved": False, "materialOwnershipOrPotatoRepairProved": False,
            "limits": ["Archive co-location is not independently authenticated installer provenance or launched-process identity.",
                       "Export jumps/import-name matches are not loaded module paths, active branches, resource ownership or lifetime proof.",
                       "Hash-pinned map witnesses are human-selected; they are not generic signatures, a complete CFG or a production hook plan."]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="An explicit local PF ZIP, never an installation directory")
    args = parser.parse_args()
    try:
        result, failed = inspect(args.archive), False
    except (AuditError, OSError, RuntimeError, ValueError, UnicodeError, struct.error, zipfile.BadZipFile, zlib.error) as error:
        result, failed = {"input": args.archive.name, "error": str(error), "filesLoadedOrExecuted": False, "targetModified": False}, True
    print(json.dumps(result, indent=2))
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
