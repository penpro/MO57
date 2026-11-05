"""The small plants' wind seen in a real -game world, from the same cameras every time: a clip for a person and numbers for the record.

    python Tools/check_plant_wind.py record <label> [--biomes Meadow,Rainforest,LocustWoodland,DryScrub] [--seconds 10] [--keep] [--reuse]
        seed 777, clear skies, 10:00, no pawn in the way. At each biome's spot (MO.WorldGen.FindBiomeSpot <biome> 40 go, deterministic for a seed)
        a colour clip with the game's own anti-aliasing, then a grey burst with r.AntiAliasingMethod 0 for motion_probe's numbers.
        Writes Saved/PlantWind/<label>_<biome>.mp4 and prints one ACTIVITY line per biome.
    python Tools/check_plant_wind.py bench <label> [--biomes ...] [--seconds 15] [--keep] [--reuse]
        the same cameras, MO.Perf.Bench at each: one BENCH line per biome (frame times, GPU ms). Run on an EMPTY machine (Docs/Performance.md).
    python Tools/check_plant_wind.py join <before> <after> [--biomes ...]
        side by side per biome: Saved/PlantWind/<before>_vs_<after>_<biome>.mp4, and all of them in one <before>_vs_<after>.mp4; one COMPARE line
        per biome (reach, speed, how much of the view moves at a moment, with TSR on) and motion maps Saved/PlantWind/map_<before>_vs_<after>_<biome>.png.

Run with the editor closed (it starves a -game instance's vegetation: TRAPS) and an instance name of its own ("windtest"). Why, and what the
numbers mean: Docs/PCG.md section 9, Tools/motion_probe.py.
"""
import argparse
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "windtest"
BIOMES = "Meadow,Rainforest,LocustWoodland,DryScrub"
BPY = r"C:/Program Files/Blender Foundation/Blender 5.2/5.2/python/bin/python.exe"   # motion_probe needs numpy
PROBE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "motion_probe.py")
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Saved", "PlantWind")
CROP = ("1280", "720", "160", "90")   # off the HUD


def q(code, key, timeout=60):
    _, out = ui.call(NAME, ["py", "-c", code], timeout=timeout)
    lines = [ln.split(key + " ", 1)[1] for ln in out.splitlines() if (key + " ") in ln]
    return lines[-1].strip() if lines else ("ERR " + out[-800:])


WHERE = r'''
import unreal
p = unreal.GameplayStatics.get_player_pawn(world, 0)
p.set_actor_hidden_in_game(True)
o = p.get_actor_location()
out("WHERE %.1f %.1f %.1f" % (o.x, o.y, o.z))
'''


def boot(reuse):
    if not reuse:
        ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, creator=True, res=(1600, 900), nosteam=True)
        if not ui.wait_ready(NAME, 300):
            sys.exit("FAIL: the instance did not boot")
        ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.NewGame 777")
        if not ui.wait_log(NAME, r"Character creator opened", since=off, timeout=420):
            sys.exit("FAIL: no character creator")
        time.sleep(5)
        ui.console(NAME, "MO.Test.ClickWidget BeginButton")
        time.sleep(45)
    ui.console(NAME, "MO.Weather.Director 0")
    ui.console(NAME, "MO.Weather.SetPreset Clear_Skies")
    ui.console(NAME, "MO.Clock.SkipToDay")
    time.sleep(3)
    ui.console(NAME, "MO.Clock.SetTime 10 0")
    time.sleep(3)


def probe(*args):
    r = subprocess.run([BPY, PROBE] + list(args), capture_output=True, text=True)
    return (r.stdout.strip() or r.stderr[-600:]).splitlines()[-1]


def spots(biomes):
    """Each biome's spot, in order (FindBiomeSpot searches from where the pawn stands: the same order gives the same spots), the camera set."""
    for biome in biomes.split(","):
        ui.console(NAME, "MO.Cam.Reset")
        ui.console(NAME, "MO.WorldGen.FindBiomeSpot %s 40 go" % biome)
        time.sleep(50)   # terrain and the ground-cover cells round the new spot
        ui.console(NAME, "MO.Clock.SetTime 10 0")
        w = q(WHERE, "WHERE")
        if w.startswith("ERR"):
            print("FAIL", biome, w, flush=True)
            continue
        x, y, z = [float(v) for v in w.split()]
        # from 1.5 m above the ground, 4 m back, down at the plants 3 m in front of the pawn's spot
        ui.console(NAME, "MO.Cam.LookAt %.0f %.0f %.0f %.0f %.0f %.0f 60" % (x - 400, y, z + 60, x + 100, y, z - 70))
        time.sleep(6)
        yield biome, x, y, z


