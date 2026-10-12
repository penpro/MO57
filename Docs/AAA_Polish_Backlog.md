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
- Tooling: `MO.Perf.Bench/Tour`, `MO.NewGame`, `MO.Water.*` (incl. `MO.Water.Meter`), `MO.Audio.Mixes`, `Tools/check_unity_collisions.py`; editor PIE no longer throttled to 3 fps (user EditorSettings.ini).

## Backlog (ordered; the first unchecked item is next)

### P0 - measure and fix what players feel first
- [ ] **Frame time**: read the per-biome GPU pass tree (`PERF_PROFILE_GPU=1`), find the fixed ~11 ms GPU floor and the Rainforest +6 ms; decide scalability tiers (Lumen / VSM / TSR / foliage cull distances).
  Exit: p95 <= 16.6 ms at 1080p on the 4090 in all nine biomes AND a documented Low / Medium / High / Epic scalability set with measured costs.
- [ ] **Hitches**: worst frames of 200-333 ms (AlpineHeath, ShoreStrand) and p99 up to 36 ms (LocustWoodland) - find what generates them (PCG partition generation, voxel mesh, async loads, GC) and spread or hide it.
  Exit: worst frame <= 50 ms in the tour.
- [x] Per-frame log spam (`RecursiveApplyAdjuster`): `MOAudioMixGuard` registers the orphan sound classes; verify with the next packaged run.

### P1 - traversal and survival feel
- [ ] **Swim polish** (splash / wading / stroke sounds, the HUD breath meter and cold-water heat exchange are done and verified in the editor; see Done): load-dependent buoyancy, fall damage into water, `Space` surfacing feel,
  clothing deformation on the horizontal poses, a swimming-pose blend (single-node playback snaps), creatures that swim. Exit: a naive-player swim loop (wade, swim, dive, get tired, nearly drown, recover, get out) with no glitches in a recorded editor run.
- [ ] **Thermal model has no thermal mass** (found by the cold-water test): `ApplyEnvironmentalTemperature` moves core temperature by `gap x exposure x 0.01` per game second against a flat +0.1 C/s regulation, so any
  cold ambient is lethal within minutes. Measured: a swimmer in 13 C lake water loses ~0.09 C per real second (36.6 -> 33.4 C in 36 s, 31.8 C at 70 s; <35 = "severe hypothermia" moodle, <32 mental impairment,
  <30 extreme). Real cold-water cooling is ~1 C per 15-20 min (net 200 W / 260 kJ per C), i.e. ~100x slower. Fix = a real heat balance (production incl. exertion + shivering vs loss in W, over the body's thermal
  mass), not a constant: it also corrects air exposure. Design fork for Wes (see Forks); the water coupling (`ApplyWaterToExposure`, `WaterTemperatureForAir`) stays as the input and is unit-tested.
- [ ] **Floating objects / boats** through Oceanology buoyancy (supersedes the unused `MOBuoyancyComponent`).
- [ ] Real reed / rush / cattail meshes for the shore tier (stand-ins today); needs a download decision (see Forks).
- [ ] Mud lake bed (gravel today).

### P2 - world richness
- [ ] Per-biome fauna and POI density review (run `MO.WorldGen.SeedSurvey`, walk each biome for 3 minutes, log empty / repetitive stretches).
- [ ] Far terrain LOD tear on the mountains (known, "not ours": voxel far LOD) - confirm or fix.
- [ ] Weather per biome (rain / snow line at altitude) and the sound layers that go with it.

### P3 - game-wide quality
- [ ] A naive-player first-ten-minutes pass (main menu -> new game -> tutorial -> first craft) with screenshots of every screen; fix every rough edge found.
- [ ] Audio pass: footsteps by surface (no C++ trigger exists today), water, interior / exterior mix.
- [ ] UI polish pass over the CommonUI screens (consistent spacing / focus / controller).

## Forks (need Wes)
- Thermal model scale: replace the equilibrium-style temperature drift with a heat balance over a thermal mass (realistic rates: hours, not minutes, to hypothermia), or keep it gamey? Until decided, a lake swim
  reaches "severe hypothermia" in ~40 s.
- Real reed meshes: a Fab / Megascans download, or reuse the Electric Dreams plants (Taro, ferns) from the local sample project?
- Hair-bun engine patch (hair vanishes under water): the engine change needs a yes.
- Performance vs fidelity: if the 4090 floor is ~11 ms, a mid-range GPU cannot hold 60 fps at Epic; the target hardware tier must be chosen before the scalability work is tuned.
