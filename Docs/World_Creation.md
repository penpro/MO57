# World creation: shape, pace, starting biome

> **Covers:** the settings a world is created with (Wes, Oct 22 2026: "a suite of 'global multipliers' for terraforming, crafting,
> gatherings, inspecting, etc so we can configure those on a per world basis and add those to the world generation menu, along with some
> knobs to adjust stuff like biome size, overall world height, etc, whatever makes sense, and then a 'starting biome' selector ... a scroll
> gallery that shows a screenshot of the biome"). Code: `MOWorldCreationTypes.h` (types, the knob table, `FMOWorldRules`),
> `MOWorldCreationWidgets.h` (knob row, biome card), `UMONewGamePanel`. UI spec: `Content/Python/ui_specs/world_creation.py`.

## 1. What a world is created with

`FMOWorldCreationSettings` = a **shape** (how the land is generated), a **pace** (how long the work takes) and a **starting biome**. Chosen once
in the New Game panel (or `MO.World.Creation` for tests), kept with the world for good: `UMOWorldSaveGame::WorldCreation`, replicated on
`AMOGameState::WorldCreation`, printed in every bug report (`WorldCreation` field) and once per world as `[MOWorldCreation] this world: ...`.
Every knob is 1 by default, which is the game exactly as authored: a default world keeps its params hash, its golden checksums and every
existing save (a save from before Oct 22 reads the defaults, which is the world it was).

| Knob | Range | What it changes (`FMOWorldCreation::ApplyShape` / the duration site) |
|---|---|---|
| Biome size | 0.5-2.5 | the climate fields' wavelengths (moisture, temperature): wider or narrower biome patches |
| World height | 0.5-1.5 | the generator's `VerticalScale`: hills, valleys and mountains alike |
| Mountains | 0-2 | massif, peak, coastal-range and old mountain amplitudes |
| Lakes | 0-2 | pond, lake and upland-lake cell chances (capped at 1) |
| Caves | 0-2 | the cave-entrance cell chance (capped at 1); 0 = no cave mouths |
| Vegetation | 0.25-1.5 | every biome species' density (`FMOWorldGenTuning::VegetationDensityScale`); above 1 costs frame time |
| Gathering time | 0.1-4 | harvest / chop / mine (`UMOHarvestSubsystem`), foraging (ground menu), gathering cover, butchering, survivor gather jobs |
| Crafting time | 0.1-4 | every recipe (`UMOCraftingSubsystem` adjusted time, the queue, the recipe panels) |
| Building time | 0.1-4 | construction (`UMOBuildProgressComponent`), the build panels |
| Digging time | 0.1-4 | terraforming (`UMOTerraformingComponent`, after the rock-hardness factor) |
| Study time | 0.1-4 | inspecting an item (`FMOInspectionActions`) |
| Skill learning | 0.1-4 | a RATE: skill experience x it (`UMOSkillsComponent::AddExperience`) |
| Growing season | 0.1-4 | a crop's days from sowing to ripe x it (`AMOCropPlotActor::GetDaysGrown`; Oct 26; the default is DECISIONS F42) |
| Creative mode (a world RULE: Off / On, default Off) | 0 / 1 | crafting and building need and consume no materials (BR25-5, Oct 24 2026: Wes, "make a 'creative mode' where crafting and building costs nothing") and since Oct 26 take no time (Wes: "in creative mode make it so building happens instantly, no build time etc", crafting the same): `FMOWorldRules::IsInstant` -> `ScaleSeconds` returns 0 for building and crafting, and a placed building is finished on the spot (`Docs/Building.md`). The one exception to Principle 11: a world rule chosen at creation, never a skip inside a survival world. Read through `FMOWorldRules::IsCreative` in `UMOCraftingSubsystem::HasIngredients`, `UMOCraftingQueueComponent` (a free entry: nothing reserved, nothing refunded on cancel) and `UMOBuildProgressComponent` (materials count as deposited). Host console: `MO.World.Creative [0\|1]` |

The knobs come in three groups (`EMOWorldKnobGroup`): Shape, Pace (time multipliers) and Rules (on / off switches, `FMOWorldKnob::bSwitch`,
shown Off / On; the pace floor does not apply to them). The panel shows Shape in one box, Pace and Rules in the other.
Principle 11 still holds: a pace never reaches zero (a 0.1 floor, also enforced on load: `FMOWorldCreation::Sanitize`), 1 is real time, and
it is a property of the world chosen before its first step, not a mid-game skip. Times below 1 are Wes's call for his worlds (Oct 22).

## 2. How it travels

- **Shape** is generator input, so it rides with the seed: `FMOWorldSeedInfo::Shape`. The host publishes it with the seed
  (`AMOGameMode::InitializeVoxelWorldWithSeed`); every machine folds it in through `UMOWorldSeedSubsystem::RegenerateVoxelWorld` ->
  `UMOWorldGenSettings::SetActiveWorldShape` before it publishes the params, so a joiner's first chunk already has it. The menu backdrop
  resets it to the default.
- **Load:** `UMOPersistenceSubsystem` applies the save's shape BEFORE comparing the generator hash (otherwise a shaped world read as
  GENERATOR CHANGED), then the game mode hands the settings to the game state.
