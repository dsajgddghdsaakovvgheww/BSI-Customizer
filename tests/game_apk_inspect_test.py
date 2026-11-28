#!/usr/bin/env python3
"""Contract test for scripts/Inspect-GameApk.py on synthetic packages.

The user asked for the game to be downloaded and played. It cannot be executed here
(no hypervisor, no /dev/kvm, no Android runtime, and the file host is not reachable
from this sandbox), so the tool answers the part that is answerable statically. This
test proves the answering logic, not the game:

* a real ELF64 with a dynamic-symbol table inside an APK inside an XAPK is read and
  its graphics imports are attributed to the right ABI;
* an ARM-only package is reported as ARM-only (never as "hookable");
* Vulkan references are reported alongside GLES, not merged into "works";
* a nonsense file is refused instead of producing a verdict;
* nothing is written next to the input and no network is touched (import check).
"""
import json
import pathlib
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOL = ROOT / "scripts" / "Inspect-GameApk.py"


def elf64(machine, symbols):
    """A minimal but structurally valid ELF64 with a .dynsym/.dynstr pair."""
    names = b"\x00"
    offsets = []
    for symbol in symbols:
        offsets.append(len(names))
        names += symbol.encode() + b"\x00"
    # Layout: header (64) | dynam sym (24 each) | names, plus a section header table.
    sym_offset = 64
    sym_size = 24 * len(symbols)
    name_offset = sym_offset + sym_size
    sh_offset = name_offset + len(names)
    # Two real sections: SHT_DYNSYM and the SHT_STRTAB its st_name values index.
    # A module without the string table is a stripped module, and the inspector
    # must refuse to guess at its imports.
    sh_count = 3
    blob = bytearray(sh_offset + 64 * sh_count)
    struct.pack_into("<4sBBBBB7x", blob, 0, b"\x7fELF", 2, 1, 1, 0, 0)
    struct.pack_into("<HHIQQQIHHHHHH", blob, 16,
                     3, machine, 1, 0, 0, sh_offset, 0, 64, 56, 0, 64, sh_count, 0)
    for index, offset in enumerate(offsets):
        struct.pack_into("<I", blob, sym_offset + index * 24, offset)
    blob[name_offset:name_offset + len(names)] = names
    struct.pack_into("<IIQQQQIIQQ", blob, sh_offset + 64,
                     0, 11, 0, 0, sym_offset, sym_size, 0, 0, 8, 24)          # .dynsym
    struct.pack_into("<IIQQQQIIQQ", blob, sh_offset + 128,
                     0, 3, 0, 0, name_offset, len(names), 1, 0, 1, 0)         # .dynstr
    return bytes(blob)


def build(path, abis, symbols_by_abi, manifest=b""):
    apk_path = path.with_suffix(".inner.apk")
    with zipfile.ZipFile(apk_path, "w") as apk:
        for abi, machine in abis.items():
            apk.writestr(f"lib/{abi}/libgame.so", elf64(machine, symbols_by_abi))
        if manifest:
            apk.writestr("AndroidManifest.xml", manifest)
    with zipfile.ZipFile(path, "w") as xapk:
        xapk.write(apk_path, "com.example.game.apk")
        xapk.writestr("manifest.json", json.dumps({"xapk_version": 2}))
    apk_path.unlink()


def run(path):
    result = subprocess.run([sys.executable, str(TOOL), "--xapk", str(path)],
                            capture_output=True, text=True)
    return result


def main():
    with tempfile.TemporaryDirectory(prefix="bsi-apk-") as folder:
        folder = pathlib.Path(folder)

        # 1. x86 with GLES/EGL imports: the honest positive statement.
        native = folder / "x86.xapk"
        build(native, {"x86_64": 62, "x86": 3},
              ["glDrawArrays", "glDrawElements", "eglSwapBuffers", "Java_com_x_y"])
        result = run(native)
        assert result.returncode == 0, result.stdout + result.stderr
        assert "x86_64: 1 libraries" in result.stdout, result.stdout
        assert "GLES: 2 library/libraries" in result.stdout, result.stdout
        assert "x86 libraries present" in result.stdout, result.stdout
        assert "glDrawArrays/glDrawElements are imported" in result.stdout, result.stdout

        # 2. ARM only: must never be described as a native hook path.
        arm = folder / "arm.xapk"
        build(arm, {"arm64-v8a": 183}, ["glDrawArrays", "vkQueuePresentKHR"])
        result = run(arm)
        assert "ARM-only" in result.stdout, result.stdout
        assert "x86 libraries present" not in result.stdout
        assert "Vulkan is referenced" in result.stdout, result.stdout

        # 3. Vulkan beside GLES stays two separate facts.
        both = folder / "both.xapk"
        build(both, {"x86_64": 62}, ["glDrawArrays", "vkCreateDevice"])
        result = run(both)
        assert "Vulkan is referenced" in result.stdout and "Vulkan" in result.stdout
        assert "GLES" in result.stdout

        # 4. The manifest is only quoted as strings, never interpreted.
        manifest = folder / "manifest.xapk"
        raw = "com.example.ffmax".encode("utf-16-le") + b"\x00" * 8 + b"android.permission.INTERNET"
        build(manifest, {"x86_64": 62}, ["glDrawArrays"], manifest=raw)
        result = run(manifest)
        assert "com.example.ffmax" in result.stdout, result.stdout

        # 5. A non-archive is refused, not "verified".
        bogus = folder / "bogus.xapk"
        bogus.write_bytes(b"not an archive")
        result = run(bogus)
        assert result.returncode != 0 and "not a zip container" in (result.stderr + result.stdout), \
            result.stdout + result.stderr

        # 6. The tool states its own limit in every report.
        result = run(native)
        assert "Static imports only" in result.stdout
        assert "cannot say whether a frame was rendered" in result.stdout

        # 7. Read-only: the input's bytes are untouched and no sibling file appears.
        before = {p.name: p.stat().st_mtime_ns for p in folder.iterdir()}
        run(native)
        after = {p.name: p.stat().st_mtime_ns for p in folder.iterdir()}
        assert before == after, (before, after)

    source = TOOL.read_text(encoding="utf-8")
    for forbidden in ("urllib", "requests", "socket", "http://", "https://"):
        assert forbidden not in source, f"the inspector must stay offline: {forbidden}"
    for forbidden in (".extractall", "os.system", "subprocess"):
        assert forbidden not in source, f"the inspector must never extract or execute: {forbidden}"
    print("PASS: static xapk inspection attributes graphics imports to the right ABI, reports "
          "ARM-only and Vulkan as their own facts, quotes the manifest without interpreting it, "
          "refuses a non-archive, states that it cannot prove a rendered frame, and never "
          "extracts, executes or goes online")
    print("LIMIT: logic on synthetic ELF/zip containers; it says nothing about the real game")
    return 0


if __name__ == "__main__":
    sys.exit(main())
