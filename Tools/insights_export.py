"""Headless Unreal Insights: export a .utrace's timing events to CSV and summarise them -- no Insights window.

Capture (packaged game, see Tools/pkg_probe.py):
    --extra="-trace=cpu,frame,bookmark -statnamedevents -tracefile=D:/path/x.utrace"
    (add ",voxel" to -trace for the voxel plugin's own scopes; -statnamedevents turns SCOPE_CYCLE_COUNTERs into trace scopes)

Export + summary:
    python Tools/insights_export.py x.utrace out.csv --threads GameThread --timers "*Navigation*,*Recast*"
    python Tools/insights_export.py x.utrace win.csv --threads GameThread --start 21.994 --end 22.012 --tree
    python Tools/insights_export.py x.utrace frames.csv --worst-frames 5 --start 10 --min-ms 1
--summary (default) prints count / total / worst per timer; --tree prints the call tree of the exported window (events >= --min-ms);
--worst-frames N finds the N slowest game-thread frames (after --start) and prints each one's tree (a hitch explainer).

TRAP: the export command has to reach the engine as -ExecOnAnalysisCompleteCmd="..." with the quotes INSIDE the command line.
Bash (and subprocess with an argument list) quote the whole argument instead, and the engine then cuts the command at its first
space -- the export silently never runs. This script passes one raw command-line string.
"""
import argparse
import collections
import csv
import os
import subprocess
import sys
import time

ENGINE = os.environ.get("UE_ENGINE_DIR", r"D:\UnrealEngine\UE_5.8")
INSIGHTS = os.path.join(ENGINE, r"Engine\Binaries\Win64\UnrealInsights.exe")


def export(trace, out, threads, timers, start, end, log):
    args = [f"-columns=TimerName,StartTime,EndTime,Duration,Depth,ThreadName"]
    if threads:
        args.append(f"-threads={threads}")
    if timers:
        args.append(f"-timers={timers}")
    if start is not None:
        args.append(f"-startTime={start}")
    if end is not None:
        args.append(f"-endTime={end}")
    for a in args:
        if " " in a or '"' in a:
            sys.exit(f"insights_export: no spaces or quotes allowed in export arguments ({a})")
    command = f"TimingInsights.ExportTimingEvents {os.path.abspath(out)} " + " ".join(args)
    line = (f'"{INSIGHTS}" -OpenTraceFile="{os.path.abspath(trace)}" -unattended -autoquit -noui -nullrhi '
            f'-ExecOnAnalysisCompleteCmd="{command}" -abslog="{os.path.abspath(log)}"')
    if os.path.exists(out):
        os.remove(out)
    t0 = time.time()
    subprocess.run(line, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if not os.path.exists(out):
        sys.exit(f"insights_export: no CSV produced (log: {log})")
    print(f"[insights_export] {out} in {time.time() - t0:.0f}s")


def load(out):
    return list(csv.DictReader(open(out, encoding="utf-8-sig")))


def summary(rows, top):
    agg = collections.defaultdict(list)
    for r in rows:
        agg[r["TimerName"]].append(float(r["Duration"]) * 1000.0)
    print(f"{len(rows)} events, {len(agg)} timers (by worst event):")
    for name, d in sorted(agg.items(), key=lambda kv: -max(kv[1]))[:top]:
        d.sort(reverse=True)
        print(f"  {name[:70]:70s} n={len(d):6d} total={sum(d):9.1f} ms worst={d[0]:7.2f} ms  next={[round(x, 2) for x in d[1:4]]}")


def tree(rows, min_ms):
    rows = sorted(rows, key=lambda r: (r.get("ThreadName", ""), float(r["StartTime"]), int(r["Depth"])))
    base = min(int(r["Depth"]) for r in rows) if rows else 0
    for r in rows:
        ms = float(r["Duration"]) * 1000.0
        if ms >= min_ms:
            print(f"{'  ' * (int(r['Depth']) - base)}{r['TimerName'][:80]}  {ms:.2f} ms")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("trace")
    ap.add_argument("out", help="CSV to write")
    ap.add_argument("--threads", default="GameThread", help="comma list, wildcards ok ('' = all threads)")
    ap.add_argument("--timers", default="", help="comma list, wildcards ok ('' = all timers)")
    ap.add_argument("--start", type=float, help="seconds into the trace")
    ap.add_argument("--end", type=float)
    ap.add_argument("--tree", action="store_true", help="print the call tree instead of the per-timer summary")
    ap.add_argument("--min-ms", type=float, default=0.1, help="--tree: hide events shorter than this")
    ap.add_argument("--top", type=int, default=30)
    ap.add_argument("--worst-frames", type=int, default=0, help="explain the N slowest game-thread frames (FEngineLoop::Tick)")
    a = ap.parse_args()
    log = os.path.splitext(a.out)[0] + "_insights.log"
    if a.worst_frames > 0:
        export(a.trace, a.out, "GameThread", "FEngineLoop::Tick", a.start, a.end, log)
        frames = sorted(load(a.out), key=lambda r: -float(r["Duration"]))[:a.worst_frames]
        for k, f in enumerate(frames):
            s, e = float(f["StartTime"]), float(f["EndTime"])
            print(f"\n=== frame {k + 1}: {1000 * (e - s):.1f} ms at {s:.3f} s")
            win = os.path.splitext(a.out)[0] + f"_frame{k + 1}.csv"
            export(a.trace, win, "GameThread", "", s - 0.00001, e + 0.00001, log)
            tree(load(win), a.min_ms)
        return
    export(a.trace, a.out, a.threads, a.timers, a.start, a.end, log)
    rows = load(a.out)
    tree(rows, a.min_ms) if a.tree else summary(rows, a.top)


if __name__ == "__main__":
    main()
