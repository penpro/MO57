"""Build the Steam store assets (see steam_art.py for sizes and rules).   python Tools/steam_art_build.py"""
import os

from PIL import Image, ImageFilter

import steam_art as A
import steam_wordmark as WM

MARK = WM.build(2400)
GR = dict(warm=0.06, contrast=0.14, sat=1.08, shadow_teal=0.03, vignette=0.30)


def capsule(W, H, plate_name, logo_w, centre, focus=(0.5, 0.5), zoom=1.0, pad=0.55, pad_r=(0.55, 0.40), top=0.0, bottom=0.0, grade=GR):
    img = A.cover(A.plate(plate_name), W * 2, H * 2, focus, zoom)
    img = A.grade(img, **grade)
    if top:
        img = A.gradient_top(img, 0.5, top)
    if bottom:
        img = A.gradient_bottom(img, 0.55, bottom)
    if pad:
        img = A.scrim(img, centre, pad_r, pad)
    img = A.with_logo(img, logo_w, centre, mark=MARK)
    img = A.grain(img)
    return img.resize((W, H), Image.LANCZOS)


def main():
    out = A.OUT
    sets = {}
    gold = dict(warm=0.07, contrast=0.16, sat=1.10, shadow_teal=0.02, vignette=0.32)
    sets["main_capsule_1232x706"] = capsule(1232, 706, "S2_gold", 0.50, (0.5, 0.36), focus=(0.5, 0.5), pad=0.50, pad_r=(0.55, 0.40), top=0.55, grade=gold)
    sets["header_capsule_920x430"] = capsule(920, 430, "S2_gold", 0.56, (0.5, 0.38), focus=(0.5, 0.5), pad=0.50, pad_r=(0.60, 0.50), top=0.55, grade=gold)
    sets["small_capsule_462x174"] = capsule(462, 174, "S2_gold", 0.80, (0.5, 0.5), focus=(0.5, 0.52), zoom=1.2, pad=0.70, pad_r=(0.70, 0.90), top=0.25, grade=gold)
    sets["vertical_capsule_748x896"] = capsule(748, 896, "S4_gold", 0.86, (0.5, 0.27), focus=(0.52, 0.42), zoom=1.15, pad=0.50, pad_r=(0.80, 0.30), top=0.50, bottom=0.55, grade=gold)
    for k, im in sets.items():
        A.save(im, os.path.join(out, k + ".png"))
    # page background: ambient, no text, low contrast (Steam tints it blue and fades the edges)
    bg = A.cover(A.plate("S2_gold"), 2876, 1620, (0.5, 0.38), zoom=1.25)
    bg = A.grade(bg, warm=0.02, contrast=-0.12, sat=0.75, shadow_teal=0.02, vignette=0.40, gamma=1.25)
    bg = bg.filter(ImageFilter.GaussianBlur(6))
    bg = bg.resize((1438, 810), Image.LANCZOS)
    A.save(A.grain(bg, 0.008), os.path.join(out, "page_background_1438x810.png"))
    # legacy half-size slots (they still read "OLD" in Steamworks) -- exact halves of the new sizes
    legacy = {"header_capsule_460x215": ("header_capsule_920x430", (460, 215)), "small_capsule_231x87": ("small_capsule_462x174", (231, 87)),
              "main_capsule_616x353": ("main_capsule_1232x706", (616, 353)), "vertical_capsule_374x448": ("vertical_capsule_748x896", (374, 448))}
    for name, (src, size) in legacy.items():
        A.save(sets[src].resize(size, Image.LANCZOS), os.path.join(out, "legacy", name + ".png"))
    # library assets (same Steamworks page): capsule 600x900, hero 3840x1240 (no logo), logo (transparent, 1280 wide)
    A.save(capsule(600, 900, "S4_gold", 0.86, (0.5, 0.27), focus=(0.52, 0.42), zoom=1.15, pad=0.50, pad_r=(0.80, 0.30), top=0.50, bottom=0.55, grade=gold), os.path.join(out, "library", "library_capsule_600x900.png"))
    hero = A.grain(A.grade(A.cover(A.plate("S2_gold"), 3840, 1240, (0.5, 0.52)), **gold))
    A.save(hero, os.path.join(out, "library", "library_hero_3840x1240.png"))
    lw = 1280
    A.save_rgba(MARK.resize((lw, int(MARK.height * lw / MARK.width)), Image.LANCZOS), os.path.join(out, "library", "library_logo_1280.png"))
    # contact sheet
    sheet = Image.new("RGB", (1232 + 920 + 60, 706 + 430 + 40), (30, 28, 24))
    sheet.paste(sets["main_capsule_1232x706"], (20, 20))
    sheet.paste(sets["header_capsule_920x430"], (1232 + 40, 20))
    sheet.paste(sets["small_capsule_462x174"], (1232 + 40, 430 + 40))
    sheet.save(os.path.join(out, "_sheet.png"))


if __name__ == "__main__":
    main()
