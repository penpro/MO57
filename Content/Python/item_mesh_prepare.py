"""Make an imported (non-Megascans) static mesh usable as an item's world mesh.

    python Tools/ue.py py -c "import item_mesh_prepare as p; out(p.prepare(['/Game/Fab/Some_Pack/x/StaticMeshes/x']))"
    (editor open, PIE NOT running: mesh builds and saves are refused in PIE)

Why (2026-10-14): Fab's FBX/glTF imports come in with Nanite off and every material parented on an engine-plugin master
(/InterchangeAssets/gltf/.../M_GLTF), which -- like the Fab Megascans masters (see fab_finalize_import.py) -- has no Nanite /
instanced usage flags and cannot be edited in place, so the mesh renders default gray wherever it is drawn by Nanite or as an
instance. Per mesh this:
  - enables Nanite when the mesh has more than NANITE_MIN_TRIS triangles (scans are 100k+);
  - gives it one convex hull if it has no simple collision, and turns off "use complex as simple" (dropped items simulate physics,
    which complex-as-simple cannot: Megascans plants ship that way);
  - for each material slot whose material is an instance of a master OUTSIDE /Game -- or of a Fab plugin master that
    fab_finalize_import.py could not reparent (no FabOverrides twin, e.g. the translucent M_MS_Base_Trm on fruit scans, which
    Nanite cannot draw at all) -- creates MI_<mesh>_MO next to the mesh on
    /Game/Materials/FabOverrides/M_MS_Base with the same base colour / normal / metallic-roughness textures (glTF and Megascans
    share those parameter names). glTF metallic-roughness has no AO in red, so AO Strength is set to 0 for glTF sources.
Saves what it touched. Output lines start with ITEMPREP. Idempotent.
"""
import unreal

PARENT = "/Game/Materials/FabOverrides/M_MS_Base"
HEIGHT = "/Game/Fab/Textures/Standard/T_DefaultDisplacement"
FAB_MASTERS = "/Game/Fab/Materials/"   # the Fab plugin's own masters: no Nanite / instanced usage, see fab_finalize_import.py
NANITE_MIN_TRIS = 4000
TEXTURE_PARAMS = ("BaseColorTexture", "NormalTexture", "MetallicRoughnessTexture")

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


_LINES = []


def log(msg):
    unreal.log("ITEMPREP " + msg)
    _LINES.append(msg)


def _master(material):
    while isinstance(material, unreal.MaterialInstance):
        material = material.get_editor_property("parent")
    return material


def _project_material(mesh, slot_index, material):
    master = _master(material)
    if master is None or (master.get_path_name().startswith("/Game/") and not master.get_path_name().startswith(FAB_MASTERS)):
        return None   # already a project master (FabOverrides, a pack's own material): leave it
    folder = mesh.get_path_name().rsplit("/", 1)[0].replace("/StaticMeshes", "/Materials")
    name = "MI_%s_MO%s" % (mesh.get_name(), "" if slot_index == 0 else "_%d" % slot_index)
    path = folder + "/" + name
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", unreal.load_asset(PARENT))
    for param in TEXTURE_PARAMS:
        tex = mel.get_material_instance_texture_parameter_value(material, param) if isinstance(material, unreal.MaterialInstance) else None
        if tex:
            mel.set_material_instance_texture_parameter_value(mi, param, tex)
    mel.set_material_instance_texture_parameter_value(mi, "HeightTexture", unreal.load_asset(HEIGHT))
    if not master.get_path_name().startswith(FAB_MASTERS):   # glTF: no AO in red. A Megascans ORM has real AO -- keep the parent's.
        mel.set_material_instance_scalar_parameter_value(mi, "AO Strength", 0.0)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def prepare(mesh_paths):
    """Returns the report as one string (the bridge's out() prints it)."""
    del _LINES[:]
    sm = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for path in mesh_paths:
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            log("SKIP %s (not a static mesh)" % path)
            continue
        notes = []
        nanite = mesh.get_editor_property("nanite_settings")
        if not nanite.enabled and mesh.get_num_triangles(0) > NANITE_MIN_TRIS:
            nanite.enabled = True
            sm.set_nanite_settings(mesh, nanite, apply_changes=True)
            notes.append("nanite on (%d tris)" % mesh.get_num_triangles(0))
        body = mesh.get_editor_property("body_setup")
        if body and body.get_editor_property("collision_trace_flag") == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
            # Dropped items simulate physics, which complex-as-simple cannot (PIE warns "Trying to simulate physics ... ComplexAsSimple";
            # Megascans plants ship that way). Back to default: the simple shapes below are the collision.
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
            notes.append("complex-as-simple -> default")
        if max(sm.get_simple_collision_count(mesh), 0) + max(sm.get_convex_collision_count(mesh), 0) == 0:   # -1 means none
            sm.set_convex_decomposition_collisions(mesh, 1, 16, 100000)
            notes.append("convex hull added")
        for i, slot in enumerate(mesh.static_materials):
            mi = _project_material(mesh, i, slot.material_interface)
            if mi:
                mesh.set_material(i, mi)
                notes.append("slot %d -> %s" % (i, mi.get_name()))
        eal.save_loaded_asset(mesh)
        b = mesh.get_bounding_box()
        e = b.max - b.min
        log("%s: %s; size %.1f x %.1f x %.1f cm" % (path.rsplit("/", 1)[-1], ", ".join(notes) or "nothing to do", e.x, e.y, e.z))
    return "\n".join(_LINES)
