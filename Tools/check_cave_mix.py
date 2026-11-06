"""The built cave entrances' mix in game (BR23-18, generator v15; Wes, Oct 23: "~2/4 small overhangs (room for a fire, partial shelter), 1/4 deeper branching caves, 1/4 shallow caves as now").

    python Tools/check_cave_mix.py [--seed 777] [--shots DIR] [--radius 1500000]

A -game instance (the editor closed) makes a new world and lists the entrances round the start (MO.WorldGen.Entrances), then walks with real collision:
- the nearest deep cave first (placing the campfire below leaves the pawn at its building): stands in its hollow, walks uphill down the passage, stays down there;
- the nearest overhang: stands on the hollow's floor, walks uphill into the shelter, is stopped by the back wall and not by a lip on the way, reads the shelter rays there (overhead cover, against the open
  ground 30 m beside the bowl) and places a campfire on the shelter's floor (ServerPlaceBuilding, the real rules);
Prints the shots' folder; how they look is Wes's. PASS means a person can use them, not that they are pretty.
"""
import argparse
import math
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_forest_light as cfl  # noqa: E402
import check_save_roundtrip as base  # noqa: E402
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "cavemix"


def pawn_at():
    m = re.search(r"AT (-?[\d.]+) (-?[\d.]+) (-?[\d.]+)", ui._probe(NAME, "p = unreal.GameplayStatics.get_player_pawn(world, 0).get_actor_location(); out('AT %.0f %.0f %.0f' % (p.x, p.y, p.z))", timeout=30))
    return tuple(float(m.group(k)) for k in (1, 2, 3)) if m else None


def face(ux, uy):
    yaw = math.degrees(math.atan2(uy, ux))
    ui._probe(NAME, f"pc = unreal.GameplayStatics.get_player_controller(world, 0); pc.set_control_rotation(unreal.Rotator(pitch=0.0, yaw={yaw:.2f}, roll=0.0)); out('FACED')", timeout=30)


def stand(x, y, z, settle=30.0):
    """Teleport to (x, y, z), let the ground build under a hovering pawn, then stop flying and fall onto it."""
    ui.console(NAME, "fly 1")
    ui.console(NAME, f"MO.Player.Teleport {x:.0f} {y:.0f} {z:.0f}")
    time.sleep(settle)
    ui.console(NAME, f"MO.Player.Teleport {x:.0f} {y:.0f} {z:.0f}")
    ui.console(NAME, "fly 0")
    time.sleep(3.0)
    return pawn_at()


def shelter_rays():
    mark = ui.log_size(NAME)
    ui.console(NAME, "MO.Test.ShelterHere")
    m = ui.wait_log(NAME, r"\[MOTEST\] ShelterHere cover=([\d.]+) wind=([\d.]+) walls=(\d+)/(\d+) enclosure=([\d.]+) indoors=(\d)", since=mark, timeout=15)
    return (float(m.group(1)), int(m.group(3)), float(m.group(5)), int(m.group(6))) if m else None


