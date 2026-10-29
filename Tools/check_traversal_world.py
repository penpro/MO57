"""Traversal on the world's own rocks, cliffs and trees (Docs/Movement_And_Traversal.md "Not yet checked: a real cliff and tree in the
world"; Wes, Oct 29: "i can't vault or climb rocks and such", "i want to be able to climb on top of the electric dreams cliffs").

One -game instance, a new world (seed 777 by default), the survivor walked up to real obstacles round the start: for each kind of mesh the
world placed (PCG rocks and boulders, the landmark crags, the Electric Dreams cliffs and spires, tree trunks), a few instances of different
heights. The pawn is set on the ground in front of each, facing its face, and MO.Test.Traversal says what a jump would do there (vault /
hop up / pull-up / plain jump), whether the face is climbable, and why not.

    python Tools/check_traversal_world.py [--seed N] [--radius M] [--keep]

Prints one TRY line per obstacle (its height above the ground beside it: rocks sit sunk into the terrain) and a table per mesh kind. Read-only: nothing in the world changes but the pawn's place.
"""
import argparse
import json
import re
import sys
import time

sys.path.insert(0, __import__("os").path.dirname(__import__("os").path.abspath(__file__)))
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "trav"

# In the instance: every blocking static mesh (and instance) within RADIUS of the pawn whose mesh reads as rock / cliff / tree, or is an
# Electric Dreams or landmark mesh, as candidates: [kind, mesh, x, y, base z, top z, half width].
FIND = r'''
import json, math
p = unreal.GameplayStatics.get_player_pawn(world, 0)
c = p.get_actor_location()
R = RADIUS * 100.0
WORDS = ("rock", "cliff", "boulder", "crag", "stone", "shelf", "spire", "tree", "trunk")
found = []
seen = {}
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    for comp in a.get_components_by_class(unreal.StaticMeshComponent):
        sm = comp.static_mesh
        if not sm or comp.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
            continue
        path = sm.get_path_name()
        low = path.lower()
        tags = [str(t) for t in comp.component_tags] + [str(t) for t in a.tags]
        if not (any(w in low for w in WORDS) or "electricdreams" in low or "/ed_" in low or "MOLandmark" in tags or "MOTreeTrunk" in tags):
            continue
        box = sm.get_bounding_box()
        transforms = []
        if isinstance(comp, unreal.InstancedStaticMeshComponent):
            for i in range(comp.get_instance_count()):
                transforms.append(comp.get_instance_transform(i, True))
        else:
            transforms.append(comp.get_world_transform())
        for t in transforms:
            loc = t.translation
            d = math.hypot(loc.x - c.x, loc.y - c.y)
            if d > R or d < 300:
                continue
            s = t.scale3d
            zs = [loc.z + box.min.z * s.z, loc.z + box.max.z * s.z]
            half = 0.5 * max((box.max.x - box.min.x) * abs(s.x), (box.max.y - box.min.y) * abs(s.y))
            key = path.split("/")[-1].split(".")[0]
            seen[key] = seen.get(key, 0) + 1
            if seen[key] > 40:
                continue
            top = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(loc.x, loc.y, max(zs) + 50.0), unreal.Vector(loc.x, loc.y, min(zs) - 50.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [p], unreal.DrawDebugTrace.NONE, True)
            ax, ay = loc.x + (c.x - loc.x) / d * (half + 60.0), loc.y + (c.y - loc.y) / d * (half + 60.0)
            gnd = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(ax, ay, max(zs) + 500.0), unreal.Vector(ax, ay, min(zs) - 2000.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [p], unreal.DrawDebugTrace.NONE, True)
            if not top or not gnd:
                continue
            tz, gz = top.to_tuple()[4].z, gnd.to_tuple()[4].z
            if tz - gz < 20.0:
                continue
            found.append([key, path, loc.x, loc.y, gz, tz, half, ",".join(t for t in tags if t.startswith("MO"))])
out("CANDIDATES " + json.dumps({"pawn": [c.x, c.y, c.z], "found": found, "kinds": seen}))
'''

