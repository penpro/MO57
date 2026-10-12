# Electric Dreams Environment: what to take for MO57 (deep dive, 2026-10-11)

Source: `D:\UEProjects\ElectricDreamsEnv` (Epic's sample, UE 5.8 build, 44 GB). Read headless with editor Python (`UnrealEditor-Cmd ... -ExecutePythonScript -RenderOffScreen`; `-nullrhi` crashes this project at startup with
"Null assigned to TNotNull") -- 18 PCG graphs, the main levels, the landscape material, the project config. Nothing was changed or copied. The graph dumps are in the session scratchpad (`ede/`, `ede2/`) and the script is
re-runnable. Web docs used for the grid-size / runtime-generation rules: Epic's "Using PCG Generation Modes" and the PCG Biome Core overview.

## 1. How the world is composed (the look)

**Three tiers, each with its own tool.**
1. *Terrain material does the big read.* One auto landscape material (`M_BGLandscape_Auto`, 137 expressions, 36 scalar parameters) blends Cliff / Ground / Mud / Mulch / Grass by SLOPE with separate "vertex" and "pixel" slope terms:
   `Slope Offset`, `Slope Hardness` and `Contrast` for each layer, plus `Height Value` and `Transition Phase`, plus a world-space breakup noise (`Breakup Tiling / Intensity / Contrast`). Per-layer tiling (cliff 2000, mud 40, mulch 20, grass 30) with
   world-aligned textures, and per-layer colour, saturation and a roughness LOW/HIGH remap -- so one set of textures reads as many surfaces. *For us:* the voxel sand / jungle-floor rules are the same idea; mud (lake basin) and a damp-ground
   layer fit as two more rules (depth below the lake surface, wetness near water) with a breakup-noise term so the edges are never a straight line.
2. *Instanced meshes do the ground scale* (trees, boulders, ferns, ground cover): PCG graphs with a weighted mesh selector (3-9 entries, weights 1-10) per layer.
3. *"Assemblies" do the hero detail*: hand-built Packed Level Blueprints (`BPP_*`: river embankment with mud + root + rock + vegetation layers, fern clusters, moneyplant, hanging vines, wall foliage, cliff walls, dried rapids) placed by PCG as
   ACTORS: ~10,600 static mesh actors + 515 fern assemblies + 349 ground-cover assemblies + ~250 vine assemblies in the main level. An artist composes a believable 5 m of riverbank once; PCG repeats it with rotation / scale / density.
   Combined with the instanced layers, repetition is hidden by the composition, not by more noise.

**Things that break up "procedural" look (all present in the sample):**
- 668 dirt / leakage DECALS (`MI_Dirt`, `MI_Leakage_8k`, mud puddle decals) on rocks and cliffs; 72 fog-card actors (`BP_FogCard`, a PCG graph of its own: sparse points 1 per 1000 m^2, 4 x 4 x 2 m extents) for depth; hanging vines on cliff
  faces; roots exposed on embankments; fallen trees; mossy rocks. None of it is a new species, all of it is "evidence of time".
- **Density is shaped, not just thresholded.** The Forest graph (412 nodes) is mostly: Normal-To-Density (slope -> density), Density Remap (e.g. in 0.8..1.0 -> out 0.25..1.0), Attribute Noise, Density Filter, Self-Pruning (spacing), Bounds Modifier (so trunks
  do not intersect), Difference / Union of layers (a lower layer is subtracted where a higher one stands: understory is cleared under big trees), and named reroutes that carry shared masks (slope, wetness, distance-to-water) to many branches.
- **Canopy vs understory samplers:** canopy uses a sparse sampler with large looseness (0.007 points/m^2, looseness 5, extents 0.8 m) so trees land irregularly; understory uses a dense one (0.125 points/m^2, looseness 1, extents 0.5 m).
  Cliff-fringe trees: 0.1 points/m^2 with 13 m extents so they stay out of each other's way.
- **Ditch / river graph:** the ditch, embankment and riverbed pieces are separate graphs driven by splines and marker actors (`BP_PCG_LocationMarker`, `PointFromPCGVolume`, `DistanceToNeighbors`, `LookAt` custom nodes): the water channel is the
  master, everything near it (embankments, roots, rocks, mud) is placed relative to it. *For us:* the generator already knows every lake / river / sea cell, so "distance to water" is the one mask to build and share.
- **Physical surfaces on purpose:** `Dirt, Grass, Gravel, LooseRock, SolidRock, Mud, Water, WaterShallow` (footsteps, bog behaviour) and two custom trace channels (`Traversable`, `ExclusionMask`: an exclusion mask that PCG respects).

## 2. Performance patterns (what actually keeps it fast)

- **Hierarchical generation with radius = 2 x grid size** at every level: grids 400 / 800 / 1600 / 3200 / 6400 / 12800 / 25600 / 51200 / 102400 / 204800 cm, radii 800 ... 409600 cm, cleanup scalar 1.1. Top partition actors are 512 m (`PCGPartitionActor_51200_x_y`).
  Epic's rules: big things (trees, boulders) on big grids, small things (grass, stones) on small grids, expensive shared work (samplers over large areas) on the Unbounded / large grid and then trimmed with Grid Size nodes; cull extra points with
  "Cull Points Outside Actor Bounds" after a big -> small hand-off; keep radii increasing as grid size shrinks; `pcg.FrameTime` 16.7 ms (game) / `pcg.EditorFrameTime` 50 ms, `pcg.RuntimeGeneration.NumGeneratingComponents` 16, pooled partition actors
  (`BasePoolSize` 100); frustum / direction-weighted scheduling (5.5+). *For us:* our biome spawner is a single custom node on one grid: moving the sparse layers (trees, boulders) to a large grid and grass to a small one is the biggest available win.
