"""Generate the HUD moodle glyph textures (white on transparent, 128x128): the widget tints them by severity (amber / red / green), so the glyphs are pure white shapes.

    python Tools/gen_moodle_icons.py [out_dir]     (default: Saved/MoodleIcons)

One glyph per moodle kind; the two-level moodles (hunger / thirst / energy) carry a ring around the glyph at level 1 (critical). Drawn at 4x and downsampled for smooth edges.
Import into the project with Content/Python/import_moodle_icons.py (editor), which also sets the UI texture settings.
"""
import math
import os
import sys

from PIL import Image, ImageDraw

S = 512          # supersampled canvas
OUT = 128
W = (255, 255, 255, 255)


def canvas():
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)


def finish(im, name, out_dir):
    im.resize((OUT, OUT), Image.LANCZOS).save(os.path.join(out_dir, name + ".png"))


def ring(d, width=22, margin=18):
    d.ellipse((margin, margin, S - margin, S - margin), outline=W, width=width)


def line(d, p0, p1, w=26):
    d.line([p0, p1], fill=W, width=w)
    r = w // 2
    for (x, y) in (p0, p1):
        d.ellipse((x - r, y - r, x + r, y + r), fill=W)


def snowflake(d):
    c = S // 2
    R = 190
    for k in range(3):
        a = math.radians(k * 60 + 90)
        p0 = (c + R * math.cos(a), c - R * math.sin(a))
        p1 = (c - R * math.cos(a), c + R * math.sin(a))
        line(d, p0, p1, 24)
        for sign in (1, -1):
            bx, by = c + sign * 130 * math.cos(a), c - sign * 130 * math.sin(a)
            for da in (-40, 40):
                a2 = a + (math.radians(da) if sign == 1 else math.radians(da) + math.pi)
                line(d, (bx, by), (bx + 62 * math.cos(a2), by - 62 * math.sin(a2)), 18)


def thermometer(d, level):
    """level 0 = low mercury (cold), 1 = high mercury (hot)."""
    cx = S // 2
    d.rounded_rectangle((cx - 44, 60, cx + 44, 330), radius=44, outline=W, width=24)
    d.ellipse((cx - 92, 292, cx + 92, 476), fill=W)
    top = 250 if level == 0 else 120
    d.rounded_rectangle((cx - 20, top, cx + 20, 360), radius=20, fill=W)
    for y in (110, 170, 230):
        d.line([(cx + 62, y), (cx + 112, y)], fill=W, width=16)


def sun(d):
    c = S // 2
    d.ellipse((c - 105, c - 105, c + 105, c + 105), fill=W)
    for k in range(8):
        a = math.radians(k * 45)
        line(d, (c + 150 * math.cos(a), c - 150 * math.sin(a)), (c + 215 * math.cos(a), c - 215 * math.sin(a)), 26)


def droplet(d, cx, cy, r):
    d.ellipse((cx - r, cy - r + r * 0.55, cx + r, cy + r + r * 0.55), fill=W)
    d.polygon([(cx, cy - r * 1.55), (cx - r * 0.93, cy + r * 0.2), (cx + r * 0.93, cy + r * 0.2)], fill=W)


def drops(d, n):
    if n == 1:
        droplet(d, S // 2, S // 2, 120)
    elif n == 2:
        droplet(d, S // 2 - 85, S // 2 + 30, 95)
        droplet(d, S // 2 + 100, S // 2 - 20, 75)
    else:
        droplet(d, S // 2, S // 2 - 55, 85)
        droplet(d, S // 2 - 110, S // 2 + 85, 70)
        droplet(d, S // 2 + 110, S // 2 + 85, 70)


def cutlery(d):
    # fork (left): three tines + neck + handle
    fx = 170
    for dx in (-48, 0, 48):
        d.rounded_rectangle((fx + dx - 11, 70, fx + dx + 11, 230), radius=11, fill=W)
    d.rounded_rectangle((fx - 59, 200, fx + 59, 250), radius=24, fill=W)
    d.rounded_rectangle((fx - 14, 240, fx + 14, 450), radius=14, fill=W)
    # knife (right): blade + handle
    kx = 345
    d.polygon([(kx - 40, 70), (kx + 28, 70), (kx + 40, 230), (kx - 40, 250)], fill=W)
    d.ellipse((kx - 40, 56, kx + 28, 120), fill=W)
    d.rounded_rectangle((kx - 14, 240, kx + 26, 450), radius=14, fill=W)


def glass(d):
    # a drinking glass: tapered body outline with a water line, plus a base
    d.polygon([(130, 70), (382, 70), (340, 440), (172, 440)], outline=W, width=24)
    d.polygon([(142, 190), (370, 190), (335, 425), (177, 425)], fill=W)
    d.rounded_rectangle((150, 436, 362, 462), radius=12, fill=W)


def moon(d):
    c = S // 2
    d.ellipse((c - 175, c - 175, c + 175, c + 175), fill=W)
    d.ellipse((c - 70, c - 215, c + 245, c + 100), fill=(0, 0, 0, 0))
    # erase = draw transparent over: Pillow draws RGBA verbatim only with an image mask, so rebuild with alpha compositing
    return


def moon_glyph():
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    mask = Image.new("L", (S, S), 0)
    md = ImageDraw.Draw(mask)
    c = S // 2
    md.ellipse((c - 175, c - 175, c + 175, c + 175), fill=255)
    cut = Image.new("L", (S, S), 0)
    ImageDraw.Draw(cut).ellipse((c - 40, c - 215, c + 275, c + 100), fill=255)
    mask = Image.composite(Image.new("L", (S, S), 0), mask, cut)
    im.paste(Image.new("RGBA", (S, S), W), (0, 0), mask)
    return im


def make(out_dir):
    os.makedirs(out_dir, exist_ok=True)

    def one(name, fn, with_ring=False):
        im, d = canvas()
        fn(d)
        if with_ring:
            ring(d)
        finish(im, name, out_dir)

    one("T_Moodle_VeryCold", snowflake)
    one("T_Moodle_Cold", lambda d: thermometer(d, 0))
    one("T_Moodle_Hot", lambda d: thermometer(d, 1))
    one("T_Moodle_VeryHot", sun)
    one("T_Moodle_Damp", lambda d: drops(d, 1))
    one("T_Moodle_Wet", lambda d: drops(d, 2))
    one("T_Moodle_Soaked", lambda d: drops(d, 3))
    one("T_Moodle_Hungry", cutlery)
    one("T_Moodle_Starving", cutlery, True)
    one("T_Moodle_Thirsty", glass)
    one("T_Moodle_Dehydrated", glass, True)
    one("T_Moodle_Tired", lambda d: None)   # replaced below
    one("T_Moodle_Exhausted", lambda d: None)
    for name, rng in (("T_Moodle_Tired", False), ("T_Moodle_Exhausted", True)):
        im = moon_glyph()
        if rng:
            ring(ImageDraw.Draw(im))
        finish(im, name, out_dir)


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Saved", "MoodleIcons")
    make(out)
    # a contact sheet for a quick look
    names = sorted(f for f in os.listdir(out) if f.startswith("T_Moodle_") and f.endswith(".png"))
    cols = 5
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * 140, rows * 140), (60, 70, 85, 255))
    for i, n in enumerate(names):
        im = Image.open(os.path.join(out, n))
        sheet.paste(im, ((i % cols) * 140 + 6, (i // cols) * 140 + 6), im)
    sheet.save(os.path.join(out, "_sheet.png"))
    print("wrote", len(names), "icons to", out)
