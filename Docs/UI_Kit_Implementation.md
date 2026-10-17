# UI kit ("Field Ledger") implementation

Source of truth: `DesignKit/MO57_UI_Kit/MO57_UI_Kit/` (made by Claude Design from `Docs/Archive/UI_Design_Brief_Prompt.md`): `tokens.json`, `nineslice/` (white-mask 9-slices + manifest), `icons/`, `screens/*.md|json` (11 per-screen
specs at 1920x1080), `_source/mo57_web_mirror.css` (the exact state table for buttons / rows / tabs / inputs / slots / HUD pills: read it before building a component). This doc is how the kit lives in the game.

## Architecture (one chokepoint per concern)

| Concern | Where | Notes |
|---|---|---|
| Design tokens (colour, number, string) | `DT_UITokens` (`/MOFramework/UI/Kit/`), read by `UMOUITokens` (`MOUITokens.h/.cpp`) | The kit CSV + `nineslice.<piece>.margin/.draw` rows + `font.face.*` / `font.asset.*` rows, all written by `Content/Python/kit_import.py`. Project Settings > MO UI Theme (`TokenTable`, in `Config/DefaultGame.ini`). Missing token = loud magenta + one log line. |
| Accessibility | `UMOUITokens::SetAccessibility(colorBlind, highContrast, reducedMotion, textScale)` | Token SWAPS (colour-blind block, plate opacity, motion -> 0, text scale on type/control/row sizes); pure resolver `FMOUITokenSet`, tests `MOFramework.UITokens.*`. Not yet wired to the Options panel. |
| Runtime colours / icons in C++ | `UMOUITokens::Color / Number / Font / Icon / NineSliceBrush` | Moodle severity tint + shape + pulsing critical border, notification types, mode indicator, reticle (+ dark outline), breath meter (segmented bar on a HUD plate): all token driven. |
| Widget generation | `Content/Python/kit_lib.py` (colour/font/brush/node helpers) + `ui_specs/kit_*.py` | Specs call `K.color("color.accent")`, never a number. Hex colours are sRGB; UMG wants linear: `kit_lib.color()` converts (`mo_ui._color` does NOT). |
| Buttons | `WBP_MOCommonButton` + `BS_Primary/Secondary/Tertiary/Danger[Lg]` (CommonButtonStyle BPs) + `TS_Btn_*` text styles | Look = one baked 9-slice per state (`Tools/kit_bake.py` -> `Saved/KitBake/Buttons` -> `kit_import.py`). `Content/Python/kit_styles.py` (re)creates the styles and re-points the legacy `MOCommonButtonStyle` (every MOButton) at the secondary look. Focus ring (kit `focus_ring`) shows for keyboard / gamepad focus only (`UMOCommonButton::FocusRing`). |
| Fonts | `font.face.<Family>.<Weight>` tokens -> composite `F_Bitter / F_IBMPlexSans / F_IBMPlexMono` (typefaces `W<weight>`), built by `UMOEditorContentUtils::MakeCompositeFont` | **Stand-ins (Roboto / Droid Sans Mono)** until the kit's OFL fonts are imported (5 TTFs: Bitter 600/700, IBM Plex Sans 400/600, IBM Plex Mono 500; a download needs Wes's yes). Swap = import the TTFs (FontFace assets), change the paths in `kit_lib.FACES`, re-run `kit_import.py`. |
| Component sheet | `WBP_KitGallery` (`ui_specs/kit_gallery.py`), `MO.UI.Show /MOFramework/UI/Kit/WBP_KitGallery.WBP_KitGallery_C` | Every token, type style, button variant, piece and icon on one sheet: the thing to LOOK at after any kit change. |

## Workflow

```bash
python Tools/kit_bake.py                                   # button state textures from tokens
python Tools/ue.py py --file Content/Python/kit_import.py  # textures, icons, tokens DT, composite fonts (editor open, not in PIE)
python Tools/ue.py py --file Content/Python/kit_styles.py  # text / button styles
python Tools/ue.py ui build Content/Python/ui_specs/kit_gallery.py
python Tools/ue.py inst start ui --nosteam --res 1920x1080 --pos 0,0          # a standalone game window (uncooked content, ~70 s)
python Tools/ue.py inst do ui run "MO.NewGame 7"                              # only for in-game menus; the gallery works from the intro level
python Tools/ue.py inst do ui run "MO.UI.Show /MOFramework/UI/Kit/WBP_KitGallery.WBP_KitGallery_C"     # or MO.UI.Open Crafting ...
powershell -File Tools/win_input.ps1 -ProcessId <pid from %TEMP%/claude/inst_ui/pid.txt> -Action shot -Out shot.png    # 2880x1620 window, the game is the top-left 1920x1080
python Tools/ue.py inst stop ui
```

## Traps found (each one cost a diagnosis)

- **A running `inst` process locks the .uasset files it loaded**: `save_loaded_asset` then fails with only "1 warning saving asset" in the log. Stop the instance before any `ui build` / style script, and look at the file's mtime.
- **`EditorAssetLibrary.save_loaded_asset` skips a clean package by default** and a class-default (CDO) change does not dirty it: `mo_ui.build` now saves with `only_if_is_dirty=False`. The Python editor also caches imported modules: after editing `mo_ui.py` use `importlib.reload(mo_ui)`.
- **`WBP_MOCommonButton` had a class-default Color and Opacity of 0.356** (a leftover), which multiplied every button's brush AND label to a dim grey. It is 1.0 now; an audit of all WBPs found no other tinted class default.
- **Slate only resolves fonts through `UFont`** (`IFontProviderInterface`): a bare `UFontFace` as `FSlateFontInfo::FontObject` renders tofu boxes. Composite fonts can only be authored from C++ (Python cannot reach `FCompositeFont`).
- A WBP whose graph owns a variable (the button's `ButtonText` as a Text object) cannot have that widget swapped for a CommonTextBlock (compile error): the C++ button pushes the style's font and colour into whatever text block it finds.
- `ui build` / the contract check accept a plain engine parent (`UUserWidget`) now (`ENGINE_BASES_NO_CONTRACT`).
- Python heredocs containing Windows paths with `\U...` crash the interpreter: write such files with the editor tools, not `python - <<EOF`.

## Screen status

| Screen | Spec | Status |
|---|---|---|
| Tokens, fonts (stand-in), buttons, icons, 9-slice kit, gallery | - | DONE, verified in the gallery (button fills pixel-equal to the tokens) |
| HUD (moodles, prompt, timed action, notifications, tutorial banner, quest tracker, reticle, breath, wind, colony bar) | `screens/hud.md` | C++ classes token driven; WBP layouts NOT yet rebuilt |
| Inventory | `screens/inventory.md` | pending |
| Crafting / Building | `screens/crafting.md` | pending (recipe-row colours already neutral) |
| Character status (medical) | `screens/medical.md` | pending |
| Options | `screens/options.md` | pending |
| Main menu / In-game menu | `screens/main_menu.md`, `ingame_menu.md` | pending |
| Colony / possession | `screens/colony.md` | pending |
| Skills / Quests | `screens/skills.md`, `quests.md` | pending |
| Modals | `screens/modals.md` | pending |
