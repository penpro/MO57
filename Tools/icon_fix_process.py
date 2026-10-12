"""Stage 2 of removing the baked checker background from the item icons: flood-fill the two checker greys from the image border into transparency.

    python Tools/icon_fix_process.py        (reads Saved/IconFix/orig/*.png, writes Saved/IconFix/fixed/*.png as RGBA)

The icons were captured with the editor's transparency checker burnt in (opaque 256x256, two near-black greys in 16 px cells). Background = pixels that are neutral grey close to one of the
two checker values AND connected to the border through such pixels, so dark pixels INSIDE an object (a deep shadow) stay. The edge gets a one-pixel soft alpha, and the colour under the
transparent area is bled outward from the object so bilinear filtering does not pull a dark fringe in.
"""
import collections
import glob
import os

import numpy as np
from PIL import Image

SRC = "D:/UEProjects/MO57/Saved/IconFix/orig/"
DST = "D:/UEProjects/MO57/Saved/IconFix/fixed/"
TOL = 4        # how far from a checker grey a pixel may be and still count as background
NEUTRAL = 5    # max spread between R, G, B for a "grey"


def checker_values(a):
    border = np.concatenate([a[0, :, :3], a[-1, :, :3], a[:, 0, :3], a[:, -1, :3]]).astype(int)
    vals, counts = np.unique(border[:, 0], return_counts=True)
    top = vals[np.argsort(-counts)][:2]
    return [int(v) for v in top]


def process(path):
    im = Image.open(path).convert("RGBA")
    a = np.asarray(im).astype(np.int16)
    h, w = a.shape[:2]
    cv = checker_values(a)
    rgb = a[..., :3]
    spread = rgb.max(-1) - rgb.min(-1)
    near = np.zeros((h, w), bool)
    for v in cv:
        near |= (np.abs(rgb[..., 0] - v) <= TOL) & (spread <= NEUTRAL)
    bg = np.zeros((h, w), bool)
    q = collections.deque()
    for x in range(w):
        for y in (0, h - 1):
            if near[y, x] and not bg[y, x]:
                bg[y, x] = True
                q.append((y, x))
    for y in range(h):
        for x in (0, w - 1):
            if near[y, x] and not bg[y, x]:
                bg[y, x] = True
                q.append((y, x))
    while q:
        y, x = q.popleft()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ny, nx = y + dy, x + dx
            if 0 <= ny < h and 0 <= nx < w and near[ny, nx] and not bg[ny, nx]:
                bg[ny, nx] = True
                q.append((ny, nx))
    alpha = np.where(bg, 0.0, 255.0)
    # one pixel soft edge: object pixels touching the background get 60 % alpha
    pad = np.pad(bg, 1, constant_values=True)
    touch = (pad[:-2, 1:-1] | pad[2:, 1:-1] | pad[1:-1, :-2] | pad[1:-1, 2:]) & ~bg
    alpha[touch] = 153.0
    out_rgb = rgb.astype(np.float32).copy()
    solid = ~bg
    # bleed object colour outward into the transparent area (3 passes of 8-neighbour copy)
    for _ in range(4):
        need = ~solid
        if not need.any():
            break
        acc = np.zeros_like(out_rgb)
        cnt = np.zeros((h, w), np.float32)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dy == 0 and dx == 0:
                    continue
                sh_rgb = np.roll(np.roll(out_rgb, dy, 0), dx, 1)
                sh_ok = np.roll(np.roll(solid, dy, 0), dx, 1)
                acc += sh_rgb * sh_ok[..., None]
                cnt += sh_ok
        fill = need & (cnt > 0)
        out_rgb[fill] = acc[fill] / cnt[fill][..., None]
        solid = solid | fill
    out = np.dstack([np.clip(out_rgb, 0, 255), alpha]).astype(np.uint8)
    img = Image.fromarray(out, "RGBA")
    bbox = Image.fromarray((alpha > 0).astype(np.uint8) * 255).getbbox()
    if bbox:   # fit the object to the frame: crop to its bounds (+ 8 % margin), centre it, enlarge at most 1.5x (the sources are 256 px: more would blur)
        l, t, r, b = bbox
        side = max(r - l, b - t)
        margin = int(0.08 * max(side, 1)) + 2
        crop = img.crop((max(0, l - margin), max(0, t - margin), min(w, r + margin), min(h, b + margin)))
        scale = min(1.5, (0.92 * w) / max(crop.width, crop.height))
        if abs(scale - 1.0) > 0.02:
            crop = crop.resize((max(1, round(crop.width * scale)), max(1, round(crop.height * scale))), Image.LANCZOS)
        canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        canvas.paste(crop, ((w - crop.width) // 2, (h - crop.height) // 2))
        img = canvas
    return img, cv, float(bg.mean())


os.makedirs(DST, exist_ok=True)
for p in sorted(glob.glob(SRC + "*.png")):
    img, cv, frac = process(p)
    img.save(DST + os.path.basename(p))
    print("%-28s checker %s  background %.0f %%" % (os.path.basename(p), cv, 100 * frac))
