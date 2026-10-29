"""Climb a cliff for real (Wes, Oct 29: "i want to be able to climb on top of the electric dreams cliffs"): what the rock lets a climber do, with
the stamina cost of climbing set to 0 for the run (F51 decides the cost; this measures the geometry: overhangs, faces lost, the pull-over at
the top).

One -game instance, a new world (seed 777 by default). The cliff mesh named (default SM_Cliff_09, an Electric Dreams cliff) is found in the
world; each of 8 sides is tried for a steep face that belongs to that mesh (a trace from 10 m out at knee height), the pawn stands at its foot
facing it, W is held and a jump grabs; whenever it is off the face (on a ledge it pulled onto, past a corner, let go) it jumps again, as a
player would; stuck on the face for 4 s (a roof, an overhang) it steers across it (A or D, wider each time) and pushes up again. Every 2 s: the feet's height, the movement mode, climbing or not, and the traversal lines the movement
component logged. Ends when the feet get no higher for 30 s (6 s with --no-steer), at the top, or after --seconds.

    python Tools/check_cliff_climb.py [--mesh SM_Cliff_09] [--seed 777] [--seconds 150] [--side 0] [--all] [--no-steer] [--keep] [--reuse]

Prints PLACE (the face found), one SAMPLE line per 2 s, and one RESULT per side climbed: the highest the feet got against the cliff's top,
and how it ended (TOPPED OUT; no higher for 30 s still on the face: a roof the steering did not get round; off the face). --all climbs
every side with a face; --reuse runs in a --keep instance already in the world.
"""
import argparse
import re
import sys
import time

sys.path.insert(0, __import__("os").path.dirname(__import__("os").path.abspath(__file__)))
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "cliff"

PLACE = r'''
import math
p = unreal.GameplayStatics.get_player_pawn(world, 0)
p.set_editor_property("climbing_stamina_cost_per_second", 0.0)
cap = p.get_editor_property("capsule_component")
r, h = cap.get_scaled_capsule_radius(), cap.get_scaled_capsule_half_height()
comp, t0 = None, None
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        if c.static_mesh and c.static_mesh.get_name() == "__MESH__":
            comp = c
            t0 = c.get_instance_transform(0, True) if isinstance(c, unreal.InstancedStaticMeshComponent) else c.get_world_transform()
            break
    if comp:
        break
res = "none: no %s in the world" % "__MESH__"
if comp:
    bb = comp.static_mesh.get_bounding_box()
    sc = t0.scale3d
    centre = t0.translation + unreal.Vector((bb.min.x + bb.max.x) * 0.5 * sc.x, (bb.min.y + bb.max.y) * 0.5 * sc.y, 0.0)
    top = t0.translation.z + bb.max.z * sc.z
    reach = max((bb.max.x - bb.min.x) * abs(sc.x), (bb.max.y - bb.min.y) * abs(sc.y)) * 0.5 + 1000.0
    res = "none: no steep face of it from 8 sides (top z %.0f)" % top
    for k in range(__SIDE__, 8):
        ang = k * math.pi / 4.0
        sx, sy = centre.x + reach * math.cos(ang), centre.y + reach * math.sin(ang)
        g = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(sx, sy, top + 5000.0), unreal.Vector(sx, sy, top - 30000.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [p], unreal.DrawDebugTrace.NONE, True)
        if not g:
            continue
        gz = g.to_tuple()[4].z
        found = None
        for knee in (100.0, 160.0, 220.0):
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(sx, sy, gz + knee), unreal.Vector(centre.x, centre.y, gz + knee), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [p], unreal.DrawDebugTrace.NONE, True)
            if not hit:
                continue
            tt = hit.to_tuple()
            if tt[10] == comp and abs(tt[7].z) < 0.5:
                found = tt
                break
        if not found:
            continue
        loc, nrm = found[4], found[7]
        nn = math.hypot(nrm.x, nrm.y) or 1.0
        nx, ny = nrm.x / nn, nrm.y / nn
        px, py = loc.x + nx * (r + 15.0), loc.y + ny * (r + 15.0)
        g2 = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(px, py, loc.z + 300.0), unreal.Vector(px, py, loc.z - 1000.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [p], unreal.DrawDebugTrace.NONE, True)
        if not g2:
            continue
        feet = g2.to_tuple()[4].z
        yaw = math.degrees(math.atan2(-ny, -nx))
        p.get_movement_component().set_movement_mode(unreal.MovementMode.MOVE_FALLING)   # out of the last side's climb
        p.set_actor_location_and_rotation(unreal.Vector(px, py, feet + h + 2.0), unreal.Rotator(0.0, 0.0, yaw), False, True)
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        pc.set_control_rotation(unreal.Rotator(0.0, 0.0, yaw))
        res = "placed side %d at (%.0f,%.0f) feet z %.0f facing yaw %.0f; face normal z %.2f; cliff top z %.0f (%.0f m above the feet)" % (k, px, py, feet, yaw, nrm.z, top, (top - feet) / 100.0)
        break
out("PLACE " + res)
'''

SAMPLE = r'''
p = unreal.GameplayStatics.get_player_pawn(world, 0)
m = p.get_movement_component()
cap = p.get_editor_property("capsule_component")
l = p.get_actor_location()
out("SAMPLE feet=%.0f mode=%s custom=%d climbing=%s" % (l.z - cap.get_scaled_capsule_half_height(), m.movement_mode, m.custom_movement_mode, p.is_climbing()))
if p.is_climbing():
    # A player keeps the camera on the face: W is camera-relative, and a face that turned 50 degrees off the camera splits W into up and across.
    unreal.GameplayStatics.get_player_controller(world, 0).set_control_rotation(unreal.Rotator(0.0, 0.0, p.get_actor_rotation().yaw))
'''