def bench(args):
    boot(args.reuse)
    for biome, _, _, _ in spots(args.biomes):
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.Perf.Bench %d %s_%s" % (args.seconds, args.label, biome))
        m = ui.wait_log(NAME, r"\[MOPERF\] %s_%s\s+frames=[^\r\n]*" % (args.label, biome), since=off, timeout=args.seconds + 30)
        print("BENCH", m.group(0).split("[MOPERF] ", 1)[1] if m else "no result for " + biome, flush=True)
    ui.console(NAME, "MO.Cam.Reset")
    if not args.keep:
        ui.stop(NAME)


def record(args):
    os.makedirs(OUT, exist_ok=True)
    boot(args.reuse)
    pid = ui.read_pid(NAME)
    for biome, x, y, z in spots(args.biomes):
        clip = os.path.join(OUT, "%s_%s.mp4" % (args.label, biome))
        print(biome, "at %.0f %.0f %.0f:" % (x, y, z), probe("clip", str(pid), str(args.seconds), clip, "--crop", *CROP), flush=True)
        ui.console(NAME, "r.AntiAliasingMethod 0")
        time.sleep(2)
        burst = os.path.join(OUT, "_%s_%s.npz" % (args.label, biome))
        probe("capture", str(pid), str(args.seconds), burst)
        ui.console(NAME, "r.AntiAliasingMethod 4")
        print("ACTIVITY", args.label, biome, probe("activity", burst, "--crop", *CROP), flush=True)
        os.remove(burst)
        time.sleep(2)
    ui.console(NAME, "MO.Cam.Reset")
    if not args.keep:
        ui.stop(NAME)


def join(args):
    parts = []
    for biome in args.biomes.split(","):
        a = os.path.join(OUT, "%s_%s.mp4" % (args.before, biome))
        b = os.path.join(OUT, "%s_%s.mp4" % (args.after, biome))
        o = os.path.join(OUT, "%s_vs_%s_%s.mp4" % (args.before, args.after, biome))
        label = "drawtext=fontfile='C\\:/Windows/Fonts/arial.ttf':text='%s':x=12:y=12:fontsize=24:fontcolor=white:box=1:boxcolor=black@0.5"   # no fontfile: ffmpeg crashes in fontconfig here
        f = "[0:v]scale=800:-2,%s[a];[1:v]scale=800:-2,%s[b];[a][b]hstack=inputs=2" % (label % ("%s %s" % (biome, args.before)), label % args.after)
        r = subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", a, "-i", b, "-filter_complex", f, "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20", "-r", "30",
                            "-t", "10", o], capture_output=True, text=True)
        print("JOIN", o, r.returncode, r.stderr[-300:])
        if r.returncode == 0:
            parts.append(o)
        print("COMPARE", biome, probe("compare", a, b, os.path.join(OUT, "map_%s_vs_%s_%s.png" % (args.before, args.after, biome))), flush=True)
    lst = os.path.join(OUT, "_list.txt")
    with open(lst, "w", encoding="utf-8") as fh:
        fh.write("".join("file '%s'\n" % p.replace("\\", "/") for p in parts))
    o = os.path.join(OUT, "%s_vs_%s.mp4" % (args.before, args.after))
    r = subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", lst, "-c", "copy", o], capture_output=True, text=True)
    os.remove(lst)
    print("ALL", o, r.returncode, r.stderr[-300:])


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    rc = sub.add_parser("record")
    rc.add_argument("label")
    rc.add_argument("--biomes", default=BIOMES)
    rc.add_argument("--seconds", type=float, default=10.0)
    rc.add_argument("--keep", action="store_true")
    rc.add_argument("--reuse", action="store_true")
    bn = sub.add_parser("bench")
    bn.add_argument("label")
    bn.add_argument("--biomes", default=BIOMES)
    bn.add_argument("--seconds", type=int, default=15)
    bn.add_argument("--keep", action="store_true")
    bn.add_argument("--reuse", action="store_true")
    jn = sub.add_parser("join")
    jn.add_argument("before")
    jn.add_argument("after")
    jn.add_argument("--biomes", default=BIOMES)
    a = ap.parse_args()
    {"record": record, "bench": bench, "join": join}[a.cmd](a)


if __name__ == "__main__":
    main()
