"""The Dark Ruins compositions' TOP dressing points, traced against the rock itself (N-211; Wes, Oct 25 2026: "we need stuff spawned on top
of them").

dr_dressing_points.py sampled the meshes' LOD0 triangles, which for these Nanite scans are the coarse fallback: measured in PIE, a third of the
plants hung 2-3 m over the terrain beside the rock and the rest stood a median 0.6-1.4 m above it. This script rebuilds the "top" points from
the surface a pawn stands on: in the PIE world (transient -- nothing is saved) it spawns each assembly's pieces as static-mesh actors 3 km up,
with the sample's transforms (Saved/DarkRuinsDump/assemblies.json, the frame of the PCG data asset), and traces straight down once per
TOP_M2 square metres (a jittered grid, complex collision). The first hit from above is the highest surface at that spot; it is kept when it
faces up (normal z >= UP_NZ) and lies above MIN_Z -- the base band (MO Conform To Ground's FollowBelowCm + FadeCm), whose pieces are each
re-seated on the terrain on their own, so a plant there would leave its rock. The pieces are destroyed afterwards.

Rewrites only the "top" lists of Content/Python/data/dr_dressing_points.json (the foot ring stays dr_dressing_points.py's). Then rebuild the
graphs (make_dr_compositions.py, PIE ended).

    python Tools/ue.py boot --seed <any>                                           (PIE running)
    python Tools/ue.py py --file Content/Python/dr_top_points_traced.py --timeout 300
"""
import json
import os
import random

import unreal

ASSEMBLIES = ["Mountain_01", "TallPath_01"]
DUMP = "D:/UEProjects/MO57/Saved/DarkRuinsDump/assemblies.json"
OUT = os.path.join(unreal.Paths.project_content_dir(), "Python", "data", "dr_dressing_points.json")
TOP_M2 = 1.5
UP_NZ = 0.6
MIN_Z = 500.0          # MO Conform To Ground: FollowBelowCm 400 + FadeCm 100 (actor units)
MARGIN = 2500.0        # beyond the pivots: the pieces' own extent
LIFT = 300000.0        # 3 km above the pawn: nothing else up there

log = lambda m: out("DRTRACE " + m)
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not w:
    raise RuntimeError("DRTRACE needs PIE running (it spawns into the transient game world)")
pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
origin = pawn.get_actor_location() + unreal.Vector(0, 0, LIFT)
dump = json.load(open(DUMP, encoding="utf-8"))
data = json.load(open(OUT, encoding="utf-8"))
rng = random.Random(211)
cell = (TOP_M2 * 10000.0) ** 0.5


def spawn(piece):
    """Python cannot spawn into the PIE world; the cheat manager's `summon` can (an actor in front of the player), then it is set up here."""
    before = set(x.get_name() for x in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor))
    unreal.SystemLibrary.execute_console_command(w, "summon /Script/Engine.StaticMeshActor")
    new = [x for x in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor) if x.get_name() not in before]
    if not new:
        raise RuntimeError("DRTRACE summon made no actor")
    a = new[0]
    r = piece["rot"]
    xf = unreal.Transform(location=origin + unreal.Vector(*piece["loc"]), rotation=unreal.Rotator(roll=r[0], pitch=r[1], yaw=r[2]),
                          scale=unreal.Vector(*piece["scale"]))
    c = a.get_editor_property("static_mesh_component")
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_static_mesh(unreal.load_asset(piece["mesh"].split(".")[0]))
    c.set_collision_profile_name("BlockAll")
    a.set_actor_transform(xf, False, True)
    return a


base_origin = origin
for index, name in enumerate(ASSEMBLIES):
    # each assembly 300 m from the last: a destroyed actor keeps its collision until the frame ends, and this whole run is one frame
    origin = base_origin + unreal.Vector(30000.0 * index, 0, 0)
    pieces = dump[name]["pieces"]
    actors = [spawn(p) for p in pieces]
    o, _e = actors[0].get_actor_bounds(False)   # self-check: a trace straight down through the first piece must hit it
    probe = unreal.SystemLibrary.line_trace_single(w, o + unreal.Vector(0, 0, 30000), o - unreal.Vector(0, 0, 30000),
                                                   unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    log("%s: first piece at %s, probe hit %s" % (name, actors[0].get_actor_location(), probe.to_tuple()[9].get_name() if probe else None))
    lo, hi = dump[name]["pivot_bounds"]
    x0, x1, y0, y1 = lo[0] - MARGIN, hi[0] + MARGIN, lo[1] - MARGIN, hi[1] + MARGIN
    top_z = origin.z + hi[2] + 4000.0
    bottom_z = origin.z + lo[2] - 6000.0
    pts, traces, steep, low, missed = [], 0, 0, 0, 0
    y = y0
    while y < y1:
        x = x0
        while x < x1:
            px, py = x + rng.random() * cell, y + rng.random() * cell
            traces += 1
            hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(origin.x + px, origin.y + py, top_z),
                                                         unreal.Vector(origin.x + px, origin.y + py, bottom_z),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
            if hit:
                h = hit.to_tuple()
                loc, normal, hit_actor = h[4], h[7], h[9]
                if hit_actor not in actors:
                    missed += 1
                elif normal.z < UP_NZ:
                    steep += 1
                elif loc.z - origin.z < MIN_Z:
                    low += 1
                else:
                    pts.append([round(loc.x - origin.x, 1), round(loc.y - origin.y, 1), round(loc.z - origin.z, 1)])
            x += cell
        y += cell
    for a in actors:
        a.destroy_actor()
    before = len(data.get(name, {}).get("top", []))
    if not pts:   # a broken run (nothing blocked the traces) must not wipe the points
        raise RuntimeError("DRTRACE %s: only %d top points from %d traces -- not written" % (name, len(pts), traces))
    data.setdefault(name, {})["top"] = pts
    log("%s: %d pieces, %d traces -> %d top points (the file had %d); %d steep, %d in the base band, %d hit something else"
        % (name, len(pieces), traces, len(pts), before, steep, low, missed))

json.dump(data, open(OUT, "w", encoding="utf-8"))
log("wrote %s" % OUT)
