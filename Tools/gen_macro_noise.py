"""Generate the tileable macro-noise texture the terrain material samples (4 independent channels, 1024x1024, wraps seamlessly).

    python Tools/gen_macro_noise.py [outdir=Saved/GeneratedTextures/macro]   ->  T_MOMacroNoise.png (RGBA8)

Why (2026-10-13, "fix texture tiling of the ground textures"): every ground surface is one tiling texture set (triplanar, two sizes blended by distance), so its repetition shows as a regular
pattern. The terrain material (M_QuixelBase) now (1) bends the world position by this noise before the triplanar lookup (domain warp: the tile grid stops being a grid), (2) multiplies the
albedo by low-frequency variation read from it at three world scales (macro colour / brightness variation), and (3) breaks the snow line and the distance tint up with it. One texture, shared by
every surface; cheap (1-4 fetches). Channels (FFT-filtered 1/f^beta noise => inherently periodic): R smooth, G smooth (independent), B medium, A fine mottling; each remapped to 0..1 around 0.5
with a soft clip so the mean is neutral (multiplying by it does not darken the terrain on average).
"""
import os
import sys

import numpy as np
from PIL import Image


def fft_noise(n, beta, rng):
    f = np.fft.fftfreq(n)[:, None]
    g = np.fft.fftfreq(n)[None, :]
    r = np.sqrt(f * f + g * g)
    r[0, 0] = 1.0
    spec = (rng.standard_normal((n, n)) + 1j * rng.standard_normal((n, n))) / np.power(r, beta)
    spec[0, 0] = 0.0
    out = np.fft.ifft2(spec).real
    return (out - out.mean()) / out.std()


def to01(a, gain=0.28):
    return np.clip(0.5 + gain * a, 0.0, 1.0)   # unit-deviation noise -> 0.5 +- 0.28 (about 96 % inside 0..1)


def main():
    n = 1024
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Saved", "GeneratedTextures", "macro")
    os.makedirs(out_dir, exist_ok=True)
    rng = np.random.default_rng(20261013)
    chans = [to01(fft_noise(n, 2.3, rng)), to01(fft_noise(n, 2.3, rng)), to01(fft_noise(n, 1.6, rng)), to01(fft_noise(n, 1.0, rng))]
    img = np.stack(chans, axis=-1)
    Image.fromarray((img * 255.0 + 0.5).astype(np.uint8), "RGBA").save(os.path.join(out_dir, "T_MOMacroNoise.png"))
    print("wrote", os.path.join(out_dir, "T_MOMacroNoise.png"), "means", [round(float(c.mean()), 3) for c in chans])


if __name__ == "__main__":
    main()
