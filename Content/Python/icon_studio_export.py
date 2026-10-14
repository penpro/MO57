"""Icon studio, step 1 of 3 (editor): export each item's world mesh + the textures its materials use, for the Blender render.

    Driven by Tools/icon_studio.py (python Tools/icon_studio.py --missing). By hand:
    python Tools/ue.py py -c "import icon_studio_export as e; out(e.run({'Mushroom01': '/Game/...'}, 'D:/UEProjects/MO57/Saved/IconStudio'))"

Per item, into <out>/<ItemId>/: mesh.fbx (LOD0, no collision) + slot<N>_<base|normal|orm|mask>.png + meta.json:
  slots[i] = {slot, name, kind: opaque|foliage, base, normal, orm, mask, normal_opengl}
Texture parameter names are the ones every project master shares (FabOverrides M_MS_Base / _Fuzz / _Foliage and the glTF
masters): BaseColorTexture, NormalTexture, MetallicRoughnessTexture (Megascans ORM: R=AO, G=roughness, B=metal), Mask (foliage
ORT: R=opacity, G=roughness). normal_opengl records the texture's flip-green flag: glTF sources are OpenGL (the import flips
green at runtime), Megascans are DirectX, so Blender flips green only when it is False.
"""
import json
import os

import unreal

mel = unreal.MaterialEditingLibrary
PARAMS = (("BaseColorTexture", "base"), ("NormalTexture", "normal"), ("MetallicRoughnessTexture", "orm"), ("Mask", "mask"))


def _export(obj, filename, exporter, options=None):
    task = unreal.AssetExportTask()
    task.object = obj
    task.filename = filename
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = exporter
    if options is not None:
        task.options = options
    return unreal.Exporter.run_asset_export_task(task)


def _master(material):
    while isinstance(material, unreal.MaterialInstance):
        material = material.get_editor_property("parent")
    return material


def export_item(item_id, mesh_path, out_root):
    mesh = unreal.load_asset(mesh_path)
    if not isinstance(mesh, unreal.StaticMesh):
        return "SKIP %s: %s is not a static mesh" % (item_id, mesh_path)
    folder = os.path.join(out_root, item_id).replace("\\", "/")
    os.makedirs(folder, exist_ok=True)
    options = unreal.FbxExportOption()
    options.collision = False
    options.level_of_detail = False
    if not _export(mesh, folder + "/mesh.fbx", unreal.StaticMeshExporterFBX(), options):
        return "FAIL %s: mesh export" % item_id
    slots = []
    for index, slot in enumerate(mesh.static_materials):
        material = slot.material_interface
        master = _master(material)
        info = {"slot": index, "name": str(slot.material_slot_name),
                "kind": "foliage" if master and "Foliage" in master.get_name() else "opaque"}
        if isinstance(material, unreal.MaterialInstance):
            for param, key in PARAMS:
                tex = mel.get_material_instance_texture_parameter_value(material, param)
                if not tex or tex.get_path_name().startswith(("/Engine/", "/InterchangeAssets/")) or "/Textures/Standard/T_Default" in tex.get_path_name():
                    continue
                filename = "%s/slot%d_%s.png" % (folder, index, key)
                if _export(tex, filename, unreal.TextureExporterPNG()):
                    info[key] = os.path.basename(filename)
                    if key == "normal":
                        info["normal_opengl"] = bool(tex.get_editor_property("flip_green_channel"))
        slots.append(info)
    with open(folder + "/meta.json", "w") as f:
        json.dump({"item": item_id, "mesh": mesh_path, "slots": slots}, f, indent=1)
    return "OK %s: %d slot(s), %s" % (item_id, len(slots), ", ".join(sorted({k for s in slots for k in s if k in ("base", "normal", "orm", "mask")})) or "no textures")


def run(items, out_root):
    """items: {ItemId: static mesh path}. Returns one report line per item."""
    return "\n".join(export_item(item_id, path, out_root) for item_id, path in items.items())
