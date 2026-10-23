"""Before / after screenshots for Wes's look checks (Oct 22 2026: "collect a few screen shots and i can review them and give thumbs up/thumbs down").

    python Tools/review_shots.py <spec.json> <outdir> <tag> [--spots spots.json] [--keep]

Runs the `ui` standalone instance (started here unless it is already up; stopped at the end unless --keep). The editor must be closed: an open
editor starves an instance's vegetation (TRAPS 5). For each spot of the spec: a new game on the spec's seed (once), a teleport to the spot, a
settle wait for terrain + PCG, then each view as a HighResShot through MO.Cam.LookAt. Writes <outdir>/<spot>_<view>_<tag>.png (1920x1080), a
1280x720 JPEG beside each for review pages, and <outdir>/spots.json (where each spot resolved to). Pass that file back with --spots on the
"after" run so both runs stand at the same place and look the same way.

Spec:
    {"seed": 3014, "hour": 12, "wait": 70,
     "spots": [{"name": "rainforest", "biome": "Rainforest"},                 # MO.WorldGen.FindBiomeSpot <biome> 40 [rough] go
               {"name": "spire", "at": [850601, -2661500, 22339]},             # a fixed place (z: roughly the ground, the pawn drops)
               ...],
     "views": {"rainforest": [{"name": "eye", "yaw": 0, "pitch": -4, "up": 170},   # camera at the spot + up cm, turned yaw / pitch degrees
                              {"name": "aerial", "yaw": 45, "pitch": -30, "up": 4000, "back": 3000}],
               "spire": [{"name": "wes", "lookat": [cx, cy, cz, tx, ty, tz], "fov": 90, "res": "2858x1027"}]}}   # an exact camera
A spot with no views of its own gets two eye-level views (yaw 0 and 180). A spot may carry its own "hour"; {"here": true} stays where
the new game put the pawn (with {"pawn": true} views: the pawn's own camera). A bug report's LookAt.Reproduce
line is an exact camera: Tools/review_from_reports.py builds the spots and views from a folder of reports.
"""
import argparse
import glob
import json
import math
import os
import re
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UE = [sys.executable, os.path.join(ROOT, "Tools", "ue.py")]
SHOTS = os.path.join(ROOT, "Saved", "Screenshots", "WindowsEditor")
LOG = os.path.join(os.environ.get("TEMP", ""), "claude", "inst_ui", "game.log")
ROUGH = {"RockyHighland", "RockyScree", "AlpineRock"}


def ue(*args, timeout=240):
    r = subprocess.run(UE + list(args), capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout)
    return (r.stdout or "") + (r.stderr or "")


def run(cmd):
    return ue("inst", "do", "ui", "run", cmd)


def log_text(since=0):
    try:
        with open(LOG, "rb") as f:
            f.seek(since)
            return f.read().decode("utf-8", errors="replace")
    except OSError:
        return ""


def log_size():
    try:
        return os.path.getsize(LOG)
    except OSError:
        return 0


