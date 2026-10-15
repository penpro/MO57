# Ground coverage pass — Oct 14, 2026

Wes: *"I want more ground coverage in general too, more trees, more bushes, etc."*

## What was actually limiting coverage

The biome spawner (`MOPCGBiomeSpawnerSettings`) turns each input point of a tier into at most one plant, with acceptance
`p = density × clump_compensation / InputPointsPerHectare`. Four things decided what a biome really got.

1. **Trees and bushes: the authored densities were low, not the caps.** The tier caps (samplers in `MOPCG_StampScatter1`) are
   1200/ha for large species (bushes, rocks, shrubs, saplings) and 800/ha for trees. The palettes used only a small share of
   them: TemperateForest used 27 % of the tree cap and 18 % of the large cap.
2. **Clumping.** Forest trees sit in clump fields with `ClusterCore` 0.55–0.85: they grow only where the clump noise exceeds
   the core, and were "compensated to keep the average" -- wrongly (see the clump bug below): TemperateForest canopy trees grew on
   ~7 % of the ground at a quarter of their authored rate, so the "forest" read as grassland with tree islands (the before shots).
3. **The far tree tier was saturated.** `MO Biome Spawner_5` (FarLargeOnly, the 1.5–2.5 km stand-ins) had a 112.5/ha cap, but
   the forests authored ~100/ha in clumps, so it realised only 40 %. Distant forests were less than half as dense as the near ones.
4. **Ground cover in the biomes without the native lawn: the cap.** The ground tier took 2000 candidates/ha (0.2 per m^2); see
   the census below.

`Tools/pcg_stocking.py` replays the spawner's rule (Monte Carlo over the clump fields), so a plan can be checked for clipping
before it is authored. Its `apply` mode authors the same plan into the live DT_Biomes.

## What the census showed (the bare biomes)

`MO.PCG.Census 4000` (everything planted within 40 m = 0.5 ha) at `FindBiomeSpot` spots, perf seed, 75 s after arriving:

| Spot | instances / ha (all tiers) | of which native lawn grass |
|---|---|---|
| TemperateForest | ~11 000 | ~10 000 |
| LocustWoodland | ~11 300 | ~10 200 |
| ElectricForest | ~880 | none |
| RockyHighland (steep) | ~1 280 | none |

Five biomes are off the scatter graph's native grass chain on purpose (`native_grass_biomes.py`: DryScrub, RockyHighland,
AlpineRock, ElectricForest, NordicStrand, plus the Rainforest), because that chain is one green lawn grass. Their floor is
whatever the biome spawner's ground tier plants, and that tier took at most **2000 candidates/ha = 0.2 per m^2**, an order
of magnitude below a covered floor (~1.4/m^2). No palette density could fix it: the tier limit was the floor.

## The clump bug (found while measuring the first after-run)

The first after-tour planted ~5x the trees per cell where the plan asked for ~2x. The cause was in the spawner, under the plan:
a grouped species grows where its clump field `PerlinNoise2D * 0.5 + 0.5` is at least its `ClusterCore`, and its acceptance was
multiplied by `1 / (1 - core)` to keep the authored rate -- right only if the field were uniform on 0..1. It is not: the noise
has a std of ~0.26 of its range, so the field bunches around 0.5 (measured from the engine's own `FMath::PerlinNoise2D`):

| core | ground covered (uniform assumption) | old realised / authored | fixed |
|---|---|---|---|
| 0.30 | 93 % (70 %) | 133 % | 100 % |
| 0.50 | 50 % (50 %) | 100 % | 100 % |
| 0.60 | 23 % (40 %) | 58 % | 100 % |
| 0.70 | 7 % (30 %) | 23 % | 100 % (cap 8x: holds to core ~0.68) |
| 0.85 | 0.3 % (15 %) | ~1 % | ~2 % (capped) |

So the forests' canopy trees (cores 0.7-0.85) had been planted at a quarter of their authored density or less (TemperateForest:
64 trees/ha, not 217 -- the "savanna" in the before shots), and the uniform-noise MODEL in `pcg_stocking.py` repeated the same
mistake, which is why lowering the cores to 0.3 overshot. The ungrouped (legacy) clumping, `clamp(2 * noise + 0.5, 0, 2)`, also
averaged 0.55 (flowers, rocks, driftwood at half their rate).

Fixed where the wrong number is made: `UMOPCGBiomeSpawnerSettings::ClumpCoverage` measures the field's coverage once from the
engine noise, `ClumpCompensation(core) = 1 / coverage` (capped at 8x local concentration), the legacy multiplier is divided by its
measured mean, and `MOFramework.PCG.ClumpRateHolds` pins "authored rate holds" for cores 0.3-0.6. The data contract on
`FMOBiomeSpeciesEntry::ClusterCore` ("density is auto-compensated so the authored per-hectare rate is preserved") is now true.
`pcg_stocking.py` samples the same noise (ported, permutation read from the engine source) and `--old` replays the pre-fix spawner.

With rates now honoured, the plan's tree factors were re-set to deliberate targets instead of "x2 of an imaginary number":
TemperateForest ~210 trees/ha in stands with glades (core 0.5), Locust and Electric ~160, Rainforest ~110 (Wes's #6 said the
rainforest trees were "VERY dense" at ~72/ha realised, so it gets 1.5x, not 4x). Meadows get real copses (~5 trees/ha, core 0.65).

## The change

