# Metaverse: Origins — Steam store art

Upload in Steamworks → Store Page → Graphical Assets. All files are opaque RGB PNGs at the exact Steamworks sizes (verified).

| Slot | File | Size |
|---|---|---|
| Header Capsule | `header_capsule_920x430.png` | 920×430 |
| Small Capsule | `small_capsule_462x174.png` | 462×174 |
| Main Capsule | `main_capsule_1232x706.png` | 1232×706 |
| Vertical Capsule | `vertical_capsule_748x896.png` | 748×896 |
| Page Background | `page_background_1438x810.png` | 1438×810 (ambient, no text, low contrast) |

Bonus, same Steamworks page:
- `legacy/` — the four old half-size slots that still read "OLD" (460×215, 231×87, 616×353, 374×448); exact halves of the new art.
- `library/` — library capsule 600×900, library hero 3840×1240 (no logo), library logo 1280 px wide (transparent).
- `wordmark_transparent.png` — the logo alone (2400 px, transparent).

## What it is
- **Art**: the game's own renders (Development build, `MO.NewGame 1029024224`, alpine-heath ridge, 16:47 sky time, partly cloudy, backlit golden hour), 3840×2160 `HighResShot`s, graded (warm highlights, kit-teal shadows, vignette, grain).
- **Logo**: "METAVERSE / ORIGINS", Windows' Sitka Display, in the UI kit's Field Ledger palette (parchment `#EEE7D7`, accent `#6CB8A9`/`#88CBBE`). It is the only text; no quotes, scores or awards. The small capsule's logo fills ~80 % of its width. The in-game wordmark is still a stand-in font, so when the kit's real Bitter display face lands, re-render `Tools/steam_wordmark.py` with it and re-run the build.

## Regenerate
```
python Tools/steam_wordmark.py          # wordmark
python Tools/steam_scout.py <name> [--hour H] [--weather P] [--cam cx cy cz tx ty tz fov] [--size 3840x2160]   # a plate, on the `ui` standalone instance (python Tools/ue.py inst start ui; inst do ui run "MO.NewGame 1029024224")
python Tools/steam_art_build.py         # composes everything into Saved/SteamArt/out (plates live in Saved/SteamArt/plates)
```
Plate recipe (pawn at (-517000, 421007, 32324) after `MO.WorldGen.FindBiomeSpot AlpineHeath 40 rough go`; `r.VolumetricRenderTarget 0` first, or the 4K clouds are blocky):
`S2_gold` cam (-517000,420700,34824) → target (-517000,391000,33624) fov 62 · `S4_gold` cam (…, +1200) → target z+2600 fov 70, yaw 255 · `S1_gold` low variant.
Sky traps: the weather bridge only syncs FORWARD in game time (SetTime backwards leaves the old sky), and takes ~60 s; `MO.Clock.SetTime <h> [m]`.
