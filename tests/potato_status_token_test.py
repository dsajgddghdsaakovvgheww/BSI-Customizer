#!/usr/bin/env python3
"""Contract test for the Potato status-token classifier and its writer.

Two independent halves:

* The classifier (`scripts/Analyze-PotatoStatus.py`) must reach the same verdict
  as the product's own counters. Synthetic tokens exercise every zero path, so a
  later status-format change cannot quietly make the tool report "prepared".
* The writer (`ShaderBridge::writeStatusToken`) is checked at source level: it
  must be driven by the existing 1-second UI tick, must write on change only, and
  must not gain a GL call, an upload or a game-content capture. That is a source
  contract, NOT a compiled-Qt test; the Qt build in CI proves it links.
"""
import json
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOL = ROOT / "scripts" / "Analyze-PotatoStatus.py"
BRIDGE = ROOT / "src" / "shaders" / "ShaderBridge.cpp"
HEADER = ROOT / "src" / "shaders" / "ShaderBridge.h"


def token(**overrides):
    base = {"schema": 1, "revision": "test", "potatoGraphic": True, "potatoStrength": 100,
            "textureDrawInspected": 0, "texturePreparedBindings": 0,
            "detail": "Texture inspection: armed; inspected calls: 0"}
    base.update(overrides)
    return base


def blocker(single_level, base_only=1, extra=""):
    return ("Texture inspection: armed; inspected calls: 7206\n"
            "Mip first blockers: protected=0; range=0; dimensions=0; format=0; "
            f"incomplete-required-level=0; single-level={single_level}; "
            f"storage-metadata=0; base-only-filter={base_only}{extra}")


def analyze(payload, missing=False):
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / "token.json"
        if not missing:
            path.write_text(json.dumps(payload), encoding="utf-8")
        result = subprocess.run([sys.executable, str(TOOL), "--token", str(path)],
                                capture_output=True, text=True)
        verdict = pathlib.Path(folder) / "verdict.json"
        if result.returncode == 0:
            subprocess.run([sys.executable, str(TOOL), "--token", str(path), "--json", str(verdict)],
                           capture_output=True, text=True)
            return result, json.loads(verdict.read_text(encoding="utf-8")) if verdict.exists() else None
        return result, None


