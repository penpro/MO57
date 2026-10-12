"""Stage 1 of removing the baked checker background from the item icons (Tools/icon_fix_process.py is stage 2, icon_fix_import.py stage 3).

    python Tools/ue.py py --file Content/Python/icon_fix_export.py        (editor open)

Exports every texture named *Icon under /MOFramework/Items to Saved/IconFix/orig/<Name>.png and writes Saved/IconFix/list.json (asset path + the texture settings to restore after the import).
Output lines start with ICONFIX.
"""
import json
import os

import unreal

OUT = "D:/UEProjects/MO57/Saved/IconFix/"
PROPS = ("compression_settings", "srgb", "mip_gen_settings", "lod_group", "filter", "address_x", "address_y", "never_stream", "compression_no_alpha", "texture_group")
ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.search_all_assets(True)
items = []
for a in ar.get_assets_by_path("/MOFramework/Items", recursive=True):
    if str(a.asset_name).endswith("Icon") and str(a.asset_class_path.asset_name) == "Texture2D":
        items.append(a)
os.makedirs(OUT + "orig", exist_ok=True)
out = []
for a in sorted(items, key=lambda x: str(x.asset_name)):
    t = a.get_asset()
    name = str(a.asset_name)
    task = unreal.AssetExportTask()
    task.object = t
    task.filename = OUT + "orig/" + name + ".png"
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    ok = unreal.Exporter.run_asset_export_task(task)
    props = {}
    for p in PROPS:
        try:
            v = t.get_editor_property(p)
            props[p] = v.name if hasattr(v, "name") else v
        except Exception:
            pass
    out.append({"name": name, "package": str(a.package_name), "path": str(a.package_path), "props": props, "exported": bool(ok)})
    unreal.log("ICONFIX exported %s ok=%s %s" % (name, ok, props))
with open(OUT + "list.json", "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1)
unreal.log("ICONFIX done: %d icons" % len(out))