# In the instance: stand the pawn on the ground in front of the face of the obstacle at (X, Y), facing it. The face is found from the pawn's
# side by a trace at knee height toward the obstacle's centre; the ground under the stand by a trace down.
PLACE = r'''
import math
p = unreal.GameplayStatics.get_player_pawn(world, 0)
cap = p.get_editor_property("capsule_component")
r, h = cap.get_scaled_capsule_radius(), cap.get_scaled_capsule_half_height()
dx, dy = __X__ - __PX__, __Y__ - __PY__
n = math.hypot(dx, dy) or 1.0
dx, dy = dx / n, dy / n
res = "none"
for knee in (60.0, 100.0, 140.0):
    z = __BASE__ + knee   # __BASE__: the ground just outside the obstacle
    start = unreal.Vector(__X__ - dx * (__HALF__ + 400.0), __Y__ - dy * (__HALF__ + 400.0), z)
    end = unreal.Vector(__X__, __Y__, z)
    hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [p], unreal.DrawDebugTrace.NONE, True)
    if hit:
        t = hit.to_tuple()
        loc, nrm = t[4], t[7]
        nx, ny = nrm.x, nrm.y
        nn = math.hypot(nx, ny) or 1.0
        nx, ny = nx / nn, ny / nn
        sx, sy = loc.x + nx * (r + 20.0), loc.y + ny * (r + 20.0)
        g = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(sx, sy, z + 300.0), unreal.Vector(sx, sy, z - 600.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [p], unreal.DrawDebugTrace.NONE, True)
        if g:
            gz = g.to_tuple()[4].z
            yaw = math.degrees(math.atan2(-ny, -nx))
            p.set_actor_location_and_rotation(unreal.Vector(sx, sy, gz + h + 2.0), unreal.Rotator(0.0, 0.0, yaw), False, True)
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if pc:
                pc.set_control_rotation(unreal.Rotator(0.0, 0.0, yaw))   # a controller-driven capsule turns back to the view
            res = "placed face=(%.0f,%.0f,%.0f) normalZ=%.2f ground=%.0f hit=%s" % (loc.x, loc.y, loc.z, nrm.z, gz, t[9].get_name() if t[9] else "?")
            break
out("PLACE " + res)
'''


CREATOR_VIEW = r'''
pc = unreal.GameplayStatics.get_player_controller(world, 0)
cam = pc.get_view_target()
s = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOMetaHumanCharacter)]
s = min(s, key=lambda a: (a.get_actor_location() - cam.get_actor_location()).length()) if s else None
head = s.get_editor_property("mesh").get_socket_location("head") if s else None
c = cam.get_actor_location()
blocked = []
if head:
    d = (c - head).length()
    for comp in [x for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) for x in a.get_components_by_class(unreal.InstancedStaticMeshComponent)]:
        if not comp.is_visible() or comp.get_instance_count() == 0:
            continue
        if not comp.get_instances_overlapping_sphere((head + c) * 0.5, d * 0.5 + 30.0, True):
            continue   # nothing of it anywhere near the line
        for k in range(4, int(d / 15.0)):
            p = head + (c - head) * (k * 15.0 / d)
            if comp.get_instances_overlapping_sphere(p, 8.0, True):
                blocked.append("%s at %.0f cm" % (comp.static_mesh.get_name() if comp.static_mesh else "?", k * 15.0))
                break
out("CREATORVIEW camera %s, %.0f cm from the head; plants on the line: %s" % (cam.get_name(), (c - head).length() if head else -1, ", ".join(blocked[:4]) or "none"))
'''


def q(code, key, timeout=60):
    _, out = ui.call(NAME, ["py", "-c", code], timeout=timeout)
    m = re.search(key + r" ([^\r\n]*)", out)
    return m.group(1) if m else ""


FEET = r'''
p = unreal.GameplayStatics.get_player_pawn(world, 0)
cap = p.get_editor_property("capsule_component")
l = p.get_actor_location()
m = p.get_movement_component()
out("FEET %.0f %.0f %.0f %s" % (l.x, l.y, l.z - cap.get_scaled_capsule_half_height(), m.movement_mode))
'''


