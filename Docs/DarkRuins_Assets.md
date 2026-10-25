# Dark Ruins Megascans assets in MO57 (Oct 24 2026)

Source: the Dark Ruins Megascans sample (`D:\UEProjects\DarkRuinsMegascansSample`, UE 5.8, Quixel Megascans content licensed for use in Unreal
Engine projects). Wes, Oct 24 2026: "there's a bunch of very large rocky assets in the dark ruins megascans project ... the big western cliffs are
ENORMOUS and could help really get some good looking cliffs without a shit ton of cpu calls, i migrated the western cliffs into the project but you
can probably import a lot of the assets for more than just the world building as well". Nothing was downloaded.

## What is in MO57

| Group (`Tools/ede_migrate.py` `DARKRUINS_GROUPS`) | Mesh | Size | Openness | Use |
|---|---|---|---|---|
| Huge Nordic Coastal Cliff | `/Game/MS/3D/Huge_Nordic_Coastal_Cliff_venrdcgga/Smart/SM_venrdcgga_RM_N1` | 14 x 8 x 9 m | 0.25, open only underneath | a fifth formation of the highland crags (x0.8-1.6) and clusters on NordicStrand's coast (`Tools/highland_crags_patch.py` step 4; `Docs/PCG.md`) |
| Desert Western Cliff Sheer XL 04 (Wes's migration) | `.../Desert_Western_Cliff_Sheer_Xl_04_xgknben/Smart/SM_..._RM_N1` | 218 x 103 x 81 m | 0.39: open at the back AND underneath | not placed yet: a cliff face that has to stand against a slope with its back buried (see below) |
| (same folder) | `SM_..._RM1_N1` | 217 x 26 x 81 m | 0.57: a backless wall | avoid (Wes: shallow / open ones do not sit in the terrain) |

Every scan is Nanite (5-16k LOD0 triangles shown, the detail is in the Nanite data) on the sample's Smart master material
(`/Game/Smart/Materials/MasterMaterials/Assets/M_Smart_Master_02`, flagged for Nanite and instanced meshes), which pulls detail textures from
`/Game/Textures/Detail` and functions from `/Game/Materials/Custom`. The Nordic cliff's closure is 44 packages, ~340 MB; its collision is 4
convex hulls (the western cliff's is ONE hull round 218 m: useless as is).

Also on disk, unused: `/Game/MS/3D/Cracked_Nordic_Beach_Cliff_ulypfbkqa` (3 x 6 x 1 m, openness 0.49: a shallow plate, avoided) with
`/Game/Materials/Custom/M_MS_CustomGeneral` and `/Game/MSPresets/MSVTTextures`: delete them on disk with the editor CLOSED (never from editor Python).

**Not in git**, like the Electric Dreams slice (`Docs/ElectricDreams_Migrated.md`): `Content/MS/`, `Content/Smart/`, `Content/Materials/Custom/` are in
`.gitignore` (`Content/Textures/` and `Content/MSPresets/MSVTTextures/` already were). DT_Biomes references them, so a machine without them plants
nothing for those species. Re-create them with:

    python Tools/ede_migrate.py scan --source darkruins      (headless on the sample with the Python plugin enabled for that run; writes Saved/darkruins_selection.json)
    python Tools/ede_migrate.py copy --source darkruins --dry
    python Tools/ede_migrate.py copy --source darkruins

A machine without the sample (DARKTOWER) copies the three folders plus `Content/Textures/Detail` from DARKSIDE's share instead. On DARKSIDE the copy
reports 46 "conflicts": the western cliff's closure and the shared Smart / Custom / Detail files came through the editor's Migrate, which re-saves
them, so they differ byte-wise from the sample's; the copy leaves them alone (the Nordic cliff's own files are identical).

## The western cliff (not placed yet)

218 m wide and 81 m tall, open behind and below: it can only stand where the slope buries its back, facing downhill (the BR18-14 rule for open-backed
slabs, `bFaceDownhill`), and it needs real collision first (complex-as-simple, `Content/Python/fix_mesh_collision.py`). One instance replaces
dozens of formation meshes at a distance. Wes dragged one into a level on Oct 24; the prototype as a landmark species is the next step.

## Candidates for later (measure before use: `Content/Python/open_geometry_probe.py`; Wes's rule: tall and open only underneath, or closed)

| Kind | Sample folders (`/Game/MS/3D/`) | Fits |
|---|---|---|
| Big coast / highland rock | Massive_Nordic_Coastal_Cliff (5: ullqbb1ga, vdssailfa, vefkehbva, veqnci0db, vffjfgeqx), Huge_Nordic_Coastal_Cliff_ulkiejkva, Nordic_Coastal_Cliff (2), Desert_Western_Cliff_Sheer_Xl_05_xgnlfc0, Quarry_Cliff (6), Icelandic_Rock_Assembly_tdbmcflda | crags, NordicStrand, highland cliffs |
| Beach rock | Nordic_Beach_Rock_Formation (10), Nordic_Beach_Rock (~20), Nordic_Beach_Boulder_vcyqebjfa, Beach_Boulder (4), Beach_Rock_wenobjws, Limestone_Rocks_ubordbyda | NordicStrand, ShoreStrand |
| Items (scans of real objects) | Rope_Coil_vcsoddfs, Tangled_Rope_vhnofbds -> Cordage01; Clay_Vase (3) -> CrudePottery01 / StorageJar01; Leather_Water_Pot_wgxobbm -> WaterSkin01; Animal_Leg_Bone_vebhfbraa -> Bone01; White_Button_Mushroom_pdww2, Scatter_Mushroom_Set_rlms3 -> Mushroom01; Wooden_Oar_vemjce3aw -> Paddle01; Old_Wood_Boards_ricaS, Worn_Wooden_Beam (2) -> Plank01; Sword_uitlbiaga -> BronzeSword01 / IronSword01; Limestone_Rocks -> Limestone01 | the item meshes (`Docs/Item_Art_Pipeline.md`: an item's mesh and icon change together) |
| Ruins and props | Medieval church ruin kit (columns, arch, door, window, slabs, stairs), Modular brick ruins kits (4), Modular_Ruins_Wall, Roman_Column, Roman_Stone_Coffin, Japanese shrine stairs / floor, Stone_Structure_scnv2, Wooden_Barrel, Old_Wooden_Crate, Old_Wooden_Door, Wooden_Boat, Wooden_Table, Pile_Of_Chains, Skull_wk5iect, Old_Roman_Coin | POIs (`Docs/World_Features_Architecture.md`); a design call for Wes first: ruins imply a past civilisation (`Docs/VISION.md`) |
