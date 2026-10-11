"""Numpy port of FMOWorldGen::SampleBaseHeightCm (v5.1) -- statistics-faithful (own hash), not bit-faithful. Validate against the test numbers before trusting."""
import numpy as np
from proto_mtn import gnoise, next_octave, smoothstep, fbm, eroded_fbm

CONT_SPLINE = [(-1.00, -80.0), (-0.85, -72.0), (-0.70, -50.0), (-0.62, -28.0), (-0.58, -12.0), (-0.55, 0.0), (-0.30, 6.0), (-0.15, 12.0), (0.00, 18.0), (0.25, 32.0), (0.55, 55.0), (1.00, 90.0)]
ERO_SPLINE = [(-1.00, 1.15), (-0.50, 0.95), (0.00, 0.55), (0.45, 0.20), (1.00, 0.05)]


def eval_spline(knots, x):
    xs = np.array([k[0] for k in knots])
    ys = np.array([k[1] for k in knots])
    n = len(knots)
    x = np.clip(x, xs[0], xs[-1])
    i = np.clip(np.searchsorted(xs, x, side="right") - 1, 0, n - 2)
    x0, x1, y0, y1 = xs[i], xs[i + 1], ys[i], ys[i + 1]
    h = np.maximum(x1 - x0, 1e-6)
    t = (x - x0) / h
    sec = (y1 - y0) / h
    im1 = np.maximum(i - 1, 0)
    ip2 = np.minimum(i + 2, n - 1)
    m0 = np.where(i > 0, (ys[i + 1] - ys[im1]) / np.maximum(xs[i + 1] - xs[im1], 1e-6), sec)
    m1 = np.where(i + 2 < n, (ys[ip2] - ys[i]) / np.maximum(xs[ip2] - xs[i], 1e-6), sec)
    t2, t3 = t * t, t * t * t
    return (2 * t3 - 3 * t2 + 1) * y0 + (t3 - 2 * t2 + t) * h * m0 + (-2 * t3 + 3 * t2) * y1 + (t3 - t2) * h * m1


def salt_of(seed, salt):
    return (seed * 0x9E3779B1 ^ salt * 0x85EBCA6B) & 0xFFFFFFFF


T = dict(cont_wl=3_000_000.0, ero_wl=450_000.0, weird_wl=300_000.0, moist_wl=175_000.0, temp_wl=375_000.0, contrast=1.9, warp=70_000.0, warp_wl=500_000.0,
         vs=2.0, mnt_amp=1000.0, mnt_wl=1_000_000.0, mnt_gain=0.38, hill_amp=26.0, hill_wl=150_000.0, coast_amp=200.0, coast_wl=500_000.0, coast_pres_wl=1_200_000.0,
         det_amp=0.9, det_wl=3500.0, lowland_cap=20.0, blend=0.12, coastal_moist=0.12, half=6_000_000.0)


def soft_band(v, lo, hi, blend):
    return smoothstep(lo - blend, lo, v) * (1 - smoothstep(hi, hi + blend, v))


