"""List every PCG node of the project's scatter graphs that places meshes, with the collision it gives them (COLLGRAPH lines).

    python Tools/ue.py py --file Content/Python/audit_pcg_collision.py
Covers the native Static Mesh Spawner (mesh entries -> descriptor body instance) and the MO spawners (CollisionProfile property).
"""
import unreal

GRAPHS = [
    "/Game/Penumbra/PCG_StampScatter",
    "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1",
    "/Game/VoxelExamples/PCGScattering/MOPCG_FarTrees",
    "/Game/VoxelExamples/PCGScattering/PCG_Test_2",
    "/Game/Penumbra/Maps/PCG_Test",
    "/MOFramework/PCG_POIS/PCG_FreshWaterSpring",
]


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def describe_entry(entry):
    d = prop(entry, "descriptor")
    mesh = prop(d, "static_mesh") if d else None
    body = prop(d, "body_instance") if d else None
    profile = body.get_editor_property("collision_profile_name") if body else None
    return (mesh.get_path_name() if mesh else None), str(profile)


for gp in GRAPHS:
    g = unreal.load_asset(gp)
    if g is None:
        print("COLLGRAPH %s: not found" % gp)
        continue
    for node in g.get_editor_property("nodes"):
        s = node.get_settings()
        if s is None:
            continue
        cls = s.get_class().get_name()
        title = str(node.get_editor_property("node_title")) if prop(node, "node_title") else ""
        sel = prop(s, "mesh_selector_parameters")
        if sel is not None:
            entries = prop(sel, "mesh_entries", []) or []
            for e in entries:
                mesh, profile = describe_entry(e)
                print("COLLGRAPH %s | %s | %s | mesh=%s | profile=%s" % (gp.rsplit("/", 1)[-1], node.get_name(), cls, mesh, profile))
            if not entries:
                print("COLLGRAPH %s | %s | %s | (mesh selector with no entries; class %s)" % (gp.rsplit("/", 1)[-1], node.get_name(), cls, sel.get_class().get_name()))
        elif cls.startswith("MOPCG"):
            prof = prop(s, "collision_profile")
            print("COLLGRAPH %s | %s | %s | collision_profile=%s" % (gp.rsplit("/", 1)[-1], node.get_name(), cls, prof.get_editor_property("name") if prof else None))
