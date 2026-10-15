# AAA polish backlog (living document)

Goal (Wes, 2026-10-11): **a complete, polished, AAA-level game.** This is the working backlog: each item has a measurable exit condition, and an item is DONE only when the condition is verified by using the
artifact (a packaged run, a tour screenshot set, an automation test), not by "it compiles". Newest evidence at the top of each section. Nothing here is committed to a design without the owner's call: true
design forks are logged under "Forks", not built blind.

## Quality gates (what "polished" means here)

| Gate | Target | Measured now | Tool |
|---|---|---|---|
| Frame time, packaged Dev build, 1080p, RTX 4090 | p95 <= 16.6 ms (60 fps) in every biome, p99 <= 25 ms | avg 71-89 fps; p95 12.8-15.3 ms; **Rainforest 51 fps (p95 23 ms)**; p99 up to 36 ms; hitches up to 333 ms (see Perf) | `MO.Perf.Tour` via `Tools/ov.bat` (`PERF_LABEL`, `SKIP_PACKAGE`, `PERF_PROFILE_GPU`) |
| Log health | no per-frame spam; no Error lines at boot | `RecursiveApplyAdjuster` spam fixed in code (31k lines / 6 min before) - verify in the next packaged run | `grep -c` in the packaged `MO57.log` |
| Tests | 100 % of `MOFramework` automation | 189 / 189 | `python Tools/ue.py auto --filter MOFramework` |
| Water | nothing planted in water; swim / dive / drown work | audit 0 in 8 biomes; swim verified in the editor; drowning tested headless | `MO.PCG.WaterAudit`, `MO.Water.*`, `MOFramework.Water.*` |
| Biomes | every biome visually distinct, no untextured ground, no floating / flooded plants | tour seed 7 clean (24 screenshots in `Saved/BiomeTour_2026-10-11/`) | `scratchpad tour.sh`, `MO.WorldGen.BiomeStats` ("(none)" must be 0 %) |
| Build hygiene | no duplicate file-local names (adaptive unity); packaged build cooks + co-op test passes | clean | `python Tools/check_unity_collisions.py`, `Tools/ov.bat` (nettest packaged) |

## Done (this effort)

- Biome-aware PCG: three tiers (ground cover / large species / shore) sharing one node type; the sampler-lattice rule (a Voxel Sampler places at most one point per `CellSize` cell); water mask chokepoint;
  `AlpineRock` summit biome + coverage test; alpine heath ground; shore flora mechanism.
- Oceanology water: ocean + upland lakes, underwater look fixed (1/4 distortion), lake bed material, lakes cooked.
- Swimming + drowning: movement component, water oracle, breath-hold physiology, retargeted swim animations, steering, stamina/MET hooks; `MOFramework.Water.*` tests.
- Swim feedback: splash / wading / stroke sounds (the bank IDs resolve; `[MOWater] splash` log lines), the procedural HUD breath meter (`MOBreathMeterWidget`: full-screen overlay, bar aligned bottom-centre,
  fades in underwater, blue -> amber -> pulsing red; verified in the editor swim test), cold-water heat exchange through the vitals pipeline (`MOFramework.Water.ColdWater`).
- Footsteps by surface (`UMOFootstepComponent`): steps from the animation (a foot whose world speed collapses from swing to stance has planted; walk 2.65 steps/s alternating, sprint 3.5/s), surface from the
  thing stood on (`IMOFootstepSurfaceInterface`), else the world generator (structure / underground / the BIOME: `FMOBiomeDefinitionRow::FootstepSet`, all 9 biomes set), landing step, hand-off to the water sounds
  at 8 % immersion. Verified in the editor in every biome. `MO.Footstep.*` verbs, `MOFramework.Footsteps.*` tests.
- Graphics quality: `UMOGameSettings` levels Low..Epic over the engine's scalability groups, first-run hardware benchmark (`EnsureGraphicsQualityChosen`, opt-out `-NoQualityBenchmark`), Options panel quality combo +
  VSync + the display's real resolution list, `Config/DefaultScalability.ini` with the measured MO-specific cuts. Options bugs fixed (motion-blur checkbox and weather label never bound).
- Crouch / sneak (LeftCtrl): it was a silent no-op (`bCanCrouch` never set). Now the capsule shrinks to 60 cm half height, walk 200 cm/s, the jump key stands up first, nobody crouches in water; the pose is a looping
  dynamic montage on the animation blueprint's DefaultSlot (idle <-> walk cross-fade, play rate follows speed), clips = the engine's Manny crouch set (MoverExamples, mounted with `UMOEditorContentUtils`, no plugin
  enabled) retargeted to the MetaHuman skeleton in `Content/Characters/Crouch/`; the footstep Sneak gait (quiet) now engages. Verified in the editor (screenshots, probes, step alternation 18/18).