- **Merge components:** every mesh spawner has `allow_merge_different_data_in_same_instanced_components = True` (fewer components, fewer draw setups) and `warn_on_identical_spawn`.
- **Engine settings (DefaultEngine.ini):** Nanite on with raised limits (`MaxVisibleClusters 10485760`, `MaxCandidateClusters 16777216`, `MaxNodes 10485760`, `RasterIndirectionMultiplier 1.1`) for dense foliage; `r.OptimizedWPO 1` (Nanite WPO optimisation);
  `r.SceneCulling.MinCellSize 1024` (instance culling cells); landscape grass OFF (`grass.Enable 0`: PCG owns all scatter); Lumen: `HeightFog 0`, `Reflections.MaxRoughnessToTraceForFoliage 0` (foliage does not trace reflections), `TranslucencyVolume.GridPixelSize 64`;
  VSM on with quality-scaled `MaxPhysicalPages` (1024 / 1536); bloom at 50% / 35% screen percentage; TSR flicker caps; hardware RT with culling (`RayTracing.Culling 3`, radius 10000, angle 2) and Niagara excluded from RT geometry;
  virtual-texture pools sized per format; `fx.Niagara.ForceLastTickGroup 1`. Scalability `PerfIndexValues_ResolutionQuality "50 50 63 75 100"`.
- **Data layers + World Partition** per area (`DL_River`, `DL_Cave`, `DL_LookoutPeak` ...) and 256 landscape streaming proxies: streaming is by cell, expensive interior spaces are separate layers.
- Static analysis note: the sample is a showcase for high-end hardware (suggested 8-core i9 + RTX 2080 8 GB minimum); the relevant lessons are the generation structure, not its Lumen / RT settings.

## 3. Applied to MO57 (ordered by value / cost)

1. **One "near water" mask, shared by every spawner** (the sample's named reroute): the generator's sea level, rivers/ponds/basins, and upland-lake ellipses -> `distance to water` + `under water`. Planting, shoreline flora, mud and wetness all read it. (Fixes: trees in lakes.)
2. **Shore zonation from that mask**: in the water edge band, reed/rush/cattail-type plants; on the bank, sedge / ferns / damp-ground species; beyond it, the biome. Species density falls with distance from water; understory cleared under big trees (Difference).
3. **Mud / rock basin and damp bank** as voxel material rules (depth below lake surface, slope, wetness), with a breakup-noise term (the sample's `Breakup` parameters).
4. **Assemblies for the shore**: compose 5-6 shore pieces once (mud bank + roots + rocks + reeds) and let the scatterer place them along the shore line, instead of more species noise. Local source assets: `D:\UEProjects\ElectricDreamsEnv` (`Asmbly/Assemblies/BPP_*Embankment_Mud*`, Megascans surfaces / plants / rocks).
5. **Dirt / leakage decals** on boulders and cliffs, fog cards in forests and valleys, hanging vines on cliff faces: cheap, high impact; all independent of the terrain.
6. **Hierarchical grids for the biome spawner** (trees on a large grid, boulders large, grass small), radius = 2 x grid, with the cull-outside-bounds step; measure with `MO.PCG.WaterAudit` / the instance counts before and after.
7. Settings worth trying one at a time (measure each): `r.OptimizedWPO 1`, `r.Lumen.Reflections.MaxRoughnessToTraceForFoliage 0`, `r.Lumen.TranslucencyVolume.GridPixelSize 64`, `r.SceneCulling.MinCellSize 1024`, VSM page budget per quality tier.
   **Measured Oct 11 2026 (packaged build, 1080p, fixed camera, 60 s settle + 30 s bench per biome, 4090; `Tools/perf_experiments_round14.txt`): NONE is worth adopting.** avg fps Rainforest / TemperateForest / Meadow:
   base 79.5 / 72.7 / 90.6 (the TemperateForest base had a 408 ms one-off); foliage-reflection roughness 0: 81.5 / 76.9 / 89.2; translucency grid 64: 74.1 / 76.7 / 89.5; scene-culling cell 1024: 78.1 / 74.4 / 89.8;
   all three: 78.4 / 74.3 / 88.3. Differences are inside the run-to-run noise (about +-2-4 fps) and the combined set is no better than the control; draw calls and GPU ms do not move. `r.OptimizedWPO` does not exist
   in 5.8 (only `r.OptimizedWPO.AffectNonNaniteShaderSelection`), `r.SceneCulling.MinCellSize` is read-only (set it with `-ini:Engine:[SystemSettings]:` or in the ini; the engine default is 4096). The overrides
   did take effect (the log echoes `... = "0"` / `"64"`; ExecCmds values are console priority and survive the scalability re-apply). The sample's wins come from its generation structure, not from these knobs.
