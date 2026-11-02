"""Bring Tools/blender_items_blender.py's output into the project (Nov 2 2026). Editor open, PIE not running. Driven by Tools/blender_items.py:

    python Tools/ue.py py -c "import import_blender_items as b; out(b.run('SimpleBow01'))" --timeout 600

From Saved/BlenderItems/<ItemId>/ into /Game/BlenderItems/<ItemId>/:
  T_<ItemId>_BaseColor / _Normal / _ORM   the baked textures (the normal map is DirectX already; the ORM is linear: R occlusion, G roughness,
                                          B metallic, the glTF order M_MS_Base reads)
  MI_<ItemId>                             on /Game/Materials/FabOverrides/M_MS_Base with the three textures and its material AO on
  SM_<ItemId>                             the static mesh (a bow braced): the world item and the icon; item_mesh_prepare gives it Nanite and a hull
  SK_<ItemId>, its skeleton and its draw  (bows) the rigged bow; the draw clip keeps whatever name the importer gives it (assets are never
                                          renamed from editor Python), reported back
Returns JSON: what was made, sizes, bones, the draw clip, and (bows) the nocking point's travel in the clip (cm, mesh space).

Why the legacy FBX importer: like kitbash.py, it honours FbxImportUI (no materials or textures from the file), so the import makes exactly the
assets above. M_MS_Base gets "used with skeletal meshes" once (a held bow is skinned).
"""
import json
import os

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
MASTER = "/Game/Materials/FabOverrides/M_MS_Base"
CVARS = {"Interchange.FeatureFlags.Import.FBX": "0", "Interchange.FeatureFlags.Import.SyncToBrowser": "0"}
SRC = os.path.join(unreal.Paths.project_saved_dir(), "BlenderItems")


def _import(tasks):
    previous = {k: unreal.SystemLibrary.get_console_variable_string_value(k) for k in CVARS}
    for k, v in CVARS.items():
        unreal.SystemLibrary.execute_console_command(None, "%s %s" % (k, v))
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    finally:
        for k, v in previous.items():
            unreal.SystemLibrary.execute_console_command(None, "%s %s" % (k, v or "1"))


def _task(filename, folder, name, options=None):
    t = unreal.AssetImportTask()
    t.filename = filename
    t.destination_path = folder
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = False
    if options is not None:
        t.options = options
    return t


def _textures(item, folder, files):
    tasks = [_task(os.path.join(SRC, item, "T_%s_%s.png" % (item, k)), folder, "T_%s_%s" % (item, k)) for k in ("BaseColor", "Normal", "ORM")]
    _import(tasks)
    tex = {k: unreal.load_asset("%s/T_%s_%s" % (folder, item, k)) for k in ("BaseColor", "Normal", "ORM")}
    tex["Normal"].set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    tex["Normal"].set_editor_property("srgb", False)
    tex["Normal"].set_editor_property("flip_green_channel", False)   # baked DirectX (green down) already
    tex["ORM"].set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    tex["ORM"].set_editor_property("srgb", False)
    for t in tex.values():
        eal.save_loaded_asset(t, False)
    return tex


def _material(item, folder, tex):
    master = unreal.load_asset(MASTER)
    if not master.get_editor_property("used_with_skeletal_mesh"):
        mel.set_material_usage(master, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
        eal.save_loaded_asset(master, False)
    path = "%s/MI_%s" % (folder, item)
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "MI_" + item, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    mel.set_material_instance_texture_parameter_value(mi, "BaseColorTexture", tex["BaseColor"])
    mel.set_material_instance_texture_parameter_value(mi, "NormalTexture", tex["Normal"])
    mel.set_material_instance_texture_parameter_value(mi, "MetallicRoughnessTexture", tex["ORM"])
    mel.set_material_instance_static_switch_parameter_value(mi, "Enable Material AO", True)
    mel.set_material_instance_scalar_parameter_value(mi, "Min Roughness", 0.0)
    mel.set_material_instance_scalar_parameter_value(mi, "Max Roughness", 1.0)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi, False)
    return mi


def _static(item, folder, fbx, mi):
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    ui.static_mesh_import_data.combine_meshes = True
    ui.static_mesh_import_data.auto_generate_collision = False
    ui.static_mesh_import_data.convert_scene_unit = True
    _import([_task(fbx, folder, "SM_" + item, ui)])
    mesh = unreal.load_asset("%s/SM_%s" % (folder, item))
    for i in range(len(mesh.static_materials)):
        mesh.set_material(i, mi)
    eal.save_loaded_asset(mesh, False)
    return mesh


