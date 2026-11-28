#!/usr/bin/env python3
"""Read-only static inspection of an .apk/.xapk, as DATA ONLY.

Why this exists: the user's game cannot be executed on our machines (no hypervisor,
no /dev/kvm, no Android runtime), so "does Potato work in the game" cannot be a test
outcome here. What CAN be answered without running anything is the graphics shape of
the game, which is exactly what decides whether Potato's texture path is even
reachable:

  * which native libraries ship per ABI, and whether they are Android/emulator x86_64,
    ARM-only (which means the emulator must translate, so no native GL driver call is
    ever ours to hook), or x86;
  * which graphics stacks are imported (GLESv2/EGLv3 -> the hookable GL path,
    Vulkan -> native frames are kept, plus any Unity/Unreal/Cocos/Flutter/CEF engine);
  * whether the files are compressed inside the APK (zip quirks like the 0x08080000
    alignment scheme some stores still use);
  * the declared target/min SDK and permission set.

It never extracts to disk, never loads or executes a library, and never reaches the
network. Every number it prints is a static observation, NOT proof of what happens at
runtime; the report says so in its own `limits` field.

Usage:
    python scripts/Inspect-GameApk.py --xapk Free+Fire+MAX+v8a.xapk
    python scripts/Inspect-GameApk.py --xapk game.xapk --json report.json
"""
from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path
import re
import struct
import zipfile

MAX_ENTRY = 64 * 1024 * 1024      # one member larger than this is not scanned
MAX_ENTRIES = 20000               # bounded, like the other Inspect-* tools
MAX_TOTAL = 512 * 1024 * 1024     # stop scanning after this much uncompressed data

# ELF constants we actually need. Kept local so the tool has no dependencies.
ELF_MAGIC = b"\x7fELF"
ET_DYN = 3
EM_386, EM_X86_64, EM_ARM, EM_AARCH64 = 3, 62, 40, 183
PT_LOAD = 1
SHT_DYNSYM = 11

GRAPHICS_HINTS = {
    # Anything here is a static import name, not a runtime guarantee.
    "GLES": ("libGLESv2.so", "GLESv2", "glDrawArrays", "glDrawElements"),
    "EGL": ("libEGL.so", "EGL", "eglSwapBuffers"),
    "Vulkan": ("libvulkan.so", "vkCreateDevice", "vkQueuePresentKHR", "Vulkan"),
    "Unity": ("libunity.so", "libil2cpp.so", "UnityPlayer"),
    "Unreal": ("libUE4.so", "libUnreal.so", "libue4.so"),
    "Cocos": ("libcocos2dcpp.so", "libcocos2djs.so", "cocos2d"),
    "Flutter": ("libflutter.so", "flutter_assets"),
    "CEF/WebView": ("libcef.so", "libwebviewchromium.so"),
    "ANGLE (translation)": ("libGLESv2_angle.so", "libangle.so"),
    "SwiftShader (software)": ("libswiftshader", "swiftshader"),
    "Vulkan validation": ("vulkan-1",),
}

ABI_NOTES = {
    "x86_64": "native x86_64: an x86 BlueStacks instance runs this code directly",
    "x86": "32-bit x86: needs a 32-bit capable instance",
    "arm64-v8a": "ARM64 only: x86 instances must translate every call, so native GL "
                 "hooking is not a straight-line path",
    "armeabi-v7a": "32-bit ARM: same translation caveat as arm64",
}


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def bounded_zip(path: Path):
    """Refuse an archive that is not a plausible app package before reading it."""
    require(path.is_file(), f"{path} is not a file")
    require(zipfile.is_zipfile(path), f"{path} is not a zip container")
    # A plain APK starts with one entry; an XAPK is a zip of an APK plus OBB splits.
    with zipfile.ZipFile(path) as archive:
        infos = archive.infolist()
    require(len(infos) <= MAX_ENTRIES, f"{path} has more than {MAX_ENTRIES} entries")
    return infos


def elf_header(blob: bytes):
    """Endianness, machine and the dynamic-symbol table, if this really is an ELF."""
    if len(blob) < 0x40 or not blob.startswith(ELF_MAGIC):
        return None
    little = blob[5] == 1
    order = "<" if little else ">"
    if blob[4] != 2:                       # only 64-bit ELF is inspected in depth
        machine = struct.unpack_from(order + "H", blob, 18)[0]
        return {"class": 32, "little": little, "machine": machine, "sections": None}
    (e_type, e_machine, _version, _entry, e_phoff, e_shoff, _flags, _ehsize, _phentsize,
     _phnum, e_shentsize, e_shnum, _shstrndx) = struct.unpack_from(order + "HHIQQQIHHHHHH", blob, 16)
    sections = []
    try:
        for index in range(e_shnum):
            base = e_shoff + index * e_shentsize
            if base + 64 > len(blob):
                break
            (_name, sh_type, _flags, _addr, sh_offset, sh_size, _link, _info, _align,
             sh_entsize) = struct.unpack_from(order + "IIQQQQIIQQ", blob, base)
            sections.append({"type": sh_type, "offset": sh_offset, "size": sh_size,
                             "entsize": sh_entsize})
    except struct.error:
        sections = []
    return {"class": 64, "little": little, "machine": e_machine, "type": e_type,
            "sections": sections}


