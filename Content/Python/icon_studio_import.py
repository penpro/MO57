"""Icon studio, step 3 of 3 (editor): import rendered icons as UI textures and list the items that need one.

    Driven by Tools/icon_studio.py. Helpers:
      items_needing_icons(include_existing)  -> {ItemId: world mesh path} for items whose WorldVisual is a real mesh (not the
                                                 /Game/Penumbra/Graybox placeholders) and, unless include_existing, no IconSmall
      import_icons(root, item_ids, replace)   -> imports <root>/<Item>/icon.png to /MOFramework/Items/<Item>/<Item>Icon

The texture settings match the hand-made icons (Stick01Icon etc.): TC_EditorIcon (uncompressed RGBA), TEXTUREGROUP_UI, no
mips, sRGB, never streamed. An existing <Item>Icon is only overwritten with replace=True. Saves what it imports.
"""
import json
import os
import re

import unreal

eal = unreal.EditorAssetLibrary
PLACEHOLDER = "/Game/Penumbra/Graybox/"
ITEMS_TABLE = "/MOFramework/Data/DT_Items.DT_Items"


def items_needing_icons(include_existing=False):
    table = unreal.load_asset(ITEMS_TABLE)
    lib = unreal.DataTableFunctionLibrary
    names = [str(n) for n in lib.get_data_table_row_names(table)]
    visuals = lib.get_data_table_column_as_string(table, "WorldVisual")
    uis = lib.get_data_table_column_as_string(table, "UI")
    found = {}
    for name, visual, ui in zip(names, visuals, uis):
        mesh = re.search(r'StaticMesh="([^"]+)"', visual)
        if not mesh or mesh.group(1).startswith(PLACEHOLDER):
            continue
        has_icon = not re.search(r"IconSmall=(None|\"\")", ui)
        if has_icon and not include_existing:
            continue
        found[name] = mesh.group(1).split(".")[0]
    return json.dumps(found)


def _configure(texture):
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)


def import_icons(root, item_ids, replace=False):
    """Returns JSON {ItemId: "/MOFramework/Items/<Id>/<Id>Icon.<Id>Icon"} for every icon now in place."""
    done = {}
    tasks = []
    for item_id in item_ids:
        png = os.path.join(root, item_id, "icon.png")
        folder = "/MOFramework/Items/%s" % item_id
        name = "%sIcon" % item_id
        path = "%s/%s" % (folder, name)
        if not os.path.isfile(png):
            continue
        if eal.does_asset_exist(path) and not replace:
            done[item_id] = "%s.%s" % (path, name)
            continue
        task = unreal.AssetImportTask()
        task.filename = png
        task.destination_path = folder
        task.destination_name = name
        task.replace_existing = True
        task.automated = True
        task.save = False
        tasks.append((item_id, path, name, task))
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t[3] for t in tasks])
    for item_id, path, name, _ in tasks:
        texture = unreal.load_asset(path)
        if not isinstance(texture, unreal.Texture2D):
            continue
        _configure(texture)
        eal.save_loaded_asset(texture, False)
        done[item_id] = "%s.%s" % (path, name)
    return json.dumps(done)
