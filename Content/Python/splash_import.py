"""Import the Penumbra studio mark (rasterised from the brand SVG by Saved/Splash/mark.html + headless Chrome) as the splash texture.

    python Tools/ue.py py --file Content/Python/splash_import.py        (editor open, not in PIE)
"""
import unreal

SRC = r"D:\UEProjects\MO57\Saved\Splash\penumbra_mark.png"
DST = "/MOFramework/UI/Splash"

task = unreal.AssetImportTask()
task.set_editor_property("filename", SRC)
task.set_editor_property("destination_path", DST)
task.set_editor_property("destination_name", "T_Splash_PenumbraMark")
task.set_editor_property("replace_existing", True)
task.set_editor_property("automated", True)
task.set_editor_property("save", False)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
for path in task.get_editor_property("imported_object_paths"):
    tex = unreal.load_asset(path)
    if tex:
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("srgb", True)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
        unreal.log("SPLASHIMPORT %s %sx%s" % (path, tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))