- UI contract sweep (`ui check` over all 88 Widget Blueprints): found and fixed (a) the confirmation dialog (exit / main menu / overwrite / load) had bare native buttons that real mouse clicks pass through
  (`confirm_buttons.py`), (b) the HUD status-effect strip did not exist at all: the code pushes moodles (thermal, wetness, hunger ...) but `WBP_StatusEffectStrip` / `WBP_StatusMoodle` were never authored, so no status
  icon was ever shown (`hud_moodles.py`; verified live: "Freezing / Soaked / Dehydrated / Hungry" appear on their own in a cold lake), (c) the Options motion-blur / weather-label bindings.
- Tooling: `MO.Perf.Bench/Tour`, `MO.NewGame`, `MO.Water.*` (incl. `MO.Water.Meter`), `MO.Audio.Mixes`, `Tools/check_unity_collisions.py`; editor PIE no longer throttled to 3 fps (user EditorSettings.ini).

## Backlog (ordered; the first unchecked item is next)

### P0 - measure and fix what players feel first
- [~] **Frame time**: the scalability set exists and is validated on the packaged build (`Config/DefaultScalability.ini`, `Tools/perf_experiments_round3.txt` / `round4.txt`; avg fps Rainforest / Temperate / Meadow,
  4090, 1080p): Epic 60 / 64 / 68 (p95 19.6 / 17.5 / 17.0 ms), High 73 / 81 / 85 (p95 16.2 / 14.5 / 13.8), Medium 80 / 90 / 93, Low 121 / 129 / 122 (the GPU floor is ~5-7 ms). The auto-benchmark picks the level on first
  run. Open: Epic misses 60 fps p95 in the Rainforest (the 16.6 ms target holds from High down); Nanite pixel error (+3 fps) and hair deep-shadow size cannot live in a scalability file (not ECVF_Scalability: a C++
  quality table would be needed); hair strands cost ~6 fps in the Rainforest (cards LODs would be the big lever); find what the ~6 ms at Low is (`PERF_PROFILE_GPU=1` at Low).
- [x] **Hitches**: a once-a-second 30 ms game-thread stall (`UMOUplandLakeSubsystem` re-ran an 8 km lake-grid query on the game thread every second: median 29.6 ms) found with the CSV profiler (`Exclusive/GameThread/Tickables`) and `MO_TICK_WATCH`; the query is now cached and runs on a worker thread. Packaged, fixed camera: Meadow p99 30.8 -> 14.8 ms, worst 36 -> 21 ms (74 -> 80 fps); Rainforest p99 39.2 -> 24.4 ms. Open: the remaining Rainforest frames over 25 ms are render-thread bound (a GPU cost, not a stall).

### P1 - traversal and survival feel
- [ ] **Swim polish** (splash / wading / stroke sounds, the HUD breath meter and cold-water heat exchange are done and verified in the editor; see Done): load-dependent buoyancy, fall damage into water, `Space` surfacing feel,
  clothing deformation on the horizontal poses, a swimming-pose blend (single-node playback snaps), creatures that swim. Exit: a naive-player swim loop (wade, swim, dive, get tired, nearly drown, recover, get out) with no glitches in a recorded editor run.
- [ ] **Thermal model has no thermal mass** (found by the cold-water test): `ApplyEnvironmentalTemperature` moves core temperature by `gap x exposure x 0.01` per game second against a flat +0.1 C/s regulation, so any
  cold ambient drives core temperature into severe hypothermia within a minute or two (shock term below 32 C, shaking, the hypothermia moodle; death by cold is NOT wired: `OnCardiacArrest` / `OnRespiratoryFailure`
  have no subscribers, the known lethality gap, so a swimmer does not die of it). Measured: a swimmer in 13 C lake water loses ~0.09 C per real second (36.6 -> 33.4 C in 36 s, 31.8 C at 70 s; <35 = "severe hypothermia" moodle, <32 mental impairment,
  <30 extreme). Real cold-water cooling is ~1 C per 15-20 min (net 200 W / 260 kJ per C), i.e. ~100x slower. Fix = a real heat balance (production incl. exertion + shivering vs loss in W, over the body's thermal
  mass), not a constant: it also corrects air exposure. Design fork for Wes (see Forks); the water coupling (`ApplyWaterToExposure`, `WaterTemperatureForAir`) stays as the input and is unit-tested.
- [ ] **Floating objects / boats** through Oceanology buoyancy (supersedes the unused `MOBuoyancyComponent`).
- [ ] Real reed / rush / cattail meshes for the shore tier (stand-ins today); needs a download decision (see Forks).
- [x] Mud lake bed (gravel today). **2026-10-13:** `UMOWorldGenSettings::UplandLakeBedSurfaceType` now defaults to `VST_MI_DarkPebbles` (dark wet stone): the bright white gravel ring around every lake is gone (packaged shots `pkg_probe/lake_final.log` vs `lake_bed.log`). A real silt / mud surface would need a new voxel material (none of the 13 is mud).

