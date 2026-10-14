# Wes's playtest reports — 2026-10-14 (29 bug reports + crash uploads)

Source: `Saved/Crashes/mo-crashes-2026-10-14.tar.gz` (the site's `crash-dumps/`), read with `python -I Tools/crash_triage.py <dir> --since 2026-10-13`.
Shipping build `ef1c84d9`, world seed -1670458245 (Session.WorldSeed of every report), ~100 min of play. Screenshots and full text were extracted per report (`NN_<id>.txt/.jpg`).
Status: **DONE** (fixed + verified), **OPEN** (not started), **LOGGED** (design / art direction to decide or a larger piece of work).

| # | Report | Cause / where | Status |
|---|---|---|---|
| C1 | Crash x4: ensure in `UMOUnifiedInventoryMenu::InitializeMenu` (duplicate delegate) | cached menu re-bound its equipment drop handler on every open | **DONE** 238198e7 (swept every re-run initializer: the only one) |
| — | 5 report titles cut at 120 characters | the form clamped the title and dropped the rest | **DONE** 5f22ab84 (whole title moves into the description) |
| 1, 26 | Open-geometry rock scans stick out / stand on end (see-through back) | aligned plates on steep ground stood on edge: species slope band (`FMOBiomeSpeciesEntry::MinSlopeDeg/MaxSlopeDeg`); plates <= 28 deg, cliff slabs >= 25 deg | **DONE** (palette applied, 0 diffs) |
| 2, 3, 18 | Nordic beach rocks / other rocks have no collision (#18: pawn sank into one) | meshes without usable simple collision + stale cooked physics; `fix_mesh_collision.py` (FIXCOLL_FORCE rebuilds) | **DONE** -- species gallery contact test 140/140 across all 11 biomes (`MO.Gallery <biome>` + `MO.Gallery.Test`) |
| 9 | Medium trees have no collision | stale cooked physics on a correct trunk capsule | **DONE** (gallery: every tree stops the pawn at the trunk, lets her pass off-centre) |
| 11 | Saplings' branches block movement | narrow 6-12 cm stem capsule | **DONE** (gallery) |
| 15 | Giant chunk of mud spawned on its edge | an embankment / mud scan scattered as a single species with random rotation -- belongs in a composed POI | OPEN |
| 20 | Dead tree floating | root flare over the downhill side: dead trees sink 35 cm | **DONE** |
| 21 | Berry bushes no collision; want to harvest berries; inventory icons missing | collision + a harvest row + missing item icons | OPEN |
| 4 | Muddy-leaves ground should sound like sand | the MuddyLeaves biomes (TemperateForest, LocustWoodland) -> SandTrail; footsteps now follow the ground the foot SEES (`PickSurfaceBiomeAtWorld`, the spawner's soft pick) instead of the hard climate line, which ignored the wide ecotones | **DONE** |
| 5, 6, 14 | Frame rate halves in scrub / dense forest (31-48 ms Shipping 1440p) | perf pass in those biomes (Development 1080p in ElectricForest: 15-16 ms) | OPEN |
| 6 | Dense forest: grass still brightly lit under the canopy, trees VERY dense | grass shading / canopy density | OPEN |
| 7, 12 | Character very shiny (wet after water) | wetness amount on the MetaHuman | OPEN |
| 8, 22 | Sand / terrain very shiny / bright | ground material roughness / specular | OPEN |
| 10, 15 | **Blue/white checkerboard on far mountains** | NOT a material: chunk-sized HOLES (sky through them; Nanite triangle view + fog-off proved it). The cave volume stamp spans the whole world, so every far chunk went through the Voxel plugin's volume meshing pass, which holes coarse steep chunks. The stamp now only exists at LODs 0..`FMOWorldGen::CaveMaxLOD` (4): past it the cave field carves nothing (test `WorldGen.CaveMaxLOD`), so no shape changes | **DONE** (same seed / spot / camera before-after; caves still carve near the player) |
| 13 | Blend from standing to swimming and back | animation | LOGGED |
| 14 | Tree size variety 75-125 % | every tree species spans at least 75-125 % of its mid size (88 entries incl. far stand-ins) | **DONE** |
| 16 | Explore mode HUD: show the biome ("Exploring plains") | mode indicator reads the dominant ground biome under the pawn every second ("EXPLORING LOCUST WOODLAND") | **DONE** |
| 17 | Lone circles of leaves look bad | ElectricForest leaf litter now shares the hornbeam stand's clump field (ClusterGroup 6): it lies under trees, not alone on open ground | **DONE** |
| 19 | Wind on tiny saplings jittery / too fast | hornbeam seedlings / saplings shared the 16 m tree's SimpleWind material: child instances at 30 % / 60 % wind (`calm_small_plant_wind.py`; the Megascans folder is gitignored, re-run the script on another machine) | **DONE** (motion: needs Wes's eye) |
| 23 | Crafting opened from the inventory (right-click > Craft) closes on any click; inspect timer bar hidden under the crafting menu; Strip Bark unlocked but not listed as a recipe | the closing inventory forced game input with `PlayerController::SetInputMode`, behind CommonUI's router (cache still "Menu"), and a never-painted crafting menu cannot become the router's active root, so it sat on screen in game capture. Now: gameplay input only through the router (`UMOGameplayInputStub::ApplyGameplayInput`), and every UI surface asserts its own config on activation / when the menu above closes (`AssertInputConfig`) | crafting hand-off **DONE** (log: Menu / NoCapture after the hand-off; UI suite 86/86, RunAll 7/7); timer bar z-order + Strip Bark OPEN |
| 24 | Dry grass assets LOD-switch (not Nanite) | 53 palette meshes still had classic LODs (Fab grasses, dead shrubs, black locust, raspberry, PN flowers, 5 lod3 rocks): Nanite on, foliage with Preserve Area (`enable_palette_nanite.py`) | **DONE** (fixed-camera A/B in DryScrub: GPU 13.8 vs 14.0 ms, draw calls -27..-37 %) |
| 25 | Mountainside very sparse | alpine / highland palettes | OPEN |
| 27, 28 | Cliffs facing uphill (holes in mountains); mountain "detail" features random | generator v6 crags / gullies: cliffs must face downhill; smooth the detail -- generator change (version bump) | LOGGED |
| 29 | Lakes all one size, unadorned | upland lake size distribution + shore dressing (POIs) | LOGGED |
| — | (live, Oct 14) "the biomes should have a lot more blending" | surface biome weights: climate band 0.03 -> 0.12 + 3-octave edge breakup (ground and scatter share it) | **DONE** (before/after `Saved/Screenshots/biome_blend_before_after.png`) |
| — | (live, Oct 14) bug report should say which item the player was looking at | LookAt.* rows captured when the form opens (mesh, instance, collision, material, biome, view + reproduce command) | **DONE** |
| — | (live, Oct 14) ED cliffs: drop 0.5 m, spread the base, 4x larger, fall through when jumping up | floor authored 2.6 m above the root + cliff meshes with no / few-hull collision | **DONE** (MO Conform To Ground node; composition rocks complex-as-simple; grid 398/398, pawn ledge test) |
| — | (live, Oct 14) `~` does not open the console in the Shipping game | Shipping compiled the popup out; now Options > Gameplay > Developer mode (or `-devmode`) + the developer menu | **DONE** (verified in Shipping code) |

Related asks in the same session: the Electric Dreams biome with its big rock features and vines -> world features (POIs) built as whole compositions,
`Docs/World_Features_Architecture.md` Phase 1 (commits b3220c8a, 34085117); `biome` / `poi` dev pickers (8bcba4f9).
