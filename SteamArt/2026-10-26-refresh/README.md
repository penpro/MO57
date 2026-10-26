# Metaverse: Origins — Steam store refresh, 26 October 2026

Eight upload images are ready in `exports/`: five store graphics and three gameplay screenshots. `preview.png` shows the whole set; open `review.html` for clickable full-size files. No Steam upload or publication has been performed.

## Upload map

| Steam slot | File under exports/ | Exact size |
|---|---|---|
| Header Capsule | header_capsule_920x430.png | 920 × 430 |
| Small Capsule | small_capsule_462x174.png | 462 × 174 |
| Main Capsule | main_capsule_1232x706.png | 1232 × 706 |
| Vertical Capsule | vertical_capsule_748x896.png | 748 × 896 |
| Page Background | page_background_1438x810.png | 1438 × 810 |
| Screenshot 1 | screenshots/01_cliffs_1920x1080.png | 1920 × 1080 |
| Screenshot 2 | screenshots/02_beach_1920x1080.png | 1920 × 1080 |
| Screenshot 3 | screenshots/03_night_1920x1080.png | 1920 × 1080 |

Upload the first five to the matching slots in the intended game's Store Page → Graphical Assets; add the last three to Screenshots. The larger capsule files replace the old smaller assets, with Steam generating smaller versions. The contact sheet, originals, native masters, script and manifest are review/source files, not upload graphics.

Steam's current [store asset specification](https://partner.steamgames.com/doc/store/assets/standard) matches all five requested sizes and specifies 1920 × 1080 minimum, 16:9 screenshots. It requires at least **five screenshots total**, so retain at least two additional existing screenshots when adding or replacing these three. Review the all-ages flags in Steamworks yourself; the documentation also discusses four suitable screenshots for additional display placements.

## Screenshot selection

| Attached shot | Decision | Reason |
|---|---|---|
| 3 — cliffs | Selected; lead screenshot and principal capsule artwork | The most distinctive silhouette and sense of scale; it gives the set a recognisable environment. |
| 4 — beach | Selected; screenshot 2 | Clear turquoise water, sand and distant mountains; the strongest bright daytime contrast with the cliffs. |
| 5 — night | Selected; screenshot 3 and small capsule | Strong moon/stars/water composition and a different time-of-day mood. |
| 1 — rocky coast | Reserve | Much of the frame is empty sky and flat shore; overlaps the stronger beach view. |
| 2 — river | Reserve | Pleasant water and vegetation but less distinctive than the selected coastline and cliffs. |
| 6 — misty forest | Ambient page-background source | Atmospheric, but the fog hides detail for a lead screenshot. The background uses a right-hand crop, naturally excluding the character. |

Wes explicitly chose conventional cropping, colour/exposure and sharpening with no AI repainting. Only existing captured pixels were processed: small tonal/saturation adjustments, a centred 16:9 crop, Lanczos resampling and mild sharpening. No objects, landscape features, clothes, sky elements, interface elements or gameplay features were added, removed or regenerated. No logos or marketing copy were added to screenshots. `previews/before_after.png` compares the same crops side by side.

These attached captures were below native 1080p. The clipped-to-16:9 regions were modestly resampled: cliffs and beach approximately 7.1%, night approximately 10.1%. This meets the output size without claiming newly captured detail. Original dimensions, exact crop rectangles and processing values are recorded in `manifest.json`; `masters/` retains the graded native-resolution images.

## Branding

Checked `DesignKit/MO57_UI_Kit/MO57_UI_Kit/README.md`, `tokens.json` and the main-menu wordmark spec, plus the existing `SteamArt/README.md` and transparent wordmark.

- Field Ledger palette: cream `#EEE7D7`, dark iron `#0E0D0B`, verdigris `#6CB8A9` with the existing logo's brighter accent `#88CBBE`.
- Reused the existing `SteamArt/wordmark_transparent.png` without redrawing or replacing its lettering. Its SHA-256 is recorded in the manifest.
- The UI kit specifies Bitter for display headings. Its font files are not bundled; the established Steam wordmark is Sitka Display, as documented by the existing Steam art pipeline. This refresh preserves that established logotype and downloads no fonts.
- Main, header and vertical capsules use the cliff image. Small uses the night scene with a large, high-contrast wordmark. Only the game name is text on these assets; no slogans, scores, awards or promotional badges. Background has no text and subdued contrast.

These choices follow the [Steam graphical asset rules](https://partner.steamgames.com/doc/store/assets/rules). The small capsule was inspected at 462, 231, 184 and 120 pixels wide. The review-board labels are outside the upload files.

## Verification and source files

All eight upload files were decoded and checked as opaque 3-channel sRGB PNGs at their exact target dimensions. `manifest.json` records output byte counts and SHA-256 hashes. Source screenshots and the logo are retained in `sources/`; their original files were not overwritten. Existing SteamArt root assets, DesignKit and the existing Tools/steam_* scripts were left unchanged.

To regenerate with Node.js and Sharp:

```powershell
# From D:\UEProjects\MO57; use your installed Node.js, or the Codex bundled executable.
# If Sharp is not installed locally, MO57_SHARP_MODULE can name its absolute module directory.
node SteamArt/2026-10-26-refresh/build.mjs
```

The builder uses the preserved sources on later runs, so the temporary clipboard attachments are no longer needed. It changes only files inside this dated refresh folder. The native masters and exported image pixels are deterministic; the manifest creation timestamp changes on each run.

Download archive: `MO57-Steam-Store-Refresh-2026-10-26.zip` in this chat's visualizations folder. It contains the eight upload images, this readme, the manifest and `review/preview.png`.
