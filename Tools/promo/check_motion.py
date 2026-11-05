"""MO57 promo: what the eye would catch and a still cannot show. Per-frame difference across a finished cut, shot by shot: a frozen frame (no change while the shot moves), a jump (a spike that is not a cut).

    python Tools/promo/check_motion.py [Saved/Promo/out/picture_cut.mov]
    python Tools/promo/check_motion.py --sources     every graded shot, frame by frame, against its source: a frame the graph lost shows as an offset from that frame on

Prints one line per shot with its median and worst frame-to-frame change, and every suspect frame. Flares and hits at the cuts are the film's own; judge those by looking.
"""
import os
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import film  # noqa: E402

W, H = 256, 144


def diffs(path):
    p = subprocess.Popen(["ffmpeg", "-loglevel", "error", "-i", path, "-vf", "scale=%d:%d,format=gray" % (W, H), "-f", "rawvideo", "-"], stdout=subprocess.PIPE)
    prev, out = None, []
    while True:
        buf = p.stdout.read(W * H)
        if len(buf) < W * H:
            break
        a = np.frombuffer(buf, dtype=np.uint8).astype(np.float32)
        out.append(0.0 if prev is None else float(np.abs(a - prev).mean()))
        prev = a
    p.wait()
    return np.array(out)


def frames_of(path, extra=()):
    p = subprocess.Popen(["ffmpeg", "-loglevel", "error"] + list(extra) + ["-i", path, "-vf", "scale=%d:%d,format=gray" % (W, H), "-f", "rawvideo", "-"], stdout=subprocess.PIPE)
    out = []
    while True:
        buf = p.stdout.read(W * H)
        if len(buf) < W * H:
            break
        out.append(np.frombuffer(buf, dtype=np.uint8).astype(np.float32))
    p.wait()
    return out


def sources():
    bad = 0
    for s in film.SHOTS:
        if s.kind == "black":
            continue
        graded = frames_of(os.path.join(ROOT, "Saved", "Promo", "graded", s.id + ".mov"))
        mezz = os.path.join(ROOT, "Saved", "Promo", "mezz", s.id + ".mov")
        if s.kind == "game":
            src = frames_of(mezz)
        else:
            d = os.path.join(ROOT, "Saved", "Promo", "kit", s.id)
            first = min(int(f[2:6]) for f in os.listdir(d) if f.startswith("f_") and f.endswith(".png"))
            src = frames_of(os.path.join(d, "f_%04d.png"), ("-framerate", "24", "-start_number", str(first)))
        # brightness differs after the grade: compare each frame's shape to its neighbours in the source
        off = []
        for i, g in enumerate(graded):
            g0 = g - g.mean()
            best = min(range(max(0, i - 2), min(len(src), i + 3)), key=lambda j: float(np.abs((src[j] - src[j].mean()) - g0).mean()))
            off.append(best - i)
        wrong = [i for i, v in enumerate(off) if v != 0]
        print("%-20s graded %d source %d  frames that do not match their own source: %d %s" % (s.id, len(graded), len(src), len(wrong), wrong[:8]), flush=True)
        bad += len(wrong)
    print("mismatched frames:", bad)


def main():
    if "--sources" in sys.argv:
        sources()
        return
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Saved", "Promo", "out", "picture_cut.mov")
    d = diffs(path)
    print("%d frames" % len(d))
    bad = 0
    for s in film.SHOTS:
        a, b = film.frame(s.bar) + 1, film.frame(s.bar + s.bars)
        part = d[a:b]
        if len(part) < 3 or s.kind == "black":
            continue
        med = float(np.median(part))
        flags = []
        for i, v in enumerate(part):
            if med > 0.15 and v < med * 0.15:
                flags.append("freeze@%d(%.2f)" % (i + 1, v))
            if v > max(med * 4.0, 2.5):
                flags.append("jump@%d(%.2f)" % (i + 1, v))
        print("%-20s median %.2f worst %.2f %s" % (s.id, med, float(part.max()), " ".join(flags[:10])))
        bad += len(flags)
    print("suspect frames:", bad)


if __name__ == "__main__":
    main()
