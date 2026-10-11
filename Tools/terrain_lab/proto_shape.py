import sys
import numpy as np
from proto_world import *
from proto_v6 import make_new, coverage
from proto_view import render

CFG = dict(massif_wl=1_800_000.0, a_mass=430.0, peak_wl=450_000.0, peak_oct=4, peak_gain=0.5, a_peak=470.0, peak_soft=0.10, peak_pow=1.15,
           gully_wl=80_000.0, gully_oct=3, gully_gain=0.5, a_gully=48.0, slope_lo=0.18, slope_hi=0.65, cliff_a=0.0)


FADE = [None, None]   # octaves: (lo, hi) ratio of wavelength to the LOD's min wavelength; None = hard cut
GFADE = [None, None]  # the gully amplitude


def ridged_soft(x, y, salt, octaves, gain, wl0, min_wl, soft):
    px, py = x / wl0, y / wl0
    s = np.zeros_like(x)
    dxs = np.zeros_like(x)
    dys = np.zeros_like(x)
    amp, norm, wl = 1.0, 0.0, wl0
    cut = False
    for k in range(octaves):
        norm += amp
        if FADE[0] is not None:
            w = smoothstep(FADE[0], FADE[1], wl / max(min_wl, 1e-6)) if min_wl > 0 else 1.0
            if np.all(w <= 0):
                cut = True
        elif not cut and wl < min_wl:
            cut = True
        if not cut:
            v, dx, dy = gnoise(px, py, salt + k * 0x9E3779B1)
            val = 1.0 - (np.sqrt(v * v + soft * soft) - soft if soft > 0 else np.abs(v))
            wk = w if FADE[0] is not None else 1.0
            dxs += wk * dx
            dys += wk * dy
            s += amp * wk * val / (1.0 + dxs * dxs + dys * dys)
        amp *= gain
        wl *= 0.5
        px, py = next_octave(px, py)
    return s / norm


def base_terms(xw, yw, seed, min_wl, c):
    m = smoothstep(-0.5, 1.0, fbm(xw, yw, salt_of(seed, 0x7301), 2, 0.5, c["massif_wl"], min_wl))
    pk = ridged_soft(xw, yw, salt_of(seed, 0x7302), c["peak_oct"], c["peak_gain"], c["peak_wl"], min_wl, c["peak_soft"])
    pk = np.power(np.clip(pk, 0, None), c["peak_pow"])
    support = smoothstep(0.05, 0.75, m)
    return m, c["a_mass"] * m + c["a_peak"] * pk * (0.15 + 0.85 * support)


def shape(xw, yw, seed, min_wl, cfg=None):
    c = dict(CFG)
    if cfg:
        c.update(cfg)
    m, base = base_terms(xw, yw, seed, min_wl, c)
    out = base
    gfade = 1.0 if GFADE[0] is None else (smoothstep(GFADE[0], GFADE[1], c["gully_wl"] / max(min_wl, 1e-6)) if min_wl > 0 else 1.0)
    if c["a_gully"] > 0 and (c["gully_wl"] >= min_wl if GFADE[0] is None else np.any(np.asarray(gfade) > 0)):
        e = 6000.0   # 60 m finite-difference baseline: the real implementation uses the analytic gradient of the same terms
        bx = base_terms(xw + e, yw, seed, min_wl, c)[1] - base_terms(xw - e, yw, seed, min_wl, c)[1]
        by = base_terms(xw, yw + e, seed, min_wl, c)[1] - base_terms(xw, yw - e, seed, min_wl, c)[1]
        slope = np.hypot(bx, by) / (2 * e) * 100.0 * 2.0    # metres per metre, VerticalScale 2 included
        sf = smoothstep(c["slope_lo"], c["slope_hi"], slope)
        r = ridged_soft(xw, yw, salt_of(seed, 0x7303), c["gully_oct"], c["gully_gain"], c["gully_wl"], min_wl, 0.05)
        out = out - c["a_gully"] * gfade * sf * (1.0 - np.clip(r * 1.3, 0, 1))
        if c.get("crag_a", 0) > 0 and c["crag_wl"] >= min_wl:
            sc = smoothstep(c["crag_lo"], c["crag_hi"], slope)
            n = 0.5 + 0.5 * fbm(xw, yw, salt_of(seed, 0x7304), 2, 0.5, c["crag_wl"], min_wl)
            out = out + c["crag_a"] * sc * smoothstep(c["crag_edge"] - 0.03, c["crag_edge"] + 0.03, n)
    return out


