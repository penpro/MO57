"""Motion probe: does something on screen move smoothly, or hold and snap? (foliage wind, Oct 18 2026: Docs/PCG.md §9)

    python Tools/motion_probe.py capture <pid> <seconds> <out.npz>              a timestamped frame burst of a game window (PrintWindow:
                                                                                 works when the window is covered); full-res grayscale
    python Tools/motion_probe.py track <burst.npz> x:y[,x:y...] [--plot out.png]  sub-pixel tracking of 64 px patches (top-left corners in
                                                                                 window pixels) + each patch's STEP SHARE

Step share = the share of a patch's travel made in frames faster than 4x its median speed. Smooth sway reads ~0-2 %; the Pivot Painter
staircase (Docs/PCG.md §9) read 34-47 %. Measure with r.AntiAliasingMethod 0 (TSR blends small steps away) and keep every patch off the
HUD (the FPS counter's 0.25 s refresh reads as a periodic jump). Pick textured patches (leaves, twigs against the sky); a patch that drifts
onto flat sky stops tracking (its line goes dead flat).
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import sys
import time

import numpy as np

PATCH = 64


def capture(pid, seconds, out_path):
    user32, gdi32 = ctypes.windll.user32, ctypes.windll.gdi32
    user32.SetProcessDPIAware()
    found = []
    enum_proc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)

    def cb(h, _):
        p = wt.DWORD()
        user32.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and user32.IsWindowVisible(h):
            r = wt.RECT()
            user32.GetClientRect(h, ctypes.byref(r))
            if r.right * r.bottom > 10000:
                found.append((h, r.right, r.bottom))
        return True

    user32.EnumWindows(enum_proc(cb), 0)
    if not found:
        sys.exit("no visible window for pid %d" % pid)
    h, w, ht = max(found, key=lambda t: t[1] * t[2])
    hdc = user32.GetDC(h)
    mdc = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, w, ht)
    gdi32.SelectObject(mdc, bmp)

    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG), ("biPlanes", wt.WORD), ("biBitCount", wt.WORD),
                    ("biCompression", wt.DWORD), ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", wt.LONG), ("biYPelsPerMeter", wt.LONG),
                    ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD)]

    bi = BITMAPINFOHEADER(ctypes.sizeof(BITMAPINFOHEADER), w, -ht, 1, 32, 0, 0, 0, 0, 0, 0)
    buf = (ctypes.c_ubyte * (w * ht * 4))()
    frames, times = [], []
    t0, t0_epoch = time.perf_counter(), time.time()
    while time.perf_counter() - t0 < seconds:
        user32.PrintWindow(h, mdc, 2)   # PW_RENDERFULLCONTENT
        times.append(time.perf_counter() - t0)
        gdi32.GetDIBits(mdc, bmp, 0, ht, buf, ctypes.byref(bi), 0)
        a = np.frombuffer(buf, dtype=np.uint8).reshape(ht, w, 4)
        frames.append((0.299 * a[:, :, 2] + 0.587 * a[:, :, 1] + 0.114 * a[:, :, 0]).astype(np.uint8))
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mdc)
    user32.ReleaseDC(h, hdc)
    np.savez_compressed(out_path, frames=np.stack(frames), times=np.array(times), size=np.array([w, ht]), t0_epoch=t0_epoch)
    print("captured %d frames in %.2f s of %dx%d -> %s" % (len(frames), times[-1], w, ht, out_path))


def _shift(a, b):
    """Sub-pixel translation of b relative to a (phase correlation + parabolic peak)."""
    fa, fb = np.fft.fft2(a - a.mean()), np.fft.fft2(b - b.mean())
    r = fa * np.conj(fb)
    r = np.real(np.fft.ifft2(r / (np.abs(r) + 1e-6)))
    iy, ix = np.unravel_index(np.argmax(r), r.shape)

    def sub(k, n, along_y):
        y0, y1, y2 = (r[(k - 1) % n, ix], r[k, ix], r[(k + 1) % n, ix]) if along_y else (r[iy, (k - 1) % n], r[iy, k], r[iy, (k + 1) % n])
        den = y0 - 2 * y1 + y2
        v = k + (0.5 * (y0 - y2) / den if abs(den) > 1e-9 else 0.0)
        return v - n if v > n / 2 else v
    return sub(iy, r.shape[0], True), sub(ix, r.shape[1], False)


def track(path, patches, plot):
    d = np.load(path)
    f, t = d["frames"].astype(np.float32), d["times"]
    if int(d["size"][0]) != f.shape[2]:
        sys.exit("burst was downscaled: track needs a full-resolution capture")
    changed = np.concatenate([[True], np.abs(np.diff(f, axis=0)).mean(axis=(1, 2)) > 0.02])   # drop re-captures of the same frame
    f, t = f[changed], t[changed]
    win = np.outer(np.hanning(PATCH), np.hanning(PATCH))
    print("%d frames over %.2f s (%.0f fps)" % (len(f), t[-1] - t[0], (len(f) - 1) / (t[-1] - t[0])))
    trajectories = []
    for x, y in patches:
        steps = [_shift(f[k - 1, y:y + PATCH, x:x + PATCH] * win, f[k, y:y + PATCH, x:x + PATCH] * win) for k in range(1, len(f))]
        steps = np.array(steps)
        dist = np.hypot(steps[:, 0], steps[:, 1])
        speed = dist / np.diff(t)
        med = np.median(speed)
        share = dist[speed > 4 * med].sum() / max(dist.sum(), 1e-9)
        print("patch (%d,%d): median %.2f px/s, max %.1f px/s (%.0fx median), step share %.0f %%" % (x, y, med, speed.max(), speed.max() / max(med, 1e-9), 100 * share))
        trajectories.append((x, y, np.concatenate([[0.0], np.cumsum(steps[:, 1])]), np.concatenate([[0.0], np.cumsum(steps[:, 0])])))
    if plot:
        from PIL import Image, ImageDraw
        width, height = 1100, 80 + 150 * len(trajectories)
        img = Image.new("RGB", (width, height), "white")
        dr = ImageDraw.Draw(img)
        dr.text((10, 5), "%s: patch displacement, 1 px = 25 plot px" % path, fill=(0, 0, 0))
        span = t[-1]
        for k, (x, y, dx, dy) in enumerate(trajectories):
            for comp, series in enumerate((dx, dy)):
                base = 60 + k * 150 + comp * 60
                s = series - np.median(series)
                dr.line([(40 + t[i] / span * (width - 60), base - np.clip(s[i] * 25, -28, 28)) for i in range(len(t))], fill=(200, 40, 40) if comp == 0 else (40, 90, 200), width=2)
                dr.text((2, base - 6), "%s(%d,%d)" % ("xy"[comp], x, y), fill=(0, 0, 0))
        for s in range(int(span) + 1):
            xx = 40 + s / span * (width - 60)
            dr.line([(xx, 25), (xx, height - 5)], fill=(225, 225, 225))
        img.save(plot)
        print("plot -> " + plot)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("capture")
    c.add_argument("pid", type=int)
    c.add_argument("seconds", type=float)
    c.add_argument("out")
    tr = sub.add_parser("track")
    tr.add_argument("burst")
    tr.add_argument("patches", help="x:y[,x:y...] top-left corners of 64 px patches, window pixels")
    tr.add_argument("--plot")
    a = ap.parse_args()
    if a.cmd == "capture":
        capture(a.pid, a.seconds, a.out)
    else:
        track(a.burst, [tuple(int(v) for v in p.split(":")) for p in a.patches.split(",")], a.plot)


if __name__ == "__main__":
    main()
