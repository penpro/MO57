# Performance: targets, how to measure, what was found

> **Covers:** the frame-time targets, the measuring protocols (packaged tours, CSV profiler, Insights, GPU profile, PIE A/B), graphics
> quality tiers, and the findings that set today's rules. PCG-specific performance rules: `Docs/PCG.md` §6. Coding rules (tick rates,
> asset loading, "never walk the world"): `Docs/TECHNICAL_REFERENCE.md` "Performance Guidelines".
> **Current state (2026-10-17):** packaged Development, 1080p, RTX 4090: ~77-100 fps per biome after the Oct 14 coverage pass (GPU 8.3-11.8
> ms; Rainforest / DryScrub the heaviest); new-game READY ~17 s; the 1 Hz 30 ms hitch is gone (lake query off the game thread).
> **Tools:** `MO.Perf.Tour ...` (via `Tools/ov.bat`: `PERF_LABEL`, `SKIP_PACKAGE`, `PERF_PROFILE_GPU`), `MO.Perf.Bench`, `MO.Perf.Csv 1`,
> `Tools/pkg_probe.py`, `Tools/csv_frames.py`, `Tools/insights_export.py`, `MO_TICK_WATCH(TEXT("Name"))` (`MOTickWatchdog.h`), `[MOPERF] HITCH`
> ring lines, `ProfileGPU` / `profilegpu`, `Tools/perf_experiments*.sh` + `perf_experiments_round*.txt`. **Open items:** `Docs/BACKLOG.md` §
> Performance, packaging, tooling. **Decision pending:** the reference resolution / quality and the target hardware tier (DECISIONS F4).

## 1. Targets (the quality gates)

| Gate | Target | Last measured | Tool |
|---|---|---|---|
| Frame time, packaged Dev build, 1080p, RTX 4090 | p95 <= 16.6 ms (60 fps) in every biome, p99 <= 25 ms | Oct 14: avg 77-100 fps, p95 11.1-13.9 ms (see `Docs/Archive/Ground_Coverage_2026-10-14.md`); earlier Oct 11 Rainforest 51 fps fixed by the wind / VSM change | `MO.Perf.Tour` via `Tools/ov.bat` |
| Log health | no per-frame spam; no Error lines at boot | RecursiveApplyAdjuster spam fixed (31k lines / 6 min before) | `grep -c` in the packaged `MO57.log` |
| Tests | 100 % of MOFramework automation | 189/189 on Oct 11 (more since) | `python Tools/ue.py auto --filter MOFramework` |
| Build hygiene | no duplicate file-local names (adaptive unity); package cooks + co-op test passes | clean | `python Tools/check_unity_collisions.py`, `nettest packaged` |

Open question for Wes (F4): if the 4090 floor is ~11 ms, a mid-range GPU cannot hold 60 fps at Epic; choose the reference resolution /
quality and the target hardware tier before tuning scalability. Tours were run windowed at 960x540 and 1920x1080.

## 2. How to measure (protocols)

- **Benchmarks only with no editor running** (the GPU is shared with the desktop). Compare within ONE session: results swing +-20 % with
  how many stale far cells are still loaded; run-to-run GPU noise is ~0.7 ms.
- **Packaged tour** (the reference): Development build, 1920x1080, a fixed seed (perf seed 1029024224), clear sky,
  `MO.Perf.Tour 60 20 1 0 1 <biomes>` (60 s settle, 20 s sample, fixed yaw). A FULL tour visits the same spots in the same order; a biome-list
  tour picks different spots, so compare full tours against a three-run baseline.
- **Hitches:** `MO.Perf.Tour ... fixed_yaw` tells whether a hitch depends on the view; `[MOPERF] HITCH` lines log a RING (3 frames before /
  the hitch / 3 after) because the game / render / GPU counters LAG the frame-time measurement by one frame; the hitch spacing gives its
  period. `MO.Perf.Csv 1` wraps the sample in `csvprofile start/stop`: rank CSV columns by (mean on hitch frames - mean elsewhere).
  `MO_TICK_WATCH` in every tickable subsystem logs `[MOTICK] Name took N ms` above 5 ms. The engine's `-hitchdetection` is compiled out.