def q(code, key, timeout=120):
    _, out = ui.call(NAME, ["py", "-c", code], timeout=timeout)
    m = re.search(key + r" ([^\r\n]*)", out)
    return m.group(1) if m else ""


def climb_one(a, side):
    """Place at the first side >= side with a face of the mesh and climb it with W held; returns (side placed or None, result line)."""
    placed = q(PLACE.replace("__MESH__", a.mesh).replace("__SIDE__", str(side)), "PLACE")
    print("PLACE", placed or "no answer")
    m = re.match(r"placed side (\d+)", placed)
    if not m:
        return None, ""
    top = int(re.search(r"cliff top z (-?\d+)", placed).group(1))
    start_feet = int(re.search(r"feet z (-?\d+)", placed).group(1))   # the ground it stood on (not the first sample: the grab has risen by then)
    time.sleep(3)   # the far world round the new spot streams in
    off = ui.log_size(NAME)
    ui.console(NAME, "MO.Test.InjectInput IA_MOMove 0 1 %.0f" % a.seconds)
    held_until = time.time() + a.seconds
    time.sleep(0.4)
    ui.console(NAME, "MO.Test.InjectInput IA_MOJump 1 0 0.15")
    best, last_rise_at, t0, end, s = -1e9, time.time(), time.time(), "time up", ""
    steer, steer_len, steered_at, steers, topped = 1, 3.0, 0.0, 0, False
    while time.time() - t0 < a.seconds:
        time.sleep(2.0)
        s = q(SAMPLE, "SAMPLE")
        feet = re.search(r"feet=(-?\d+)", s)
        if feet:
            f = int(feet.group(1))
            if f > best + 5:
                best, last_rise_at = f, time.time()
            if "climbing=False" in s and "custom=0" in s and f > top - 150:
                topped = True
        print("%5.0fs %s" % (time.time() - t0, s))
        if topped:
            break
        if "climbing=False" in s and "custom=0" in s:
            ui.console(NAME, "MO.Test.InjectInput IA_MOJump 1 0 0.15")   # off the face (a ledge pulled onto, a corner, let go): jump at it again, as a player would
        elif "climbing=True" in s and a.steer and time.time() - max(last_rise_at, steered_at) > 4.0:
            # Stuck on the face (a roof, an overhang): across it and up again, wider each time, as a player would look for a line.
            print("      steer %s %.1f s" % ("right" if steer > 0 else "left", steer_len))
            ui.console(NAME, "MO.Test.InjectInput IA_MOMove %d 0 %.1f" % (steer, steer_len))
            time.sleep(steer_len)
            ui.console(NAME, "MO.Test.InjectInput IA_MOMove 0 1 %.0f" % max(1.0, held_until - time.time()))
            steer, steer_len, steered_at, steers = -steer, min(steer_len * 1.5, 12.0), time.time(), steers + 1
        if time.time() - last_rise_at > (30.0 if a.steer else 6.0):
            end = "no higher for %d s" % (30 if a.steer else 6)
            break
    ui.console(NAME, "MO.Test.InjectInput IA_MOMove 0 0 0.1")   # let go of the keys (a newer injection replaces the held one)
    lines = [l.split("] ", 1)[-1] for l in re.findall(r"\[MOTraversal\][^\r\n]*", ui.read_log_from(NAME, off))]
    for l in lines[:16]:
        print("   ", l[:220])
    last = re.search(r"feet=(-?\d+).*climbing=(\w+)", s)
    how = end
    if topped:
        how = "TOPPED OUT"
    elif last and last.group(2) == "True":
        how += ", still on the face"
    elif last:
        how += ", off the face"
    how += "; %d steer(s)" % steers
    return int(m.group(1)), "side %s: the feet rose %.1f m of the cliff's %.1f m (%s); %d traversal line(s)" % (
        m.group(1), (best - start_feet) / 100.0, (top - start_feet) / 100.0, how, len(lines))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mesh", default="SM_Cliff_09")
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--seconds", type=float, default=150.0)
    ap.add_argument("--side", type=int, default=0, help="the first side tried (0-7)")
    ap.add_argument("--all", action="store_true", help="climb every side that has a face, one after another")
    ap.add_argument("--reuse", action="store_true", help="the instance is already in a world (a --keep run)")
    ap.add_argument("--no-steer", dest="steer", action="store_false", help="W only: no steering across the face when stuck")
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    try:
        if not a.reuse:
            ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, creator=True, res=(1280, 720), nosteam=True)
            if not ui.wait_ready(NAME, 300):
                print("FAIL: the instance did not boot")
                return 1
            ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
            off = ui.log_size(NAME)
            ui.console(NAME, "MO.NewGame %d" % a.seed)
            if not ui.wait_log(NAME, r"Character creator opened", since=off, timeout=420):
                print("FAIL: no character creator")
                return 1
            time.sleep(5)
            ui.console(NAME, "MO.Test.ClickWidget BeginButton")
            time.sleep(40)
        results, side = [], a.side
        while side < 8:
            placed, line = climb_one(a, side)
            if placed is None:
                break
            results.append(line)
            if not a.all:
                break
            side = placed + 1
        for line in results:
            print("RESULT", line)
        return 0 if results else 1
    finally:
        if not a.keep:
            ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(main())
