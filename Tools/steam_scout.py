"""Scout / capture helper for the Steam store art plates (runs against the `ui` standalone instance, which must be in a game).

    python Tools/steam_scout.py <name> [--hour H] [--weather Preset] [--cam cx cy cz tx ty tz [fov]] [--size 3840x2160] [--wait S]

Sets the clock / weather / camera (all optional), waits, takes a HighResShot and moves the PNG to Saved/SteamArt/plates/<name>.png.
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UE = [sys.executable, os.path.join(ROOT, "Tools", "ue.py")]
SHOTS = os.path.join(ROOT, "Saved", "Screenshots", "WindowsEditor")
OUT = os.path.join(ROOT, "Saved", "SteamArt", "plates")


def run(cmd):
    subprocess.run(UE + ["inst", "do", "ui", "run", cmd], capture_output=True, text=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("name")
    ap.add_argument("--hour", type=int)
    ap.add_argument("--weather")
    ap.add_argument("--cam", nargs="+")
    ap.add_argument("--size", default="1920x1080")
    ap.add_argument("--wait", type=float, default=4.0)
    a = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    if a.hour is not None:
        run("MO.Clock.SetTime %d" % a.hour)
    if a.weather:
        run("MO.Weather.SetPreset " + a.weather)
    if a.cam:
        run("MO.Cam.LookAt " + " ".join(a.cam))
    time.sleep(a.wait)
    before = set(glob.glob(os.path.join(SHOTS, "HighresScreenshot*.png")))
    run("HighResShot " + a.size)
    for _ in range(60):
        time.sleep(1.0)
        new = set(glob.glob(os.path.join(SHOTS, "HighresScreenshot*.png"))) - before
        if new:
            time.sleep(1.0)
            src = sorted(new)[-1]
            dst = os.path.join(OUT, a.name + ".png")
            shutil.move(src, dst)
            print("wrote", dst)
            return
    print("no screenshot appeared")


if __name__ == "__main__":
    main()
