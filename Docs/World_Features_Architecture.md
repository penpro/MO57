# World Features Architecture

> **Covers:** points of interest and other "kinds of things" placed in the world as self-contained compositions (Electric Dreams
> assemblies, rock spires), the deterministic placer, the terrain that rises to meet a composition, and the unifying design for caves /
> rivers / POIs / landmarks / ore / player builds (mostly still a plan).
> **Current state (2026-10-17):** only the `Many` policy with composition actors exists (see "As built" below); the per-site seed reaches
> Blueprint compositions (Wes, Oct 16: "fix it, I don't care what it changes"); the cliff / embankment assemblies are being iterated until
> they work every time (Wes, Oct 16); real cliff WALLS are an open design fork (DECISIONS F9).
> **Entry points:** `DT_WorldFeatures` (`FMOWorldFeatureDefinitionRow`, `MOWorldFeatureTypes.h`), `FMOWorldFeaturePlacer`,
> `UMOWorldFeatureSubsystem`, `AMOWorldFeatureActor`, `FMOFeatureTerrain`, PCG node `MO Conform To Ground`. **Dev verbs:**
> `MO.WorldFeatures.Dump`, `.Grounding`, `.GroundOverride <Id> <path.json|clear>`, console words `poi` / `biome`. **Tests:**
> `WorldFeatures.Placer`, `.SlopeAlign`, `.ChooseVariant`. **Open items:** `Docs/BACKLOG.md` § PCG and world features.
> **Rule (Wes, Oct 14):** "build entire pcg POIs that have all these things self contained so they're very fine tuned and then spawn the larger
> POIs ... rather than just spawning a bunch of one off instances": new world content = one composition (graph or BP) + one row; never hero
> content as scatter species. Measure a composition before placing it. Traps: `Docs/TRAPS.md` § 10 (World features and content import) (DT JSON round-trip empties
> TMaps, `FMOWorldGenParamsRef` default is fatal, collision counts read -1 in PIE, `unreal.Rotator(roll, pitch, yaw)`).

**Status:** design plan, partly built -- see "As built" just below. This is the
target abstraction so we don't ship cave-system, river-system,
POI-system, landmark-system as four piecemeal codebases.

## As built (Oct 14 2026)

Only the `Many` policy with composition actors exists. Everything else below is still the plan.

- **Catalog:** `DT_WorldFeatures` (`FMOWorldFeatureDefinitionRow`, `MOWorldFeatureTypes.h`): the composition actor class + actor property values
  (ImportText strings), biomes, density / spacing / footprint / priority, slope / height above sea / footprint relief, sink, scale, spawn distance.
- **Placer:** `FMOWorldFeaturePlacer` -- pure and deterministic (every co-op machine computes the same sites): one jittered candidate per
  SpacingCm square, the row's own rules, then higher-priority footprints win. **Slope-aligned rows** (`bAlignToSlope`): the slope is the plane
  through 8 generator heights on the footprint's edge (`MeasureFootprintPlane`), relief is the deviation from that plane, and the yaw turns
  the composition's `AuthoredDownhillYawDeg` to face the terrain's downhill (+- `AlignYawJitterDeg`). Tests `WorldFeatures.Placer`, `.SlopeAlign`.
- **Registry:** `UMOWorldFeatureSubsystem` spawns the actors locally near every player (never replicated), despawns past 1.25x the spawn
  distance, writes the site seed into the actor's PCG components; the biome scatter skips every footprint.
- **Generic composition actor:** `AMOWorldFeatureActor` -- a PCG component + `Variants` (PCG graphs); the site seed picks one
  (`ChooseVariant`, test `WorldFeatures.ChooseVariant`). A PCG-only POI is a graph + a row: no Blueprint, no code.
- **Rows (Electric Dreams, ElectricForest):** ED_CliffAssembly / ED_EmbankmentAssembly / ED_SmallAssembly (ED's own BP_PCG_*Assembly, live PCG),
  **ED_RockSpire** (`PL_LargeCliff_01b/01c`: ~35 m mossy rock formations with cliff vines, 500-830 instances), **ED_CliffWall disabled** (below).
- **Finding -- the demo's "LargeCliff" sets are not walls.** `PL_LargeCliff_02b..02e` are ONE ~35 m formation plus ~8k dressing pieces laid on the demo
  landscape's 15 deg, 84 m-deep hillside (fitted: authored downhill ~31 deg in the set's frame, ground ~21 m above the root). On our terrain, even on a
  slope-aligned site, half the dressing is buried and what shows is the same rock as the spire, for 155 ISM components. A real cliff WALL needs its own
  composition (massive cliff meshes chained along the contour of a steep slope, vines on the face, talus at the foot) or generator crags dressed
  with these meshes -- logged as a design fork, not built. Graphs for every set are in `/Game/WorldFeatures/Cliffs/` (reference the gitignored ED content).
- **Standing on our terrain:** the `MO Conform To Ground` PCG node (after Copy Points, before the mesh spawner) spreads a composition's BASE pieces
  (pivots within FollowBelowCm of the root, unscaled) by BaseSpread, measures each base piece's lowest mesh point against the generator ground (+ EmbedCm x scale),
  moves the WHOLE composition by the median correction (the column stays on its foot) and settles each base piece the rest of its own way. Needed because
  Electric Dreams authored its assemblies' floor ~2.6 m above the root: a 4x outcrop floated 7-11 m. Row `SinkCm` now scales with the site scale, and the
  placer checks slope / relief / sinkholes over the SCALED footprint.
