"""Import the "Field Ledger" UI kit (DesignKit/MO57_UI_Kit) into the plugin content:

    python Tools/ue.py py --file Content/Python/kit_import.py        (editor open, not in PIE)

  * 9-slice masks  nineslice/2x/T_UI_*@2x.png  ->  /MOFramework/UI/Kit/NineSlice/T_UI_*   (white on transparent; the 2x file is the shipped one: UMG brush margins are 0..1 fractions, so the
    manifest's `umgMargin` is resolution independent)
  * icons          icons/png128/ic_*.png        ->  /MOFramework/UI/Kit/Icons/T_ic_*      (white on transparent, tinted at runtime from tokens)
  * tokens         tokens_datatable.csv + nineslice.<piece>.margin / .draw rows made from nineslice/manifest.json  ->  /MOFramework/UI/Kit/DT_UITokens
                   (row struct FMOUITokenRow; Project Settings > MO UI Theme points at it)

Idempotent (replace_existing). UI textures: UI lod group, no mips, no streaming, uncompressed-ish (TC_EditorIcon) so thin strokes stay crisp.
"""
import glob
import os

import unreal

KIT = r"D:\UEProjects\MO57\DesignKit\MO57_UI_Kit\MO57_UI_Kit"
DST = "/MOFramework/UI/Kit"


def import_textures(files, dest, rename):
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", f)
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("destination_name", rename(os.path.splitext(os.path.basename(f))[0]))
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
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            unreal.EditorAssetLibrary.save_loaded_asset(tex)
            n += 1
    return n


n1 = import_textures(sorted(glob.glob(os.path.join(KIT, "nineslice", "2x", "T_UI_*@2x.png"))), DST + "/NineSlice", lambda s: s.replace("@2x", ""))
n2 = import_textures(sorted(glob.glob(os.path.join(KIT, "icons", "png128", "ic_*.png"))), DST + "/Icons", lambda s: "T_" + s)
# baked composites (Tools/kit_bake.py): one texture per button state, still valid 9-slices
n3 = import_textures(sorted(glob.glob(r"D:\UEProjects\MO57\Saved\KitBake\Buttons\T_Btn_*.png")), DST + "/Buttons", lambda s: s)

# tokens: create the table once, refill every run (the CSV is the source of truth)
dt_path = DST + "/DT_UITokens"
dt = unreal.load_asset(dt_path) if unreal.EditorAssetLibrary.does_asset_exist(dt_path) else None
if not dt:
    fac = unreal.DataTableFactory()
    fac.set_editor_property("struct", unreal.MOUITokenRow.static_struct())
    dt = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DT_UITokens", DST, unreal.DataTable, fac)
# the 9-slice margins (UMG 0..1 fractions) and draw modes ride along as tokens, so C++ (UMOUITokens::NineSliceBrush) and the spec generator read the SAME numbers
import csv
import io as _io
import json

manifest = json.load(open(os.path.join(KIT, "nineslice", "manifest.json")))["pieces"]
base = open(os.path.join(KIT, "tokens_datatable.csv"), encoding="utf-8-sig", newline="").read().rstrip("\r\n") + "\n"
buf = _io.StringIO()
w = csv.writer(buf, lineterminator="\n")
for name, piece in manifest.items():
    m = piece["umgMargin"]
    w.writerow(["nineslice.%s.margin" % name, "%s,%s,%s,%s" % (m["Left"], m["Top"], m["Right"], m["Bottom"]), "String", "9-slice margin as UMG 0..1 fractions"])
    w.writerow(["nineslice.%s.draw" % name, piece["drawAs"], "String", "Box | Image | Tile"])
# fonts: `font.face.<Family>.<Weight>` = the source FontFace asset (the weights a family has), `font.asset.<Family>` = the composite Font asset built from them (typefaces W<weight>).
# The maps (and the stand-in note) live in kit_lib.py so the specs and the runtime read the same fonts.
from kit_lib import FACES, FONT_ASSETS
for key, face_path in FACES.items():
    w.writerow(["font.face.%s" % key, face_path, "String", "FontFace asset for family.weight (stand-in until the kit fonts are imported)"])
for family, asset in FONT_ASSETS.items():
    w.writerow(["font.asset.%s" % family, asset, "String", "composite Font asset for the family (typefaces W<weight>)"])
all_csv = r"D:\UEProjects\MO57\Saved\KitBake\tokens_all.csv"
os.makedirs(os.path.dirname(all_csv), exist_ok=True)
with open(all_csv, "w", encoding="utf-8", newline="") as f:
    f.write(base + buf.getvalue())
ok = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(dt, all_csv, unreal.MOUITokenRow.static_struct())
unreal.EditorAssetLibrary.save_loaded_asset(dt)
rows = len(unreal.DataTableFunctionLibrary.get_data_table_row_names(dt))

# composite fonts (Slate resolves fonts through UFont only): one per family from the faces above
families = {}
for key, face_path in FACES.items():
    family, weight = key.rsplit(".", 1)
    families.setdefault(family, []).append((int(weight), face_path))
fonts_made = 0
for family, entries in families.items():
    entries.sort()
    names = [unreal.Name("W%d" % wt) for wt, _ in entries]
    faces = [unreal.load_asset(fp) for _, fp in entries]
    font_asset = unreal.MOEditorContentUtils.make_composite_font(FONT_ASSETS[family].split(".")[0], names, faces)
    if font_asset:
        unreal.EditorAssetLibrary.save_loaded_asset(font_asset)
        fonts_made += 1

unreal.log("KITIMPORT fonts=%d nineslice=%d icons=%d buttons=%d tokens_ok=%s rows=%d" % (fonts_made, n1, n2, n3, ok, rows))