def main():
    # 1. The tool never equates a binding count with a visible result.
    source = TOOL.read_text(encoding="utf-8")
    assert "That is a binding " in source and "count, not a measured pixel change" in source, \
        "the tool must refuse to call prepared bindings a visible result"

    # 2. Every early stop has its own verdict.
    result, verdict = analyze(token(potatoGraphic=False))
    assert result.returncode == 0 and verdict["verdict"] == "not_armed", result.stdout + result.stderr

    result, verdict = analyze(token(potatoStrength=0))
    assert result.returncode == 0 and verdict["verdict"] == "not_armed", result.stdout + result.stderr

    result, verdict = analyze(token(textureDrawInspected=0))
    assert result.returncode == 0 and verdict["verdict"] == "not_inspected", result.stdout + result.stderr

    # 3. An inspected-but-refused draw is "no eligible sampler", and the reason is
    #    quoted from the product's own line rather than guessed.
    result, verdict = analyze(token(textureDrawInspected=7206, detail=blocker(2)))
    assert result.returncode == 0 and verdict["verdict"] == "no_eligible_sampler", result.stdout + result.stderr
    assert verdict["firstBlocker"] and "single-level=2" in verdict["firstBlocker"]
    assert "single level" in verdict["next"], verdict["next"]

    result, verdict = analyze(token(textureDrawInspected=7206, detail=blocker(0)))
    assert verdict["verdict"] == "no_eligible_sampler"
    assert "stored chain sampled base-only" in verdict["next"], verdict["next"]

    # A pre-repair build must be named, not silently accepted as a new result.
    result, verdict = analyze(token(textureDrawInspected=7206, detail=blocker(0, extra="; non-mip-filter=1")))
    assert "predates the base-only repair" in (verdict.get("next") or ""), verdict

    # 4. A non-zero binding count is reported as prepared, still not as success.
    result, verdict = analyze(token(textureDrawInspected=7206, texturePreparedBindings=12))
    assert result.returncode == 0 and verdict["verdict"] == "prepared", result.stdout + result.stderr

    # 5. A missing or counter-free token is an absent measurement, never a
    #    product failure: distinct exit code and an explicit message.
    result, _ = analyze({}, missing=True)
    assert result.returncode == 2 and "NO TOKEN" in result.stdout, result.stdout
    result, _ = analyze({"schema": 1, "detail": "nothing"})
    assert result.returncode == 2 and "NO COUNTERS" in result.stdout, result.stdout

    # 6. The writer is a status re-serialization, not a new inspection path.
    bridge = BRIDGE.read_text(encoding="utf-8")
    tick = re.search(r"connect\(timer,&QTimer::timeout,this,\[this\]\{(.*?)\}\);", bridge, re.S)
    assert tick, "the status token must be written from the existing 1-second tick"
    assert "writeStatusToken();" in tick.group(1), tick.group(1)
    writer = re.search(r"void ShaderBridge::writeStatusToken\(\)\{(.*?)\n\}", bridge, re.S)
    assert writer, "writeStatusToken must exist"
    body = writer.group(1)
    for forbidden in ("glGet", "GetIntegerv", "http", "QNetwork", "QHttp", "upload", "QScreen"):
        assert forbidden not in body, f"the status token must not perform {forbidden}"
    assert "block==s.lastToken" in body, "the token is written on change, not every second"
    assert "QIODevice::WriteOnly|QIODevice::Truncate" in body
    assert "BSI_SHADERS_STATUS_TOKEN" in bridge, "a harness must be able to name the token path"

    # 7. The bridge is ALSO built against the oldest supported player SDK
    #    (Qt 5.15.2), so it must not use emplace-style QJsonObject construction:
    #    a newer-Qt-only form broke the release payload build once already.
    assert "QJsonObject token{" not in bridge, \
        "build the token with insert(), not initializer-list emplace: the bridge targets Qt 5.15.2 too"
    assert "#include <QtGlobal>" in bridge, "qEnvironmentVariable needs QtGlobal on Qt 5"
    assert "writeStatusToken" in HEADER.read_text(encoding="utf-8"), "declared in the header"

    # 8. The shared constant block must hold every effect weight AND the packed
    #    texel pair without overlap. Adding a control once moved the pair into a
    #    weight slot, which would have made DirectX read a weight as a texel size.
    params = (ROOT / "src" / "shaders" / "Parameters.h").read_text(encoding="utf-8")
    count = int(re.search(r"std::array<Control, (\d+)>", params).group(1))
    hlsl = (ROOT / "src" / "shaders" / "HlslSources.h").read_text(encoding="utf-8")
    d3d = (ROOT / "src" / "shaders" / "D3d11Pipeline.cpp").read_text(encoding="utf-8")
    vulkan = (ROOT / "src" / "shaders" / "VulkanPipeline.cpp").read_text(encoding="utf-8")
    push = re.search(r"float weights\[(\d+)\];vec2 texel", hlsl.replace("\n", " ") + (ROOT / "src" / "shaders" / "ShaderSources.h").read_text(encoding="utf-8"))
    assert push and int(push.group(1)) == count, "the push constant must declare every weight"
    slots = int(re.search(r"float4 packed\[(\d+)\]", hlsl).group(1)) * 4
    texel_slot = re.search(r"#define texel packed\[(\d+)\]\.zw", hlsl)
    assert texel_slot, "texel must stay a named packed slot, not a literal index"
    pair_start = int(texel_slot.group(1)) * 4 + 2
    assert count <= pair_start, (
        f"{count} effect weights would alias the texel pair at {pair_start}: widen the block")
    assert slots >= pair_start + 2, f"{slots} constant slots cannot hold the texel pair at {pair_start}"
    upload = re.search(r"std::array<float,(\d+)> constants", d3d)
    assert upload and int(upload.group(1)) == slots, (
        f"the DirectX upload is {upload.group(1) if upload else 'missing'} floats but the shader reads {slots}")
    vk_upload = re.search(r"std::array<float,(\d+)> constants", vulkan)
    vk_pair = re.search(r"constants\[(\d+)\]=\d\.f/p\.target\.extent\.width", vulkan)
    assert vk_upload and vk_pair and int(vk_upload.group(1)) == 4 * int(re.search(r"float4 packed\[(\d+)\]", hlsl).group(1)), (
        "the Vulkan push constant must use the same slot count as DirectX")
    assert count <= int(vk_pair.group(1)), (
        f"{count} weights would alias the Vulkan texel pair at {vk_pair.group(1)}")
    buffer_bytes = re.search(r"ByteWidth=(\d+)", d3d)
    assert buffer_bytes and int(buffer_bytes.group(1)) >= slots * 4, (
        "the DirectX constant buffer must be at least as large as the shader's block")

    print("PASS: Potato status-token classifier reaches one verdict per real early stop "
          "(off, never inspected, no eligible sampler, prepared), quotes the product's own "
          "first blocker instead of guessing, never sells a binding count as visible pixels, "
          "treats a missing token as an absent measurement, and the writer stays on the "
          "existing 1-second UI tick with no GL query, no upload and no game content; "
          "every effect weight fits the constant block without aliasing the texel pair")
    print("LIMIT: classifier contract plus a source-level writer contract; the Qt build in "
          "CI is what proves the bridge still compiles and links")
    return 0


if __name__ == "__main__":
    sys.exit(main())
