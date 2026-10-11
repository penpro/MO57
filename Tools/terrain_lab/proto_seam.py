"""LOD-to-LOD mismatch: at the same XY, how much does the surface differ between a LOD step and the next coarser one (the seam crack a transition cell has to hide)."""
import numpy as np
import proto_shape as ps
from proto_world import *

CFG = dict(a_mass=515.0, a_peak=580.0, peak_wl=420_000.0, peak_soft=0.22, peak_pow=1.0, a_gully=70.0, gully_wl=60_000.0, slope_lo=0.15, slope_hi=0.55)
MASK = ps.mask_fn_factory(region_wl=3_000_000.0, lo=-0.25, hi=0.33)
TUN = dict(relief_region_wl=3_000_000.0)


def new_h(x, y, seed, step_cm, cfg=CFG):
    return sample_base_height_cm(x, y, seed, step_cm, mountain_fn=lambda a, b, s, mw: ps.shape(a, b, s, mw, cfg), tun=TUN, mask_fn=MASK)


def old_h(x, y, seed, step_cm):
    return sample_base_height_cm(x, y, seed, step_cm)


def report(name, fn, seed=7):
    # mountain ground: a 6 km window around the old+new summit region
    rng = np.random.default_rng(5)
    px = -2.7e6 + 5.4e6 * rng.random(60000)
    py = -2.7e6 + 5.4e6 * rng.random(60000)
    h0 = fn(px, py, seed, 50.0)
    sel = h0 > 40000.0    # ground above 400 m: mountain
    px, py, h0 = px[sel], py[sel], h0[sel]
    print("%s: %d mountain samples (>400 m)" % (name, len(px)))
    prev = h0
    for lod in range(1, 14):
        step = 50.0 * (2 ** lod)
        h = fn(px, py, seed, step)
        d_prev = np.abs(h - prev)
        d_fine = np.abs(h - h0)
        print("  LOD %2d step %7.1f m: vs the next-finer LOD p50 %5.1f p99 %6.1f max %6.1f m | vs LOD0 p99 %6.1f max %6.1f m" % (
            lod, step / 100, np.median(d_prev) / 100, np.percentile(d_prev, 99) / 100, d_prev.max() / 100, np.percentile(d_fine, 99) / 100, d_fine.max() / 100))
        prev = h


if __name__ == "__main__":
    report("OLD v5.1", old_h)
    report("NEW v6 (gully 70, hard cull)", new_h)
