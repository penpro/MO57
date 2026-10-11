"""Generate a seamless beach-sand texture set (albedo / normal / roughness / height) with no downloads.

    python Tools/gen_sand_textures.py [size=2048] [outdir=Saved/GeneratedTextures/sand]

Why this exists (2026-10-10, "add the sand material to sea level shores"): the project had gravel / muddy leaves / cliff voxel materials and no sand at all, and the Megascans packs on this
machine hold no sand SURFACE (only a Sand Rock mesh). So the tile is made here: every layer is FFT-filtered noise (inherently periodic, so the tile is seamless) plus integer-frequency ripples
(periodic too). The editor side (texture import, MI_Sand, VST_MI_Sand, the mega-material entry) is Content/Python/make_sand_material.py.

Layers (all in one height field, so the normal, the roughness and the height agree with each other):
  - grain:    high-frequency noise, the fine texture you see under your boots
  - drift:    mid-frequency undulation a few centimetres deep
  - ripples:  wind ripples, a wave vector with integer cycles per tile, phase-warped by noise so they are irregular
Albedo is a pale warm beach sand, modulated by the same fields plus sparse dark mineral specks and a slow blotchy variation (damp / dry patches).
"""
import os
import sys

import numpy as np
from PIL import Image


def fft_noise(n, beta, rng, aniso=(1.0, 1.0)):
    """Periodic noise with a 1/f^beta spectrum, normalised to zero mean / unit deviation."""
    f = np.fft.fftfreq(n)[:, None] * aniso[0]
    g = np.fft.fftfreq(n)[None, :] * aniso[1]
    r = np.sqrt(f * f + g * g)
    r[0, 0] = 1.0
    spec = (rng.standard_normal((n, n)) + 1j * rng.standard_normal((n, n))) / np.power(r, beta)
    spec[0, 0] = 0.0
    out = np.fft.ifft2(spec).real
    return (out - out.mean()) / out.std()


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 2048
    out_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Saved", "GeneratedTextures", "sand")
    os.makedirs(out_dir, exist_ok=True)
    rng = np.random.default_rng(20261010)

    y, x = np.mgrid[0:n, 0:n].astype(np.float64) / n

    grain = fft_noise(n, 0.35, rng)                      # nearly white: individual grains
    fine = fft_noise(n, 1.1, rng)
    drift = fft_noise(n, 2.0, rng)                       # soft undulation
    blotch = fft_noise(n, 2.6, rng)                      # slow damp / dry variation
    warp = fft_noise(n, 2.2, rng)

    # Wind ripples: integer cycles per tile keep the pattern seamless; the phase is bent by periodic noise.
    ripples = np.zeros((n, n))
    for (kx, ky, amp) in ((11, 4, 1.0), (7, -9, 0.45)):
        phase = 2.0 * np.pi * (kx * x + ky * y) + 1.6 * warp
        ripples += amp * np.sin(phase)
    ripples /= np.abs(ripples).max()

    height = 0.50 * ripples + 0.30 * np.clip(drift, -2.5, 2.5) / 2.5 + 0.13 * np.clip(fine, -3, 3) / 3 + 0.07 * np.clip(grain, -3, 3) / 3
    height = (height - height.min()) / (height.max() - height.min())

    # Normal map (OpenGL convention: +Y up), from the periodic height field.
    strength = 5.0
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5 * n / 64.0
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5 * n / 64.0
    nx, ny, nz = -dx * strength, dy * strength, np.ones_like(height)
    inv = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.stack([nx * inv, ny * inv, nz * inv], axis=-1)

    # Albedo (sRGB): warm pale sand, lit by the height (ripple crests a touch brighter), blotchy damp patches, dark mineral specks.
    base = np.array([0.80, 0.72, 0.58])
    lum = (1.0 + 0.030 * np.clip(drift, -2.5, 2.5) / 2.5 + 0.07 * (height - 0.5) + 0.13 * np.clip(grain, -3, 3) / 3
           + 0.06 * np.clip(fine, -3, 3) / 3 + 0.035 * np.clip(blotch, -2.5, 2.5) / 2.5)
    tint = np.stack([1.0 + 0.012 * blotch, np.ones_like(blotch), 1.0 - 0.020 * blotch], axis=-1)
    albedo = base[None, None, :] * lum[..., None] * tint
    specks = rng.random((n, n)) < 0.004
    albedo[specks] *= rng.uniform(0.35, 0.65, size=(int(specks.sum()), 1))
    albedo = np.clip(albedo, 0.0, 1.0)

    # Roughness: sand is rough everywhere; damp patches (darker) are a little smoother.
    rough = np.clip(0.90 - 0.06 * np.clip(blotch, -2.5, 2.5) / 2.5 + 0.03 * np.clip(grain, -3, 3) / 3, 0.0, 1.0)

    def save_rgb(name, arr):
        Image.fromarray((np.clip(arr, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8), "RGB").save(os.path.join(out_dir, name))

    def save_gray(name, arr):
        Image.fromarray((np.clip(arr, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8), "L").save(os.path.join(out_dir, name))

    save_rgb("sand_diff_%dk.png" % (n // 1024), albedo)
    save_rgb("sand_nor_gl_%dk.png" % (n // 1024), normal * 0.5 + 0.5)
    save_gray("sand_rough_%dk.png" % (n // 1024), rough)
    save_gray("sand_height_%dk.png" % (n // 1024), height)

    # Seam check: the difference across the wrap-around edge must be no larger than between neighbouring pixels inside the tile.
    def seam(a):
        a = np.asarray(a, dtype=np.float64)
        edge = np.abs(a[:, 0] - a[:, -1]).mean() + np.abs(a[0, :] - a[-1, :]).mean()
        inner = np.abs(a[:, 1:] - a[:, :-1]).mean() + np.abs(a[1:, :] - a[:-1, :]).mean()
        return edge / max(inner, 1e-9)

    print("wrote 4 maps to %s (size %d)" % (out_dir, n))
    print("seam ratio (wrap edge / inner neighbour difference, ~1 = seamless): albedo %.2f, height %.2f" % (seam(albedo[..., 0]), seam(height)))


if __name__ == "__main__":
    main()
