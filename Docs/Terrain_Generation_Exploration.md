# Terrain Generation from C++ — Exploration (2026-10-09) + implementation status

> **Covers:** the C++ world generator (`FMOWorldGen`: climate, relief, mountains, coasts, rivers, lakes, biomes, spawn search), how the
> Voxel terrain and PCG read it, the terrain ground material, and the dated build log of every generator version.
> **Current state (2026-10-17):** generator **v11** (`CurrentGeneratorVersion` in `MOWorldGen.h`); the game level `MOPCGScattering` and the
> menu level run on `VHG_MOTerrain` + the `VVG_MOCaves` stamp; world 120 km edge (`WorldHalfSizeCm` 6e6) with an island edge; per-biome relief
> with regional (30 km) mountain ranges, VerticalScale 2, highest ground ~1.4-1.9 km; deep ocean; rivers and sea-level basins; upland lakes
> (Oceanology surfaces); soft biome boundaries (`SampleSurfaceBiomeWeights`, `SurfaceClimateBlend` 0.12); coastal new-game spawn (never an
> island, vegetated ground). Caves and geology: `Docs/Caves_And_Geology.md`. Water: `Docs/Water.md`. Planting: `Docs/PCG.md`.
> **Entry points:** `MOFrameworkCore/Public/MOWorldGen.h` (rules in its header: IEEE-exact ops only, no sin / cos / pow / ISPC; a math change
> bumps `CurrentGeneratorVersion` and re-pins the goldens), `MOWorldGen*.cpp`, `UMOWorldGenSettings` (Project Settings -> MOFramework -> World
> Generation), `UMOTerrainFunctionLibrary` (Voxel nodes `MO Terrain Sample` / `MO Terrain Bounds` / `MO Terrain Caves` / `... Caves Bounds`),
> `UMOWorldSeedSubsystem` (terrain seed = CRC32 of the graph seed string). Numpy replica for fast iteration: `Tools/terrain_lab/`.
> **Dev verbs:** `MO.WorldGen.Sample x y`, `.Compare [r] [step]`, `.Checksum`, `.FindPeak`, `.FindCoast`, `.FindBiome`, `.FindBiomeSpot`,
> `.BiomeStats`, `.SeedSurvey`, `.FindWater`, `.DebugNoLodCull`; `MOFramework.WorldGen.DumpMaps` (hillshaded PNGs in `Saved/WorldGen/`);
> photo tools `MO.Cam.LookAt` / `MO.Cam.Reset`. **Tests:** `MOFramework.WorldGen.*` (GoldenChecksum, CoastalSpawn, CoastalSpawnOnVegetatedGround,
> MountainsAreTall, RangeCoarseLOD, RangeLODSeams, OceanDepth, IslandEdge, BiomesFromDataTable, SoftThreshold, SurfaceBlendBands,
> PickSurfaceBiome, CliffsFaceDownhill, UplandLakes ...). **Open items:** `Docs/BACKLOG.md` § World: terrain, caves, geology. **Decisions:** DECISIONS § World.
> **Terrain ground material:** all 13 surfaces share `M_QuixelBase`, built / updated ONLY by `Content/Python/upgrade_terrain_material.py`
> (code-only changes update in place; a new node / input needs `git checkout f78553e3 -- <M_QuixelBase.uasset>` = the pristine graph, editor
> closed, then rerun; never delete expressions from editor Python). It holds anti-tiling (two reads on rotated grids at unrelated scales; no
> single axis-aligned read at mid scales), far detail-normal flattening (`FarNormalFlatten` 0.85; Cliff / IcelandicQuarryRock 0.15), distance
> tint, the `RoughnessMin` floor, snow by altitude + aspect (north = +X; the UDS noon sun shines along +X; snow line +-600 m x lean toward the
> sun), and the weather on the ground (Oct 19, BR18-16: MO_Weather reads `MPC_MOGroundWeather`'s wetness / snow cover and the cave stamp's
> `VFM_MOWeatherShelter` metadata -- wet ground, puddles, a snow cover, none in caves; `Docs/Weather_And_Time.md` §2a). ddx / ddy return 0 in
> the Nanite pass's custom node: pass steepness in as an input. Verify with fixed `MO.Cam.LookAt` cameras on one
> slope at 3 km / 1 km / 300 m, packaged.
> **Reading this file:** the sections below are a dated build log, newest first (Oct 11 night at the top, then back to the Oct 9 exploration
> and its rationale). The header above is the current state; where a dated section disagrees with it, the header wins.

## Update (2026-10-11, night): Oceanology IS the water now (Epic Water is out of the levels and the code)

Wes: "im installing oceanology so let's switch to that ... you should be able to use most of the oceanology features for what we're doing". Uncommitted. Plugin: `Plugins/Marketplace/Oceanology_Plugin` ("Oceanology Legacy" 5.9.0 for UE 5.8, source +
Win64 binaries, enabled in MO57.uproject and a dependency of MOFramework).

### What changed
- **Levels** (`MOPCGScattering`, `LoadingLevel`): the Epic `WaterBodyCustom` is gone, each level has one `BP_OceanologyInfiniteOcean` (preset `03_Beaufort_GentleBreeze`). The menu fly-by over the new ocean looks right (checked in play).
  Pre-swap level files are in the session scratchpad `level_backup/` (copied before the first Oceanology edit).
- **`MOOceanologyIntegration.h/.cpp`** (`UMOOceanologyIntegrationSubsystem`, a tickable game-world subsystem): the project's facts about Oceanology in this engine -- the underwater-volume fix and the distortion cap below.
- **`UMOUplandLakeSubsystem`** now spawns `AOceanologyLake` from `BP_OceanologyLake` (async-loaded, `/Oceanology_Plugin/Design/Lake` in DirectoriesToAlwaysCook), replication OFF, plane sized from the generator (see below).
  12 lakes within 4 km spawn in the test world; no Oceanology warnings or errors in the log.
