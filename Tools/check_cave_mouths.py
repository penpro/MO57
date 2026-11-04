"""Cave mouths in game (BR23-1 / BR24-22, Nov 3): nothing planted hangs in the air round the built entrances' bowls.

    python Tools/check_cave_mouths.py [--seed 5730] [--at -339018 -236264] [--mouths 3] [--shots]

A -game instance (the editor need not be closed for this one, but an open editor starves the vegetation: close it for a fair count) makes a
new world, hovers over --at (flying: no fall), finds the cave mouths there (MO.WorldGen.FindCaveEntrance), and at each one asks
MO.PCG.OpeningAudit: per mesh within 2.5 hollow radii, how many stand over carved ground and how many float (a floor-facing one's base > 30 cm
over the traced ground; a lump on a wall or ceiling has the cave's air under it and is not measured). The cave dressing (the meshes DT_CaveScatter names) must not float: before the fix 42-52 per bowl did (FindCaveSurfaces took a
hollow's cap sphere in the open air over its rim for a floor), after it 0-7 (a trace from above misreads dressing inside the passage).
FAIL over --limit per bowl. --shots: two looks at each mouth into Saved/CaveMouthShots (by eye: Wes).
Default place: seed 5730 near the Nov 1 cave report, three built entrances 400-570 m from there.
"""
import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_buildings as cb  # noqa: E402  (its q / look / shot helpers)
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

FLY = r"""
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING)
pawn.set_actor_location(unreal.Vector(%f, %f, %f), False, True)
out('TP ok')
"""
ROW = re.compile(r"\[MOQUERY\] OPENING (\S+)\s+n=\s*(\d+) \| ([\d.]+)-([\d.]+) \| carve\s+(\d+) \| radial\s+(\d+) \| walls\s+(\d+) \| floating\s+(\d+) \((\d+)\) \| worst (-?\d+)")


def cave_dressing():
    """The mesh names DT_CaveScatter plants (read from the asset's soft paths: no editor needed)."""
    path = os.path.join(ue.ROOT, "Plugins", "MOFramework", "Content", "Data", "DT_CaveScatter.uasset")
    with open(path, "rb") as f:
        data = f.read()
    return {m.decode().rsplit("/", 1)[-1] for m in re.findall(rb"/Game/[A-Za-z0-9_/]+?/(S_[A-Za-z0-9_]+|SM_[A-Za-z0-9_]+)(?=[^A-Za-z0-9_])", data)} | \
        {m.decode() for m in re.findall(rb"/(S_[A-Za-z0-9_]+|SM_[A-Za-z0-9_]+)(?![A-Za-z0-9_/])", data)}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--seed", default="5730")
    ap.add_argument("--at", nargs=2, type=float, default=(-339018.0, -236264.0), metavar=("X", "Y"))
    ap.add_argument("--mouths", type=int, default=3)
    ap.add_argument("--limit", type=int, default=12, help="most floating dressing allowed per bowl")
    ap.add_argument("--shots", action="store_true")
    a = ap.parse_args()
    dressing = cave_dressing()
    print("cave dressing meshes (DT_CaveScatter): %s" % ", ".join(sorted(dressing)), flush=True)
    cb.NAME = "cavemouths"
    cb.SHOTS = os.path.join(ue.ROOT, "Saved", "CaveMouthShots")
    fails = []
    ui.start(cb.NAME, ue.UPROJECT, ue.EDITOR_EXE, creator=True, res=(1600, 900), nosteam=True)
    try:
        ui.wait_ready(cb.NAME, 300)
        ui.call(cb.NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        off = ui.log_size(cb.NAME)
        ui.console(cb.NAME, "MO.NewGame " + a.seed)
        ui.wait_log(cb.NAME, r"Character creator opened", since=off, timeout=420)
        time.sleep(5)
        ui.console(cb.NAME, "MO.Test.ClickWidget BeginButton")
        time.sleep(40)
        cb.q(FLY % (a.at[0], a.at[1], 20000.0), "TP")
        time.sleep(5)
        mark = ui.log_size(cb.NAME)
        ui.console(cb.NAME, "MO.WorldGen.FindCaveEntrance 150000")
        ui.wait_log(cb.NAME, r"FindCaveEntrance radius=", since=mark, timeout=120)
        time.sleep(3)
        ents = re.findall(r"Entrance #\d+ dist=([\d.]+) x=(-?[\d.]+) y=(-?[\d.]+) z=(-?[\d.]+)", ui.read_log_from(cb.NAME, mark))
        if not ents:
            print("FAIL no cave mouth within 1.5 km of --at", flush=True)
            return 1
        for k, (dist, x, y, z) in enumerate(ents[:a.mouths]):
            x, y, z = float(x), float(y), float(z)
            cb.q(FLY % (x + 1500.0, y, z + 400.0), "TP")
            time.sleep(35)   # the cells round it generate
            m0 = ui.log_size(cb.NAME)
            ui.console(cb.NAME, "MO.PCG.OpeningAudit 6000")
            ui.wait_log(cb.NAME, r"OPENING DONE", since=m0, timeout=120)
            rows = [r.groups() for r in ROW.finditer(ui.read_log_from(cb.NAME, m0))]
            floating = sum(int(r[7]) for r in rows if r[0] in dressing)
            n = sum(int(r[1]) for r in rows if r[0] in dressing)
            worst = max([int(r[9]) for r in rows if r[0] in dressing] or [0])
            walls = sum(int(r[6]) for r in rows if r[0] in dressing)
            ok = floating <= a.limit
            label = "mouth %d (%.0f, %.0f, %.0f), %.0f m from --at: %d of %d cave-dressing instances float (worst %d cm; %d on walls / ceilings not measured), limit %d" % (
                k, x, y, z, float(dist) / 100.0, floating, n, worst, walls, a.limit)
            print("%s %s" % ("PASS" if ok else "FAIL", label), flush=True)
            for r in rows:
                if r[0] in dressing or int(r[7]) > 0:
                    print("     %-36s n=%4s radii %s-%s carve %s walls %s floating %s worst %s" % (r[0], r[1], r[2], r[3], r[4], r[6], r[7], r[9]), flush=True)
            if not ok:
                fails.append(label)
            if a.shots:
                cb.SHOOT = True
                for j, (dx, dy, dz) in enumerate(((2200.0, 1200.0, 900.0), (-1800.0, -1600.0, 700.0))):
                    print("     shot:", cb.look((x + dx, y + dy, z + dz), (x, y, z - 200.0), "mouth%d_%s_%d" % (k, a.seed, j), fov=70), flush=True)
    finally:
        ui.stop(cb.NAME)
    print("CAVE MOUTHS %s (%d fail)" % ("PASS" if not fails else "FAIL", len(fails)), flush=True)
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