def buildables():
    m = re.search(r"BUILDABLES=(\d+)", ui._probe(NAME, "out('BUILDABLES=%d' % len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor)))", timeout=30))
    return int(m.group(1)) if m else -1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--radius", type=int, default=1500000)
    ap.add_argument("--shots", default=os.path.join(ue.ROOT, "Saved", "CaveMix"))
    a = ap.parse_args()
    os.makedirs(a.shots, exist_ok=True)
    base.NAME = NAME
    cfl.NAME = NAME
    rep = ui.Report()
    started = time.time()
    ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, nosteam=True, res=(1920, 1080))
    try:
        ui.wait_ready(NAME, 300)
        if not base.host_fresh_world(rep, a.seed):
            return 1
        time.sleep(6)
        for line in ("MO.Weather.Director 0", "weather Clear", "MO.Clock.SetTime 12 0"):
            ui.console(NAME, line)
        start = pawn_at()
        mark = ui.log_size(NAME)
        ui.console(NAME, f"MO.WorldGen.Entrances {a.radius}")
        done = ui.wait_log(NAME, r"WORLDGEN Entrances radius=\S+ overhangs=(\d+) shallow=(\d+) deep=(\d+)", since=mark, timeout=30)
        rows = re.findall(r"WORLDGEN Entrance kind=(\w+) x=(-?\d+) y=(-?\d+) ground=(-?\d+) floor=(-?\d+) uphill=(-?[\d.]+),(-?[\d.]+) shelter=(-?\d+),(-?\d+) passages=(\d+) rooms=(\d+)", ui.read_log_from(NAME, mark))
        counts = tuple(int(done.group(k)) for k in (1, 2, 3)) if done else (0, 0, 0)
        print(f"entrances within {a.radius / 100000.0:.0f} km of the start: {counts[0]} overhangs, {counts[1]} shallow, {counts[2]} deep", flush=True)
        rep.step("the mix is in this world: overhangs and deep caves both", counts[0] > 0 and counts[2] > 0, str(counts))

        def nearest(kind):
            found = [r for r in rows if r[0] == kind]
            return min(found, key=lambda r: (float(r[1]) - start[0]) ** 2 + (float(r[2]) - start[1]) ** 2) if found else None

        d = nearest("Deep")
        if d:
            x, y, ground, floor = float(d[1]), float(d[2]), float(d[3]), float(d[4])
            print(f"deep cave at ({x:.0f}, {y:.0f}): floor {floor:.0f}, ground {ground:.0f}, {d[9]} passages, {d[10]} rooms", flush=True)
            ux, uy = float(d[5]), float(d[6])
            rep.step("the deep cave: it branches", int(d[9]) >= 1 and int(d[10]) >= 1, f"{d[9]} passages, {d[10]} rooms")
            at = stand(x, y, floor + 200.0, settle=40.0)
            rep.step("the deep cave: the pawn stands on the hollow's floor", bool(at) and abs(at[2] - (floor + 90.0)) < 150.0, f"pawn z {at[2] if at else 'none'}, floor {floor:.0f}")
            face(ux, uy)
            mark = ui.log_size(NAME)
            ui.console(NAME, "MO.Test.MoveFor 0 1 11.0")
            lowest, gone = None, -1.0
            for _ in range(12):
                time.sleep(2.0)
                walked = pawn_at()
                if walked:
                    gone = max(gone, (walked[0] - x) * ux + (walked[1] - y) * uy)
                    lowest = walked[2] if lowest is None else min(lowest, walked[2])
            rep.step("the deep cave: walking uphill from the hollow goes down the passage, under the hollow's floor", gone > 800.0 and lowest is not None and lowest < floor - 200.0, f"{gone:.0f} cm along, lowest z {lowest}, floor {floor:.0f}")
            rep.step("the deep cave: ... and stays down there, standing at the far end for 13 s", bool(walked) and walked[2] < floor - 200.0, f"z {walked[2] if walked else 'none'}, floor {floor:.0f}")
            stuck = re.findall(r"\[MOCharacter\][^\r\n]*is stuck inside[^\r\n]*", ui.read_log_from(NAME, mark))
            rep.step("the deep cave: the stuck-in-the-ground rescue never fired on the way", not stuck, stuck[0][-200:] if stuck else "none")
            ui.console(NAME, "MO.Cam.LookAt %.0f %.0f %.0f %.0f %.0f %.0f 70" % (x - ux * 900.0, y - uy * 900.0, floor + 500.0, x + ux * 800.0, y + uy * 800.0, floor))
            time.sleep(6)
            cfl.shoot(a.shots, "deep_from_the_bowl", started)
        o = nearest("Overhang")
        if o:
            x, y, ground, floor = float(o[1]), float(o[2]), float(o[3]), float(o[4])
            ux, uy = float(o[5]), float(o[6])
            sx, sy = float(o[7]), float(o[8])
            print(f"overhang at ({x:.0f}, {y:.0f}): floor {floor:.0f}, ground {ground:.0f}, shelter's middle ({sx:.0f}, {sy:.0f})", flush=True)
            at = stand(x, y, floor + 200.0, settle=40.0)
            rep.step("the overhang: the pawn stands on the hollow's floor", bool(at) and abs(at[2] - (floor + 90.0)) < 150.0, f"pawn z {at[2] if at else 'none'}, floor {floor:.0f}")
            ui.console(NAME, "MO.Cam.LookAt %.0f %.0f %.0f %.0f %.0f %.0f 70" % (x - ux * 900.0, y - uy * 900.0, floor + 500.0, sx, sy, floor + 100.0))
            time.sleep(6)
            cfl.shoot(a.shots, "overhang_from_the_bowl", started)
            face(ux, uy)
            ui.console(NAME, "MO.Test.MoveFor 0 1 7.0")
            time.sleep(8.0)
            walked = pawn_at()
            into = (walked[0] - x) * ux + (walked[1] - y) * uy if walked else -1.0
            mid = ((sx - x) * ux + (sy - y) * uy)
            rep.step("the overhang: walking uphill from the bowl's middle reaches the shelter's middle", into >= mid, f"{into:.0f} cm along, its middle is {mid:.0f} cm along, z {walked[2] if walked else 'none'}")
            rep.step("the overhang: ... on its floor, not up over its roof", bool(walked) and walked[2] < floor + 200.0, f"pawn z {walked[2] if walked else 'none'}, floor {floor:.0f}")
            inside = stand(sx + ux * 200.0, sy + uy * 200.0, floor + 150.0, settle=3.0)
            rays_in = shelter_rays()
            print(f"under the roof at {inside}: cover, walls, enclosure, indoors = {rays_in}", flush=True)
            ui.console(NAME, "MO.Cam.LookAt %.0f %.0f %.0f %.0f %.0f %.0f 80" % (sx + ux * 200.0, sy + uy * 200.0, floor + 160.0, x, y, floor + 100.0))
            time.sleep(6)
            cfl.shoot(a.shots, "overhang_looking_out", started)
            count0 = buildables()
            face(-ux, -uy)
            ui.console(NAME, "MO.Test.PlaceBuildingRPC BuildCampfire 150")
            time.sleep(4.0)
            count1 = buildables()
            rep.step("the overhang: a campfire can be placed on its floor", count1 == count0 + 1, f"buildings {count0} -> {count1}")
            open_at = stand(x + ux * 4000.0, y + uy * 4000.0, ground + 4000.0, settle=20.0)
            rays_out = shelter_rays()
            print(f"open ground beside it at {open_at}: cover, walls, enclosure, indoors = {rays_out}", flush=True)
            rep.step("the overhang: partial shelter, more cover under its roof than on the open ground", bool(rays_in and rays_out) and rays_in[0] >= 0.3 and rays_in[0] > rays_out[0], f"{rays_in} against {rays_out}")

        for which in ("Overhang", "DeepCave"):
            off = ui.log_size(NAME)
            ui.console(NAME, f"poi {which}")
            m = ui.wait_log(NAME, rf"\[MOConsole\] poi {which}: hollow at x=(-?\d+) y=(-?\d+)", since=off, timeout=30)
            rep.step(f"the console's poi menu finds a {which}", bool(m), m.group(0)[-80:] if m else "no line")
            time.sleep(6.0)
        print(f"shots in {a.shots}", flush=True)
    finally:
        ui.stop(NAME)
    print("CAVE MIX %s" % ("PASS" if rep.ok else "FAIL"), flush=True)
    return 0 if rep.ok else 1


if __name__ == "__main__":
    sys.exit(main())
