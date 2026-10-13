"""Capture the four standard views from the pawn's position and make a 2x2 contact sheet:   python Tools/look_sheet.py <prefix> <px> <py> <pz> [wait_s=80] [height_cm=2500]

Uses steam_scout.py (HighResShot into Saved/SteamArt/plates). Camera: 3 m ahead of the pawn, height cm above it, looking 300 m out and 1500 cm above the pawn, fov 62. Before/after comparisons of
terrain / tree / material work use the same positions, so the shots line up.
"""
import math
import subprocess
import sys
import os

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    prefix, px, py, pz = sys.argv[1], float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4])
    wait = sys.argv[5] if len(sys.argv) > 5 else "80"
    h = float(sys.argv[6]) if len(sys.argv) > 6 else 2500.0
    names = []
    first = True
    for tag, yaw in (("e", 0), ("n", 90), ("w", 180), ("s", 270)):
        ux, uy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        cam = [px + ux * 300, py + uy * 300, pz + h, px + ux * 30000, py + uy * 30000, pz + 1500, 62]
        name = "%s_%s" % (prefix, tag)
        args = [sys.executable, os.path.join(ROOT, "Tools", "steam_scout.py"), name, "--cam"] + [str(int(c)) for c in cam] + ["--wait", wait if first else "3"]
        first = False
        print(subprocess.run(args, capture_output=True, text=True).stdout.strip())
        names.append(name)
    sheet = Image.new("RGB", (960 * 2, 540 * 2))
    for i, n in enumerate(names):
        sheet.paste(Image.open(os.path.join(ROOT, "Saved", "SteamArt", "plates", n + ".png")).convert("RGB").resize((960, 540)), (960 * (i % 2), 540 * (i // 2)))
    out = os.path.join(ROOT, "Saved", "SteamArt", "plates", "sheet_%s.png" % prefix)
    sheet.save(out)
    print("sheet", out)


if __name__ == "__main__":
    main()