def _skeletal(item, folder, fbx, mi):
    before = set(eal.list_assets(folder, recursive=False))
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = True
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = True
    ui.create_physics_asset = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    ui.skeletal_mesh_import_data.convert_scene_unit = True
    for prop, value in (("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME), ("import_bone_tracks", True),
                        ("remove_redundant_keys", False)):
        try:
            ui.anim_sequence_import_data.set_editor_property(prop, value)
        except Exception as e:   # not every engine exposes all of them to Python
            unreal.log_warning("import_blender_items: anim option %s: %s" % (prop, e))
    _import([_task(fbx, folder, "SK_" + item, ui)])
    sk = unreal.load_asset("%s/SK_%s" % (folder, item))
    # new slot structs (iterating the Array hands out copies: setting their material changed nothing -- the first import's bow was grey)
    slots = [unreal.SkeletalMaterial(material_interface=mi, material_slot_name=m.get_editor_property("material_slot_name"))
             for m in sk.get_editor_property("materials")]
    sk.set_editor_property("materials", slots)
    missing = [str(m.get_editor_property("material_slot_name")) for m in sk.get_editor_property("materials")
               if m.get_editor_property("material_interface") != mi]
    if missing:
        raise RuntimeError("import_blender_items: %s slots %s did not take %s" % (sk.get_name(), missing, mi.get_name()))
    eal.save_loaded_asset(sk, False)
    made = sorted(set(eal.list_assets(folder, recursive=False)) - before)
    for path in eal.list_assets(folder, recursive=False):   # the skeleton and the draw clip too (the import task saves nothing)
        eal.save_asset(path, only_if_is_dirty=False)
    anims = [a for a in eal.list_assets(folder, recursive=False) if isinstance(unreal.load_asset(a), unreal.AnimSequence)]
    return sk, anims, made


def _nock_travel(anim, bone="string_nock"):
    L = anim.get_editor_property("sequence_length")
    pts = []
    for t in (0.0, L):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, t, unreal.AnimPoseEvaluationOptions())
        p = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation
        pts.append([round(p.x, 2), round(p.y, 2), round(p.z, 2)])
    ref = unreal.AnimPoseExtensions.get_reference_pose(anim.get_editor_property("skeleton"))
    b = unreal.AnimPoseExtensions.get_bone_pose(ref, bone, unreal.AnimPoseSpaces.WORLD).translation
    top = unreal.AnimPoseExtensions.get_bone_pose(ref, "string_top", unreal.AnimPoseSpaces.WORLD).translation
    return {"first": pts[0], "last": pts[1], "brace": [round(b.x, 2), round(b.y, 2), round(b.z, 2)],
            "string_top": [round(top.x, 2), round(top.y, 2), round(top.z, 2)], "length_s": round(L, 4),
            "frames": anim.get_editor_property("number_of_sampled_frames")}


def run(item):
    src = os.path.join(SRC, item)
    with open(os.path.join(src, item + ".json")) as f:
        info = json.load(f)
    folder = "/Game/BlenderItems/" + item
    tex = _textures(item, folder, info.get("textures", {}))
    mi = _material(item, folder, tex)
    result = {"item": item, "material": mi.get_path_name()}
    static_fbx = os.path.join(src, item + ("_static.fbx" if info.get("kind") == "bow" else ".fbx"))
    sm = _static(item, folder, static_fbx, mi)
    box = sm.get_bounding_box()
    result["static_mesh"] = sm.get_path_name()
    result["static_size"] = [round(box.max.x - box.min.x, 1), round(box.max.y - box.min.y, 1), round(box.max.z - box.min.z, 1)]
    if info.get("kind") == "bow":
        sk, anims, made = _skeletal(item, folder, os.path.join(src, item + ".fbx"), mi)
        result["skeletal_mesh"] = sk.get_path_name()
        result["made"] = made
        skel = sk.get_editor_property("skeleton")
        ref = unreal.AnimPoseExtensions.get_reference_pose(skel)
        result["bones"] = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(ref)]
        result["anims"] = anims
        if anims:
            draw = unreal.load_asset(anims[0])
            result["draw"] = draw.get_path_name()
            result["nock"] = _nock_travel(draw)
    return json.dumps(result)
