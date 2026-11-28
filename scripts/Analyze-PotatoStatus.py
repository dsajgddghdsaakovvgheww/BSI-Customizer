#!/usr/bin/env python3
"""Read a Potato status token and say why the effect did or did not apply.

The app writes the token itself (`BSI_SHADERS_STATUS_TOKEN` or
`<settings>.json.token`) from the same 1-second tick that refreshes its UI, so
this reads a machine-produced record instead of a hand-pasted report.

This is a CLASSIFIER, not a proof of pixels: it never claims that a non-zero
binding count looks good on screen, and it never turns a missing token into a
failure of the product.

Usage:
    python scripts/Analyze-PotatoStatus.py --token status.json
    python scripts/Analyze-PotatoStatus.py --token status.json --json report.json

Exit codes: 0 = a verdict was reached (including "no effect yet"),
            2 = the token is missing/unreadable/has no counters.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

# Each verdict is one sentence a user can act on. Ordered by how early the
# product stops, so the first matching blocker is the one to report.
VERDICTS = (
    ("not_armed",
     "Potato graphic is off or its strength is 0. Nothing is expected to change.",
     ("potatoGraphic", "potatoStrength")),
    ("not_inspected",
     "Potato is armed but never inspected a draw. The game's rendering was not "
     "reached at all (wrong API/route selection, player not presenting, or the "
     "game never drew while the setting was active).",
     ("textureDrawInspected",)),
    ("no_eligible_sampler",
     "Draws were inspected but every sampler was refused, so no private sampler "
     "was bound. The 'Mip first blockers' line names the predicate that refused.",
     ("textureDrawInspected",)),
    ("prepared",
     "Potato prepared private samplers for at least one draw. That is a binding "
     "count, not a measured pixel change: verify in game.",
     ("texturePreparedBindings",)),
)


def load(path: pathlib.Path) -> dict:
    raw = path.read_text(encoding="utf-8")
    try:
        token = json.loads(raw)
    except json.JSONDecodeError as error:
        raise SystemExit(f"{path} is not the JSON token the app writes: {error}") from error
    if not isinstance(token, dict):
        raise SystemExit(f"{path} does not contain a JSON object")
    return token


def number(token: dict, key: str) -> int | None:
    value = token.get(key)
    if value is None:
        return None
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, (int, float)):
        return int(value)
    match = re.fullmatch(r"\s*(-?\d+)\s*", str(value))
    return int(match.group(1)) if match else None


def blocker_line(token: dict) -> str | None:
    detail = token.get("detail") or ""
    for line in str(detail).splitlines():
        if "Mip first blockers" in line:
            return line.strip()
    return None


def frame_pass(token: dict) -> dict | None:
    """What the presentation pass itself did, when the token even says.

    `processedFrames` only increments when the production pass rendered on the
    identified game surface, so it is the in-game answer to "is my picture being
    changed" — not the texture-eligibility path below it.
    """
    if "processedFrames" not in token:
        return None
    processed = number(token, "processedFrames") or 0
    smoothness = number(token, "frameSmoothness")
    enabled = token.get("framePassEnabled")
    neutral = token.get("framePassNeutral")
    result = {"processedFrames": processed, "frameSmoothness": smoothness,
              "framePassEnabled": enabled, "framePassNeutral": neutral}
    if processed > 0 and smoothness:
        result["key"] = "frame_active"
        result["summary"] = (
            f"The frame pass rendered {processed} game frame(s) on the identified game "
            f"surface with Smoothness at {smoothness}. The picture you saw was processed; "
            "compare it against 0 to see the difference for yourself.")
    elif processed > 0:
        result["key"] = "frame_active_not_smoothing"
        result["summary"] = (
            f"The frame pass rendered {processed} game frame(s), but Smoothness is "
            f"{'0' if smoothness in (None, 0) else smoothness}. The pass works; raise "
            "Smoothness above 0 to change the picture.")
    elif neutral is True or enabled is False:
        result["key"] = "frame_off"
        result["summary"] = (
            "The post-process pass is off/neutral, so nothing is drawn over the game "
            "frame. No frame change is expected in this state.")
    else:
        result["key"] = "frame_bypassed"
        result["summary"] = (
            "The pass is enabled but has not rendered a frame on an identified game "
            "surface. The token's status line says why; a DirectX/Vulkan-only game still "
            "keeps its original frames.")
    return result


def classify(token: dict) -> dict:
    armed = bool(token.get("potatoGraphic")) and (number(token, "potatoStrength") or 0) > 0
    inspected = number(token, "textureDrawInspected") or 0
    prepared = number(token, "texturePreparedBindings") or 0
    result = {"armed": armed, "inspected": inspected, "preparedBindings": prepared,
              "firstBlocker": blocker_line(token)}
    if not armed:
        key, text = "not_armed", VERDICTS[0][1]
    elif inspected == 0:
        key, text = "not_inspected", VERDICTS[1][1]
    elif prepared == 0:
        key, text = "no_eligible_sampler", VERDICTS[2][1]
    else:
        key, text = "prepared", VERDICTS[3][1]
    result["verdict"] = key
    result["summary"] = text
    surface = frame_pass(token)
    if surface is not None:
        result["framePass"] = surface
    # The single most useful extra fact for each zero path.
    if key == "no_eligible_sampler":
        line = result["firstBlocker"] or ""
        if "non-mip-filter" in line:
            result["next"] = ("This build still records the old fatal label "
                              "'non-mip-filter=', so it predates the base-only "
                              "repair. Update before drawing conclusions.")
        elif "base-only-filter=" in line and "single-level=0" in line:
            result["next"] = ("Every refused source was a stored chain sampled "
                              "base-only: that path is accepted when the plan can "
                              "reach a stored level. Check the strength and the "
                              "descriptor refusals in the same line.")
        elif re.search(r"single-level=[1-9]", line):
            result["next"] = ("The bound textures report a single level. No filter "
                              "change can make a coarser image for them; this is a "
                              "resource-shape answer, not a Potato setting.")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--token", required=True, type=pathlib.Path)
    parser.add_argument("--json", type=pathlib.Path, help="write the verdict here")
    args = parser.parse_args()

    if not args.token.is_file():
        print(f"NO TOKEN: {args.token} does not exist. The app writes it about one "
              "second after the Shaders page first evaluated its status; a run that "
              "never got that far leaves none. Not a product failure on its own.")
        return 2
    token = load(args.token)
    if not any(key.startswith("texture") for key in token):
        print(f"NO COUNTERS: {args.token} has no texture fields. Write it with a "
              "build that includes the status-token bridge.")
        return 2

    verdict = classify(token)
    # The frame pass is the answer to "did my picture change"; the texture verdict
    # below it explains the separate mip-sampling path. Never merge the two.
    surface = verdict.get("framePass")
    if surface:
        print(f"FRAME PASS: {surface['key']}")
        print(f"  {surface['summary']}")
    else:
        print("FRAME PASS: unknown")
        print("  This token predates the frame-pass fields. Update to a build that "
              "reports them; nothing here can say whether the frame was processed.")
    print(f"TEXTURE VERDICT: {verdict['verdict']}")
    print(f"  {verdict['summary']}")
    print(f"  armed={verdict['armed']} inspected={verdict['inspected']} "
          f"preparedBindings={verdict['preparedBindings']}")
    if verdict.get("firstBlocker"):
        print(f"  first blocker: {verdict['firstBlocker']}")
    if verdict.get("next"):
        print(f"  next: {verdict['next']}")
    if args.json:
        args.json.write_text(json.dumps(verdict, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