- **Pace** is read where the time is decided, on the server, through one call: `FMOWorldRules::ScaleSeconds(Context, EMOWorldPace::X,
  Seconds)`. The UI's times use the same call, so a shown time is the real one. A new activity = one `EMOWorldPace` entry + its call.
- **Starting biome:** the new game's start search (`MOGameModeStart::FindGeneratedStart`, `MOGameMode.cpp`) asks `FMOBiomeFinder::FindNearest`
  for the spot deep inside that biome nearest the island's centre (flat, else rough ground); none in the world = the coast, as before. Only lowland biomes are starts (Wes, Oct 23: "gate the higher
  elevation biomes"): `FMOWorldCreation::IsStartingBand` -- the band's floor within 50 m of the sea and no minimum slope, so AlpineHeath,
  AlpineRock and RockyHighland are offered neither as cards nor by the spawn (test `WorldCreation.StartingBiomesAreLowland`). A start with
  the sea within 300 m faces it (`FMOWorldGen::FindWaterFacingYaw`; the strands), as the coastal start does.
- **The world's origin** (Wes, Oct 26: "have the landfall script the picks the starting location shift the entire world to 0,0 so we're
  always starting at an origin point"): floats are finest at (0,0), and the engine's hair simulation jittered at a start 17 km out
  (`Docs/Engine_Patches.md` #3). So a NEW world is generated shifted: `AMOGameMode::ChooseNewWorldOrigin` runs the start search above on the
  generator alone (seed + params, before any terrain), in the generator's own field, and that start in whole metres becomes
  `FMOWorldCreationSettings::WorldOrigin`. Every machine then generates with `FMOWorldGenParams::Origin` = it, so the start is the world's
  (0,0) (within half a metre) and the island's centre is at `-Origin` (`FMOWorldGenParams::GetWorldCentre`). The colony grows round the
  origin; the far side of the island is still up to ~120 km out (expeditions, not the base).
  - It travels like the shape: saved in `WorldCreation`, published with the seed (`FMOWorldSeedInfo::Origin`), applied by
    `RegenerateVoxelWorld` -> `UMOWorldGenSettings::SetActiveWorldOrigin`, and part of the params hash when non-zero (so a joiner with
    another origin reads as a generator mismatch). A load sets it from the save before the hash check. A world saved before Oct 26 has
    none: (0,0), its land where it always was. The menu backdrop is always (0,0).
  - The land is the same bit for bit; the PLANTS are not: PCG placement follows world coordinates, so a seed's vegetation is re-rolled by
    its origin (same biomes and densities, other spots). Both machines share the origin, so co-op agrees; a save keeps its origin, so its
    removals stay where they were. Seen Oct 26: seed 20261025's start went from bushes to an open sunlit clearing (the hair looked gold).
  - Switches: `AMOGameMode::bStartAtWorldOrigin` (default on); `MO.World.StartAtOrigin 0` (cheat cvar, set before the new game) makes the
    next world unshifted -- the A/B for "is it the origin?". Logs: `[MOGameMode] World origin: the start (x, y) of the generator's field
    is the world's (0,0) ...`, `[MOWorldSeed] host published world seed N (parameter 'Seed', origin (x, y))`.
  - Anything placed against the WHOLE island must centre on `GetWorldCentre()` and test with `IsInsideWorld()`, never (0,0) +-
    `WorldHalfSizeCm`; the generator itself is world-space (`FMOWorldGen`, `Docs/Terrain_Generation_Exploration.md` § World space).
    The game level's PCG volume is +-125 km for this (`Docs/PCG.md` §1); `RegenerateVoxelWorld` logs an Error when a runtime PCG
    volume does not cover the island.
- **The start site** (Oct 24, Wes: "the egg should be near where the first player starts"): the first survivor of a NEW world wakes beside
  the stone clone egg they grew in and its thrown-off hatch (`Content/ImageTo3D/CloneEggPod01`, `CloneEggPodHatch01`). `AMOGameMode`
  flags a new game; when its first survivor has landed (`OnPawnLandedSafely`) `UMOStartSiteSubsystem::PlaceBesideWhenReady` keeps the
  spot and facing and places the props once the world is READY (the vegetation round the spawn has collision by then).
  - Where: 3.3 m from the survivor's feet, behind them to the right (bearing 140 degrees from their facing), the egg's opening turned to them,
    the hatch between. 24 candidates round them (`MOStartSite::Candidates`, tests `MOFramework.StartSite.*`): the first with voxel ground
    under every prop's footprint, at most 60 cm of rise across one, no open water and no tree / rock / building inside a prop wins.
    None fits: the preferred spot anyway (logged).
  - Each prop sits on the lowest ground under it, pressed in a little (the egg 10 cm). Grass is kept out with a worked-ground zone per prop.
  - The props are `AMOStartSiteProp` actors: server-spawned, replicated, always relevant, so a joiner sees them.
  - They are saved (domain `StartSite`, `Docs/Persistence.md` §1) and a load spawns them again where they were. A respawn or a load never
    places a new site; a world saved before Oct 24 has none.
  - The props and the layout are Project Settings > MOFramework > Start Site (`UMOStartSiteSettings`, DefaultGame.ini). The defaults are
    the egg and its hatch.
- **New knob:** a field in `FMOWorldShape` / `FMOWorldPace` (or a world rule on `FMOWorldCreationSettings`, group Rules, `bSwitch`) + one entry in `BuildKnobs()` (`MOWorldCreationTypes.cpp`); the panel, the
  console and the clamps follow from the table.

## 3. The panel

`WBP_NewGamePanel` (960 x 860, scrolls): name, seed, the starting-biome cards (`WBP_BiomeCard`: the biome's `PreviewImage` from DT_Biomes and
an MOButton with its name; the coast first, `CoastPreviewImage`), the World knobs and the Pace knobs (`WBP_WorldKnobRow`: label, slider in
the knob's steps, value), Defaults, Back, Start. A biome is offered when its DT_Biomes row has a `PreviewImage`; the pictures are 1280x720
instance shots (`Tools/review_shots.py`) imported to `/MOFramework/UI/BiomePreviews/T_BiomePreview_<Biome>`.

## 4. Tests and checks

- Automation: `MOFramework.WorldCreation.DefaultShapeKeepsTheGenerator` (default = authored hash, shaped = its own; biome size doubles the
  climate wavelengths; height raises the top), `MOFramework.WorldCreation.KnobsClampAndPaceNeverZero`.
- The origin: `MOFramework.WorldGen.OriginEquivalence` (every world-space generator function with origin O at P equals, bit for bit, origin
  0 at P + O, and every position it returns is less O: ~6400 checks over three origins, a fractional one included),
  `MOFramework.WorldGen.OriginAtCoastalStart` (the coastal start moved to the origin is within half a metre of (0,0), same ground, same
  view). `nettest game` checks the first pawn starts within 30 m of (0,0) and the joiner uses the host's origin; `nettest hostsave` checks
  a reload keeps the saved origin.
- Console: `MO.World.Creation BiomeSize=2 Crafting=0.5 StartingBiome=Rainforest` sets the next new game; no arguments prints the pending and
  this world's settings (`[MOQUERY] WORLDCREATION`).
- Open: the Host Game panel starts a new world with the defaults (it has no creation settings of its own yet): how it should get them is
  fork F37 in `Docs/DECISIONS.md` (N-198). A joiner always gets the host's.
- Wes's vote, Oct 23: no high-elevation starts and a strand start faces the water (N-203, built: section 2). The panel itself: thumbs up.
- End to end: `python Tools/world_creation_check.py` (host + joiner: the shape reaches the joiner's terrain, a pace scales an
  inspection, the start lands in the biome, save / load keeps it all with no GENERATOR CHANGED).
