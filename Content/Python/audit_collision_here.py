"""Does the scatter around the player actually stop the player? A TRACE audit of the running PIE world -- the truth, not the asset settings.

For every instanced mesh near the pawn (anything larger than MIN_SIZE_CM), a few instances are tested the way the character meets them:
  * drop  -- a pawn-sized capsule falls onto the instance from above: does it land ON it (or fall through to the ground)?
  * walk  -- the capsule walks into it horizontally at hip height from WALK_FROM_CM away: is it stopped by it?
plus what the component says (collision profile / enabled) and the mesh's trace flag. Reading collision COUNTS in PIE returns -1 (an artifact), which is why the
older audit_mesh_collision.py reported "no collision" for meshes that have hulls -- this one never asks, it traces.

    python Tools/ue.py boot --seed -1675528670
    python Tools/ue.py run "MO.Player.Teleport X Y Z"          (a bug report's Player.Location; wait ~15 s for the scatter)
    python Tools/ue.py py -c "import audit_collision_here as a; a.audit(40)"

Output lines start with COLLHERE; one per mesh, worst first.
"""
import math

import unreal

MIN_SIZE_CM = 60.0       # smaller things (grass, pebbles) are walk-through by design
PER_MESH = 6             # instances tested per mesh
WALK_FROM_CM = 400.0
CAPSULE_R, CAPSULE_HH = 34.0, 88.0


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def _sweep(world, start, end, ignore):
    r = unreal.SystemLibrary.capsule_trace_single(world, start, end, CAPSULE_R, CAPSULE_HH, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore,
                                                  unreal.DrawDebugTrace.NONE, True)
    return r.to_tuple() if r is not None else None


def audit(radius_m=40.0, per_mesh=PER_MESH):
    world = _world()
    if world is None:
        print("COLLHERE no game world (boot PIE first)")
        return
    pawn = unreal.GameplayStatics.get_player_controller(world, 0).get_controlled_pawn()
    origin = pawn.get_actor_location()
    radius = radius_m * 100.0
    ignore = [pawn]
    rows = {}
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if actor == pawn or actor.get_class().get_name() == "VoxelWorld":
            continue
        for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            mesh = comp.get_editor_property("static_mesh")
            if mesh is None or comp.get_instance_count() == 0:
                continue
            path = mesh.get_path_name()
            if path.startswith("/Engine/"):
                continue
            box = mesh.get_bounding_box()
            local_size = box.max - box.min
            row = rows.setdefault(path, {"tested": 0, "landed": 0, "walk_tested": 0, "blocked": 0, "size": 0.0, "profiles": set(), "enabled": set(),
                                         "owners": set(), "flag": "?"})
            row["profiles"].add(str(comp.get_collision_profile_name()))
            row["enabled"].add(comp.get_collision_enabled().name)
            row["owners"].add(actor.get_class().get_name())
            try:
                row["flag"] = mesh.get_editor_property("body_setup").get_editor_property("collision_trace_flag").name.replace("CTF_", "")
            except Exception:
                pass
            for k in range(comp.get_instance_count()):
                if row["tested"] >= per_mesh:
                    break
                t = comp.get_instance_transform(k, True)
                c = t.translation
                if math.hypot(c.x - origin.x, c.y - origin.y) > radius:
                    continue
                s = t.scale3d
                size = max(abs(local_size.x * s.x), abs(local_size.y * s.y), abs(local_size.z * s.z))
                if size < MIN_SIZE_CM:
                    continue
                row["size"] = max(row["size"], size)
                top = c.z + abs(box.max.z * s.z) + 300.0
                drop = _sweep(world, unreal.Vector(c.x, c.y, top), unreal.Vector(c.x, c.y, c.z - 300.0), ignore)
                row["tested"] += 1
                if drop and drop[0] and drop[10] == comp:
                    row["landed"] += 1
                # Walk in at hip height from the side facing the pawn's spawn (any side would do; one is enough per instance).
                ground = _sweep(world, unreal.Vector(c.x + WALK_FROM_CM, c.y, c.z + 2000.0), unreal.Vector(c.x + WALK_FROM_CM, c.y, c.z - 2000.0), ignore + [actor])
                hip = (ground[4].z if ground and ground[0] else c.z + CAPSULE_HH) + 10.0
                if abs(local_size.z * s.z) > 120.0:   # only things at least knee-high can stop a walk
                    row["walk_tested"] += 1
                    walk = _sweep(world, unreal.Vector(c.x + WALK_FROM_CM, c.y, hip), unreal.Vector(c.x - 50.0, c.y, hip), ignore)
                    if walk and walk[0] and walk[10] == comp:
                        row["blocked"] += 1
    tested = {p: r for p, r in rows.items() if r["tested"] > 0}

    def badness(item):
        r = item[1]
        return (r["landed"] / r["tested"], -r["size"])

    for path, r in sorted(tested.items(), key=badness):
        name = path.rsplit("/", 1)[-1].split(".")[0]
        walk = "%d/%d" % (r["blocked"], r["walk_tested"]) if r["walk_tested"] else "-"
        print("COLLHERE %-40s size %5.1f m  landed %d/%d  walk-blocked %-5s profile %s %s  trace %s  owner %s" % (
            name, r["size"] / 100.0, r["landed"], r["tested"], walk, ",".join(sorted(r["profiles"])), ",".join(sorted(r["enabled"])), r["flag"],
            ",".join(sorted(r["owners"]))[:40]))
    print("COLLHERE summary: %d meshes larger than %.1f m within %.0f m of %s" % (len(tested), MIN_SIZE_CM / 100.0, radius_m, origin))
