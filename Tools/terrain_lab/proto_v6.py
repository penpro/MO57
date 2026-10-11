import sys
import numpy as np
from proto_world import *
from proto_view import render

SEED = 7


def make_new(region_wl=4_000_000.0, ridge_wl=1_200_000.0, ridge_oct=5, ridge_gain=0.36, soft=0.15, amp_m=1000.0, region_lo=0.15, region_hi=0.7, region_gain=1.6, core=0.0):
    def mask_fn(xw, yw, seed, inland, relief, pvf, min_wl):
        rf = np.clip(fbm(xw, yw, salt_of(seed, 0x7201), 2, 0.5, region_wl) * region_gain, -1, 1)
        return smoothstep(region_lo, region_hi, rf) * smoothstep(-0.15, 0.4, inland * 0 + inland)

    def ridge_fn(xw, yw, seed, min_wl):
        px, py = xw / ridge_wl, yw / ridge_wl
        s = np.zeros_like(xw)
        dxs = np.zeros_like(xw)
        dys = np.zeros_like(xw)
        amp, norm, wl = 1.0, 0.0, ridge_wl
        cut = False
        for k in range(ridge_oct):
            norm += amp
            if not cut and wl < min_wl:
                cut = True
            if not cut:
                v, dx, dy = gnoise(px, py, salt_of(seed, 0x7202) + k * 0x9E3779B1)
                val = 1.0 - (np.sqrt(v * v + soft * soft) - soft if soft > 0 else np.abs(v))
                dxs += dx
                dys += dy
                s += amp * val / (1.0 + dxs * dxs + dys * dys)
            amp *= ridge_gain
            wl *= 0.5
            px, py = next_octave(px, py)
        return amp_m * s / norm
    return mask_fn, ridge_fn


def coverage(label, mask_fn=None, mountain_fn=None, seeds=(7, 18, 424242, 12345), n=150):
    fr_on, fr_ramp, fr_land = [], [], []
    for seed in seeds:
        xs = -2.7e6 + 5.4e6 * (np.arange(n) + 0.5) / n
        X, Y = np.meshgrid(xs, xs)
        H, p = sample_base_height_cm(X, Y, seed, 800.0, mountain_fn, parts=True, mask_fn=mask_fn)
        land = H > 0
        m = p["mountainous"]
        fr_on.append(np.mean(m[land] >= 0.98))
        fr_ramp.append(np.mean((m[land] > 0.02) & (m[land] < 0.98)))
    print("%s: land with the mountain mask fully on %.1f%%, in the ramp %.1f%%" % (label, 100 * np.mean(fr_on), 100 * np.mean(fr_ramp)))


if __name__ == "__main__":
    stats_like_test(label="OLD v5.1 ")
    coverage("OLD")
    mask_fn, ridge_fn = make_new()
    stats_like_test(label="NEW range ", mountain_fn=ridge_fn, mask_fn=mask_fn)
    coverage("NEW", mask_fn, ridge_fn)
