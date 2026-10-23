"""A short Unreal Insights capture of one spot in a -game instance, and its top game-thread costs (the Oct 22 2026 performance sweep).

    python Tools/perf_capture.py <outdir> <seed> <x> <y> <z> [--settle 60] [--seconds 30] [--keep]

Starts the `perf` instance (editor closed: TRAPS 5), a new game on <seed>, teleports to (x, y, z), lets the world settle, then traces
cpu + frame + bookmark for <seconds> with Trace.File (the boot is not in the trace), stops, and summarises the game thread with
Tools/insights_export.py. Also prints the [MONet] fps / frame-time lines of the traced window. An editor-binary instance carries editor-only
cost: compare captures with each other (same spot, same settle), not with the packaged numbers in Docs/Performance.md.
"""
import argparse
import os
import re
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UE = [sys.executable, os.path.join(ROOT, "Tools", "ue.py")]
LOG = os.path.join(os.environ.get("TEMP", ""), "claude", "inst_perf", "game.log")


def ue(*args, timeout=300):
    r = subprocess.run(UE + list(args), capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout)
    return (r.stdout or "") + (r.stderr or "")


def run(cmd):
    return ue("inst", "do", "perf", "run", cmd)


def log_size():
    try:
        return os.path.getsize(LOG)
    except OSError:
        return 0


def log_from(off):
    try:
        with open(LOG, "rb") as f:
            f.seek(off)
            return f.read().decode("utf-8", errors="replace")
    except OSError:
        return ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir")
    ap.add_argument("seed", type=int)
    ap.add_argument("x", type=float)
    ap.add_argument("y", type=float)
    ap.add_argument("z", type=float)
    ap.add_argument("--settle", type=float, default=60.0)
    ap.add_argument("--seconds", type=float, default=30.0)
    ap.add_argument("--res", default="1280x720")
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    trace = os.path.abspath(os.path.join(a.outdir, "capture.utrace")).replace("\\", "/")

    ue("inst", "start", "perf", "--res", a.res)
    off = log_size()
    run(f"MO.NewGame {a.seed}")
    deadline = time.time() + 420
    while time.time() < deadline and not re.search(r"\[MOReadiness\] READY after", log_from(off)):
        time.sleep(3)
    run("MO.Clock.SetTime 12")
    run("MO.Weather.SetPreset Clear_Skies")
    run("MO.Player.Teleport %d %d %d" % (a.x, a.y, a.z + 250))
    print(f"settling {a.settle:.0f} s at ({a.x:.0f}, {a.y:.0f})", flush=True)
    time.sleep(a.settle)
    off = log_size()
    run(f"Trace.File {trace} cpu,frame,bookmark")
    time.sleep(a.seconds)
    run("Trace.Stop")
    window = log_from(off)
    time.sleep(3)
    for line in re.findall(r"\[MONet\] role=\S+ [^\r\n]*?fps=[^\r\n]*?frame=[\d./]+ms", window):
        print("  " + line[line.find("fps="):])
    if not a.keep:
        ue("inst", "stop", "perf")
    if not os.path.exists(trace):
        print(f"no trace at {trace}")
        return 1
    csv_path = os.path.join(a.outdir, "gamethread.csv")
    out = subprocess.run([sys.executable, os.path.join(ROOT, "Tools", "insights_export.py"), trace, csv_path, "--threads", "GameThread"],
                         capture_output=True, text=True, encoding="utf-8", errors="replace")
    summary = (out.stdout or "") + (out.stderr or "")
    open(os.path.join(a.outdir, "summary.txt"), "w", encoding="utf-8").write(summary)
    print(summary[-6000:])
    return 0


if __name__ == "__main__":
    sys.exit(main())
