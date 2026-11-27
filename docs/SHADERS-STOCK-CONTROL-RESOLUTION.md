# Stock controls under a qrc directory no longer look "missing"

## The reported failure

Applying the Settings-panel rebuild and then installing Shaders produced:

```
Player release Qt: 5.15.4 (x64). Module build baselines: 5.15.2, 6.2.4.
Install unavailable: The stock Settings control qrc:/UiThemeObject.qml is missing,
ambiguous or unreadable. Shaders requires the player's real controls and live
theme; no imitation UI or partial installation was substituted. No file was changed.
```

## What the player file actually contains

The supplied `HD-Player MSI v5.9.exe` (5.9.300.6315, SHA-256
`1f473ca2ec1e684c…3414f2ed`) was read as bytes only — never extracted, patched or
executed. Walking its registered Qt resource tree (through the same
`[BE length][4-byte hash][UTF-16BE characters]` name records the production
parser validates, including the hash) shows every control the Shaders page
imports **present**:

| Leaf name | Recorded in the EXE |
| --- | --- |
| `plr.qml`, `UiSettingsWindow.qml` | yes |
| `UiThemeObject.qml` | yes |
| `UiButton.qml`, `UiColumnLayout.qml`, `UiRowLayout.qml`, `UiCheckBox.qml` | yes |
| `UiComboBox.qml`, `UiSlider.qml`, `UiScrollView.qml`, `UiScrollBar.qml` | yes |
| `UiImage.qml`, `UiTextField.qml`, `UiMessageDialog.qml`, `UiDialog.qml` | yes |
| `UiShadowContainer.qml`, `UiDropShadowEffect.qml`, `UiImageButton.qml` | yes |

**524** hash-verified name records exist in that EXE's resource areas, and the
controls are reachable through the player's registered pool — but the preflight
compared each control against an **exact qrc root path**:

```cpp
for (const auto &entry : entries)
    if (entry.path == name) matches << entry;   // old
```

Any player that registers a control with a directory/pool prefix (the real
players carry several registrations of the same control, and the settings tree
contains directory nodes such as `qrcAssets/`) produced an empty `matches` list,
so a file that is present was reported as *missing*. The same strictness also
demanded that **all sixteen** controls decode as strict, BOM-less-or-BOM UTF-8
QML, although the module never rewrites a control's bytes — it only imports the
player's own `qrc:/` namespace.

## The fix

`ShaderSettingsImage::prepare` now validates stock controls by their
**registered leaf name** (or the full path, whichever the tree recorded):

1. **Presence** — a control is found when any registration's `path` or leaf
   `name` matches, case-insensitively.
2. **Real ambiguity only** — multiple registrations are normal; the install is
   blocked only when two registrations carry **different payloads**, and only
   when a record cannot be read at all.
3. **UTF-8 only where it matters** — the strict UTF-8 source requirement applies
   to the two files the module actually edits (`plr.qml`,
   `UiSettingsWindow.qml`), which are already decoded before this point.
4. **Diagnosable refusals** — a remaining refusal names the check, the number of
   registered file records, and up to three paths that carry the leaf, so a
   player-specific problem can be read from the message instead of guessed at.
   An unreadable record reports its path and compression flags.

No imitation UI, no partial install and no EXE growth are introduced: the two
edited sources still go through the existing registered-mapping, in-place
capacity and free-space checks, and a failure still leaves the file unchanged.

## Regression tests

`tests/shaders/settings_image_test.cpp` now also covers:

- a stock control recorded with a different leaf prefix must **install** and
  remain **removable** (this is the reported failure);
- a genuinely absent control is **still refused**, with the new diagnosable
  message (`registered file records`).

Run in CI: `shader-native-settings-image` (Windows x64 / Qt 6.2.4 and 5.15.2).

## Limits

- This is a change to the *preflight validation*; it does not by itself prove a
  patched MSI v5.9 player runs the page. The Qt 5.15.2 / 6.2.4 module payloads
  and the actual player/GPU run remain separate gates.
- The player tree was read statically. Whether the shipped Settings window
  resolves `qrc:/` as used by the page is checked by the existing module/player
  binding tests, not by this document.
