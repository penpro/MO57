"""Generator v13's floating islands, ravines and lobed lakes in game (fork F62; Wes, Nov 3: "floating islands and better caves with chasms more like minecraft").

    python Tools/check_sky_features.py [--seed 777] [--max 2] [--label on] [--spots spots.json] [--at X Y] [--bare] [--no-islands] [--no-ravines] [--no-lobes] [--nobench] [--holes] [--island-lod N] [--only islands|ravines|lakes]

A -game instance (close the editor first: an open editor starves the vegetation and spoils the benches) makes a new world, then:
- finds the floating islands, ravines and upland lakes near the start (MO.WorldGen.FindIsland / FindRavine / FindLake) -- or, with --spots, visits the spots an
  earlier run saved (the control: the same places with a feature off, --bare for bare island tops);
- at each island: looks at it near, mid and far (the views scale with the island), a frame bench at the mid view, then stands on its top: two looks and MO.WorldGen.IslandPlants
  (what grew there by tag and mesh, how it meets the rock, rocks hanging out of it), and the first look again later (a big island's top takes a minute to build after a teleport);
- at each ravine: looks down into it, along it from its lip and from its floor, a bench from above;
- at each lake: from high over it and from its shore;
- saves the spots, the shots and every [MOPERF] and ISLANDPLANTS line to Saved/SkyFeatures/<label>/. A bench taken while another UnrealEditor process runs is marked SHARED GPU: discard.

Prints "SKY FEATURES DONE". Whether it LOOKS right goes on Wes's by-eye list.
"""
import argparse
import json
import math
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402
import check_buildings as cb  # noqa: E402  (q / look / shot)

FLY = r"""
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING)
pawn.set_actor_location(unreal.Vector(%f, %f, %f), False, True)
# The camera (MO.Cam.LookAt) sits where the pawn is: hide the pawn and what it holds, or its hand fills the shot.
pawn.set_actor_hidden_in_game(True)
for a in pawn.get_attached_actors():
    a.set_actor_hidden_in_game(True)
out('TP ok')
"""

# A ravine's floor: its path meanders off the cell centre FindRavine gives (by up to 15 % of its length), so trace straight down every 2.5 m
# across it (perpendicular to its direction, +-SPAN) and keep the deepest hit -- the slot.
FLOOR = r"""
x0, y0, top, bottom, px, py, span = %f, %f, %f, %f, %f, %f, %f
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
best = None
n = int(span / 250.0)
for i in range(-n, n + 1):
    x, y = x0 + px * i * 250.0, y0 + py * i * 250.0
    r = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, top), unreal.Vector(x, y, bottom), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn], unreal.DrawDebugTrace.NONE, True)
    if r:
        z = r.to_tuple()[4].z
        if best is None or z < best[2]:
            best = (x, y, z)
out('FLOOR %%s' %% ('%%.0f %%.0f %%.0f' %% best if best else 'none'))
"""


NOBENCH = False


def ue_processes():
    """How many UnrealEditor.exe run (ours is one): a second one shares the GPU and spoils a frame bench (round 2's control, Nov 3)."""
    import subprocess
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe", "/FO", "CSV", "/NH"], capture_output=True, text=True).stdout
    return sum(1 for l in out.splitlines() if l.startswith('"UnrealEditor.exe"'))


def bench(label, seconds=10):
    if NOBENCH:
        return ""
    n = ue_processes()
    off = ui.log_size(cb.NAME)
    ui.console(cb.NAME, "MO.Perf.Bench %d %s" % (seconds, label))
    m = ui.wait_log(cb.NAME, r"\[MOPERF\] " + re.escape(label) + r"[^\r\n]*", since=off, timeout=seconds + 30)
    line = m.group(0) if hasattr(m, "group") else (m or "")
    n = max(n, ue_processes())
    if n > 1:
        line += " | SHARED GPU: %d UnrealEditor processes -- discard" % n
    print("   " + line, flush=True)
    return line


