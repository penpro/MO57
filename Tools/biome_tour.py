"""Biome review tour on the standalone instance (full resolution, not throttled like PIE).

    python Tools/biome_tour.py <outdir> <wait_s> <Biome> [<Biome> ...]      (the instance must be in a game: `inst do ui run "MO.NewGame 7"`)

For each biome: MO.WorldGen.FindBiome <id> go, wait for the terrain + PCG, three eye-height views (yaw 0 / 120 / 240) plus one high oblique, and MO.PCG.Census (with the near / mid / far
distance bands). Output: <outdir>/<Biome>_<n>.png and <outdir>/census.txt.
"""
import math
import os
import re
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UE = [sys.executable, os.path.join(ROOT, "Tools", "ue.py")]
ROUGH = {"RockyHighland", "RockyScree"}
DEEP = {"ElectricForest", "Rainforest", "TemperateForest", "LocustWoodland"}   # forests: stand well inside the biome, not on its nearest edge cell
LOG = os.path.join(os.environ.get("TEMP", ""), "claude", "inst_ui", "game.log")


def run(cmd):
    subprocess.run(UE + ["inst", "do", "ui", "run", cmd], capture_output=True, text=True)


def log_tail(pattern, since):
    with open(LOG, encoding="utf-8", errors="replace") as f:
        lines = f.readlines()[since:]
    return [l for l in lines if re.search(pattern, l)]


def nlines():
    with open(LOG, encoding="utf-8", errors="replace") as f:
        return sum(1 for _ in f)


def shoot(out):
    subprocess.run([sys.executable, os.path.join(ROOT, "Tools", "ui_shoot.py"), os.path.dirname(out), "tmp", "hud"], capture_output=True, text=True)
    src = os.path.join(os.path.dirname(out), "tmp_hud.png")
    if os.path.exists(src):
        os.replace(src, out)


def main():
    outdir, wait_s, biomes = sys.argv[1], float(sys.argv[2]), sys.argv[3:]
    os.makedirs(outdir, exist_ok=True)
    for b in biomes:
        mark = nlines()
        if b in ROUGH or b in DEEP:   # steep biomes: "deep inside" spots on rough ground (the nearest-cell finder lands on the biome's gentle edge)
            run("MO.WorldGen.FindBiomeSpot %s 40 %sgo" % (b, "rough " if b in ROUGH else ""))
            time.sleep(3)
            found = log_tail(r"went to.*x=(-?\d+) y=(-?\d+) z=(-?\d+)|FINDSPOT   #0 x=(-?\d+) y=(-?\d+) z=(-?\d+)", mark)
        else:
            run("MO.WorldGen.FindBiome %s go" % b)
            time.sleep(3)
            found = log_tail(r"FindBiome %s: nearest x=(-?\d+) y=(-?\d+) z=(-?\d+)" % b, mark)
        if not found:
            print("%s: no spot found" % b)
            continue
        m = re.search(r"x=(-?\d+) y=(-?\d+) z=(-?\d+)", found[0] if (b in ROUGH or b in DEEP) else found[-1])
        x, y, z = (float(m.group(i)) for i in (1, 2, 3))
        print("%s: spot (%.0f, %.0f, %.0f), waiting %.0fs" % (b, x, y, z, wait_s))
        time.sleep(wait_s)
        for i, yaw in enumerate((0, 120, 240)):
            cx, cy, cz = x, y, z + 170.0
            tx, ty, tz = cx + 10000 * math.cos(math.radians(yaw)), cy + 10000 * math.sin(math.radians(yaw)), cz - 250.0
            run("MO.Cam.LookAt %.0f %.0f %.0f %.0f %.0f %.0f 80" % (cx, cy, cz, tx, ty, tz))
            time.sleep(2.0)
            shoot(os.path.join(outdir, "%s_%d.png" % (b, i)))
        run("MO.Cam.LookAt %.0f %.0f %.0f %.0f %.0f %.0f 70" % (x - 3000, y - 3000, z + 2200, x + 4000, y + 4000, z - 400))
        time.sleep(2.0)
        shoot(os.path.join(outdir, "%s_3.png" % b))
        run("MO.Cam.Reset")
        mark = nlines()
        run("MO.PCG.Census 40000")
        time.sleep(4)
        with open(os.path.join(outdir, "census.txt"), "a", encoding="utf-8") as f:
            f.write("=== %s (%.0f, %.0f, %.0f)\n" % (b, x, y, z))
            for l in log_tail(r"CENSUS", mark)[:30]:
                f.write(l.split("MOQUERY] ")[-1])
    print("TOUR DONE ->", outdir)


if __name__ == "__main__":
    main()
