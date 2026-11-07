# Caves and geology

> **Covers:** cave shape and placement, bedrock and ore bodies, what digging yields, dig time by rock, what grows/lies in caves.
> **Current state (2026-11-06):** world generator **v16** (ravines since v13 and the rocks in their walls, section 1; the built entrances' mix since v15, their bowls not round and chained since v16). Caves are level limestone galleries entered from hillsides and, since v12,
> from built entrances in karst (a doline hollow, and in it an overhang, a passage down into a chamber, or that and branches further down; F30 A, BR23-18); kept away from sea level and shores; bedrock provinces carry realistic ore bodies; both dig paths (player + survivors) yield from the
> geology through one map; dig time scales with rock hardness. Torches make caves playable (`Docs/Light_Sources.md`).
> **Entry points:** `FMOWorldGen::SampleCaveDistanceAtGround` (`MOFrameworkCore/Private/MOWorldGenCaves.cpp`), `SampleBedrock` /
> `SampleOreIn` / `FindCaveSurfaces` / `FindOutcrops` / `SampleDig` (`MOWorldGenGeology.cpp`), `FMOGeologyYield` (MOFramework),
> `UMOPCGCaveSpawnerSettings` (MO Cave Spawner), `/MOFramework/Data/DT_CaveScatter`, graph `Content/Penumbra/Maps/VVG_MOCaves`.
> **Dev verbs:** `MO.WorldGen.FindCaveEntrance [r]`, `.FindCave [r]`, `.CaveSample x y z`, `.Bedrock`, `.FindOre <kind> [r]`,
> `.Checksum`, `.Sinkholes [r]`, `MO.PCG.OpeningAudit [r]` (what stands round the built entrances); cvar `mo.PCG.CaveDensityScale`. **Tests:**
> `MOFramework.WorldGen.Sky.Ravines`, `MOFramework.WorldGen.Caves`, `.GalleriesStayLevel`, `.CavesStayDry`, `.CaveEntrances`, `.OpeningKeepOut`, `.NoCavesOnHighGround`, `.CaveMaxLOD`,
> `.Geology`, `.CaveSurfaces`, `.DigYields`, `.GoldenChecksum`.
> **Open items:** `Docs/BACKLOG.md` § World: terrain, caves, geology. **Decisions:** `Docs/DECISIONS.md` (Oct 16 caves v8, Oct 17 geology calls).
> **History:** the dated build log is `Docs/Terrain_Generation_Exploration.md` ("Geology", "Update 2026-10-10" caves, to-do list).

## 1. Shape and placement (generator v9-v12)

| Rule | Value | Since |
|---|---|---|
| Passages are **galleries**: stacked levels every `GalleryLevelSpacingCm` 30 m; each follows the zero set of a 2D field in plan; its height wanders `GalleryLevelAmplitudeCm` 11 m over `GalleryLevelWavelengthCm` 400 m | 250 passages measured within 15 deg (steepest 4 deg) | v9, Oct 16 |
| Tunnel size `TunnelRadiusCm` | 160 cm (3.2 m wide, ~2.8 m high); a mask tapers stretches shut smoothly | v9 (was 320) |
| Openings | only where a passage runs INTO a 16-32 deg slope (12 m-baseline slope). Never a hole in flat ground (Wes) | v9 |
| Sinkholes | chance 0 by default (feature kept for worlds that want them; their tests use such params) | v9 |
| Ceiling | no cave above `CaveMaxGroundCm` 750 m of ground, `CaveHighFadeCm` 100 m taper (far LODs could not hold thin rock under summits) | v7, Oct 13 |
| Water gate | no cave air below sea level + `CaveWaterClearanceCm` 5 m (fade 6 m; `CaveMinZCm` starts there); no cave within `CaveShoreKeepOutCm` 250 m of LOW ground (base height under the 18 m river/basin ceiling); 120-180 m from upland lake shores | v10, Oct 17 |
| Karst only | `bCavesOnlyInLimestone` (in the params hash): non-limestone nodes of the 16 m water-gate lattice close, so a contact tapers over a few metres | v11, Oct 17 |
| Density | ~4 mouths per km2 of hill country (16-32 deg band), ~11 per km2 of karst hillside; none on flat lowland or in the ranges. A coastal spawn can be ~9 km from the nearest mouth (world 31415926) | measured Oct 17 |
| Reach | Ordinary ground: a walk-in mouth in 0 % of 24 random 1 km tiles of cave country (3 worlds); most tiles have no ground at >= 16 deg. `MOFramework.WorldGen.CaveReach` reports it per world and per tile (slope, limestone, passages) and for candidate tunings. With the built entrances (v12) a walk-in mouth in ~25 % of those tiles (5 of the 8 limestone ones) | measured Oct 19 |
| Ravines (ON since v13, F62 A: Wes Nov 4; "better caves with chasms more like minecraft", "50-100m deep, full of crags and ledges") | `RavineChance` 0.35 per `RavineCellCm` 500 m (the Caves knob scales it; 0 = off and out of the params hash): a meandering slot open to the sky, 160-400 m long, 50-100 m deep at mid-length, widest a little above mid-depth, ledges every 5-10 m, crags, kept out of water. Its distance is `min`'d into `SampleCaveDistanceAtGround`, so it is a cave-field feature: the same stamp, LOD gate and dig. It is the one deliberate hole in flat ground, and it ends in a sheer wall (the field jumps at its tips, in the rock beside them), so `.Caves` and `.GalleriesStayLevel` measure the caves with `RavineChance` 0. `DistanceToSinkholeRimZone` includes it (plants, world features, spawns keep out). +48 % cave-field generation in the chunks round one (lab). No rain or snow in them yet (N-174). Tests `WorldGen.Sky.Ravines`, verb `MO.WorldGen.FindRavine` | built Nov 4 |
| Built entrances (ON since v12, F30 A: Wes Oct 19; the mix since v15) | `CaveEntranceCellCm` 400 m / `CaveEntranceChance` 0.9 (v14: 500 m, 0.7; in the params hash; 0 turns them off and out of it): a doline hollow (spherical cap 11-15 m across, 3.5-5 m deep, walkable rim), gated on karst + dry, ground <= 20 deg, inside its cell's jitter margin; what it leads to is the mix below. `DistanceToSinkholeRimZone` includes the hollow (plants, world features, spawns keep out). Tests `MOFramework.WorldGen.CaveEntranceBuilt`, `.CaveEntranceMix`, walked in game by `Tools/check_cave_mix.py` | built Oct 19, mix Nov 6 |

**The mix (BR23-18, generator v15, Nov 6; Wes, Oct 23: "a mix: ~2/4 small overhangs (room for a fire, partial shelter), 1/4 deeper branching caves, 1/4 shallow caves as now").** Each built entrance rolls its kind (`CaveOverhangShare` 0.5, `CaveDeepShare` 0.25, the rest shallow; `EMOCaveEntranceKind`, `MO.WorldGen.Entrances [r]` lists them). The cell is smaller and the chance higher than v14's so the walk-in caves stay about as many as they were, with the overhangs on top.
- **Overhang (native since N-357, generator v16, Nov 6; Wes: "the overhangs should be "native" to their biome ... the native biome vegetation and such should go right up to the overhang, it's suppose to give the existing biome some nice variation not be its own little poi type thing ... grass and shit on top of the overhang, dirt/rocks/vines underneath"):** no passage, no bowl, not a cave. `SettleOverhang` tries up to 32 hashed places in its cell for a hillside of 20-50 deg; flat country has none, so an overhang there simply isn't. A half dome (`SettleShelter`) is cut into that slope, its flat floor meeting the hillside at its front edge so you walk in level, 2.4-3.2 m high, 6-14 m wide, long enough that the hill has climbed over its back half (measured: 80 cm of rock over the dome, or the place is refused). Most are chained (Wes's vote, Nov 7: "these look good but i want them chained together so they're not so uniform... i think you thought i was talking about caves"): 1-4 more domes of different sizes run on along the slope, each overlapping the last by a tenth to a half of their widths and blended into it (SmoothMin 150 cm), so the hillside holds one long, uneven shelter; a chain tries the other way, then shorter and smaller, before it ends, and the domes after the first take a slope from 16 deg. Their outlines carry two waves (up to +-26 %) and two scales of roughness (28 cm at 2.3 m, 55 cm at 7 m). They take the caves' gates off: any rock (`bCavesOnlyInLimestone` is about solution caves), any height, only dry (no water on the floor or in the mouth, no upland lake zone). Only the dome is carved, so only it wears the cave stamp's bare `VST_Cliff`; `DistanceToCaveEntranceHollow` never reads inside one, so the biome's plants come to its lip and over its roof (`SampleOpeningDistanceUnderCm` still drops a point over the open floor). Under the roof (`FMOWorldGen::IsUnderOverhang`) a surface point becomes `ShelterFloor` / `ShelterWall` / `ShelterCeiling`, with their own `DT_CaveScatter` rows: forest floor and root scans, closed and seated like the gravel, for the dirt, closed rocks, hanging roots off the ceiling (`bHangs`: by the mesh's top), ivy on the walls. Nothing lies on the floor within 2.5 m of the mouth, where the ground falls away and a plate would hang over the lip. No gravel, no rim rocks (`FindCaveBowlRocks` skips them). The chain's ceilings blend; its floor is the one of the dome you stand in, and a dome's floor within 80 cm of the first's is the first's, so the shelter's floor runs level (blending whole domes had dipped it ~37 cm where they overlapped). Over any dome's back half the carve keeps 60 cm under the hillside's own surface (`ShelterRoofKeepCm`): the roughness had lifted ceilings through thin roofs, 301 holes in 8,281 columns. Measured (`MOFramework.WorldGen.OverhangsNative`, 6,400 cells): 469 overhangs, 405 of them not on limestone, 2.8 domes each (1 x90, 2 x109, 3 x139, 4 x80, 5 x51), every chain's domes overlapping, no hole through a roof in 7,644 columns; plants allowed on every roof and lip spot tried; the ground opened is 29 % of the domes' plans. In game (`Tools/check_cave_mix.py`): cover 0.89 under the roof against 0.33 on open ground, a campfire places.
- **Shallow:** as v12-v14: a 3.4 m passage down at ~27 deg into one chamber.
- **Deep:** the same passage and chamber, then two or three passages from the chamber (one each side, sometimes one ahead), each forking once or twice more, 2.8-3.8 m wide, at a walkable 3-12 deg down, the forks ending in small rooms: up to 8 runs, up to 180 m of branches, down to about 29 m under the rim (seed 670711536). Its centre keeps to the middle third of the cell so the branches stay in it; a run is kept only with 3 m of rock over its middle and end, dry and on karst there, so a cave near a limestone contact has fewer branches. Laid out once per cell and memoised per thread; the test holds that each cell's cave is the same whatever order the cells are asked in (a reused cache slot once kept the last cell's room, and the same world counted different rooms from run to run).