- Cheats: `MO.Water.Underwater` is rewritten for Oceanology (per water actor: camera inside its underwater volume, volume queryable, camera below the waves, the post process state AND the engine's own volume-test distance);
  `MO.Water.UnderwaterTrace` (Epic-specific) is gone; `MO.Water.FixUnderwaterVolume 0|1` (dev switch: reproduces the bug); `MO.Water.UnderwaterDistortion` (the cap).
- Tests: `MOFramework.Water.LevelOceans` (one Oceanology ocean per level, no Epic water actor), `MOFramework.Water.UnderwaterVolumeEncompassesCamera`, `MOFramework.WorldGen.UplandLakePlane`. Full suite 180/180.
  The Epic `WaterTrace` collision channel / `WaterBodyCollision` ini edits from earlier today are now dead config (nothing in the game uses Epic Water); left in place so a level that still has an Epic body keeps working.

### Findings (each one cost a diagnosis)
1. **The underwater look never applied to the infinite ocean** -- the plugin's box volume has NO collision and its post process is a bounded volume attached to it. In the live game the box has a physics object but no physics STATE, and
   `UPostProcessComponent::EncompassesPoint` (physics-object branch) answers "distance = FLT_MAX" for a camera in the middle of it (measured: `MO.Water.FixUnderwaterVolume 0` + `MO.Water.Underwater`). The demo map hides it with a separate post
   process volume. Fix: give the box a query-only body that ignores every channel (`MOOceanology::EnsureUnderwaterVolumeQueryable`, applied at world begin play and on spawn). Verified in play: the deep-blue underwater look at the seabed,
   NO tint above the water (the plugin's material is waterline-gated), nothing hits the 160 km box. The headless test world builds a body for the no-collision box where the game does not, so the control there is information only.
2. **A lake cannot be rotated.** Oceanology's water mesh is a quad tree over WORLD-aligned bounds, so the plane is the ellipse's axis-aligned bounding box (`FLake::PlaneHalfSizeCm`, margin 2%), not the rotated box the generator buried.
   `UplandLakePlane` measured 46 of 2470 lakes (0.04% of samples, worst 3 m) with water over low ground in the corners; the generator (`ComputeUplandLake` step 2b) now also buries that rectangle (+3 m, 60 cm freeboard).
   **Golden height pin re-pinned 0x4DA7FD20 -> 0xB0A52BB6** (lake surfaces moved for a few lakes; the water-off anchor is untouched).
3. **The plane is whole tiles from a grid-snapped centre**: half-size = TileSize x floor(ExtentInTiles x actor scale), centred on the actor snapped to the tile grid. SpawnLake counts the tiles, rounds up, and snaps, so the plane always covers the shore.
   **Do not size a lake by scaling the actor to a large factor**: the quad tree pre-allocates ~(2 x ExtentInTiles x scale)^2 x 4/3 nodes. Upland lakes are 70-130 m radius (scale ~9-15, fine); a 3 km lake would need ~270 MB, so a big water body needs a bigger TileSize instead.
4. **Oceanology actors replicate** (`bReplicates = true` in the parent): the upland lakes are spawned per machine from the seed, so SpawnLake turns replication off before FinishSpawning (else the host's copy doubles them on clients).
5. **Caustics**: the plugin's only caustics are a projected, scrolling texture (`T_Caustics`: the surface/underwater PP, and a decal under each lake) -- the cheap "fake" kind, already on by default. There is no heavier mode to avoid.
6. **Underwater wobble** (Wes: "really strong and looks pretty bad, make it 1/4"): `FOceanologyUnderwater::DistortionIntensity` and `VignetteDistortion` (0.05 each) warp the whole frame. Presets overwrite them on every load, so the project ENFORCES a cap
   (`MO.Water.UnderwaterDistortion`, default 0.0125, re-clamped every 0.5 s). Checked in play at the lake: the trunk and the seabed no longer swim.
7. **Buoyancy / swimming (investigated, NOT integrated)**: `UOceanBuoyancyComponent` = pontoons (sphere samples on sockets/offsets) applying Archimedes forces + damping + optional wave force to the owner's physics mesh or the character movement;
   `UOceanSwimmingComponent` = swim / surface-lock / sprint / dive state, head-pontoon enter/exit events, bubbles/trail effects, and a TIMER-based drowning that kills. Both need an `AOceanologyWaterVolume` (an `APhysicsVolume`, which cannot be spawned with new brush
   geometry at run time) around the water, and Blueprint-side input. AMOCharacter is a plain ACharacter, so it would plug into the stock CMC swimming mode. Today the pawn WALKS ON THE SEABED 60 m down and cannot drown.
   **The swim animations are already in the plugin -- no download needed**: `Oceanology_Plugin/Content/Character/Mannequins/Animations/Swimming/` has surface swim (idle, forward, slow forward, left/right) and underwater swim (forward, up, down, left/right) clips plus three blend spaces,
   the Manny/Quinn animation blueprints, and Enhanced Input actions `IA_SwimFast / IA_SwimUpOrDown / IA_DiveUp / IA_SurfaceLockedSwimming` (`Character/Input/Actions`). They are for the UE5 mannequins; retarget Manny -> the MetaHuman skeleton with an IK Retargeter. (A web search for a separate free pack found only paid Fab listings; none was confirmed free.)
   Design notes for the swim unit: drowning must come from the medical sim (head under -> SpO2 falls; the plugin's timer death is turned off), swimming costs energy, and the water-volume problem for upland lakes needs an answer (a volume per lake is a box: reuse one level-placed brush volume per size class, or drive CMC swimming from our own water query).

8. **Cooking (found by the packaged co-op test, fixed)**: Oceanology loads its materials, effects and sounds through SOFT paths in `UOceanologyRuntimeSettings`, which the cooker does not follow. The first packaged run crashed in
   `UOceanologyUnderwaterComponent::InitBubbles` (null bubbles material) the first time an upland lake spawned; the editor never shows it because the assets are on disk. `Config/DefaultGame.ini` now always-cooks `/Oceanology_Plugin/` Design/Lake,
   Design/Audio/Cue, Design/Ocean/{Blueprints/Effects, MaterialCollections, Materials}, Effects/Particles/Water and Niagara, and `MOFramework.Water.OceanologyAssetsAreCooked` reads every soft path of the settings and fails if one is outside those
   directories (it failed before the ini fix, so it can fail). Packaged Development build re-verified: checksum match (height 0xB0A52BB6), packaged two-process co-op test PASS, "[MOLakes] 5 lake surface(s) created" in the packaged log, no crash.

### Overnight 2026-10-11: PCG made biome-aware (tiers, water edge, native chains)

Wes went to bed with: keep working; biomes + assets; snapshot each biome and check it ("doesn't have weird shit like trees spawning in water"). Tooling built for that loop: `MO.PCG.WaterAudit [radius]` (instances standing in water), `MO.WorldGen.FindBiomeSpot <id> [radius_km] [rough] [go]`
(dry, flat, deep-inside spots of a biome) and the tour driver (`scratchpad/tour.sh <seed> <dir> <wait> <biomes...>`: boot, go, wait for PCG, 3 screenshots + audit per biome).

**What the tour showed (baseline):** every biome looked like "lush grass + black locust + alder" -- Meadow had big forest trees, the rocky highland was a grass slope, the dry scrub green. Cause: `MOPCG_StampScatter1` holds BIOME-BLIND native chains (Static Mesh Spawners with only an elevation node,
sampler 50 points / m^2 for the grass chain) laid over the one biome-aware node, and the biome spawner itself sat on the small GRID16 chain, so beyond that chain's radius the "biome" content was simply absent.
**Two facts that were wrong in the first version of this note (measured later the same night, see "The sampler lattice" below):** the graphs carry `HiGenGridSizeMultiplier = 4`, so GRID16 is a 64 m cell with a 128 m generation radius (not 16 m / 32 m), GRID32 = 128 m / 256 m, GRID128 = 512 m / 1 km;
and the Voxel Sampler can place AT MOST ONE point per `CellSize` lattice cell, so a node's `points_per_squared_meter` only thins below that cap. The biome spawner's samplers had 10 m and 25 m lattices, i.e. a ceiling of 100 and 16 points / ha while the node was told 2000 and 1200: every authored density was realised at 5 % or 1.4 % -- that is why forests read as open parkland.

**Fix (data + one filter, no new graph concepts):**
- `UMOPCGElevationBandsSettings::AllowedBiomeIds` (+ `bExcludeWater`): the shared elevation node of a native chain now says which biomes the chain belongs to. Grass / flower chains: not on bare rock or in dry scrub; the 4 km alder forest: forest biomes only
  (`MOPCG_FarTrees` too).
- `UMOPCGBiomeSpawnerSettings::SpeciesFilter` (ground cover / large / all, split by HISMTag): the biome spawner now runs TWICE -- ground cover on the GRID16 chain (64 m cells, 128 m radius; sampler 0.2 / m^2 = 2000 / ha on a 150 cm lattice), and LARGE species (trees, shrubs, bushes, boulders) on a
  GRID32 chain (128 m cells, 256 m radius; sampler 0.12 / m^2 = 1200 / ha on a 250 cm lattice): palette-driven forests per biome out to 256 m instead of black locust everywhere. Measured in the log: ~820 input points per 64 m cell and ~1950 per 128 m cell, 2-14 ms each. The biome-blind black-locust tree chain and the native resource chain are OFF (the biome rows carry those resources already).
- **Water edge:** `FMOBiomeDefinitionRow::WaterEdgeSpecies` + `WaterEdgeBandCm`: per-biome shoreline flora (reeds = the tall `smdrailr` grass at 420 / ha per variant, wet grass, shore flowers, stones), planted ADDITIVELY on dry ground within the band above the local water surface and within 25 m of the water,
  density falling from the waterline (`FMOPCGWaterMask::ShoreFactor`: upland-lake ellipse or the sea / river / pond test). Seven biomes have rows (rocky / alpine / dry at 40-50%). Replace the stand-in reeds with real reed / rush / cattail meshes when Wes allows a download (ElectricDreamsEnv has Taro / ferns / ground cover to try first).

**The sampler lattice (the rule to remember).** `Voxel Sampler`: `Ratio = (area * PointsPerSquaredMeter) / CellCount` clamped to 1, one candidate per `CellSize` cell. Realised density = min(PointsPerSquaredMeter, 1e4 / CellSize^2) (CellSize in cm). Pick `CellSize <= ~0.7 * 100 / sqrt(rho)` cm
so the target is reached with random thinning (rho 0.2 / m^2 -> 150 cm; rho 0.12 -> 250 cm). The biome spawner logs `[MOBiomeSpawner] in=N ... ms= inBounds=` per execution: `in / area_ha` is the real emission rate and must equal the node's `InputPointsPerHectare`, otherwise every `DensityPerHectare` in `DT_Biomes` is scaled by the ratio.
The pre-existing native chains have the same cap (grass: 50 / m^2 on a 60 cm lattice = 27.8 / m^2; far alder: 0.12 on 5000 cm = 4 / ha).

**Later the same night (final state of the PCG / biome pass).**
- **Three tiers now, one node type:** ground cover (GRID16, 0.2 / m^2 on a 150 cm lattice), large species (GRID32, 0.12 / m^2 on 250 cm), and a SHORE tier (`SpeciesFilter = WaterEdgeOnly`, GRID16, 2 / m^2 on a 60 cm lattice, input 20,000 / ha) that plants only the biome rows' `WaterEdgeSpecies` and is skipped for any 64 m cell
  with no water near it (`FMOPCGWaterMask::MayHaveShore`, a 10 m height grid + the lake list; soundness test `MOFramework.PCG.MayHaveShore`). The shore tier's own mask (`EdgeMask`, bank margin 1.008) lets reeds stand right at the waterline, in the strip every other species is kept out of (margin 1.05).
  Density: reeds 4000 / ha per variant (x 0.4-1.0 by biome), wet grass 1200, shore flowers 400, stones 70; band `waterEdgeBandCm` = 120 cm above the water. Measured at a lake: 130 plants in the 64 m cell holding its east bank (was 52 with the first numbers, 0 before the bank strip existed).
  The reeds are STAND-INS (the tall `smdrailr` grass): they read as tufts at the waterline, not as reed beds. Real reed / rush / cattail meshes need a download (Wes's yes) or an ElectricDreams asset.
- **Summits were untextured grey:** every biome ended at 1000 m (the mountain ranges reach 2.4 km) and no biome covered the cold temperatures up there, so those points had NO surface type. New row `AlpineRock` (height >= 950 m, any slope / climate, priority 22, `VST_Cliff`, the rocky species at 60 %). `BiomeStats` now reports "(none) 0.0 %" (was 3.7 % at mean height 1400 m).
  Regression test: `MOFramework.WorldGen.BiomesFromDataTable` sweeps height x slope x moisture x sea-level temperature and asserts every point has a biome (verified to FAIL when AlpineRock starts at 2000 m: 237 uncovered combinations, first at 1050 m).
- **Alpine ground:** `AlpineHeath` painted pale `VST_MI_Gravel` (read as desert behind the grass); now `VST_MI_AlpineHeath` (`Content/Python/make_alpine_material.py`: the muddy-leaves set tinted olive-brown, registered in the mega material).
- **Verified by tour (seed 7, one spot per biome + a lake bank + a summit):** `MO.PCG.WaterAudit` = 0 instances in water in every biome; TemperateForest / LocustWoodland / Rainforest show real tree stands out to ~250 m; Meadow open with scattered saplings; DryScrub / RockyHighland / ShoreStrand visibly different grounds.
- **Tool traps:** the editor throttles PIE to ~3 fps when its window is not foreground (PCG needs 70-100 s per spot, not 20); never run two `tour.sh` at once (the script now takes `tour.lock`); a Python `get_instance_transform` census over thousands of instances takes ~30 s.

### To-do (Wes, 2026-10-11) -- not done yet
- [x] **PCG must not plant in lakes** -- DONE (overnight 2026-10-11). Diagnosis: only the biome spawner had a lake check (1.05 radii) and it did not take effect; EVERY other chain (native Static Mesh Spawners, the MO item / resource / spawn-point
  nodes) only knew the sea plane (the "Ocean" elevation band, Z < 0), and upland lakes are ABOVE sea level. `MO.PCG.WaterAudit` measured 51,374 of 56,937 instances within 200 m of one lake in the water. Fix at the chokepoint: `FMOPCGWaterMask`
  (`MOPCGWaterMask.h`: sea plane via the point's own ground Z + the generator's upland-lake zone) applied inside `UMOPCGElevationBandsSettings` (every spawner chain passes through one; `bExcludeWater` default ON, not cacheable / main thread while on)
  and by the biome spawner. Result at the same lake: 0 in water (5,795 instances nearby, 95 on the dry bank 1.0-1.05 radii); a temperate forest spot: 42,772 instances, 0 in water. Tests: `MOFramework.PCG.WaterMask`. Tool: `MO.PCG.WaterAudit [radius]`.
- [ ] **Shoreline flourish / level design**: water reeds, rushes, cattails (and whatever fits) along lake and river shores, in the shallows and on the bank; a PCG species set keyed to "within N m of water, ground within X cm of the surface". Needs reed meshes (Megascans/Fab) -- ask before downloading.
- [x] **Lake basin material** -- DONE (overnight 2026-10-11): the bowl was the surrounding biome's ground (the forest's muddy leaves under 5 m of water). `FMOWorldGen::UplandLakeBedWeight` (1 inside the shore ellipse, ragged rim over the next ~10% of a
  radius) drives the terrain node (`UMOTerrainFunctionLibrary`): where it is >= 0.5 the voxel surface is `UMOWorldGenSettings::UplandLakeBedSurfaceType` (default `VST_MI_Gravel`: grey rock + rocky trail, already in the mega material). Looks right in play
  (clear water over gravel, ragged gravel rim, caustics on the stones). A softer MUD bed is a texture-set job (`Content/Python/make_sand_material.py` is the recipe; ElectricDreamsEnv `SwampWater` / `DriedGrassOnDampSoil` are candidates).
  Tests: `MOFramework.WorldGen.UplandLakeBed`.

**Where to get the assets for the shore items (Wes, 2026-10-11): `D:\UEProjects\ElectricDreamsEnv`** -- Epic's Electric Dreams Environment sample (44 GB content, intensive PCG, great-looking Megascans). Surveyed read-only; nothing copied yet. Moving assets = the editor's Migrate tool, selectively (the 3D assets alone are 21 GB).
- Basin / bank materials (Megascans Surfaces): `SwampWater`, `DriedGrassOnDampSoil` (damp mud-grass), `MossyRockyGround`, `IcelandicQuarryRock`, `JungleGround`; 3D assets `DryRiverBed`, `GrassyEmbankment`, `MossyEmbankment`, `NordicBeachRock(s)`, `NordicBeachRockyGround`, `BeachSandWithPebbles`, `SmoothRockShelf`, `ErodedRockyGround`, `SmallStonesPack`.
- Shore plants (Megascans 3D_Plants): `Taro` (big-leaf water-edge plant), `Periwinkle`, `KikuyuGrass`, `GroundCover`, ferns (`Fern`, `BostonFern`, `SilverLadyFern`, `BeechFern`), `CustomMoss`, `Lichen`. No reed / cattail / rush asset by name: those still need a source (ask before downloading).
- PCG to learn from: `PCG/Graphs/Ditch` (a water-channel graph), `Ground`, `Forest` (+ `ForestCliffs`, `DiscardPointsInBumpyAreas`), `SmallAssembly` / `LargeAssembly`; river-bank "assemblies" `Asmbly/Assemblies/BPP_*Embankment_Mud*` (mud bank with root, rock and vegetation layers) -- exactly the shore look Wes wants.
- [x] Swim / dive / drown unit -- DONE (2026-10-11, commits cce2524a + follow-ups). Findings 7's design notes were followed: the water query is our own (`FMOWorldGen::GetWaterSurfaceCm` via `UMOWaterSubsystem`, no per-lake volume), swimming is stock CMC swimming driven by
  `UMOCharacterMovementComponent` with ONE brushless `APhysicsVolume` (bWaterVolume) that the component returns while swimming; drowning is the medical sim (`UMORespirationComponent`: oxygen reserve -> SpO2 cap -> tunnel vision / unconsciousness -> asphyxia death 75 s after empty); the swim animations are the
  plugin's Manny set retargeted to the MetaHuman (`Plugins/MOFramework/Content/Characters/Swim`, 14 clips, UE 5.8 retargeters need their op stack). Verified in the editor (GoToLake -> Stroke -> MO.Water.Swim lines + screenshots) and headless (`MOFramework.Water.*`). Still open: HUD breath meter, cold water, load buoyancy, swim blends,
  creatures that swim (see `Docs/Archive/AAA_Polish_Backlog.md`).
- [ ] Float-able objects: Oceanology buoyancy on dropped items / logs / boats (supersedes the legacy `MOBuoyancyComponent` + `AMOWaterActorBase` Gerstner actors, which no level uses).
- [x] Lake look: the default lake is bright turquoise; tune toward a freshwater highland colour (lake preset, surface scattering) once the shores are planted. **Done 2026-10-13:** `UMOUplandLakeSubsystem::SpawnLake` sets the lake's `SurfaceScattering` from `mo.Lakes.Absorption` ("2.5 4.5 3 50") / `mo.Lakes.Scattering` ("0.005 0.012 0.008 2"; empty = the blueprint's own). Absorption behaves as how far each channel carries (bigger = clearer: 35/30/45 went clear and pink); the plugin default 10/25/30 + a bright scattering made the resort-pool turquoise. Now dark teal-navy in the deep water, green in the shallows; from the 600 m overview the lakes read as dark water instead of cyan pools (`pkg_probe/lake_def|B|C|final.log`). The bed is now dark pebbles instead of the pale gravel (`UplandLakeBedSurfaceType`).
- [x] Lake cost: 12 lakes live within 4 km (SpawnRadiusCm 400000); measure GPU/CPU and shrink the radius if needed. **Measured 2026-10-13 (packaged, 1080p, ProfileGPU, log `pkg_probe/lake_gpu.log`):** a lake filling the view from its bank costs SingleLayerWater 1.10 ms + its depth prepass 0.14 ms of a 10.1 ms GPU frame; the 600 m overview with the sea and several lakes in view 0.26 + 0.10 ms of 9.1 ms. Lakes out of view cost next to nothing and the CPU query runs on a worker (hitch hunt, Oct 12): the radius stays.
- [ ] Hair buns vanishing under the water effect (Single Layer Water vs hair strand depth): the engine patch is still waiting for Wes's yes; re-check with the Oceanology look first.
- [x] **No caves up high (Wes, 2026-10-13)** -- **done in generator v7 (commit 60c666f9)**: `FMOWorldGenTuning::CaveMaxGroundCm` 750 m (below the summit biomes) with a `CaveHighFadeCm` 100 m taper, applied in `FMOWorldGen` (tunnels / caverns / sinkholes), so the `MO Terrain Caves` stamp follows; checksums re-pinned (`PinnedProbeCave`); test `MOFramework.WorldGen.NoCavesOnHighGround` (no cave sample above the ceiling; control with the ceiling off: 57 of 4800 high-ground samples are cave). Original note: caves must not generate above a certain altitude, about where the mountain peaks start (`AlpineRock` >= 950 m): they still fracture the peaks when seen from far away (the far voxel LODs cannot represent the thin rock between a cave and the surface, so the summit tears). Wanted: an altitude ceiling for the tunnel / cavern / sinkhole carving in `FMOWorldGen` (a soft ceiling like the biome splits: probability fades out over a band, deterministic, no ISPC / sin / cos) and the `MO Terrain Caves` Voxel stamp, then re-pin the golden checksums (`MO.WorldGen.Checksum`) and extend `MOFramework.WorldGen.Caves` with a "no cave above the ceiling" check. Look first at whether the tears are the caves or the plain far-LOD mismatch (the neighbouring-LOD surface mismatch p99 table above): `MO.WorldGen.FindPeak` + the tour at a peak with caves off (`MO.Cave...` toggle) tells which.
- [ ] **Distant mountains look bland (Wes, 2026-10-12)**: far mountains read as one uniform material with a hard elevation split. Wanted: snow caps with dark rock showing through (steep faces and ridges dark, flatter high ground snow), and the
  material set switching out with distance (a far-LOD look: macro colour variation, darker rock bands, snow line that follows slope / curvature / aspect, not a flat height cut). Related: the known far-LOD tear on the mountains (P2 in `Docs/Archive/AAA_Polish_Backlog.md`);
  `AlpineRock` (>= 950 m, `VST_Cliff`) and `AlpineHeath` are the current summit biomes. Check first how the Voxel far LOD / material blending actually resolves surface types at distance, then decide between a distance material LOD and a better near/far-consistent rule.
  **Tried 2026-10-13 late evening, reverted (negative result):** in `upgrade_terrain_material.py` FINISH_CODE, (1) past ~1 km the mountain zone's steep faces lerp to dark rock and the far snow keeps a wide slope window instead of following altitude alone; (2) noise-driven exposed-rock patches (180 / 290 m + 1.1 km rotated reads) in the mountain zone, snow thinned over them. Packaged shots from 8 km / 3 km (the 1334 m summit of seed 1029024224, `MO.WorldGen.FindPeak 25 go 8`): (1) changed nothing -- these mountains are broad domes at range, almost no face passes 30 deg on the far LOD; (2) read as pale camouflage blotches at 3 km (linear 0.10 grey rock is BRIGHTER than the grass albedo, so 'rock' does not mean 'darker') and vanished in the aerial haze by 6-8 km (a debug full-red tint only reads salmon there). A red / mask debug build confirmed the code ran and the masks were right, so the limit is not the shader: it is the far geometry (smooth domes: the relief that would carry rock faces, gullies and ridges is not there at 5-10 km) and the haze. Options for Wes: mountain-scale relief that survives the far LOD (ridges / gullies 100+ m deep), a clearer atmosphere on Clear_Skies (height-fog density), a lower or temperature-driven snow line so the domes at least carry a cap (realism call), or an artist's far texture.
- [x] **Soften every elevation / climate split in the ground materials (Wes, 2026-10-12)**: height, slope, moisture and temperature thresholds in `FMOWorldGen` / the terrain node currently switch the surface type abruptly (hard bands). Every split should have a blend
  band (fade width) AND a noise term that breaks the edge up (the Electric Dreams landscape material's `Breakup Tiling / Intensity / Contrast` and per-layer `Slope Offset / Hardness / Contrast` are the reference, `Docs/ElectricDreams_Lessons.md` section 1). One chokepoint
  for it (a shared "soft threshold" helper used by every material rule), plus a test that the transition has width and is deterministic (no ISPC / sin / cos, see the determinism rules).
  **Done 2026-10-13.** The chokepoint is `FMOWorldGen::SoftThreshold(Value, Edge, Width, Breakup)` (a smoothstep band centred on the edge; the breakup moves the edge; width 0 = the old hard step). `FMOWorldGen::SampleSurfaceBiomeWeights` runs every biome's height,
  moisture and temperature edge through it, composites the biomes in priority order (each takes its share of what the higher-priority ones left) and returns up to 3 weights that sum to 1; the MO Terrain node's surface output mixes those biomes' surfaces
  (`FVoxelSurfaceTypeBlendBuilder`), and a column inside one biome keeps its ready-made blend (no extra cost there). Tuning (`FMOWorldGenTuning`, not hashed into the save identity because it changes no geometry): `SurfaceHeightBlendFraction` 0.08 of the edge's
  height above sea level, clamped to `SurfaceHeightBlendMinCm` 1.2 m .. `SurfaceHeightBlendCm` 40 m (a fixed 40 m band spread beach sand over the whole coastal plain, because the shore edge is only ~4 m above the sea); `SurfaceClimateBlend` 0.03 (moisture /
  temperature units); `SurfaceBreakup` 0.6 of the band, noise wavelength `SurfaceBreakupWavelengthCm` 90 m. The slope split is the material's (M_QuixelBase's slope window, already a smoothstep that widens with distance); slope-banded biomes still never apply
  to the column surface. Tests: `MOFramework.WorldGen.SoftThreshold`, `MOFramework.WorldGen.SurfaceBlendBands` (weights are a partition and deterministic over 3000 columns; of 30 hard `ResolveBiome` boundaries found by walking lines, 30 have a mixed band
  of 3 m or more, mean 58 m). Seen in the packaged game: at 300 m the grass thins out into the dirt instead of stopping at a line; at 3 km the boundary is softer but the far-LOD vertex step still shows.
  **Foliage follows the same band:** `FMOWorldGen::PickSurfaceBiome` rolls each PCG point's biome against those weights (a seeded hash of the point's 10 cm cell; a slope-banded biome that `ResolveBiome` picks still wins outright, and a biome
  whose slope range excludes the point is dropped from the roll). The biome spawner uses it in generator mode (`mo.PCG.BiomeSoftPick`, default 1; 0 = the old hard first match + 4-tap climate feather) and so does `FMOPCGWaterMask::BiomeAt`
  (the elevation-bands node's `AllowedBiomeIds` filter), so both biomes' plants interleave and thin out where their ground does. `DT_Biomes.EdgeBlendWidth` now only matters with the cvar off / outside generator mode. Packaged A/B, same nine shots
  and cells: spawner CPU 5.2 vs 6.1 us a point (the four climate taps are gone), instances 459 k vs 450 k (the feather no longer thins the band), READY 6.5 vs 6.3 s. Test `MOFramework.WorldGen.PickSurfaceBiome` (150 mixed columns:
  mean ground weight 0.573, share of points picking that biome 0.573; inside one biome the pick is that biome; cliffs win; deterministic).
- [ ] **Sand variety (Wes, 2026-10-12)**: one bare sand material makes beaches and deserts repetitive. Wanted: a few sand materials (colour / grain / ripple / pebble / wet-dark near the waterline / dry-pale dunes / sand over rock) distributed by a low-frequency noise and by
  distance to water, blended with the same soft-threshold rule. Recipe for a voxel material from a texture set: `Content/Python/make_sand_material.py`; migrated candidates in `Docs/ElectricDreams_Migrated.md` (`DriedGrassOnDampSoil` for the damp edge, `BeachSandWithPebbles` meshes
  for pebble scatter); more sand sets may need a download (ask Wes first).
- [~] **Alpine / snow ground: far more variation, like real mountains (Wes, 2026-10-13 evening)** -- **(a) ASPECT DONE Oct 15 2026**: north = +X (the Ultra Dynamic Sky noon sun shines along +X, measured), FINISH_CODE shifts the snow line by `AspectLift` 600 m x the face's lean toward the sun (+-300 m on a 30 deg face), faded to a third at range; PIE shots `Saved/CoverShots/snow_top.png` (south half of a 1,674 m summit bare, north holds snow to the rim) / `snow_far.png` (9.5 km: clean cap, no blotches). Range check done in a standalone -game instance (`Saved/SteamArt/plates/snow_300|1000|4000.png`, 11:00 clear): 300 m snowfield with rock, 4 km cap with rock bands and the bare south ridges; at 1 km the sunlit snow reads almost flat white (albedo vs exposure, not the aspect term -- a SnowColor / exposure question for Wes, untouched). Still open: (b) bigger noise if Wes wants more. Original entry: the alpine / snow level reads too uniform. Wanted: much more
  variation in the snow and rock, either (a) ASPECT-driven snow -- northern hemisphere, so south-facing slopes hold less snow (sun-warmed, bare rock and scree) and
  north-facing slopes more -- or (b) at least much bigger, more dynamic random noise in the snow / rock mix; ideally both, on top of altitude and slope. Notes for whoever
  builds it: it is a look rule, so the material is the layer (M_QuixelBase via `Content/Python/upgrade_terrain_material.py` FINISH_CODE: the pixel normal's horizontal
  direction gives the aspect, cheap per pixel, no generator / checksum change); first pin which world axis is NORTH so the bare slopes face the sun's actual path
  (check the Ultra Dynamic Sky sun azimuth at noon) and keep a slope term (cliffs stay rock). Mind the reverted Oct 13 attempt above: at range, grey rock under haze
  read as pale blotches brighter than grass, so judge from packaged shots at 300 m / 1 km / 4 km, and keep the far snow / rock contrast dark-on-white.
- [x] **DONE in generator v9 (2026-10-16, Wes: "I don't care about old saves")**: the spaghetti tunnels are replaced by GALLERIES -- stacked levels every
  `GalleryLevelSpacingCm` (30 m) whose passages follow the zero set of a 2D field in plan (distance and direction from its analytic gradient) at a height
  that wanders `GalleryLevelAmplitudeCm` (11 m) over `GalleryLevelWavelengthCm` (400 m): `MOFramework.WorldGen.GalleriesStayLevel` follows 250 passages
  centre to centre, all within 15 deg (steepest 4 deg). `TunnelRadiusCm` 320 -> 160 (3.2 m wide, ~2.8 m high), a mask tapers stretches of the meander
  shut (smoothly: skipping the level put a cliff in the field). Openings: the cover holds everywhere except where a passage runs INTO a slope of
  16..32 deg (12 m-baseline slope -- over a terrace edge a 3 m one let the gate jump), so caves are entered from hillsides, never fallen into; the
  `Caves` test checks the cover over every flatter column. Mouth density, measured Oct 17 by `MOFramework.WorldGen.CaveEntrances` (the hilliest 2 km of CAVE COUNTRY -- above the coast, below the 650 m high-ground taper -- scanned at 4 m, mouths grouped by 30 m): **4 per km2** at 16-32 deg (asserted >= 3); the test also prints what other bands would give (14-24: 8.5, 12-20: 13, 10-16: 18.5) -- kept at 16-32 because a 3 m tube meeting a gentle slope cuts a long open trench, the hole-in-flat-ground Wes ruled out. Flat lowland and the ranges have none, so a coastal spawn can be kilometres from the nearest mouth (world 31415926: ~9 km). In game: `MO.WorldGen.FindCaveEntrance [radius]` lists mouths with their slope; a mouth on a 31 deg hillside reads as a tunnel into the hill (Saved/Logs/cave_v9). Sinkholes default to chance 0
  (kept as a feature for a world that wants them; their tests run on such params). Found on the way: `DistanceToSinkholeRimZone` returned 30 m with no
  sinkhole, rejecting world features with bigger footprints wherever there was none (see World_Features_Architecture.md). (v9 pin 0xEA773801.)
- [x] **DONE in generator v10 (2026-10-17, Wes: caves must not reach sea level and fill with the ocean / show the underwater post process, and "we don't
  want them right near shore lines -- they'd immediately be below water table")**: one WATER gate in `SampleCaveDistanceAtGround`, folded into every cave
  feature's gate. (1) A floor: no cave air below `SeaLevelCm + CaveWaterClearanceCm` (5 m), tapering shut over `CaveWaterFadeCm` (6 m) -- the ocean mesh
  fills every point under sea level, so this alone keeps water and the underwater effect out of caves; `CaveMinZCm` starts there too (the volume stamp no
  longer samples anything under the sea). (2) A shore keep-out: no cave within `CaveShoreKeepOutCm` (250 m) of LOW ground -- every sea-level water body
  lies on base ground under the river headwater / basin rim ceiling (18 m), where the water table is the sea's; read on a 16 m lattice (bilinear, so the
  field stays smooth on any coast) from the pre-water base height at the node + three rings (8 / 16 / 16 samples) -- and 120-180 m from an upland lake's
  shore (`UplandLakeShoreDistanceCm`, conservative). First cut (rings on the carved ground, lake keep-out in the lake's radii) failed the new test: 6 of
  300 cave samples had water within 100 m (an upland lake along its ellipse's short axis). `MOFramework.WorldGen.CavesStayDry`: 300 random cave-air
  samples in 40 x 40 km -- none below the floor (lowest 12 m above sea level), none with water (`ClassifyWaterBody`) within 100 m in 16 directions.
  Mouth density unchanged (4 per km2 in hill country); cave chunk 7 ms. `PinnedProbeCave` 0x794FC2AD.
- [ ] ~~**Caves: half the diameter, and more lateral (Wes, 2026-10-13 evening)**~~ (the original note, kept): caves are too big and dive too steeply. Wanted: about half the current diameter, and
  passages that run sideways through the rock instead of heading straight down. Where it lives: `FMOWorldGenTuning` (`TunnelRadiusCm` 320 -> ~160,
  `SinkholeRadiusCm` 550 and the cavern threshold / `CavernWavelengthCm` for the big rooms), and the tunnel path's vertical behaviour (`TunnelVerticalSquash` 1.6 and
  the depth term of the spaghetti field in `MOWorldGenCaves.cpp`: flatten the field vertically -- higher Z frequency / a stronger squash / a depth that changes
  slowly along the path -- so the iso-lines stay near one depth). Geometry change: bump `CurrentGeneratorVersion` to 8, re-pin `PinnedProbeCave` (`MO.WorldGen.Checksum`),
  extend `MOFramework.WorldGen.Caves` with a diameter check and a "mean |dz| per metre along tunnel centre lines is low" check; mind the cave-field gradient limit
  (the trap noted under v5) and the LOD skip sizes (smaller tunnels drop out of far LODs sooner, which is fine). Verify by walking one in the packaged game
  (`MO.WorldGen.FindCave`).

## Update (2026-10-11, later): generator v6 -- mountain RANGES, not towers

Wes: "the mountains are still super fucked looking ... smooth out the larger features so they don't collapse to points or empty cells at the far LODs, enough mass to never collapse, a range of larger mountains with some sheer cliff details closer
up but not huge regions of thin sheer cliffs ... the height is good, but it's too steep and there's too many valleys, 1000 really tall skinny mountains instead of larger masses with 1000 small steep features". Uncommitted. Goldens re-pinned (Height 0x4DA7FD20, Cave 0x31D4E2B5;
the water-off probe anchor 0x9C4AE2B3 is untouched: `bRangeMountains = false` there). Generator version 6.

### Diagnosis (a numpy replica of the generator, `reproduced the real test numbers` before it was trusted: slope p50 4.0 / p90 38.5 / p99 77.2 vs the test's 3.9 / 36.1 / 77.8)
- **Every wavelength in the tuning is in CENTIMETRES.** The fields that decide WHERE mountains stand (erosion 450000 = 4.5 km, weirdness 300000 = 3 km) and the biome's hill / mountain scale (looked up from the LOCAL moisture, 175000 = 1.75 km, blended over
  0.12 climate units = ~150 m) were km-scale, while the ridge noise inside has a 10 km wavelength. The mountain mask went 0 -> 1 over ~600 m and the biome mountain scale 0.03 -> 1 over ~150 m: every km-sized patch was a full-height tower with vertical walls
  (3.4% of the land steeper than 60 degrees). Isolated, the ridged noise at full strength is just rolling dunes (p99 35 degrees); the "mountains" were the cliffs of the mask.
- At a coarse LOD this is a wall the mesher cannot represent: around a summit, 6% of adjacent samples at a 128 m step were steeper than 45 degrees (max 84) and 4% at 512 m -- the shards and holes in the distant mountains.

### The fix, at the layers that made the bad state
1. **Range scale.** `MountainRegion*`: a 2-octave field of 30 km, smoothstep Low..High (-0.25..0.33) x gain 1.6, times the inland factor -> wide soft ramps (km-scale, never a wall). The old erosion / weirdness mask still exists for the legacy path.
2. **Regional biome relief.** `ReliefRegionWavelengthCm` (30 km): `FMOClimateSample::MoistureRegion / TemperatureRegion` feed `BiomeReliefAt`; a biome patch is 2-3 km, a range is tens of km. (The prairie-flatness test classifies by the regional moisture now: dry 360 cm vs moist 2638 cm of change per 800 m.)
3. **A layered mountain** (`RangeShapeM`, metres before VerticalScale 2): a broad **massif** (smoothstep of a 2-octave 18 km field, 515 m: the MASS, it survives every LOD) + ridged **peaks** with ROUNDED crests (pseudo-Huber crest, `PeakSoftness` 0.22, 4 octaves from 4.2 km, 580 m, needing the massif's support:
   `0.15 + 0.85 x support`) - **erosion gullies** (ridged valley lines, 600 m, 25 m deep, cut where the base is steep: slope 0.15..0.55 from a 60 m finite difference) + sparse **crags** (sharp-edged ledges, 150 m, 12 m, only on slope 0.30..0.65 and only where a noise says so: the sheer detail up close, never a region-wide wall).
   Gullies and crags fade out over the LODs (full at 6x resolvable, gone at 1x) and every fine octave is culled below 2 x StepCm, so a coarse LOD sees only the massif and the big peaks.
4. `ErodedFbm` got a crest-softness parameter (0 = the exact old math); lake placement got a dense burial re-check (the sparse grid missed a dip between samples on the new foothills: 1 lake in 2470 showed a bare water plane).

### Measured (real tests, 4 seeds)
| | v5.1 | v6 |
|---|---|---|
| land slope p50 / p90 / p99 over 40 m | 3.9 / 36.1 / 77.8 | 3.3 / 10.9 / 34.8 |
| land steeper than 45 / 60 / 70 deg | 6.9% / 3.4% / 1.9% | 0.3% / 0.02% / 0.00% |
| highest ground (MakeReliefParams) | 1.45-1.65 km | 1.43-1.86 km |
| around a summit, adjacent samples steeper than 45 deg at 128 m / 512 m LOD step | 6% / 4% | 0.5-0.8% / 0-0.6%; walls (> 63 deg): 0.00-0.02% / 0.00% |
| summit height kept at a 128 m / 512 m step | | 99-100% / 94-98% |
| neighbouring-LOD surface mismatch p99 (64/128, 128/256, 256/512 m) | | 25 / 7 / 51 m |
| SampleColumn at LOD0 | 0.95 us | 1.5 us (the slope needs 3 base evaluations, only above 25 m of base) |
New tests: `MOFramework.WorldGen.RangeCoarseLOD` (walls and mass at the LODs the voxel world uses), `RangeLODSeams`; `MountainsAreTall` tightened (< 2.5% over 45, < 0.5% over 60, p99 < 55 degrees); the sinkhole test now measures the ground at the probe, not at the axis.
Dev tools: `MO.WorldGen.FindPeak [radius_km] [go [dist_km]]` (the highest summits; `go` puts the pawn short of the highest, hovering) + the existing `MO.Cam.LookAt` for photo shots; `MO.WorldGen.DebugNoLodCull 1` (cheat cvar: the base height ignores the LOD).

### What is NOT explained by the generator
A small TEAR in the farthest LOD (12 km): a V-shaped gap between two coarse faceted peaks, seen through to the sky, present in the user's shot and still there. It is identical with `MO.WorldGen.DebugNoLodCull 1` (no octave culling at all), so it is not our LOD mismatch; it is how the Voxel plugin meshes the farthest chunks.
Worth a look next: the voxel world's LOD quality / max LOD settings, or a Voxel bug report.

## Update (2026-10-11): deep ocean (v5.1), a world twice as wide, and why the underwater effect only worked looking up

All uncommitted. Generator **v5.1**: goldens re-pinned (Height 0x0CF8AC95, Cave 0x64490A6D; the water-off probe is still the v1 anchor 0x9C4AE2B3).

### Ocean and shoreline (Wes: "the ocean depth is really shallow and the shoreline meanders WAAAAYYYY out into the water")
- Diagnosis: the continent spline had a sea floor at -6 m (x2 = -12 m) and the hill noise rode on that near-flat shelf, so noise decided where the waterline fell and it wandered far out. Two layers: `ContinentSplineM` now drops
  fast below the waterline (0 m at continentalness -0.55, -14 m at -0.60, -45 m at -0.70, -80 m at -1; x `VerticalScale` 2) and the hill noise fades out over open sea (`HillSeaFadeLow/High` = -0.62 / -0.42, a smoothstep of the continentalness).
- Measured (test `MOFramework.WorldGen.OceanDepth`): sea shallower than 20 m 99% -> 8%, deeper than 100 m 67%, deepest -162 m (was -31 m); 234 of 300 shore points drop 20 m within 400 m in their steepest seaward direction.
  The spawn island check and the `Landmass` test needed a shallow-sea control because the deeper sea no longer produces islets.
- The water-off probe restores the old spline and `HillSeaFade` off (if you add another v5-style default, add it to `RunProbe(false)` too).
- Sand: `ShoreStrand` is now everything from the sea floor up to +4 m (heightMin -100000), so nothing below sea level is muddy leaves.
- World: `WorldHalfSizeCm` 3 -> 6 million (120 km edge); PCG volumes in both levels scaled x1.3 (+-65 km). Biomes halved again (moisture 175 km / temperature 375 km wavelengths).
- **Distant-mountain raggedness is NOT fixed** by any of this. Tried: 2x world, removing the cave stamp, the voxel world's game LOD quality in the editor (`always_use_game_quality`). Mountain meshes look smooth in the shots; the speckle I could
  reproduce at seed 7 is the dithered distant trees. Wes's holes were not reproduced. If it persists: send the seed, location and a 1:1 screenshot.

### Underwater post process: why it only worked looking up (root cause, fixed by configuration + level data)
- Symptom: the underwater look appeared when the camera pointed at the water surface, not when it pointed at the character.
- Diagnosis (instrumented, not guessed -- `MO.Water.UnderwaterTrace`): `UWaterSubsystem::ComputeUnderwaterPostProcess` only runs its underwater test `if (World->SweepMultiByChannel(...))`, which is true only for a
  **blocking** hit on `CollisionChannelForWaterTraces`. That channel defaulted to WorldStatic, which the game's ground / drop / placement traces use, so the ocean could not block it: the level's ocean actor had the
  `OverlapAll` profile. The sweep returned false, the whole test was skipped, `cached depth = -1` -- except when the character's own mesh (blocks WorldStatic) happened to be within 100 cm of the camera, i.e. when the
  third-person camera swung up beside the pawn. Replaying the plugin's steps on the game thread said "underwater" at every pitch, which is why that replay alone was misleading: it never checked the sweep's return value.
- Fix: a dedicated trace channel **WaterTrace** (`ECC_GameTraceChannel2`, default response Ignore for every profile) set as `CollisionChannelForWaterTraces` (`[/Script/Water.WaterRuntimeSettings]`), and the
  `WaterBodyCollision` profile now blocks it, overlaps WorldStatic (as the old OverlapAll ocean did) and blocks nothing else. The ocean actor in `MOPCGScattering` and `LoadingLevel` uses `WaterBodyCollision`; upland lakes get it
  from the class default. Tests: `MOFramework.Water.TraceChannel` (channel is dedicated, only the water profile blocks it, the water profile blocks no gameplay channel) and `MOFramework.Water.LevelOceans` (loads both levels, every
  water body uses the water profile). Seen: seabed camera at pitch 0 / 25 / 60 and 1.4 m under the surface at pitch 0 all have the effect; above water there is none (hits=0).
- Tools: `MO.Water.Underwater` (the plugin's steps at the camera), `MO.Water.UnderwaterTrace [0|1|all]` (per rendered view: the plugin's result as `cached(prev)` -- native multicast delegates run in REVERSE binding order, so a
  handler of ours runs BEFORE the water subsystem's and `View->UnderwaterDepth` still reads -1 there -- plus the replay).

### Hair buns vanish under the water effect (open: engine-level, no project-side fix found)
- Not groom LOD: the character's hair is strands only (`r.HairStrands.Strands 0` leaves a bald head, `Cards 0` changes nothing; 3 hair groups, no cards in use).
- Cause: the post process fogs every pixel by `SceneDepth`; hair-strand pixels over the water (buns, flyaways) have no depth of their own there, so they read the far water-surface depth and are replaced by fog colour. Hair over
  the skull survives because the head's depth is behind it. Evidence (same pose, seabed, pitch 0): `r.Water.SingleLayer 0` brings the buns back; capping the post process's `Max Depth` at 10 cm brings them back and 30 / 150 / 300 / 600 / 1500 cm
  dissolve them progressively; `r.Water.SingleLayer.DepthPrepass 0`, `r.HairStrands.Visibility.UseHWRaster 1`, `r.HairStrands.Visibility.MSAA.SamplePerPixel 8` change nothing.
- Why (engine source, UE 5.8): after `RenderSingleLayerWater`, `DeferredShadingRenderer.cpp` replaces `SceneTextures.Depth` with the SLW depth prepass (scene + water, no hair), and `AddHairVisibilityComposeSamplePass`
  then skips writing depth (`bWriteDepth = !bReadSLWDepth`, "already written"), so hair never reaches the depth every post process reads. (Read from the source, not proven by a patch.)
- Tried and DISPROVEN (do not retry): a copy of the water post process using `SceneDepthWithoutWater`, one using `Min(SceneDepth, CustomDepth)` with Render CustomDepth on the grooms (a debug post process showed custom depth is
  far everywhere, hair included), forcing hair cards.
- Candidate engine fix (NOT applied: it edits the source build at D:\UnrealEngine\UE_5.8 and needs Wes's yes): in `HairStrandsComposition.cpp` `AddHairVisibilityComposeSamplePass`, write depth also in the SLW case
  (`bWriteDepth = true`), since the main depth was replaced after the earlier hair pass wrote into the old one; Renderer module rebuild, then re-run the seabed-pitch-0 shot.

### Oceanology (asked about 2026-10-11)
Not installed in any engine on this machine (UE_5.3 / 5.5 / 5.7 / 5.8 Marketplace folders: no); the older projects under D:\Transfer reference it by marketplace id only. The Fab listing for **Oceanology Legacy** (Galidar,
$189.99, UE 5.4-5.8, C++ plugin) lists: Gerstner waves, local lakes, a buoyancy component, a swimming component with animations and breath, an underwater post process with caustics / absorption / god rays, shoreline foam. It would
REPLACE Epic's Water plugin here (ocean actor, upland-lake spawner, the collision fix above), so it is a design fork, not a switch to flip. It must be installed from the Epic Launcher library for UE_5.8 (not scriptable from here).
The listing metadata says "Allows usage with AI: No": read the license before pointing an AI tool at its source.

## Update (2026-10-10, afternoon): generator v5 — per-biome relief, tall mountains, half-size biomes, an island edge, sand, a rainforest, recruitment feedback

Wes's asks: mountains "many times higher" while lowland prairie stays flat (per-biome height variance); biomes about half the size; land that falls off into water at the edges so the world border is never met on land; a
rainforest in the spirit of Epic's PCG forest project; sand on the sea-level shores (voxel material and all); more Megascans for beaches; the small green bush that stops the pawn; and a survivor who could no longer be
recruited with nothing on screen saying why. All uncommitted. Generator **v5**, goldens re-pinned (Height 0xA310F037, Cave 0xEF77D093; the water-off probe is still the v1 anchor 0x9C4AE2B3).

### The land (FMOWorldGen v5)
| Piece | What | Numbers |
|---|---|---|
| **Per-biome relief** | `DT_Biomes` columns `reliefScale` (hills + the continent's height above a 20 m lowland cap) and `mountainScale` (mountain ranges + coastal ranges). The biome that applies is chosen from the **climate alone** (moisture + sea-level temperature) as a *soft first-match-wins* over the biomes that have no height band and no minimum slope (`BiomeReliefAt`): the height/slope-banded biomes (shore, alpine, rocky) are consequences of the terrain, so they cannot also shape it. Continuous: relief changes over `ReliefClimateBlend` (0.12 of a climate unit), never at a line | prairie (Meadow) 0.25 / 0.03, arid DryScrub 0.35 / 0.15, TemperateForest 1 / 1, LocustWoodland (cool + moist) 1.2 / 1.6, Rainforest 1.1 / 1. Inside one dry climate the ground changes **5.8 m over 800 m (0.4 degrees)** against 100+ m in a moist one |
| **Taller mountains** | `MountainAmplitudeM` 320 -> 1000, wavelength 8 -> 10 km, new `MountainGain` 0.38 (0.5 = every octave adds the same slope, six octaves of a tall range were near-vertical cliffs) | highest ground 1.3-1.7 km (was ~0.4); land p99 0.9-1.2 km; slopes over 40 m: p90 27 deg, **1.9% of the land steeper than 60 deg** |
| **Small biomes** | v5: moisture 700 -> 350 km, temperature 1500 -> 750 km, erosion 900 -> 450 km, weirdness 600 -> 300 km (wavelengths, cm x 1e5). **Halved again the same day for moisture and temperature only** (the biome deciders): 175 km / 375 km, about 1.75 km per biome patch. Erosion and weirdness stay, so mountain ranges keep their size. The continent (landmass) wavelength is unchanged | a walk crosses >= 3x as many moist/dry borders as before v5 (test `BiomesAreSmaller`) |
| **Island edge** | the continent field is pulled to open ocean between 62% and 93% of the half-size (squarish norm from two square roots: IEEE-exact), and hills / mountains / coastal ranges fade with it (`FMOClimateSample::EdgeLand`) so no islet survives | the outer 6% of the world is >= 5 m under water on 4 seeds x 4800 samples; 65-90% of the inner world is land (`IslandEdge`) |
| Upland lakes retuned | cell 180 -> 130 km (radius cap follows: 0.1 x cell), candidates 3 -> 6, footprint relief 18 -> 24 m: the taller relief had thinned them to 1 per 11 km^2 | 1 per 5.7 km^2 |

`FMOWorldGenBiome::IsClimateBiome()` is the rule for who shapes relief; `FMOWorldGen::BiomeReliefAt()` is the one function. With every scale at 1 the multipliers are exactly 1 and the v1-v4 bits are untouched (the anchor pins it). The v5 defaults moved four
wavelengths, the mountain numbers and the edge: **`RunProbe(false)` restores all of them** to stay the v1 anchor; if you add a v5-style default, add it there too.
**Trap found by measuring:** the first flatness test compared a dry point with a point 1.5 km away and read 170 m "flat prairie" -- the second point was in a moist climate (biomes are 3.5 km now), so it measured the BORDER. A flatness test must keep start, middle and end inside one climate class.
**Trap:** a biome's hill scale below 1 must also flatten the continent spline (`LowlandCapM`), which alone is ~130 m of change per 1.5 km; scaling only the hill noise did nothing visible.
**Trap:** the cave-field gradient limit (20) assumed gentle ground: the depth taper contributes ~10 per cm of depth change and a horizontal step changes depth by the ground slope, so 75 degree slopes read ~25-35. Now 40 (a real jump reads 100+).
**Spawn:** the island check flood-filled on two lattices 4-connected; a land strip running diagonally between lattice points read as cut (one seed failed the independent 250 m cross-check). Now 8-connected on 175 / 250 / 350 m, cross-check at 225 m.

### Ground and plants
- **Sand**: no sand surface existed anywhere on the machine (Megascans here: one Sand Rock mesh). `Tools/gen_sand_textures.py` makes a seamless 2K albedo / normal / roughness / height set (FFT-filtered noise: periodic by construction; seam ratio 0.99-1.00),
  `Content/Python/make_sand_material.py` imports it, duplicates MI_Gravel -> `MI_Sand` and VST_MI_Gravel -> `VST_MI_Sand` (same parent and tiling), adds the new surface types to the **VoxelMegaMaterial's `surface_types`** (a surface type the mega material does not list is not painted),
  and builds a darker greener `VST_MI_JungleFloor` for the rainforest. `ShoreStrand` (priority 13) paints sand from **everything below sea level up to +4 m**: beaches, river banks, the sea floor (it first stopped at -5 m, and deeper seabed showed muddy leaves).
- **Rainforest** (`Rainforest`, moisture >= 0.70 and temperature >= 0.55, priority 12, JungleFloor ground, hill 1.1): modelled on `PCGBiomeSample`'s structure (read from the engine plugin: biome assets with *child assets* scattered around their parents -- saplings, then seedlings, then boulders -- and sun / water-distance / flow filters).
  Here as ClusterGroups: group 4 = the tree stand (emergent giants 1.5-2.2x at the clump cores, 7 canopy species, 6 saplings and 2 seedlings in the ring), group 5 = the shrub thicket; lush tall grass, 9 flower families, mossy boulders, fallen trunks and branches. 2520 instances/ha (forest 1270).
  Not taken from the sample (no equivalent here yet): sun-exposure and water-distance filters per species (shade plants, riparian plants).
- **Walk-through species (`EMOSpeciesCollision`: `Auto` / `Passable` / `Solid` on `FMOBiomeSpeciesEntry`)** -- CORRECTED 2026-10-10 evening. I first blamed the BlackAlder *saplings* for "the small green bush that stops the pawn", made every `Sapling_*` `Passable`, and Wes
  had to tell me they were not it (I had guessed from the resource type instead of asking which bush or sweeping the thing under the crosshair). **Saplings are `Auto` again** (solid like trees). The real culprits: every **Black Locust variant is a ground-level plant** (0.5-1.5 m tall,
  up to 1.8 m wide) that the resource table calls a *Tree*, so it blocked like an oak -- and last night's collision pass had made it worse (trunk capsules on the variants, and the legacy PCG node `MOPCG_StampScatter1/StaticMeshSpawner_3`, which places locust A/B/C, set to `BlockAll`). Now every `wdvhberja` species is `Passable`
  (`OverlapAllDynamic`, still reached by interaction traces) via the one rule in `biome_palettes.py::species()`, and node 3 is `OverlapAllDynamic` (`fix_mesh_collision.py`). The enum stays: it is how a species overrides its resource type's default.
- **Megascans**: the local library (`Documents/Megascans Library`) held 6 unused rocks (Sand Rock, Stone, Mossy Forest Boulder -- tagged "tropical jungle" --, three Nordic rocks). Copied to `Content/Megascans/3D_Assets/<Name>/` (their internal references point there), reparented onto `M_MS_Base` with a flat ORM by
  `Content/Python/fix_megascans_import.py` (the export pointed at an `M_MS_Default_Material` this project does not have, so every MI had no parent), convex collision by `fix_mesh_collision.py` (now also covers `Collision = Solid` species). Placed on the shore, in the rainforest / woodland / highland. Driftwood = the existing Bent Branch, Spruce Branch and Broken Bark scans.
  **Nothing new can be downloaded here** (Fab / Bridge need an account): more beach packs (shells, dune grass, driftwood) are a download for you, then they drop into `biome_palettes.py`.

### Recruitment: why it did nothing, and what it does now
Diagnosis (layers): `UMOColonyManagerSubsystem::CanRecruitByStanding` gates a stranger on **trust built by shared game time** (2% per hour: ~8 game hours standing within 12 m), and its "bootstrap" test (`OthersOnRoster == 0`) never fired because the founder IS on the roster. On the real-time clock that is ~8 real hours before the first survivor will join -- and the gate
refused with `return false` + a log line, so the player saw an interaction that did nothing, no quest, no message. Fixes at those layers: the gate only judges a colony of >= `RecruitGateMinRoster` (3) settlers (the first survivors you meet just join);
`BeginInteractionDetailed` returns an `EMORecruitmentOutcome` (+ standing / needed / hours-to-pass computed by the bond function itself, `HoursToReachStanding`); `AMOCharacter::HandleRecruitmentInteraction` sends every outcome to the player through the new `AMOPlayerController::ClientShowNotification` (a Client RPC: works for a joined client).
Seen in PIE: a lone founder -> the first survivor "joins you"; a colony of 3 -> yellow notice "does not know your settlement well enough to join yet (trust 0% of the 15% needed). Spend about 8 more hours of game time near them." Tests: `MOFramework.Colony.Recruit.GateMath`.
No quest ever appears because no survivor has `PossibleQuests` configured (the component rolls one only when the list is non-empty): quests are a data task, not a bug.

### Follow-up the same evening: audio, editor exit, menu fade
- **"Exit Game" on the main menu closed the whole EDITOR from inside PIE.** The log of the session that lost its editor shows `Exit Game button clicked` -> `Cmd: quit force` -> `Engine exit requested (reason: Win RequestExit)` -> a normal shutdown, not a crash. `QuitGame(... bIgnorePlatformRestrictions = true)` issues `quit force`; in PIE `AMOMainMenuPlayerController::ExitGame` now calls `GEditor->RequestEndPlayMap()` and returns (packaged builds still quit).
- **The start-of-game blast was the intro video.** Its `UMediaSoundComponent` had no sound class, so it played at full volume and ignored the Music / SFX / Ambient sliders (Wes had 0.27 / 0.53 / 0.20 = a ~4x blast). It now plays at music x master.
- **My ambience driver flapped.** `FindWaterNear` sampled rings whose radius depended on the previous state (35 m to enter, 56 m to stay): with two rings per radius a narrow river or pool was hit by the 17 m ring and missed by the 28 m one, so a stationary listener flipped Water <-> Outdoor every second and every flip restarted every ambient layer (3 s crossfade each) -- a wall of overlapping sound.
  Now `DistanceToWaterCm` measures ONCE on fixed 7 m rings and the entry / exit distances are compared against that one number (a position settles in one step, test `MOFramework.Audio.AmbientEnvironment`), and a state is held `MinDwellSeconds` (4 s) before it can change.
- **Main menu fades in from black over `MenuFadeInSeconds` (2 s)**: a camera fade over the menu level plus the menu widget's own opacity, started the first time the menu is shown (after the intro the first time; straight away on every later visit, where the level streaming in is what it hides).

### Open items / forks
- The sand ripple look is a first guess (MI_Sand NormalStrength 0.6): tune by eye. The shore band above sea level is +4 m.
- AlpineHeath is still "everything above 320 m": with 1-2 km mountains that is a lot of gravel; a bare-rock / snow-cap biome for the top is the obvious next biome.
- The rainforest is temperate-looking trees in a dense layered stand (the packs have no palms / tropical broadleaf); real jungle variety needs a tropical pack.
- Seabed is walkable (no swimming): the sea floor runs out to the world edge at ~12 m depth, so the pawn can still wade out there; only LAND no longer meets the boundary.
- The main-menu flyby uses the same generator, so it now shows the new coast; its start point was chosen against v4 land (re-check the first frame).

## Update (2026-10-10, night): generator v4 — relief x2, coastal ranges, upland lakes, a coastal new-game start, fuller biomes

Wes's asks this session: spawn near an ocean (not an island), lakes above sea level (~1 per 5 km^2), `weather` / `time` console popups, "double the height delta", check collision of rocks and
other props, a flyby on the main menu, and keep making the land feel alive. What changed, why, and how it was measured (all uncommitted; the world generator is **v4**, goldens re-pinned).

### The land (FMOWorldGen v4)
| Piece | What | Numbers |
|---|---|---|
| `VerticalScale` (new tuning, default **2**) | multiplies the whole height sum (continents + mountains + coastal ranges + hills + detail), above and below sea level | `1` + `CoastalRangeAmplitudeM = 0` reproduces v1-v3 bit for bit (the `RunProbe(false)` golden pins it) |
| Coastal ranges (`CoastalRange*`) | ridged hills/mountains that rise from the shore (a band starting at continentalness -0.52, fading out where the continental mountains take over, with a slow presence field so only ~60% of coast has them) | exist because only 8 of 40 seeds had ground 1 deg above the horizon across open water; with them (and x2): **36 of 40** seeds, median 2.1 deg |
| Upland lakes (tier 2 of the basin code, `UplandLake*`) | one site per 1.8 km cell; a few jittered candidates roll downhill into the nearest low spot; the flattest one whose footprint is high (>= 19 m above sea, clear of the river zone), level and walled-in wins; **water surface = lowest ground around the lake minus a freeboard** (so the shore is always a dry bank, and a square water plane laid over the ellipse stays buried outside it); the ellipse is carved into a bowl | **677 lakes in a 60 km world = one per 4.9 km^2** (1 per 5.5 before the x2); radius 70-180 m; surfaces 17-241 m above sea |
| Sinkholes | keep out of lake zones (a hole in a lake bed) | test: 0 inside a zone, and the gate has work to do |

`MOFramework.WorldGen.UplandLakes` checks: density 3-8 km^2, surface above the limit, reach inside the cell (a column only asks its own cell), bowl cut to depth, dry bank all round, **ground >= surface + 20 cm everywhere
outside the ellipse inside the water plane's box** (the plane can never hover over a hillside), continuity, determinism, zone helper, sinkhole gate.

### Water surfaces for the upland lakes: `UMOUplandLakeSubsystem`
The world's water plane is at sea level, so a bowl in the hills needs its own surface. The subsystem spawns an `AWaterBodyCustom` of the level's ocean actor's class once per lake near any player
(spawn 4 km, despawn 5 km), laid at the lake's surface height and scaled to the ellipse, then gives it the ocean's mesh, material and underwater post process. Four traps found by looking (all fixed, all in the file header): **SpawnActor with the ocean actor as `Template` composes the spawn transform with the template's own root transform**
(the ocean's scale of 100000 made every lake 80 000x too big and flooded the horizon), **and copies its replication Role** (a level-placed actor on a client is a simulated proxy: "Actor has an invalid Role" ensure, found only by the packaged two-process net test), so the lake is a plain actor of the ocean's class;
the class default is **Static** mobility so the water body's runtime `SetStaticMesh` is refused (9 warnings per lake, no surface), so it is made Movable before `FinishSpawning`; and the ocean is
the source of the look, so a level without one gets bowls but no water (warned once). The same subsystem runs in the main-menu world, so the flyby sees lakes too. The PCG biome spawner skips points inside a lake's ellipse
(`IsInUplandLakeZone`); the legacy native Static Mesh Spawner nodes in the scatter graph are not lake-aware (trees can stand in a lake from those).

### New-game start: the shore, facing the water, never on an island (`FMOWorldGen::FindCoastalSpawn`, `AMOGameMode::FindNewGameSpawn`)
1. Coarse grid (400 m cells, +-24 km, base height only) -> shore cells (land next to water) -> 16 rays each: unbroken open water (fetch) and the far ground's **elevation angle** (height above the eye / distance, within 6.4 km behind the water); a view needs the +-22.5 deg
   neighbours to be open water too (rules out rivers/inlets/ponds). Score = skyline + fetch - a mild pull toward the origin.
2. **Not an island**: 4-connected land components of the coarse grid (>= 600 km^2, or >= 30% of that when touching the grid edge), then the picked spawn is flood-filled on the REAL terrain at 200 m and again at 300 m
   (`MeasureLandmassCells`, a neck one cell wide at 200 m is cut by the coarser lattice): both must reach 600 km^2. The spawn also needs 10 of 16 dry points on 150 m / 300 m rings (not a spit), water within 400 m ahead and 85% water over the next 1.5 km at 25 m steps, slope < 26 deg, no sinkhole / lake.
3. Costs 5-30 ms. The pawn is spawned at generator height + 200 cm (the generator matches the voxel surface to 0.5 cm), held by the existing spawn-settle (45 s allowance for a coastal start because the voxel world has to stream somewhere the loading screen never looked), and the control rotation faces the water.
`MOFramework.WorldGen.CoastalSpawn` (40 seeds): 40/40 found, distance p50 12.8 km, open water p50 4.4 km, 36/40 with >= 1 deg skyline, no island by an independent 250 m cross-check, deterministic.
`MO.WorldGen.FindCoast [go]`, `MO.WorldGen.SeedSurvey [first] [count]` (ranks seeds by forest + inland hills + skyline: seeds 7, 18, 19, 49 are the best of the first 120).

### Main-menu backdrop (`AMOMenuFlyby`, LoadingLevel)
LoadingLevel (the menu map; the code finds it by name) is no longer empty: the terrain stack of the game level (voxel world + MO terrain graph, cave stamp, sky, weather, ocean, PCG volume) was copied in with the editor's copy/paste
and one `AMOMenuFlyby` placed. At BeginPlay it applies a seed (`UMOWorldSeedSubsystem::ApplyBackdropSeed`, default 7), asks the generator for that seed's new-game coast, lays a 23 km loop along the shore and back through the land (shifted until
~70% of it is over land), becomes the view target and flies it at 9 m/s, 70 m over the ground (the generator knows the ground; it looks ahead 150 m so it rises before a ridge), looking a little toward the sea. A lap takes ~44 minutes. **The flight starts at the first point of the loop with ground under the camera and open water ahead** (seed 7: 13.6 km along it) --
the first version started at distance 0, which for seed 7 is over the sea: the packaged build's menu opened on bare water for minutes at 9 m/s, and its biome spawner logged `accepted=0 suppressed=N` for every cell (points under the sea are dropped by design). Found by launching the packaged exe and looking, not by the editor frames (which had been captured at chosen fractions).
Traps: the spline must be absolute (attached, it rode the camera and ran away exponentially: 390 km from the origin in a minute), dedicated servers skip it, `QUIT_EDITOR` (not the window close box) leaves the editor without the save prompt the generated menu level triggers.
Dev: `unreal.MOMenuFlyby.jump_to_fraction(0.3)` through the editor Python bridge moves the camera for screenshots.

### Biomes (DT_Biomes as data: `Content/Python/biome_palettes.py`)
The forest was three tree meshes and one grass; the meadow two grasses; the world used two of ~110 meadow flowers. Palettes now use the packs' growth stages (BlackAlder canopy -> saplings -> seedlings, CommonHazel shrubs), 8 wild-grass and 6 dune-grass variants, dead shrubs, raspberry
and locust variants, 10-15 flower families per biome. **New biome AlpineHeath** (height >= 320 m, gravel, boulders, stunted trees, tough grass, late flowers) for the doubled relief. Instances/ha: forest ~1270, meadow ~2180, woodland ~840, scrub ~1020, shore ~720, highland ~430, alpine ~790.
`MO.WorldGen.BiomeStats` (share of the land per biome) and `MO.WorldGen.FindBiome <id> [go]` (teleport to the nearest) are the tools for tuning; seed 12345: woodland 25%, forest 23.5%, scrub 16%, meadow 8.4%, shore 4.7%.

### Collision of scattered things (found because a rock "had no collision")
Cause, by layer: **the biome spawner gave EVERY species the node's single `CollisionProfile`, which the graph set to NoCollision**, so every tree and rock it placed was walk-through. Fix at that layer: harvestable species take
`FMOResourceNodeDefinitionRow::GetPlacementCollisionProfile()` (tree / rock / ore / generic -> BlockAll; bush / herb -> OverlapAllDynamic), only decorative species (no ResourceNodeId: grass, flowers) use the node's `DecorativeCollisionProfile`
(NoCollision). Also fixed: the spring POI's boulders and the locust woodland's native spawner entries (NoCollision / IgnoreOnlyPawn -> BlockAll), and two locust variants got a trunk capsule. **Trap: `EditorStaticMeshLibrary` collision counts read -1 while PIE is running**, which first made every mesh look collision-less; the
meshes had convex hulls all along. Proof it works: `MO.Dev.ScatterCollision [filter]` sweeps a pawn capsule into spawned instances (grass control: blocked = 0, boulders / trees: all blocked);
`MOFramework.Data.SolidMeshCollision` fails if a solid-kind mesh in the biome table has no collision geometry; `Content/Python/audit_table_mesh_collision.py` / `fix_mesh_collision.py` / `audit_mesh_collision.py`.

### Weather that changes by itself (`UMOWeatherDirectorSubsystem`) and the dev popups
The director holds a weather for a spell of GAME time, then follows a Markov chain over the UDS presets (45% fair / 13% wet / 0.5% thunderstorm over a long run); a wet front falls as snow below ~0.5 C. It respects a weather set by anyone else (adopts it, holds it 6 game hours). `MO.Weather.Director 0|1` (cvar), `.Info`, `.Next`.
**The default clock is real time, so a spell is hours of real time unless the time scale is raised** (`MO.Clock.SetTimeScale 24` -> a day per hour): the one-line change that makes day/night and weather visibly dynamic is a design call left for Wes.
Typing `weather` / `time` in the Tilde console opens a quick-pick modal (`UMOQuickPickDialog`, `WBP_QuickPickModal`, built from `Content/Python/ui_specs/quick_pick.py`); the buttons call Server RPCs on the player controller (works from a joined client). `weather rain` / `time 2pm` skip the popup.
`time` moves the clock FORWARD to the next occurrence (`UMOGameClockSubsystem::AdvanceToTimeOfDay`; time never rewinds, so nothing timed on the DateTime sees a negative age).

### Ambience that follows the ground (`UMOAmbientEnvironmentSubsystem`)
The audio subsystem had Cave / Water layer states nothing ever set. A local-player ticker (0.75 s) now classifies the listener from the generator: **Cave** when more than 12 m under the generator's surface (leaves at 8 m; a sinkhole shaft counts), **Water** within 35 m of
water (sea-level water by the generator's height, or an upland lake's ellipse; leaves beyond 56 m; not when the listener is >30 m above the surface), else back to the clock's Day / Night. Indoor is left alone (a building decides its own sound). Pure `Classify()` is tested (`MOFramework.Audio.AmbientEnvironment`, shore found by scanning seeds, expectations built from the generator's own heights).
New data row `Ambient.Cave` in `DT_AmbientLayers` (calm hollow + drip loops; drip / bat / air event groups from the existing cave sounds). Seen in PIE: shore -> `Outdoor_Day -> Water` (3 base / 4 event groups), falling down a sinkhole -> `Water -> Cave` (2 base / 3 groups, dominant Bats), back up -> `Outdoor_Day`.

### Open items / forks
- Swimming: the water planes (ocean and lakes) still do not make the pawn swim.
- The legacy native PCG spawner nodes (grass/locust/field trees) are not lake-aware or biome-aware; the biome spawner is where new variety belongs.
- Lakes are static levels (no flow, no seasons); rivers are sea-level channels only.
- `VerticalScale` 2 doubles every slope: re-tune anything that assumed the old relief (biome bands are in real metres; `AlpineHeath` starts at 320 m).

## Geology: bedrock, ores, and what lives in caves (2026-10-17)

Wes: "I want there to be PCG inside caves, rocks, rocky materials on walls and ceilings, rock and dirt floors, different ores can also be found, coal,
iron, copper, etc -- check what is actually realistic in terms of where to find different ores though, I still want realism."

**Where ores really are** (checked Oct 17; sources at the end of this section):

| Ore | Where it occurs in the real world | How MO57 models it (`MOWorldGenGeology.cpp`) |
|---|---|---|
| Coal | SEAMS: beds inside sedimentary rock (sandstone, shale -- the "coal measures") of basins and foothills; laterally continuous, 0.3-2 m thick, dipping gently, pinching out | a bed per 16 m of strata (each at its own height in its cycle), 40-220 cm thick, a ~1 degree regional dip, present where a 400 m field opens -- in `CoalMeasures` bedrock only |
| Iron | hematite filling joints and fissures of LIMESTONE (northern England's filled whole fissures and caverns); bog iron (limonite) precipitating in wetlands; magnetite in igneous rock | hematite: vertical sheets along one joint set (~60 m apart, 30-120 cm wide), mineralised in patches -- in `Limestone`. Bog iron stays a SURFACE ore (the biomes' dig tables) |
| Copper | veins in basic igneous rock (basalt, gabbro) and at granite margins; native copper and malachite where they weather; also red-bed sandstones | thin 3D vein sheets, sparse -- in `Basalt`, rarer in `Granite` |
| Tin | cassiterite tied to GRANITE; placers in rivers draining it | 3D vein sheets in `Granite` (placers: future surface work) |
| Flint | chert nodules in thin beds of limestone and chalk | a 25 cm bed per 6 m of strata with nodules in it -- in `Limestone` |
| Lead | GALENA in limestone and dolostone (Mississippi Valley-type: the Pennines, Derbyshire, the Upper Mississippi), filling fractures, breccias and old solution cavities; galena speleothems in caves | a second joint set (~90 m apart, 20-70 cm wide), mineralised in rarer patches than the hematite -- in `Limestone` (Oct 17, not yet measured; the test asserts it is present and rarer than hematite) |
| Caves | solution caves form in LIMESTONE (karst); clay / mud and breakdown rubble on their floors | galleries are not limited to limestone yet (open fork below); floors carry rubble and gravel |

**Bedrock** (`FMOWorldGen::SampleBedrock`): regional provinces (~7 km fields), sharp contacts -- volcanic provinces are `Basalt`; the continental
interiors (continentalness > 0.5) and one province band are `Granite`; the rest is sedimentary, `Limestone` or `CoalMeasures`. Measured
(`MOFramework.WorldGen.Geology`, 60 x 60 km): limestone 41%, coal measures 24%, granite 19%, basalt 16% of the land; 97.9% of points 100 m apart share
their rock. Ore share of each rock in the cave band: coal 5.9% of the coal measures; hematite 0.44% + flint 1.4% + galena 0.14% of limestone; tin 0.38% + copper 0.18%
of granite; copper 0.62% of basalt. Coal is a bed: 98% of coal points continue 2 m sideways, 0% are still coal 3 m up. The test also asserts every ore
sits in its host rock.

**What lives in caves** (`MO Cave Spawner`, `UMOPCGCaveSpawnerSettings`, on the GRID16 tier of `MOPCG_StampScatter1`, fed by `WaitForVoxelWorld_0`):
the node finds the cave surfaces under its 64 m cell itself (`FMOWorldGen::FindCaveSurfaces`: a column per metre, sphere-traced up through the cave
band, crossings refined, normal from the field's gradient, floor / wall / ceiling by the normal; `MOFramework.WorldGen.CaveSurfaces`), asks the rock
30 cm behind each point what it is and holds, and plants the rows of `DT_CaveScatter` (`FMOCaveScatterRow`: surface, bedrock, ore + minimum grade,
density per m2, mesh, material, scale, align, sink, resource node, collision). It IS the biome spawner's build (subclass; only the planting differs), so
ore rows are real harvest targets (DT_Resources: `CoalSeam` -> `Coal01`, `IronOreDeposit`, `CopperOreDeposit`, new `TinOreDeposit`, `FlintDeposit`) and
removals persist (the removal ledger now keys a 1 m height, so a quarried floor stone no longer takes the ceiling rock above it). The rows: gravel /
eroded-rock / sandstone floor skins, stones, fallen blocks (solid), rock lumps on walls and ceilings (pale limestone on the sand-rock scan -- a tint only
multiplies a scan's colour, the dark Nordic rock came out black), and the ore lumps (coal black, hematite red, malachite green, cassiterite dark,
chert nodules on the flint scan). Cost: 5-14 ms of surface finding per 64 m cell with caves. Dev verbs: `MO.WorldGen.Bedrock`, `MO.WorldGen.FindOre
<Coal|Hematite|Copper|Tin|Flint|Any> [radius]`, `mo.PCG.CaveDensityScale`.

Wes's answers (Oct 17): **caves only in limestone** -- generator v11, `bCavesOnlyInLimestone` (default on): the 16 m cave-gate lattice that does the water
keep-out also closes every node whose bedrock is not limestone, so a contact tapers over a few metres; `WorldGen.CavesStayDry` asserts >= 99% of cave
air in limestone; entrances measured per km2 of karst hillside (~11). Coal, copper and tin no longer show in caves -- they are reached by DIGGING:
**mining yields from the geology** -- `FMOWorldGen::SampleDig` (soil within `SoilDepthCm` -- 250 cm on the level to 40 cm on 35 deg slopes -- of the
unworked ground, else the bedrock or the ore body there) and `FMOGeologyYield` (the one map to items: Dirt01 / Limestone01 / Sandstone01 / Granite01 /
Basalt01 / Coal01 / IronOre01 / CopperOre01 / TinOre01 / FlintNodule01; skips points in air or cave air) feed BOTH dig paths -- the player's dig tool
(`UMOTerraformingComponent::GrantDigYield`: incremental steps accumulate the volume, granted at completion or on an interrupt; the atomic path at
once) and the survivors' ExcavateAndHaul job (earth and stone fill the fill zone; ore is kept). Test `WorldGen.DigYields`. And "they should be able to
collect pretty much any ore, the actual hurdle with iron is having fire hot enough": `IronOreDeposit` takes a stone pick (0.5) like copper; the heat
ladder (station temperature, fuel, bellows) is researched in `Docs/Technology_History.md` and waits on Wes + Codex's station-context work.
Galena (Oct 17, after Wes went to bed): `EMOOreKind::Galena` -> `GalenaOre01`, which also gives the GlazePottery chain a producer: powdered galena
is how medieval lead glazes were made (the `MineralGlaze01` it needs had none). Rows authored (4b1e1265). In game: GalenaOutcrop / GalenaWall nodes plant in limestone; mining one ("GalenaDeposit: MineOre", stone maul) gave 2 GalenaOre01, and GrindGalenaGlaze made 2 MineralGlaze01. Carried light:
`Docs/Light_Sources.md`.
Still open: per-bedrock / floor voxel surface types; cave exposure (measure first); geology-placed surface outcrops (coal at seam
outcrops, malachite stains, tin placers) to replace the biome-placed copper / flint deposits.

Sources: [coal and iron in sedimentary layers; copper and tin in igneous veins](https://dspmuranchi.ac.in/pdf/Blog/PROSPECTING%20CRITERIA%20OF%20VARIOUS%20MINERAL%20DEPOSITS.pdf),
[hematite in limestone fissures and caverns (Ramsay 1878)](https://geology.19thcenturyscience.org/books/1878-Ramsay-Geology/PDFpages/1878-Ramsay-Geology-0596.pdf),
[USGS: copper in sedimentary and volcanogenic rocks](https://www.usgs.gov/publications/copper-deposits-sedimentary-and-volcanogenic-rocks),
[bog iron formation (Britannica)](https://www.britannica.com:443/print/article/1365546), [galena in limestone (Britannica)](https://www.britannica.com/science/galena-mineral),
[galena speleothems in Upper Mississippi Valley caves (NSS Bulletin)](https://legacy.caves.org/pub/journal/PDF/V41/v41n1-Peck.htm).

## Update (2026-10-10): the level runs on the generator; caves, water and below-world light

**State: all of this is uncommitted work on top of commit `5b9461f6` (the rollback point).**

| Piece | Where | State |
|---|---|---|
| The game level `MOPCGScattering` uses `VHG_MOTerrain` (was `VHG_Flat`) | the height stamp's Graph (set from python: `get_stamp().export_text()` -> replace the graph path -> `VoxelStampRef.import_text` -> `set_stamp`) | done; `MO.WorldGen.Compare 3000 500`: 121/121 hits, mean 0.1 cm, max 0.4 cm |
| **`VHG_MOTerrain` SurfaceType pin wired** (it was not: the whole terrain rendered as the grey WorldGrid checker) | graph | done |
| Biome ground surfaces | `DT_Biomes.groundSurfaceType`: forest/meadow = `VST_MI_MuddyLeaves`, shore/scrub = `VST_MI_Gravel`, rocky = `Cliff/VST_Cliff` | done (RockyHighland cannot win in the node: slope is assumed 0; slope rock needs a Smart Surface Type) |
| Three new biomes from the Megascans plants/rocks in `Content/Fab`: `ShoreStrand`, `DryScrub`, `LocustWoodland` | `DT_Biomes` (6 rows, 23 species, `MO.Test.ValidateData` green) | done |
| **Caves**: spaghetti tunnels, caverns, sinkholes | `FMOWorldGen::SampleCaveDistanceAtGround`, `MOWorldGenCaves.cpp`; nodes `MO Terrain Caves` + `MO Terrain Caves Bounds`; graph `Content/Penumbra/Maps/VVG_MOCaves`; stamp actor `MOCaveStamp` (Subtractive, DefaultVolumeLayer) | done, verified in game |
| **Rivers + lake/pond basins** carved into low ground at sea level | `FMOWorldGen::ApplyWater`, `MOWorldGenWater.cpp` | done, verified in game |
| Below-the-world light gate | `UMOSubterraneanLightingSubsystem` | done, verified A/B |
| Determinism probe (the "cpu check") | `FMOWorldGen::RunProbe`, `MO.WorldGen.Checksum`, pins in `MOWorldGen.h` | editor verified; packaged / second PC still to compare (one console line each) |
| Dev tools | `MO.Cam.LookAt/Reset`, `MO.Light.Test/Cube/Off`, `MO.WorldGen.Sinkholes/FindCave/FindWater/CaveSample/Checksum` | done |

### Caves

Shape: three feature types, all a signed distance in cm (negative = cave air) that a **Subtractive volume stamp** carves out of the height terrain.
- **Tunnels**: `sqrt(A^2 + B^2)` of two independent 3D gradient-noise fields is small along the curve where both are zero: a winding passage ~3.2 m radius (varies +-35%), flatter than
  tall (vertical squash 1.6). Distance scale from the measured noise (`|gradient| ~1.2 per lattice cell`).
- **Caverns**: a smooth 3D field above a threshold, at least 35 m down.
- **Sinkholes**: one candidate per 300 m cell (60%), a shaft that flares at the rim, wobbles +-25% and ends in a flattened chamber. The ONLY features that reach the surface (tunnels and
  caverns keep 9 m of rock above them and taper in over 16 m). `DistanceToSinkholeRimZone` lets PCG keep clear.
- Where: only under ground more than 3 m above sea level (6 m ramp), so the lowland spawn has none; caves live inland / on hills. Not deeper than 140 m below the ground.
- **Every gate is a continuous penalty that saturates at `CaveFarCm` = 3000 cm.** The first version closed its gates with `return Far` and had a 146 cm/cm cliff in the field; the test now
  measures `max |gradient|` (it is ~13 where the 16 m taper ramps, never 100+) and the mean gradient at a cave wall (0.97, ~unit, which is what marching cubes interpolates on).
- LOD: features smaller than the sample spacing are skipped (tunnels beyond LOD 1, caverns beyond LOD 4, sinkholes beyond LOD 3), so far chunks see solid ground.
- Cost (single thread, Development editor, 34^3 chunk): **12.6 ms underground, 4.3 ms in the air** (a per-call column memo shares the 2D ground height across the Z layers). Cave air is 0.87% of the rock
  between 10 and 120 m down; 156 sinkholes per 16 x 16 cells (4.8 km square).
- The sample's `VVG_CaveTunnels` / `VVG_CavePockets` (`D:/UEProjects/CavesShadowsTest`, the old Voxel 2.0 `CanyonsAndCaves` content) showed the technique: 2D simplex iso-lines as tunnel paths, a Chebyshev box
  distance, 3D Perlin surface warp, `Make Box` bounds. We keep the idea (zero-contour tunnels, Subtractive stamp) but compute it in C++ so it shares the terrain's seed, ground height and tests.

Wiring (a graph, ~8 nodes, all of it adapter): `Get Position 3D (double)` (World space) -> `MO Terrain Caves.WorldPosition`; the SAME `Seed` parameter (type Seed, named exactly `Seed`) -> `.Seed`;
`.Distance` -> `Output Volume.Distance`; `MO Terrain Caves Bounds` -> `Output Volume.Bounds`; `Output Volume.Surface Type` default = `VST_Cliff` (the cave walls). The stamp actor is a
`VoxelVolumeGraphStamp` with Blend Mode **Subtractive**, layer `DefaultVolumeLayer`, identity transform. `UMOWorldSeedSubsystem::ApplySeedToStamps` seeds every graph stamp (height AND volume)
through `IVoxelParameterOverridesOwner`; seeding only the height stamp would carve caves that do not line up with the ground.

PCG: the biome spawner skips points inside a sinkhole's rim zone and points more than 2 m below the generator's ground (cave walls/floors), and (water) points below sea level.

### Water

Rivers and lakes need **no water actor and no flow**: the world already has a water plane at sea level (`WaterBodyCustom_0`, scale 100000), so everything below `SeaLevelCm` is under water. A river is a
channel whose bed is below sea level; a pond or lake is a bowl. They sit at sea level, in lowland only.
- **Rivers**: the zero contour of a smooth 2-octave noise field (`distance ~ |field| / |gradient|`, gradient floored so flats do not swell the river), width 24 m +-30%, 1.3 m deep at the centre,
  banks rising 9 degrees. They fade out as the ground rises past 12 m (+6 m) above sea level: no headwaters are carved.
- **Basins**: one candidate per 600 m cell (ponds, 15-48 m radius) and per 2 km cell (lakes, 60-115 m), elliptical, only where the WHOLE rim (16 samples of the base terrain) sits 0.6-11 m above sea
  level; a bowl `sea - depth * (1 - q^2)` inside the shore, a bank `sea + 0.22 * R * (q - 1)` outside. `height = min(base, river, bowls)`: water only ever lowers ground.
- Cost: nothing above 12 m of ground is touched (the early-out); basin rim checks are memoised per thread. `MinHeightCm` (the stamp's HeightRange) accounts for the deepest basin.
- The base terrain is pinned separately: `RunProbe(false)` (water off) must equal the v1 base golden `0x9C4AE2B3` (a test); everything else is `GeneratorVersion` 3.
- Seen in game: a turquoise lake with a gravel beach and forest behind it (Shore biome = gravel surface); rivers read as 24 m channels. 1.4% of low land is water with the first defaults, rivers 0.7%
  (`MOFramework.WorldGen.Water` prints it). Rivers/lakes are flat sea-level water (no current, no elevated mountain lakes): flowing water and lakes at altitude need their own water bodies (the project already has
  `AMOLakeActor` / `AMOWaterActorBase` Gerstner actors without a material, and the Water plugin's `WaterBodyLake`, which auto-creates a `WaterZone` -- neither is used yet).

### Below-the-world light (Lumen "sun beneath the world")

Measured with `MO.Light.Test <pitch> <yaw> <lux>` (a 20 000 lux directional light you place) in a sinkhole chamber:
- a light from **above** (pitch -50) leaves the chamber dark (blue sky bounce only) and an enclosed tunnel is **pitch black at noon**: terrain blocks sun and sky correctly, no leak;
- a light from **below** (pitch +40) lights the walls like direct sun, with shadows ON or OFF identically.
It was NOT fixed by: the mega material's `bGenerateTwoSidedMaterial` (the generated Nanite material verifiably was two-sided), `VoxelWorld.ComponentSettings.bCastShadowAsTwoSided`,
non-Nanite voxel meshes, virtual shadow maps off, or a 180 km down-facing shadow quad 200 m under the caves. (The engine's Nanite shadow raster takes two-sidedness from the material only; the one-sided
voxel surface has no bottom to be a front face for a light below.) The fix is `UMOSubterraneanLightingSubsystem`: every directional light's intensity is attenuated by how far below the horizon it points
(full at the horizon, zero at 5 degrees below), keeping the intensity Ultra Dynamic Sky writes. Verified A/B in the same view: `MO.Light.BlockFromBelow 0` -> lit rock, `1` -> black.
Traps: `ULightComponent::bAffectsWorld` cannot be changed at runtime; `SetVisibility` is overwritten by the sky blueprint.

### The "cpu check" (determinism across machines and builds)

Co-op worlds are generated on each machine from the seed, so two machines must produce identical bits. `MO.WorldGen.Checksum` prints the fixed fingerprints (heights, caves) and `match=1` if they equal
the pinned values (`FMOWorldGen::PinnedProbe*`). Editor / Development / i9-14900K: `height=0xA8159C7B cave=0x2DB9B6B5 version=3 match=1`. The generator uses only IEEE-exact operations and
integer hashing, so a given binary is deterministic on every CPU; the checks that remain are *different binaries* (packaged client vs editor: run the command in each) and a second physical PC.
Do the second one by running the packaged build there and typing `MO.WorldGen.Checksum` in the dev console.

## Implementation status (2026-10-09, later the same day)

**Built and verified end to end (S0, S1 and the PCG half of S2):**

| Piece | Where | State |
|---|---|---|
| Pure generator (climate, height, biome; deterministic, IEEE-exact ops only) | `MOFrameworkCore/Public/MOWorldGen.h`, `Private/MOWorldGen.cpp` | built, 9 automation tests green |
| Tuning (Project Settings -> MOFramework -> World Generation) + snapshot builder/publisher | `MOWorldGenSettings.{h,cpp}` | built; published at module start, before every voxel runtime (`RegenerateVoxelWorld`), and by the PCG spawner |
| **Voxel nodes `MO Terrain Sample` and `MO Terrain Bounds`** | `MOTerrainFunctionLibrary.{h,cpp}` | built, registered, found by the graph editor palette (MO -> Terrain), **wired in a real graph: `Content/Penumbra/Maps/VHG_MOTerrain`** |
| **Live check: the real game's terrain IS the generator** | `MO.WorldGen.Compare` / `MO.WorldGen.Sample` (MOCheatSubsystem) | with the level's stamp pointed at `VHG_MOTerrain` (temporary, reverted): new game, world seed 12345 -> terrain seed -524743771; **169/169 voxel-collision samples within 0.5 cm of `FMOWorldGen`, mean 0.1 cm**; PCG spawner ran on the same biome map |
| PCG biome spawner on the shared generator (`bUseWorldGenerator`, default on; legacy noise kept behind the flag) | `MOPCGBiomeSpawnerSettings.{h,cpp}`, `UMOBiomeDatabaseSettings::ResolveBiomeAtWorld` | built; live P2 biome gate (`Content/Python/test_biomes_p2.py`) 5/5; packaged build smoke passed |
| PNG map dump (look at a whole world headless) | `MOFramework.WorldGen.DumpMaps` -> `Saved/WorldGen/*.png` | built |
| Per-biome ground material | `FMOBiomeDefinitionRow::GroundSurfaceType` (new, empty) | built; needs surface-type assets authored in the editor |

**Found only by wiring the graph (unit tests could not see it):** the stamp evaluates `OutputHeight.Bounds` and `.HeightRange` once at initialisation, in a context with
NO position. They were first outputs of the position-driven node, and the graph reported "Get Position 2D (double): Cannot query positions here". They are now a separate
no-input node, `MO Terrain Bounds`; the node test asserts it has no input pins.

**Status when this section was written (all of (1)-(3), (6) are done in the 2026-10-10 update above):** (4) smooth per-biome material cross-fades are still open; (5) slope-banded biomes are
resolved only by the PCG spawner (the node assumes flat; see `MOWorldGen.h` PITFALLS), so slope rock needs a Smart Surface Type graph.

**Defaults I chose (design forks, section 10):** heightfield base + (later) volume carving; normalised 0..1 moisture/temperature with a lapse
rate (physical units deferred -- the existing DT rows use 0..1); the existing priority-ordered band rows kept for biome resolution (nearest-match
cells deferred); generator in `MOFrameworkCore`; own C++ noise.

### Using it

`VHG_MOTerrain` already exists (Seed parameter -> `MO Terrain Sample`; `Get Position 2D (double)` in World space -> `World Position`; `MO Terrain Sample.Height` ->
`Output Height.Height`; `MO Terrain Bounds` -> `Output Height.Bounds` and `.Height Range`).

1. Select the level's height stamp (`VHG_Flat Max DefaultHeightLayer 0 HeightGraph` in `MOPCGScattering`), set **Graph** to `VHG_MOTerrain`, save the level. Its transform must stay
   the identity (positions in, heights out are absolute world values).
2. Verify in the real game: `python Tools/ue.py boot --seed 12345`, then `python Tools/ue.py run "MO.WorldGen.Compare 1500 250" --grep MOQUERY` -> expect `hits=169/169` and
   `maxDiff` under a voxel (it was 0.5 cm).
3. Look at a world without launching anything: `python Tools/ue.py auto --filter MOFramework.WorldGen.DumpMaps`, then open `Saved/WorldGen/world_seed12345_relief.png`.
4. Tune in Project Settings -> MOFramework -> World Generation (restart PIE to regenerate).

To build such a graph again by hand: Add -> Voxel -> Stamps -> Voxel Height Graph; delete the pre-seeded Advanced Noise 2D + Make Box nodes; drag from the `World Position` pin
and pick `Get Position 2D (double)` (set World); right-click the node's `Seed` pin -> Promote to parameter (the parameter must be named exactly `Seed`); add `MO Terrain Bounds`.

### How PCG sees the biome (the question that shaped the build)

The vegetation spawner does **not** re-derive biomes from the terrain it samples; it asks the same functions the node uses (`FMOWorldGen::SampleClimate` +
`TemperatureAtHeight` + the DT band rows) with the same terrain seed (`UMOWorldSeedSubsystem::GetActiveTerrainSeed` = CRC32 of the graph's seed string).
So what the terrain paints and what the spawner plants are one answer by construction, and species palettes stay in `DT_Biomes` (data, not graph).
Stock PCG graphs can still read the terrain: the node's `SurfaceType` blend appears as the Voxel Sampler's per-point material weights, and its
`BiomeIndex`/`Moisture`/`Temperature` outputs can be written to float metadata and listed in the sampler's *Query Metadata*.

---

**Original exploration (written before the build; kept as the rationale).** Every claim below is tagged by how I know it: **[read]** = I read it in the
vendored plugin source (path given), **[recalled]** = from general knowledge, **[unverified]** =
needs the spike in §9. Companion docs: `Voxel_Plugin_Reference.md` (how the plugin fits together),
`Terrain_Foundation_Plan.md` (the graph-based plan this would replace), `World_Features_Architecture.md`
(caves/rivers/POIs that sit on top of whatever base terrain we generate).

---

## 1. The answer

**Yes, and from reading the source it needs no plugin patch** (one small spike, S1 in §9, confirms it end to end).
The Voxel plugin has a public, reflection-based extension point meant for exactly this: subclass `UVoxelFunctionLibrary` in our own module, write `UFUNCTION`s that take
and return voxel buffers, add one `VOXEL_REGISTER_FUNCTION` line per function. Each function becomes a
node in the Voxel graph editor. A node `MO Terrain Height (Position2D, Seed) -> Height, SurfaceType,
BiomeId` makes the whole height graph four nodes that never change; a sibling `MO Terrain Density
(Position3D, Seed) -> Distance` does the same for volume graphs. All terrain logic then lives in C++ and
DataTables (diffable, unit-testable, scriptable by Claude), and the graph asset becomes a stable adapter.

**The bigger win is not the node, it is the layer under it.** Write the generator as a *pure C++
function library* (`(Seed, X, Y[, Z], LOD) -> climate, biome, height, density, surface`), with the Voxel
node as a ~200-line adapter. Then the same function serves PCG scatter, audio, weather/temperature, AI,
spawn placement, tests, and a PNG map dump so we can *look* at a whole world without launching the game.
The code already says this is the goal: `MOBiomeDatabaseSettings.h:57` — "the mask function; the PCG
biome spawner and any query path must share it or they drift".

**Recommendation for the biome model:** do not copy Minecraft's *look*, copy its *architecture* (§5):
continuous climate fields drive both the terrain shape and the biome label, so mountains always get
mountain biomes and borders are never seams — and express the climate in physical units (°C, mm/yr,
lapse rate with altitude), which is also what Engineering Principle #11 (realism) asks for.

---

## 2. What the plugin offers (three extension points, ranked)

| # | Extension point | Evidence | Fit |
|---|---|---|---|
| **A** | **`UVoxelFunctionLibrary` UFUNCTION nodes** (CPU, buffer in -> buffer out) | `VoxelGraph/Public/VoxelFunctionLibrary.h:37-67,147-151`; examples `FunctionLibrary/VoxelPositionFunctionLibrary.{h,cpp}`, `VoxelBasicFunctionLibrary.cpp:24-69` (plain C++ loops over buffers, reads LOD) **[read]** | **Recommended.** Smallest surface, full graph benefits (preview, LOD, dependency tracking, blending, `OutputHeight`/`OutputVolume`, metadata, surface types). |
| B | `FVoxelNode` subclass with `Compute()` (pins, async `TValue`, ISPC) | `Public/Nodes/VoxelAdvancedNoiseNodes.h:33-68`, `Private/Nodes/VoxelAdvancedNoiseNodes.cpp:40-134` **[read]** | Only if we need async waits / variadic pins / ISPC kernels. More machinery than A for no gain at first. |
| C | **Custom stamp type**: subclass `FVoxelHeightStamp` + `FVoxelHeightStampRuntime::Apply(FVoxelHeightBulkQuery/SparseQuery)` | `Voxel/Public/VoxelHeightStamp.h:18-103`, `VoxelStampQuery.h:217-326,446-554`, reference impl `Heightmap/VoxelHeightmapStamp.cpp` **[read]** | No graph at all, but we would own blend modes, smoothness, bounds, surface/metadata plumbing, dependency collection. Keep as the escape hatch if A hits a wall. |

Discovery is by reflection (`GetDerivedClasses<UVoxelFunctionLibrary>()`, `GetDerivedStructs<FVoxelNode>()` in
`VoxelFunctionLibrary.cpp:51`, `VoxelSourceParser.cpp:528-545`) **[read]**, so a game module's library should be
found without registration code beyond the macro (palette + packaged behaviour **[unverified]**, S1). `MOFramework.Build.cs:87-90` already depends on `Voxel`,
`VoxelCore`, `VoxelGraph`, `VoxelPCG`, and `MOWorldSeedSubsystem.cpp` already includes `FVoxelExposedSeed`,
so the headers are reachable today **[read]**. The editor-only `VoxelSourceParser` writes tooltip/default
metadata for new nodes into `Plugins/Voxel/Source/VoxelGraph/Private/VoxelSourceParser.json` (a gitignored
vendored path; `#if WITH_EDITOR`) — harmless, but expect that file to change when the editor first sees our node **[read]**.

Release notes show the function-library mechanism changed recently (`6c24e4997`: C# UHT plugin replaced by
`VOXEL_REGISTER_FUNCTION`) **[read]**, so keep the adapter thin and behind the existing `MOVoxel` facade
pattern: a re-vendor should cost minutes, not days.

**Noise is not callable from C++.** Voxel's noise exists only as ISPC (`VoxelNoiseNodesImpl.isph`,
`VoxelAdvancedNoiseNodesImpl.ispc`) **[read]**. A pure-C++ generator needs its own noise (integer-hash gradient or
simplex; ~150 lines, deterministic). The existing `ClimateNoise` uses `FMath::PerlinNoise2D` — fine for a mask,
too weak for terrain.

---

## 3. How evaluation really works (facts that constrain the design)

All **[read]** in `Voxel/Private/Graphs/VoxelHeightGraphStamp.cpp:189-252`, `VoxelVolumeGraphStamp.cpp:173-234`:

- **Batched, not per-sample.** A stamp builds a *buffer of positions* for a whole bulk query (a grid:
  `Start`, `Step`, `Indices`, `StrideX`) and evaluates the graph once; our function is called with arrays.
  Render chunk = 32 voxels, `VoxelSize` = 100 cm (`VoxelWorld.h:35,222`), so a chunk is ~33² height samples
  or ~33³ (~36k) volume samples at *every* LOD — LOD multiplies `Step` by 2^LOD. Exact padding/neighbour
  margins **[unverified]**.
- **LOD is a query parameter** (`FVoxelGraphParameters::FLOD`, read via `Query->FindParameter<...>()`, or the
  built-in `Get LOD` node). A generator should skip octaves whose wavelength is below ~2x`Step`: coarse LODs
  get cheaper *and* stop aliasing.
- **Positions are doubles in stamp-local space unless asked otherwise.** `GetPosition2D_Double(WorldSpace)` exists
  (`VoxelPositionFunctionLibrary.cpp:64-79`). Terrain must be keyed to **world** space (so moving the stamp actor
  never moves mountains) and should use the double variant: float32 gives 8 cm resolution at 1000 km, and our
  spawn was already at Y = -1,144,733 cm (~11 km).
- **Multi-threaded.** Nodes run on worker threads: no `UObject`, no `UDataTable`, no `TSoftObjectPtr`. Shared
  data = an immutable snapshot (`TSharedPtr<const FMOWorldGenParams>`) built on the game thread *before*
  `CreateRuntime` — the same ordering `MOWorldSeedSubsystem` already uses to apply the seed.
- **Outputs the graph can take from us:** `OutputHeight`: `Height`, `SurfaceType` (up to 15 weighted layers per
  sample via `FVoxelSurfaceTypeBlendBuilder`, `Surface/VoxelSurfaceTypeBlend.h`), `HeightRange`, `Bounds`,
  metadata; `OutputVolume`: `Distance` (negative inside), `SurfaceType`, `Bounds`
  (`Graphs/VoxelOutputNode_OutputHeightBase.h:18-36`, `...VolumeBase.h:18-30`). `FVoxelSurfaceType` is a 16-bit
  handle, so a snapshot can hold per-biome surface handles safely.
- **Height layer + volume layer compose.** Volume stamps can read the heightfield below them
  (`FVoxelVolumeBulkQuery::QueryHeights`), so "heightfield base + 3D carving/overhangs on top" is the
  stock pattern (`Voxel_Plugin_Reference.md` §18).
- **Seed flows unchanged.** The `Seed` graph parameter is already pushed to every graph and stamp
  (`MOWorldSeedSubsystem.cpp:159-357`); our node just takes it as a `FVoxelSeed` pin.
- **ISPC targets are `avx512skx, avx2, avx, sse4`, chosen at runtime** (`VoxelCore/VoxelCore.Build.cs:489`)
  **[read]** — see determinism risk in §8.
- Today the project has **no real terrain graph**: only `Content/VoxelExamples/PCGScattering/VHG_Flat` and
  `Content/Penumbra/Maps/VHG_Test_00_Flat`; the `VHG_Realistic` of `Terrain_Foundation_Plan.md` was never built
  **[read]** (file search). So this is a clean slate, not a migration.

---

## 4. Recommended architecture

```
 WorldSeed + GeneratorVersion + DT_Biomes (+ terrain tuning rows)
            │  (game thread, before CreateRuntime)
            ▼
   FMOWorldGenParams  — immutable snapshot, thread-safe shared ptr
            │
 ┌──────────┴───────────────────────────────────────────────┐
 │  FMOWorldGen  (PURE C++: no UObject, no Voxel, no globals)│   <- MOFrameworkCore (see note)
 │   SampleClimate(x,y)      -> C, E, PV, T0, Precip         │
 │   SampleHeight(x,y,LOD)   -> metres, + slope/biome weights│
 │   SampleDensity(x,y,z,LOD)-> signed distance (cm)         │
 │   ResolveBiome(...)       -> id + top-K weights           │
 └──────────┬──────────────┬──────────────┬─────────────────┘
            │              │              │
   Voxel adapter     Gameplay/tools    Tests + map dump
   (MOFramework)     (PCG biome spawner, (automation, PNG of
   UMOTerrainFunction audio, weather,     biomes/height/slope —
   Library: 2 nodes   spawn, AI, UI map) look at it)
```

- **Placement:** `MOFrameworkCore` already holds `FMOBiomeDefinitionRow`, the weather/ambient interfaces and has
  no Voxel dependency (its Build.cs rule: *nothing there may depend on MOFramework*), so audio/weather/tests can use
  the generator without Voxel. Caveat: that module's charter says "policy-free plumbing"; the generator is
  data-driven by DT rows, which I read as compatible, but this is Wes's call (§10 fork 5).
- **Adapter:** `UMOTerrainFunctionLibrary` in `MOFramework` (already depends on Voxel). Illustrative, **not
  compiled** — exact buffer setters for the surface-blend buffer are **[unverified]**:

```cpp
UCLASS()
class MOFRAMEWORK_API UMOTerrainFunctionLibrary : public UVoxelFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(Category = "MO|Terrain")
	void MOTerrainHeight(
		const FVoxelDoubleVector2DBuffer& WorldPosition,   // wire from "Get Position 2D (double), World"
		const FVoxelSeed& Seed,
		FVoxelFloatBuffer& Height,
		FVoxelSurfaceTypeBlendBuffer& SurfaceType,
		FVoxelFloatBuffer& BiomeId) const;                 // out-params become output pins (see BreakVector2D)
};
VOXEL_REGISTER_FUNCTION(UMOTerrainFunctionLibrary, MOTerrainHeight);

void UMOTerrainFunctionLibrary::MOTerrainHeight(...) const
{
	const int32 Num = WorldPosition.Num();                 // constant buffers broadcast: handle Num()==1
	const int32 LOD = /* Query->FindParameter<FVoxelGraphParameters::FLOD>() ... */ 0;
	const TSharedPtr<const FMOWorldGenParams> Params = FMOWorldGen::GetPublishedParams();
	Height.Allocate(Num); BiomeId.Allocate(Num); /* surface blend: FVoxelSurfaceTypeBlendBuilder */
	for (int32 I = 0; I < Num; ++I)
	{
		const FMOTerrainSample S = FMOWorldGen::SampleColumn(*Params, WorldPosition[I], Seed, LOD);
		Height.Set(I, S.HeightCm); BiomeId.Set(I, float(S.PrimaryBiome));
	}
}
```

Graph asset (made once by hand, ~30 s): `Get Position 2D (double) [World] + Seed param -> MO Terrain Height -> OutputHeight`.
Everything else is data. Whether the graph editor's preview viewport works with our node is **[unverified]** but
expected, since the preview just evaluates the graph.

---

## 5. Biome + terrain model (Minecraft's architecture, realism's units)

**What Minecraft 1.18+ does [recalled, not verified here]:** a handful of continuous noises — continentalness,
erosion, weirdness (folded into "peaks & valleys"), temperature, humidity — feed (1) terrain *splines* that
produce offset/factor/jaggedness for a 3D density function, and (2) a biome lookup (nearest match in that
parameter space). Because both read the same fields, terrain and biome can't disagree, borders are organic, and
no per-biome height generators have to be stitched together.

**Proposal for MO57:**

1. **Climate fields (pure functions of position + seed):** continentalness `C` (ocean -> coast -> inland),
   erosion `E` (flat plains <-> rugged), peaks-and-valleys `PV` (ridged), sea-level temperature `T0`, precipitation
   `P`. Every noise is salted by `Hash(Seed, "Name")` so adding a field never shifts existing ones.
2. **Height = splines of climate, not a noise sum:** `H = SeaLevel + Spline_C(C) + Relief(E, PV) + Detail`.
   Spline control points live in a DataTable/curve asset (tunable without recompiling).
3. **Realism pass (cheap, high value):** apply the atmospheric lapse rate after height — `T = T0 - 6.5 °C/km x
   elevation` **[recalled]** — so alpine/tundra bands *emerge* instead of being height bands in a table. Add
   continentality (coast moderates temperature/raises precipitation). Rain-shadow/prevailing wind is a later macro
   bake (§6).
4. **Biome = Whittaker cell (mean temperature x precipitation) + terrain context (elevation, slope, near-sea-level,
   drainage)**, as DataTable rows with a centre+radius in climate space and nearest-match selection (stable
   irregular cells, graceful ties) instead of today's priority-ordered rectangles
   (`MOBiomeDefinitionRow.h`, `MOBiomeDatabaseSettings.cpp:75-97`). Extend the existing row; keep species palettes.
5. **Blend parameters, not heights.** Compute weights for the nearest K=3 biomes; each biome contributes terrain
   parameters (`ReliefMul`, `RoughnessMul`, `RidgedWeight`, `TerraceStrength`, soil depth, border sharpness) and
   surface layers by weight. Soft 20-200 m borders by default; per-biome sharpness for cliffs/shores.
6. **Surface:** top soil / subsoil / rock-by-slope / beach / snow-by-temperature as weighted layers (the
   `OutputHeight.SurfaceType` pin takes up to 15).

Existing `moisture/temperature` 0..1 bands become physical units. Migration is mechanical (one-time conversion of
the current rows) and removes the "height band" special-casing.

---

## 6. What makes terrain "not basic noise" (technique catalog for the C++ side)

| Technique | Why it matters | Cost |
|---|---|---|
| Climate-spline shaping (§5) | Continents, coasts, plateaus, mountain belts with structure | trivial |
| **Domain warping** (offset the lookup position by low-freq noise) | Flowing, organic shapes; breaks grid-aligned noise look | 2 extra low-octave samples |
| **Erosion-aware fBm** (octave amplitude damped by accumulated slope/derivative; Inigo Quilez-style) **[recalled]** | Looks eroded: smooth valley floors, sharp ridges | ~1.5x octave cost |
| Ridged / hybrid multifractal | Mountain ranges with spines | same as fBm |
| **Terracing / mesas / cliffs** (smooth quantisation, slope steepening near thresholds) | Badlands, plateaus, strata | trivial |
| **Rivers & lakes** | The thing noise is worst at. Cheap: valley carve along near-zero contours of a dedicated noise. Right: a coarse **macro bake** at world creation (e.g. 2048² climate/height, flow accumulation, rain shadow) sampled bicubically by the node | cheap vs. seconds + 16 MB (est.) |
| **3D density** (height as a bias inside a signed density, plus 3D noise within a band around the surface) | Overhangs, arches, cliffs that lean, floating features — impossible in a heightfield | ~36k samples/chunk (see §8) |
| **Caves** (spaghetti: two near-zero noise bands; cheese: threshold noise; both gated by depth/biome, e.g. karst) | The Minecraft caves feel; replaces the volume-graph recipe in `Voxel_Plugin_Reference.md` §18b | 3D noise cost, gated |
| Per-biome relief/roughness | A desert *feels* different from a taiga before any foliage appears | free (parameters) |

Density hygiene for the mesher **[recalled/unverified]**: keep `Distance` roughly signed-distance-like (clamped to a
band) rather than raw noise sums; evaluate the expensive 3D terms only where `|z - H| < band`, and use the linear
`(H - z)` elsewhere — that is what keeps a 36k-sample chunk affordable.

---

## 7. How it plugs into what exists

| Existing system | Change |
|---|---|
| `MOWorldSeedSubsystem` / `AMOGameMode` seed pipeline | **None** — node reads the same `Seed` pin. Add publishing of `FMOWorldGenParams` before `CreateRuntime`. |
| `DT_Biomes` / `FMOBiomeDefinitionRow` | Extend with climate-space centre/radius + terrain parameters + surface layers; keep species palette. |
| `UMOPCGBiomeSpawnerSettings` (`MOPCGBiomeSpawnerSettings.cpp:162-196`) | Replace its own `ClimateNoise` + height/slope bands with one call to the shared resolver. Removes the drift the header warns about. |
| Terraforming / sculpt stack | Unaffected: sculpt layers sit above the generated base. A pure base height additionally gives "how deep has the player dug here" queries and a restore target. |
| Weather / thermal (`MOWeatherIntegrationSubsystem`, ambient environment providers) | Ambient baseline temperature/humidity from the climate field (+lapse rate) instead of a global value. |
| `MOAudioSubsystem` (already biome-aware), AI, spawn manager | Query the resolver instead of tracing the world; analytic base height works where no voxel chunk exists yet (relevant to the open "spawn-manager creatures spawning off-terrain" item and to the spawn-settle polling). Hypothesis — verify when built. |
| World Features (`World_Features_Architecture.md`) | Caves/rivers/POIs become features whose scatter rules query biome/climate; macro terrain is the base layer under them. |
| Tests / tooling | Automation tests on the pure functions (below) and a map-dump (PNG of biome id, height, slope, climate) so terrain design can be looked at without a game. |

Tests that become possible (none exist for terrain today): determinism (same seed => identical samples),
tile-edge continuity (no seams), LOD stability (LOD n height within tolerance of LOD 0), range/sanity (sea level
coverage %, max slope), biome coverage histogram per seed, performance budget per 1k samples.

---

## 8. Risks and unknowns

1. **Determinism across machines (highest consequence).** Co-op clients generate their *own* terrain from the replicated
   seed. If host and client disagree by even centimetres the world is silently different — the bug class behind the
   "client can't see the host's pawn" work. Scalar C++ with integer-hash noise and no fast-math is safe on one binary;
   **ISPC is not** unless pinned to one target, because the plugin compiles `avx512skx/avx2/avx/sse4` and picks at
   runtime **[read]**. Stock Voxel noise has the same exposure **[unverified whether it actually diverges]**. Mitigation:
   scalar C++ first; a probe test that hashes N million samples and compares across two CPUs/targets; DT-row hash in
   the join handshake so modded rows can't desync.
2. **CPU speed without ISPC (estimate, unmeasured).** Rough budget: ~1.1k height samples/chunk x ~0.3-1 µs ≈ 0.3-1 ms
   (fine); ~36k volume samples x 150 ns ≈ 5 ms/chunk-thread *before* gating (acceptable only with the band early-out).
   Levers: LOD octave culling, column early-out, per-chunk column cache of `H`, then an ISPC kernel (UE's native
   ISPC or the plugin's pipeline, determinism caveat above). Measure with the plugin's stats (`VOXEL_SCOPE_COUNTER`).
3. **Save compatibility.** Base terrain is a pure function of `(Seed, GeneratorVersion, params)`; sculpt history replays
   on top. Any generator change moves the ground under existing saves. Store `GeneratorVersion` in the world save
   now; bump it deliberately. (Pre-release we may choose to break saves, but the field should exist from day one.)
4. **Editor hot-reload.** Changing tuning rows should regenerate terrain. Options: invalidate through the plugin's
   `FVoxelDependency` (what the curve pin does, `VoxelCurveFunctionLibrary.h:16-25`) or just recreate the runtime
   during dev. **[unverified which is practical]**.
5. **Plugin churn.** Dev-phy build, vendored, gitignored, API changes between pulls. Mitigation: public API only,
   thin adapter, spike test in the automation suite that fails loudly if the node stops registering.
6. **Stamp bounds.** `OutputHeight`/`OutputVolume` need a finite `Bounds`; a world-spanning stamp needs the world
   border as its box. The finite-world size is not defined anywhere I could find (`WorldBorder`/`WorldRadius` grep
   returned nothing) — a number we have to pick.
7. **Not verified at all:** that a game-module function library shows up in the editor palette and evaluates in a
   packaged build; the exact bulk-query padding; the project's ISPC build mode; surface-blend buffer setters.

---

## 9. Spike plan (not executed)

Each step ends with a visible artifact and a pass/fail; each is small enough to revert.

| Step | Do | Pass criteria |
|---|---|---|
| **S0** | Pure `FMOWorldGen` skeleton + noise + automation tests (determinism, continuity) + PNG map dump. No Voxel. | Tests green; I can open the PNG and see continents/mountains/biome cells. |
| **S1** | One node `MOTerrainHeight` (height only) in `UMOTerrainFunctionLibrary`; sandbox graph + stamp in a sandbox map. | Node appears in the graph editor; preview + PIE terrain match the PNG map at sampled points; **packaged Development build** loads it; node registration check passes (no "missing VOXEL_REGISTER_FUNCTION" log). |
| **S2** | Add surface blend + biome id metadata; point the PCG biome spawner at the shared resolver. | Biome materials visible; PCG species follow the same biome map; old vs new biome histogram sane. |
| **S3** | Density node (3D carving + overhangs) on a volume layer above the heightfield. | Chunk gen time within budget (measure); overhang/cave visible; sculpt still works on top. |
| **S4** | Determinism probe across two CPUs / ISPC targets; co-op two-process test on the new terrain (`ue.py nettest`). | Identical sample hashes; both pawns stand on the same ground. |
| **S5** | Macro bake for rivers/rain shadow only if S0-S3 show the noise-valley trick is not enough. | Rivers run downhill to the sea in the PNG. |

---

## 10. Design forks (recommendation first; Wes decides — none blocks S0/S1)

1. **Base terrain: heightfield + volume carving (rec.) vs. full 3D density.** Heightfield base keeps the existing
   height-sculpt path cheap and gets caves/overhangs from a second layer; full 3D everywhere costs ~30x the samples
   for features most of the world doesn't need. Revisit if we want floating islands.
2. **Climate in physical units (rec.) vs. 0..1 noise.** Physical units give lapse rate, continentality and a
   Whittaker table "for free" and match the realism pillar; the cost is a one-time conversion of the current rows.
3. **Nearest-match biome cells with parameter blending (rec.) vs. priority-ordered bands with height blending.**
   The former has no seams and scales to many biomes; the latter is what exists and works for two or three.
4. **Rivers: noise-valley now, macro bake later (rec.).** Do not build the hydrology bake until we see the noise
   version in the PNG and in play.
5. **Generator in `MOFrameworkCore` (rec.) vs. `MOFramework`.** Core lets audio/weather/tests use it without Voxel;
   `MOFramework` avoids bending Core's "policy-free plumbing" charter. Both keep the Voxel adapter in `MOFramework`.
6. **Own noise in C++ (rec.) vs. wrapping stock noise nodes.** A hybrid (stock noise nodes feeding our logic-only
   node) is faster to try but cannot be reused outside the graph, which is the point of the whole exercise.

---

## Appendix — what I read this session

Plugin: `VoxelFunctionLibrary.h/.cpp`, `FunctionLibrary/Voxel{Position,Basic,Math,Curve}FunctionLibrary.*`,
`Nodes/VoxelAdvancedNoiseNodes.{h,cpp}`, `Nodes/VoxelRandomNodes.h`, `VoxelBufferAccessor.h`, `VoxelObjectPinType.h`,
`VoxelHeightStamp.h`, `VoxelStampQuery.h`, `Heightmap/VoxelHeightmapStamp.{h,cpp}`, `Graphs/VoxelHeightGraphStamp.cpp`,
`Graphs/VoxelVolumeGraphStamp.cpp`, `Graphs/VoxelOutputNode_Output{Height,Volume}Base.h`,
`Graphs/VoxelStampGraphParameters.h`, `Surface/VoxelSurfaceType*.h`, `VoxelWorld.h`, `VoxelCore.Build.cs`,
`VoxelGraph.Build.cs`, `VoxelSourceParser.cpp`, `ReleaseNotes.md`, plugin `CLAUDE.md`.
Project: `MOBiomeDefinitionRow.h`, `MOBiomeDatabaseSettings.{h,cpp}`, `MOPCGBiomeSpawnerSettings.cpp` (grep),
`MOWorldSeedSubsystem.cpp` (grep), `MOFramework.Build.cs`, `MOFrameworkCore.Build.cs`, `Voxel_Plugin_Reference.md`,
`Terrain_Foundation_Plan.md`, `World_Features_Architecture.md`.
