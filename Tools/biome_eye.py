"""Eye-level ground shots per biome with HighResShot (no HUD, full resolution):   python Tools/biome_eye.py <outdir> <wait_s> <Biome> [<Biome> ...]     (the `ui` instance must be in a game)

For each biome: MO.WorldGen.FindBiomeSpot <id> 40 [rough] go, wait for the terrain + PCG, then four 1920x1080 shots from 1.7 m above the pawn, yaw 0/90/180/270, aimed 5 m out and a little down at the ground
(that is where texture tiling, ground clutter and material blends show). Writes <outdir>/<Biome>_<n>.png + <Biome>_sheet.png.
"""
import math
import os
import re
import subprocess
import sys
import time

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UE = [sys.executable, os.path.join(ROOT, "Tools", "ue.py")]
LOG = os.path.join(os.environ.get("TEMP", ""), "claude", "inst_ui", "game.log")
ROUGH = {"RockyHighland", "RockyScree"}


def run(cmd):
    subprocess.run(UE + ["inst", "do", "ui", "run", cmd], capture_output=True, text=True)


def log_size():
    with open(LOG, encoding="utf-8", errors="replace") as f:
        return sum(1 for _ in f)


def main():
    outdir, wait_s, biomes = sys.argv[1], float(sys.argv[2]), sys.argv[3:]
    os.makedirs(outdir, exist_ok=True)
    run("r.VolumetricRenderTarget 0")
    for b in biomes:
        mark = log_size()
        run("MO.WorldGen.FindBiomeSpot %s 40 %sgo" % (b, "rough " if b in ROUGH else ""))
        time.sleep(3)
        with open(LOG, encoding="utf-8", errors="replace") as f:
            lines = f.readlines()[mark:]
        m = None
        for ln in lines:
            m = re.search(r"FINDSPOT went to x=(-?\d+) y=(-?\d+) z=(-?\d+)", ln) or m
        if not m:
            print(b, ": no spot")
            continue
        x, y, z = (float(v) for v in m.groups())
        print(b, ": spot", x, y, z, "waiting", wait_s)
        time.sleep(wait_s)
        files = []
        for i, yaw in enumerate((0, 90, 180, 270)):
            ux, uy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
            cam = [x, y, z + 170 + 60, x + ux * 500, y + uy * 500, z + 120, 75]
            name = "%s_%d" % (b, i)
            subprocess.run([sys.executable, os.path.join(ROOT, "Tools", "steam_scout.py"), name, "--cam"] + [str(int(c)) for c in cam] + ["--wait", "2"], capture_output=True, text=True)
            src = os.path.join(ROOT, "Saved", "SteamArt", "plates", name + ".png")
            dst = os.path.join(outdir, name + ".png")
            if os.path.exists(src):
                os.replace(src, dst)
                files.append(dst)
        sheet = Image.new("RGB", (960 * 2, 540 * 2))
        for i, f in enumerate(files):
            sheet.paste(Image.open(f).convert("RGB").resize((960, 540)), (960 * (i % 2), 540 * (i // 2)))
        sheet.save(os.path.join(outdir, "%s_sheet.png" % b))
    run("MO.Cam.Reset")
    print("done ->", outdir)


if __name__ == "__main__":
    main()