- **The terrain under a composition (Oct 16 2026, #15 / #19):** a row's `AuthoredGround` (`FMOWorldFeatureGround`: a 1 m grid of heights + weights
  in the composition's unscaled frame) makes our terrain RISE to the ground the composition was made for, never sink. `FMOFeatureTerrain::Apply` runs
  where the height is made (the MO Terrain voxel node), so rendering, collision, every LOD and every co-op machine agree; sites come from a private
  `FMOWorldFeaturePlacer` over the full catalog (same priorities as spawning), found per 1 km tile and kept (thread-safe, thread-local last tile).
  Electric Dreams' cliff / embankment assembly (`BP_PCG_LargeAssembly`) turned out to be a ~4 m ROCK BLOCK (cliff walls, rock tops with plants, a
  37 m tower), and the playtest's "holes" were gaps between the rocks into a hollow block: the grid fills the block's interior, inset 1.5 m from the
  rim (an outward fade built mossy mounds over the walls -- tried, rejected), and its low scree apron fades 3 m outward so it never hovers on a
  slope. Measured in game by `Content/Python/measure_feature_ground.py` (all ground-lying pieces, 25th percentile per cell, interior holes closed,
  rim inset, root-relative low cutoff; wide flat ground cover on the lower ground -- scree spreads, ground skins -- counts over its whole footprint
  at its underside, not only at its pivot: an apron measured at its pivots hovered at its edges on a slope). The per-site seed moves scree and
  plants, so a row's grid is MERGED from several sites (`merge()`, offline, from each site's `<Id><tag>.known.json`: union of cells, the lower
  ground at the 2/3 quantile across sites, the block's interior at the median) -- the Oct 16 grids come from 5 sites in 2 worlds. Tuned live with
  `MO.WorldFeatures.GroundOverride <Id> <path.json|clear>` (rebuilds the terrain, no editor). Re-measure after an Electric Dreams migration
  changes the assets. The site's PCG seed now reaches Blueprint compositions (set after
  `FinishSpawning`; `CleanupLocal` before regenerating, `GenerateLocal(force)` alone ADDS instances).
- **Per-instance grounding (`FMOCompositionGrounding`) is a census, not a fix:** `MO.WorldFeatures.Grounding [radius] [apply]` reports plants
  floating / buried and rocks with a gap under them against the shaped ground; auto-apply (`mo.WorldFeatures.Ground`) is OFF -- moving pieces one by
  one broke the compositions apart (measured, screenshots in the Oct 16 tracker).
- **Sinkhole keep-out:** `FMOWorldGen::DistanceToSinkholeRimZone` returns `NoSinkholeDistanceCm` (1e9) where there is no sinkhole. It returned 30 m,
  so every row with a footprint over 30 m (the cliff / embankment assemblies, 45 m) was rejected in every cell WITHOUT a sinkhole -- ~53% of their
  nominal density survived; the rows' densities were halved (6 -> 3, 12 -> 6 /km2) to keep the look the playtests saw.
- **Collision policy for compositions:** `Content/Python/fix_mesh_collision.py` covers every mesh the compositions spawn (manifest
  `Content/Python/data/worldfeature_meshes.json`, written in PIE by `worldfeature_meshes.py` because the meshes are PCG point attributes the asset registry
  cannot see): rocks / cliffs / logs = complex as simple (their real surface: hulls left 15-25 m drops on a 4x cliff), standing trees = trunk capsule,
  plants / vines / saplings = walk-through. The ED meshes are gitignored, so re-run it after `Tools/ede_migrate.py copy` (the tool says so).
  Verified: capsule grid over a 4x spire 398/398 on the rock; the pawn dropped on a 54 m ledge stands on it.
- **ED_RockSpire is 4x** (Wes, Oct 14: "about 4 times larger", base cliffs spread 1.3x, embedded): scale 3.6-4.4 (~140 m tall), footprint 16 m x scale,
  600 m spacing, 1 / km2, spawn 1.5 km, slope <= 12 deg, sink 0 (the node grounds it).
