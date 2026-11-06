"""The forest floor under Epic's hardware Lumen (N-333, Nov 6). A ray that lands on a plant reads black unless Lumen lights the hit where it lands, so with Ray Lighting Mode on Surface Cache the floor
goes dark with a black ring round every tuft ("shitty black paint poured under each plant").

    python Tools/check_forest_light.py [--seed 777] [--shots DIR]

A -game instance (the editor closed, or it starves the vegetation) hosts a new world, flies to one forest camera at 17:20 and shoots it twice: as configured, and with the cvar on Surface Cache as the
control. PASS: the project's setting is Hit Lighting, and the configured view is at least --gain times the control's mean brightness. Prints the logged frame time of each (one reading, not a bench).
The look is Wes's: this only proves the light arrives.
"""
import argparse
import glob
import os
import re
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_save_roundtrip as base  # noqa: E402
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "forestlight"
CVAR = "r.Lumen.HardwareRayTracing.LightingMode"
# A closed stand on seed 777: the promo's G19 shot, where the rings were first seen.
PLAYER = (431085, 517200, 12600)
CAM_FROM = (430560, 517900, 12500)
CAM_TO = (430420, 518640, 12350)


def frame_ms():
    mark = ui.log_size(NAME)
    time.sleep(14)
    hits = re.findall(r"frame=([\d.]+)/([\d.]+)ms", ui.read_log_from(NAME, mark))
    return sum(float(h[0]) for h in hits) / len(hits) if hits else 0.0


def shoot(out_dir, label, started):
    ui.console(NAME, "shot")
    deadline = time.time() + 20
    newest = None
    while time.time() < deadline:
        fresh = [s for s in glob.glob(os.path.join(ue.ROOT, "Saved", "Screenshots", "**", "*.png"), recursive=True) if os.path.getmtime(s) > started]
        if fresh:
            candidate = max(fresh, key=os.path.getmtime)
            if candidate != getattr(shoot, "last", None):
                newest = candidate
                break
        time.sleep(0.3)
    if not newest:
        return None
    shoot.last = newest
    time.sleep(0.5)
    path = os.path.join(out_dir, label + ".png")
    shutil.copy(newest, path)
    return path


def mean_brightness(path):
    from PIL import Image, ImageStat
    return sum(ImageStat.Stat(Image.open(path).convert("RGB")).mean) / 3.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--gain", type=float, default=1.3)
    ap.add_argument("--shots", default=os.path.join(ue.ROOT, "Saved", "ForestLight"))
    a = ap.parse_args()
    os.makedirs(a.shots, exist_ok=True)
    base.NAME = NAME
    rep = ui.Report()
    started = time.time()
    ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, nosteam=True, res=(1920, 1080))
    try:
        ui.wait_ready(NAME, 300)
        if not base.host_fresh_world(rep, a.seed):
            return 1
        time.sleep(10)
        setting = ui.console(NAME, CVAR)
        m = re.search(re.escape(CVAR) + r' = "(\d+)"\s+LastSetBy: (\w+)', setting)
        rep.step("the project's setting is Hit Lighting", bool(m) and m.group(1) == "1", m.group(0) if m else setting[-160:].strip())
        for line in ("MO.Weather.Director 0", "weather Clear", "MO.Clock.SetTime 17 20", "fly 1", "MO.Player.Teleport %d %d %d" % PLAYER):
            ui.console(NAME, line)
        time.sleep(50)   # the forest grows in round the player
        ui.console(NAME, "MO.Cam.LookAt %d %d %d %d %d %d 45" % (CAM_FROM + CAM_TO))
        time.sleep(8)
        hit_ms = frame_ms()
        hit = shoot(a.shots, "hit_lighting", started)
        ui.console(NAME, CVAR + " 0")
        time.sleep(8)
        cache_ms = frame_ms()
        cache = shoot(a.shots, "surface_cache_control", started)
        ui.console(NAME, CVAR + " 1")
        if not hit or not cache:
            return rep.step("two shots", False, f"{hit} {cache}") or 1
        h, c = mean_brightness(hit), mean_brightness(cache)
        rep.step(f"the floor gets its light: mean brightness at least {a.gain} x the control", h >= c * a.gain, f"{h:.1f} against {c:.1f}")
        print(f"frame time: hit lighting {hit_ms:.1f} ms, surface cache {cache_ms:.1f} ms (one reading, not a bench)", flush=True)
        print(f"shots in {a.shots}", flush=True)
    finally:
        ui.stop(NAME)
    print("FOREST LIGHT %s" % ("PASS" if rep.ok else "FAIL"), flush=True)
    return 0 if rep.ok else 1


if __name__ == "__main__":
    sys.exit(main())