- **Insights headless:** capture with `--extra "-trace=cpu,frame,bookmark[,voxel] -statnamedevents -tracefile=..."`, export with
  `Tools/insights_export.py` (`UnrealInsights.exe -OpenTraceFile=... -ExecOnAnalysisCompleteCmd=...`); every MO component BeginPlay has a
  `QUICK_SCOPE_CYCLE_COUNTER` (`--timers "STAT_UMO*_BeginPlay"`).
- **First-use loads after READY:** `--extra='-LogCmds="LogStreaming Verbose"'`; each `FlushAsyncLoading` after READY, then the next
  AddPackage line names it. Fix at the LOADER (preload), not at the use.
- **GPU:** `ProfileGPU` (via `-ExecCmds` through `MO.Test.AfterReady` / `MO.Delay`) at a fixed `MO.WorldGen.FindBiomeSpot <biome> 40 go` spot;
  parse the table's last column for pass names. Run the shadows-off control (`ShowFlag.DynamicShadows 0`) before blaming shadows.
- **PIE A/B:** the editor is frame-capped (~58 fps), so compare `gpu=` / `draws=` at a pinned `MO.Cam.LookAt`, not fps.
- **Voxel stalls:** `voxel.LogStateTime 1` (wall vs summed CPU per state), `-DumpVoxelStatus`, our local plugin patch
  `voxel.render.LogSlowChunkMs <ms>`; the readiness trace prints `voxelNewState` / `voxelPending`.

## 3. Graphics quality tiers

(TECHNICAL_REFERENCE, Oct 2026, verbatim) `UMOGameSettings` (a `UGameUserSettings`): level 0 Low .. 3 Epic over the engine's scalability
groups; `EnsureGraphicsQualityChosen()` benchmarks once per machine (not in the editor / on servers / `-unattended` / `-NoQualityBenchmark`:
every test harness passes it). `Config/DefaultScalability.ini` holds the MO-specific cuts: ONLY `ECVF_Scalability` cvars are allowed
(anything else is an ensure, fatal in the cook), and a cvar overridden at a lower level must also be pinned at Epic. `ApplyMOSettings()`
applies the non-resolution engine settings (the class skips `Super::ApplySettings` on purpose to avoid viewport rebuilds). Options panel:
quality combo, VSync, the display's real resolution list. Tests: `MOFramework.Graphics.*`.

Measured tiers (avg fps Rainforest / Temperate / Meadow, 4090, 1080p, packaged, Oct 12): Epic 59/65/69, High 69/78/81, Medium 77/85/87,
Low 115/121/129 (GPU floor ~6-7 ms). Lumen HWRT is off below Epic. A `-game` instance has GIsEditor false, so it WOULD benchmark: every
harness launch passes `-NoQualityBenchmark` (`ue_inst.py`, `perf_experiments.sh`, `ov_task.sh`). Epic texture pool 4000 MB (Cine 6000),
`LimitPoolSizeToVRAM` (the 1000 MB default printed "STREAMING POOL OVER BUDGET").

## 4. Findings that set today's rules

- **The 1 Hz 30 ms hitch** (Oct 11-12): `UMOUplandLakeSubsystem::RefreshNow` ran an 8 km lake query (median 29.6 ms) on the game thread every
  second. Fix: cache + `IsQueryFresh` (viewers within 200 m) + `Async(ThreadPool)` (Meadow p99 30.8 -> 14.8 ms). Wrong turns: an audio guard
  (a real cost, not THE cause) and 9 runs of GPU bisection driven by the lagging counters. **Rule: periodic game-thread work must not
  search the world or recompute a pure function of slowly changing inputs.**
