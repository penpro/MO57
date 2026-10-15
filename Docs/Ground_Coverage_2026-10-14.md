# Ground coverage pass — Oct 14, 2026

Wes: *"I want more ground coverage in general too, more trees, more bushes, etc."*

## What was actually limiting coverage

The biome spawner (`MOPCGBiomeSpawnerSettings`) turns each input point of a tier into at most one plant, with acceptance
`p = density × clump_compensation / InputPointsPerHectare`. Four things decided what a biome really got.

1. **Trees and bushes: the authored densities were low, not the caps.** The tier caps (samplers in `MOPCG_StampScatter1`) are
   1200/ha for large species (bushes, rocks, shrubs, saplings) and 800/ha for trees. The palettes used only a small share of
   them: TemperateForest used 27 % of the tree cap and 18 % of the large cap.
2. **Clumping.** Forest trees sit in clump fields with `ClusterCore` 0.55–0.75: they grow only where the clump noise exceeds
   the core, and are compensated to keep the average. TemperateForest trees grew on ~30 % of the ground. The rest was open glade,
   so the "forest" read as grassland with tree islands (see the before shots).
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

## The change

1. **Ground tier 2000 -> 6000 candidates/ha** (`MO Biome Spawner_0` + `VoxelSampler_0`, 90 cm lattice;
   `Content/Python/pcg_tier_caps.py`). `VoxelSampler_0` also feeds the stick / stone item clutter, so its thinning node went
   0.1 -> 0.0333 (`thin_item_clutter.py`): same ~100 item points per hectare. The test `MOFramework.PCG.ItemThinning`
   now checks items per HECTARE instead of a share of the sampler.
2. **Far trees 112.5 -> 300/ha** (`MO Biome Spawner_5`, 404 cm lattice) and **far canopy 18 -> 40/ha**
   (`MO Biome Spawner_6`, 1106 cm): the far forests had been capped at ~40 % of the near ones.
3. **Palettes** (`Docs/Ground_Coverage_2026-10-14.plan.json`, authored with `python Tools/pcg_stocking.py apply <plan>`):
   a factor per tier, a `<tier>:<HISMTag>` factor where a flat factor would grow the wrong thing (the rocky biomes' ground
   tier is 85 % pebble packs: their GRASS goes x12-30, pebbles x1.5), and `<tier>_core_max` to widen the tree clumps.
   **The factors are relative to the table as it was on Oct 14 evening: applying the plan again multiplies again.**

Realised density per hectare from the model, before -> after:

| Biome | ground | large (bushes / shrubs / rocks) | trees | far trees |
|---|---|---|---|---|
| TemperateForest | 546 -> 1202 (+ native lawn) | 220 -> 443 | 215 -> 405 (clumps on ~70 % of the ground, was ~30 %) | 40 -> 184 |
| Meadow | 1447 -> 1809 (+ native lawn) | 18 -> 74 | 2 -> 6 | |
| RockyHighland | 458 -> 2230 (grass 51 -> 1518) | 125 -> 250 | 4 -> 9 | |
| ShoreStrand | 456 -> 729 (+ native lawn) | 134 -> 214 | | |
| DryScrub | 1100 -> 3052 (dry grass 935 -> 2805) | 392 -> 623 | 30 -> 46 | |
| LocustWoodland | 371 -> 890 (+ native lawn) | 130 -> 313 | 164 -> 287 | |
| AlpineHeath | 945 -> 1417 (+ native lawn) | 289 -> 434 | 6 -> 11 | |
| Rainforest | 1886 -> 1949 (floor = ferns + litter, unchanged) | 614 -> 798 | 223 -> 316 | 43 -> 133 |
| AlpineRock | 665 -> 865 | 91 -> 119 | | |
| ElectricForest | 301 -> 2920 (grass 116 -> 2310) | 839 -> 965 | 146 -> 268 | 45 -> 150 |
| NordicStrand | 543 -> 2527 (grass 154 -> 1848) | 107 -> 193 | | |

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