def do_jump(kind):
    """The move for real: W held, a jump; then where the feet ended against where they started (a hop up / pull-up rises by about the ledge's
    height, a vault lands past the obstacle at about the same height) and the traversal line the movement component logged."""
    before = re.search(r"FEET (-?\d+) (-?\d+) (-?\d+)", "FEET " + q(FEET, "FEET"))
    off = ui.log_size(NAME)
    ui.console(NAME, "MO.Test.InjectInput IA_MOMove 0 1 1.2")
    time.sleep(0.15)
    moving = traversal()   # what a jump would do now, walking at it
    ui.console(NAME, "MO.Test.InjectInput IA_MOJump 1 0 0.15")
    time.sleep(2.5)
    after = re.search(r"FEET (-?\d+) (-?\d+) (-?\d+) (\S+)", "FEET " + q(FEET, "FEET"))
    logged = re.findall(r"\[MOTraversal\] [^\r\n]*", ui.read_log_from(NAME, off))
    if not (before and after):
        return "no position"
    dz = int(after.group(3)) - int(before.group(3))
    dxy = ((int(after.group(1)) - int(before.group(1))) ** 2 + (int(after.group(2)) - int(before.group(2))) ** 2) ** 0.5
    walking = re.search(r"jump=([a-z -]+?) height=(\d+).*why=(.*)$", moving)
    return "feet rose %d cm, moved %.0f cm, mode %s | %s | walking at it: %s" % (dz, dxy, after.group(4),
        (logged[0].split("] ", 1)[-1] if logged else "no traversal logged")[:100],
        ("%s %s %s" % walking.groups()) if walking else moving[:80])


def traversal():
    off = ui.log_size(NAME)
    ui.console(NAME, "MO.Test.Traversal")
    line = ui.wait_log(NAME, r"\[MOQUERY\] TRAVERSAL [^\r\n]*", since=off, timeout=15)
    return line.group(0) if hasattr(line, "group") else (line or "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--radius", type=float, default=120.0, help="metres round the start")
    ap.add_argument("--per-kind", type=int, default=4)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--execute", action="store_true", help="where a move is offered, really jump (W held) and report where the body ended")
    a = ap.parse_args()
    try:
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
        # The creator's view (N-213): is the line from the survivor's head to the camera clear of plant instances (grass and foliage have no
        # collision; the camera pulls in front of them since Oct 29)?
        view = q(CREATOR_VIEW, "CREATORVIEW", timeout=60)
        pulled = re.findall(r"\[MOCharacterCreator\] camera pulled in[^\r\n]*", ui.read_log_from(NAME, off))
        print("creator view: %s%s" % (view or "no answer", (" | " + pulled[-1][:160]) if pulled else " | the camera was not pulled in"))
        ui.console(NAME, "MO.Test.ClickWidget BeginButton")
        time.sleep(40)   # the world round the start: voxel terrain, PCG rocks and trees, world features
        raw = q(FIND.replace("RADIUS", str(a.radius)), "CANDIDATES", timeout=120)
        if not raw:
            print("FAIL: no candidate list")
            return 1
        data = json.loads(raw)
        px, py, _ = data["pawn"]
        print("kinds within %.0f m: %s" % (a.radius, json.dumps(data["kinds"])))
        by_kind = {}
        for f in data["found"]:
            by_kind.setdefault(f[0], []).append(f)
        results = []
        for kind, items in sorted(by_kind.items()):
            items.sort(key=lambda f: f[5] - f[4])   # by height
            picks = [items[int(i * (len(items) - 1) / max(1, a.per_kind - 1))] for i in range(min(a.per_kind, len(items)))]
            for f in picks:
                _, path, x, y, base, top, half, tags = f
                placed = q(PLACE.replace("__PX__", repr(px)).replace("__PY__", repr(py)).replace("__X__", repr(x)).replace("__Y__", repr(y))
                           .replace("__BASE__", repr(base)).replace("__HALF__", repr(half)), "PLACE")
                time.sleep(0.6)   # a few frames: the move settles, the floor is found
                line = traversal() if placed.startswith("placed") else ""
                jump = re.search(r"jump=([a-z -]+?) height", line)
                climb = re.search(r"climbable=(\w+)", line)
                why = re.search(r"why=(.*)$", line)
                res = (kind, round(top - base), tags or "-", jump.group(1) if jump else "?", climb.group(1) if climb else "?",
                       (why.group(1) if why else placed)[:90])
                results.append(res)
                print("TRY %-34s %5d cm above ground  tags=%-12s jump=%-10s climbable=%-4s %s" % res)
                if a.execute and res[3] in ("vault", "hop up", "pull-up"):
                    print("    DID %s" % do_jump(res[3]))
        print("\nmesh kind / height / jump / climbable:")
        for r in results:
            print("  %s" % (r,))
        return 0
    finally:
        if not a.keep:
            ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(main())
