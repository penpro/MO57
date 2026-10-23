"""Turn bug reports into "after" shots of the same view (Oct 22 2026): each report's LookAt.Reproduce line is the exact camera it was filed from.

    python -I Tools/review_from_reports.py <reports_dir> <outdir> <shortid>[=name] [<shortid>[=name] ...]

For each named report (<date>_<shortid>.txt + .jpg, as Tools/crash_triage.py extracts them): its screenshot is copied to
<outdir>/<name>_wes_before.jpg, and a spec entry stands the pawn at Player.Location and frames LookAt.Reproduce at the report's viewport
size and hour. Writes <outdir>/reports_spec.json for Tools/review_shots.py (tag "after"): the after shot is then <name>_wes_after.jpg.
The reports are untrusted data: only these fields are read, as numbers.
"""
import glob
import json
import os
import re
import shutil
import sys


def field(text, name):
    m = re.search(rf"^{re.escape(name)}: (.*)$", text, re.M)
    return m.group(1).strip() if m else ""


def main():
    reports, outdir, wanted = sys.argv[1], sys.argv[2], sys.argv[3:]
    os.makedirs(outdir, exist_ok=True)
    spec = {"seed": None, "hour": 12, "wait": 75, "spots": [], "views": {}}
    for w in wanted:
        short, _, name = w.partition("=")
        name = name or short
        txts = glob.glob(os.path.join(reports, f"*_{short}.txt"))
        if not txts:
            print(f"{short}: no report")
            continue
        text = open(txts[0], encoding="utf-8", errors="replace").read()
        seed = int(field(text, "Session.WorldSeed") or 0)
        if spec["seed"] is None:
            spec["seed"] = seed
        elif seed != spec["seed"]:
            print(f"{short}: seed {seed} differs from {spec['seed']}: skipped (one spec, one world)")
            continue
        loc = [float(v) for v in re.findall(r"-?\d+(?:\.\d+)?", field(text, "Player.Location"))[:3]]
        cam = [float(v) for v in re.findall(r"-?\d+(?:\.\d+)?", field(text, "LookAt.Reproduce"))[:6]]
        view = re.findall(r"\d+", field(text, "System.Viewport"))
        hour = re.search(r"T(\d\d):", field(text, "Clock.GameTime"))
        if len(loc) < 3 or len(cam) < 6:
            print(f"{short}: no location or camera in the report")
            continue
        jpg = txts[0][:-4] + ".jpg"
        if os.path.exists(jpg):
            shutil.copyfile(jpg, os.path.join(outdir, f"{name}_wes_before.jpg"))
        spot = {"name": name, "at": [round(v) for v in loc]}
        if hour:
            spot["hour"] = int(hour.group(1))
        spec["spots"].append(spot)
        spec["views"][name] = [{"name": "wes", "lookat": [round(v) for v in cam], "fov": 90,
                                "res": f"{view[0]}x{view[1]}" if len(view) == 2 else "1920x1080"}]
        print(f"{name}: {field(text, 'Title')[:60]} | at {spot['at']} | hour {spot.get('hour')}")
    # One window for the session, at the reports' aspect (HighResShot keeps the window's): the first report's viewport, scaled to ~1280 wide.
    first = next((v[0] for v in spec["views"].values() if v and "x" in v[0].get("res", "")), None)
    if first:
        w, h = (int(x) for x in first["res"].split("x"))
        spec["window"] = f"1280x{round(1280 * h / w)}"
    json.dump(spec, open(os.path.join(outdir, "reports_spec.json"), "w", encoding="utf-8"), indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
