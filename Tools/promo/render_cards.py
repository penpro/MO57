"""MO57 promo: render every text card and lower third in film.py with the kit, and the overlays (flare, embers, dust). Eevee, one job per card: type is quick.

    python Tools/promo/render_cards.py [--only ID,ID] [--scale 1.0] [--stills]     frames in Saved/Promo/kit/cards/<id>/f_####.png
    python Tools/promo/render_cards.py --batch cards.json [--only ...]             write the jobs for `darktower.py batch`, one after another on that GPU; `darktower.py pull` each into the folder above

--stills renders only the middle frame of each (to look at the type before committing to all of it). On this PC by default (--darktower to use that GPU instead; Eevee needs a GPU either way).
A lower third's second line comes from the capture log (Saved/Promo/shots/<id>.json: seed, clock, weather); until a shot has one it reads the planned line.
"""
import argparse
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import film  # noqa: E402

BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
OUT = os.path.join(ROOT, "Saved", "Promo", "kit", "cards")

OVERLAYS = {
    "flare_warm": {"kind": "flare", "frames_total": 16, "colour": "warm", "direction": 1},
    "flare_warm_r": {"kind": "flare", "frames_total": 16, "colour": "warm", "direction": -1},
    "flare_teal": {"kind": "flare", "frames_total": 20, "colour": "teal", "direction": 1},
    "embers": {"kind": "embers", "frames_total": 240, "count": 120},
    "dust_burst": {"kind": "dust", "frames_total": 48, "count": 260},
}

END_CARD = {
    "style": "end", "frames_total": 160, "y": -0.235, "gap": 2.0, "stagger": 16,
    "lines": [
        {"text": "EARLY ACCESS ON STEAM", "size": 0.034, "kind": "label", "color": "parchment", "tracking": 0.2},
        {"text": "SOLO OR CO-OP", "size": 0.026, "kind": "label", "color": "teal", "tracking": 0.2},
        {"text": "Everything after the opening is the game itself, captured from the current development build. Time-lapse where marked.", "size": 0.019, "kind": "label", "color": "dim", "tracking": 0.05},
    ],
}


def seconds_frames(bars):
    return int(round(bars * film.FRAMES_PER_BAR))


def lower_sub(shot_id):
    path = os.path.join(ROOT, "Saved", "Promo", "shots", shot_id + ".json")
    if os.path.isfile(path):
        with open(path, encoding="utf-8") as f:
            return json.load(f).get("lower_sub", "")
    return "seed - clock - weather"


def jobs(scale):
    out = {}
    for c in film.CARDS:
        out[c.id] = ("build_textcard.py", {"style": "slam" if c.style == "slam" else "fade", "text": c.text, "size": c.size, "kind": c.kind, "tracking": c.tracking,
                                           "frames_total": seconds_frames(c.bars), "scale": scale})
    for s in film.SHOTS:
        if s.lower:
            out["L_" + s.id] = ("build_textcard.py", {"style": "lower", "text": s.lower, "sub": lower_sub(s.id), "sub_size": 0.0225, "frames_total": min(seconds_frames(s.bars) - 8, 96), "scale": scale})
        if s.timelapse:
            out["TL_" + s.id] = ("build_textcard.py", {"style": "lower", "text": "Time-lapse", "sub": "", "size": 0.026, "margin_y": 0.06, "frames_total": seconds_frames(s.bars), "scale": scale})
    for name, cfg in OVERLAYS.items():
        out[name] = ("build_overlay.py", dict(cfg, scale=scale))
    out["end_card"] = ("build_textcard.py", dict(END_CARD, scale=scale))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--scale", type=float, default=1.0)
    ap.add_argument("--stills", action="store_true")
    ap.add_argument("--darktower", action="store_true")
    ap.add_argument("--batch")
    ap.add_argument("--no-lowers", action="store_true")
    a = ap.parse_args()
    only = set(a.only.split(",")) if a.only else None
    all_jobs = jobs(a.scale)
    if a.batch:
        rows = [[name, script, dict(cfg, overwrite=True)] for name, (script, cfg) in all_jobs.items() if (not only or name in only) and not (a.no_lowers and name.startswith("L_"))]
        with open(a.batch, "w", encoding="utf-8") as f:
            json.dump(rows, f, indent=1)
        print("BATCH", a.batch, len(rows), "jobs")
        return
    fonts = os.path.join(ROOT, "Saved", "Promo", "fonts")
    env = dict(os.environ, MO57_PROMO_FONTS=fonts)
    for name, (script, cfg) in all_jobs.items():
        if only and name not in only:
            continue
        cfg = dict(cfg)
        if a.stills:
            cfg["still"] = cfg.get("frames_total", 72) // 2
        if a.darktower:
            sys.path.insert(0, HERE)
            import darktower
            darktower.run("card_" + name, script, cfg)
            continue
        cfg["out"] = os.path.join(OUT, name).replace("\\", "/")
        r = subprocess.run([BLENDER, "-b", "--factory-startup", "--python", os.path.join(HERE, "kit", script), "--", json.dumps(cfg)], capture_output=True, text=True, env=env)
        errs = [ln for ln in r.stdout.splitlines() if "Error" in ln or "Traceback" in ln]
        print("CARD", name, "ok" if not errs and r.returncode == 0 else ("FAIL " + " | ".join(errs[-3:])[:300]), flush=True)


if __name__ == "__main__":
    main()
