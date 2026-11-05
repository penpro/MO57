"""Motion probe: does something on screen move smoothly, or hold and snap? (foliage wind, Oct 18 2026: Docs/PCG.md §9)

    python Tools/motion_probe.py capture <pid> <seconds> <out.npz>              a timestamped frame burst of a game window (PrintWindow:
                                                                                 works when the window is covered); full-res grayscale
    python Tools/motion_probe.py track <burst.npz> x:y[,x:y...] [--plot out.png]  sub-pixel tracking of 64 px patches (top-left corners in
                                                                                 window pixels) + each patch's STEP SHARE and JUMP SHARE

Step share = the share of a patch's travel made in frames faster than 4x its median speed. Jump share = the same, against the median of
the 10 frames around each frame (and over 0.05 px): a hold-and-snap staircase moves in isolated frames between still ones, a gust ramps
over several. Read the jump share on mostly-still foliage, where a few gusts push the step share up (a hornbeam canopy: step 14-57 %,
jump 0-4 %). The Pivot Painter staircase (Docs/PCG.md §9) read 35-37 % jump share, smooth sway 0-5 %, thin crossing grass blades ~25 %
anywhere (the tracker, not the wind): compare the same patches near and far, never a number alone. Hitch frames (> 1.8x the median
interval) are counted, and so are the jumps that fall on them (frame pacing, not the material). Measure with r.AntiAliasingMethod 0
(TSR blends small steps away) and keep every patch off the HUD (the FPS counter's 0.25 s refresh reads as a periodic jump). Pick textured
patches (leaves, twigs against the sky); a patch that drifts onto flat sky stops tracking (its line goes dead flat).

    python Tools/motion_probe.py clip <pid> <seconds> <out.mp4> [--crop w h x y]   a colour clip of the window (ffmpeg), for a person to judge
    python Tools/motion_probe.py activity <burst.npz> [--crop w h x y]             per-pixel motion of a burst: tracking measures coherent motion
                                                                                    only (a flicker of many blades cancels out), this measures it all

Activity (Nov 4 2026, the plants' wind, Docs/PCG.md section 9): how far pixels swing (temporal std, grey levels), how fast (the share of each
pixel's temporal power under 0.7 Hz = slow sway, over 1.2 Hz = flicker) and where (the 64 px tiles moving at a moment against the tiles that
ever move: 1.0 = all of them all the time, the constant sway; low = "here and there"). Compare the same camera before and after, at
r.AntiAliasingMethod 0. Needs a python with numpy (Blender's); ffmpeg on PATH for clip and compare.

    python Tools/motion_probe.py compare <before.mp4> <after.mp4> <out.png>        the same camera before and after, as a player sees it (TSR on): reach and speed, and
                                                                                    a picture: a frame of each, then each clip's motion over 9 s, over 0.75 s at 2 s and at 6 s
                                                                                    (a window that lights up all over is everything moving; patches are "here and there")
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

import numpy as np

PATCH = 64


def capture(pid, seconds, out_path, colour_clip=False, crop=None):
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
        if colour_clip:
            rgb = a[:, :, 2::-1]
            if crop:
                rgb = rgb[crop[3]:crop[3] + crop[1], crop[2]:crop[2] + crop[0]]
            frames.append(rgb.copy())
        else:
            frames.append((0.299 * a[:, :, 2] + 0.587 * a[:, :, 1] + 0.114 * a[:, :, 0]).astype(np.uint8))
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mdc)
    user32.ReleaseDC(h, hdc)
    if colour_clip:
        fps = (len(frames) - 1) / max(times[-1] - times[0], 1e-6)
        fh, fw = frames[0].shape[:2]
        p = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "%dx%d" % (fw, fh), "-r", "%.2f" % fps,
                              "-i", "-", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20", out_path], stdin=subprocess.PIPE)
        for f in frames:
            p.stdin.write(f.tobytes())
        p.stdin.close()
        p.wait()
        print("clip %s: %d frames, %.1f fps, %dx%d" % (out_path, len(frames), fps, fw, fh))
        return
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


def _jump_share(speed, dist, half=5, floor_px=0.05):
    """Share of the travel made in frames faster than 4x the median speed of the frames around them (+-half, the frame itself left out)
    and longer than floor_px (the tracker's noise). A hold-and-snap staircase moves in isolated frames between still ones (high); a gust
    ramps up and down over several frames (low) -- unlike the step share, which a mostly-still patch with a few gusts also reads high."""
    n = len(speed)
    jump = np.zeros(n, dtype=bool)
    for k in range(n):
        around = np.concatenate([speed[max(0, k - half):k], speed[k + 1:k + 1 + half]])
        jump[k] = dist[k] > floor_px and speed[k] > 4 * np.median(around)
    return dist[jump].sum() / max(dist.sum(), 1e-9), jump


def track(path, patches, plot):
    d = np.load(path)
    f, t = d["frames"].astype(np.float32), d["times"]
    if int(d["size"][0]) != f.shape[2]:
        sys.exit("burst was downscaled: track needs a full-resolution capture")
    changed = np.concatenate([[True], np.abs(np.diff(f, axis=0)).mean(axis=(1, 2)) > 0.02])   # drop re-captures of the same frame
    f, t = f[changed], t[changed]
    win = np.outer(np.hanning(PATCH), np.hanning(PATCH))
    dt = np.diff(t)
    hitch = dt > 1.8 * np.median(dt)   # a frame that took about twice as long or more: wind time jumps with it (frame pacing, not the material)
    print("%d frames over %.2f s (%.0f fps), %d hitch frames (> 1.8x the median interval)" % (len(f), t[-1] - t[0], (len(f) - 1) / (t[-1] - t[0]), hitch.sum()))
    trajectories = []
    for x, y in patches:
        steps = [_shift(f[k - 1, y:y + PATCH, x:x + PATCH] * win, f[k, y:y + PATCH, x:x + PATCH] * win) for k in range(1, len(f))]
        steps = np.array(steps)
        dist = np.hypot(steps[:, 0], steps[:, 1])
        speed = dist / np.diff(t)
        med = np.median(speed)
        share = dist[speed > 4 * med].sum() / max(dist.sum(), 1e-9)
        jshare, jump = _jump_share(speed, dist)
        print("patch (%d,%d): median %.2f px/s, max %.1f px/s (%.0fx median), step share %.0f %%, jump share %.0f %% (%d jumps, %d on hitch frames)"
              % (x, y, med, speed.max(), speed.max() / max(med, 1e-9), 100 * share, 100 * jshare, jump.sum(), (jump & hitch).sum()))
        trajectories.append((x, y, np.concatenate([[0.0], np.cumsum(steps[:, 1])]), np.concatenate([[0.0], np.cumsum(steps[:, 0])])))
        # How FAR and how FAST it sways, apart (the median speed above is their product): the RMS of the detrended displacement, and the
        # period of the strongest swing (FFT of the larger component, resampled to even time). Crop wind, Nov 4: "about 20% as much and a
        # little faster".
        tu = np.linspace(t[0], t[-1], len(t))
        comps = []
        for series in (trajectories[-1][2], trajectories[-1][3]):
            s = np.interp(tu, t, series)
            s = s - np.polyval(np.polyfit(tu, s, 1), tu)
            comps.append(s)
        rms = float(np.sqrt(np.mean(comps[0] ** 2) + np.mean(comps[1] ** 2)))
        main = comps[0] if np.var(comps[0]) >= np.var(comps[1]) else comps[1]
        spec = np.abs(np.fft.rfft(main * np.hanning(len(main))))
        freqs = np.fft.rfftfreq(len(main), (tu[-1] - tu[0]) / max(len(tu) - 1, 1))
        k = int(np.argmax(spec[1:])) + 1 if len(spec) > 1 else 0
        # A burst shorter than the slow sway reads its whole length as the "strongest period"; so also: the share of the motion's power
        # above 0.5 Hz (ripples against a slow lean) and the strongest period among the faster parts (> 0.25 Hz).
        power = spec ** 2
        fast = float(power[freqs > 0.5].sum() / max(power[1:].sum(), 1e-12))
        band = np.nonzero(freqs > 0.25)[0]
        kf = band[int(np.argmax(spec[band]))] if len(band) else 0
        print("patch (%d,%d): sway rms %.2f px, strongest period %.2f s, fast share %.0f %%, strongest fast period %.2f s"
              % (x, y, rms, 1.0 / freqs[k] if k and freqs[k] > 0 else float("nan"), 100 * fast, 1.0 / freqs[kf] if kf else float("nan")))
    if plot:
        from PIL import Image, ImageDraw
        width, height = 1100, 80 + 150 * len(trajectories)
        img = Image.new("RGB", (width, height), "white")
        dr = ImageDraw.Draw(img)
        dr.text((10, 5), "%s: patch displacement (1 px = 25 plot px, or less where a label says so: the whole swing always fits)" % path, fill=(0, 0, 0))
        span = t[-1]
        for k, (x, y, dx, dy) in enumerate(trajectories):
            for comp, series in enumerate((dx, dy)):
                base = 60 + k * 150 + comp * 60
                s = series - (series.max() + series.min()) / 2
                scale = min(25.0, 28.0 / max(np.abs(s).max(), 1e-9))
                dr.line([(40 + t[i] / span * (width - 60), base - s[i] * scale) for i in range(len(t))], fill=(200, 40, 40) if comp == 0 else (40, 90, 200), width=2)
                dr.text((2, base - 6), "%s(%d,%d)%s" % ("xy"[comp], x, y, "" if scale >= 25.0 else " 1px=%.1f" % scale), fill=(0, 0, 0))
        for s in range(int(span) + 1):
            xx = 40 + s / span * (width - 60)
            dr.line([(xx, 25), (xx, height - 5)], fill=(225, 225, 225))
        img.save(plot)
        print("plot -> " + plot)


def activity(path, crop=None):
    d = np.load(path)
    frames, times = d["frames"], d["times"]
    if crop:
        frames = frames[:, crop[3]:crop[3] + crop[1], crop[2]:crop[2] + crop[0]]
    changed = np.concatenate([[True], np.abs(np.diff(frames[:, ::8, ::8].astype(np.int16), axis=0)).mean(axis=(1, 2)) > 0.02])   # PrintWindow outruns the game
    frames, times = frames[changed], times[changed]
    n, h, w = frames.shape
    dt = (times[-1] - times[0]) / (n - 1)
    hz = np.fft.rfftfreq(n, dt)
    slow_p, fast_p, swing = 0.0, 0.0, 0.0
    tiles = []
    for y in range(0, h - PATCH + 1, PATCH):        # tile by tile, so a burst of 500 full frames fits in memory
        for x in range(0, w - PATCH + 1, PATCH):
            t = frames[:, y:y + PATCH, x:x + PATCH].astype(np.float32)
            t -= t.mean(axis=0, keepdims=True)
            power = (np.abs(np.fft.rfft(t, axis=0)) ** 2).mean(axis=(1, 2))
            slow_p += power[(hz > 0.05) & (hz < 0.7)].sum()
            fast_p += power[hz > 1.2].sum()
            swing += float(t.std(axis=0).mean())
            tiles.append(np.abs(np.diff(t, axis=0)).mean(axis=(1, 2)))
    tiles = np.stack(tiles)                         # tile x frame: mean |frame difference|
    floor = max(0.25, float(np.percentile(tiles, 5)) * 2)
    moving = tiles > floor
    ever = moving.mean(axis=1) > 0.02
    at_once = float(moving.mean(axis=0).mean())
    print("activity %d frames, %.1f fps: swing %.2f grey levels, speed %.3f levels/frame, slow (<0.7 Hz) %.0f %% of the motion's power, flicker (>1.2 Hz) "
          "%.0f %%, tiles moving at a moment %.0f %% (floor %.2f), of those that ever move %.0f %%" % (
              n, 1 / dt, swing / len(tiles), float(tiles.mean()), 100 * slow_p / max(slow_p + fast_p, 1e-6), 100 * fast_p / max(slow_p + fast_p, 1e-6),
              100 * at_once, floor, 100 * at_once / max(float(ever.mean()), 1e-6)))


def compare(a_path, b_path, out_path, w=640, h=360, seconds=9.5, fps=30):
    def grey(path):
        r = subprocess.run(["ffmpeg", "-loglevel", "error", "-i", path, "-t", str(seconds), "-vf", "fps=%d,scale=%d:%d" % (fps, w, h), "-f", "rawvideo", "-pix_fmt", "gray", "-"], capture_output=True)
        return np.frombuffer(r.stdout, np.uint8).reshape(-1, h, w).astype(np.float32)

    def frame(path):
        r = subprocess.run(["ffmpeg", "-loglevel", "error", "-ss", "2", "-i", path, "-frames:v", "1", "-vf", "scale=%d:%d" % (w, h), "-f", "rawvideo", "-pix_fmt", "rgb24", "-"], capture_output=True)
        return np.frombuffer(r.stdout, np.uint8).reshape(h, w, 3)

    def heat(m, scale=8.0):
        v = np.clip(m / scale, 0, 1)
        return (np.stack([v, v ** 2, 0.25 * (1 - v)], -1) * 255).astype(np.uint8)

    a, b = grey(a_path), grey(b_path)
    da, db = np.abs(np.diff(a, axis=0)), np.abs(np.diff(b, axis=0))

    def window(d, s):
        return d[int(s * fps):int((s + 0.75) * fps)].mean(axis=0)

    img = np.vstack([np.hstack([frame(a_path), frame(b_path)]), np.hstack([heat(da.mean(axis=0)), heat(db.mean(axis=0))]),
                     np.hstack([heat(window(da, 2)), heat(window(db, 2))]), np.hstack([heat(window(da, 6)), heat(window(db, 6))])])
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "%dx%d" % (img.shape[1], img.shape[0]), "-i", "-", "-frames:v", "1", out_path], input=img.tobytes())

    def moving(d):      # the share of 32 px tiles over 1 level / frame, averaged over 0.25 s steps: how much of the view moves at a moment
        n = int(0.25 * fps)
        steps = [d[i:i + n].mean(axis=0) for i in range(0, len(d) - n, n)]
        return float(np.mean([(s[:h // 32 * 32, :w // 32 * 32].reshape(h // 32, 32, w // 32, 32).mean(axis=(1, 3)) > 1.0).mean() for s in steps]))

    print("compare: reach (temporal std) %.2f -> %.2f grey levels, speed %.2f -> %.2f levels/frame, of the view moving at a moment %.0f %% -> %.0f %%" % (
        a.std(axis=0).mean(), b.std(axis=0).mean(), da.mean(), db.mean(), 100 * moving(da), 100 * moving(db)))


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
    cl = sub.add_parser("clip")
    cl.add_argument("pid", type=int)
    cl.add_argument("seconds", type=float)
    cl.add_argument("out")
    cl.add_argument("--crop", type=int, nargs=4, metavar=("W", "H", "X", "Y"))
    ac = sub.add_parser("activity")
    ac.add_argument("burst")
    ac.add_argument("--crop", type=int, nargs=4, metavar=("W", "H", "X", "Y"))
    cm = sub.add_parser("compare")
    cm.add_argument("before")
    cm.add_argument("after")
    cm.add_argument("out")
    a = ap.parse_args()
    if a.cmd == "compare":
        compare(a.before, a.after, a.out)
    elif a.cmd == "capture":
        capture(a.pid, a.seconds, a.out)
    elif a.cmd == "clip":
        capture(a.pid, a.seconds, a.out, colour_clip=True, crop=a.crop)
    elif a.cmd == "activity":
        activity(a.burst, a.crop)
    else:
        track(a.burst, [tuple(int(v) for v in p.split(":")) for p in a.patches.split(",")], a.plot)


if __name__ == "__main__":
    main()
