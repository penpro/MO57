"""Ground-cover rendering settings on the scatter graph's NATIVE Static Mesh Spawners (the grass chains and the flower chain): no shadow, no ray-traced / reflected presence, and a distance fade-out
(150 m start, 200 m end: the near tier's 180 m radius plus a margin; the far-grass tier takes over beyond it). Idempotent.

    python Tools/ue.py py --file Content/Python/tune_native_grass.py        (editor open, not in PIE)
"""
import unreal

GRAPH = "/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1"
NODES = ("StaticMeshSpawner_2", "StaticMeshSpawner_4", "StaticMeshSpawner_6")
START_CULL, END_CULL = 15000, 20000

g = unreal.load_asset(GRAPH)
by = {n.get_name(): n for n in g.get_editor_property("nodes")}
for nm in NODES:
    s = by[nm].get_settings()
    msp = s.get_editor_property("mesh_selector_parameters")
    ents = list(msp.get_editor_property("mesh_entries"))
    for e in ents:
        d = e.get_editor_property("descriptor")
        d.set_editor_property("cast_shadow", False)
        d.set_editor_property("visible_in_ray_tracing", False)
        d.set_editor_property("visible_in_reflections", False)
        d.set_editor_property("instance_start_cull_distance", START_CULL)
        d.set_editor_property("instance_end_cull_distance", END_CULL)
        e.set_editor_property("descriptor", d)
    msp.set_editor_property("mesh_entries", ents)
    s.set_editor_property("mesh_selector_parameters", msp)
    unreal.log("NATIVEGRASS %s: %d entries" % (nm, len(ents)))
unreal.EditorAssetLibrary.save_loaded_asset(g)
unreal.log("NATIVEGRASS saved")
