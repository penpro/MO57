# MO57 UI Kit — "Field Ledger" v1.0

Design canvas (all screens, component sheet, icon sheet, do/don't): the "MO57 UI Design System" Design artifact. Every screen board has a **Spec overlay** tweak that draws the pixel rects and gamepad focus order.

## What's in this folder

| Path | What it is |
|---|---|
| `tokens.json` | Flat, namespaced tokens (199 keys) + `colorblind` override block + `textScale` notes. `type.*.umgSize` = px × 0.75 for UMG. |
| `tokens_datatable.csv` | Same tokens as a DataTable-importable CSV (`Name, Value, Kind, Notes`). |
| `contrast_report.json` | 39 computed WCAG pairs (all pass). Body text on panels is AAA; HUD text over pure-white snow is 9.4:1. |
| `nineslice/1x`, `nineslice/2x` | 21 white-on-transparent 9-slice textures (`T_UI_*.png`). Tint from tokens. |
| `nineslice/manifest.json` | Per file: size, slice margins in px **and** as UMG 0–1 Margin, draw-as, usage and token. |
| `nineslice/baked_preview/` | 24 pre-tinted state renders (button/panel/input/slot/etc.) for reference only. |
| `icons/svg`, `icons/png128` | 94 icons, white on transparent: 19 moodles, 10 item types, 5 severity shapes, 39 UI glyphs, wind arrow + ring, 19 mouse/gamepad glyphs. |
| `icons/manifest.json` | Groups, file names and the drawing rule. |
| `screens/*.json`, `screens/*.md` | Per-screen specs (11 screens): element id, suggested widget, rect `[x,y,w,h]` at 1920×1080, anchor, tokens, focus order. |
| `_source/` | The generator scripts (tokens, icons, 9-slice, specs). Edit a value, re-run, everything regenerates. |

## Fonts (all SIL Open Font License 1.1 — free for commercial games, embedding allowed)

| Role | Family | Files to import | Get it |
|---|---|---|---|
| Display / titles / headings | Bitter | `Bitter-SemiBold.ttf` (600), `Bitter-Bold.ttf` (700) | fonts.google.com/specimen/Bitter |
| UI / body / labels / HUD | IBM Plex Sans | `IBMPlexSans-Regular.ttf` (400), `IBMPlexSans-SemiBold.ttf` (600) | fonts.google.com/specimen/IBM+Plex+Sans · github.com/IBM/plex |
| Numbers (timers, weights, vitals) | IBM Plex Mono | `IBMPlexMono-Medium.ttf` (500) | fonts.google.com/specimen/IBM+Plex+Mono |

Use the static TTFs from the download's `static/` folder, not the variable font. Keep each `OFL.txt` beside the fonts in your build. Numbers use Plex Mono because UMG has no tabular-figures switch.

## Decisions made instead of asking (change any of these and the tokens follow)

1. **One accent: verdigris `#6CB8A9`.** The only cool hue in a warm system, so it can't be mistaken for any severity. Focus is a separate tallow ring (`#F3E2B3`), so hover, focus and selected never look alike.
2. **Menus are dark iron + leather; paper appears only where a person would write** (medical chart, journal, knowledge). Paper has its own darker severity inks (`color.paper.severity.*`).
3. **Every 9-slice is a white mask, layered fill/stroke/shadow**, so states are tint changes, never texture swaps.
4. **Gamepad glyphs are generic and positional** (no platform letters, colours or shapes). Use Steam Input's origin glyphs at runtime where available.
5. **No red-cross or biohazard emblems.** Medical = bandage roll, infection = organism glyph.
6. **Menus never pause.** Every menu header shows the live in-game clock, and the in-game menu leads with a ticking "world continues" readout.
7. **Screen spec schema is neutral** (`screens/*.json`), since the `ue.py ui build` spec format wasn't available. Map `widget`/`rect`/`anchor`/`tokens`/`focus` onto it, or send me one existing spec and I'll convert all 11.

## Implementation order

1. **Tokens + fonts.** Import `tokens_datatable.csv` → `DT_UITokens`, write a small `UMOUITokenLibrary` (`Color(Name)`, `Float(Name)`), make Composite Fonts and one `CommonTextStyle` per type token.
2. **Button, panel, list row, focus ring, icon.** `WBP_MOButton` (5 CommonButtonStyles), `WBP_MOPanel`, `WBP_ListRow`, `WBP_FocusRing`, `WBP_Icon`. Swapping these into the 88 WBPs changes every menu at once.
3. **HUD.** Plates + outline, moodle strip, prompt, timed action, notifications.
4. **Inventory.** 5. **Crafting/Building.** 6. **Character status.** 7. **Form controls + Options.** 8. **Menus, colony, journal, modals.** 9. **Accessibility switches** (all token swaps).

## Accessibility summary

- Text scale 100 / 125 / 150%: multiplies `type.*`, `control.height.*`, `row.height`. Layouts reflow and never clip.
- Reduced motion: `motion.base/slow` → 0; the critical pulse becomes a static double border.
- Colour-blind: swap `color.severity.*` with the `colorblind` block. Severity always has a shape too (circle-check / diamond / triangle / octagon / square).
- High-contrast HUD: plate opacity 0.82 → 0.95, secondary text → primary.
