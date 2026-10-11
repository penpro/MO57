"""Numpy prototype of the generator's mountain term (same gradient noise formulation as MOWorldGenNoise.h, a different hash: statistics, not bits)."""
import numpy as np

G2 = np.array([[1.0, 0.0], [0.92387953, 0.38268343], [0.70710678, 0.70710678], [0.38268343, 0.92387953],
               [0.0, 1.0], [-0.38268343, 0.92387953], [-0.70710678, 0.70710678], [-0.92387953, 0.38268343],
               [-1.0, 0.0], [-0.92387953, -0.38268343], [-0.70710678, -0.70710678], [-0.38268343, -0.92387953],
               [0.0, -1.0], [0.38268343, -0.92387953], [0.70710678, -0.70710678], [0.92387953, -0.38268343]], dtype=np.float64)


def _hash(xi, yi, salt):
    h = xi.astype(np.uint64) * np.uint64(0x9E3779B97F4A7C15)
    h ^= (yi.astype(np.uint64) + np.uint64(0xC2B2AE3D27D4EB4F)) * np.uint64(0xD6E8FEB86659FD93)
    h ^= np.uint64(salt) * np.uint64(0xFF51AFD7ED558CCD)
    h ^= h >> np.uint64(32)
    h *= np.uint64(0xD6E8FEB86659FD93)
    h ^= h >> np.uint64(32)
    h *= np.uint64(0xD6E8FEB86659FD93)
    h ^= h >> np.uint64(32)
    return (h & np.uint64(15)).astype(np.int64)


def gnoise(x, y, salt):
    """Gradient noise, value ~[-1,1] and derivatives wrt the inputs (lattice units): same formulation as GradientNoise2."""
    fx0, fy0 = np.floor(x), np.floor(y)
    xi, yi = fx0.astype(np.int64), fy0.astype(np.int64)
    fx, fy = x - fx0, y - fy0
    ux = fx ** 3 * (fx * (fx * 6 - 15) + 10)
    uy = fy ** 3 * (fy * (fy * 6 - 15) + 10)
    dux = 30 * fx ** 2 * (fx - 1) ** 2
    duy = 30 * fy ** 2 * (fy - 1) ** 2
    ga = G2[_hash(xi, yi, salt)]
    gb = G2[_hash(xi + 1, yi, salt)]
    gc = G2[_hash(xi, yi + 1, salt)]
    gd = G2[_hash(xi + 1, yi + 1, salt)]
    va = ga[..., 0] * fx + ga[..., 1] * fy
    vb = gb[..., 0] * (fx - 1) + gb[..., 1] * fy
    vc = gc[..., 0] * fx + gc[..., 1] * (fy - 1)
    vd = gd[..., 0] * (fx - 1) + gd[..., 1] * (fy - 1)
    k = va - vb - vc + vd
    s = 1.41421356
    val = s * (va + ux * (vb - va) + uy * (vc - va) + ux * uy * k)
    dx = s * (ga[..., 0] + ux * (gb[..., 0] - ga[..., 0]) + uy * (gc[..., 0] - ga[..., 0]) + ux * uy * (ga[..., 0] - gb[..., 0] - gc[..., 0] + gd[..., 0]) + dux * (uy * k + vb - va))
    dy = s * (ga[..., 1] + ux * (gb[..., 1] - ga[..., 1]) + uy * (gc[..., 1] - ga[..., 1]) + ux * uy * (ga[..., 1] - gb[..., 1] - gc[..., 1] + gd[..., 1]) + duy * (ux * k + vc - va))
    return val, dx, dy


def next_octave(px, py):
    return (0.8 * px - 0.6 * py) * 2.0, (0.6 * px + 0.8 * py) * 2.0


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def fbm(x, y, salt, octaves, gain, wavelength, min_wl=0.0):
    px, py = x / wavelength, y / wavelength
    s = np.zeros_like(x)
    amp, sumsq, wl = 1.0, 0.0, wavelength
    for k in range(octaves):
        if wl >= min_wl:
            s += amp * gnoise(px, py, salt + k * 0x9E3779B1)[0]
        sumsq += amp * amp
        amp *= gain
        wl *= 0.5
        px, py = next_octave(px, py)
    return s / np.sqrt(sumsq)


def eroded_fbm(x, y, salt, octaves, gain, wavelength, min_wl, ridged):
    """The current generator's ErodedFbm (lattice-unit slope damping)."""
    px, py = x / wavelength, y / wavelength
    s = np.zeros_like(x)
    dxs = np.zeros_like(x)
    dys = np.zeros_like(x)
    amp, norm, wl = 1.0, 0.0, wavelength
    cut = False
    for k in range(octaves):
        norm += amp
        if not cut and wl < min_wl:
            cut = True
        if not cut:
            v, dx, dy = gnoise(px, py, salt + k * 0x9E3779B1)
            val = 1.0 - np.abs(v) if ridged else v
            dxs += dx
            dys += dy
            s += amp * val / (1.0 + dxs * dxs + dys * dys)
        amp *= gain
        wl *= 0.5
        px, py = next_octave(px, py)
    return s / norm


def old_mountain(x, y, min_wl=0.0):
    """MountainM / amplitude / Mountainous / scale at full strength (the current v5 shape)."""
    return eroded_fbm(x, y, 0x7006, 6, 0.38, 1_000_000.0, min_wl, True)


def grid(extent_cm=2_400_000.0, step_cm=4_000.0, ox=0.0, oy=0.0):
    n = int(extent_cm / step_cm)
    xs = ox + (np.arange(n) - n / 2) * step_cm
    ys = oy + (np.arange(n) - n / 2) * step_cm
    return np.meshgrid(xs, ys)


def slope_stats(h_cm, step_cm, label, base_cells=1):
    gy, gx = np.gradient(h_cm, step_cm)
    sl = np.degrees(np.arctan(np.hypot(gx, gy)))
    p = np.percentile(sl, [50, 90, 99])
    print("%-34s slope deg p50 %.1f p90 %.1f p99 %.1f | >30: %.1f%% >45: %.1f%% >60: %.2f%% | height m min %.0f p50 %.0f p95 %.0f max %.0f" % (
        label, p[0], p[1], p[2], 100 * np.mean(sl > 30), 100 * np.mean(sl > 45), 100 * np.mean(sl > 60),
        h_cm.min() / 100, np.percentile(h_cm, 50) / 100, np.percentile(h_cm, 95) / 100, h_cm.max() / 100))
    return sl


if __name__ == "__main__":
    X, Y = grid(extent_cm=3_000_000.0, step_cm=4_000.0)   # 30 km at 40 m (the test's slope baseline)
    amp_cm = 100.0 * 1000.0 * 2.0   # amplitude 1000 m x VerticalScale 2
    h = amp_cm * old_mountain(X, Y)
    slope_stats(h, 4_000.0, "OLD v5 mountain (full strength)")
