# PCG: vegetation, ground clutter, removals and the world reveal

> **Covers:** how the world is planted (biome spawner tiers, native chains, cave spawner), the chokepoints every chain passes,
> how density is really realised, what keeps it fast, how removed things stay gone, how the reveal waits for the world, and why the
> foliage wind keeps its precision far from the origin (§9).
> **Current state (2026-10-17):** one PCG component per level (on the `PCGVolume`, runtime hierarchical generation around the
> local pawn/camera); vegetation comes from `DT_Biomes` palettes through the MO biome spawner in tiers; native biome-blind chains
> are cut or biome-limited; removals are permanent, replicated and saved; new-game READY ~7-17 s packaged.
> **Entry points:** graph `MOPCG_StampScatter1` (game level `/Game/VoxelExamples/PCGScattering/MOPCGScattering`, menu
> `/Game/Penumbra/Maps/LoadingLevel`); `UMOPCGBiomeSpawnerSettings`, `UMOPCGElevationBandsSettings`, `FMOPCGWaterMask`
> (`MOPCGWaterMask.h`), `UMOPCGCaveSpawnerSettings`, `UMOWorldRemovalSubsystem`, `FMOHISMHarvestHelper`,
> `UMOWorldReadinessSubsystem`; palettes: the live DT_Biomes (source of truth since Oct 25, N-204), changed by targeted patch tools.
> **Dev verbs:** `MO.PCG.Census [r]`, `.Grounding <mesh substr> [r]` (does each base touch the ground: worst point of its base outline, by slope), `.Rings`, `.MeshSurvey [top]`, `.HideTag <tag> [0|1]`, `.HideMesh <substr> [0|1]`,
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
| Landmarks (Oct 24) | LandmarksOnly (`add_landmark_tier_chain.py`), species with `bLandmark` | GRID256 (1 km), r 2.5 km | 32/ha on 1500 cm | the xlarge boulders, the cliff faces, the crag formations: the REAL instances near and far (no near floor, shadows, collision); every other tier leaves them out |
| Native lawn grass | the scatter graph's native chain, gated per biome by `MO Elevation Bands_3` | | ~14k/ha | `Content/Python/native_grass_biomes.py` holds the biome list + reasons (AlpineHeath IN for #25, Rainforest OUT for #6) |
| Item clutter | `MO Mesh Spawner_0/1` fed by `RandomChoice_3` off `VoxelSampler_0` | | ~100 items/ha | sticks / bark / stones: thin the ratio (`thin_item_clutter.py`), NEVER the sampler (the ground tier shares it) |
| Caves | MO Cave Spawner (GRID16) | | | `Docs/Caves_And_Geology.md` §5 |

Far tiers use the engine's per-instance `InstanceMinDrawDistance`, which needs `InstanceEndCullDistance > 0`
(`MOPCGBiomeSpawner::ApplyNearDrawFloor`), so a far cell that becomes near leaves no ghost trees. Far tiers and ground cover cast
no shadow and no ray tracing; native grass fades 150-200 m (`tune_native_grass.py`). Filter enum values were APPENDED (4..6) so
saved graphs keep their meaning.

**The glowing grass (BR23-11 / BR24-21; Wes, Oct 25 2026: "for the love of god, fix the weird glowing/weirdly shaded grass").** The lawn
grass is Megascans Two Sided Foliage; the light it passes through its blades is Desaturate(BaseColor x Mask x Controls.G, 1 - Controls.R)
^ Controls.B per `Translucency Controls*` vector (`QMF_GenerateTranslucency`; measured live: R = saturation, G = strength, B = exponent).
The wild / thatching grass shipped at strength 5.0 with exponent 0.5 (a square root lifting every dim value), the lawn grass leaves 3.0:
the transmitted light out-shone the lit side, so the blades read pale, self-lit, and a shadow on them (a tree's, the pawn's) drowned in it.
`Content/Python/fix_foliage_transmission.py` caps every instance under M_MS_Foliage / M_MS_Billboard at strength 0.5, exponent >= 1
(14 instances, 11 changed; rerun after importing plants; test `MOFramework.Rendering.FoliageTransmissionCapped`). A/B (seed 4242, noon,
toward the sun): `D:/MOAssets/previews/grass/g0_spawn.png` before, `g4_s05_e1.png` after, `shadow_strip.png` (original / capped /
no transmission / capped without VSM: VSM is not the fault, turning it off made the blades paler). The grass still CASTS no shadow
(ground cover, for cost); receiving was never off.

