import numpy as np
from PIL import Image


def render(height_fn, cam, target, path, w=720, h=300, fov_deg=70.0, max_dist=24_000.0, steps=500, **_):
    """Heightfield ray-marcher with bisection refinement (units: metres)."""
    cx, cy, cz = cam
    tx, ty, tz = target
    fwd = np.array([tx - cx, ty - cy, tz - cz], dtype=np.float64)
    fwd /= np.linalg.norm(fwd)
    right = np.cross(fwd, [0, 0, 1.0])
    right /= np.linalg.norm(right)
    up = np.cross(right, fwd)
    aspect = w / h
    tanf = np.tan(np.radians(fov_deg) / 2)
    u = (np.arange(w) + 0.5) / w * 2 - 1
    v = 1 - (np.arange(h) + 0.5) / h * 2
    U, V = np.meshgrid(u * tanf * aspect, v * tanf)
    d = fwd[None, None, :] + U[..., None] * right[None, None, :] + V[..., None] * up[None, None, :]
    d /= np.linalg.norm(d, axis=-1, keepdims=True)

    def above(t):
        return (cz + d[..., 2] * t) - height_fn(cx + d[..., 0] * t, cy + d[..., 1] * t)

    t_prev = np.full((h, w), 5.0)
    t = np.full((h, w), 5.0)
    hit = np.zeros((h, w), dtype=bool)
    lo = t_prev.copy()
    hi = t_prev.copy()
    for i in range(steps):
        dt = np.maximum(8.0, 0.004 * t)
        t_new = t + dt
        a = above(t_new)
        newly = (a < 0) & ~hit
        lo = np.where(newly, t, lo)
        hi = np.where(newly, t_new, hi)
        hit |= newly
        t = np.where(hit, t, t_new)
        if t.min() > max_dist:
            break
        t = np.where(t > max_dist, max_dist, t)
        if (hit | (t >= max_dist)).all():
            break
    for _ in range(10):
        mid = 0.5 * (lo + hi)
        below = above(mid) < 0
        hi = np.where(below, mid, hi)
        lo = np.where(below, lo, mid)
    ts = 0.5 * (lo + hi)
    px = cx + d[..., 0] * ts
    py = cy + d[..., 1] * ts
    e = 12.0
    hx = height_fn(px + e, py) - height_fn(px - e, py)
    hy = height_fn(px, py + e) - height_fn(px, py - e)
    n = np.stack([-hx / (2 * e), -hy / (2 * e), np.ones_like(hx)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    light = np.array([0.55, -0.35, 0.55])
    light /= np.linalg.norm(light)
    lam = np.clip((n * light).sum(-1), 0.0, 1)
    hz = pz = height_fn(px, py)
    alt = np.clip(hz / 1500.0, 0, 1)
    rock = np.stack([0.50 + 0.12 * alt, 0.43 + 0.12 * alt, 0.34 + 0.14 * alt], -1)
    steep = np.clip((np.sqrt(hx * hx + hy * hy) / (2 * e) - 0.5) * 1.5, 0, 1)[..., None]
    rock = rock * (1 - 0.35 * steep)
    col = rock * (0.18 + 0.95 * lam)[..., None]
    fog = 1 - np.exp(-ts / 12000.0)
    skycol = np.array([0.80, 0.78, 0.74])
    col = col * (1 - fog[..., None]) + skycol * fog[..., None]
    yy = (np.arange(h)[:, None] / h) * np.ones((1, w))
    sk = np.stack([0.60 + 0.25 * yy, 0.68 + 0.18 * yy, 0.86 + 0.06 * yy], -1)
    col = np.where(hit[..., None], col, sk)
    Image.fromarray((np.clip(col, 0, 1) * 255).astype(np.uint8)).save(path)
