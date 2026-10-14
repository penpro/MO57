# Wes's playtest reports — 2026-10-14 (29 bug reports + crash uploads)

Source: `Saved/Crashes/mo-crashes-2026-10-14.tar.gz` (the site's `crash-dumps/`), read with `python -I Tools/crash_triage.py <dir> --since 2026-10-13`.
Shipping build `ef1c84d9`, world seed -1675528670, ~100 min of play. Screenshots and full text were extracted per report (`NN_<id>.txt/.jpg`).
Status: **DONE** (fixed + verified), **OPEN** (not started), **LOGGED** (design / art direction to decide or a larger piece of work).

| # | Report | Cause / where | Status |
|---|---|---|---|
| C1 | Crash x4: ensure in `UMOUnifiedInventoryMenu::InitializeMenu` (duplicate delegate) | cached menu re-bound its equipment drop handler on every open | **DONE** 238198e7 (swept every re-run initializer: the only one) |
| — | 5 report titles cut at 120 characters | the form clamped the title and dropped the rest | **DONE** 5f22ab84 (whole title moves into the description) |
| 1, 26 | Open-geometry rock scans stick out / stand on end (see-through back) | species placement data (sink / tilt / align) for open-backed scans; `Saved/Logs/species_audit.tsv` has every species with its mesh facts | OPEN |
| 2, 3, 18 | Nordic beach rocks / other rocks have no collision (#18: pawn sank into one) | species collision or meshes without simple collision (audit lists `simple` count per mesh) | OPEN |
| 9 | Medium trees have no collision | same audit | OPEN |
| 11 | Saplings' branches block movement | want a narrow trunk collision (capsule) instead of the branch hull | OPEN |
| 15 | Giant chunk of mud spawned on its edge | an embankment / mud scan scattered as a single species with random rotation -- belongs in a composed POI | OPEN |
| 20 | Dead tree floating | sink for the dead tree species | OPEN |
| 21 | Berry bushes no collision; want to harvest berries; inventory icons missing | collision + a harvest row + missing item icons | OPEN |
| 4 | Muddy-leaves ground should sound like sand | footstep set of that surface / biome | OPEN |
| 5, 6, 14 | Frame rate halves in scrub / dense forest (31-48 ms Shipping 1440p) | perf pass in those biomes (Development 1080p in ElectricForest: 15-16 ms) | OPEN |
| 6 | Dense forest: grass still brightly lit under the canopy, trees VERY dense | grass shading / canopy density | OPEN |
| 7, 12 | Character very shiny (wet after water) | wetness amount on the MetaHuman | OPEN |
| 8, 22 | Sand / terrain very shiny / bright | ground material roughness / specular | OPEN |
| 10, 15 | **Blue/white checkerboard on far mountains** | missing-material fallback at far LOD | OPEN (high) |
| 13 | Blend from standing to swimming and back | animation | LOGGED |
| 14 | Tree size variety 75-125 % | species MinScale / MaxScale | OPEN |
| 16 | Explore mode HUD: show the biome ("Exploring plains") | HUD text from the generator biome under the pawn | OPEN |
| 17 | Lone circles of leaves look bad | leaf scatter should sit around a larger asset (tree) -- composition, i.e. a POI / cluster rule | OPEN |
| 19 | Wind on tiny saplings jittery / too fast | foliage wind parameters | OPEN |
| 23 | Crafting opened from the inventory (right-click > Craft) closes on any click; inspect timer bar hidden under the crafting menu; Strip Bark unlocked but not listed as a recipe | UI hand-off inventory -> crafting in one frame (modal background / deferred deactivation suspected -- reproduce with `ue.py ui` before fixing); z-order of the timer bar; recipe discovery vs list | OPEN |
| 24 | Dry grass assets LOD-switch (not Nanite) | enable Nanite on those meshes (audit `nanite` column) | OPEN |
| 25 | Mountainside very sparse | alpine / highland palettes | OPEN |
| 27, 28 | Cliffs facing uphill (holes in mountains); mountain "detail" features random | generator v6 crags / gullies: cliffs must face downhill; smooth the detail -- generator change (version bump) | LOGGED |
| 29 | Lakes all one size, unadorned | upland lake size distribution + shore dressing (POIs) | LOGGED |
| — | (live, Oct 14) "the biomes should have a lot more blending" | surface biome weights: climate band 0.03 -> 0.12 + 3-octave edge breakup (ground and scatter share it) | **DONE** (before/after `Saved/Screenshots/biome_blend_before_after.png`) |
| — | (live, Oct 14) bug report should say which item the player was looking at | LookAt.* rows captured when the form opens (mesh, instance, collision, material, biome, view + reproduce command) | **DONE** |
| — | (live, Oct 14) ED cliffs: drop 0.5 m, spread the base, 4x larger, fall through when jumping up | floor authored 2.6 m above the root + cliff meshes with no / few-hull collision | **DONE** (MO Conform To Ground node; composition rocks complex-as-simple; grid 398/398, pawn ledge test) |
| — | (live, Oct 14) `~` does not open the console in the Shipping game | Shipping compiled the popup out; now Options > Gameplay > Developer mode (or `-devmode`) + the developer menu | **DONE** (verified in Shipping code) |

Related asks in the same session: the Electric Dreams biome with its big rock features and vines -> world features (POIs) built as whole compositions,
`Docs/World_Features_Architecture.md` Phase 1 (commits b3220c8a, 34085117); `biome` / `poi` dev pickers (8bcba4f9).
