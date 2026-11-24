#!/usr/bin/env python3
"""Bounded, read-only native-renderer research; never loads or executes a PE.

Dependencies: pefile==2024.8.26 and capstone==5.0.6. Accept explicit local PEs
or the EXE-only reference ZIPs supported by Inspect-EmulatorQt.py. No network,
process attachment, recursion into adjacent DLLs, extraction, patching, shader
compatibility certification, or inference of resource ownership occurs.

RVA witnesses are STATIC instruction/data evidence, not a running-game trace.
Private renderer tables are deliberately not called or given guessed ABIs.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import zlib

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG, X86_REG_RIP

SPEC = importlib.util.spec_from_file_location("bsi_qt_read_only", Path(__file__).with_name("Inspect-EmulatorQt.py"))
qt = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(qt)
AuditError = qt.InventoryError
MAX_RANGES = 65536
MAX_CODE = 1024 * 1024
MAX_TEXT = 4 * 1024 * 1024
LITERALS = (
    "HD-Opengl-Native", "libOpenglRender", "grApiInit", "grLibInit",
    "Initializing ui ...", "Initializing graphics ...",
    "imgdOnEglCreateContext", "tmapOnEglCreateContext",
    "eglGetProcAddress", "wglGetProcAddress", "eglCreateImageKHR",
    "eglDestroyImageKHR", "glEGLImageTargetTexture2DOES", "glCopyImageSubData",
)


def require(ok, message):
    if not ok:
        raise AuditError(message)


class Image:
    def __init__(self, data):
        self.data = data
        self.metadata = qt.image(data)
        require(self.metadata["machine"] == "0x8664", "Native instruction audit supports Windows x64 only")
        self.pe = pefile.PE(data=data, fast_load=True)
        try:
            self.pe.parse_data_directories(directories=[1, 3, 13])
            self.base = self.pe.OPTIONAL_HEADER.ImageBase
            self.sections = list(self.pe.sections)
            self.imports = {}
            for kind, attr in (("normal", "DIRECTORY_ENTRY_IMPORT"), ("delay", "DIRECTORY_ENTRY_DELAY_IMPORT")):
                for module in getattr(self.pe, attr, []):
                    for symbol in module.imports:
                        require(len(self.imports) < 131072, "Too many import slots")
                        self.imports[symbol.address - self.base] = {
                            "module": module.dll.decode("ascii"), "kind": kind,
                            "symbol": symbol.name.decode("ascii") if symbol.name else f"#{symbol.ordinal}",
                        }
            self.ranges = sorted({(entry.struct.BeginAddress, entry.struct.EndAddress)
                                  for entry in getattr(self.pe, "DIRECTORY_ENTRY_EXCEPTION", [])})
            require(len(self.ranges) <= MAX_RANGES, "Too many unwind ranges")
            previous_end, total = 0, 0
            for begin, end in self.ranges:
                require(previous_end <= begin < end and end - begin <= MAX_CODE, "Overlapping/invalid/oversized unwind range")
                self.raw(begin, end - begin, execute=True)
                previous_end, total = end, total + end - begin
            require(total <= 64 * 1024 * 1024, "Too many native instruction bytes")
            self.decoded_bytes = 0
            self.starts = [begin for begin, _ in self.ranges]
            self.md = Cs(CS_ARCH_X86, CS_MODE_64)
            self.md.detail = True
            self.cache = {}
        except Exception:
            self.pe.close()
            raise

    def raw(self, rva, size=1, *, execute=False):
        require(0 <= rva <= 0xFFFFFFFF and 0 < size <= len(self.data), "Invalid byte range")
        for section in self.sections:
            delta = rva - section.VirtualAddress
            if 0 <= delta and delta + size <= section.SizeOfRawData:
                require(not execute or section.Characteristics & 0x20000000, "Instruction bytes are not executable")
                at = section.PointerToRawData + delta
                require(at + size <= len(self.data), "Truncated raw section")
                return at
        raise AuditError("Byte range has no raw-section backing (virtual zero-fill is not file data)")

    def code_sections(self):
        for section in self.sections:
            if section.Characteristics & 0x20000000:
                at = self.raw(section.VirtualAddress, section.SizeOfRawData, execute=True)
                yield section.VirtualAddress, self.data[at:at + section.SizeOfRawData]

    def owner(self, rva):
        import bisect
        at = bisect.bisect_right(self.starts, rva) - 1
        return self.ranges[at] if at >= 0 and rva < self.ranges[at][1] else None

    def decode(self, begin):
        if begin in self.cache:
            return self.cache[begin]
        scope = self.owner(begin)
        if scope and scope[0] == begin:
            at = self.raw(begin, scope[1] - begin, execute=True)
            code = self.data[at:at + scope[1] - begin]
        else:
            # A leaf builder can lack unwind metadata. Decode only from an
            # observed direct-call target, with a fixed cap and first-ret stop.
            section = next((s for s in self.sections if s.VirtualAddress <= begin
                            < s.VirtualAddress + s.SizeOfRawData and s.Characteristics & 0x20000000), None)
            require(section is not None, "Direct-call target has no executable raw backing")
            size = min(4096, section.VirtualAddress + section.SizeOfRawData - begin)
            at = self.raw(begin, size, execute=True)
            code = self.data[at:at + size]
        self.decoded_bytes += len(code)
        require(self.decoded_bytes <= 64 * 1024 * 1024, "Decoded-instruction work exceeds audit limit")
        instructions = []
        for ins in self.md.disasm(code, self.base + begin):
            instructions.append(ins)
            if (not scope or scope[0] != begin) and ins.mnemonic.startswith("ret"):
                break
        self.cache[begin] = instructions
        return instructions

    def rip_target(self, ins, operand):
        if operand.type == X86_OP_MEM and operand.mem.base == X86_REG_RIP:
            return ins.address + ins.size + operand.mem.disp - self.base
        return None

    def import_at_call(self, ins):
        if ins.mnemonic not in ("call", "jmp") or len(ins.operands) != 1:
            return None
        operand = ins.operands[0]
        slot = self.rip_target(ins, operand)
        if operand.type == X86_OP_IMM:
            try:
                rva = operand.imm - self.base
                at = self.raw(rva, 6, execute=True)
                if self.data[at:at + 2] == b"\xff\x25":
                    slot = rva + 6 + struct.unpack_from("<i", self.data, at + 2)[0]
            except AuditError:
                return None
        return self.imports.get(slot)

    def direct_calls(self, instructions):
        return [(ins, ins.operands[0].imm - self.base) for ins in instructions
                if ins.mnemonic == "call" and len(ins.operands) == 1 and ins.operands[0].type == X86_OP_IMM]

    def import_calls(self, begin):
        return [{"instructionRva": ins.address - self.base, **symbol}
                for ins in self.decode(begin) if (symbol := self.import_at_call(ins))]

    def callers(self, target):
        rows = []
        for start, code in self.code_sections():
            for match in re.finditer(rb"(?=([\xe8].{4}))", code, re.S):
                rva = start + match.start()
                if rva + 5 + struct.unpack_from("<i", match[1], 1)[0] != target:
                    continue
                scope = self.owner(rva)
                if scope and any(ins.address - self.base == rva and ins.mnemonic == "call" for ins in self.decode(scope[0])):
                    rows.append({"callRva": rva, "targetRva": target, "unwindRange": list(scope)})
        return rows


def literal_references(image):
    literals, targets, scopes = {}, {}, set()
    for text in LITERALS:
        found, at = [], 0
        while (at := image.data.find(text.encode("ascii") + b"\0", at)) >= 0:
            require(len(found) < 128, "Too many repeated anchor literals")
            rva = image.pe.get_rva_from_offset(at)
            image.raw(rva, len(text) + 1)
            found.append(rva)
            targets[rva] = text
            at += len(text) + 1
        literals[text] = {"literalRvas": found, "decodedCodeReferences": []}
    # Candidate LEAs are never credited without a decoded instruction start
    # from a bounded unwind range. Other RIP reads in those ranges are included
    # too: the renderer basenames are copied with SIMD/scalar loads, not LEAs.
    for start, code in image.code_sections():
        for match in re.finditer(rb"(?=([\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d].{4}))", code, re.S):
            rva = start + match.start()
            target = rva + 7 + struct.unpack_from("<i", match[1], 3)[0]
            scope = image.owner(rva)
            if target in targets and scope:
                scopes.add(scope)
    for scope in sorted(scopes):
        for ins in image.decode(scope[0]):
            for operand in ins.operands:
                target = image.rip_target(ins, operand)
                if target in targets:
                    literals[targets[target]]["decodedCodeReferences"].append({
                        "instructionRva": ins.address - image.base, "unwindRange": list(scope),
                        "mnemonic": ins.mnemonic, "literalRva": target,
                    })
    return literals


def renderer_boundary(image, literals):
    entries = []
    begins = sorted({row["unwindRange"][0] for row in literals["grApiInit"]["decodedCodeReferences"]})
    require(len(begins) <= 32, "Too many distinct renderer-boundary candidates")
    for begin in begins:
        instructions = image.decode(begin)
        helper_calls = []
        for call, target in image.direct_calls(instructions):
            try:
                imports = image.import_calls(target)
            except AuditError:
                continue
            for api in imports:
                if api["symbol"] in ("LoadLibraryW", "LoadLibraryA", "GetProcAddress"):
                    helper_calls.append({"callerCallRva": call.address - image.base,
                                         "calleeRva": target, "api": api})
        resolved_calls = []
        for witness in helper_calls:
            if witness["api"]["symbol"] != "GetProcAddress":
                continue
            after = [ins for ins in instructions if ins.address - image.base > witness["callerCallRva"]]
            # Narrow, explicit Windows x64 nonvolatile return-value flow. No
            # guessed propagation across arbitrary writes/caller-saved regs.
            if not after:
                continue
            first = after[0]
            if (first.mnemonic != "mov" or len(first.operands) != 2
                    or any(op.type != X86_OP_REG for op in first.operands)
                    or first.reg_name(first.operands[1].reg) != "rax"):
                continue
            register = first.operands[0].reg
            if first.reg_name(register) not in ("rbx", "rbp", "rsi", "rdi", "r12", "r13", "r14", "r15"):
                continue
            for ins in after[1:]:
                if (ins.mnemonic == "call" and len(ins.operands) == 1
                        and ins.operands[0].type == X86_OP_REG and ins.operands[0].reg == register):
                    resolved_calls.append({"lookupHelperCallRva": witness["callerCallRva"],
                                           "returnSavedAtRva": first.address - image.base,
                                           "nonvolatileRegister": first.reg_name(register),
                                           "indirectCallRva": ins.address - image.base})
                    break
                family = {"rbx": {"rbx", "ebx", "bx", "bl", "bh"},
                          "rbp": {"rbp", "ebp", "bp", "bpl"},
                          "rsi": {"rsi", "esi", "si", "sil"},
                          "rdi": {"rdi", "edi", "di", "dil"}}
                name = first.reg_name(register)
                aliases = family.get(name, {name, name + "d", name + "w", name + "b"})
                if any(ins.reg_name(written) in aliases for written in ins.regs_access()[1]):
                    break
        modules = {name: [ref for ref in literals[name]["decodedCodeReferences"] if ref["unwindRange"][0] == begin]
                   for name in ("HD-Opengl-Native", "libOpenglRender")}
        entries.append({"unwindRangeBegin": begin, "moduleOperandReferences": modules,
                        "directHelperImportWitnesses": helper_calls,
                        "resolvedPointerCallWitnesses": resolved_calls,
                        "directCallers": image.callers(begin), "privateAbiInferred": False})
    return entries


def startup_evidence(image, literals, boundaries):
    rows = []
    for boundary in boundaries:
        for caller in boundary["directCallers"]:
            begin, end = caller["unwindRange"]
            ui_refs = [ref for ref in literals["Initializing ui ..."]["decodedCodeReferences"]
                       if ref["unwindRange"][0] == begin and ref["instructionRva"] < caller["callRva"]]
            if not ui_refs:
                continue
            ui_anchor = ui_refs[-1]["instructionRva"]
            candidates = [(call, target) for call, target in image.direct_calls(image.decode(begin))
                          if ui_anchor < call.address - image.base < caller["callRva"]][:8]
            for call, target in candidates:
                try:
                    qml_calls = [entry for entry in image.import_calls(target)
                                 if entry["symbol"].startswith("?load@QQmlApplicationEngine@@")]
                except AuditError:
                    continue
                if qml_calls:
                    rows.append({"mainUnwindRange": [begin, end], "uiInitCallRva": call.address - image.base,
                                 "uiInitTargetRva": target, "qmlEngineLoadCalls": qml_calls,
                                 "graphicsInitCallRva": caller["callRva"], "graphicsInitTargetRva": caller["targetRva"],
                                 "scope": "same decoded native caller, instruction order only; not an installed startup trace"})
    return rows


def callback_builder_evidence(image, literals, boundaries):
    named = {row["unwindRange"][0]: label
             for label in ("imgdOnEglCreateContext", "tmapOnEglCreateContext")
             for row in literals[label]["decodedCodeReferences"]}
    builders, seen = [], set()
    for boundary in boundaries:
        for caller in boundary["directCallers"]:
            before = [ins for ins in image.decode(caller["unwindRange"][0])
                      if ins.address - image.base < caller["callRva"]][-128:]
            for call, target in image.direct_calls(before):
                if target in seen:
                    continue
                seen.add(target)
                try:
                    instructions = image.decode(target)
                except AuditError:
                    continue
                stores = []
                for lea, store in zip(instructions, instructions[1:]):
                    if (lea.mnemonic != "lea" or len(lea.operands) != 2 or lea.operands[0].type != X86_OP_REG
                            or lea.operands[0].size != 8 or store.mnemonic != "mov" or len(store.operands) != 2
                            or store.operands[0].type != X86_OP_MEM or store.operands[0].size != 8
                            or store.operands[1].type != X86_OP_REG or store.operands[1].reg != lea.operands[0].reg):
                        continue
                    pointer = image.rip_target(lea, lea.operands[1])
                    destination = store.operands[0].mem
                    if pointer is None or destination.base == X86_REG_RIP or destination.index:
                        continue
                    try:
                        image.raw(pointer, 1, execute=True)
                    except AuditError:
                        continue
                    stores.append({"addressLoadRva": lea.address - image.base, "storeRva": store.address - image.base,
                                   "targetCodeRva": pointer, "destinationBaseRegister": store.reg_name(destination.base),
                                   "destinationOffset": destination.disp, "namedContextCallback": named.get(pointer)})
                if any(row["namedContextCallback"] for row in stores):
                    builders.append({"builderRva": target, "nativeStartupCallerRva": call.address - image.base,
                                     "laterGraphicsInitCallRva": caller["callRva"], "adjacentCodePointerStores": stores,
                                     "scope": "code-pointer assignments; callback parameters, lifetime and private ABI are not inferred"})
    return builders


def registered_qml(image):
    slots = {rva for rva, entry in image.imports.items() if entry["symbol"].startswith("?qRegisterResourceData@")}
    thunks, calls, pools = set(), set(), {}
    for start, code in image.code_sections():
        for match in re.finditer(rb"(?=(\xff[\x15\x25].{4}))", code, re.S):
            rva = start + match.start()
            if rva + 6 + struct.unpack_from("<i", match[1], 2)[0] in slots:
                calls.add(rva)
                if match[1][1] == 0x25:
                    thunks.add(rva)
    for start, code in image.code_sections():
        for match in re.finditer(rb"(?=([\xe8\xe9].{4}))", code, re.S):
            rva = start + match.start()
            if rva + 5 + struct.unpack_from("<i", match[1], 1)[0] in thunks:
                calls.add(rva)
    for call in sorted(calls):
        try:
            at = image.raw(call - 26, 26, execute=True)
        except AuditError:
            continue
        instructions = list(image.md.disasm(image.data[at:at + 26], image.base + call - 26))
        if len(instructions) != 4 or sum(ins.size for ins in instructions) != 26:
            continue
        args = {}
        for ins in instructions:
            ops = ins.operands
            if len(ops) != 2 or ops[0].type != X86_OP_REG:
                break
            reg = ins.reg_name(ops[0].reg)
            if ins.mnemonic == "mov" and reg == "ecx" and ops[1].type == X86_OP_IMM:
                args[reg] = ops[1].imm
            elif ins.mnemonic == "lea" and reg in ("rdx", "r8", "r9") and (target := image.rip_target(ins, ops[1])) is not None:
                args[reg] = target
            else:
                break
        if set(args) == {"ecx", "rdx", "r8", "r9"} and args["ecx"] in (1, 2, 3):
            key = tuple(args[name] for name in ("ecx", "rdx", "r8", "r9"))
            pools.setdefault(key, []).append(call)
            require(len(pools) <= 128, "Too many distinct registered pools")
    source_rows, count = [], 0
    for (version, tree, names, body), registrations in sorted(pools.items()):
        stride = 14 if version == 1 else 22
        require(struct.unpack_from(">H", image.data, image.raw(tree, stride) + 4)[0] == 2, "Registered root is not a directory")
        pending, seen = [(0, "")], set()
        while pending:
            node, path = pending.pop()
            require(node < 65536 and node not in seen, "Cyclic, reused or excessive resource nodes")
            seen.add(node)
            at = image.raw(tree + node * stride, stride)
            nameoff, flags = struct.unpack_from(">IH", image.data, at)
            value = struct.unpack_from(">I", image.data, at + 10)[0]
            require(flags in (0, 1, 2, 4), "Unsupported resource flags")
            if node:
                nameat = image.raw(names + nameoff, 6)
                length, wanted = struct.unpack_from(">HI", image.data, nameat)
                require(0 < length <= 1024, "Invalid resource name size")
                textat = image.raw(names + nameoff + 6, length * 2)
                units = struct.unpack_from(">" + "H" * length, image.data, textat)
                h = 0
                for ch in units:
                    require(ch >= 32 and ch not in (47, 92), "Invalid resource name component")
                    h = (h << 4) + ch
                    h ^= (h & 0xF0000000) >> 23
                    h &= 0x0FFFFFFF
                require(h == wanted, "Resource name hash mismatch")
                path += "/" + image.data[textat:textat + length * 2].decode("utf-16be")
                require(len(path) <= 4096, "Resource path exceeds audit limit")
            if flags == 2:
                children = struct.unpack_from(">I", image.data, at + 6)[0]
                require(children <= 65536 and value + children <= 65536, "Invalid resource child bounds")
                require(len(seen) + len(pending) + children <= 65536, "Resource work exceeds connected-tree limit")
                pending.extend((value + n, path) for n in range(children))
                continue
            count += 1
            require(count <= 262144, "Resource inventory exceeds audit limit")
            pos = image.raw(body + value, 4)
            size = struct.unpack_from(">I", image.data, pos)[0]
            require(0 < size <= 32 * 1024 * 1024, "Invalid resource record length")
            pos = image.raw(body + value + 4, size)
            if path not in ("/plr.qml", "/UiGraphicsSetting.qml"):
                continue
            require(len(source_rows) < 64, "Too many selected source records")
            payload = image.data[pos:pos + size]
            require(flags != 4, "Selected QML uses unimplemented zstd; no source completeness claim")
            if flags == 1:
                require(size >= 4, "Truncated qCompress length")
                wanted = struct.unpack_from(">I", payload)[0]
                require(0 < wanted <= MAX_TEXT, "Oversized selected QML")
                stream = zlib.decompressobj()
                payload = stream.decompress(payload[4:], MAX_TEXT + 1)
                require(len(payload) == wanted and stream.eof and not stream.unconsumed_tail and not stream.unused_data,
                        "Truncated, oversized or trailing selected QML compression")
            require(len(payload) <= MAX_TEXT, "Oversized selected QML")
            text = payload.decode("utf-8")
            source_rows.append({"path": path, "treeRva": tree, "recordRva": body + value,
                                "decodedBytes": len(payload), "sha256": hashlib.sha256(payload).hexdigest(),
                                "registrationCallRvas": registrations,
                                "rootCompletionLines": [n for n, line in enumerate(text.splitlines(), 1)
                                                        if re.match(r"^ {4}Component\.onCompleted\s*:", line)],
                                "setBackendUiParamsPresent": "plrBackend.setBackendUiParams(" in text})
    return {"uniquePoolCount": len(pools), "connectedFileEntryCount": count, "selectedSources": source_rows,
            "mapping": "contiguous x64 argument setup -> actual qRegisterResourceData import/thunk; connected tree and name hashes checked",
            "sourcePresenceIsNotCompiledCacheActivationProof": True}


def inspect(path):
    path = qt.safe_path(path)
    data, archive_hash = qt.input_bytes(path)
    image = Image(data)
    try:
        literals = literal_references(image)
        boundaries = renderer_boundary(image, literals)
        selected_imports = [{"iatRva": rva, **entry} for rva, entry in sorted(image.imports.items())
                            if re.search(r"(?:opengl|gles|egl|vulkan|d3d|dxgi)", entry["module"], re.I)
                            or entry["symbol"] in ("GetProcAddress", "LoadLibraryW", "LoadLibraryA")]
        metadata = dict(image.metadata)
        metadata.pop("sections")
        return {"schema": "bsi.player-renderer-static-audit.v1", "input": path.name, "archiveSha256": archive_hash,
                "image": metadata, "normalAndDelayGraphicsOrLoaderImports": selected_imports,
                "exactAsciiAnchorEvidence": literals, "rendererBoundaries": boundaries,
                "startupInstructionEvidence": startup_evidence(image, literals, boundaries),
                "nativeCallbackBuilderEvidence": callback_builder_evidence(image, literals, boundaries),
                "registeredQml": registered_qml(image),
                "playerOrDllExecuted": False, "targetModified": False, "privateRendererAbiCalled": False,
                "runtimeRendererOrGameRouteProved": False, "shaderSupportCertified": False,
                "limits": ["Missing import/literal is not absent runtime functionality; dynamic or private dispatch may exist.",
                           "Unwind ranges and decoded witnesses are not complete source functions/control flow or runtime order.",
                           "A selected library basename is not a loaded-module path, version, installed identity or GL ownership.",
                           "EXE-only archives do not include the selected graphics provider implementations."]}
    finally:
        image.pe.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("players", nargs="+", type=Path)
    args = parser.parse_args()
    reports, failed = [], False
    for path in args.players:
        try:
            reports.append(inspect(path))
        except (AuditError, OSError, UnicodeError, struct.error, zlib.error, pefile.PEFormatError) as error:
            reports.append({"input": path.name, "error": str(error), "playerOrDllExecuted": False, "targetModified": False})
            failed = True
    print(json.dumps({"schema": "bsi.player-renderer-static-audit-batch.v1", "reports": reports}, indent=2))
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