**The blurry flowers (BR23-10 / BR24-19 / BR24-20; Wes: "a few of these little flowers are blurry", "look good here ... but weird and blurry
in other places").** Two causes, both measured on his own views (`Content/Python/flower_motion_capture.py`: a report's camera held still,
then a strafe at walking pace, a screenshot mid-pan and one converged; `Tools/flower_sharpness.py` scores the flowers' purple pixels):
1. The 24 PN flower meshes the biome palettes plant were Nanite (`enable_palette_nanite.py`, Oct 14) and under Nanite their material MA_Flowers
   writes wrong motion vectors: motion blur streaked them with the camera STILL, TSR ghosted them; with no AA and no motion blur the same
   flowers were sharp, so the velocity is the fault, not TSR (`D:/MOAssets/previews/grass/nanite_flowers_motionblur_ab.png`). Classic again
   (`Content/Python/flower_meshes_non_nanite.py`; MA_Flowers cross-fades its LODs; `enable_palette_nanite.py` now leaves MA_Flowers meshes
   alone): Wes's rainforest view (report 89300267) went from a smear to a sharp flower, 3.1x the edge contrast still, 2.4x mid-pan
   (`pal_flowers_before_after.png`). The native scatter's two lupine meshes had always been classic -- "the ones that look good".
2. The near-camera fade (MO_CameraFade, 40-160 cm, temporal dither) on ground cover: under the low third-person camera whole tufts sit in the
   band, and while the camera moves TSR cannot average the screen door -- dotted, broken blades, flowers washing out (16 % of the purple gone
   mid-pan). Ground cover now fades at 15-45 cm (`foliage_camera_fade.py` GROUND_COVER_BAND): solid blades mid-pan, no purple lost
   (`grass_moving_before_after_band.png`). What remains -- ~20 % less edge contrast while moving -- is TSR's normal cost; TAA scored softer
   still and moving. Test `MOFramework.Rendering.FlowersNotNanite` (96 flower meshes classic, the band on ground cover).

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
- **Open-backed cliff slabs face downhill (Oct 19 2026, BR18-14 / BR18-19):** a mesh that is only a rock face with a hollow back
  must never take the slope's normal (on a 36 deg slope it leaned with the hill, showed its back and floated its loose chunks).
  `bFaceDownhill` + `FaceYawDeg` (where the mesh's face looks, measured by `Content/Python/cliff_face_probe.py`: area-weighted
  triangle normals; HugeSandstoneCliff = 90) stand it world-up, the face turned down the slope (+-10 deg jitter), and sink it by
  the ground's drop under its face (`FMOSpeciesPlacement::FaceDownhillRotation` / `FaceDropCm`, test
  `MOFramework.PCG.CliffSlabsFaceDownhill`). A closed rock (probe strength ~0, the Nordic coastal cliff) stays aligned. Measured
  on seed 670711536 (RockyHighland, `MO.BiomeTest`): two slabs probed three times, tilt 0.0, face within 2-25 deg of the probe's
  downhill (it reads the slope over 12 m, the spawner the point normal), the front base 1.8-4.5 m under ground.

## 5. Palettes: where the numbers live

