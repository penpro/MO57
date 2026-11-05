"""MO57 promo: the film's look as 3D LUTs, built from numbers, not from a preset. A filmic shoulder and toe, the kit's teal in the shadows, warm highlights, and per-mood variants.

    python Tools/promo/grade.py          writes Saved/Promo/luts/<mood>.cube (33 points; ffmpeg's lut3d reads them)

The moods are film.py's grade names: neutral, dawn, day, gold, night, storm, cave. Input is the game's display-referred picture (sRGB, already tone-mapped by the engine), so this is a
grade, not a tone map: it must leave skin and foliage believable. Tune here and re-run edit.py: nothing is baked into the footage.
"""
import os
import sys

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "Saved", "Promo", "luts")
N = 33

# contrast: S-curve strength; lift: black level; sat: saturation; shadow / high: colour added there (r, g, b); gamma: mid brightness; green: pulls foliage's yellow-green towards deeper olive
MOODS = {
    "neutral": dict(contrast=0.10, lift=0.0, sat=1.0, shadow=(-0.004, 0.003, 0.006), high=(0.004, 0.002, -0.004), gamma=1.0, green=0.0),
    "dawn": dict(contrast=0.14, lift=0.004, sat=0.92, shadow=(-0.012, 0.004, 0.018), high=(0.018, 0.006, -0.016), gamma=0.98, green=0.25),
    "day": dict(contrast=0.16, lift=0.0, sat=0.96, shadow=(-0.008, 0.004, 0.012), high=(0.010, 0.004, -0.010), gamma=1.0, green=0.30),
    "gold": dict(contrast=0.18, lift=0.0, sat=1.0, shadow=(-0.012, 0.002, 0.016), high=(0.024, 0.008, -0.022), gamma=0.98, green=0.22),
    "night": dict(contrast=0.14, lift=0.004, sat=0.86, shadow=(-0.012, 0.002, 0.022), high=(0.016, 0.004, -0.012), gamma=1.02, green=0.15),
    "storm": dict(contrast=0.18, lift=0.002, sat=0.78, shadow=(-0.010, 0.006, 0.016), high=(-0.004, 0.004, 0.006), gamma=0.96, green=0.3),
    "cave": dict(contrast=0.12, lift=0.0, sat=0.94, shadow=(-0.006, 0.002, 0.010), high=(0.016, 0.004, -0.014), gamma=1.0, green=0.0),
}


def s_curve(x, k):
    """A symmetric contrast curve about mid grey, shoulder and toe; k 0 = identity."""
    return x + k * (np.sin(2 * np.pi * (x - 0.25)) + 1.0) * 0.5 * (x - 0.5) * 2.0 * (1.0 - np.abs(2 * x - 1.0)) * 0.9


def apply(rgb, m):
    x = np.clip(rgb, 0.0, 1.0)
    luma = x @ np.array([0.2126, 0.7152, 0.0722])
    # foliage: the engine's yellow-greens read neon once sharpened by a grade; pull them towards olive
    if m["green"]:
        g_excess = np.clip(x[..., 1] - np.maximum(x[..., 0], x[..., 2]), 0.0, 1.0)
        x = x.copy()
        x[..., 1] -= g_excess * m["green"] * 0.55
        x[..., 0] -= g_excess * m["green"] * 0.05
    x = np.clip(x, 0.0, 1.0) ** (1.0 / m["gamma"])
    x = s_curve(x, m["contrast"])
    luma = (x @ np.array([0.2126, 0.7152, 0.0722]))[..., None]
    x = luma + (x - luma) * m["sat"]
    shadow_w = (1.0 - np.clip(luma * 2.2, 0.0, 1.0)) ** 1.5
    high_w = np.clip((luma - 0.45) / 0.55, 0.0, 1.0) ** 1.2
    x = x + np.array(m["shadow"]) * shadow_w + np.array(m["high"]) * high_w
    x = m["lift"] + x * (1.0 - m["lift"])
    return np.clip(x, 0.0, 1.0)


def write_cube(path, m):
    g = np.linspace(0.0, 1.0, N)
    b, gg, r = np.meshgrid(g, g, g, indexing="ij")     # .cube order: red changes fastest
    rgb = np.stack((r, gg, b), axis=-1).reshape(-1, 3)
    out = apply(rgb, m)
    with open(path, "w", encoding="ascii", newline="\n") as f:
        f.write("TITLE \"MO57 promo\"\nLUT_3D_SIZE %d\n" % N)
        np.savetxt(f, out, fmt="%.6f")


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, m in MOODS.items():
        path = os.path.join(OUT, name + ".cube")
        write_cube(path, m)
        print("LUT", path)


if __name__ == "__main__":
    main()