def sample_base_height_cm(x, y, seed, step_cm=400.0, mountain_fn=None, parts=False, biome_relief=True, tun=None, mask_fn=None):
    t = dict(T)
    if tun:
        t.update(tun)
    min_wl = 2.0 * max(step_cm, 1.0)
    # warp
    px, py = x / t["warp_wl"], y / t["warp_wl"]
    sx, sy = salt_of(seed, 0xA101), salt_of(seed, 0xA102)
    wx = gnoise(px, py, sx)[0] + 0.5 * gnoise(px * 2 + 11.7, py * 2 - 5.3, sx + 1)[0]
    wy = gnoise(px + 7.31, py - 3.17, sy)[0] + 0.5 * gnoise(px * 2 - 4.1, py * 2 + 9.9, sy + 1)[0]
    xw = x + t["warp"] * wx * 0.66
    yw = y + t["warp"] * wy * 0.66

    def field(salt, octs, wl):
        return np.clip(fbm(xw, yw, salt_of(seed, salt), octs, 0.5, wl) * t["contrast"], -1, 1)

    cont = field(0xC001, 4, t["cont_wl"])
    # island edge (interior window: negligible, kept for completeness)
    nx, ny = xw / t["half"], yw / t["half"]
    edge = np.sqrt(np.sqrt(nx ** 4 + ny ** 4))
    pull = smoothstep(0.62, 0.93, edge)
    cont = cont + (-1 - cont) * pull
    edge_land = 1 - pull
    ero = field(0xE002, 3, t["ero_wl"])
    weird = field(0xD003, 3, t["weird_wl"])
    pv = 1.0 - np.abs(3.0 * np.abs(weird) - 2.0)
    coastal = 1.0 - smoothstep(-0.25, 0.25, cont)
    moist = np.clip(0.5 + 0.45 * fbm(xw, yw, salt_of(seed, 0xB004), 3, 0.5, t["moist_wl"]) * t["contrast"] + t["coastal_moist"] * coastal, 0, 1)

    base = eval_spline(CONT_SPLINE, cont)
    inland = smoothstep(-0.15, 0.4, cont)
    relief = np.maximum(eval_spline(ERO_SPLINE, ero), 0.0)
    pvf = np.clip(0.5 + 0.5 * pv, 0, 1)
    # biome relief of MakeReliefParams: Forest (moisture >= 0.4, 1/1) over Meadow (everything else, 0.30/0.03)
    if biome_relief:
        mo = moist
        if t.get("relief_region_wl"):
            mo = np.clip(0.5 + 0.45 * fbm(xw, yw, salt_of(seed, 0xB104), 2, 0.5, t["relief_region_wl"]) * t["contrast"] + t["coastal_moist"] * coastal, 0, 1)
        fit_f = soft_band(mo, 0.4, 1.0, t["blend"])
        rem = 1 - fit_f
        hill_s = fit_f * 1.0 + rem * 0.30
        mtn_s = fit_f * 1.0 + rem * 0.03
        # the remaining (1-fit)*(1-fitMeadow=0) neutral share is 0 because Meadow fits everything
    else:
        hill_s = np.ones_like(x)
        mtn_s = np.ones_like(x)
    hill_s = hill_s * edge_land
    mtn_s = mtn_s * edge_land
    flat = np.minimum(base, t["lowland_cap"])
    base = np.where(hill_s < 1.0, flat + hill_s * (base - flat), base)

    mountainous = smoothstep(0.2, 0.75, inland * relief * (0.6 + 0.4 * pvf)) if mask_fn is None else mask_fn(xw, yw, seed, inland, relief, pvf, min_wl)
    if mountain_fn is None:
        ridge = eroded_fbm(xw, yw, salt_of(seed, 0x7006), 6, t["mnt_gain"], t["mnt_wl"], min_wl, True)
        mnt = t["mnt_amp"] * mountainous * ridge * mtn_s
    else:
        mnt = mountain_fn(xw, yw, seed, min_wl) * mountainous * mtn_s
    # coastal ranges
    band = smoothstep(-0.52, -0.28, cont) * (1 - smoothstep(-0.05, 0.3, cont))
    presence = smoothstep(-0.3, 0.6, fbm(xw, yw, salt_of(seed, 0x400A), 2, 0.5, t["coast_pres_wl"]))
    cr = eroded_fbm(xw, yw, salt_of(seed, 0x4009), 6, 0.5, t["coast_wl"], min_wl, True)
    coast_m = t["coast_amp"] * band * presence * cr * mtn_s
    sea_fade = smoothstep(-0.62, -0.42, cont)
    hill = t["hill_amp"] * (0.35 + 0.65 * np.minimum(relief, 1.0)) * hill_s * sea_fade * eroded_fbm(xw, yw, salt_of(seed, 0x5007), 4, 0.5, t["hill_wl"], min_wl, False)
    det = np.clip(t["det_amp"] * fbm(xw, yw, salt_of(seed, 0x3008), 3, 0.5, t["det_wl"], min_wl) * 1.6, -t["det_amp"], t["det_amp"])
    total = 100.0 * t["vs"] * (base + mnt + coast_m + hill + det)
    if parts:
        return total, dict(base=base, mnt=mnt, coast=coast_m, hill=hill, mountainous=mountainous, moist=moist, cont=cont)
    return total


def stats_like_test(seeds=(7, 18, 424242, 12345), mountain_fn=None, tun=None, n=150, label="", mask_fn=None):
    allland, slopes = [], []
    highest = 0.0
    for seed in seeds:
        xs = -2.7e6 + 5.4e6 * (np.arange(n) + 0.5) / n
        X, Y = np.meshgrid(xs, xs)
        H = sample_base_height_cm(X, Y, seed, 800.0, mountain_fn, tun=tun, mask_fn=mask_fn)
        highest = max(highest, H.max())
        land = H > 0
        allland.append(H[land])
        # slopes at the test's 40 m baseline on a third of the land points
        sel = land & (((np.arange(n)[None, :] + np.arange(n)[:, None]) % 3) == 0)
        px, py = X[sel], Y[sel]
        e = 2000.0
        dx = sample_base_height_cm(px + e, py, seed, 400.0, mountain_fn, tun=tun, mask_fn=mask_fn) - sample_base_height_cm(px - e, py, seed, 400.0, mountain_fn, tun=tun, mask_fn=mask_fn)
        dy = sample_base_height_cm(px, py + e, seed, 400.0, mountain_fn, tun=tun, mask_fn=mask_fn) - sample_base_height_cm(px, py - e, seed, 400.0, mountain_fn, tun=tun, mask_fn=mask_fn)
        slopes.append(np.degrees(np.arctan(np.hypot(dx, dy) / 4000.0)))
    land = np.concatenate(allland)
    sl = np.concatenate(slopes)
    pl = np.percentile(land, [50, 95, 99]) / 100
    ps = np.percentile(sl, [50, 90, 99])
    print("%s highest %.0f m | land p50 %.0f p95 %.0f p99 %.0f m | slope p50 %.1f p90 %.1f p99 %.1f deg | >45: %.1f%% >60: %.2f%% >70: %.2f%%" % (
        label, highest / 100, pl[0], pl[1], pl[2], ps[0], ps[1], ps[2], 100 * np.mean(sl > 45), 100 * np.mean(sl > 60), 100 * np.mean(sl > 70)))
    return sl


if __name__ == "__main__":
    stats_like_test(label="PORT (v5.1)")
    print("real test (seed 7/18/424242/12345): highest 1652/1564/1485/1451 m, land p50 ~72 p95 ~660 p99 ~970 | slope p50 3.9 p90 36.1 p99 77.8 | >45 6.9% >60 3.39% >70 1.92%")
