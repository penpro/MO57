# Open-faced rock scans taken out of the world (BR25-2, Oct 24 2026)

Wes: "remove all open faced geometry assets, log them for rebuilding in trellis to be closed geometry, give screen shots of all the open geometry assets
so we can hae them cleaned up by chatgpt and then rebuilt with closed geometry in trellis"; then "we can handle the larger taller ones that have only
the bottom open, the shallow depth ones are hard to get to sit properly in the terrain so avoid those ones though".

Measured with `Content/Python/open_geometry_probe.py` (openness = |sum of area-weighted normals| / area: closed ~0, a one-sided sheet 0.3-1.0) and
removed with `Tools/remove_open_geometry.py`. Kept in the world: closed rocks, and tall scans open only underneath (the ground hides the base:
MassiveNordicCoastalCliff_01, NordicBoulder_01, NordicBeachRocks_01, MossyForestBoulder_04, TundraMossyBoulder_01). `removed_species.json` holds
every removed palette entry whole, to put a closed rebuild back as the same species with its mesh swapped.

The renders are 1024 px, untextured (the icon exporter found no textures on these Megascans slots): they show the SHAPE. For colour, the meshes are
in the project at the paths below.

## Rebuild candidates (open at the back: a closed TRELLIS rebuild keeps their look and sits like any rock)

| Render | Mesh | Size | Openness | Was in |
|---|---|---|---|---|
| `SM_HugeSandstoneCliff_04.png` | `/Game/Megascans/3D_Assets/HugeSandstoneCliff/SM_HugeSandstoneCliff_04` | 1159 x 341 x 1485 cm | 0.57 | AlpineRock, RockyHighland |
| `SM_HugeSandstoneCliff_07.png` | `/Game/Megascans/3D_Assets/HugeSandstoneCliff/SM_HugeSandstoneCliff_07` | 2215 x 841 x 670 cm | 0.57 | RockyHighland |

## Avoid (shallow: plates, shelves and root sheets that do not sit in uneven ground, open or closed)

| Render | Mesh | Size | Openness | Was in |
|---|---|---|---|---|
| `SM_ErodedRockyGround_01.png` | `/Game/Megascans/3D_Assets/ErodedRockyGround/SM_ErodedRockyGround_01` | 725 x 369 x 32 cm | 0.85 | AlpineHeath, AlpineRock, NordicStrand, RockyHighland |
| `SM_ErodedRockyGround_03.png` | `/Game/Megascans/3D_Assets/ErodedRockyGround/SM_ErodedRockyGround_03` | 727 x 368 x 33 cm | 0.85 | AlpineHeath, AlpineRock, RockyHighland |
| `SM_ForestRockFormation_05.png` | `/Game/Megascans/3D_Assets/ForestRockFormation/SM_ForestRockFormation_05` | 564 x 420 x 63 cm | 0.83 | ElectricForest |
| `SM_ForestRockFormation_06.png` | `/Game/Megascans/3D_Assets/ForestRockFormation/SM_ForestRockFormation_06` | 579 x 425 x 69 cm | 0.78 | ElectricForest |
| `SM_ForestRoots_01.png` | `/Game/Megascans/3D_Assets/ForestRoots/SM_ForestRoots_01` | 236 x 158 x 22 cm | 0.69 | ElectricForest |
| `SM_ForestRoots_02.png` | `/Game/Megascans/3D_Assets/ForestRoots/SM_ForestRoots_02` | 152 x 154 x 17 cm | 0.73 | ElectricForest |
| `SM_MossyRocks_01.png` | `/Game/Megascans/3D_Assets/MossyRocks/SM_MossyRocks_01` | 185 x 231 x 35 cm | 0.56 | ElectricForest, NordicStrand |
| `SM_MossyRocks_02.png` | `/Game/Megascans/3D_Assets/MossyRocks/SM_MossyRocks_02` | 340 x 186 x 35 cm | 0.48 | AlpineHeath, AlpineRock, RockyHighland |
| `SM_NordicBeachRockyGround_01.png` | `/Game/Megascans/3D_Assets/NordicBeachRockyGround/SM_NordicBeachRockyGround_01` | 336 x 325 x 77 cm | 0.64 | NordicStrand |
| `SM_NordicBeachRockyGround_02.png` | `/Game/Megascans/3D_Assets/NordicBeachRockyGround/SM_NordicBeachRockyGround_02` | 351 x 343 x 25 cm | 0.52 | NordicStrand |
| `SM_RockyForestPath_01.png` | `/Game/Megascans/3D_Assets/RockyForestPath/SM_RockyForestPath_01` | 494 x 350 x 42 cm | 0.92 | ElectricForest |
| `SM_SandstoneRockyGround_01.png` | `/Game/Megascans/3D_Assets/SandstoneRockyGround/SM_SandstoneRockyGround_01` | 264 x 221 x 17 cm | 0.82 | AlpineHeath, AlpineRock, RockyHighland |
| `SM_SmoothRockShelf_01.png` | `/Game/Megascans/3D_Assets/SmoothRockShelf/SM_SmoothRockShelf_01` | 308 x 273 x 42 cm | 0.75 | AlpineHeath, AlpineRock, ElectricForest, NordicStrand, RockyHighland |
| `SM_TundraRockyGround_01.png` | `/Game/Megascans/3D_Assets/TundraRockyGround/SM_TundraRockyGround_01` | 604 x 473 x 84 cm | 0.78 | AlpineHeath |
