# Dark Ruins Megascans assets in MO57 (Oct 24 2026)

Source: the Dark Ruins Megascans sample (`D:\UEProjects\DarkRuinsMegascansSample`, UE 5.8, Quixel Megascans content licensed for use in Unreal
Engine projects). Wes, Oct 24 2026: "there's a bunch of very large rocky assets in the dark ruins megascans project ... the big western cliffs are
ENORMOUS and could help really get some good looking cliffs without a shit ton of cpu calls, i migrated the western cliffs into the project but you
can probably import a lot of the assets for more than just the world building as well". Nothing was downloaded.

## What is in MO57

| Group (`Tools/ede_migrate.py` `DARKRUINS_GROUPS`) | Mesh | Size | Openness | Use |
|---|---|---|---|---|
| Huge Nordic Coastal Cliff | `/Game/MS/3D/Huge_Nordic_Coastal_Cliff_venrdcgga/Smart/SM_venrdcgga_RM_N1` | 14 x 8 x 9 m | 0.25, open only underneath | a fifth formation of the highland crags (x0.8-1.6) and clusters on NordicStrand's coast (`Tools/highland_crags_patch.py` step 4; `Docs/PCG.md`) |
| Desert Western Cliff Sheer XL 04 (Wes's migration) | `.../Desert_Western_Cliff_Sheer_Xl_04_xgknben/Smart/SM_..._RM_N1` | 218 x 103 x 81 m | 0.39: open at the back AND underneath | a rare landmark cliff face on steep alpine / highland ground, its back and base buried (below) |
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

## The sample's own assemblies (Oct 25, for cliff compositions)

The sample ships artist-built packed-level assemblies (`/Game/PLBPs/*.umap`). `Tools/darkruins_assembly_dump.py` lists them (run headless in
the sample; `Saved/DarkRuinsDump/assemblies.json`); the ones that build cliffs:

| Assembly | What | Size (pivots) |
|---|---|---|
| Mountain_01 | 9 Huge / Massive Nordic Coastal Cliff pieces (6 meshes) stacked into one mountain face | 57 x 32 x 64 m |
| TallPath_01 | massive Nordic cliffs + a rock arch | 7 x 15 x 52 m |
| BoulderAssembly_01, Buildup_Rocky_01, ShelfAssembly_01 | boulder / rocky-buildup / shelf clusters (beach rocks, Icelandic rock assembly, limestone slab) | 3-6 m |

`Tools/darkruins_assembly_export.py` (headless in the sample, enabling Python and PCG for the run) turns each into a PCG data asset
(`/Game/MO57Export/PCGAssemblies/PL_<name>_PCG`, written into the SAMPLE's own new folder) with the engine's `PCGLevelToAsset`: the form the
Electric Dreams compositions load (a PCGAsset element). `ede_migrate.py --source darkruins` group "assemblies" brings them and every mesh they
name (soft references): 198 files, 3.25 GB, Oct 25. The ground assemblies (01-09) are mostly shallow rocky-ground plates and sand / mud
skins: not exported.

## The western cliff faces (placed Oct 25)

218 m wide and 81 m tall, open behind and below: it stands only where the slope buries its back. `Tools/highland_crags_patch.py` step 5 makes it
a landmark species of AlpineRock (0.012 / ha) and RockyHighland (0.008 / ha) on 32-65 deg ground, x0.4-0.7 (87-153 m wide, 32-57 m tall),
upright with its face (+Y, `FaceYawDeg` 90, measured: the area-weighted normal points +Y) turned downhill (`bFaceDownhill`). The pivot is at
its back edge and `FMOSpeciesPlacement::FaceDropCm` reaches to the face edge (bounds origin + extent), so on a 35 deg slope at x0.6 the
spawner sinks it ~40 m: the back and the open base go into the hill and the front stands as the cliff; `SinkFraction` 0.2 buries the rest.
Collision: its real surface (complex as simple), set by `Content/Python/fix_mesh_collision.py`, which now classifies a Megascans 'Smart' scan
by its folder name (`megascans_folder`: `SM_venrdcgga_RM_N1` says nothing, `Huge_Nordic_Coastal_Cliff_venrdcgga` does). The asset is
gitignored, so re-run the fixer after `ede_migrate.py copy --source darkruins` on another machine. PIE seed 777: 5 within 2.8 km, every one
on AlpineRock / RockyHighland ground; `D:/MOAssets/previews/western_cliff_face.png` (the look is Wes's: BACKLOG by eye).

## Candidates for later (measure before use: `Content/Python/open_geometry_probe.py`; Wes's rule: tall and open only underneath, or closed)

| Kind | Sample folders (`/Game/MS/3D/`) | Fits |
|---|---|---|
| Big coast / highland rock | Massive_Nordic_Coastal_Cliff (5: ullqbb1ga, vdssailfa, vefkehbva, veqnci0db, vffjfgeqx), Huge_Nordic_Coastal_Cliff_ulkiejkva, Nordic_Coastal_Cliff (2), Desert_Western_Cliff_Sheer_Xl_05_xgnlfc0, Quarry_Cliff (6), Icelandic_Rock_Assembly_tdbmcflda | crags, NordicStrand, highland cliffs |
| Beach rock | Nordic_Beach_Rock_Formation (10), Nordic_Beach_Rock (~20), Nordic_Beach_Boulder_vcyqebjfa, Beach_Boulder (4), Beach_Rock_wenobjws, Limestone_Rocks_ubordbyda | NordicStrand, ShoreStrand |
| Items (scans of real objects) | Rope_Coil_vcsoddfs, Tangled_Rope_vhnofbds -> Cordage01; Clay_Vase (3) -> CrudePottery01 / StorageJar01; Leather_Water_Pot_wgxobbm -> WaterSkin01; Animal_Leg_Bone_vebhfbraa -> Bone01; White_Button_Mushroom_pdww2, Scatter_Mushroom_Set_rlms3 -> Mushroom01; Wooden_Oar_vemjce3aw -> Paddle01; Old_Wood_Boards_ricaS, Worn_Wooden_Beam (2) -> Plank01; Sword_uitlbiaga -> BronzeSword01 / IronSword01; Limestone_Rocks -> Limestone01 | the item meshes (`Docs/Item_Art_Pipeline.md`: an item's mesh and icon change together) |
| Ruins and props | Medieval church ruin kit (columns, arch, door, window, slabs, stairs), Modular brick ruins kits (4), Modular_Ruins_Wall, Roman_Column, Roman_Stone_Coffin, Japanese shrine stairs / floor, Stone_Structure_scnv2, Wooden_Barrel, Old_Wooden_Crate, Old_Wooden_Door, Wooden_Boat, Wooden_Table, Pile_Of_Chains, Skull_wk5iect, Old_Roman_Coin | POIs (`Docs/World_Features_Architecture.md`); a design call for Wes first: ruins imply a past civilisation (`Docs/VISION.md`) |
