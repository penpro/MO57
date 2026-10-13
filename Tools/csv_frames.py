"""Summarise a UE CSV profile (csvprofile start / stop): frame-time percentiles and the worst frames with their top game-thread timers.

    python Tools/csv_frames.py <Profile.csv> [--worst 12] [--top 6]

The CSV profiler appends columns as new stats appear, so the header that matches the data is the LAST "EVENTS,..." row (the first one is shorter).
Packaged builds write to Saved/StagedBuilds_DevTest/Windows/MO57/Saved/Profiling/CSV/.
"""
import argparse
import csv
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--worst", type=int, default=12)
    ap.add_argument("--top", type=int, default=6)
    a = ap.parse_args()
    csv.field_size_limit(10 ** 9)
    rows = list(csv.reader(open(a.csv, encoding="utf-8", errors="replace")))
    heads = [i for i, r in enumerate(rows) if r and r[0] == "EVENTS"]
    if not heads:
        sys.exit("no EVENTS header row")
    hdr = rows[heads[-1]]
    ix = {h: i for i, h in enumerate(hdr)}

    def fl(r, h):
        i = ix.get(h)
        if i is None or i >= len(r):
            return 0.0
        try:
            return float(r[i])
        except ValueError:
            return 0.0

    end = heads[-1] if len(heads) > 1 else len(rows)
    data = [r for r in rows[heads[0] + 1:end] if len(r) > 10]
    if not data:
        sys.exit("no frames")
    ft = sorted(fl(r, "FrameTime") for r in data)
    gt = sorted(fl(r, "GameThreadTime") for r in data)
    pct = lambda v, p: v[min(int(len(v) * p), len(v) - 1)]
    print("%d frames | frame p50 %.1f p95 %.1f p99 %.1f max %.1f | game p95 %.1f p99 %.1f max %.1f | >33 ms: %d"
          % (len(data), pct(ft, .5), pct(ft, .95), pct(ft, .99), ft[-1], pct(gt, .95), pct(gt, .99), gt[-1], sum(1 for v in ft if v > 33.3)))
    excl = [h for h in hdr if h.startswith("Exclusive/GameThread/")]
    worst = sorted(range(len(data)), key=lambda i: -fl(data[i], "FrameTime"))[:a.worst]
    for i in sorted(worst):
        r = data[i]
        top = sorted(((fl(r, h), h) for h in excl), reverse=True)[:a.top]
        print("f%-5d ft %5.1f gt %5.1f rt %5.1f :: %s" % (i, fl(r, "FrameTime"), fl(r, "GameThreadTime"), fl(r, "RenderThreadTime"),
                                                     ", ".join("%s=%.1f" % (h.split("/", 2)[2], v) for v, h in top)))


if __name__ == "__main__":
    main()
