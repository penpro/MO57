"""Stage 0 of importing Electric Dreams ground scans as voxel terrain materials: export their (virtual) textures as PNG so Tools/ed_surfaces_prep.py can unpack them.

    python Tools/ue.py py --file Content/Python/ed_surfaces_export.py        (editor open, PIE NOT running; the textures must already be migrated into Content/)

Writes D:/UEProjects/MO57/Saved/EDSurf/<Name>_<BC|N|AoRDp|DpRF>.png. Surfaces come as <Name>_BC (base colour) + <Name>_N + <Name>_AoRDp (R = AO, G = roughness, B = height);
the ground-scan "3D assets" (ForestGround) ship <Name>_01_BC + _DpRF (R = displacement, G = roughness, B = fuzz) + _N. Output lines start with EDEXPORT.
"""
import os

import unreal

OUT = "D:/UEProjects/MO57/Saved/EDSurf/"
M = "/Game/Megascans/"
# (texture asset path, output file stem)
JOBS = [
    (M + "3D_Assets/ForestGround/T_ForestGround_01_BC", "ForestGround_BC"),
    (M + "3D_Assets/ForestGround/T_ForestGround_01_DpRF", "ForestGround_DpRF"),
    (M + "3D_Assets/ForestGround/T_ForestGround_01_N", "ForestGround_N"),
]

os.makedirs(OUT, exist_ok=True)
for asset_path, stem in JOBS:
    tex = unreal.load_asset(asset_path)
    if not tex:
        unreal.log_warning("EDEXPORT missing %s" % asset_path)
        continue
    task = unreal.AssetExportTask()
    task.object = tex
    task.filename = OUT + stem + ".png"
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log("EDEXPORT %s -> %s (%s)" % (asset_path.split("/")[-1], stem, "ok" if ok else "FAILED"))
unreal.log("EDEXPORT done")
