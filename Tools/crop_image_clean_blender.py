"""Blender (headless): paint the soil out of a crop reference photo before TRELLIS -- the stems end flat, cut at the soil line.

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Tools/crop_image_clean_blender.py -- <in image> <out.png>

Why (Nov 3 2026): Qwen-Image stands seedlings in a block of soil and roots (15-20 % of the picture's height on Barley Sprouted) whatever
the prompt says ("no pot, no soil, cut off at ground level"), and TRELLIS builds it. Soil pixels (dark -- luminance under 0.22 -- not
greenish, and unlike the backdrop) are counted per row in the bottom 40 % (no soil when the peak row holds under 3 % of the width). The
block is solid: from its densest row it runs up while dark pixels cover half of that row's width. Everything from its top down is
painted the backdrop colour (the median of the border), so the stems end in a straight cut. A photo with no soil comes out unchanged.
"""
import sys

import bpy
import numpy as np

a = sys.argv[sys.argv.index("--") + 1:]
src, dst = a[0], a[1]
img = bpy.data.images.load(src)
w, h = img.size[:]
px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, img.channels)[..., :3]   # row 0 = the bottom
border = np.concatenate([px[:6].reshape(-1, 3), px[-6:].reshape(-1, 3), px[:, :6].reshape(-1, 3), px[:, -6:].reshape(-1, 3)])
bg = np.median(border, axis=0)
fg = np.abs(px - bg).max(axis=2) > 0.10
r, g, b = px[..., 0], px[..., 1], px[..., 2]
lum = 0.2126 * r + 0.7152 * g + 0.0722 * b
# Soil is DARK and not greenish: dry brown straw (luminance 0.4-0.6) and blue-green flax in shadow (green over red) passed the looser
# first test and the cut took a third of the plant (Nov 3: Emmer / Flax / Pea Ripe cut at 37-40 %).
soil = fg & (lum < 0.22) & ((g - r) < 0.03)
# Where the block ends (Nov 3, five tries on 11 photos): the highest dark row anywhere caught dark stems far above the soil; a dark COUNT
# or a dark SHARE per row cannot tell shaded dry stems (emmer, flax, pea at ripe) from soil carrying light root hairs and crumbs (40 %
# cuts, then soil left on three seedlings). Shape can: a soil block is SOLID across its own width, stems are thin with backdrop between
# them. The width is taken at the densest dark row; the block goes on up while dark pixels cover half of it (3-row gaps allowed).
counts = soil[: int(h * 0.4)].sum(axis=1)
top = 0
peak = counts.max()
if peak >= 0.03 * w:
    dense = int(np.argmax(counts))
    xs = np.nonzero(soil[dense])[0]
    x0, x1 = int(np.percentile(xs, 5)), int(np.percentile(xs, 95)) + 1
    cover = soil[: int(h * 0.4), x0:x1].mean(axis=1)
    # Walk up from the densest row (inside the block). From the lowest dark row, the walk stalled in the white roots hanging under a
    # seedling's soil block (Barley Sprouted: 7.6 % cut, the block reaches 20 %).
    top, gap = dense + 1, 0
    for row in range(dense + 1, len(counts)):
        if cover[row] >= 0.5:
            top, gap = row + 1, 0
        else:
            gap += 1
            if gap > 3:
                break
cut = min(top + max(2, h // 200), int(h * 0.4)) if top > 0 else 0
if cut:
    px[:cut] = bg
out = bpy.data.images.new("clean", w, h, alpha=False)
full = np.ones((h, w, 4), dtype=np.float32)
full[..., :3] = px
out.pixels.foreach_set(full.ravel())
out.filepath_raw = dst
out.file_format = "PNG"
out.save()
print("CLEANIMG %s -> %s: soil up to row %d of %d (%.1f %% of the height) painted out" % (src.rsplit("/", 1)[-1], dst.rsplit("/", 1)[-1], cut, h, 100.0 * cut / h))
