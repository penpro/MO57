# PCG: vegetation, ground clutter, removals and the world reveal

> **Covers:** how the world is planted (biome spawner tiers, native chains, cave spawner), the chokepoints every chain passes,
> how density is really realised, what keeps it fast, how removed things stay gone, and how the reveal waits for the world.
> **Current state (2026-10-17):** one PCG component per level (on the `PCGVolume`, runtime hierarchical generation around the
> local pawn/camera); vegetation comes from `DT_Biomes` palettes through the MO biome spawner in tiers; native biome-blind chains
> are cut or biome-limited; removals are permanent, replicated and saved; new-game READY ~7-17 s packaged.
> **Entry points:** graph `MOPCG_StampScatter1` (game level `/Game/VoxelExamples/PCGScattering/MOPCGScattering`, menu
> `/Game/Penumbra/Maps/LoadingLevel`); `UMOPCGBiomeSpawnerSettings`, `UMOPCGElevationBandsSettings`, `FMOPCGWaterMask`
> (`MOPCGWaterMask.h`), `UMOPCGCaveSpawnerSettings`, `UMOWorldRemovalSubsystem`, `FMOHISMHarvestHelper`,
> `UMOWorldReadinessSubsystem`; palettes `Content/Python/biome_palettes.py` + `Docs/Ground_Coverage_2026-10-14.plan.json`.
> **Dev verbs:** `MO.PCG.Census [r]`, `.Rings`, `.MeshSurvey [top]`, `.HideTag <tag> [0|1]`, `.HideMesh <substr> [0|1]`,
> `.WaterAudit [r]`, `.ResourceCensus <r>`, `MO.WorldGen.FindBiomeSpot <id> [km] [rough] [go]`, `.BiomeStats`,
> `MO.Readiness.Dump|Restart|Force`, `MO.Test.RemovalPersists`, `MO.Test.WorldRemovalAudit`; cvars `mo.PCG.BiomeSoftPick`,
> `mo.PCG.BiomeSpawnerProfile`, `mo.Readiness.*`, `mo.Foliage.RigidShadows`, `pcg.RuntimeGeneration.GlobalRadiusMultiplier`.
> **Tests:** `MOFramework.PCG.*` (WaterMask, MayHaveShore, ClumpRateHolds, ItemThinning, BiomeSamplerLattice, NoDeadBranches),
> `MOFramework.WorldGen.BiomesFromDataTable`, `MOFramework.WorldRemoval.Ledger`, `nettest packaged-removal`.
> **Open items:** `Docs/BACKLOG.md` § PCG and world features. **Traps:** `Docs/TRAPS.md` § 9 (PCG). **Reference:** `Docs/ElectricDreams_Lessons.md`,
> `Docs/Voxel_Plugin_Reference.md` §8. POI compositions: `Docs/World_Features_Architecture.md`. Cave planting: `Docs/Caves_And_Geology.md`.

## 1. How PCG is hosted (checked 2026-10-12)

