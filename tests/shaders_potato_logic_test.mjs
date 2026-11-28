// Executes the REAL JS functions of src/shaders/BsiShadersSetting.qml in a Node
// VM context whose globals behave like QML's (Engine, draft, readonly
// textureDraftChanged), so the automatic Potato review is tested as behaviour
// instead of as text. Qt-free and GPU-free; the Qt tests remain authoritative
// for the controls themselves.
//
//   node tests/shaders_potato_logic_test.mjs
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import vm from "node:vm";
import { fileURLToPath } from "node:url";

const page = join(dirname(fileURLToPath(import.meta.url)), "../src/shaders/BsiShadersSetting.qml");
const source = readFileSync(page, "utf8");

function takeFunction(name) {
  const match = source.match(new RegExp("    function " + name + "\\([^)]*\\)\\s*\\{[\\s\\S]*?\\n    \\}"));
  assert.ok(match, "the page no longer declares " + name + "()");
  return match[0];
}

const saveBody = (source.match(/function save\(\)\s*\{[\s\S]*?\n    \}/) || [""])[0];
assert.ok(saveBody, "the page no longer declares save()");

// The page's own property, computed the same way.
const textureDraftChanged = `
Object.defineProperty(globalThis, "textureDraftChanged", { get: function () {
  return !!draft.potatoGraphic !== !!Engine.saved.potatoGraphic
    || textureStrength(draft) !== textureStrength(Engine.saved);
} });
`;

const saved = (overrides = {}) => ({
  enabled: false, potatoMode: 0, potatoGraphic: false,
  potatoStrength: 100, textureOverridesReviewed: false, saturation: 0, ...overrides,
});

// A v1/v2 file has no Potato keys, so the bridge reports the defaults; a v3 file
// can hold Potato on without the (previously manual) review flag.
const scenarios = [
  ["saved Potato already reviewed", saved({ potatoGraphic: true, textureOverridesReviewed: true })],
  ["saved Potato without the review flag (v3 file)", saved({ potatoGraphic: true })],
  ["Potato never configured (v1/v2 file)", saved()],
];

function build(initial) {
  const qsTr = (text) => ({
    raw: String(text), args: [],
    arg(value) { this.args.push(String(value)); return this; },
    toString() { let index = 0; return this.raw.replace(/%\d/g, () => this.args[index++] ?? ""); },
  });
  const context = vm.createContext({
    qsTr, qsTranslate: (_context, text) => text, Qt: { callLater: (fn) => fn() },
    // A QML-visible plain object, not a host-realm class instance. `controls`
    // mirrors the real bridge's visual-effect slider list.
    Engine: { saved: { ...initial }, controls: [{ key: "saturation", minimum: 0, maximum: 100 }],
              metrics: { textureDraws: 0, textureStatus: "" } },
    console,
  });
  vm.runInContext(
    `
    var draft = JSON.parse(JSON.stringify(Engine.saved));
    var keepCurrentDisplay = false, feedback = "", saveFailed = false, restartPrompt = null;
    var needsTextureReview = false;
    var revealed = [];
    function reveal(control) { revealed.push(control); }
    function closeRestartPrompt() { restartPrompt = null; }
    ${textureDraftChanged}
    ${takeFunction("textureStrength")}
    ${takeFunction("setValue")}
    ${takeFunction("differs")}
    ${takeFunction("visitPotatoOverrides")}
    ${takeFunction("potatoStatus")}
    ${takeFunction("fInitUi")}
    var SAVE_REVIEWS_DRAFT = ${/visitPotatoOverrides\(true\)/.test(saveBody)};
    var SAVE_REVIEWS_SAVED = ${/visitPotatoOverrides\(false\)/.test(saveBody)};
    `,
    context,
  );
  return context;
}

const read = (context, expression) => vm.runInContext(expression, context);

// 1. Opening the page reviews an already-saved Potato setting, never switches
//    Potato on, and never turns an unchanged draft dirty (Save stays disabled).
for (const [label, initial] of scenarios) {
  const page = build(initial);
  vm.runInContext("fInitUi()", page);
  assert.equal(read(page, "Engine.saved.potatoGraphic"), initial.potatoGraphic,
    `${label}: the automatic review must never change the saved Potato switch`);
  assert.equal(read(page, "!!draft.potatoGraphic"), initial.potatoGraphic,
    `${label}: opening the page must not change the Potato switch`);
  assert.equal(read(page, "differs()"), false, `${label}: opening must not create an unsaved change`);
  if (initial.potatoGraphic) {
    assert.equal(read(page, "!!draft.textureOverridesReviewed"), true,
      `${label}: a saved Potato setting is shown as already reviewed`);
  }
}

// 2. A first-ever save that turns Potato on must be reviewable, or the native
//    schema refuses the draft and Potato can never be enabled.
{
  const page = build(saved());
  vm.runInContext("fInitUi()", page);
  assert.equal(read(page, "draft.potatoGraphic"), false);
  vm.runInContext('setValue("potatoGraphic", true)', page);
  assert.equal(read(page, "draft.textureOverridesReviewed"), false,
    "the automatic review happens at Save, not while typing");
  assert.equal(read(page, "differs()"), true, "switching Potato on must mark the page dirty");
  // The helper itself must not turn Potato on when the SAVED switch is off.
  vm.runInContext("visitPotatoOverrides(true)", page);
  assert.equal(read(page, "Engine.saved.potatoGraphic"), false,
    "the automatic review must never flip the saved Potato setting");
  assert.equal(read(page, "draft.textureOverridesReviewed"), true,
    "Save must satisfy the schema's review requirement");
  assert.equal(read(page, "draft.potatoGraphic"), true, "the review must keep the user's Potato choice");
  assert.equal(read(page, "SAVE_REVIEWS_DRAFT"), true,
    "save() must apply the automatic review to the draft it is about to write");
}

// 3. An unrelated edit while Potato stays off must not fabricate a review.
{
  const page = build(saved());
  vm.runInContext("fInitUi()", page);
  vm.runInContext('setValue("saturation", 20)', page);
  vm.runInContext("visitPotatoOverrides(true)", page);
  assert.equal(read(page, "draft.textureOverridesReviewed"), false,
    "an unrelated edit must not stamp the driver review");
  assert.equal(read(page, "draft.potatoGraphic"), false, "an unrelated edit must not touch Potato");
}

// 4. The saved status explains the automatic check and never claims a measured
//    effect or reports a saved setting as unsaved.
{
  const page = build(saved({ potatoGraphic: true, textureOverridesReviewed: true }));
  vm.runInContext("fInitUi()", page);
  const text = String(read(page, "potatoStatus()"));
  assert.match(text, /automatic/i, "the status must describe the automatic driver check: " + text);
  assert.match(text, /not measured/i, "the status must not imply measured game pixels: " + text);
  assert.ok(!/not saved/i.test(text), "a saved setting must not report itself as unsaved: " + text);
}

// 5. Zero strength is honest about doing nothing.
{
  const page = build(saved({ potatoGraphic: true, textureOverridesReviewed: true, potatoStrength: 0 }));
  vm.runInContext("fInitUi()", page);
  assert.match(String(read(page, "potatoStatus()")), /native sampling/i,
    "0% strength must say native sampling");
}

console.log("PASS: the page's real JS applies the automatic Potato driver review on open and Save, "
  + "never enables Potato by itself, keeps an unchanged draft clean, and reports it truthfully");
console.log("LIMIT: runs the page's functions in a Node VM with a mocked Engine; Qt controls, "
  + "GPU pixels and the installed player are covered by the separate shader tests");