def mask_fn_factory(region_wl=4_000_000.0, lo=0.15, hi=0.7, gain=1.6):
    def mask_fn(xw, yw, seed, inland, relief, pvf, min_wl):
        rf = np.clip(fbm(xw, yw, salt_of(seed, 0x7201), 2, 0.5, region_wl) * gain, -1, 1)
        return smoothstep(lo, hi, rf) * inland
    return mask_fn


def terrain(seed, px, py, step_cm, cfg=None, mask=None, tun=None):
    mask = mask or mask_fn_factory()
    tun = tun or dict(relief_region_wl=4_000_000.0)
    return sample_base_height_cm(px, py, seed, step_cm, mountain_fn=lambda x, y, s, mw: shape(x, y, s, mw, cfg), tun=tun, mask_fn=mask)


def pipeline(seed, tag, cfg=None, dist_km=(7.0, 2.8), mask=None):
    mask = mask or mask_fn_factory()
    stats_like_test(label="NEW %s" % tag, mountain_fn=lambda x, y, s, mw: shape(x, y, s, mw, cfg), mask_fn=mask, tun=dict(relief_region_wl=4_000_000.0))
    n = 270
    xs = -2.7e6 + 5.4e6 * (np.arange(n) + 0.5) / n
    X, Y = np.meshgrid(xs, xs)
    Hn = terrain(seed, X, Y, 800.0, cfg, mask)
    i = np.unravel_index(np.argmax(Hn), Hn.shape)
    px, py = X[i], Y[i]
    half_m, step_m = 12_000.0, 25.0
    g = np.arange(-half_m, half_m, step_m)
    GX, GY = np.meshgrid(g, g)
    grid = terrain(seed, px + GX * 100.0, py + GY * 100.0, 2500.0, cfg, mask) / 100.0
    print("peak %.0f m; grid max %.0f m" % (Hn[i] / 100, grid.max()))

    def f(xm, ym):
        fx = (xm + half_m) / step_m
        fy = (ym + half_m) / step_m
        ix = np.clip(fx.astype(np.int64), 0, grid.shape[1] - 2)
        iy = np.clip(fy.astype(np.int64), 0, grid.shape[0] - 2)
        tx = np.clip(fx - ix, 0, 1)
        ty = np.clip(fy - iy, 0, 1)
        return grid[iy, ix] * (1 - tx) * (1 - ty) + grid[iy, ix + 1] * tx * (1 - ty) + grid[iy + 1, ix] * (1 - tx) * ty + grid[iy + 1, ix + 1] * tx * ty

    for k, dk in enumerate(dist_km):
        cx, cy = 0.0, -dk * 1000.0
        cz = f(np.array([cx]), np.array([cy]))[0] + (300.0 if dk > 4 else 200.0)
        tz = f(np.array([0.0]), np.array([0.0]))[0] * (0.62 if dk > 4 else 0.7)
        render(f, (cx, cy, cz), (0.0, 0.0, tz), "shape_%s_%d.png" % (tag, k), w=900, h=380, fov_deg=72.0, max_dist=24_000.0, steps=420)
    return grid


if __name__ == "__main__":
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 7
    tag = sys.argv[2] if len(sys.argv) > 2 else "a"
    pipeline(seed, tag)
