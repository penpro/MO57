# Terrain Generation from C++ — Exploration (2026-10-09) + implementation status

## Implementation status (2026-10-09, later the same day)

**Built (S0, S1 minus the in-editor graph, and the PCG half of S2):**

| Piece | Where | State |
|---|---|---|
| Pure generator (climate, height, biome; deterministic, IEEE-exact ops only) | `MOFrameworkCore/Public/MOWorldGen.h`, `Private/MOWorldGen.cpp` | built, 8 automation tests green |
| Tuning (Project Settings -> MOFramework -> World Generation) + snapshot builder/publisher | `MOWorldGenSettings.{h,cpp}` | built; published at module start, before every voxel runtime (`RegenerateVoxelWorld`), and by the PCG spawner |
| **The Voxel node `MO Terrain Sample`** | `MOTerrainFunctionLibrary.{h,cpp}` | built; registered; its UFUNCTION translates to a valid graph node (2 inputs, 7 outputs, all pin types valid -- checked by `MOFramework.WorldGen.TerrainNode`) |
| PCG biome spawner on the shared generator (`bUseWorldGenerator`, default on; legacy noise kept behind the flag) | `MOPCGBiomeSpawnerSettings.{h,cpp}`, `UMOBiomeDatabaseSettings::ResolveBiomeAtWorld` | built; live P2 biome gate (`Content/Python/test_biomes_p2.py`) 5/5 |
| PNG map dump (look at a whole world headless) | `MOFramework.WorldGen.DumpMaps` -> `Saved/WorldGen/*.png` | built |
| Per-biome ground material | `FMOBiomeDefinitionRow::GroundSurfaceType` (new, empty) | built; needs surface-type assets authored in the editor |

**NOT done / NOT verified:** (1) the node has not been placed in a real Voxel graph and run in PIE or a packaged build -- see "Wiring it up"
below; (2) generator-vs-terrain agreement in a live world (impossible until (1)); (3) cross-CPU determinism probe (S4); (4) the density /
caves node (S3), macro bake / rivers (S5); (5) smooth per-biome material cross-fades; (6) slope-banded biomes are resolved only by the PCG
spawner (the node assumes flat; see `MOWorldGen.h` PITFALLS).

**Defaults I chose (design forks, section 10):** heightfield base + (later) volume carving; normalised 0..1 moisture/temperature with a lapse
rate (physical units deferred -- the existing DT rows use 0..1); the existing priority-ordered band rows kept for biome resolution (nearest-match
cells deferred); generator in `MOFrameworkCore`; own C++ noise.

### Wiring it up (one-time, ~1 minute in the Voxel graph editor)

1. Content Browser -> right-click -> Voxel -> Voxel Height Graph, e.g. `VHG_MOTerrain`. Delete the pre-seeded Advanced Noise 2D and Make Box 2D From Radius nodes.
2. Add a graph parameter of type Seed named exactly `Seed` (`MOWorldSeedSubsystem` sets it by that name).
3. Add `Get Position 2D (double)` with Space = World. Add the node **MO Terrain Sample** (category MO|Terrain).
4. Wire: position -> `WorldPosition`; `Seed` parameter -> `Seed` (directly: no Mix Seeds). Then `Height` -> OutputHeight.Height, `HeightRange` -> OutputHeight.HeightRange
   (its default of +-10 m would flatten the mountains), `Bounds` -> OutputHeight.Bounds. `SurfaceType` is optional (empty until biome rows get a `GroundSurfaceType`).
   `BiomeIndex` / `Moisture` / `Temperature` can feed float metadata layers for stock PCG graphs.
5. Point the level's height stamp at `VHG_MOTerrain`; its transform must be the identity (positions in, heights out are absolute world values).
6. Look first without launching anything: `python Tools/ue.py auto --filter MOFramework.WorldGen.DumpMaps`, then open `Saved/WorldGen/world_seed12345_relief.png`.

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