Each gameplay level and the menu level has exactly ONE PCG component, on the `PCGVolume` actor (plus the engine's
`PCGWorldActor`). It is the runtime generator: `GenerationTrigger = GENERATE_AT_RUNTIME`, partitioned (hierarchical generation,
radii per grid size), so PCG spawns AROUND the generation sources (the local player's pawn / camera) and nowhere else. The
volume's brush is the generation bounds (about +-65 km) and it holds the graph instance. Deleting it deletes every spawner.
`VoxelSandBox.umap` still has its own PCG actors but is never loaded by the game. The "invoker" in other docs is the VOXEL
invoker (`AMOCharacter::VoxelCollisionInvoker`), which is separate from PCG.

## 2. Grid, radius and lattice rules (the numbers that decide everything)

- **Hi-gen radius = 2 x grid x `HiGenGridSizeMultiplier`** (4 on both MO graphs): GRID16 = 64 m cells / 128 m default radius,
  GRID32 = 128 m / 256 m, GRID128 = 512 m / 1 km, GRID256 = 1 km, GRID512 = 2 km, GRID2048 = 8 km / 16 km.
- **The radius fields are named by the UNSCALED grid**: GRID16 (64 m cells) reads `radius1600`, GRID32 `radius3200`, GRID128
  `radius12800`, GRID256 `radius25600`, GRID512 `radius51200`. The fields are PROTECTED: reach them by reflection and call
  `ComputeHash()` after changing them (`set_game_radii.py`). Verify with `MO.PCG.Rings` (counts per distance ring by kind).
- **The Voxel Sampler places at most ONE point per `CellSize` lattice cell**: realised density = min(ppsm, 1e4 / CellSize^2)
  (CellSize in cm). Pick `CellSize <= ~70 / sqrt(rho)` cm. The biome spawner logs `[MOBiomeSpawner] in=N ... ms= inBounds=`;
  `in / area_ha` must equal the node's `InputPointsPerHectare`, otherwise every authored density is scaled by the ratio.
  `MOFramework.PCG.BiomeSamplerLattice` enforces ppsm <= 0.9 x the lattice cap AND InputPointsPerHectare = what is emitted.
- **Acceptance:** each input point of a tier becomes at most one plant, `p = density x clump_compensation / InputPointsPerHectare`.
  Under a tier cap, cover comes from SIZE, not density.

## 3. The tiers (as of 2026-10-14; verify with `MO.PCG.Rings`)

| Tier | Node / filter | Cells | Cap | What |
|---|---|---|---|---|
| Ground cover | `MO Biome Spawner_0`, GroundCover | GRID16 (64 m), r 180 m | 6000 candidates/ha on a 90 cm lattice (was 2000) | grass, flowers, small plants of the palette |
| Large | LargeOnly | GRID32 (128 m), r 256 m | 1200/ha on 250 cm | bushes, shrubs, rocks, saplings (tag decides) |
| Trees | TreesOnly (`add_tree_tier_chain.py`) | GRID128 (512 m), r 800 m; real trees to ~1.5 km | 800/ha | trees |
| Shore | WaterEdgeOnly | GRID16 | 2/m2 on 60 cm, input 20,000/ha; skipped where `MayHaveShore` is false | `WaterEdgeSpecies` (stand-in reeds today) |
| Far grass | FarGroundCoverOnly, tag `MOFar_Grass` | 512 m cells | 288/ha; min 100 m from viewer; 16 % density x2.1 scale | |
| Far trees | FarLargeOnly (`MO Biome Spawner_5`), tag `MOFar_Tree` | GRID256 (1 km) | 300/ha on 404 cm (was 112.5) | stand-ins 1.5-2.5 km, scale 1.1 |
| Far canopy | FarCanopyOnly (`MO Biome Spawner_6`), tag `MOFar_Canopy` | GRID512 | 40/ha on 1106 cm (was 18) | clump proxies (`Content/Penumbra/FarCanopy`) 2.2-7.5 km |
| Native lawn grass | the scatter graph's native chain, gated per biome by `MO Elevation Bands_3` | | ~14k/ha | `Content/Python/native_grass_biomes.py` holds the biome list + reasons (AlpineHeath IN for #25, Rainforest OUT for #6) |
| Item clutter | `MO Mesh Spawner_0/1` fed by `RandomChoice_3` off `VoxelSampler_0` | | ~100 items/ha | sticks / bark / stones: thin the ratio (`thin_item_clutter.py`), NEVER the sampler (the ground tier shares it) |
| Caves | MO Cave Spawner (GRID16) | | | `Docs/Caves_And_Geology.md` §5 |

Far tiers use the engine's per-instance `InstanceMinDrawDistance`, which needs `InstanceEndCullDistance > 0`
(`MOPCGBiomeSpawner::ApplyNearDrawFloor`), so a far cell that becomes near leaves no ghost trees. Far tiers and ground cover cast
no shadow and no ray tracing; native grass fades 150-200 m (`tune_native_grass.py`). Filter enum values were APPENDED (4..6) so
saved graphs keep their meaning.

## 4. The chokepoints (one rule each, every chain passes through)

- **`MOPCGElevationBands`**: every spawner chain passes one. `bExcludeWater` (default on) + `AllowedBiomeIds` + `bExcludeCaves`.
  `FMOPCGWaterMask` = the sea plane via the point's own ground Z + the generator's upland-lake zone (+ `ShoreFactor`, `BiomeAt`,
  `IsInCave`, `MayHaveShore`). `MO.PCG.WaterAudit` measured 51k instances in one lake before; 0 after.
- **The biome spawner** plants `DT_Biomes` palettes. Biome identity comes from the biome data, never from graph-wide layers.
  **New vegetation = a species row** (the HISM tag decides the tier), never a new native chain. A tag outside the large list (e.g.
  `MOBiomeCover_Fern`) lands a species in the near tier with no far copies.
- **Soft biome boundaries:** `FMOWorldGen::PickSurfaceBiome` rolls each point's biome against `SampleSurfaceBiomeWeights` (the
  same weights that blend the ground surface), so plants interleave across an ecotone (`mo.PCG.BiomeSoftPick` 1; 0 = old hard
  match). Tune with `FMOWorldGenTuning::SurfaceClimateBlend` (0.12; keep < ~0.2 so the narrowest climate band keeps a pure core).