- **The live DT_Biomes is the source of truth** (Oct 25 2026, N-204): a palette changes only through a targeted patch tool with readback (`ue.py rows get` -> patch -> `ue.py rows set`: `Tools/highland_crags_patch.py`, `rock_scale_cap.py`, `remove_open_geometry.py`, `pcg_stocking.py`'s plans, the Content/Python `*_patch` / `fix_*` scripts). `Content/Python/biome_palettes.py` is the AUTHORED palettes of Oct 10-16 and no longer applies: run plainly it writes nothing; `BIOME_PALETTES_REGENERATE=True` writes `Saved/Logs/biome_palettes_regen.json` and prints per row the live entries the plan lacks (Oct 25: 89 across all 11 rows -- the crags, the Dark Ruins cliffs, the size tiers, the coverage boulders), so it cannot revert the table by accident.
- **The script has DRIFTED from the live table (Oct 16):** Meadow / RockyHighland / DryScrub / AlpineHeath have +1 species live;
  Rainforest hazel densities live are ~0.7x; live has 42 Black Alder tree entries vs 28 in the plan. Regenerating would revert live
  tuning: diff plan vs `ue.py rows get` first, or edit live rows directly (rows set with the FULL live species arrays; `rows list`
  output has CRLF). The Oct 16 alder burial (+25 cm `SinkMinCm` on Forest_ / Field_ alders, Wes) was done that way and is also in
  the script's `species()`; Oct 23 (BR23-7, "still floating, drop them about 25cm"): 50 cm, the same 42 entries.
  **Oct 23 evening (Wes's vote: "still floating ... they still look exactly like they did"), measured with `MO.PCG.Grounding`:** the
  temperate alders were buried as set (pivot 50 cm down, the base 10-45 cm under), and every sapling / seedling mesh starts at its pivot.
  The trees Wes saw were ElectricForest's **hornbeams**: their resource row is BlackAlder, so the game names them "Black Alder", and both
  burial changes went to the real alders. They stood on the ground unsunk: 4-26 cm of air under their root ends on flat ground, more on
  slopes. And the x3-wide rainforest alders' bases stood up to 76 cm up on 10-20 deg slopes (a wider base reaches lower ground downhill).
  Fixed: the hornbeam forest / field trees get the alders' 50 cm, and an upright tree with a measured `BaseRadiusCm` sinks by the slope's
  drop across its base (`FMOSpeciesPlacement::SlopeDropCm`, test `PCG.TreesSinkOnSlopes`; radii in DT_Biomes and `biome_palettes.py`
  `BASE_RADIUS_CM`). The probe's gap reads the lowest vertex at the outer edge of a mesh's bottom 30 cm: right for a trunk and its flare,
  but a leafy low plant (sapling, seedling, berry bush) reads its lowest leaves (~25 cm), not air under its stem.
- **World features keep their core, not their footprint, bare** (Oct 23, BR23-3): inside a composition's footprint only ground cover,
  shrubs and bushes grow, outside its core (`CoreFraction`, `Docs/World_Features_Architecture.md`).
- **Cliff slabs cluster** (Oct 23, BR23-12, "cluster together to overlap ... scaling them up by about 200%"): the HugeSandstoneCliff
  species of RockyHighland / AlpineRock share clump group 7 (core 0.75: they stand in clumps, overlapping), scale 1.4-2.6 (was
  0.7-1.3), SinkFraction 0.3 (was 0.2). Data only; Wes's own idea, a cliff composition (its own graph clustering slabs round a centre)
  is the next step if this does not read as one face.
- **Lake shores in dry scrub** (Oct 23, BR23-16d): DryScrub's water-edge species are the Meadow's (reeds, grasses, flowers, alder
  saplings, hazel) over an 8 m band (`WaterEdgeBandCm` 800, was 120: a 1.2 m strip of half-density grass).
- **A scan that is an item's mesh must be the ITEM'S mesh** (Oct 23, BR23-9 "small sticks not interactable"): `UMOPCGInteractionSubsystem`
  maps meshes to items by the item's mesh PATH, and the debris branches / sticks (paErt, uehjfcofa) were scattered from their Megascans
  originals while Branch01 / Stick01 use copies under `/MOFramework/Items/`: they looked like the items and could not be picked up. Their
  species now point at the item copies (ShoreStrand, Rainforest, NordicStrand; `biome_palettes.py` `MOVED`). Resource nodes that share an
  item's mesh (beach rocks -> Hammerstone01 / RiverCobble01, the dead shrub -> PlantFiber01) are kept: they are worked, not picked up.
- Experiments: `Tools/pcg_stocking.py <export> [--old] [--plan]` replays the spawner rule (Monte Carlo over the clump fields,
  engine noise ported); `apply` multiplies the LIVE table (experiments only). Shore lists: `Tools/shore_dressing.py` (additive).
- Realised densities after the Oct 14 pass (trees/ha): TemperateForest ~213, Locust / Electric ~160, Rainforest ~108 (Wes called
  ~72 "VERY dense": don't push past ~110), Meadow copses ~5. Full table: `Docs/Archive/Ground_Coverage_2026-10-14.md`.
- **Measure before tuning:** `MO.PCG.Census 4000` (0.5 ha) at a `FindBiomeSpot` spot, by RADIUS (not `MeshSurvey` totals, which
  depend on how many far cells finished); the spawner log lines per tier.

- **Wes's Oct 24 reports (BR24, `Docs/BugReports_2026-10-24.md`):**
  - Sandstone slabs (HugeSandstoneCliff 04 / 07) LIE on the slope now (`bLieOnSlope`, `FMOSpeciesPlacement::LieOnSlopeRotation` /
    `LieOffsetCm`, test `PCG.SlabsLieOnTheSlope`): the rock face looks out of the ground along the surface normal, the slab's height runs up
    the slope, sunk until 15 % of its thickness stands proud (`LieShowFraction`) -- a rock outcrop. Stood upright at x2 they were walls and
    pillars on steep ground ("larger cliffs are WAAAAAAYYYYYY out of the ground... maybe try laying them down and alligning them to the slope?").
  - Open plates / rocky-ground scans need flat ground (`MaxGroundReliefFraction` 0.5): the ground under the footprint is sampled at 8 points,
    and a spot whose relief against its best plane (`FMOSpeciesPlacement::GroundReliefCm`) is more than half the mesh's thickness stays empty.
  - Closed rocks in size tiers (`Content/Python` patch, live in DT_Biomes): per biome its 3 densest closed rocks get a large tier (x3-6, a
    tenth the density) and an xlarge tier (x4-8 since Oct 24 evening, was x8-16; a fiftieth, slopes up to 30 deg), decorative and a quarter of
    their height buried; AlpineRock's
    rocks are x3. Wes: "make a lot of the closed geometry rocks larger, like 2-20 time larger ... set into the ground in a dynamic depth".
    Within a tier most are small and few giant (Wes, Oct 24: "make fewer of them that large, bias the sizes away from all being that large"):
    a species' `ScaleBias` spreads its scale as MinScale + range x u^bias (`FMOSpeciesPlacement::DrawScale`, test
    `PCG.ScaleBiasKeepsGiantsRare`). xlarge 3: 9 % in the top quarter of the range, mean x10 then (was 25 %, x12); large 2: mean x4. Then
    Wes, Oct 24 evening: "i was wrong about the rock features they look kind of bad at that size lets reduce them to about half at the larger
    end": the 29 xlarge entries' Min/MaxScale halved (x8-16 -> x4-8 of the base; S_Stone x10-20 -> x5-10), the large tier and the bias
    unchanged. PIE seed 777 strand: within 600 m no tier rock passes its new maximum (largest S_Stone x9.1; it had 19 at x10-20). 1 (the
    default) is the old even draw, so every other species and every world's layout are unchanged.
  - The spawner planted only a biome's first 64 species (a leftover of an old fixed loop): the Rainforest and ElectricForest have 72, and
    their far stand-ins and canopy proxies (rows 64-71) never grew. The cap is gone.
- **Landmarks and the highland crags** (Oct 24 evening; Wes: "a 'sub biome' in the higher up areas where we have the cliffs made up of those huge
  rock formations ... clustering them should give some pretty nice results, also the largest rocks, and the cliff faces, let's have them render
  much further out"). `Tools/highland_crags_patch.py` writes it (rows get -> patch -> rows set):
  - `FMOBiomeSpeciesEntry::bLandmark`: the species goes to the landmark tier only (table in §3; `MOPCGBiomeSpawner::SpeciesPassesFilter`, tests
    `PCG.LandmarksHaveOneTier`, `PCG.LandmarkTierReachesFar`). Landmarks: every xlarge boulder (all biomes) and every HugeSandstoneCliff face.
  - `FMOBiomeSpeciesEntry::MaterialOverride`: a material for every slot (a recoloured scan); the bucket (one HISM per mesh, biome and material)
    takes it, as the cave spawner's ore tints always did.
  - **The crags**, a sub-biome of RockyHighland, AlpineHeath and AlpineRock: the four TRELLIS cliff formations (`/Game/ImageTo3D/Cliffs`, their
    Granite variant; 24-48 m since Oct 25, x0.8-1.6 of their 23-30 m meshes, was 15-31 m -- Wes: "they're also not very large compared to what i thought", the rock detail layer keeps them crisp) share clump group 8 (period 480 m) and grow only in its cores (core 0.72, ~7 % of the ground; the capped x8
    compensation packs ~10 a hectare there, so they overlap into one formation). The biome's large boulders strew the ring round them
    (core 0.6). Upright (a closed formation does not lean with the slope), any yaw, sunk 22 % of their height, solid, landmarks. The heath gets
    0.6x, the alpine rock 1.2x.
  - **Rock detail layer** (Oct 25; Wes: "the large rocks from trellis have shitty resolution on the texture"): the formations already
    carry 8192^2 bakes, but one UV atlas over a 30 m rock is <= ~270 texels / m, and tiling an atlas 2x2 would scramble it (scattered
    charts). `Content/Python/rock_detail_layer.py` gives `/Game/Materials/FabOverrides/M_MS_Base` a static switch "Enable Rock Detail"
    (OFF by default: every other item on the master keeps its shader) -- a world-aligned (triplanar) tile of the terrain's own granite
    (`rock_05`, CC0; its OpenGL normal copied as DirectX: `/Game/ImageTo3D/Shared/T_Rock05_Normal_DX`), its brightness over its mean (0.27)
    modulating the bake and its normal blended angle-correctly into the bake's; Detail Size 200 cm, albedo 0.5, normal 0.7 (group
    "Rock Detail"). On for the four formations' `MI_*_MO` (every rock-type variant inherits it). Shots from the same camera:
    `D:/MOAssets/previews/crag_close_off.png` / `_on.png` (2.5 m: the bake's smear and chart seams vs granite grain and pits).
  - **Smaller and more of them, no light through them** (Oct 25 evening, BR25-15; Wes: "still look like garbage up close and they have holes in
    them where you can see light through them ... scale them down and make more of them in clusters"): x0.4-0.8 (12-23 m; x0.8-1.6 had
    doubled the texture stretch) and 2.5x the density (`highland_crags_patch.py` CLIFF_DENSITY 0.75); the Nordic cliff keeps x0.8-1.6 and its
    own 0.3 / ha (his "these cliffs look pretty good ... properly scaled"). The image-to-3D shells are not watertight: through a gap the far
    wall's culled back faces showed the sky. Every material instance under `/Game/ImageTo3D/Cliffs` is two-sided now (a base-property
    override, `Content/Python/trellis_cliffs_two_sided.py`): the inner wall draws instead. Shots: `D:/MOAssets/previews/br25b/crags_after_wide.png`,
    `crags_after_close.png`.
  - **The Huge Nordic Coastal Cliff** (Oct 24 night, from the Dark Ruins Megascans sample, `Docs/DarkRuins_Assets.md`: 14 x 8 x 9 m, open only
    underneath) is a fifth crag formation in its own scanned material (x0.8-1.6, sunk 20 %) and grows in clusters on NordicStrand's coast (clump
    group 9, period 320 m, core 0.65, 0.35 / ha). PIE seed 777 (AlpineRock): 333 of them against 299-341 of each TRELLIS formation, as authored.
  - **The western cliff faces** (Oct 25, Dark Ruins' Desert Western Cliff Sheer XL 04, 218 x 103 x 81 m, open behind and below): rare
    landmarks on 32-65 deg AlpineRock / RockyHighland ground, x0.4-0.7, face turned downhill so `FaceDropCm` buries the back and the base
    (`Docs/DarkRuins_Assets.md`). An open-backed mesh is fine only this way (BR18-14); the open-geometry rule (below) still keeps scans out of
    the free scatter.
- **Open-faced scans** (BR25-2, Oct 24 night; `Docs/BugReports_2026-10-25.md`): `Content/Python/open_geometry_probe.py` measures every rock
  species' mesh (openness = |sum of area-weighted normals| / area, closed ~0, a one-sided sheet 0.3-1.0; `up` = which way the open side's
  normals point) and `Tools/remove_open_geometry.py` takes the open ones out of DT_Biomes, keeping each removed entry whole in
  `Art/References/ImageTo3D/2026-10-25-open-geometry/removed_species.json` for a closed rebuild. The rule (Wes: "we can handle the larger
  taller ones that have only the bottom open, the shallow depth ones are hard to get to sit properly in the terrain so avoid those ones"):
  a scan open only underneath (`up` >= 0.7) and tall (height >= 0.4 x its narrower side) STAYS (the ground hides the base); a shallow plate
  (rocky ground, shelves, root sheets) or a backless face is out. Out: 16 meshes / 31 entries (2 sideways-open sandstone faces to rebuild,
  14 shallow to avoid). A new scan species: run the probe first.

## 6. Performance rules (what keeps it fast)

- **A Nanite mesh needs a material flagged "Used with Nanite"** or a -game / cooked build draws it with the engine's default grey material
  ("Material ... missing usage flag Nanite! Default Material will be used in game"; the editor flags it on the fly, so it looks fine there).
  `enable_palette_nanite.py` (Oct 14) turned the PN meadow flowers' meshes Nanite without their base material `MA_Flowers` (97 flower
  materials): fixed Oct 19 by `Content/Python/flower_materials_nanite_usage.py` (25 warnings per run before, 0 after). Turning a mesh Nanite
  = check its materials' usage flag in the same change.

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
- **A load that grew nothing says so** (Oct 19): when the vegetation stage caps with the player standing on bare ground (0 instances within
  `mo.Readiness.NearInstanceRadiusCm`), `[MOReadiness] NO VEGETATION around <pawn> at <location> after Ns` (Error) reports how many biome-spawner
  runs finished for this load and what they planted (`MOPCGBiomeSpawner::GetRunTotals`, snapshotted when the stage begins), so the next
  empty world tells whether the spawner ran at all. Found: one new game on seed 670711536 had 0 instances in every `MO.PCG.Rings` ring; the same
  seed reloaded was fine (BugReports_2026-10-18, WATCH). Control: `mo.Readiness.NearInstanceRadiusCm 1` + `mo.Readiness.MaxPcgSeconds 3`
  + `mo.Readiness.MaxPcgHardSeconds 4` before a new game -> the line prints (137 runs, 152,886 instances planted).

## 9. Foliage wind keeps its precision far from the origin (Oct 18 2026)

- **Symptom (Wes, Oct 18):** "all the megascans trees seem to glitch every second or two ... move smoothly for like .75 seconds, then
  freeze for .25 seconds, then glitch to where the animation would have been". Not co-op, not a hitch (his host's frames were 15.7 ms
  average, 17-30 ms worst per second), not shadows (`mo.Foliage.RigidShadows 0`, `r.Shadow.Virtual.Cache 0` and a 4x
  `r.Shadow.Virtual.Cache.FramesStaticThreshold` left it unchanged), not TSR, texture streaming or the sampler (`r.MaxAnisotropy 1`,
  an explicitly linear UI texture group: unchanged).