### P2 - world richness
- [ ] Per-biome fauna and POI density review (run `MO.WorldGen.SeedSurvey`, walk each biome for 3 minutes, log empty / repetitive stretches).
- [ ] Far terrain LOD tear on the mountains (known, "not ours": voxel far LOD) - confirm or fix.
- [ ] **Terrain material look (Wes, 2026-10-12; details in `Docs/Terrain_Generation_Exploration.md` To-do)**: (1) distant mountains: snow caps with dark rock, materials that switch with distance; (2) a fade + noise breakup on EVERY elevation / climate material split
  (no hard bands); (3) several sand materials so beaches and deserts stop repeating.
- [~] **Alpine / snow variation (Wes, 2026-10-13; details in `Docs/Terrain_Generation_Exploration.md` To-do)** -- aspect-driven snow DONE Oct 15 (checked at 300 m / 1 km / 4 km; sunlit snow near-flat white at 1 km = albedo/exposure, open): much more variation in the snow / rock so it reads as
  real mountains -- aspect-driven snow (northern hemisphere: less snow on south-facing slopes) and / or bigger, more dynamic noise.
- [ ] **Caves half the diameter and more lateral (Wes, 2026-10-13; same To-do)**: smaller passages that run sideways instead of straight down (generator v8).
- [ ] Weather per biome (rain / snow line at altitude) and the sound layers that go with it.

### P3 - game-wide quality
- [~] A naive-player first-ten-minutes pass (main menu -> new game -> tutorial -> first craft) with screenshots of every screen; fix every rough edge found. **Pass 1 (Oct 15, `Saved/FirstTen/`, -game
  instance via `ue.py inst` + `MO.Test.PressKey` / `ClickWidget` / `shot showui`) -- FIXED:** seed shown as "15,204" started a different world (6e1e70ac); the first hint was "Possession Menu: press P" (now gated
  on the first recruit; default quest SortOrder = last; 58bcd369); a starting stack of 10 sticks was in no slot, invisible (ReconcileSlots, e8c8d913); "[W]" / "[Mouse XY 2D-Axis]" hints -> "[WASD]" / "[Mouse]";
  SKILL / STATION labels with no value; selecting an uncraftable recipe showed no selection (31cdf120); status panel "Name: Unknown" for a named pawn, and a rename was lost at the next save (identity = the
  one source of name + gender; RequestSetPawnIdentity, bece2ea7); camera inside sapling / fern leaves (leaves within ~1.6 m of the lens dither away, `foliage_camera_fade.py`, 8aeff17b);
  wildlife never used its ~18.5k PCG spawn points + co-op spawning anchored to the host (182bec33). **OPEN (logged for Wes):** thirst + cold moodles red within a minute
  (survival pacing fork); the starting kit (10 cobbles, 20 sticks) crafts nothing in the hand menu -- first craft is the campfire (B); skill list has no
  icons; gender has no source on the body (player-set only); the Forage (ground) context menu's header / range text is pale on its translucent panel and hard to read over grass; hazel shrubs (Common Hazel, the commonest bush near spawn in the woodland biomes) are decorative cover ("grass" tag, BlockAll): E on one does nothing while the tutorial says "inspect a tree" -- make them harvestable for twigs / bark like the elderberry bush (data: DT_Resources + the palette tag)? (FIXED since: a new player could not place ANY building -- the ghost floated at the end of a level aim and its tint never turned red, 7a0f778b; placing without rotating left the build tutorial on "rotate"; the build menu named five structures "Build X" beside "Stick Floor" -- now structure names, and the detail's OUTPUT line no longer reads "Build Campfire"; "humanitys" in 21 descriptions; the first build was taught after the 7 movement lessons, 3af1c9c6; the New Game input boxes' dim text, abb8c4d2; the status Location row's raw cm, now "Locust Woodland · 19.6 km S, 3.1 km E".)
- [ ] Audio pass: footsteps done for humans (see Done); open: creature footsteps (hooves / paws: the skeleton has no ball_l / ball_r), structure / prop surfaces (stone, rock, metal: needs physical materials or
  the interface on the actors; props on top of terrain read as wood today), rain / snow overlays on the steps, interior / exterior mix, UDW outdoor mix failing in `RecursiveApplyAdjuster` (silenced via LogAudio=Warning).
- [x] HUD review of the packaged build (`Tools/win_input.ps1 -Action shot`, PrintWindow: works while another window covers the game, never moves the cursor): the tutorial banner was dark grey text with no panel
  (read as a rendering fault on a bright sky) and the quest tracker text ran past its panel: fixed (`hud_text_fixes.py`; the tracker's progress bar was also named `ProgressBar_0`, so the C++ `ProgressBar` member was
  never bound). The tool REFUSES real input unless the game window is the foreground window (another application was in front when it was first used: do not drive the mouse while the machine is in use).