def stop_at(cam, target, label, settle=30.0, fov=70):
    """Fly the pawn to the camera (the voxel world builds round it), wait, then look and shoot."""
    cb.q(FLY % cam, "TP")
    time.sleep(settle)
    path = cb.look(cam, target, label, fov=fov)
    print("   shot %s: %s" % (label, path), flush=True)
    return path


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--seed", default="777")
    ap.add_argument("--spots", help="visit these spots (a spots.json an earlier run wrote) instead of finding them")
    ap.add_argument("--label", default="on")
    ap.add_argument("--max", type=int, default=2, help="islands, ravines and lakes to visit, each")
    ap.add_argument("--at", nargs=2, type=float, help="hover here (world X Y) before finding them: a hillier start for the ravines")
    ap.add_argument("--nobench", action="store_true", help="no frame benches (another UE_5.8 process shares the GPU)")
    ap.add_argument("--bare", action="store_true", help="the control: nothing planted on the tops (mo.PCG.IslandPlants 0)")
    ap.add_argument("--no-islands", action="store_true", help="the control: mo.WorldGen.IslandChance 0")
    ap.add_argument("--no-ravines", action="store_true", help="the control: mo.WorldGen.RavineChance 0")
    ap.add_argument("--no-lobes", action="store_true", help="the control: mo.WorldGen.LakeLobes 1")
    ap.add_argument("--only", choices=["islands", "ravines", "lakes"], help="visit only these")
    ap.add_argument("--holes", action="store_true", help="islands: only the far views (1.2, 2.5, 4, 8, 13.5 km, the land under the island in frame) for the far-chunk holes test; --island-lod 0 is its control")
    ap.add_argument("--island-lod", type=int, default=-1, help="mo.WorldGen.IslandStampMaxLOD: the highest LOD the stamps draw at (a far-chunk holes test)")
    a = ap.parse_args()
    out_dir = os.path.join(ue.ROOT, "Saved", "SkyFeatures", a.label)
    os.makedirs(out_dir, exist_ok=True)
    cb.NAME = "sky"
    cb.SHOTS = out_dir
    cb.SHOOT = True
    global NOBENCH
    NOBENCH = a.nobench
    perf = []
    ui.start(cb.NAME, ue.UPROJECT, ue.EDITOR_EXE, creator=True, res=(1600, 900), nosteam=True)
    try:
        ui.wait_ready(cb.NAME, 300)
        # The controls (MOWorldGenSettings::BuildParams reads them at the next world start; -1 = the settings' own).
        if a.no_islands:
            ui.console(cb.NAME, "mo.WorldGen.IslandChance 0")
        if a.no_ravines:
            ui.console(cb.NAME, "mo.WorldGen.RavineChance 0")
        if a.no_lobes:
            ui.console(cb.NAME, "mo.WorldGen.LakeLobes 1")
        if a.bare:
            ui.console(cb.NAME, "mo.PCG.IslandPlants 0")
        if a.island_lod >= 0:
            ui.console(cb.NAME, "mo.WorldGen.IslandStampMaxLOD %d" % a.island_lod)
        off = ui.log_size(cb.NAME)
        t0 = time.time()
        ui.console(cb.NAME, "MO.NewGame " + a.seed)
        opened = ui.wait_log(cb.NAME, r"Character creator opened", since=off, timeout=600)
        print("world ready for the creator after %.0f s (%s)" % (time.time() - t0, "creator open" if opened else "NO CREATOR"), flush=True)
        if not opened:
            return
        time.sleep(5)
        ui.console(cb.NAME, "MO.Test.ClickWidget BeginButton")
        time.sleep(40)
        for l in re.findall(r"\[MOFloatingIsland\][^\r\n]*", ui.read_log_from(cb.NAME, off)):
            print("   " + l, flush=True)
        perf.append(bench("start_" + a.label))

        if a.spots:
            spots = json.load(open(a.spots))
        else:
            spots = {"islands": [], "ravines": []}
            if a.at:
                cb.q(FLY % (a.at[0], a.at[1], 20000.0), "TP")
                time.sleep(5)
            m0 = ui.log_size(cb.NAME)
            ui.console(cb.NAME, "MO.WorldGen.FindIsland 1500000")
            ui.console(cb.NAME, "MO.WorldGen.FindRavine 1000000")
            ui.console(cb.NAME, "MO.WorldGen.FindLake 600000")
            time.sleep(8)
            log = ui.read_log_from(cb.NAME, m0)
            spots["lakes"] = []
            for m in re.finditer(r"Lake #\d+ dist=[\d.]+ x=(-?[\d.]+) y=(-?[\d.]+) surfaceZ=(-?[\d.]+) radius=([\d.]+) aspect=([\d.]+) depth=([\d.]+) lobes=(\d+)", log):
                lk = dict(zip(("x", "y", "surface", "radius", "aspect", "depth", "lobes"), map(float, m.groups())))
                lk["radius"] *= max(1.0, lk["aspect"])
                lk["lobes"] = int(lk["lobes"])
                spots["lakes"].append(lk)
            for m in re.finditer(r"Island #\d+ dist=[\d.]+ x=(-?[\d.]+) y=(-?[\d.]+) top=(-?[\d.]+) ground=(-?[\d.]+) radius=([\d.]+) thickness=([\d.]+)", log):
                spots["islands"].append(dict(zip(("x", "y", "top", "ground", "radius", "thickness"), map(float, m.groups()))))
            for m in re.finditer(r"Ravine #\d+ dist=[\d.]+ x=(-?[\d.]+) y=(-?[\d.]+) ground=(-?[\d.]+) length=([\d.]+) halfwidth=([\d.]+) depth=([\d.]+) dir=(-?[\d.]+),(-?[\d.]+)", log):
                spots["ravines"].append(dict(zip(("x", "y", "ground", "length", "halfwidth", "depth", "dx", "dy"), map(float, m.groups()))))
            print("found %d island(s), %d ravine(s), %d lake(s)" % (len(spots["islands"]), len(spots["ravines"]), len(spots["lakes"])), flush=True)
            json.dump(spots, open(os.path.join(out_dir, "spots.json"), "w"), indent=1)

        if a.only:
            spots = {k: (v if k == a.only else []) for k, v in spots.items()}

        for k, isl in enumerate(spots["islands"][:a.max]):
            x, y, top, ground = isl["x"], isl["y"], isl["top"], isl["ground"]
            mid = (top + ground) / 2.0
            print("--- island %d at (%.0f, %.0f): top %.0f over ground %.0f, radius %.0f" % (k, x, y, top, ground, isl["radius"]), flush=True)
            r = isl["radius"]
            if a.holes:
                # A chunk whose bounds meet a volume stamp is meshed by the voxel plugin's volume pass, which holes coarse chunks of steep ground (Docs/Voxel_Plugin_Reference.md section 6).
                for dist in (120000.0, 250000.0, 400000.0, 800000.0, 1350000.0):
                    cam = (x - dist, y - dist * 0.3, max(top, ground + 30000.0) + 20000.0)
                    stop_at(cam, (x, y, ground), "island%d_%.1fkm_%s" % (k, dist / 100000.0, a.label), settle=50.0, fov=30)
                    if dist == 800000.0:
                        perf.append(bench("island%d_8km_%s" % (k, a.label)))
                continue
            # Round 2: islands of 230 m to 2.9 km -- the views scale with the island (and the far one is far: 2 km at the least, 12 km at the most).
            for dist, tag in ((max(15000.0, 1.8 * r), "near"), (max(60000.0, 4.5 * r), "mid"), (min(1200000.0, max(200000.0, 12.0 * r)), "far")):
                cam = (x - dist, y - dist * 0.3, top + 2000.0 + dist * 0.08)
                stop_at(cam, (x, y, mid), "island%d_%s_%s" % (k, tag, a.label), settle=35.0 if tag != "far" else 45.0, fov=60 if tag != "far" else 25)
                if tag == "mid":
                    perf.append(bench("island%d_%s" % (k, a.label)))
            if not a.no_islands:
                # On the top: stand among what grew there, and count it (MO.WorldGen.IslandPlants: per island by tag and mesh; round the pawn how each plant meets the rock).
                # The pawn hovers over the rock's top at the centre (the top rolls: a ray finds it, the deepest hit across 20 m being the rock and not a crown): the voxel
                # collision and the nearest planting tiers build round it.
                cb.q(FLY % (x, y, top + 3000.0), "TP")
                time.sleep(45.0)
                rock = cb.q(FLOOR % (x, y, top + 3000.0, top - 5000.0, 1.0, 0.0, 1000.0), "FLOOR", timeout=120)
                if rock in ("", "none"):
                    print("   top: no rock under the pawn at the centre", flush=True)
                    continue
                rx, ry, rz = map(float, rock.split())
                print("   rock at (%.0f, %.0f, %.0f): %.0f m over the land" % (rx, ry, rz, (rz - ground) / 100.0), flush=True)
                stop_at((rx, ry, rz + 180.0), (rx + 10000.0, ry + 2000.0, rz), "island%d_top_%s" % (k, a.label), settle=30.0, fov=80)
                stop_at((rx, ry, rz + 180.0), (rx - 10000.0, ry - 3000.0, rz), "island%d_top2_%s" % (k, a.label), settle=5.0, fov=80)
                off = ui.log_size(cb.NAME)
                ui.console(cb.NAME, "MO.WorldGen.IslandPlants 2000000")
                ui.wait_log(cb.NAME, r"ISLANDPLANTS done[^\r\n]*", since=off, timeout=120)
                for l in re.findall(r"\[MOQUERY\] ISLANDPLANTS[^\r\n]*", ui.read_log_from(cb.NAME, off)):
                    print("   " + l, flush=True)
                    perf.append(l)
                # The first view again, later: is ground missing from it a chunk still building, or a hole?
                stop_at((rx, ry, rz + 180.0), (rx + 10000.0, ry + 2000.0, rz), "island%d_top3_%s" % (k, a.label), settle=45.0, fov=80)

        for k, rv in enumerate(spots["ravines"][:a.max]):
            x, y, ground, length, dx, dy = rv["x"], rv["y"], rv["ground"], rv["length"], rv["dx"], rv["dy"]
            print("--- ravine %d at (%.0f, %.0f): %.0f m long, %.0f m deep" % (k, x, y, length / 100.0, rv["depth"] / 100.0), flush=True)
            stop_at((x - dy * 2500.0, y + dx * 2500.0, ground + 6000.0), (x, y, ground - rv["depth"] * 0.5), "ravine%d_above_%s" % (k, a.label))
            perf.append(bench("ravine%d_%s" % (k, a.label)))
            # Down into it from its lip, a third of the way along: the ledges and the crags of the far wall.
            lip = (x - dx * length * 0.3 - dy * rv["halfwidth"] * 2.5, y - dy * length * 0.3 + dx * rv["halfwidth"] * 2.5, ground + 1500.0)
            stop_at(lip, (x + dx * length * 0.1, y + dy * length * 0.1, ground - rv["depth"] * 0.6), "ravine%d_lip_%s" % (k, a.label), settle=20.0, fov=75)
            # From its floor, looking along it and up: the walls' height, the ledges and the crags (the depth the shots from above can't show).
            cb.q(FLY % (x, y, ground + 2000.0), "TP")
            time.sleep(8)
            span = 0.15 * length + 3.0 * rv["halfwidth"]
            floor = cb.q(FLOOR % (x, y, ground + 1500.0, ground - rv["depth"] - 3000.0, -dy, dx, span), "FLOOR", timeout=120)
            if floor in ("", "none"):
                print("   floor: no hit across the ravine", flush=True)
                continue
            fx, fy, fz = map(float, floor.split())
            print("   floor at (%.0f, %.0f, %.0f): %.0f m under the lip" % (fx, fy, fz, (ground - fz) / 100.0), flush=True)
            stop_at((fx - dx * 300.0, fy - dy * 300.0, fz + 350.0), (fx + dx * 6000.0, fy + dy * 6000.0, fz + 0.7 * (ground - fz)),
                    "ravine%d_floor_%s" % (k, a.label), settle=20.0, fov=80)

        for k, lk in enumerate(spots.get("lakes", [])[:a.max]):
            x, y, s, r = lk["x"], lk["y"], lk["surface"], lk["radius"]
            print("--- lake %d at (%.0f, %.0f): radius %.0f m, %d lobe(s)" % (k, x, y, r / 100.0, lk["lobes"]), flush=True)
            # From high over its south-west, looking down at the whole of it; then low over the shore.
            stop_at((x - 1.2 * r, y - 0.4 * r, s + 1.6 * r), (x, y, s), "lake%d_above_%s" % (k, a.label), settle=30.0, fov=60)
            stop_at((x - 1.4 * r, y - 0.2 * r, s + 0.25 * r + 800.0), (x, y, s), "lake%d_shore_%s" % (k, a.label), settle=20.0, fov=70)
    finally:
        with open(os.path.join(out_dir, "perf.txt"), "w") as f:
            f.write("\n".join(perf) + "\n")
        ui.stop(cb.NAME)
    print("SKY FEATURES DONE (%s): %s" % (a.label, out_dir), flush=True)


if __name__ == "__main__":
    main()