def wait_log(pattern, since, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        m = re.search(pattern, log_text(since))
        if m:
            return m
        time.sleep(2)
    return None


def camera(spot, view):
    """MO.Cam.LookAt arguments: from the spot, `back` cm behind along the view's yaw and `up` cm up, aimed along yaw / pitch."""
    yaw, pitch = math.radians(view.get("yaw", 0)), math.radians(view.get("pitch", -4))
    fx, fy = math.cos(yaw), math.sin(yaw)
    back, up = view.get("back", 0), view.get("up", 170)
    cx, cy, cz = spot[0] - fx * back, spot[1] - fy * back, spot[2] + up
    reach = 30000.0
    tx, ty, tz = cx + fx * math.cos(pitch) * reach, cy + fy * math.cos(pitch) * reach, cz + math.sin(pitch) * reach
    return [round(v) for v in (cx, cy, cz, tx, ty, tz)] + [view.get("fov", 70)]


def high_res_shot(dst, res="1920x1080"):
    before = set(glob.glob(os.path.join(SHOTS, "HighresScreenshot*.png")))
    run("HighResShot " + res)
    for _ in range(60):
        time.sleep(1.0)
        new = set(glob.glob(os.path.join(SHOTS, "HighresScreenshot*.png"))) - before
        if new:
            time.sleep(1.0)
            shutil.move(sorted(new)[-1], dst)
            try:
                from PIL import Image
                img = Image.open(dst).convert("RGB")
                img.resize((1280, max(1, round(1280 * img.height / img.width)))).save(dst[:-4] + ".jpg", quality=85)
            except Exception as e:   # PIL is optional: the PNG is the record
                print(f"  (no jpeg: {e})")
            return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("spec")
    ap.add_argument("outdir")
    ap.add_argument("tag")
    ap.add_argument("--spots", help="spots.json of an earlier run: stand at the same places")
    ap.add_argument("--keep", action="store_true", help="leave the instance running")
    a = ap.parse_args()
    spec = json.load(open(a.spec, encoding="utf-8"))
    known = json.load(open(a.spots, encoding="utf-8")) if a.spots else {}
    os.makedirs(a.outdir, exist_ok=True)

    # "window": the instance's window size. HighResShot keeps the WINDOW's aspect (a 2858x1027 shot of a 16:9 window came out pillarboxed),
    # so views copied from a bug report run in a window of the report's aspect (Tools/review_from_reports.py sets it).
    start_args = ["inst", "start", "ui"] + (["--res", spec["window"]] if spec.get("window") else [])
    started = ue(*start_args, timeout=300)   # an instance already up says so and is used as it is
    if "already running" not in started and "READY" not in started.upper():
        print(started[-400:])
    off = log_size()
    run(f"MO.NewGame {spec['seed']}")
    if not wait_log(r"\[MOReadiness\] READY after", off, 420):
        print("no READY after the new game: giving up")
        return 1
    run(f"MO.Clock.SetTime {spec.get('hour', 12)}")
    run("MO.Weather.SetPreset Clear_Skies")

    resolved = dict(known)
    for s in spec["spots"]:
        name = s["name"]
        off = log_size()
        if s.get("here"):
            spot = [0, 0, 0]   # where the new game put the pawn (its coastal spawn, facing the water): views of kind "pawn" only
        elif name in known:
            spot = known[name]
            run("MO.Player.Teleport %d %d %d" % (spot[0], spot[1], spot[2] + 250))
        elif "at" in s:
            spot = s["at"]
            run("MO.Player.Teleport %d %d %d" % (spot[0], spot[1], spot[2] + 250))
        else:
            b = s["biome"]
            run("MO.WorldGen.FindBiomeSpot %s 40 %sgo" % (b, "rough " if b in ROUGH else ""))
            m = wait_log(r"FINDSPOT went to x=(-?\d+) y=(-?\d+) z=(-?\d+)", off, 60)
            if not m:
                print(f"{name}: no spot of {b}")
                continue
            spot = [int(m.group(1)), int(m.group(2)), int(m.group(3)) - 250]
        if not s.get("here"):
            resolved[name] = spot
        if "hour" in s:
            run(f"MO.Clock.SetTime {s['hour']}")
        print(f"{name}: at {spot}; settling {spec.get('wait', 70)} s", flush=True)
        time.sleep(spec.get("wait", 70))
        views = spec.get("views", {}).get(name) or [{"name": "eye0", "yaw": 0}, {"name": "eye180", "yaw": 180}]
        for v in views:
            if v.get("pawn"):
                run("MO.Cam.Reset")   # the pawn's own view
            else:
                cam = list(v["lookat"]) + [v.get("fov", 90)] if "lookat" in v else camera(spot, v)
                run("MO.Cam.LookAt " + " ".join(str(round(x)) for x in cam))
            time.sleep(4)
            dst = os.path.join(a.outdir, f"{name}_{v['name']}_{a.tag}.png")
            print(f"  {os.path.basename(dst)}: {'ok' if high_res_shot(dst, v.get('res', '1920x1080')) else 'NO SHOT'}", flush=True)
        if "hour" in s:
            run(f"MO.Clock.SetTime {spec.get('hour', 12)}")
        run("MO.Cam.Reset")
    json.dump(resolved, open(os.path.join(a.outdir, "spots.json"), "w", encoding="utf-8"), indent=1)
    if not a.keep:
        ue("inst", "stop", "ui")
    return 0


if __name__ == "__main__":
    sys.exit(main())
