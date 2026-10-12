"""Stage 3 of removing the baked checker background from the item icons: import the processed PNGs (Saved/IconFix/fixed/) over the existing textures and restore their settings.

    python Tools/ue.py py --file Content/Python/icon_fix_import.py        (editor open, no instance running)

Same asset paths, so every item row / widget reference stays valid. The texture settings recorded by icon_fix_export.py (compression, sRGB, mips, LOD group, filter, addressing) are re-applied after
the import. Output lines start with ICONFIX.
"""
import json

import unreal

OUT = "D:/UEProjects/MO57/Saved/IconFix/"
tools = unreal.AssetToolsHelpers.get_asset_tools()
items = json.load(open(OUT + "list.json", encoding="utf-8"))
enum_for = {"compression_settings": unreal.TextureCompressionSettings, "mip_gen_settings": unreal.TextureMipGenSettings, "lod_group": unreal.TextureGroup,
            "filter": unreal.TextureFilter, "address_x": unreal.TextureAddress, "address_y": unreal.TextureAddress, "texture_group": unreal.TextureGroup}
for it in items:
    task = unreal.AssetImportTask()
    task.filename = OUT + "fixed/" + it["name"] + ".png"
    task.destination_path = it["path"]
    task.destination_name = it["name"]
    task.replace_existing = True
    task.replace_existing_settings = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    t = unreal.load_asset(it["package"])
    for k, v in it["props"].items():
        try:
            val = getattr(enum_for[k], v) if k in enum_for else v
            t.set_editor_property(k, val)
        except Exception as ex:
            unreal.log_warning("ICONFIX %s: could not restore %s=%s (%s)" % (it["name"], k, v, ex))
    t.set_editor_property("compression_no_alpha", False)
    unreal.EditorAssetLibrary.save_loaded_asset(t)
    unreal.log("ICONFIX imported %s (%dx%d, %s)" % (it["name"], t.blueprint_get_size_x(), t.blueprint_get_size_y(), t.get_editor_property("compression_settings")))
unreal.log("ICONFIX import done")
