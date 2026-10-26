# Metaverse: Origins — Steam library assets

Four upload files are ready in `exports/`, matching the 26 October 2026 store refresh. `preview.png` shows the set; `review.html` links to the full files and placement proofs. Nothing has been uploaded to Steam.

| Library slot | File under exports/ | Exact size | Format |
|---|---|---|---|
| Capsule | library_capsule_600x900.png | 600 × 900 | Opaque RGB PNG |
| Header | library_header_920x430.png | 920 × 430 | Opaque RGB PNG |
| Hero | library_hero_3840x1240.png | 3840 × 1240 | Opaque RGB PNG, no text or logo |
| Logo | library_logo_1280x368.png | 1280 × 368 | Transparent RGBA PNG |

The logo uses its natural wide shape with a little space for a soft dark shadow. Its 1280-pixel width meets the supplied specification; it does not need to be padded to 720 pixels tall. All dimensions and the separate hero/logo arrangement were checked against the pasted brief and the current [Steam library asset documentation](https://partner.steamgames.com/doc/store/assets/libraryassets).

## Art and branding

- **Capsule:** the same graded cliff capture and established cream/verdigris wordmark as the new store graphics, recomposed for the narrower 2:3 library slot. The branded store vertical capsule was not simply cropped.
- **Header:** byte-identical to the refreshed 920 × 430 store header for consistent branding.
- **Hero:** a native-resolution crop of an existing 3840 × 2160 in-game panorama, originally `Saved/SteamArt/plates/S2_gold.png`. Its source is preserved here as `sources/hero_panorama_3840x2160.png`. This is the game's woodland/plains vista, with gentle colour and contrast treatment. It is not an enlarged clipboard screenshot and uses no AI repainting.
- **Logo:** the established Sitka-based wordmark from the existing Steam art set, in the Field Ledger cream/verdigris colours. Its lettering was preserved; the exported logo adds a subtle dark shadow over a genuinely transparent background. No fonts were downloaded.

Only the game title appears on the capsule, header and logo. The hero upload has no words or logo; Steam overlays the separate logo. Checkerboard, safe-area frame and labels appear only in review files outside `exports/`.

## Logo placement in Steamworks

Upload the hero and logo separately, then use the Placement Tool:

1. Select **bottom-left**.
2. Start with the logo occupying roughly one third of the hero's width. The full-size local preview uses a 1280-wide logo at x=144, y=816 on the 3840 × 1240 hero. These are preview coordinates, not additional Steam form fields.
3. Leave some vertical room above the logo's bounds, resize the client preview and check the scroll/parallax behaviour before saving.

The centred hero safe rectangle is **860 × 380**, at x=1490, y=430 in the full-resolution image. Its central woodland detail survives that crop, and there are no faces near the outer edges to lose. See `previews/hero_safe_area.png` for the review guide and `previews/hero_with_separate_logo.png` for the proposed placement. Neither is an upload image.

## Verification and rebuild

The builder decoded all four PNGs and checked exact dimensions, sRGB colour space and expected channel/alpha counts. The three artwork files are opaque RGB. The logo is RGBA with nonempty artwork and fully transparent pixels; all four corner alpha values are zero. The header's hash matches the store-header source. Input and output SHA-256 hashes are in `manifest.json`.

Rebuild from the repository root using Node.js with Sharp:

```powershell
node SteamArt/2026-10-26-refresh/library/build.mjs
```

The builder can use the Codex bundled Sharp module or an installed module. `MO57_SHARP_MODULE` can provide another absolute module directory. It reads the committed parent store-refresh masters/wordmark/header and the preserved native 4K source here; it only writes inside this library folder.

The download archive `MO57-Steam-Library-Assets-2026-10-26.zip` contains the four upload images, this readme, the manifest and `review/preview.png`. The larger source and detailed placement proofs remain in the project folder. Existing store exports, earlier library assets, DesignKit and unrelated changes were not modified.
