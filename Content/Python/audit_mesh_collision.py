"""Audit the collision of every static mesh component in the running PIE world (what is ACTUALLY spawned around the player), grouped per mesh.

Run (PIE running, after the scatter has generated):
    python Tools/ue.py py --file Content/Python/audit_mesh_collision.py

A mesh is a PROBLEM when its component answers collision (profile blocks / collision enabled) but the mesh asset has no collision geometry to answer with -- the player walks straight
through it (a Nanite rock with the default "no simple collision, complex-as-simple off"). Output lines start with "COLLAUDIT" so the log can be grepped.
"""
import unreal

ESL = unreal.EditorStaticMeshLibrary


def classify(mesh):
    """(simple shape count, convex count, trace flag name) of a static mesh asset."""
    simple = ESL.get_simple_collision_count(mesh)
    convex = ESL.get_convex_collision_count(mesh)
    flag = "?"
    try:
        body = mesh.get_editor_property("body_setup")
        flag = body.get_editor_property("collision_trace_flag").name
    except Exception:
        pass
    return simple, convex, flag


def main():
    world = unreal.EditorLevelLibrary.get_game_world()
    if world is None:
        print("COLLAUDIT no game world (start PIE first)")
        return
    per_mesh = {}
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = comp.get_editor_property("static_mesh")
            if mesh is None:
                continue
            key = mesh.get_path_name()
            if key.startswith("/Engine/Transient") or key.startswith("/Engine/EditorMeshes") or actor.get_class().get_name() == "VoxelWorld":
                continue   # the voxel terrain's own chunk meshes and editor gizmos are not scatter
            entry = per_mesh.setdefault(key, {"mesh": mesh, "comps": 0, "instances": 0, "profiles": set(), "enabled": set(), "owners": set()})
            entry["comps"] += 1
            if isinstance(comp, unreal.InstancedStaticMeshComponent):
                entry["instances"] += comp.get_instance_count()
            else:
                entry["instances"] += 1
            entry["profiles"].add(str(comp.get_collision_profile_name()))
            entry["enabled"].add(comp.get_collision_enabled().name)
            entry["owners"].add(actor.get_class().get_name())

    problems = 0
    for key in sorted(per_mesh):
        e = per_mesh[key]
        simple, convex, flag = classify(e["mesh"])
        enabled = e["enabled"] - {"NO_COLLISION"}
        solid = bool(enabled) and any(p not in ("NoCollision", "OverlapAllDynamic") for p in e["profiles"])
        has_geom = (simple + convex) > 0
        # "USE_COMPLEX_AS_SIMPLE" answers queries with the render triangles, so it counts as collision.
        complex_ok = flag in ("CTF_USE_COMPLEX_AS_SIMPLE", "USE_COMPLEX_AS_SIMPLE")
        problem = solid and not has_geom and not complex_ok
        problems += 1 if problem else 0
        print("COLLAUDIT %s %s inst=%d comps=%d simple=%d convex=%d trace=%s collision=%s profiles=%s owners=%s" % (
            "PROBLEM" if problem else ("ok     " if solid else "nocoll "), key, e["instances"], e["comps"], simple, convex, flag,
            ",".join(sorted(e["enabled"])), ",".join(sorted(e["profiles"])), ",".join(sorted(e["owners"]))))
    print("COLLAUDIT summary: %d distinct meshes, %d with a collision problem" % (len(per_mesh), problems))


main()
