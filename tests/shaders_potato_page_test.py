"""Potato graphic page contract: declared controls, no dangling ids, safe Save.

Regression for the shipped defect where the Potato switch, strength slider and
driver-review checkbox were removed from src/shaders/BsiShadersSetting.qml while
`save()` still called `reveal(potatoReview)` inside a Qt.callLater callback.
`potatoReview` no longer had an `id:`, so the first resolution-gated Save raised

    ReferenceError: potatoReview is not defined

and the page could not be saved. The page also silently dropped
`potatoGraphic`, `potatoStrength` and `textureOverridesReviewed` (no control wrote
them any more) even though the C++ runtime, the v5 preference schema and
Runtime::setTexture() still consume all three.

This is a source contract check: it needs no Qt, GPU, player or network, so it
runs with the other portable tests. The Qt-level behavioural tests remain
tests/shaders_plugin_ui_test.py and tests/shaders_native_settings_test.py.
"""
import re
from pathlib import Path

PAGE = Path(__file__).resolve().parents[1] / "src/shaders/BsiShadersSetting.qml"

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def main():
    source = PAGE.read_text(encoding="utf-8")
    ids = set(re.findall(r"^\s*id:\s*(\w+)", source, re.M))
    object_names = set(re.findall(r'objectName:\s*"([^"]+)"', source))

    # 1. Every reveal() target must exist. reveal(control) is reveal's own
    #    parameter, not a page id.
    reveal_targets = set(re.findall(r"\breveal\(\s*(\w+)\s*\)", source))
    for target in sorted(reveal_targets - {"control"}):
        check(target in ids, f"reveal({target}) refers to an id the page never declares")

    # 2. reveal() must tolerate an absent/undefined control instead of throwing
    #    out of its deferred callback.
    body = re.search(r"function reveal\(control\)\s*\{(.*?)\n    \}", source, re.S)
    check(body is not None, "reveal() is missing")
    if body:
        check("if (!control" in body.group(1), "reveal() must no-op on an absent control")

    # 3. The Potato controls the runtime settings need must be reachable.
    for name in ("shaderPotatoGraphic", "shaderPotatoStrength",
                 "shaderPotatoStrengthValue", "shaderPotatoTextureStatus"):
        check(name in object_names, f"Potato control is not in the page: {name}")

    # 4. Every config key the page writes must be a key the C++ schema accepts.
    accepted = {"enabled", "potatoMode", "potatoGraphic",
                "potatoStrength", "textureOverridesReviewed"}
    written = set(re.findall(r'setValue\(\s*"(\w+)"', source))
    for key in sorted(written - accepted):
        check(False, f"page writes '{key}', which the v5 preference schema does not accept")

    # 5. The three Potato keys must actually be writable again (they were not).
    for key in ("potatoGraphic", "potatoStrength", "textureOverridesReviewed"):
        check(key in written, f"no control writes '{key}' any more")

    # 6. Automatic mode replaces the manual driver-review checkbox. The schema
    #    still requires the flag for an enabled Potato, so the page must stamp it
    #    itself (visitPotatoOverrides) from fInitUi/save, never from a control.
    check("function visitPotatoOverrides" in source,
          "the automatic driver-override review helper is missing")
    for caller in ("fInitUi", "save"):
        block = re.search(r"function " + caller + r"\(\)\s*\{(.*?)\n    \}", source, re.S)
        check(block is not None, f"{caller}() is missing")
        if block:
            check(re.search(r"visitPotatoOverrides\((true|false)\)", block.group(1)),
                  f"{caller}() must apply the automatic driver-override review")
    check(not re.search(r'I reviewed driver LOD', source),
          "the manual driver-review checkbox should be gone in automatic mode")
    check("needsTextureReview: false" in source,
          "needsTextureReview must not re-introduce a manual save gate")

    # 7. Automatic mode must never enable Potato by itself.
    helper = re.search(r"function visitPotatoOverrides\((.*?)\)\s*\{(.*?)\n    \}", source, re.S)
    check(helper is not None, "visitPotatoOverrides() is missing its body")
    if helper:
        body = helper.group(2)
        check("Engine.saved.potatoGraphic" in body,
              "the automatic review must consider the SAVED Potato switch")
        check('setValue("potatoGraphic"' not in body,
              "the automatic review must never switch Potato on")
        # A first-ever save ("Off -> On") must also be reviewable, or the schema
        # would refuse the draft. That is why Save passes includeDraft = true,
        # while fInitUi passes false and can never fabricate a review.
        check("includeDraft" in body,
              "Save must be able to review a draft that is turning Potato on")
        save_block = re.search(r"function save\(\)\s*\{(.*?)\n    \}", source, re.S)
        if save_block:
            check("visitPotatoOverrides(true)" in save_block.group(1),
                  "save() must review the DRAFT Potato switch it is about to write")
        init_block = re.search(r"function fInitUi\(\)\s*\{(.*?)\n    \}", source, re.S)
        if init_block:
            check("visitPotatoOverrides(false)" in init_block.group(1),
                  "opening the page may only review an already SAVED Potato setting")

    # 8. No leftover reference to the deleted control.
    check("potatoReview" not in source, "a reference to the removed potatoReview id remains")

    if failures:
        print("FAIL: Potato graphic page contract")
        for message in failures:
            print("  - " + message)
        return 1
    print("PASS: Potato graphic switch/strength/status are declared, every reveal() target "
          "exists, reveal() tolerates an absent control, automatic driver review is wired, "
          "and the page only writes schema-accepted keys")
    print("LIMIT: source contract only; Qt behaviour, GPU pixels and the installed player "
          "are covered by the separate shader tests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