- [ ] **Unbound optional widgets** (`ui check` `absent` lines, features that exist in C++ with no control in the blueprint): WBP_UnifiedInventory (CloseButton, loot-nearby buttons, secondary stack-organise),
  WBP_ItemContextMenu (TransferButton, PickupButton), WBP_JournalPanel (AbandonButton, TrackButtonText), WBP_BuildingRecipeDetail (craft amount spin / slider / text, resource-source checkboxes), WBP_EquipmentPanel (swap
  progress bars), WBP_MOColonyPortrait (ClickButton, NameLabel, ActivityIndicator), WBP_HUDRoot (WindIndicator: needs an authored up-pointing arrow texture). For each: wire it or delete the dead C++ member. Also
  bare native `EntryButton` in WBP_MOListEntry (mouse-dead; is it used?), and ~15 legacy WBPs in `/MOFramework/UI/` root that `ui check` cannot resolve (duplicates of the subfolder versions?).
- [~] **Moodle icons**: a generated glyph set (`Tools/gen_moodle_icons.py` -> `Content/UI/Icons/Moodles/T_Moodle_*`, imported with `Content/Python/import_moodle_icons.py`) fills the controller's EMPTY icon arrays
  (a designer-authored array is never touched): snowflake / thermometers / sun, droplets x1-3, fork+knife, glass, moon, a ring at the critical level. They are placeholders in style; a hand-drawn set can replace the
  textures (same names) without code changes. (A fresh pawn showing Dehydrated / Hungry within a minute: see the survival pacing fork.)
- [~] **UI design system (blocks the polish pass below)**: the screens were built one by one and share no panel / button / list-row / text language. The brief for Claude Design is `Docs/UI_Design_Brief_Prompt.md`
  (tokens, 9-slice kit, icon set, per-screen specs); when its output arrives: tokens -> `MOCommonButtonStyle` / panel kit -> HUD -> inventory -> crafting / building -> status -> the rest, via `ui build` specs.
  Landed meanwhile (screen-independent defects found in the packaged captures): recipe lists were one pink wall (the uncraftable / unbuildable default was a red tint; now neutral, dimmer), the recipe detail panel
  showed "Text Block" / a white square until a recipe was picked (now hidden until `DisplayRecipe`, starts empty), list selection colour muted. Still open there: the crafting list rows are 12 px text and a 0.5-scaled
  button, the amount spin box is a white native box, the "Characters" possession screen is unstyled, `FuelTimeText` / "Current:" placeholders.
- [ ] UI polish pass over the CommonUI screens (consistent spacing / focus / controller). The Options panel's faint labels and row order are gone with its design-kit rebuild
  (checked Oct 15: readable labels, VSync under Display, Graphics quality under Quality); the frame-rate cap read "120.0" -- kit spin boxes are whole numbers by default now.

## Forks (need Wes)
- **Survival stat pacing (found with the moodle strip)**: `UMOSurvivalStatsComponent` (the stats that drive the Hunger / Thirst / Energy moodles, and that eating / drinking restore) decays at 0.5 / 0.8 / 0.2 per REAL
  second with no clock scaling while `UMOGameClockSubsystem` runs at TimeScale 1.0. Measured: Thirst 82 -> 33 and Hunger 89 -> 58 in ONE minute: a new pawn is "Dehydrated" (critical) in ~2 min, "Starving" in ~3.3 min,
  "Exhausted" in ~8 min. The realistic model (`UMOMetabolismComponent`: glycogen 500 g, hydration 99.995 % after 14 s, 2.5 L/day) runs in parallel at realistic rates and drives no HUD. Options: (A, recommended) derive
  Hunger / Thirst / Energy from the metabolism nutrient levels so there is one realistic source of truth (pillar 11: "hunger decreases 1/sec" is the named anti-example); (B) scale the decay by the clock and run a faster
  clock; (C) keep the arcade pacing. Not changed: it sets the pacing of the whole early game (tutorial, food, crafting).
- Thermal model scale: replace the equilibrium-style temperature drift with a heat balance over a thermal mass (realistic rates: hours, not minutes, to hypothermia), or keep it gamey? Until decided, a lake swim
  reaches "severe hypothermia" in ~40 s.
- Real reed meshes: a Fab / Megascans download, or reuse the Electric Dreams plants (Taro, ferns) from the local sample project?
- Hair-bun engine patch (hair vanishes under water): the engine change needs a yes.
- Performance vs fidelity: if the 4090 floor is ~11 ms, a mid-range GPU cannot hold 60 fps at Epic; the target hardware tier must be chosen before the scalability work is tuned.