- **Clump compensation is real since be6506f7:** the clump field `PerlinNoise2D*0.5+0.5` is NOT uniform (std ~0.26), so
  compensating by 1/(1-core) planted canopy trees at ~23 % of authored. `ClumpCoverage` / `ClumpCompensation(core) = 1/coverage`
  (cap 8x) fixed it; `MOFramework.PCG.ClumpRateHolds`. **Any palette number tuned by eye before be6506f7 was tuned against the bug.**
- **Collision policy:** harvestable species take `FMOResourceNodeDefinitionRow::GetPlacementCollisionProfile()` (tree / rock /
  ore -> BlockAll; bush / herb -> OverlapAllDynamic); decorative species use the node's `DecorativeCollisionProfile`
  (NoCollision). `EMOSpeciesCollision` (Auto / Passable / Solid) on `FMOBiomeSpeciesEntry` overrides "a Tree in the resource table
  but a knee-high plant" (every Black Locust `wdvhberja` variant is Passable). `MO.Dev.ScatterCollision [filter]`,
  `MOFramework.Data.SolidMeshCollision`.
- **Rock alignment:** aligned species rotate as `PointRot * FQuat(Up, Yaw)` (yaw about the surface normal), pivot-to-bottom
  lift + `SinkFraction` (auto 0.25 for aligned) / `SinkMinCm`.

## 5. Palettes: where the numbers live

- `Content/Python/biome_palettes.py` builds the palettes and applies the per-tier factors of
  `Docs/Ground_Coverage_2026-10-14.plan.json` (`<tier>`, `<tier>:<HISMTag>`, `<tier>_core_max`). Regenerate:
  `python Tools/ue.py py --file Content/Python/biome_palettes.py`, then `python Tools/ue.py rows set .../DT_Biomes --file
  Saved/Logs/biome_palettes.json`, then diff live vs script.
- **The script has DRIFTED from the live table (Oct 16):** Meadow / RockyHighland / DryScrub / AlpineHeath have +1 species live;
  Rainforest hazel densities live are ~0.7x; live has 42 Black Alder tree entries vs 28 in the plan. Regenerating would revert live
  tuning: diff plan vs `ue.py rows get` first, or edit live rows directly (rows set with the FULL live species arrays; `rows list`
  output has CRLF). The Oct 16 alder burial (+25 cm `SinkMinCm` on Forest_ / Field_ alders, Wes) was done that way and is also in
  the script's `species()`.
- Experiments: `Tools/pcg_stocking.py <export> [--old] [--plan]` replays the spawner rule (Monte Carlo over the clump fields,
  engine noise ported); `apply` multiplies the LIVE table (experiments only). Shore lists: `Tools/shore_dressing.py` (additive).
