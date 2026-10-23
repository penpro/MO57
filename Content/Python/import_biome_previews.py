"""Import the starting-biome card pictures (Docs/World_Creation.md) as UI textures.

    python Tools/ue.py py --file Content/Python/import_biome_previews.py        (editor open, no instance running)

Reads Saved/Review/<date>/biome_<Biome>_card_after.jpg and coast_card_after.jpg (1280x720 instance shots, Tools/review_shots.py) and writes
/MOFramework/UI/BiomePreviews/T_BiomePreview_<Biome> (and T_BiomePreview_Coast): UI texture group, no mips, sRGB, default compression.
The DT_Biomes PreviewImage rows and the panel's CoastPreviewImage are set separately (ue.py rows set / the UI spec). Lines start BIOMEPREVIEW.
"""
import glob
import os

import unreal

SHOTS = "D:/UEProjects/MO57/Saved/Review/2026-10-22/"
DEST = "/MOFramework/UI/BiomePreviews"
tools = unreal.AssetToolsHelpers.get_asset_tools()

sources = {"Coast": SHOTS + "coast_card_after.jpg"}
for path in glob.glob(SHOTS + "biome_*_card_after.jpg"):
    biome = os.path.basename(path)[len("biome_"):-len("_card_after.jpg")]
    sources[biome] = path.replace("\\", "/")

for biome, path in sorted(sources.items()):
    if not os.path.exists(path):
        unreal.log_warning("BIOMEPREVIEW %s: no shot at %s" % (biome, path))
        continue
    name = "T_BiomePreview_" + biome
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    t = unreal.load_asset(DEST + "/" + name)
    if not t:
        unreal.log_warning("BIOMEPREVIEW %s: import failed" % biome)
        continue
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    t.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    t.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(t)
    unreal.log("BIOMEPREVIEW imported %s (%dx%d)" % (name, t.blueprint_get_size_x(), t.blueprint_get_size_y()))
unreal.log("BIOMEPREVIEW done: %d" % len(sources))
