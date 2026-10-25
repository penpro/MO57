"""Sharpness of the purple flowers (the lupines) in screenshots -- BR23-10's "blurry flowers" as a number.

    python Tools/flower_sharpness.py a.png [b.png ...] [--crop x0,y0,x1,y1] [--sheet out.png]

For each image: the pixels whose hue is purple (250-320 deg), saturation >= 0.25 and value >= 0.15 are the flowers; the score is the mean
gradient magnitude of the luminance on those pixels (Sobel, 0-255 scale) and its 90th percentile, plus the purple pixel count. Lower = softer.
Compare images of the SAME view (a still converged frame against one taken mid-pan, one AA method against another). --sheet writes the
crops side by side for a look.
"""
import argparse
import colorsys
import sys

from PIL import Image, ImageFilter


def score(path, crop, every=False):
    im = Image.open(path).convert("RGB")
    if crop:
        im = im.crop(crop)
    lum = im.convert("L")
    # scale 8: a Sobel response spans -1020..1020, which an 8-bit image would clip at +-127; /8 keeps it inside (x8 again below)
    gx = lum.filter(ImageFilter.Kernel((3, 3), [-1, 0, 1, -2, 0, 2, -1, 0, 1], 8, 128))
    gy = lum.filter(ImageFilter.Kernel((3, 3), [-1, -2, -1, 0, 0, 0, 1, 2, 1], 8, 128))
    px, ax, ay = im.load(), gx.load(), gy.load()
    vals = []
    for y in range(1, im.height - 1):
        for x in range(1, im.width - 1):
            r, g, b = px[x, y]
            h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
            if every or (250 / 360 <= h <= 320 / 360 and s >= 0.25 and v >= 0.15):
                dx, dy = (ax[x, y] - 128) * 8, (ay[x, y] - 128) * 8
                vals.append((dx * dx + dy * dy) ** 0.5)
    vals.sort()
    if not vals:
        return 0, 0.0, 0.0
    return len(vals), sum(vals) / len(vals), vals[int(0.9 * (len(vals) - 1))]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("images", nargs="+")
    ap.add_argument("--crop")
    ap.add_argument("--sheet")
    ap.add_argument("--all", action="store_true", help="every pixel of the crop, not only the purple ones (white / pink flowers)")
    a = ap.parse_args()
    crop = tuple(int(v) for v in a.crop.split(",")) if a.crop else None
    for p in a.images:
        n, mean, p90 = score(p, crop, a.all)
        print("FLOWERSHARP %-60s purple px %6d  gradient mean %5.1f  p90 %5.1f" % (p[-60:], n, mean, p90))
    if a.sheet:
        ims = [Image.open(p).convert("RGB").crop(crop) if crop else Image.open(p).convert("RGB") for p in a.images]
        sheet = Image.new("RGB", (sum(i.width for i in ims), max(i.height for i in ims)))
        x = 0
        for i in ims:
            sheet.paste(i, (x, 0))
            x += i.width
        sheet.save(a.sheet)
        print("FLOWERSHARP sheet %s" % a.sheet)


if __name__ == "__main__":
    sys.exit(main())