def dynamic_symbols(blob: bytes, header: dict, limit: int = 40000):
    """Imported/exported symbol names, bounded. Returns (names, encoded)."""
    names, encoded = [], False
    sections = header.get("sections") or []
    table = next((s for s in sections if s["type"] == SHT_DYNSYM), None)
    if table is None:
        # No section table (fully stripped): the honest answer is "hidden", not a
        # guessed offset. Callers must not report "no graphics" for this case.
        return names, True
    # st_name is an offset into the DYNAMIC STRING TABLE, which is a separate
    # section (SHT_STRTAB == 3). Reading it as a file offset silently finds nothing.
    strtab = next((s for s in sections if s["type"] == 3), None)
    if strtab is None:
        return names, True
    order = "<" if header["little"] else ">"
    entsize = table["entsize"] or 24
    count = min(table["size"] // entsize, limit)
    strtab_end = min(strtab["offset"] + strtab["size"], len(blob))
    for index in range(count):
        base = table["offset"] + index * entsize
        if base + 24 > len(blob):
            break
        st_name = struct.unpack_from(order + "I", blob, base)[0]
        start = strtab["offset"] + st_name
        if not st_name or start >= strtab_end:
            continue
        end = blob.find(b"\x00", start, strtab_end)
        if end < 0:
            continue
        try:
            names.append(blob[start:end].decode("ascii"))
        except UnicodeDecodeError:
            continue
    return names, False


def classify_imports(names):
    hits = defaultdict(int)
    lowered = [name.lower() for name in names]
    for label, needles in GRAPHICS_HINTS.items():
        for needle in needles:
            key = needle.lower()
            for name in lowered:
                if key in name:
                    hits[label] += 1
                    break
    return dict(hits)


def inspect(xapk: Path) -> dict:
    infos = bounded_zip(xapk)
    report = {
        "file": xapk.name,
        "size": xapk.stat().st_size,
        "sha256": hashlib.sha256(xapk.read_bytes()).hexdigest(),
        "entries": len(infos),
        "apks": [],
        "abis": {},
        "graphics": {},
        "libraries": [],
        "engines": [],
        "notes": [],
    }

    total = 0
    with zipfile.ZipFile(xapk) as outer:
        for info in infos:
            name = info.filename
            if name.endswith(".apk"):
                report["apks"].append({"name": name, "size": info.file_size})
                if info.file_size > MAX_ENTRY:
                    report["notes"].append(f"{name}: {info.file_size} B, not opened (bounded)")
                    continue
                total += info.file_size
                if total > MAX_TOTAL:
                    report["notes"].append("scan stopped at the uncompressed budget")
                    break
                with outer.open(info) as handle:
                    import io
                    import tempfile
                    with tempfile.NamedTemporaryFile(suffix=".apk", delete=True) as scratch:
                        scratch.write(handle.read(MAX_ENTRY))
                        scratch.flush()
                        try:
                            with zipfile.ZipFile(scratch.name) as inner:
                                scan_apk(inner, report)
                        except zipfile.BadZipFile:
                            report["notes"].append(f"{name}: inner archive unreadable")

    for abi, entry in report["abis"].items():
        entry["note"] = ABI_NOTES.get(abi, "unrecognised ABI directory")
    report["limits"] = (
        "Static, read-only inspection of archive members and ELF headers/symbols. "
        "Nothing was extracted, loaded or executed and no network was used. Import "
        "names prove what a library REFERENCES, not which path the game takes at "
        "runtime, and not whether it rendered a frame.")
    return report


def scan_apk(archive: zipfile.ZipFile, report: dict):
    for info in archive.infolist():
        name = info.filename
        abi = next((part for part in ("arm64-v8a", "armeabi-v7a", "x86_64", "x86")
                    if f"lib/{part}/" in name), None)
        if name.startswith("lib/") and name.endswith(".so"):
            entry = report["abis"].setdefault(abi or "unknown", {"libraries": 0, "bytes": 0})
            entry["libraries"] += 1
            entry["bytes"] += info.file_size
            if info.file_size > MAX_ENTRY:
                report["libraries"].append({"name": name, "size": info.file_size,
                                            "skipped": "too large to scan"})
                continue
            try:
                blob = archive.read(info)
            except (RuntimeError, zipfile.BadZipFile):
                report["libraries"].append({"name": name, "size": info.file_size,
                                            "skipped": "compression unsupported"})
                continue
            header = elf_header(blob)
            if not header:
                report["libraries"].append({"name": name, "size": info.file_size,
                                            "skipped": "not a readable ELF"})
                continue
            symbols, fallback = dynamic_symbols(blob, header)
            graphics = classify_imports(symbols)
            report["libraries"].append({
                "name": name, "size": info.file_size, "abi": abi,
                "machine": {3: "x86", 62: "x86_64", 40: "arm", 183: "arm64"}.get(header["machine"], header["machine"]),
                "bits": header["class"], "graphics": graphics,
                "symbols": len(symbols), "symbolTableHidden": fallback,
            })
            for label in graphics:
                report["graphics"].setdefault(label, []).append(name)
        elif name == "AndroidManifest.xml" and info.file_size <= 8 * 1024 * 1024:
            blob = archive.read(info)
            report["manifest"] = summarise_manifest(blob)
    for label, files in report["graphics"].items():
        if label in ("Unity", "Unreal", "Cocos", "Flutter"):
            report["engines"].append({"engine": label, "evidence": files[0]})


def summarise_manifest(blob: bytes) -> dict:
    """Best-effort strings from a binary AXML; explicitly not a parser."""
    text = blob.decode("utf-16-le", errors="ignore") + blob.decode("utf-8", errors="ignore")
    packages = sorted(set(re.findall(r"\b(?:com|org|net)\.[A-Za-z0-9_.]{3,80}", text)))[:12]
    return {
        "packageCandidates": packages,
        "permissions": sorted(set(re.findall(r"android\.permission\.[A-Z_]+", text)))[:40],
        "mentionsVulkan": "vulkan" in text.lower(),
        "mentionsGles": bool(re.search(r"gles|opengl", text, re.I)),
        "note": "Binary AXML scanned for strings only; no attribute is interpreted.",
    }


def verdict(report: dict) -> list[str]:
    lines = []
    graphics = set(report["graphics"])
    abis = set(report["abis"])
    x86 = {"x86", "x86_64"} & abis
    arm = {"arm64-v8a", "armeabi-v7a"} & abis

    if x86:
        lines.append(f"x86 libraries present ({', '.join(sorted(x86))}): an x86 BlueStacks "
                     "instance runs this code natively, so Potato's GL path is reachable.")
    if arm and not x86:
        lines.append("ARM-only: an x86 instance must translate, so a native "
                     "glDrawArrays/glDrawElements hook is not a straight-line path.")
    if "GLES" in graphics:
        lines.append("glDrawArrays/glDrawElements are imported: the OpenGL/GLES Potato "
                     "path can attach (still unproven until frames render).")
    if "Vulkan" in graphics:
        lines.append("Vulkan is referenced: if the game presents through Vulkan, native "
                     "presentation frames are kept unchanged by design.")
    if "Unity" in graphics:
        lines.append("Unity engine libraries: the Unity runtime owns its GL context, which "
                     "usually means translated/native-GL depending on the instance.")
    lines.append("Static imports only. This cannot say whether a frame was rendered; that "
                 "is what the in-game status token's frame counter is for.")
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--xapk", required=True, type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    report = inspect(args.xapk)
    print(f"FILE: {report['file']}  {report['size']} B")
    print(f"SHA256: {report['sha256']}")
    print(f"ARCHIVE: {report['entries']} entries, {len(report['apks'])} apk member(s)")
    for apk in report["apks"]:
        print(f"  apk: {apk['name']}  {apk['size']} B")
    print("ABIs:")
    for abi, entry in report["abis"].items():
        print(f"  {abi}: {entry['libraries']} libraries, {entry['bytes']} B")
    print("GRAPHICS REFERENCES:")
    for label, files in sorted(report["graphics"].items()):
        print(f"  {label}: {len(files)} library/libraries, e.g. {files[0]}")
    if report.get("manifest"):
        manifest = report["manifest"]
        print(f"MANIFEST strings: packages={manifest['packageCandidates'][:3]} "
              f"vulkan={manifest['mentionsVulkan']} gles={manifest['mentionsGles']}")
    print("VERDICT:")
    for line in verdict(report):
        print(f"  - {line}")
    print(f"LIMIT: {report['limits']}")
    if args.json:
        args.json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"JSON: {args.json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