- Realised densities after the Oct 14 pass (trees/ha): TemperateForest ~213, Locust / Electric ~160, Rainforest ~108 (Wes called
  ~72 "VERY dense": don't push past ~110), Meadow copses ~5. Full table: `Docs/Archive/Ground_Coverage_2026-10-14.md`.
- **Measure before tuning:** `MO.PCG.Census 4000` (0.5 ha) at a `FindBiomeSpot` spot, by RADIUS (not `MeshSurvey` totals, which
  depend on how many far cells finished); the spawner log lines per tier.

## 6. Performance rules (what keeps it fast)

- **PCG runtime scheduler = largest grid first** (UE 5.8 `PCGRuntimeGenScheduler.cpp`: `Priority += GridSize`): a big far tier
  delays every near cell. `UMOWorldReadinessSubsystem` holds grids >= `mo.Readiness.HoldGridsFrom` (512) during a cycle (radius
  -1e9: a radius of 0 still generates the viewer's own cell because the scheduler compares SQUARED distances) and restores at Ready.
- **Classify species once per execution, never per point:** `FName(TEXT(...))` + `Mesh.ToString()` per species per point cost
  13.5 us/pt (161 us/pt in parallel: name-table lock). The per-point loop is `ParallelForWithTaskContext`; profile with
  `mo.PCG.BiomeSpawnerProfile 1`.
- **The biome spawner is time-sliced** (`FMOPCGBiomeSpawnerContext`, plant on slice 1, `AddInstances` in adaptive batches until
  `Context->AsyncState.ShouldStop()`; `pcg.FrameTime` budget 5 ms). Log: `build X in N slice(s)`. Worst single build batch ~9 ms.
- **A disabled PCG node passes its input through**: its upstream chain still computes. Dead native chains are cut at the root
  (`Content/Python/cut_pcg_dead_branches.py`, test `MOFramework.PCG.NoDeadBranches`). A HiGenGridSize node with NO edges stopped
  ALL generation silently.
- **Wind on shadow casters = the dense-forest GPU cost** (WPO invalidates virtual shadow map pages every frame: 7 of 19 ms in the
  Rainforest). `mo.Foliage.RigidShadows` (default 1) = `ShadowCacheInvalidationBehavior` Rigid on biome-scatter shadow casters;
  `WindDisableDistanceCm` (80 m) -> `WorldPositionOffsetDisableDistance`.
- **Do NOT raise PCG's throttles** (`pcg.FrameTime 30`, more executing threads, NumGeneratingComponents 48 made it SLOWER: frames
  stalled ~5 s). Runtime radius scale is `pcg.RuntimeGeneration.GlobalRadiusMultiplier` (menu level x4.5).
- **Change detection loses cells** for a still camera (`pcg.RuntimeGeneration.EnableChangeDetection` rescans only after the
  camera moves > 10 % of the smallest grid); the readiness hold's release forces the rescan. Compare `MO.PCG.Rings` at 45 s after
  any PCG timing change, not per-cell results. (A worker split of the spawner's point loop was tried and REVERTED.)
- Full measurement method and history: `Docs/Performance.md`.

## 7. Removals stay gone (`UMOWorldRemovalSubsystem`, commit 7e145b73)

- **Chokepoint:** `FMOHISMHarvestHelper::RemoveInstance` (+ batch variants) records via `RecordInstanceRemoval` BEFORE removing.
  **Any new removal path must go through the helper**: a raw `ISM->RemoveInstance` regrows when the cell regenerates and desyncs
  co-op. Terraform grass sweeps are separate (terrain-mod zones suppress regeneration).
- **Key:** `FMOWorldRemovalLedger` = (X/10 cm, Y/10 cm, 1 m Z cell `CellZ` (`AnyZ` for old saves), mesh FName); lookups tolerate
  +-1 cell. Relies on deterministic generation (per-point random streams from point seeds).
- **Consumers:** the biome spawner merge and the MO item mesh spawner skip `IsRemoved`. Native StaticMeshSpawners do NOT consult it.
- **Replication:** `AMOGameState::RemovedInstances` FastArray -> client `HandleReplicatedRemoval`. **Save:** `IMOSaveDomain`
  "WorldRemovals" -> `UMOWorldSaveGame::WorldRemovalData`. `MO.Save.LoadFrom` reloads IN PLACE (same UWorld).
- Removals are permanent; regrowth over game time is an open design decision (DECISIONS F3).

## 8. The reveal waits for the world (`UMOWorldReadinessSubsystem`)

- Stages: Terrain (`UMOVoxelReadinessSubsystem` Ready) -> Vegetation (`UPCGSubsystem::IsAnyGraphCurrentlyExecuting` quiet 1.5 s,
  cap `mo.Readiness.MaxPcgSeconds` 15) -> Streaming (`IStreamingManager` wanting <= 8, cap 4 s) -> Ready; total cap 60 s.
  `mo.Readiness.Enabled 0` bypasses (harnesses).
- `BeginTracking` is called by `AMOGameMode` (after the voxel poll starts: starting it earlier, the cooked default world reports
  ready) and by `AMOMainMenuPlayerController` (`bStartVoxelPolling=true`).
- **One chokepoint:** `UMOGameInstance::ReleaseLoadingOverlayWhenWorldReady` (DismissLoadingScreen and the menu reveal both end
  there; every overlay removal broadcasts `OnLoadingOverlayReleased`).
- **Loading focus:** `AMOGameMode::BeginLoadingFocus` = a camera actor at the seed-determined spawn is the view target until the
  pawn is possessed (voxel + PCG build around the VIEW; with no pawn that was the origin, 17 km off).
- Timings: packaged new-game READY 45.7 s -> 29.1 s (da3c8c22) -> 7.1 s (Oct 13 pm, far voxel chunks no longer resolve
  sub-voxel lakes: `FMOWorldGen::IsWaterTierResolved`) -> ~14-17 s after the Oct 14 coverage pass (more instances).
- Bug reports carry `WorldBuild.*` fields from the readiness subsystem (for PCG stalls on Shipping, which writes no log).