**The bowls (N-355, generator v16, Nov 6; Wes's vote: "as with lakes, make them non uniform, right now they're like perfect little circles, chain some together, give then non-uniformity, random scatter of like 2-5 of them").** `BuildEntranceBowls` in `MOWorldGenCaves.cpp`: every bowl is a spherical cap whose rim lies at 0.92-1.22 of its radius by heading (two waves round it, polynomials of the heading: no libm in the field), with a lobe or two overlapping it (0.55-0.8 of its radius, off by 0.45-0.75 of it). A chain of 1-5 bowls (20 / 25 / 25 / 20 / 10 %) wanders off the main one, away from the way in, each 0.35-0.75 of the main radius, overlapping the last a little or a few metres past it (0.95-1.4 x the two radii: at more overlap a chain read as one blob in game), and proportionally shallower, so its rim is no steeper and walks the same; each sits on its own ground. A bowl stands only where the way in keeps its rock (a shelter's roof, the passage's first stretch), its outline stays 10 m inside the cell (the field asks one cell), and its ground is dry, karst, gentle and at the right height. An overhang's main outline runs through its mean on the way in, so the roof stands where `SettleEntrance` measured it. `DistanceToCaveEntranceHollow` is the nearest bowl's rim plus a fifth of its radius. The main bowl's size stays F60's. Measured over 736 entrances (`MOFramework.WorldGen.CaveBowls`): 5.0 bowls each counting lobes, 556 with a bowl past the main rim; the rim's farthest over its nearest round the main bowl: mean 2.58, least 1.18 (a circle reads 1.00); 4.2 % of a bowl's surface points steeper than a pawn walks.

**What stands in and round a bowl (N-354, Nov 6; Wes's vote: "keep the gravel from the edges and bias more closed geometry rocks around the edges, so same pcg scheme as i've been saying large distance between pcg hot spots (the cave in this case) with smaller nodes scattered around the poi center in cascades, keeping the larger "gravel pit" look towards the center and larger rocks biased towards the outside").** `FMOCaveScatterRow::BowlMiddleWeight` / `BowlRimWeight` scale a row's density by `FMOWorldGen::CaveBowlShare` (0 at a bowl's middle, 1 at its rim; a smooth step from a quarter to three quarters of the way out; passages under a bowl are untouched): the gravel plates 1.6 to 0, the floor's bigger rubble 0.1 to 1.2. `FMOWorldGen::FindCaveBowlRocks` is the cascade, a pure function of the world: per bowl, 3-9 large rocks round the rim, about 9 m apart (0.88-1.3 of the way out), 2-5 medium ones round each, 1-4 small ones round each of those, and 2-5 thinner clusters further out (up to 3 radii, most near), none in the way in. Rows on `BowlRockLarge` / `Medium` / `Small` are the meshes (all closed, `MOFramework.PCG.CaveTableIsClosed`; 3-6 m, 1-2 m, 25-70 cm); `PerSquareMetre` is the weight among them; each point takes one rock. A rock rests on the lowest ground under it. `mo.PCG.BowlRocks 0` turns them off when a cell generates. Measured over 40 entrances (`MOFramework.WorldGen.CaveBowlRocks`): 11 large, 46 medium, 113 small each; mean share 1.05 / 1.31 / 1.31; none in the way in, none in the air.

**Rocks in a ravine's walls (N-330, Nov 6; Wes, Nov 5: "the rocks need a different sampler so they're not looking for horizontal surface to spawn on but looking to spawn on the sides of the ravine, and we need big rocks too not just little ones, all of which need pushed into the ravine laterally").** The scatter samples ground from above, which a sheer wall does not have, so `MORavineRocks.cpp` asks the generator: `FMOWorldGen::SampleRavineWall` marches out from the ravine's path at a height to its first rock. Each ravine within 1.2 km of a player gets about one rock per 10 m square of wall (at most 220 a wall), 30 % of them big, up to the ravine's half width at the lip and 0.3 of it towards the floor so they cannot close the slot, each set into the wall by half its width. The meshes and components are shared with the islands' rocks (`MOFeatureRocks`, angular scans: round boulders read as eggs stuck on a wall). From the seed alone, the same on every machine; placement runs off the game thread, one ravine at a time; climbable (`MOClimbable`); `mo.WorldGen.RavineRocks`. Before and after: `Saved/SkyFeatures/n330_before`, `n330_after`.

Why the 16-32 deg band: lower bands give more mouths (14-24: 8.5/km2, 12-20: 13, 10-16: 18.5) but a 3 m tube on a gentle slope cuts
an open trench, which Wes ruled out ("no holes in flat ground").

## 2. How the carving works

- A **Subtractive volume stamp** (`MOCaveStamp` = `FVoxelVolumeGraphStamp` of `VVG_MOCaves`, layer `DefaultVolumeLayer`, identity
  transform) carves a signed distance (cm, negative = air) out of the height terrain. Graph: `Get Position 3D (double, World)` +
  the `Seed` parameter -> `MO Terrain Caves` -> `Output Volume.Distance`; `MO Terrain Caves Bounds` -> `.Bounds`; surface default
  `VST_Cliff`.
- **Weather shelter (Oct 19, BR18-16):** the cave stamp also carries a metadata override, `VFM_MOWeatherShelter` = 1, so what it carves
  gets no rain, puddles or snow in the terrain material (open ground keeps the default 0; a mouth fades with the stamp's alpha). A built
  entrance's doline bowl is carved by the same stamp and stays bare too (N-174). `Docs/Weather_And_Time.md` §2a.
- `UMOWorldSeedSubsystem::ApplySeedToStamps` seeds BOTH graph stamps (height and volume). Seeding only the height stamp carves caves
  that do not line up with the ground.
- **The cave stamp's `LODRange` is `{0, FMOWorldGen::CaveMaxLOD}`** (4 at 50 cm voxels). A volume stamp's bounds make the plugin mesh
  every touched chunk through its volume pass, which holes coarse steep chunks: the "far checkerboard" of Oct 14 was this
  (fix 636bbb23). **Any new volume stamp needs tight bounds AND a LOD range** (`Docs/Voxel_Plugin_Reference.md` §6).
- Every gate is a **continuous penalty** that saturates at `CaveFarCm` 3000 cm: a hard gate is a cliff in the field. Tests measure
  `max |gradient|` (limit 40; the depth taper times ground slope reads 25-35 on steep ground, a real jump 100+).
- Far LODs skip features smaller than the sample spacing; a coarse 25 m scan finds almost no mouths (a mouth is a few metres wide):
  measure at 4 m.
- Cost: a 34^3 chunk ~7-11 ms underground with the exact per-column memo (`ComputeCaves` TMap per column; a direct-mapped cache
  collided within a chunk and recomputed ground per Z layer: 39.7 -> 10.7 ms, Oct 13).
- **Determinism:** IEEE-exact operations only (no sin / cos / pow / ISPC). Any change to the cave math bumps
  `CurrentGeneratorVersion` and re-pins `PinnedProbeCave` (`MOWorldGen.h`); `MO.WorldGen.Checksum` must print `match=1` in the
  editor AND a packaged build (`/fp:precise` is forced for MOFrameworkCore, see `Docs/TRAPS.md`). Pins on 2026-10-17 (generator v11):
  `PinnedProbeCave` `0x0FBFC116`, `PinnedProbeHeight` `0x3343B4EB` (v8 ground), base anchor `PinnedProbeHeightNoWater` `0x9C4AE2B3`;
  the header (`MOWorldGen.h`) is the source of truth.

## 3. Bedrock and ore bodies (`MOWorldGenGeology.cpp`)

**Bedrock** (`FMOWorldGen::SampleBedrock`): regional provinces (~7 km fields), sharp contacts. Volcanic provinces are `Basalt`;
continental interiors (continentalness > 0.5) and one province band are `Granite`; the rest is sedimentary `Limestone` or
`CoalMeasures`. Measured 60 x 60 km: limestone 41 %, coal measures 24 %, granite 19 %, basalt 16 % of land; 97.9 % of points 100 m
apart share their rock.

| Ore | Real-world occurrence (sources in the worldgen doc "Geology") | MO model |
|---|---|---|
| Coal | seams in sedimentary basins, 0.3-2 m, gently dipping | a bed per 16 m of strata, 40-220 cm, ~1 deg dip, where a 400 m field opens; `CoalMeasures` only; 5.9 % of that rock |
| Iron (hematite) | fills joints/fissures of limestone | vertical sheets on one joint set (~60 m apart, 30-120 cm), mineralised in patches; `Limestone`; 0.44 % |
| Iron (bog) | precipitates in wetlands | a SURFACE ore in the biomes' dig tables (Rainforest) |
| Copper | veins in basalt / gabbro, granite margins; malachite where weathered | sparse 3D vein sheets; `Basalt` 0.62 %, rarer in `Granite` 0.18 % |
| Tin | cassiterite in granite; river placers | 3D veins in `Granite` 0.38 % (placers: not built) |
| Flint | chert nodules in limestone / chalk beds | a 25 cm bed per 6 m with nodules; `Limestone` 1.4 % |
| Lead (galena) | Mississippi-Valley-type in limestone fractures | a second joint set (~90 m, 20-70 cm), rarer patches; `Limestone` 0.14 % |

The test (`WorldGen.Geology`) asserts every ore sits in its host rock; 98 % of coal points continue 2 m sideways, 0 % are coal 3 m up.

## 4. Digging: yields and time

- **One map:** `FMOWorldGen::SampleDig` (soil within `SoilDepthCm` of the unworked ground, 250 cm on the level to 40 cm on 35 deg
  slopes; else the bedrock or the ore body) -> `FMOGeologyYield::ItemFor` / `ForDig` / `ForDigInWorld`: soil -> `Dirt01` (spoil),
  rock -> `Limestone01` / `Sandstone01` / `Granite01` / `Basalt01`, ore -> `Coal01` / `IronOre01` / `CopperOre01` / `TinOre01` /
  `FlintNodule01` / `GalenaOre01`. Points in air or cave air are skipped.
- **Both dig paths use it:** the player's dig tool (`UMOTerraformingComponent::GrantDigYield`: soil goes to a dropped spoil pile,
  stone + ore to the inventory; incremental steps accumulate volume and grant at completion or on interrupt) and the survivors'
  `ExcavateAndHaul` job (earth and stone fill the fill zone; ore is kept). `GiveOrDrop` never loses items.
- **Dig time by rock** (Wes, Oct 17: "yes, different times for different rocks/minerals"; commit 37a3d694):
  `FMOGeologyYield::HardnessFor` = earth 1, coal 1.5, coal measures 3, limestone 5, granite / basalt 40; veins: galena 3,
  hematite 8, chert 10, copper 15, cassiterite 40. `DigTimeMultiplier` averages it over the dug points and scales the player dig
  AND the survivors' excavation bite. The dig slab follows the ground. Evidence for the ratios: `Docs/Technology_History.md`
  (Grimes Graves chalk ~6x soil with haulage; fire-set hard rock ~40-60x).
- **Any ore with stone tools** (Wes, Oct 17: "they should be able to collect pretty much any ore, the actual hurdle with iron is
  having fire hot enough"): `IronOreDeposit` takes a stone pick (MinToolQuality 0.5) like copper. The heat ladder is the gate
  (`Docs/PROGRESSION_ROADMAP.md`, `Docs/Technology_History.md`).

## 5. What lives in caves and on outcrops

- **MO Cave Spawner** (`UMOPCGCaveSpawnerSettings`, a subclass of the biome spawner: only the planting differs; `SubmitBuckets`
  shared) runs on the GRID16 tier of `MOPCG_StampScatter1`, fed by `WaitForVoxelWorld_0` (the edge gives the grid; its points are
  ignored). It finds the cave surfaces under its 64 m cell itself (`FindCaveSurfaces`: a column per metre, sphere-traced, normal
  from the field's gradient, floor / wall / ceiling by the normal), asks the rock 30 cm behind each point what it is and holds, and
  plants `DT_CaveScatter` rows (`FMOCaveScatterRow`: surface, bedrock, ore + minimum grade, density / m2, mesh, material, scale,
  align, sink, resource node, collision). Cost 5-14 ms per 64 m cell with caves.
- **A surface has rock under it** (BR23-1 / BR24-22, Nov 3): a built entrance's hollow is a cap sphere, and the sphere runs on above the
  ground round its rim. `FindCaveSurfaces` searches each column up to a metre over the ground, so it found the sphere's edge in the open
  air and took it for a floor: the dressing (sand rocks, rocky-ground slabs) hung up to a metre over the ground round every bowl. A
  crossing at or above the column's ground is not a surface now. Measured by `Tools/check_cave_mouths.py` (`MO.PCG.OpeningAudit`) at three
  bowls of seed 5730 (near -339018, -236264): floor-facing dressing floating > 30 cm 40 / 50 / 51 (worst 74-81 cm) -> 0 / 0 / 0 (worst
  22-24 cm); test `WorldGen.OpeningKeepOut` (976 points in the air round 8 entrances without the rule, 0 with it). Lumps on walls and
  ceilings (15-34 per bowl) have the cave's air under them and are not measured.
- **The scatter keeps off carved ground:** the hollow's cap is fitted to its centre's ground, so on a slope it cuts the surface up to 1.31
  radii out against the radial keep-out's 1.2. `FMOWorldGen::SampleOpeningDistanceUnderCm` (the cave field 30 cm under a point, where a
  sinkhole or entrance is in reach; one cell hash each elsewhere) is part of `FMOPCGWaterMask::IsInCave`, which the biome spawner now asks
  too (it had its own copy of the radial test).
- Rows: gravel / eroded-rock / sandstone floor skins, stones, fallen blocks (solid), rock lumps on walls and ceilings, ore lumps
  (coal black, hematite red, malachite green, cassiterite dark, chert on the flint scan). MIs in `/Game/Kitbash/Cave`.
- **Outcrops:** `FMOWorldGen::FindOutcrops` (slope >= 30 deg over 3 m) + `DT_CaveScatter` rows with `Surface=Outcrop`
  (Coal / Malachite / Hematite / Tin / Chert) put surface ore where a prospector would find it.
- Ore rows are real harvest targets (DT_Resources `CoalSeam`, `IronOreDeposit`, `CopperOreDeposit`, `TinOreDeposit`, `FlintDeposit`,
  `GalenaDeposit`), and removals persist: the removal ledger keys a 1 m height cell (`CellZ`; `AnyZ` for old saves) so a quarried
  floor stone no longer removes the ceiling rock above it (`Docs/PCG.md` § removal ledger).
- Galena also feeds the glaze chain: `GalenaOre01` -> `GrindGalenaGlaze` -> `MineralGlaze01` (medieval lead glazes).

## 6. How to extend (rules)

- **New ore** = a `SampleOreIn` rule in its host rock + a `DT_CaveScatter` row + a `DT_Resources` row. Never place ore by biome or
  at random.
- **New dig material** = a `HardnessFor` case (and an `ItemFor` mapping).
- **Any change to cave/geology math** = generator version bump, re-pin, and keep `CavesStayDry`, `GalleriesStayLevel`, `Caves`,
  `CaveEntrances`, `Geology` green. Field tests use karst-OFF params (karst made `GalleriesStayLevel` take > 5 min and starved the
  air samples).
- **Looking at caves:** probe air with `MO.WorldGen.CaveSample` before placing a camera; a camera placed along a cave normal can
  land inside the rock, and from inside the rock you see the ocean plane under all terrain (it looks like water in the cave; it is not).

## 7. Superseded history (kept for search)

- v2 (Oct 10): spaghetti tunnels (two 3D noise zero-crossings, R ~3.2 m), caverns (3D field > threshold, >= 35 m down),
  sinkholes (cell-hash shaft + chamber, the only surface features); caves only where ground > sea + 3 m. **Superseded** by v9
  galleries (Oct 16) and v10/v11 gates.
- "A small tear at 12 km is a Voxel-plugin far-chunk issue" (Oct 11) was **wrong**: it was the cave volume stamp's holes, fixed
  by the stamp LOD range (Oct 14).
