# Why "Inspect compatibility" said 0 of 147 records carried `UiThemeObject.qml`

A refusal in the Shaders preflight:

```
The stock Settings control qrc:/UiThemeObject.qml is missing from the player's registered resources
(147 registered file records; 0 carry that leaf: none). ... No file was changed.
```

was read for a long time as a *name matching* problem. It was not. It was a
resource-pool **enumeration** problem, and it is reproducible on a shipped
player build.

## The player

An older BlueStacks 5.12-era MSI player (`HD-Player.exe`, ~13 MB, Qt5,
link-time resource pools) registers **three** compiled-in resource pools through
`qRegisterResourceData`. Their roles are:

| pool | records | contents |
| --- | --- | --- |
| 1 | 130 | `plr.qml`, `UiSettingsWindow.qml`, every `Plr*Setting.qml`, the `Ui*` control library |
| 2 | 17 | helpers |
| 3 | 424 reachable, **0 listable** | the large `qrcAssets/` image set |

The strict walk listed pool 1 and pool 2 and returned **nothing** for pool 3,
which is where 147 = 130 + 17 came from in the refusal.

## The damage

Pool 3's name table was partly destroyed after linking. A compressed copy of the
window overlaps it, so 54 of its 484 leaf name records are a valid
`length | hash` word followed by zero characters:

```
13 00 8a 21 07 | 00 00 00 ...      (length 19, hash 0x08a2107, then zeroes)
```

Qt still loads those files: the player's own resource loader reaches them through
the data table, and the name records it needs are intact. All sixteen controls
the Shaders page imports (`UiThemeObject.qml`, `UiButton.qml`, `UiSlider.qml`,
`UiMessageDialog.qml`, …) sit among the 430 readable records.

## Why it was fatal

`QtResources::filesInPool()` is **all-or-nothing on purpose**. It exists to
validate *one candidate mapping* just before an edit, so a single node that does
not parse must make the whole candidate unusable. `registeredFiles()` used that
strict walk for enumeration and simply skipped a pool whose walk failed:

```cpp
for (const auto &file : filesInPool(data, pool)) { ... }   // pool failed => pool vanished
```

So one unreadable asset hid 430 readable controls, and the preflight honestly
reported "0 carry that leaf" while looking at an incomplete list.

## The fix

Enumeration keeps the strict walk as the first choice (every other player build
still enumerates identically) and falls back to `QtResources::reachableFiles()`
when the pool cannot be validated as a whole. That fallback:

* walks the same child-index tree, so `File::path` means exactly what it meant before;
* applies the same per-record tests — verified name hash, mapped payload, decoded
  size inside its section, record inside the tree's own span;
* **skips** an unreadable node instead of refusing the pool;
* never invents a name, so a destroyed record is absent rather than guessed.

Editing is unchanged: `findLink()` still validates the complete tree of the
resource being rewritten, and a payload it cannot validate is refused.

A refusal also now lists what every pool did, for example:

```
Resource pools: pool at 0x70ab70 (v3, call-site registration): a name record or payload
in the tree is unreadable, 430 of 484 reachable records listed; ...
```

## Evidence

| build | records before | records after | controls |
| --- | --- | --- | --- |
| BlueStacks 5.9 | 550 | 550 | all present |
| BlueStacks 5.10 | 582 | 582 | all present |
| MSI latest | 736 | 736 | all present |
| BlueStacks latest | 880 | 880 | all present |
| 5.12-era build in question | **147** | **577** | **all 16, one readable payload each** |

The 5.12 build's `plr.qml` (55 664 bytes) and `UiSettingsWindow.qml`
(13 673 bytes) both decode and still carry the ids the integration edits
(`iAppwin`, `fOpenSettingsWindow`, `fChangeSelectedSetting`,
`fSwitchSettingPanel`), so the next step of the installation is reachable.

## Regression test

`tests/shaders/settings_image_fixture.h` gains `splitFixture()`: the panel QML in
one pool, the control library in another, one name record in the control pool
zeroed after its length/hash word — the exact damage shape. The test asserts:

* an installation against that image succeeds (it failed before the fix);
* removal on an uninstalled image stays a pure no-op;
* a control that is really missing is still refused with the same diagnosable
  message, now including the per-pool report;
* the unreadable record is skipped, never renamed into a guess.
