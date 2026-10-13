"""Render the Metaverse: Origins wordmark (transparent PNG) in the Field Ledger palette. Local fonts only (Sitka Display variable).

    python Tools/steam_wordmark.py [out.png]      -> Saved/SteamArt/wordmark.png (2400 px wide, parchment + teal on transparent)
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SITKA = r"C:\Windows\Fonts\SitkaVF.ttf"
PARCHMENT = (238, 231, 215, 255)      # color.text.primary  #EEE7D7
TEAL = (108, 184, 169, 255)           # color.accent        #6CB8A9
TEAL_HI = (136, 203, 190, 255)        # color.accent.hover  #88CBBE


def font(size, wght=700, opsz=None):
    f = ImageFont.truetype(SITKA, size)
    try:
        axes = f.get_variation_axes()
        vals = []
        for ax in axes:
            n = ax.get("name", b"")
            n = n.decode() if isinstance(n, bytes) else str(n)
            if "eight" in n:
                vals.append(wght)
            elif "ptical" in n:
                vals.append(opsz if opsz else min(max(size / 10.0, ax["minimum"]), ax["maximum"]))
            else:
                vals.append(ax["default"])
        f.set_variation_by_axes(vals)
    except Exception as e:   # static font build: keep the default instance
        print("variation axes unavailable:", e)
    return f


def tracked(draw, xy, text, fnt, fill, tracking):
    """draw text with extra letter spacing (px); returns the width."""
    x, y = xy
    for ch in text:
        draw.text((x, y), ch, font=fnt, fill=fill)
        x += draw.textlength(ch, font=fnt) + tracking
    return x - xy[0] - tracking


def measure(draw, text, fnt, tracking):
    return sum(draw.textlength(ch, font=fnt) for ch in text) + tracking * (len(text) - 1)


def build(width=2400):
    scratch = Image.new("RGBA", (10, 10))
    d = ImageDraw.Draw(scratch)
    big = font(520, 720, 72)
    small = font(300, 700, 48)
    t1, t2 = "METAVERSE", "ORIGINS"
    tr1, tr2 = 40, 120
    w1 = measure(d, t1, big, tr1)
    w2 = measure(d, t2, small, tr2)
    pad = 60
    W = int(max(w1, w2 + 1100)) + pad * 2
    H = 520 + 420 + pad * 2
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    dr = ImageDraw.Draw(img)
    # title line
    tracked(dr, ((W - w1) / 2, pad - 40), t1, big, PARCHMENT, tr1)
    # ORIGINS between two rules
    y2 = pad + 520 + 70
    cx = W / 2
    tracked(dr, (cx - w2 / 2, y2), t2, small, TEAL_HI, tr2)
    ry = y2 + 150
    gap = 70
    rule_len = (W - w2) / 2 - gap - pad - 60
    dr.rectangle((pad + 60, ry - 4, pad + 60 + rule_len, ry + 4), fill=TEAL_HI)
    dr.rectangle((W - pad - 60 - rule_len, ry - 4, W - pad - 60, ry + 4), fill=TEAL_HI)
    bbox = img.getbbox()
    img = img.crop((max(bbox[0] - 20, 0), max(bbox[1] - 20, 0), min(bbox[2] + 20, W), min(bbox[3] + 20, H)))
    scale = width / img.width
    return img.resize((width, int(img.height * scale)), Image.LANCZOS)


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Saved", "SteamArt", "wordmark.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    im = build()
    im.save(out)
    # preview on the kit's dark surface
    prev = Image.new("RGBA", (im.width + 400, im.height + 400), (14, 13, 11, 255))
    prev.alpha_composite(im, (200, 200))
    prev.convert("RGB").resize((prev.width // 3, prev.height // 3), Image.LANCZOS).save(out.replace(".png", "_preview.png"))
    print("wrote", out, im.size)