- **Cause:** the Megascans Pivot Painter wind (`MF_AdvancedWind_Level`, called once per branch level by `MA_Foliage`: Black Alder, Common
  Hazel) and its copy in the PN plant packs (`PN_WindAnimation`: meadow flowers, wild berries) look up Epic's 64x64
  `WindTurblenceVectorAndGustMagnitude` at `frac(Time * Speed + dot(pivot, windDir))` and `frac((...) / RandomRotationChangeRate)`.
  `ms_PivotPainter2_DecodePostion` returns the pivot in WORLD space, and MO57's trees stand 5-15 km from the origin: the dot product is
  ~1e5-1e6 cm, where a float's spacing is 1/64-1/16 of a UV, so `Time * Speed` (Speed = Local Wind Noise 0.1-0.4 x MPC Wind Strength
  x 0.05 = 0.01-0.04 UV/s) can only move the lookup a whole texel at a time. Each branch level held its pose, then snapped, every
  (1/64) / Speed = 1.56 / 0.78 / 0.52 / 0.39 s. Doubling `MPC_GlobalFoliageActor` Wind Strength halved the periods (the prediction);
  the same trees 0.8 km from the origin swayed smoothly (0-1 % of the motion in jumps against 34-47 % at 11 km).
- **Fix:** `Content/Python/foliage_wind_precision.py` (editor, idempotent) reduces the world-space offset modulo 1200 before time is
  added (one `Fmod` per graph, description `MO_WindPrecision`). 1200 is a multiple of 1 (the texture's wrap) and of the default change
  rates 1-4, so both lookups read exactly the texels they read before; only the sum stays small. Rerun it after reimporting MSPresets /
  the PN packs (`PN_WildBerries` is a gitignored vendor folder: its copy is patched locally only).
- **Rule:** any material term that adds a time-driven offset to a WORLD-space position (Pivot Painter pivots, `ObjectPivotPoint`,
  `WorldPosition` demoted to float) must reduce the world term (frac / fmod by a period the consumer is blind to) BEFORE the add,
  unless it scales the world term down first (what decides it is the float spacing of the SUM, compared with one texel or one period).
- **The other wind graphs hold (N-167, measured Oct 18 night):** every remaining time + world site scales the world term down before
  the add. Epic `SimpleGrassWind` (hornbeams, and the ferns / elderberry of the `Content/Custom` Megascans masters, through
  `MF_SimpleWind` of `MS_Foliage_Material_LATEST`; the Fab graph's optional secondary sway): WorldPosition / 1024 and / 200 + time,
  so at 15 km the sum's spacing is 1.2e-4 / 4.9e-4 of a sway period. `MF_WindGust` (MSPresets and LATEST) and Fab `QMF_WindGust`:
  UV = rotate(WorldPosition x 1e-4) x tiling (0.5 / 1) + time x speed, |UV| <= ~150 at 15 km, spacing <= 1.5e-5 UV (1/16 of a texel
  of the 4096 px `T_WindNoise`, 1/2000 of the 32 px Fab one). Fab `QMF_WindAnimation` (black locust, wild / dune grass, through
  `/Game/Materials/FabOverrides/M_MS_Foliage`, which calls the `/Fab` plugin copy: same graph node for node) adds time only to a
  per-instance random in [0, 1) and vertex colour. Measured (motion_probe jump share, wind strength 6, AA off): in the world,
  hornbeams at 10.9 km 0-4 % against 0-2 % at 0.2 km, black locust at 11.6 km 0-2 % against 0-14 % at 1 km. Controlled: the same
  meshes added as runtime instances 300 m up, seen against the sky by the same camera, at 0 / 11.6 / 15 / 40 / 80 km from the origin,
  mean jump share: hornbeam sapling 4.8 / 3.2 / 5.3 / 8.5 / 1.2 %, black locust 8.7 / 7.3 / 5.3 / 8.5 / 10.5 % (the same three
  leaflet-tip patches flutter at every distance), wild grass 0.8 / 1.7 / 1.3 / 0.5 / 0.0 %, black alder sapling (`MA_Foliage` after
  the N-166 fix) 2.0 / 6.7 / 10.2 / 5.2 / 2.8 %. No distance trend, and a time-precision term left anywhere would be worst at 80 km
  (its float spacing is 7x the 11.6 km one); the spread follows each site's gust strength and leaflet flutter. No graph needed the Fmod.
- **Guard:** `MOFramework.Data.FoliageWindPrecision` (editor context) walks every /Game material function that depends on a
  PivotPainter2 package: an Add of a time-only and a pivot-only operand must take the pivot operand straight from the `MO_WindPrecision`
  Fmod (a synthetic graph checks the detector both ways first). Oct 18: 10 functions, 3 sites (MF_AdvancedWind_Level, both
  PN_WindAnimation), all reduced. A reimported vendor graph fails it: rerun `foliage_wind_precision.py` (add the function to its list).
- **Pivot Painter data textures** are data, sampled at each branch's own texel: no mips, never streamed, sRGB off, nearest filter.
  `Content/Python/pivot_painter_textures.py` (editor, idempotent) enforces exactly that on every texture in a `PivotPainter` folder and
  reports the rest: the compression differs by design (positions 16-bit `TC_HDR`, axis / extent 8-bit `TC_VECTOR_DISPLACEMENTMAP`,
  62 each). Oct 18 it fixed the 6 BlackAlder Forest_08 / Sapling_01 / Seedling_01 textures (8x2 px, mips from the texture group and
  streaming on; mip 0 unchanged, so the vertex wind reads what it read before); those trees render and sway like the rest. All 124
  live in gitignored vendor folders (BlackAlder, CommonHazel, Megascans, PN_WildBerries): rerun after a reimport.
- **How it was measured (reuse it):** a timestamped `PrintWindow` burst of the game window, sub-pixel phase-correlation tracking of
  64 px leaf / twig patches with AA off, "step share" = the share of a patch's travel made in frames over 4x its median speed (before
  34-47 %, after 0-1 %) and a jump count with TSR on (before 4-7 per 15 s on the 1.56 / 0.78 s grid, after 0):
  `python Tools/motion_probe.py capture <pid> 10 b.npz` then `track b.npz x:y,... --plot p.png`; this run's plots are in
  `Saved/WindStutter/` (not tracked). Traps met on the way: an ROI that included the HUD FPS counter (refreshes every 0.25 s) read as
  a periodic jump; `r.Test.OverrideTimeMaterialExpressions` also zeroes velocities, and TSR then blends small steps away (measure with
  `r.AntiAliasingMethod 0`); `MaterialEditingLibrary.update_material_instance` and `EditorAssetLibrary.duplicate_asset` crash a `-game`
  instance (graph edits belong in the editor). Read the **jump share** (N-167), not the step share, on mostly-still foliage: the travel
  made in frames over 4x the median of the 10 frames around them, so a staircase counts and a gust ramp does not (the N-166 captures:
  before 35-37 %, after 0 %; a gusty hornbeam canopy read 14-57 % step share but 0-4 % jump share). `track` also reports how many jumps
  fall on hitch frames (frame pacing, not the material). For an A/B where only the distance from the origin changes, add runtime
  instances of the mesh to a loaded PCG instanced-mesh component (`add_instance`, nothing saved) high in the sky at each distance and
  aim `MO.Cam.LookAt` at them from the same offset; pick patches by texture on the near frame and reuse them. Keep the camera
  >= 2.5 m from the foliage you measure (narrow the FOV instead): the camera fade dithers leaves 40-160 cm from the lens with a
  pattern that changes every frame, which the tracker reads as jumps at any distance from the origin (wild grass at 1.2 m: 25-64 %;
  at 3 m: 0-2 %).
