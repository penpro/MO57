"""In PIE: every big rock / cliff instance near the pawn (or NEARROCKS_AT="x y"), nearest first -- mesh, scale, owner, the gap under it and how far it
sinks: which placement a "this cliff floats / is too big" report means.

    python Tools/ue.py py --file Content/Python/near_rocks.py        (PIE; NEARROCKS_AT="x y", NEARROCKS_R / NEARROCKS_MIN metres optional)

Lines: NEARROCK <dist m> <mesh> x<scale> <owner> base z vs ground: the instance's lowest point (its bounds) against the generator ground under its
centre, and the size of the mesh at that scale.
"""
import math
import os

import unreal

KEYS = ("Cliff", "Crag", "Rock", "Boulder", "Slab", "Assembly", "Formation", "Buildup", "Arch", "Shelf", "venrdcgga", "ullqbb1ga", "vdssailfa",
        "veqnci0db", "ulypfbkqa", "xgknben", "Quarry", "Stone")
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
at = os.environ.pop("NEARROCKS_AT", "")
radius = float(os.environ.pop("NEARROCKS_R", "150")) * 100.0
min_size = float(os.environ.pop("NEARROCKS_MIN", "6")) * 100.0   # the largest side, metres
if at:
    x, y = (float(v) for v in at.split())
    here = unreal.Vector(x, y, 0)
else:
    here = unreal.GameplayStatics.get_player_pawn(w, 0).get_actor_location()
rows = []
for comp_cls in (unreal.InstancedStaticMeshComponent,):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        comps = actor.get_components_by_class(comp_cls)
        for c in comps:
            sm = c.get_editor_property("static_mesh")
            if not sm or not any(k.lower() in sm.get_name().lower() for k in KEYS):
                continue
            b = sm.get_bounding_box()
            for i in range(c.get_instance_count()):
                t = c.get_instance_transform(i, True)
                p = t.translation
                d = math.hypot(p.x - here.x, p.y - here.y)
                if d > radius:
                    continue
                size = (b.max - b.min) * t.scale3d.x
                if max(abs(size.x), abs(size.y), abs(size.z)) < min_size:
                    continue
                rows.append((d, sm.get_name(), t.scale3d.x, actor.get_name(), p, abs(size.x), abs(size.y), abs(size.z), t.rotation.rotator()))
rows.sort()
out("NEARROCK %d instance(s) within %.0f m of (%.0f, %.0f)" % (len(rows), radius / 100, here.x, here.y))
for d, mesh, sc, owner, p, sx, sy, sz, rot in rows[:60]:
    out("NEARROCK %6.1f m  %-48s x%.2f  %-40s at (%.0f, %.0f, %.0f)  size %.0fx%.0fx%.0f m  rot p%.0f y%.0f r%.0f" % (
        d / 100, mesh[:48], sc, owner[:40], p.x, p.y, p.z, sx / 100, sy / 100, sz / 100, rot.pitch, rot.yaw, rot.roll))
