"""Make the Megascans packs copied from the local Bridge library (Content/Megascans/3D_Assets) render, in this project's material setup.

    python Tools/ue.py py --file Content/Python/fix_megascans_import.py        (editor open, PIE NOT running: saving is blocked in PIE)

Why (2026-10-10): the library's exported materials point at /Game/MSPresets/M_MS_Default_Material, which this project does not have (the packs in Content/Fab use /Game/Materials/FabOverrides/M_MS_Base),
so every imported MI had no parent and no parameters. This reparents each MI onto M_MS_Base, hooks up its own base colour + normal textures, and gives it a flat "rough, non-metal, unoccluded"
ORM texture (the library ships Megascans' packed DpR / DpRF maps, not the ORM layout M_MS_Base reads; for rocks and stones the colour and normal carry the look).
Idempotent. Output lines start with MSFIX.
"""
import os

import unreal

ROOT = "/Game/Megascans/3D_Assets"
PARENT = "/Game/Materials/FabOverrides/M_MS_Base"
DEFAULT_HEIGHT = "/Fab/Textures/Standard/T_DefaultDisplacement"
ORM_PATH = ROOT + "/T_Rock_ORM_Flat"
ORM_PNG = "D:/UEProjects/MO57/Saved/GeneratedTextures/orm_rock_flat.png"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("MSFIX " + msg)


def make_orm():
    if eal.does_asset_exist(ORM_PATH):
        return unreal.load_asset(ORM_PATH)
    # The flat ORM (R = AO 1, G = roughness 0.84, B = metallic 0) is a 16x16 PNG made by the system Python (the editor's has no PIL):
    #   python -c "from PIL import Image; Image.new('RGB',(16,16),(255,215,0)).save('Saved/GeneratedTextures/orm_rock_flat.png')"
    if not os.path.exists(ORM_PNG):
        log("FAILED: %s is missing (see the comment above)" % ORM_PNG)
        return None
    task = unreal.AssetImportTask()
    task.filename = ORM_PNG
    task.destination_path = ROOT
    task.destination_name = "T_Rock_ORM_Flat"
    task.replace_existing = True
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(ORM_PATH)
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    eal.save_loaded_asset(tex)
    log("created " + ORM_PATH)
    return tex


def main():
    parent = unreal.load_asset(PARENT)
    height = unreal.load_asset(DEFAULT_HEIGHT)
    orm = make_orm()
    if not parent or orm is None:
        log("FAILED: parent material %s not found" % PARENT)
        return
    fixed = 0
    for a in eal.list_assets(ROOT, recursive=True):
        mi = unreal.load_asset(a)
        if not mi or mi.get_class().get_name() != "MaterialInstanceConstant":
            continue
        folder = a.rsplit("/", 1)[0]
        d = e = None
        for t in eal.list_assets(folder, recursive=False):
            name = t.rsplit("/", 1)[1].split(".")[0]
            if name.startswith("T_") and name.endswith("_2K_D"):
                d = unreal.load_asset(t)
            elif name.startswith("T_") and name.endswith("_2K_N"):
                e = unreal.load_asset(t)
        if d is None or e is None:
            log("SKIP %s (no D/N texture pair: %s %s)" % (a, d, e))
            continue
        mi.set_editor_property("parent", parent)
        mel.set_material_instance_texture_parameter_value(mi, "BaseColorTexture", d)
        mel.set_material_instance_texture_parameter_value(mi, "NormalTexture", e)
        mel.set_material_instance_texture_parameter_value(mi, "MetallicRoughnessTexture", orm)
        mel.set_material_instance_texture_parameter_value(mi, "HeightTexture", height)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        fixed += 1
        log("fixed %s: base=%s normal=%s" % (mi.get_name(), d.get_name(), e.get_name()))
    log("DONE %d material(s) reparented onto %s" % (fixed, PARENT))


main()
