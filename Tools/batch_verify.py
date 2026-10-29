"""One batch's verification in one command (the loop deep dive, Oct 22 2026): build, then the automation suite and the host + joiner
nettest AT THE SAME TIME, then one summary.

    python Tools/batch_verify.py [--seed 670711536] [--skip-build] [--only case,case] [--pairs 2] [--out DIR]

Run serially they cost ~5 min + ~28 min after a ~1 min build (Oct 22: build 48 s, suite 313 s, nettest ~28 min); together the suite
finishes inside the nettest. --pairs 2 (N-199) runs the nettest as two host + joiner pairs on halves of the cases (~18 min) and the suite
AFTER it, not beside it: four game processes plus the suite's editor slowed a timed test past its limit (CoastalSpawn 2181 ms of 1500)
and two placement-sensitive cases (Oct 29); ~24 min in all. The editor must be closed (the build and the instances need it: CLAUDE.md). Logs: <out>/build.txt, auto.txt,
nettest.txt. Exit code 0 only when the build succeeded, the suite has no failure and every nettest row passed.
"""
import argparse
import os
import re
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UE = [sys.executable, os.path.join(ROOT, "Tools", "ue.py")]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", default="670711536", help="nettest world (a vegetated seed: CONTROL steps need ground items)")
    ap.add_argument("--skip-build", action="store_true")
    ap.add_argument("--only", default="", help="MO_ACTIONS_ONLY: nettest cases to run (default all)")
    ap.add_argument("--pairs", default="1", help="nettest actions --pairs: 2 runs two host + joiner pairs on halves of the cases (N-199)")
    ap.add_argument("--filter", default="MOFramework", help="automation filter")
    ap.add_argument("--out", default=os.path.join(os.environ.get("TEMP", "."), "claude", "batch_verify"))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    t0 = time.time()

    if not a.skip_build:
        with open(os.path.join(a.out, "build.txt"), "w", encoding="utf-8") as f:
            r = subprocess.run(UE + ["build"], cwd=ROOT, stdout=f, stderr=subprocess.STDOUT)
        text = open(os.path.join(a.out, "build.txt"), encoding="utf-8", errors="replace").read()
        ok = r.returncode == 0 and "Result: Succeeded" in text
        print(f"[verify] build: {'OK' if ok else 'FAILED'} ({time.time() - t0:.0f} s)", flush=True)
        if not ok:
            print("\n".join(l for l in text.splitlines() if "error" in l.lower())[-3000:])
            return 1

    env = dict(os.environ)
    if a.only:
        env["MO_ACTIONS_ONLY"] = a.only
    t1 = time.time()
    auto_f = open(os.path.join(a.out, "auto.txt"), "w", encoding="utf-8")
    net_f = open(os.path.join(a.out, "nettest.txt"), "w", encoding="utf-8")
    serial = a.pairs not in ("", "1")   # two pairs: the suite runs after the nettest (load, see the docstring)
    net = subprocess.Popen(UE + ["nettest", "actions", "--seed", a.seed, "--pairs", a.pairs], cwd=ROOT, stdout=net_f, stderr=subprocess.STDOUT, env=env)
    auto = None if serial else subprocess.Popen(UE + ["auto", "--filter", a.filter], cwd=ROOT, stdout=auto_f, stderr=subprocess.STDOUT)
    done = {}
    while len(done) < 2:
        if serial and auto is None and "nettest" in done:
            auto = subprocess.Popen(UE + ["auto", "--filter", a.filter], cwd=ROOT, stdout=auto_f, stderr=subprocess.STDOUT)
        for name, p in (("auto", auto), ("nettest", net)):
            if p is not None and name not in done and p.poll() is not None:
                done[name] = time.time() - t1
                print(f"[verify] {name} finished after {done[name]:.0f} s", flush=True)
        time.sleep(5)
    auto_f.close()
    net_f.close()

    auto_text = open(os.path.join(a.out, "auto.txt"), encoding="utf-8", errors="replace").read()
    m = re.search(r"\[auto\] (\d+) passed, (\d+) failed", auto_text)
    auto_ok = bool(m) and m.group(2) == "0"
    print(f"[verify] automation: {m.group(0) if m else 'no result line'}")
    if not auto_ok:
        print("\n".join(l for l in auto_text.splitlines() if "FAIL" in l)[-3000:])

    net_text = open(os.path.join(a.out, "nettest.txt"), encoding="utf-8", errors="replace").read()
    passes, fails = net_text.count("\n[PASS]"), [l for l in net_text.splitlines() if l.startswith("[FAIL]")]
    print(f"[verify] nettest: {passes} pass, {len(fails)} fail")
    for l in fails:
        print("   " + l[:300])
    slow = re.search(r"\[nettest\] slowest cases: [^\n]*", net_text)
    if slow:
        print("[verify] " + slow.group(0)[10:][:600])
    print(f"[verify] total {time.time() - t0:.0f} s")
    return 0 if auto_ok and not fails and passes else 1


if __name__ == "__main__":
    sys.exit(main())
