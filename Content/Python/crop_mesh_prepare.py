"""Make an imported TRELLIS crop plant a foliage mesh: the Fab foliage master, its mask texture, Nanite, no collision.

    python Tools/ue.py py -c "import crop_mesh_prepare as c; out(c.prepare('/Game/ImageTo3D/Crops/Barley/Ripe', r'D:/.../Barley_Ripe_ORT.png'))"
    (editor open, PIE NOT running, and no -game instance holding the package: a running `inst` locks the .uasset files it loaded
     and the save silently fails -- TRAPS "Asset saves silently don't happen")

Why (Nov 3 2026, DECISIONS "Crop plants are made with TRELLIS"): the crop plants share the world grass's master
(/Game/Materials/FabOverrides/M_MS_Foliage, the master of the Dry / Wild Grass the field plot used as stand-ins), so they move with
the small plants' wind (Content/Python/plant_flicker_wind.py) and pass light through their leaves (two-sided foliage shading).
Tools/crop_finish_blender.py wrote the ORT mask (opacity / roughness / translucency) this imports. Per call:
  - imports <ort_png> next to the mesh's textures as T_<mesh>_ORT (Masks compression, sRGB off);
  - creates MI_<mesh> on M_MS_Foliage with the stand-in grass's settings (MI_tbbqejqr's scalars, vectors and static switches),
    the mesh's own base colour and normal, and the ORT mask; "mask out Twigs" off (the twig mask keys on vertex colours this mesh
    does not paint for it, and would cut holes) and a short camera fade (15-60 cm, not the grass's 40-160 cm that speckled heads 1-2 m away);
  - Nanite on, collision off (crops are walked through), the slot -> the MI; saves what it touched.
Output lines start with CROPPREP. Idempotent.
"""
import os

import unreal

# The world grass's master; its wind is every small plant's (Content/Python/plant_flicker_wind.py: flickers here and there, sized by the mesh).
PARENT = "/Game/Materials/FabOverrides/M_MS_Foliage"
LIKE = "/Game/Fab/Megascans/Plants/Dry_Grass_tbbqejqr/Medium/tbbqejqr_tier_2/Materials/MI_tbbqejqr"
SWITCH_OVERRIDES = {"mask out Twigs": False}
# The grass's camera fade (dither from 40 to 160 cm) speckled crop heads 1-2 m from the camera (Nov 3 A/B): a crop keeps a short one,
# so a camera inside a standing field still sees through the stalks right in front of it.
SCALAR_OVERRIDES = {"CameraFadeStartCm": 15.0, "CameraFadeEndCm": 60.0}

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
_LINES = []


def log(msg):
    unreal.log("CROPPREP " + msg)
    _LINES.append(msg)


def _assets(folder, cls):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    return [d.get_asset() for d in reg.get_assets_by_path(folder, recursive=True) if str(d.asset_class_path.asset_name) == cls]


def _import_mask(png, dest, name):
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset("%s/%s" % (dest, name))
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    return tex


def _copy_like(mi):
    like = unreal.load_asset(LIKE)
    for p in mel.get_scalar_parameter_names(like):
        mel.set_material_instance_scalar_parameter_value(mi, p, mel.get_material_instance_scalar_parameter_value(like, p))
    for p in mel.get_vector_parameter_names(like):
        mel.set_material_instance_vector_parameter_value(mi, p, mel.get_material_instance_vector_parameter_value(like, p))
    for p in mel.get_static_switch_parameter_names(like):
        mel.set_material_instance_static_switch_parameter_value(mi, p, mel.get_material_instance_static_switch_parameter_value(like, p))
    for p, v in SWITCH_OVERRIDES.items():
        mel.set_material_instance_static_switch_parameter_value(mi, p, v)
    for p, v in SCALAR_OVERRIDES.items():
        mel.set_material_instance_scalar_parameter_value(mi, p, v)
    # The light through the leaves stays under the reflected light (fix_foliage_transmission.py: G = strength <= 0.5, B = exponent >= 1;
    # test MOFramework.Rendering.FoliageTransmissionCapped). The copy above writes the grass's EFFECTIVE values as overrides, strength 1.0
    # among them: the test caught it on Nov 4.
    for p in mel.get_vector_parameter_names(mi):
        if str(p).startswith("Translucency Controls"):
            c = mel.get_material_instance_vector_parameter_value(mi, p)
            mel.set_material_instance_vector_parameter_value(mi, p, unreal.LinearColor(c.r, min(c.g, 0.5), max(c.b, 1.0), c.a))


def prepare(folder, ort_png):
    """Returns the report as one string (the bridge's out() prints it)."""
    del _LINES[:]
    meshes = _assets(folder, "StaticMesh")
    if len(meshes) != 1:
        log("SKIP %s: %d static meshes (want 1)" % (folder, len(meshes)))
        return "\n".join(_LINES)
    mesh = meshes[0]
    textures = {t.get_name(): t for t in _assets(folder, "Texture2D")}
    base = next((t for n, t in textures.items() if n.endswith("_BaseColor")), None)
    normal = next((t for n, t in textures.items() if n.endswith("_Normal")), None)
    tex_dir = (base.get_path_name() if base else mesh.get_path_name()).rsplit("/", 1)[0]
    mask = _import_mask(os.path.abspath(ort_png).replace("\\", "/"), tex_dir, "T_%s_ORT" % mesh.get_name())

    mat_dir = mesh.get_path_name().rsplit("/", 2)[0] + "/Materials"
    mi_path = "%s/MI_%s" % (mat_dir, mesh.get_name())
    mi = unreal.load_asset(mi_path) if eal.does_asset_exist(mi_path) else None
    if not mi:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset("MI_%s" % mesh.get_name(), mat_dir, unreal.MaterialInstanceConstant,
                                                                     unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(PARENT))
    _copy_like(mi)
    if base:
        mel.set_material_instance_texture_parameter_value(mi, "BaseColorTexture", base)
    if normal:
        mel.set_material_instance_texture_parameter_value(mi, "NormalTexture", normal)
    mel.set_material_instance_texture_parameter_value(mi, "Mask", mask)
    mel.update_material_instance(mi)

    sm = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    nanite = mesh.get_editor_property("nanite_settings")
    if not nanite.enabled:
        nanite.enabled = True
        sm.set_nanite_settings(mesh, nanite, apply_changes=True)
    sm.remove_collisions(mesh)
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    for i in range(len(mesh.static_materials)):
        mesh.set_material(i, mi)
    saved = [eal.save_asset(a.get_path_name(), only_if_is_dirty=False) for a in (mask, mi, mesh)]
    b = mesh.get_bounding_box()
    e = b.max - b.min
    log("%s: MI %s on %s (base %s, normal %s, mask %s); nanite on, no collision; size %.1f x %.1f x %.1f cm; saved %s"
        % (mesh.get_name(), mi.get_name(), PARENT.rsplit("/", 1)[-1], base.get_name() if base else None,
           normal.get_name() if normal else None, mask.get_name(), e.x, e.y, e.z, saved))
    return "\n".join(_LINES)
