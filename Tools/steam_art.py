"""Compose the Steam store graphical assets for Metaverse: Origins from in-engine plates (Tools/steam_scout.py) and the wordmark (Tools/steam_wordmark.py).

    python Tools/steam_art.py                # -> Saved/SteamArt/out/*.png (+ legacy/ half-size, library/), plus a contact sheet

Steamworks sizes (Store Page > Graphical Assets):
    header_capsule_920x430, small_capsule_462x174, main_capsule_1232x706, vertical_capsule_748x896, page_background_1438x810
    legacy/ = the old half-size slots (460x215, 231x87, 616x353, 374x448) that still show "OLD"
    library/ = library capsule 600x900, library hero 3840x1240, library logo (transparent)
Rules honoured: the logo is the only text; no quotes / scores / awards; page background has no text and low contrast; capsules are opaque RGB.
Everything is local: the plates are the game's own renders, the type is Windows' Sitka, the palette is the UI kit's (Field Ledger).
"""
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import steam_wordmark as WM   # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLATES = os.path.join(ROOT, "Saved", "SteamArt", "plates")
OUT = os.path.join(ROOT, "Saved", "SteamArt", "out")


def plate(name):
    return Image.open(os.path.join(PLATES, name + ".png")).convert("RGB")


def cover(img, W, H, focus=(0.5, 0.5), zoom=1.0):
    """Crop img to the W:H aspect around `focus` (0..1), optionally zoomed in, and resample to W x H."""
    iw, ih = img.size
    ar = W / H
    cw, ch = (iw, iw / ar) if iw / ih < ar else (ih * ar, ih)
    cw, ch = cw / zoom, ch / zoom
    cx = min(max(focus[0] * iw, cw / 2), iw - cw / 2)
    cy = min(max(focus[1] * ih, ch / 2), ih - ch / 2)
    box = (int(round(cx - cw / 2)), int(round(cy - ch / 2)), int(round(cx + cw / 2)), int(round(cy + ch / 2)))
    return img.crop(box).resize((W, H), Image.LANCZOS)


def to_f(img):
    return np.asarray(img, dtype=np.float32) / 255.0


def to_img(a):
    return Image.fromarray((np.clip(a, 0, 1) * 255.0 + 0.5).astype(np.uint8), "RGB")


def grade(img, warm=0.05, contrast=0.12, sat=1.05, shadow_teal=0.025, vignette=0.28, lift=0.0, gamma=1.0):
    """Gentle filmic grade: S-curve, warm highlights, teal-leaning shadows (the kit accent), soft vignette."""
    a = to_f(img)
    a = np.clip(a + lift, 0, 1) ** gamma
    lum = (a * np.array([0.2126, 0.7152, 0.0722], np.float32)).sum(axis=2, keepdims=True)
    a = lum + (a - lum) * sat
    a = a + contrast * (a - 0.5) * (1.0 - np.abs(a - 0.5) * 1.6)              # soft S-curve
    hi = np.clip((lum - 0.45) / 0.55, 0, 1)
    lo = np.clip((0.4 - lum) / 0.4, 0, 1)
    a[..., 0] += warm * hi[..., 0]
    a[..., 2] -= warm * 0.9 * hi[..., 0]
    a[..., 1] += shadow_teal * lo[..., 0]
    a[..., 2] += shadow_teal * 1.2 * lo[..., 0]
    h, w = a.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.sqrt(((xx - w / 2) / (w / 2)) ** 2 + ((yy - h / 2) / (h / 2)) ** 2) / 1.414
    a *= (1.0 - vignette * np.clip(r, 0, 1) ** 2.2)[..., None]
    return to_img(a)


def scrim(img, center=(0.5, 0.5), radius=(0.55, 0.5), strength=0.55, color=(14, 13, 11)):
    """A soft elliptical dark pad behind the logo (kit surface.0), so the wordmark reads on any plate."""
    a = to_f(img)
    h, w = a.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.sqrt(((xx / w - center[0]) / radius[0]) ** 2 + ((yy / h - center[1]) / radius[1]) ** 2)
    m = np.clip(1.0 - d, 0, 1) ** 1.4 * strength
    c = np.array(color, np.float32) / 255.0
    a = a * (1 - m[..., None]) + c * m[..., None]
    return to_img(a)


def gradient_bottom(img, start=0.45, strength=0.75, color=(14, 13, 11)):
    a = to_f(img)
    h = a.shape[0]
    y = np.linspace(0, 1, h, dtype=np.float32)
    m = np.clip((y - start) / (1 - start), 0, 1) ** 1.3 * strength
    c = np.array(color, np.float32) / 255.0
    a = a * (1 - m[:, None, None]) + c * m[:, None, None]
    return to_img(a)


def gradient_top(img, end=0.45, strength=0.6, color=(14, 13, 11)):
    a = to_f(img)
    h = a.shape[0]
    y = np.linspace(0, 1, h, dtype=np.float32)
    m = np.clip((end - y) / end, 0, 1) ** 1.3 * strength
    c = np.array(color, np.float32) / 255.0
    a = a * (1 - m[:, None, None]) + c * m[:, None, None]
    return to_img(a)


def with_logo(img, width_frac, center=(0.5, 0.5), shadow=0.65, mark=None):
    mark = mark or WM.build(2400)
    W, H = img.size
    lw = int(W * width_frac)
    lh = int(mark.height * lw / mark.width)
    m = mark.resize((lw, lh), Image.LANCZOS)
    x = int(center[0] * W - lw / 2)
    y = int(center[1] * H - lh / 2)
    base = img.convert("RGBA")
    # two-layer shadow: a wide soft one for separation and a tight dark halo so thin strokes (ORIGINS, the rules) survive bright clouds
    def shadow_layer(opacity, blur, dx, dy, grow=0):
        alpha = m.getchannel("A")
        if grow:
            alpha = alpha.filter(ImageFilter.MaxFilter(grow))
        alpha = alpha.point(lambda v: int(v * opacity))
        blk = Image.new("RGBA", m.size, (6, 5, 4, 255))
        blk.putalpha(alpha)
        layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
        layer.alpha_composite(blk, (x + dx, y + dy))
        return layer.filter(ImageFilter.GaussianBlur(blur))
    base.alpha_composite(shadow_layer(shadow, max(3, lw // 70), max(2, lw // 300), max(3, lw // 200)))
    base.alpha_composite(shadow_layer(0.95, max(1.5, lw / 420), 0, max(1, lw // 700), grow=3))
    base.alpha_composite(m, (x, y))
    return base.convert("RGB")


def grain(img, amount=0.012, seed=7):
    rng = np.random.default_rng(seed)
    a = to_f(img)
    n = rng.normal(0.0, amount, a.shape[:2]).astype(np.float32)
    return to_img(a + n[..., None])


def save(img, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, optimize=True)
    print("wrote", os.path.relpath(path, ROOT), img.size)


def save_rgba(img, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, optimize=True)
    print("wrote", os.path.relpath(path, ROOT), img.size, "(transparent)")