- **Dense-forest GPU = wind on shadow casters** (VSM page invalidation): `mo.Foliage.RigidShadows`, `WindDisableDistanceCm` (`Docs/PCG.md` §6).
- **New-game reveal 45.7 -> 7.1 s** (Oct 13): PCG scheduler order, FName per point, dead chains, LOD-culled generator features
  (`Docs/PCG.md` §6, §8).
- **Post-reveal worst frame 77 -> 38 ms** (Oct 13 eve, 1e73d060): exact cave column memo, time-sliced biome spawner, AI asset preload,
  `MaxSimultaneousTileGenerationJobsCount=8` for navigation (halved the worst frame).
- Far trees: real trees to 1.5 km, stand-ins and canopy proxies beyond, `InstanceMinDrawDistance` handoff, no shadows / RT on far tiers;
  native grass fades 150-200 m: 32 -> 12.6 ms in the Oct 12 measurement (`Docs/PCG.md` §3).
- The Oct 14 coverage pass cost +0.3 to +1.5 ms GPU per biome and READY 14.0 -> 17.3 s; adaptive build batches cap the worst build batch at ~9 ms.

## 5. The Oct 22 sweep (Wes: "a deep dive on performance ... old logging and other deprecated code ... a general sweep")

What was looked at, and what was found, by evidence rather than by grep alone:

- **Boot: the main menu read every save on disk twice.** The Load panel built its list in `NativeConstruct` (the main menu constructs it
  hidden at boot), `UMOLoadPanel::SetFilterToCurrentWorld` rebuilt it, and the main menu rebuilt it a third time; each listing called
  `GetSaveSlotMetadata`, which deserializes a WHOLE world save (0.6-0.9 MB) for a name, a time and a thumbnail. One harness host: 39
  saves, 78 full loads, ~3 s inside one frame before the title screen. Fixed: the list is built when a panel is shown, and the metadata is
  kept per slot while the file's size and time are the same (a rename, a save or a delete changes them). The old-format appearance
  warnings those loads printed (156 per session: `SkinTone` / `EyeColor` changed type) are gone with them; such a save still loads
  its survivors with the default skin tone and eye colour (the data, not a cost).
- **Logging.** `UE_LOG` is compiled out of Shipping, so log lines cost only Development / editor runs; there they cost readability more
  than frame time. The biome spawner's per-cell summary was 1,290 Warning lines in one nettest host (it pushed every other line out of
  a log tail the harness read): now Log level behind `mo.PCG.LogCells 1`. The 1 Hz `[MONet]` heartbeat stays (the network diagnosis
  reads it). The Oct 18 pass already commented out 399 debug lines in 96 files; nothing under `#if 0`.
- **World walks on player actions.** `UMOForagingSubsystem::QueryHISMInstancesInRadius` and `AMOSurvivorController::FindNearestHarvestTarget`
  iterate EVERY actor of the world, `FindGatherableCoverKinds` every instanced component in memory, to find what is within a few
  metres. They run per action (a forage search, the ground menu opening, a survivor looking for work), not per frame; each now logs
  how many it walked and the milliseconds. Measured in the Oct 22 nettest: a survivor's harvest-target search walked 21,888 actors in
  2.8 ms (tolerable); a survivor's 200 m GATHER search (`QueryHISMInstancesInRadius`) walked 4,314 instanced components instance by
  instance, **137 ms on the host's game thread** each time a survivor takes a gather job. Fix (N-196): skip a component whose bounds
  cannot reach the sphere, ask a HISM's cluster tree for the candidates, keep the exact distance test.
- **Ticking:** 21 classes tick; all but the per-frame ones (buoyancy, foot plants, AI controllers, water) throttle with a tick interval
  or a timer, and the tickable subsystems carry `MO_TICK_WATCH`. Nothing new to cut there.
