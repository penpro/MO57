"""Import the generated moodle glyphs (Tools/gen_moodle_icons.py -> Saved/MoodleIcons) into /MOFramework/UI/Icons/Moodles as UI textures.

    python Tools/gen_moodle_icons.py
    python Tools/ue.py py --file Content/Python/import_moodle_icons.py        (editor open)

The textures are UI-group, no mipmaps, no streaming (a 128 px glyph tinted by severity), sRGB. The folder is in DirectoriesToAlwaysCook (UMOCharacterUIController loads them by path).
"""
import glob
import os

import unreal

SRC = r"D:\UEProjects\MO57\Saved\MoodleIcons"
DST = "/MOFramework/UI/Icons/Moodles"

tasks = []
for f in sorted(glob.glob(os.path.join(SRC, "T_Moodle_*.png"))):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f)
    t.set_editor_property("destination_path", DST)
    t.set_editor_property("destination_name", os.path.splitext(os.path.basename(f))[0])
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", False)
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

n = 0
for t in tasks:
    for path in t.get_editor_property("imported_object_paths"):
        tex = unreal.load_asset(path)
        if not tex:
            continue
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("srgb", True)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
        n += 1
unreal.log("MOODLEICONS imported %d textures into %s" % (n, DST))
