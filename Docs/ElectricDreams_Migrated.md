# Electric Dreams assets in MO57 (migrated 2026-10-12)

Source: Epic's Electric Dreams Environment sample (`D:\UEProjects\ElectricDreamsEnv`, content licensed for use in Unreal Engine projects). A SELECTED slice was copied into `Content/` with package paths preserved (what the
editor's Migrate does): 582 packages, 6.9 GB source. Nothing was downloaded. Why these: they are the ingredients of the open shore / mud / damp-ground work (`Docs/ElectricDreams_Lessons.md` section 3,
`Docs/Terrain_Generation_Exploration.md` to-do) and of the "evidence of time" detail layer (decals, fog cards).

| Group | Contents | Use |
|---|---|---|
| surfaces (420 MB) | `Megascans/Surfaces/` SwampWater, DriedGrassOnDampSoil, MossyRockyGround, NordicMoss, MossyGrass | lake mud bed, damp bank, moss / heath ground (voxel material rules: recipe `Content/Python/make_sand_material.py`) |
| plants (393 MB) | `Megascans/3D_Plants/` Taro, JungleGinger, Periwinkle, KikuyuGrass, Fern, BostonFern, SilverLadyFern, CloverVarieties, CustomMoss, Lichen, GroundCover, DeadLeaves, YellowArchangel, WhiteWindflower | shore / understory species rows (`waterEdgeSpecies`, `DT_Biomes`); there is no reed / cattail in the sample |
| decals (272 MB) | `Megascans/Decals/` Leakage, TileableLeakage, TileableMossPatches, ScatteredSticksAndTwigs, JungleDebris | dirt / leakage / moss on rocks and cliffs |
| rocks, embankments, roots (2.45 GB) | `Megascans/3D_Assets/` GrassyEmbankment, MossyEmbankment, NordicBeachRock(s), SmoothRockShelf, SmallStonesPack, DryRiverBed, ForestRoots, DryBranches | shore rocks / banks / roots |
| fx (47 MB) | `FX/MistCards` (BP_FogCard), `Effects/` (water drips, falling leaves, puddle material, wind parameter collection) | fog cards in valleys / forests, cave drips |
| shore assemblies (3.0 GB) | `Asmbly/Assemblies/` BPP_SmallEmbankment_01/02, BPP_SmallRiverbedEmbankment_01/02, BPP_FernAssembly_01, BPP_MediumEmbankment_Mud_01, BPP_MediumGround_Mud_01 (+ the cliffs / rocks their layers pull in) | composed bank pieces a scatterer can place along a shore |

Shared support folders came along with the closure: `Content/Custom` (materials), `Content/MSPresets/...` (Megascans master materials; new subfolders only, the existing `MSPresets/MS_Foliage_Material` was not touched),
`Content/Meshes`, `Content/Textures`, `Content/PhysicalMaterials`. No file in MO57 was overwritten (0 conflicts); the engine-side dependencies are plain engine content.

**Not in git.** About 7 GB of third-party binaries: the new folders are in `.gitignore` (same practice as UltraDynamicSky / BlackAlder); `Tools/ede_migrate.py copy` re-creates them from the sample. A fresh clone does not have them, so
nothing tracked may REFERENCE them until a decision is made (either they are committed through LFS on purpose, or every referencing asset stays untracked too). Cooking only includes what is referenced, so unused
migrated assets cost disk, not build size.

**First load is slow, once.** The Nanite meshes have no cached build data on this machine: loading an assembly built ~40 Nanite meshes (10+ minutes, 13 GB of editor memory) and the editor does not answer the bridge meanwhile.
The derived-data cache keeps the result.

**Verified (2026-10-12):** the registry sees every folder; `summon /Game/Asmbly/Assemblies/BPP_SmallEmbankment_01.BPP_SmallEmbankment_01_C` and `..BPP_FernAssembly_01..` in PIE place a rock / mud / root / fern bank next to the
pawn with the Megascans materials rendering (Nanite, Substrate: both projects use it).

**Change the slice:** edit `GROUPS` in `Tools/ede_migrate.py`, `python Tools/ede_migrate.py scan` (headless on the sample, ~2 min), `copy --dry`, `copy`. A group's closure is hard references inside `/Game`.
Size guide (marginal): a medium assembly pulls 0.4-3 GB (it drags cliffs and rocks); a plant pack 50-100 MB; a Megascans surface 35-110 MB; the sample's 8K-textured cliffs are 0.3-1.9 GB each.
