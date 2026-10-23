# Bug reports, Oct 23 2026 (Wes, standalone Development build of 46fb39a8, seed 3014)

> **What this is:** the live tracker for the 19 reports Wes sent on Oct 23 (02:43-03:27 UTC, `mo-crashes-2026-10-23.tar.gz`; one session,
> standalone, world seed 3014, save `World_20261022_193552`). One row per report: what it asks, the layer responsible, the plan and the status.
> Backlog rows: BR23-n in `Docs/BACKLOG.md`. When every row is closed or moved to the backlog, archive this file (`Docs/Archive/`).
> **Crashes in the same tarball:** 19 uploads, all Oct 19-21 and all from the harness's own `-game` instances during the N-185 work (13 with no
> stack, 3 `ShaderTableHash` ensures, 1 Voxel ComputeShader ensure, 1 `ensure(false)`, 1 Voxel metadata `Guid.IsValid` in the editor): accounted
> for by N-185 (the material keep-alive, the Voxel selection patch); none from Wes's session.
> **Read with:** `python -I Tools/crash_triage.py <dir>`; full texts and screenshots: `ue_crash_bundle.parse_bundle` (`BugReport.txt`, `Screenshot.jpg`).

## Reports

| Row | Report | Title (Wes) | Ask / diagnosis (layer) | Plan | Status |
|---|---|---|---|---|---|
| BR23-1 | b9bf5174 | floating rocks etc around cave entrance | (a) rocks float: the PCG scatter runs before the cave stamp edits the terrain under it (voxel stamp vs PCG order); (b) the mouth's surrounding bowl too big; (c) biome vegetation at the entrance, not only rocks; (d) caves meander, split, small chambers | cave generator + PCG order (`Docs/Caves_And_Geology.md`) | OPEN: batch 3 |
| BR23-2 | f0073cd4 | footsteps are still really loud, need to drop them by about half | the gait loudness (`FMOFootstepMath::VolumeFor`) | x0.5 | DONE: x0.5 |
| BR23-3 | 54ae9d0e | the electric dreams rock spire | bare terrain all around it; 3-10 spire assemblies instead of one | world features (`Docs/World_Features_Architecture.md`) | OPEN: batch 2 |
| BR23-4 | 4d0ab8cf | electric dreams embankment | bare around it; batches of 3-10 instead of one | world features | OPEN: batch 2 |
| BR23-5 | 42f7b3dd | electric dreams small assembly | no standalone small-assembly POIs: scatter them around the cliffs / embankments | world features | OPEN: batch 2 |
| BR23-6 | 1ea3d80b | rock spire with vines | deeper (1 m small, 5 m huge); two groupings (large central, small scattered round them), closer together, ground not bare; interaction on the flowers / berry bushes / vines | world features + the PCG interaction mapping | OPEN: batch 2 |
| BR23-7 | d035208d | all black alder trees floating | the live rows already bury alder trees 25 cm (Oct 16, `SinkMinCm`, applied by `FMOSpeciesPlacement::BurialOffsetCm`); they are not tilted to the slope, so the downhill side of the root flare shows | 50 cm (Wes: "drop them about 25cm" more) on the live DT_Biomes rows + `biome_palettes.py` | DONE: 50 cm (42 entries) |
| BR23-8 | f754e929 | rainforest footsteps sound stupid | Rainforest's set is MuddyStep | another existing set (MossPad: a damp soft floor) | DONE: MossPad |
| BR23-9 | 0224c839 | small sticks not interactable | the stick he aimed at is a scattered Bent Branch (paErt) from its Megascans ORIGINAL; Branch01 / Stick01 use copies under /MOFramework/Items, and the item lookup goes by mesh path (PCG data) | the species point at the item meshes (paErt, uehjfcofa; 3 biomes) | DONE: the species are the item meshes; checked at Wes's spot |
| BR23-10 | 89300267 | a few of these little flowers are blurry | ghost / fade / shimmer: masked foliage under TSR (velocity of the wind's WPO, or LOD dither) | find the mesh, test | OPEN: batch 2 |
| BR23-11 | 7b08a28c | the grasses look funny and too bright | no shadows (Wes may have turned them off) and bright under canopy: the grass material's lighting | look at the settings he ran with | OPEN: batch 2 |
| BR23-12 | 420fc8b8 | cliff faces need work | tuck into the land; cluster to overlap (their own PCG graph around a centre, called from the main one); scale x2 | PCG | OPEN: batch 2 |
| BR23-13 | 43d0479d | add lakes as pois | lakes as POIs; still perfect circles -- overlap voxel stamps | lakes (`Docs/Water.md`) + world features | OPEN: batch 3 |
| BR23-14 | de5ef930 | make dry scrub biome much smaller | about 1/4 of its area | the biome climate ranges (DT_Biomes DryScrub: moisture 0-0.4, temperature 0.35-1), measured by a census | OPEN: batch 2 |
| BR23-15 | db5aeede | harvesting with full invetory | yields that do not fit drop to the ground | the harvest outcome (server) | DONE: `_client_harvest_full_pack` |
| BR23-16 | 926ca09e | lake changes | (a) water sounds -80 %: the Water ambience (DT_AmbientLayers: river flow, waterfalls, brook -- moving water at a still lake); (b) wavelength -80 %, amplitude down ("almost still"); (c) water noise -90 %; (d) vegetation round lakes even in dry scrub | (a) BaseLayerVolume / EventVolume x0.2; (b) `mo.Lakes.WaveScale` 0.4 -> 0.1, a wavelength scale 0.2 (speed by deep-water dispersion); (c) the procedural turbulence x0.1; (d) PCG, batch 2 | a-c DONE; d OPEN: batch 2 |
| BR23-17 | 9970cb4b | breath meter is hard to read | HUD (the report screenshot hides the HUD) | look at the meter, make it readable | DONE: 56 x 18, strong label |
| BR23-18 | c4aa1cdb | add to cave spawn | a mix: ~2/4 small overhangs (room for a fire, partial shelter), 1/4 deeper branching caves, 1/4 shallow caves as now | cave generator | OPEN: batch 3 |
| BR23-19 | 1680cdbf | adjust the height splitting blend | the height-band transitions read as straight lines at distance: blend them over 25-50 m of elevation (not at the water line / beaches) | the terrain material's height bands (`Docs/Terrain_Generation_Exploration.md`) | OPEN: batch 3 |

## Batches

1. **Concrete fixes** (BR23-2, 7, 8, 9, 15, 16 a-c, 17): one build, automation, the affected co-op cases; look-and-feel results go on Wes's check list.
2. **PCG / world-feature tuning** (BR23-3 to 6, 10 to 12, 14, 16 d): counts, depths, clustering, coverage near POIs, biome size.
3. **Generator work** (BR23-1, 13, 18, 19): cave shapes and mix, lake shapes, the height-band blend -- design first where it is a fork.
