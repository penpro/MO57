# Packaging, cooking and Shipping

> **Covers:** building a packaged Development or Shipping game, what breaks the cook, Shipping-only diagnosis, and where packaged builds
> keep their logs, saves and settings. The step-by-step recipe lives in `Docs/AUTONOMOUS_TOOLING.md` ("Packaging a Development build for
> real-account testing", "Packaged-game probes and profiling", "Packaging vs. the MCP port"); this doc is the system view and the traps.
> **Current state (2026-10-17):** Development packages build and pass `nettest packaged`; Wes plays Shipping builds over Steam;
> the engine is the 5.8.3 LAUNCHER install (engine modules are precompiled: `bUseLoggingInShipping`, `bUseConsoleInShipping` and engine
> patches are impossible: `Docs/Engine_Patches.md`).
> **Entry points:** `RunUAT BuildCookRun` (recipe in AUTONOMOUS_TOOLING), `Tools/pkg_probe.py`, `Tools/Update-BuildInfo.ps1` (PreBuildStep,
> mutex + retry), `MO57ShipDiag` target, `Config/DefaultGame.ini` `DirectoriesToAlwaysCook`, `Config/DefaultEngine.ini`
> (`DefaultGameTarget=MO57`). **Tests:** `nettest packaged`, `packaged-death`, `packaged-removal`, `bugreport --package`;
> `MOFramework.Water.OceanologyAssetsAreCooked`. **Open items:** `Docs/BACKLOG.md` § Performance, packaging, tooling.

## 1. Development vs Shipping

- **Package Development for real-account tests: Shipping writes no log** (UE_LOG is compiled out; the launcher engine refuses
  `bUseLoggingInShipping`), so a two-account Shipping test is undiagnosable. Diagnostics that must work in Shipping go into bug-report
  fields (`UMOBugReportSubsystem::RegisterContributor`, `Docs/Persistence.md` §4).
- **Shipping-only reports:** first reproduce the player's view (seed from his GameUserSettings `PendingWorldSeed`, HighResShot via
  `MO.Test.AfterReady` + `MO.Delay`, census) before reading engine code: "ZERO PCG in game" was the coastal spawn on bare sand.
  For real Shipping code paths: the `MO57ShipDiag` target (a logging Shipping exe; shared build environment, so only MOFramework UE_LOG
  lines appear), copy a fresh Shipping package to `Saved/StagedBuilds_ShipDiag`, drop the exe in, `pkg_probe.py --shipdiag`.
  Two game targets need `DefaultGameTarget=MO57` (DefaultEngine.ini) or UAT packaging fails instantly.
- **Settings and saves live in different places:** Shipping reads `%LOCALAPPDATA%/MO57/Saved` (Wes's REAL settings: back up byte for
  byte before editing, restore + compare after); a staged Development build reads its own `Saved/StagedBuilds_DevTest/.../Saved` (its
  Load list is empty; logs `...\Windows\MO57\Saved\Logs\MO57.log`, a second copy `MO57_2.log`).
- The staged `MO57.exe` is a LAUNCHER stub: stop the whole process tree, or the next BuildCookRun fails deleting the staging dir.
  Packaged CSV profiles land in the staged build's `Saved/Profiling/CSV` and are WIPED by the next `--package`.
- First launch after packaging hitches (PSO compilation) in a way `-game` dev instances never show; suspected behind the Oct 16 PCG stall
  and barren-spire reports.

## 2. What breaks the cook (each one stopped a package)

- **Subsystem `Initialize()` calling `GetSubsystem()` for a sibling that doesn't exist in a cook world** (e.g. UI subsystems): the cook
  commandlet promotes the ensure to FATAL. Defer cross-subsystem binds to `OnWorldBeginPlay` (`MOWeatherIntegrationSubsystem`).
- **A cvar in `DefaultScalability.ini` that is not `ECVF_Scalability`** is an ensure, fatal in the cook.
- **Substrate legacy-conversion masters with the shading-model pin wired to a constant** (migrated Electric Dreams / Megascans masters)
  abort `BuildCookRun` ("missing FAnisotropyPS") and crash the editor on recompile / duplicate. Fix the content:
  `Content/Python/fix_substrate_shading_model_pin.py` (idempotent; RE-RUN after migrating any new ED / Megascans master); foliage masters
  with per-instance shading models: `Content/Python/fix_hornbeam_masters.py`. Repair an unrepaired master BEFORE duplicating or recompiling
  it. An editor-only render proves nothing: package after migrating a new material family.
- **Soft-referenced plugin content is not cooked:** Oceanology loads materials / effects / sounds through soft paths in its runtime
  settings; list them in `DirectoriesToAlwaysCook` (the test reads every soft path). Retargeted animation folders referenced softly too.
- **Too-wide always-cook dirs pull in assets that can't compile:** Oceanology's `NS_GerstnerWaves(_Splash)` take ddx/ddy in a GPU compute
  script; DX12/SM6 compiles it, the DX11 (SM5) fallback can't (Niagara X3662). Only `AOceanologyWaterNiagaraWavesVolume` uses them (no MO
  level places one), so the always-cook list was narrowed to `Effects/RVT`.
- **"Default Material will be used" for an RT hit shader:** a Substrate Single Layer Water material without the legacy "Single Layer Water
  Material" output node (`M_MudPuddle_01_PCG`); add the output node. `Content/PCG/` is gitignored: that fix exists only on disk.
- **The MCP HTTP listener in the cook commandlet:** `bAutoStartServer=True` made every editor process (the cook child included) bind port
  8000 -> "unable to bind" -> cook failed. `bAutoStartServer=False`; `ue.py editor start` passes `-ModelContextProtocolStartServer`.
- **`Update-BuildInfo.ps1` two-writer race** (PreBuildStep runs per target; BuildCookRun builds Editor + Game together): an IOException
  in the first seconds looks like a flaky cook; the script now serialises writers with a named mutex.
- Diagnose cook-only shader failures from `Saved/ShaderDebugInfo/<platform>/<asset>`; trace referencers by binary grep of .uasset.
  One-package cook (~40 s, works with the editor open): `MSYS_NO_PATHCONV=1 /d/UnrealEngine/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe MO57.uproject -run=cook
  -targetplatform=Windows -cooksinglepackagenorefs -map=/Game/... -outputdir=<scratch> -unattended`.

## 3. After a package

Run the packaged co-op test (`python Tools/ue.py nettest packaged`) after touching water, plugin content, spawn or movement; check
`MO.WorldGen.Checksum` prints `match=1` in the packaged build (determinism, `/fp:precise`); for real windows, launch the packaged exe
yourself (`MO57.exe -NoSteam -windowed`; omit `-log`, it opens an overlapping console), find your PIDs and stop only those. Never use
computer-use `open_application` for the game or the editor (it starts another instance; "Unreal Editor" even starts UE 5.7).