- **Dev:** `poi` / `biome` pickers, `MO.WorldFeatures.Dump`. Traps are in the handoff (`Docs/Handoff_2026-10-13.md`, Oct 14 sections).
- **Formations, cores and neighbours (Oct 23, Wes's BR23-3..6):**
  - `CoreFraction` (row): the solid part of a composition as a fraction of its footprint. A cluster's members pack by it: the scales are
    drawn first and placed largest first, the largest anywhere in the disc that takes it (24 tries) and every later one beside one already
    standing (cores touching x 1-1.25), never inside another's core (0.9 x the summed cores). The biome scatter plants nothing in a member's
    core and only low growth just outside it (ground cover, shrubs, bushes; no trees or rocks), out to 1.25 cores (never past the
    footprint): beyond that the biome's own forest, trees right up to the rock (Wes's vote, Oct 23: "have the trees go right up to the
    poi"; it was the whole footprint, an odd clearing of ferns round every POI). `UMOWorldFeatureSubsystem::FFootprint::LowGrowthSq`,
    `TreeClearanceCores`, the spawner's `FeatureRingOk`. 1 = the old rule (the whole footprint bare).
  - `NearFeatureIds` / `NearWithinCm` (row): placed only within that distance of a higher-priority row's site footprint, never on it.
  - Rows now: ED_RockSpire clusters 3-10 (core 0.5); ED_CliffAssembly 3-10 within 120 m (1 site / km2, was 3, core 0.5);
    ED_EmbankmentAssembly 3-10 within 120 m (2 / km2, was 6, core 0.5) -- the site densities cut so the assembly count stays near 2x;
    ED_SmallAssembly only within 60 m of a cliff / embankment site (spacing 60 m, 120 / km2), no longer a POI of its own. The spire graphs
    (`PCG_PL_LargeCliff_01b/01c`) bury the formation 125 cm x scale (was 50: 1 m at 0.8x, 5 m at 4x; the MO Conform To Ground node's
    `EmbedCm` decides it, not the row's SinkCm).
  - **Sink by the slope (Oct 23, Wes's vote: "an 'average slope' over the area ... so the lower side of the slope doesn't expose the
    bottom"):** a spire's base pieces rest on the LOWEST ground under their own footprint, not under their pivot
    (`UMOPCGConformToGroundSettings::LowestGroundUnder`, test in `WorldFeatures.ConformBaseWeight`); every other composition sinks by the
    drop from its centre to the lowest of 8 points round its solid core (`FMOWorldFeatureMember::SlopeSinkCm`, applied in the one root
    transform `FMOFeatureTerrain::MemberRootTransform`, so the spawn and the terrain shaping agree; hillside rows with `bAlignToSlope` keep
    0). "Another meter on the larger ones": spire `EmbedCm` 150 (1.2 m at 0.8x, 6 m at 4x), ED_CliffAssembly / ED_EmbankmentAssembly
    `SinkCm` 190 (was 90). ED_SmallAssembly now also stands near the spires (`NearFeatureIds` + ED_RockSpire): small assemblies lead up
    to them too.
  - **Oct 24 (Wes: "push the cliffs together closer ... overlapping the bases slightly"; "still need a lot more trees around the pois
    instead of just ferns"):** ED_CliffAssembly / ED_EmbankmentAssembly `CoreFraction` 0.35 (was 0.5): members pack with their bases
    overlapping; and round every POI out to 2 footprints (`UMOWorldFeatureSubsystem::TreeBoostFootprints`) the biome's trees grow 2.5x as
    dense (`FeatureTreeBoost`) and not in clumps (the clump field left gaps of ferns), from 1.25 core radii out as before.
  - Tests `WorldFeatures.Cluster` (largest first, every member beside an earlier one, a 0.5 core packs members at 0.54 of the summed
    footprints against 1.08), `WorldFeatures.NearOthers` (727 sites, none far or on a cliff; control without the rule: 14,959 of 15,686
    far). Seen on Wes's world (seed 3014) at his spire: a 7-member formation (3.58x down to 0.59x), forest up to the rock, 60 ground-cover
    plants in the nearest member's ring, none in its core; a small assembly 179 m from an embankment's centre (inside its reach).

- **Dark Ruins cliff compositions (Oct 25 2026; Wes: "we need to have more of them and we need stuff spawned on top of them").** The Dark
  Ruins Megascans sample's own artist assemblies, as PCG data assets (`Tools/darkruins_assembly_export.py` in the sample, the engine's
  `PCGLevelToAsset`; migrated by `ede_migrate.py --source darkruins`, gitignored): rows `DR_NordicMountain` (Mountain_01: nine Huge /
  Massive Nordic Coastal Cliff pieces, a ~60 m mountain) and `DR_NordicTallPath` (TallPath_01: a ~52 m stack with an arch, within 400 m of
  a mountain) in RockyHighland / AlpineRock / AlpineHeath / NordicStrand, 2-3 / km2 in clusters of 1-5, x0.7-1.5, seen to 2.5 km
  (`Tools/dr_worldfeature_rows.py`, from the ED_RockSpire row). Graphs (`Content/Python/make_dr_compositions.py`,
  `/Game/WorldFeatures/DarkRuins/`): the spire graph's core (Load PCG Data Asset -> Copy Points onto the site -> MO Conform To Ground ->
  spawner by Mesh) plus
  - FOOT: the sample's boulder / rocky-buildup / shelf clusters copied onto a ring round the base (the base outline at the assembly's
    root plane pushed out 2.5 m, a point every 7 m), their shallow ground skins (rocky-ground plates, sand, mud) left out at the export;
  - TOP: the highland biomes' grass, wild grass, dead shrubs, alder seedlings, a flower and small stones on the assembly's own top surfaces
    (`Content/Python/dr_dressing_points.py`: up-facing triangles that are the highest up-facing surface at their spot, 1 per 1.5 m2; ~1,050
    on a mountain), walk-through. Their points (density 0.5) are MERGED into Conform To Ground's input with the rock, so they ride the same
    median shift and stay on the rock; a density filter afterwards splits them to their own spawner (random yaw, x0.7-1.3).
  Collision: `fix_mesh_collision.py` (world-feature manifest re-collected in PIE) gives every cliff and cluster mesh its real surface; it
  now knows "Arch" and classifies a Megascans 'Smart' scan by its folder. Verified (PIE seed 777, AlpineRock): 144 spawns logged on the way
  in; a summoned mountain built 3 + 6 cliff pieces, ~1,100 dressing instances, the clusters' rocks; downward traces hit it 57-73 m up, a
  pawn dropped on top stands at 72.7 m. Shots: `D:/MOAssets/previews/dr_cluster_wide.png`, `dr_mountain_dressed.png`, `dr_top_grass_close.png`.
  Not yet: AuthoredGround for these rows (they stand on the terrain as it is); slope-aligned cliff WALLS (these are rock all round).
  **Oct 25: smaller, and three defects found by measuring them in PIE** (Wes: "the composite cliffs are scaled way too high and look
  horrible close up and just look like a ball"):
  - x0.3-0.55 (mountain, 18-33 m) and x0.35-0.6 (stack), clusters 90 / 60 m, footprint relief 15 / 12 m, embed 2 / 1.5 m.
  - The base was pulled apart: the graphs inherited the ED spire's `BaseSpread` 1.3 (Wes's Oct 14 call for the spire), so every base piece
    stood 1.3x farther out than the artist put it (measured: each spawned cliff mapped back through the actor's transform onto the sample's
    layout -- upper pieces exact, base pieces 1.3x out). The DR graphs keep the authored layout (`base_spread` 1.0).
  - The forest grew inside the rock: the footprints were the spire template's (core 3600 x 0.45, 1800 x 0.5 actor units) while the rock
    reaches 5860 / 1650 (p90) and 7940 / 4190 (max, the talus): footprint 7500 x core 0.8 and 3500 x 0.6 now (`Tools/dr_worldfeature_rows.py`).
  - The top dressing did not sit on the rock: its points were created in the component's space and never took the site's scale or yaw
    (112 of 112 plants floating, up to 77 m, at x0.33: fixed, they are copied onto the site like the assembly), and even then a third hung
    2-3 m over the terrain and the rest 0.6-1.4 m above the rock: the points came from the meshes' LOD0 triangles, for these Nanite scans
    the coarse fallback, and the low ledges took a base piece's terrain correction. **Rebuilt (N-211):** `Content/Python/dr_top_points_traced.py`
    traces the rock itself in PIE (the pieces summoned 3 km up in the transient game world, one jittered trace per 1.5 m2, the first hit from
    above kept when it faces up and lies above the conform node's base band, z > 500): 2777 points on the mountain, 10 on the stack.
    Measured on three mountains: 95-99 % of sampled plants stand on their own rock at a median gap of -5 cm (the jitter's sink).
    Shots: `D:/MOAssets/previews/dr_top_traced.png`, `dr_traced_wide.png`.
  Shots: `D:/MOAssets/previews/dr_small_close.png`, `dr_small_wide.png` (trees in the rock, before the footprints), `dr_small_wide_footprint.png`.
- **Cooking (Oct 25 2026):** the rows name their graphs and PCG data assets in TEXT, which the cooker cannot follow: no DR or ED cliff was in
  the packaged build. `FMOWorldFeatureCook` adds every package a row reaches to the cook (`Docs/Packaging.md` section 2); a new row needs
  no config edit. Test `MOFramework.WorldFeatures.CompositionsAreCooked`.

**Anchored in:** CLAUDE.md Engineering Principle #2 ("If N systems need
the same behavior, build one abstraction — not N copies") and #9
("Production-ready means extension without modification").

---

## The unifying claim

**Everything that adds non-default shape, content, or meaning to a region
of the world is a World Feature.** That includes, but is not limited to:

| Class of thing | Example instances |
|---|---|
| Procedural terrain modifications | Cave systems, river networks, sinkholes, beaches |
| Natural landmarks | Boulder fields, dolmens, lone trees, cliff faces |
| Resource concentrations | Ore veins, salt licks, clay pits, fishing holes |
| POIs | Ruins, abandoned camps, shipwrecks, ancient roads |
| Settlements | Wandering NPC camps, raider outposts, established villages |
| Player constructions | Shelters, walls, farms, workshops, tamed clearings |
| Sculpt history | Player-dug caves, terraformed flats, mined-out veins |

Treating these as separate systems is a trap: they all share the same
core needs (catalog, placement, persistence, save/load, PCG suppression
of overlapping foliage, gameplay queries like "what's nearby?"). Build
one abstraction with four moving parts; let every feature class plug in.

---

## The four primitives

### 1. Catalog — `DT_WorldFeatures` (DataTable)

A row per feature TYPE. The row describes:

- **Identity**: `FeatureId`, `DisplayName`, category tags
- **Geometry**: what kind of stamp / actor / metadata write this places
  in the world (see Stamp Kinds below)
- **Instance Parameters**: per-instance randomized values (basin radius,
  rock count, mesh-variant selection from a pool) — these make every
  placed instance unique without authoring N distinct rows. The values
  are seeded from `(WorldSeed, FeatureId, InstanceLocation)` for
  determinism. Stamps and Side Effects reference these by name.
- **Scatter rules**: how it's placed (density, min-distance, biome
  restriction, elevation band, slope range, distance-from-water,
  required metadata, preferred metadata, etc.)
- **Side effects**: foliage suppression radius, metadata to write,
  ambient sound zone, PCG seed injection into another graph,
  encounter table to roll on first approach
- **Persistence**: per-instance saved? shared global state? both?
- **Modding hooks**: like DT_ItemDefinitions, modders can overlay rows
  to add new feature types without touching C++

Catalog rows fall into four **instance policies**:

| Policy | Meaning | Example |
|---|---|---|
| `Many` | PCG scatters as many as density allows | Boulders, ore veins, small ruins |
| `Generated` | Placement rule fires once per world but produces variable output | River network, cave system |
| `Singleton` | Exactly one per world, location determined by rules | "The Old Tower" (campaign anchor) |
| `Marker` | Hand-placed via level marker actor, no PCG | Bespoke dungeons, test fixtures |

### 2. Placer — how features get into the world

**Procedural** (`Many`, `Generated`, `Singleton` policies): one new
PCG node, `MO World Feature Spawner`, reads `DT_WorldFeatures` and
scatters per the scatter rules. Sits next to our existing
`MO Resource Spawner` in PCG graphs.

**Hand-placed** (`Marker` policy): one new actor class,
`AMOWorldFeatureMarker`. Designer drops it in the level, picks a
FeatureId from a dropdown. On world init, the subsystem reads markers
and applies their features.

**Player-built** (no catalog row — actually built by gameplay): the
building system already creates persistent world content. We register
each placed building with the same registry described below, so saved
buildings are just "features the player placed." This unifies save/load
between proc-gen content and player content.

**Player-sculpted** (Dig / Raise / Flatten via Terraforming Component):
each completed sculpt action registers a `SculptScar` feature instance
in the registry. This is already partly built — `UMOTerrainModification
Subsystem` tracks modified zones for foliage-cleanup purposes. Promoting
those zones to feature instances makes save/load + PCG queries fall out
for free.

### 3. Registry — `UMOWorldFeatureSubsystem` (world subsystem)

Runtime authority for "what features exist in this world right now."
Queries:

- `FindFeaturesNear(Location, Radius, OptionalFilterTags) → TArray<Handle>`
- `IsLocationInsideFeatureOfType(Location, FeatureCategory)` → for
  gameplay rules ("am I inside a cave?", "am I in a POI?")
- `GetFeatureMetadata(Handle)` → the row data + per-instance overrides

The registry maintains a spatial grid (like `UMOTerrainModification
Subsystem` already does) for O(1)-ish queries. PCG nodes, AI
behaviors, quest triggers, ambient audio, all talk to it through this
one API.

### 4. Persistence — features through save/load

Per-instance feature state is small: feature row name, transform,
optional override params, optional "modified by player" flag. The
registry serializes the whole list via `BuildSaveData /
ApplySaveDataAuthority` (same pattern as our other subsystems —
`UMOPersistenceSubsystem` orchestrates).

Crucially: **the Voxel Plugin's own sculpt save handles the actual
voxel mesh data.** Our feature registry just records "this feature was
here so don't re-roll it on load and so PCG knows to suppress foliage."
Two layers, one save trip.

---

## Stamp kinds (what a feature actually does to the world)

A feature row picks **one or more** of these geometry primitives:

| Stamp kind | Implementation | Used by |
|---|---|---|
| `VolumeGraphStamp` | `FVoxelVolumeGraphStamp` referencing a `UVoxelGraph` | Cave systems, sinkholes |
| `HeightGraphStamp` | `FVoxelHeightGraphStamp` referencing a `UVoxelHeightGraph` | Local terrain perturbations (cliff line, plateau) |
| `HeightmapStamp` | `FVoxelHeightmapStamp` from a texture | Authored hills, valleys |
| `SplineStamp` | Spline path + cross-section profile | Rivers, roads, ravines |
| `MeshStamp` | Voxelized static mesh, additive or subtractive | Boulders, dolmens, ore veins, building parts |
| `MetadataWrite` | Float/vector/int metadata layer override | Biome zone tags, "POI here" flags |
| `ActorSpawn` | One or more `AActor` spawns | NPCs at camps, chests in ruins |
| `PCGSeed` | Inject N points into a specific PCG graph at this location | Foliage hotspots, mushroom rings |

A feature is the *combination* of these. A "Small Roadside Ruin"
might be: 3 MeshStamps (broken walls), 1 ActorSpawn (lootable chest),
1 MetadataWrite (`VFM_POI_Ruin = 1`), foliage-suppression radius 8m.

A "River" is: 1 SplineStamp (carve channel), 1 MetadataWrite along the
spline (`VFM_WaterChannel = 1` for fish-spawn PCG), 1 ActorSpawn at
slow-flow points (visual fish school decorator).

---

## Worked examples

### Freshwater spring (one row, `Many` policy, parametric + composite)

This is the **canonical Phase 1 feature** — it exercises every primitive
(composite stamps, per-instance variance, actor spawn, metadata, PCG
side effect, persistence) and delivers real gameplay (freshwater
supply, the first step out of the "all available water is salt water"
trap).

```yaml
FeatureId: poi_freshwater_spring
DisplayName: "Freshwater Spring"
InstancePolicy: Many

InstanceParameters:
  BasinVariant:   { type: int,   range: [0, 3] }        # which basin mesh from pool
  BasinRadius:    { type: float, range: [200, 500] }    # cm; 2m..5m
  BasinDepth:     { type: float, range: [40, 120] }     # cm
  RockCount:      { type: int,   range: [3, 8] }
  RockJitterDeg:  { type: float, range: [0, 30] }
  FlowRate:       { type: float, range: [5, 20] }       # litres per hour
  ClarityFactor:  { type: float, range: [0.6, 1.0] }    # affects safe-to-drink-raw

Stamps:
  # 1. Subtractive basin — picks one of 4 voxelized bowl meshes
  - kind: MeshStamp
    meshFromPool: [VSM_SpringBasin_00..03]
    selector: BasinVariant
    behavior: Subtractive
    scale: <BasinRadius / 250>     # base mesh authored at 2.5m radius
    smoothness: 30                 # smooth blend into surrounding terrain

  # 2. Rocks around the rim — RockCount instances ringed around basin
  - kind: MeshStamp
    meshFromPool: [VSM_SmallRock_00..05]
    behavior: Additive
    placementRule: ringAroundCenter
      radius: <BasinRadius + 80>
      angleJitter: <RockJitterDeg>
      count: <RockCount>
      perRockScaleRange: [0.7, 1.3]   # individually scaled
      perRockYawRandom: true

SideEffects:
  # 3. Water surface + interaction actor
  - kind: ActorSpawn
    actorClass: AMOFreshwaterSpringActor
    transform: relative (0, 0, <-BasinDepth + 10>)   # 10cm below basin rim
    spawnParams:
      MaxFillRate_LitresPerHour: <FlowRate>
      ClarityFactor: <ClarityFactor>
      WaterSurfaceRadius: <BasinRadius - 30>

  # 4. Freshwater metadata so PCG flora and gameplay queries can see it
  - kind: MetadataWrite
    layer: VFM_FreshWater
    value: <FlowRate / 20.0>       # 0..1 normalized
    radius: <BasinRadius + 50>

  # 5. PCG flora hotspot — moss, ferns, water-loving plants nearby
  - kind: PCGSeed
    targetGraph: PCG_LocalizedFlora
    radius: 1500                   # 15m of dense flora around spring
    pointCount: 30
    extraTags: [moisture_loving, near_freshwater]

  # 6. Foliage suppression in the basin itself (no grass in the water)
  - foliageSuppressionAtCenter: <BasinRadius - 20>

  # 7. Optional: animal at the water (deer drinking, etc.)
  - encounter:
    onFirstApproach: rollEncounterTable(DT_SpringEncounters)
    chance: 0.4

ScatterRules:
  density: 0.000003 per square meter      # ~3 per square km
  biomeTags: [Forest, Plains, Hills, Mountains_Lower]
  elevationRange: [500, 12000]            # not at sea level, not on peaks
  slopeMax: 12deg                         # springs need flat-ish ground
  distanceFromSeaMin: 5000                # 50m from coast — no salt influence
  minDistanceFromSameFeature: 30000       # 300m between springs

PersistInstance: true
# Saved per-instance state:
#   - discovered: bool
#   - cumulativeLitresDrawnByPlayer: float
#   - currentFillLevelLitres: float  (regenerates at FlowRate)
#   - encounter rolled?
```

**What makes each spring unique** — the seven `InstanceParameters` give
roughly `4 × continuous × continuous × 6 × continuous × continuous ×
continuous = effectively infinite` distinct configurations, but only **4
basin meshes + 6 rock meshes** of authored content. The placement rule
(ringAroundCenter with jitter) gives spatial variety without authoring
fixed rock arrangements. The result: every spring the player finds looks
distinct, but the asset count stays manageable.

**Why this is the Phase 1 feature** — it touches every primitive in the
abstraction, so if Phase 1 ships it cleanly, the next ten features are
just catalog rows. If Phase 1 can't ship it cleanly, we know the
abstraction needs to be reshaped before we add caves or rivers on top.

### Cave system (one row, `Generated` policy)

```yaml
FeatureId: cave_network_default
DisplayName: "Underground Cave Network"
InstancePolicy: Generated
Stamps:
  - kind: VolumeGraphStamp
    graph: VVG_Caves_RidgedPerlin
    parameters: { Seed: <world seed via Mix Seeds salt=20>, DepthFloor: -500, DepthGate: 5000 }
ScatterRules:
  - one stamp per world, world-sized bounds
SideEffects:
  - kind: MetadataWrite
    layer: VFM_CaveSurface
    radius: 200  # cm; the band of voxels around the carved isosurface
PersistInstance: false  # the seed already determines the shape; no per-instance state to save
```

This is exactly Task #118 (build procedural caves) but expressed as a
World Feature row. The graph stays as `VVG_Caves_RidgedPerlin`; the
catalog row is the wrapper that knows "this is a cave feature, here's
how to scatter it, here's the metadata it writes."

### Iron ore vein (one row, `Many` policy)

```yaml
FeatureId: ore_vein_iron_shallow
DisplayName: "Iron Ore Vein"
InstancePolicy: Many
Stamps:
  - kind: MeshStamp
    mesh: VSM_OreVein_Iron  # voxelized blob mesh with VMI_IronOre material
    behavior: Additive
    smoothness: 50
ScatterRules:
  density: 0.0001 per square meter on cave walls
  requiredMetadata: VFM_CaveSurface > 0.8
  elevationRange: [-5000, 0]
  minDistanceFromSameFeature: 1500  # cm
SideEffects:
  yieldOnMine: { item: "IronOreChunk", baseCount: [2, 4], scalesWith: BrushVolume }
PersistInstance: true  # save which veins have been mined out
```

This is Task #119 (cave ore via metadata + PCG) but as a feature row.
The PCG graph that places it just calls `MO World Feature Spawner` →
the spawner reads catalog → reads metadata → reads density → drops
stamps. Adding a new ore (copper, silver, gold) is one new row, zero
new C++.

### River (one row, `Generated` policy)

```yaml
FeatureId: river_default
DisplayName: "River"
InstancePolicy: Generated
GeneratorRule:
  source: peakHeightMap > 0.8 OR springSpawnPoints
  flow: gradient descent through height field
  terminate: at sea level OR another river OR lake
Stamps:
  - kind: SplineStamp
    profile: USplineProfile_RiverChannel  # 4m wide, 1m deep, smooth banks
    behavior: Subtractive on height
  - kind: MetadataWrite
    layer: VFM_WaterChannel
    radius: 300  # cm around spline
SideEffects:
  spawnAtSlowFlow: { ActorClass: AFishSchoolDecorator, density: 0.0005 per spline meter }
  foliageSuppressionFromSpline: 200  # cm
PersistInstance: false  # path determined by seed + height field; deterministic
```

Note this one needs a custom **generator rule** — splines aren't pure
scatter, they're pathfound through the heightfield. That's fine; the
catalog row points at the rule and the rule produces SplineStamps that
flow into the same placement pipeline as everything else.

### Roadside ruin (one row, `Many` policy, composite)

```yaml
FeatureId: poi_roadside_ruin_small
DisplayName: "Small Roadside Ruin"
InstancePolicy: Many
Stamps:
  - kind: MeshStamp; mesh: VSM_Ruin_BrokenWall_01; transform: relative (0,0,0)
  - kind: MeshStamp; mesh: VSM_Ruin_BrokenWall_02; transform: relative (300,-150,0); rotation: 30deg
  - kind: MeshStamp; mesh: VSM_Ruin_Floor_01;     transform: relative (0,0,-50)
ScatterRules:
  density: 0.000005 per square meter
  biomeTags: [Forest, Plains]
  elevationRange: [0, 8000]
  slopeMax: 15deg
  minDistanceFromSameFeature: 100000
SideEffects:
  - kind: ActorSpawn
    actorClass: AMOLootChest
    spawnTable: DT_RuinLoot_Tier1
    transform: relative (50, 0, 50)
  - kind: MetadataWrite; layer: VFM_POI_Ruin; radius: 600
  - foliageSuppression: 400
  encounter:
    onFirstApproach: rollEncounterTable(DT_RuinEncounters_Tier1)
PersistInstance: true  # save which ruins have been looted
```

This is the "compose multiple primitives" case. The composite is just
a list — no special "composite feature" class is needed.

### Player-built shelter

```yaml
FeatureId: player_built_<unique>
# auto-generated per placement, references a "building part" sub-catalog
InstancePolicy: PlayerBuilt
Stamps:
  - kind: MeshStamp; mesh: <whatever the player picked>; transform: <player placement>
SideEffects:
  collisionProfile: StructuralPart
  foliageSuppression: <part bounds>
PersistInstance: true  # obviously — these are saves
```

Player-built features flow through the same registry, the same save
path. The "feature registry" already has the building system's
persistence story by default.

---

## Comparison to existing systems

Walking through what we already have and how it slots in:

| Existing | What it does today | World Features story |
|---|---|---|
| `MOPCGResourceSpawnerSettings` + `DT_ResourceNodes` | Spawns trees / bushes / rocks via PCG, tags them for interaction | A special case of `MO World Feature Spawner` with category "Resource". Could be merged or could stay separate (they're already DataTable-driven; refactor not urgent). |
| `MOPCGItemSpawnerSettings` | Spawns world items | Same — items are features with `ActorSpawn` stamp kind. |
| `MOPCGSpawnPointSettings` | Spawn-point markers | Could be features with `ActorSpawn` kind + spawn-table side effect. |
| Building system | Places building parts, persists them | Already operates on the same pattern (parts have definitions, placements persist). Plumb the persistence through `UMOWorldFeatureSubsystem` and the building system becomes a feature producer. |
| `UMOTerrainModificationSubsystem` | Tracks modified zones for foliage cleanup | The zones become `SculptScar` feature instances. The cleanup pass is one consumer of the registry; PCG suppression is another. |
| `MOTerraformingComponent` | Applies voxel sculpts | Produces `SculptScar` features on completion. Side effects already pipe through Voxel Plugin's own sculpt save. |
| Cave VVG (planned, Task #118) | Procedural caves | A catalog row of `InstancePolicy: Generated, kind: VolumeGraphStamp`. |
| Ore PCG (planned, Task #119) | Cave ore | A catalog row of `InstancePolicy: Many, kind: MeshStamp`. |
| Rivers (future) | Spline-driven river network | A catalog row of `InstancePolicy: Generated` with custom generator rule. |
| POIs (future) | Ruins, camps, set pieces | Catalog rows of `Many` or `Marker` policy with composite stamps. |

**The migration story:** none of the existing systems need to be torn
out. New systems (caves, ore, rivers, POIs) go directly through the
World Feature abstraction. Old systems (resource spawning, building)
adopt it incrementally as they touch related areas — but the abstraction
is shaped to absorb them when the time comes.

---

## Build order (do NOT do all at once)

Standard "smallest end-to-end slice first" rule. The slice that
exercises every primitive:

### Phase 1 — Skeleton + freshwater spring (the canonical first feature)
1. `FMOWorldFeatureDefinitionRow` struct, `DT_WorldFeatures` DataTable.
   Schema covers Identity / InstanceParameters / Stamps / SideEffects /
   ScatterRules / PersistInstance.
2. `UMOWorldFeatureSubsystem` skeleton: registry, BuildSaveData /
   ApplySaveDataAuthority, FindFeaturesNear query, save/load
   integration with `UMOPersistenceSubsystem`.
3. `UMOPCGWorldFeatureSpawnerSettings` PCG node: scatter by density +
   biome + elevation + slope + distance-from-sea, spawn `MeshStamp`
   kind from mesh pools with `selector` parameter, support composite
   stamp lists with `placementRule: ringAroundCenter`, write metadata
   side effects, support PCGSeed injection into another graph,
   support ActorSpawn side effects.
4. `AMOFreshwaterSpringActor` C++ class — water surface visual + audio +
   `IMOInteractionInterface` ("Drink", "Fill container"), tracks
   `CurrentFillLevelLitres`, regenerates at `MaxFillRate_LitresPerHour`.
5. Author authoring assets: 4 voxelized basin meshes (`VSM_SpringBasin_00..03`),
   6 small rock meshes (`VSM_SmallRock_00..05`), water surface mesh +
   material, looping water flow audio.
6. **One catalog row**: `poi_freshwater_spring` per the worked example
   above. Add `MO World Feature Spawner` to the level's main PCG graph.
   Verify: ~3 springs per square km, every one looks visually distinct,
   foliage clears in the basin, dense flora ring around it, drink
   interaction works, save/load preserves discovered + drawn-water
   state.

This is a meatier slice than "just spawn boulders" — but it gives us
**(a)** a real working POI primitive, **(b)** validation that every
abstraction primitive holds against a complex feature, and **(c)** the
opening move on water progression (freshwater exists in the world).
Budget: 1–2 weeks of focused work.

#### Phase 1 simpler fallback — landmark_boulder_small

If Phase 1 stalls on the spring feature's complexity (especially the
parametric `placementRule: ringAroundCenter` or the actor spawn side
effect), ship `landmark_boulder_small` first (one row, one mesh, no
parameters, no actor) to validate the catalog + subsystem + scatter
+ persistence path independently. Then layer composite stamps, instance
parameters, and side effects on top to bring the spring online.

### Phase 2 — Bring in the planned tasks
5. Add `VolumeGraphStamp` kind, port Task #118 (procedural caves) to
   a catalog row.
6. Add `requiredMetadata` scatter rule, port Task #119 (cave ore) to a
   catalog row.
7. Verify: caves carve, ore stamps appear on cave walls, metadata
   reads work both ways.

### Phase 3 — Composite + side effects
8. Add `ActorSpawn` and composite-stamp support. Build one small
   ruin POI feature as the test case.
9. Add encounter-on-approach side effect (rolls a table from
   `DT_*Encounters`).

### Phase 4 — Splines and generated rules
10. Add `SplineStamp` kind. Plumb a custom generator rule API.
11. Build the river network feature.

### Phase 5 — Existing-system absorption (only if Phase 1-4 prove the abstraction)
12. Migrate `MOPCGResourceSpawnerSettings` to share the World Feature
    spawn path. **Only do this if no API has had to fork** to support
    composite/generated features — otherwise resources stay separate.
13. Plumb building-system placements through the registry for unified
    save/load.

Don't do Phase 5 until Phase 1-4 are stable and we've verified the
abstraction holds for at least 8–10 distinct feature types. The risk
of premature unification is locking the abstraction into the shape of
its first few use cases.

---

## What this gets us

1. **One save/load story** for every persistent world content. Caves,
   ore, ruins, buildings — all flow through one registry, one
   serialization path, one query API.
2. **Adding a new POI type is one catalog row.** No new C++, no new
   PCG node, no new save-game struct. Modders can do it.
3. **AI / quests / ambient audio / VFX** all query one subsystem to
   ask "what's near me?". No per-system spatial indexes.
4. **The Voxel Plugin stamp abstraction is exposed cleanly to
   designers** without them having to author graph assets for every
   variation.
5. **Player-built content unifies with proc-gen content** at the
   registry layer. A player-built shelter and a proc-gen ruin are
   both "features that exist at coordinates X" as far as the
   query API is concerned. Quest triggers, AI patrol routes,
   ambient music regions — all work the same way for both.

---

## Design context — water progression

The freshwater spring is the first move in the water progression. The
full progression (build toward, not all at once):

| Source | Availability | Player effort | Yield |
|---|---|---|---|
| **Salt water (z=0 ocean)** | Abundant | None | Undrinkable raw — causes hydration penalty + sickness. Requires distillation. |
| **Freshwater spring** (Phase 1) | Scattered POI, ~3 per km² | Find it; drink in-place or fill containers | Small per-spring flow rate. Limited. Can't supply a settlement. |
| **River** (future, Generated feature) | Procedural networks | Find one near your camp | Plentiful but localized; flowing water needs no boiling for clarity |
| **Lake** (future, Generated feature) | Fewer than rivers; basin fill points | Found near valleys | Large finite supply; still water needs care for safety |
| **Rain collection** (future, player-built) | Anywhere with sky exposure + container | Build a collector | Variable yield by weather. Encourages settlement near rainfall. |
| **Snow melt** (future, high altitude) | Mountains in cold seasons | Travel + fuel to melt | Clean, requires fuel; seasonal availability |
| **Distillation** (future, crafted) | From salt water | Tier-2 cookware + fuel | Slow per-batch but works anywhere coastal |
| **Pawn-tended supply** (future, RimWorld-mode) | Whatever the colony has | Assignment + travel time | Pawn delivers from any source to a stockpile |

The spring is intentionally low-yield because that drives the
progression — solo survivors can stay alive at a spring, but a
settlement needs a river/lake nearby or rain infrastructure or
distillation tech. The "find a spring" moment is the first
clear-water reward; everything else is built around / from there.

The salt-vs-fresh distinction is **already enforceable** via the
`VFM_FreshWater` metadata layer: the drink-source actor checks for
positive metadata; ocean water has none. No need to retag the
existing voxel water — the freshwater zones are additive overrides on
a world that is salt by default.

---

## Open questions (resolve at Phase 1 implementation time)

- **Spatial grid cell size**: `UMOTerrainModificationSubsystem` uses
  1000cm (10m). Features have a wider size range than modified zones
  — probably want a bigger cell (say 50m) plus per-feature bounds for
  the precise check. Profile and tune.
- **Save size**: a world with 5000 features (boulders, ore veins,
  POIs, buildings) at ~64 bytes per row is ~300KB. Fine. But if we
  decide to save per-feature timestamps + interaction history, that
  could grow. Budget early.
- **Mod overlay vs base catalog**: modders should be able to ADD
  features (new POI types) without editing the base DataTable. The
  same overlay pattern we use for items (`UMOItemDatabaseSettings`)
  applies — `UMOWorldFeatureDatabaseSettings` with mod-overlay support.
- **Streaming**: very large worlds may need feature catalogs streamed
  in by region. Not a Phase 1 concern; design the subsystem so adding
  a region-stream layer later is non-breaking.
- **Determinism**: with seed-driven generation, every feature instance
  should be reproducible from (world seed, feature id, world location).
  The registry only stores deltas from the deterministic seed result —
  i.e., what the player changed. This minimizes save size.
- **Voxel-PCG sampling for 3D**: open question from Task #119. Resolve
  before designing the SculptScar feature path, because it determines
  whether player-mined caves can host PCG-spawned ore (probably yes,
  via metadata).

---

## Cross-references

- `Docs/Voxel_Plugin_Reference.md` §17 + §18 — the voxel-side stamp
  primitives this builds on
- `Docs/Archive/Terrain_Foundation_Plan.md` — base terrain generation (the
  canvas features get placed on)
- `Docs/Archive/PCG_Integration_Plan.md` — the PCG node patterns we already use
- CLAUDE.md Engineering Principle #2 + #9 — the design rationale this
  doc serves