1. **Ground tier 2000 -> 6000 candidates/ha** (`MO Biome Spawner_0` + `VoxelSampler_0`, 90 cm lattice;
   `Content/Python/pcg_tier_caps.py`). `VoxelSampler_0` also feeds the stick / stone item clutter, so its thinning node went
   0.1 -> 0.0333 (`thin_item_clutter.py`): same ~100 item points per hectare. The test `MOFramework.PCG.ItemThinning`
   now checks items per HECTARE instead of a share of the sampler.
2. **Far trees 112.5 -> 300/ha** (`MO Biome Spawner_5`, 404 cm lattice) and **far canopy 18 -> 40/ha**
   (`MO Biome Spawner_6`, 1106 cm): the far forests had been capped at ~40 % of the near ones.
3. **Palettes** (`Docs/Ground_Coverage_2026-10-14.plan.json`, applied by `biome_palettes.py` on top of its palettes -- regenerate
   with `python Tools/ue.py py --file Content/Python/biome_palettes.py`, then `ue.py rows set .../DT_Biomes --file
   Saved/Logs/biome_palettes.json`; the live table was diffed against it: 0 mismatches): a factor per tier, a `<tier>:<HISMTag>`
   factor where a flat factor would grow the wrong thing (the rocky biomes' ground tier is 85 % pebble packs: their GRASS goes
   x12-30, pebbles x1.5), and `<tier>_core_max` to shape the clumps (stands with glades at 0.5; DryScrub bushes 0.45 so their
   clumps do not saturate the large tier). `pcg_stocking.py apply` multiplies the LIVE table -- use it for experiments only.
4. **The spawner's clump compensation** (above).

Realised density per hectare, before (what the pre-fix spawner REALLY planted, old caps) -> after (fixed spawner, plan), from the
corrected model:

| Biome | ground | large (bushes / shrubs / rocks) | trees | far grass | far trees | far canopy |
|---|---|---|---|---|---|---|
| TemperateForest | 300 -> 1201 | 252 -> 445 | 64 -> 213 | 30 -> 120 | 21 -> 92 | 15 -> 28 |
| Meadow | 1112 -> 1808 | 12 -> 78 | 0 -> 5 | 127 -> 174 | 0 -> 3 |  |
| RockyHighland | 252 -> 2233 | 70 -> 250 | 2 -> 9 | 4 -> 122 | 1 -> 5 |  |
| ShoreStrand | 248 -> 721 | 74 -> 215 |  | 23 -> 68 |  |  |
| DryScrub | 827 -> 3048 | 242 -> 618 | 30 -> 45 | 118 -> 221 |  |  |
| LocustWoodland | 202 -> 883 | 152 -> 312 | 70 -> 160 | 20 -> 87 |  |  |
| AlpineHeath | 593 -> 1413 | 159 -> 433 | 3 -> 11 | 38 -> 73 | 2 -> 6 |  |
| Rainforest | 1083 -> 1961 | 766 -> 826 | 72 -> 108 | 23 -> 43 | 21 -> 46 | 15 -> 15 |
| AlpineRock | 366 -> 865 | 50 -> 119 |  |  |  |  |
| ElectricForest | 172 -> 2865 | 698 -> 936 | 47 -> 163 | 10 -> 147 | 19 -> 65 | 15 -> 27 |
| NordicStrand | 299 -> 2522 | 59 -> 193 |  | 14 -> 123 |  |  |

Far grass follows the near grass in the biomes that got much more of it (x5-15, still under its 288/ha tier).

Lake and river shores (bug #29, "lakes unadorned"): `Tools/shore_dressing.py` adds boulders, a fallen log and lakeside alder
and hazel saplings to each biome's water-edge list (up to 1.2 m above the waterline, about 5 m inland). Each copies its tested
palette settings. The strip is thin, so its per-hectare numbers are x10 (`STRIP_SCALE`): ~5 rocks and ~5 alders per 100 m of shore.

## How it was measured

Same protocol before and after: packaged Development build, 1920×1080, seed 1029024224, clear sky,
`MO.Perf.Tour 60 20 1 0 1 <11 biomes>` (60 s settle, 20 s sample, fixed yaw). Logs:
`Saved/Logs/pkg_probe/cover_base.log` / `cover_after.log`. Eye-level shots per biome: `Tools/biome_eye.py` in a game
instance (clock frozen at 11:00, clear sky), `Saved/CoverShots/before|after/<Biome>_sheet.png`.

Baseline (before):

| Biome | fps | avg ms | p95 ms | GPU ms | draws |
|---|---|---|---|---|---|
| TemperateForest | 90.0 | 11.1 | 12.1 | 9.9 | 1130 |
| Meadow | 94.9 | 10.5 | 11.4 | 9.3 | 1207 |
| RockyHighland | 92.8 | 10.8 | 11.7 | 9.6 | 1208 |
| ShoreStrand | 90.6 | 11.0 | 12.0 | 9.9 | 1265 |
| DryScrub | 85.8 | 11.7 | 12.5 | 10.4 | 1325 |
| LocustWoodland | 89.2 | 11.2 | 12.1 | 10.0 | 1229 |
| AlpineHeath | 93.7 | 10.7 | 11.6 | 9.4 | 1180 |
| Rainforest | 83.5 | 12.0 | 12.8 | 10.8 | 1274 |
| AlpineRock | 101.1 | 9.9 | 11.0 | 8.4 | 1024 |
| ElectricForest | 88.8 | 11.3 | 12.2 | 10.0 | 1072 |
| NordicStrand | 94.9 | 10.5 | 11.7 | 9.2 | 1140 |

New game READY: 14.0 s.

After: (filled in below)
