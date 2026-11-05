"""Wait until a DARKTOWER kit job has written at least N frames, then scp those frames here to look at them.

    python Tools/promo/darktower_wait_frames.py JOB N [TIMEOUT_S]      Saved/Promo/kit/JOB/... ; prints the seconds per frame

For one job you just started and want to look at before committing hours to it. It does not loop forever: it gives up at the timeout.
"""
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))


def count(job):
    r = subprocess.run(["ssh", "darktower", r"(Get-ChildItem 'D:\MO57Promo\out\%s' -Filter *.png -ErrorAction SilentlyContinue | Measure-Object).Count" % job], capture_output=True, text=True)
    try:
        return int(r.stdout.strip())
    except ValueError:
        return 0


def main():
    job, n = sys.argv[1], int(sys.argv[2])
    timeout = float(sys.argv[3]) if len(sys.argv) > 3 else 1500.0
    t0 = time.time()
    first_at, first_n = None, 0
    while time.time() - t0 < timeout:
        c = count(job)
        if c and first_at is None:
            first_at, first_n = time.time(), c
        if c >= n:
            break
        time.sleep(20)
    c = count(job)
    rate = (time.time() - first_at) / max(c - first_n, 1) if first_at and c > first_n else 0
    print("JOB", job, "frames", c, "s/frame ~%.0f" % rate, flush=True)
    dest = os.path.join(ROOT, "Saved", "Promo", "kit", job + "_peek")
    os.makedirs(dest, exist_ok=True)
    subprocess.run(["scp", "-q", "darktower:D:/MO57Promo/out/%s/f_00*.png" % job, dest + "/"])
    print("PEEK", dest, sorted(os.listdir(dest))[:6])


if __name__ == "__main__":
    main()
