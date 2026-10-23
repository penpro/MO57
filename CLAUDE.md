# MO57: rules for every agent (Claude and Codex)

MO57 ("Metaverse: Origins") is an ultra-realistic procedural survival game on Unreal Engine 5.8.3 with destructible voxel terrain,
possess-any-pawn colonies and Steam co-op. Plugin: `Plugins/MOFramework/` (MOFrameworkCore <- MOFrameworkMedical <- MOFramework, + Editor).
This file holds the non-negotiable rules and a router; everything else is one hop away (`Docs/INDEX.md`). Wes's global rules (the Seven
Laws) also apply; where this file differs, this file wins for MO57 (e.g. commits, N1).

## STOP: check before these operations

| Before you... | Read / do |
|---|---|
| fix a bug | `Docs/ENGINEERING_PRINCIPLES.md`: one-sentence diagnosis naming the layer first; two failed patches = instrument |
| add behaviour to several systems | one abstraction + registration (`Docs/TECHNICAL_REFERENCE.md` § Module structure) |
| add any instant / auto / fast mode or change a duration | Principle 11 + the tedium rule (below) |
| pause anything | don't: `Docs/PAUSE_POLICY.md` (`AMOPlayerController::SetPause` refuses) |
| add or change an item, recipe, station or knowledge | `Docs/PROGRESSION_ROADMAP.md` first (the design authority), then `ue.py rows set` |
| edit a DataTable or CSV | `Docs/TOOLING.md` §4: live tables via `python Tools/ue.py rows set` (readback-verified); CSVs only via `Tools/ue_csv_utils.py`, never by hand |
| compile C++ | close the editor (`python Tools/ue.py editor stop`), announce the window in `claude-codex-coop.md`; `python Tools/check_unity_collisions.py` before committing new .cpp |
| edit engine source | the engine is a launcher install: stage the fix in `Docs/Engine_Patches.md` |
| delete or rename an asset | never from editor Python (it hangs the editor): delete files on disk with the editor closed |
| test an RPC or co-op | `Docs/Coop_And_Networking.md` §6: real processes (`ue.py inst` / `nettest`), never RPCs from editor Python |
| make a design call | `Docs/DECISIONS.md`: never re-ask a decided call; log a new fork with a recommendation instead of building blind |
| commit | the git rules below |
| hit something strange | `Docs/TRAPS.md` |

## Engineering principles (full text and examples: `Docs/ENGINEERING_PRINCIPLES.md`)

Production-ready software: fix systems, not symptoms. These override convenience and speed.
1. Trace a bug to the layer that creates the bad state, not where it surfaces.
2. If N systems need the same behaviour, build one abstraction (interface, base class, registry), not N copies.
3. A comment claiming behaviour doesn't make it true: verify; fix the code or the comment.
4. Read the whole flow (caller, callees, siblings) before changing one node.
5. When the cause is unclear, add diagnostic logging; two failed patches in a row = stop patching, start instrumenting.
6. Failure modes must be distinguishable at the data layer; the UI displays, it doesn't disambiguate.
7. Defaults are policy: set the default most code wants.
8. "It compiles" is not "it works": give a concrete repro with expected vs failing output; verify by using it.
9. Production-ready = extension without modification: the next instance of a pattern touches one new file.
10. Diagnose before designing the fix: one sentence naming the responsible layer.
11. **The simulation IS the design** (first pillar: realism). Every system models its real phenomenon; no instant build / dig / craft in
    shipped builds, no HP regen, no fast travel, no free skill use, no hidden timers, no game-y decay rates. **Tedium is not simulation:**
    batching repeated gestures into one action with a combined real duration is fine, and tedium that can't be batched is delegated to
    pawns (`UMOSurvivorJobQueueComponent`). Before any instant / auto / fast-X mode ask: tune the simulation? player skipping it (refuse)?
    batching gestures (allow, combined timer, or a pawn job)? Never collapse a simulated action to zero.

## Git, the shared tree, the editor

- **Commit and push after every completed, verified unit without asking** (Wes, 2026-10-17: "commit always, more backups and
  documentation is better, don't ask anymore just be diligent about documentation"). Update the docs the change touches in the same commit.
- **Batch the verification** (Wes, 2026-10-18: "stopping and packaging for EVERY change is insane and takes FOREVER ... find a batch of 10
  changes/fixes to make and THEN package and test"): write about 10 fixes, then ONE build, ONE `ue.py auto`, ONE host + joiner pass with a
  checklist covering every fix (package only when the batch needs packaged behaviour), then commit each verified fix on its own. A fix that
  fails moves to the next batch; never rebuild for one fix. `Docs/TOOLING.md` § Batch verification.
- **Defer look-and-feel checks to Wes** (2026-10-18: "for visual things like the glitchy foliage material wind motion that are easy for me
  to verify, differ those to me, just create a list"): verify mechanics, numbers and co-op behaviour yourself, but whether something LOOKS
  right (wind smoothness, a material, a slab seated, readability) goes on the "Wes: check by eye" list in `Docs/BACKLOG.md`.
- Codex works in the same tree: stage your exact paths, **never `git add -A`**, never stage or edit another agent's uncommitted files,
  never stage `.claude/settings.local.json` or `Content/Developers/penum/MegaMaterialCache.uasset`.
- `git add` fails silently here (case-insensitive paths; one bad pathspec stages nothing): check the staged column of `git status --short`
  before EVERY commit; after a push `git show HEAD --stat`.
- Coordinate in `claude-codex-coop.md` (append-only; CLAIM files before editing shared ones, RELEASE after). The editor, builds and
  instances have ONE owner at a time.
- Close the editor when a build needs it (Wes N5) with `ue.py editor stop` (graceful; refuses with unsaved packages). Never force-kill by
  process name; stop only processes you launched (recorded PID). Never touch Wes's saves (Harper_Wright-01, Test158, v1gate).
- After code changes: `python -m graphify update .` (the graph is in `graphify-out/`, gitignored; query it before grepping:
  `python -m graphify query "..."`, `explain "Symbol"`, `path "A" "B"`, `affected "X"`).
- Local / free / offline by default; paid APIs only with Wes's yes; downloads need his yes unless already authorized (`Docs/DECISIONS.md`).

## Traps that bite every session (full list: `Docs/TRAPS.md`)

1. Engine = UE **5.8.3 launcher install** at `D:\UnrealEngine\UE_5.8`: build with its UBT (`ue.py build`); the UE_5.7 path fails in ~5 s.
2. Editor Python runs every RPC LOCALLY: test verbs that send RPCs defer a tick; assert on the game's own logs.
3. `MSYS_NO_PATHCONV=1` for `/Game/...` and `/MOFramework/...` paths in Git Bash.
4. `-ExecCmds` splits on commas, not semicolons (`py x.py,QUIT_EDITOR`).
5. Close the editor before visual / PCG / harvest tests in a `-game` instance (an open editor starves its vegetation).
6. DataTables: `rows get` the live table before writing; the `*_JSON.json` / CSV mirrors are stale; raw MCP `set_rows` replaces whole structs.
7. Write scripts and text containing apostrophes or backslashes with the Write tool, not a bash heredoc; never write file content through
   PowerShell (PowerShell recipes are a fallback for running things: `Docs/TOOLING.md`).
8. Shipping writes no log: package Development to diagnose; for a visual report LOOK at the player's view first.
9. A simulated click proves nothing about hit-testing: one real click (`MO.Test.HitTest`) on anything a player must click.
10. Any new volume stamp needs tight bounds and a LOD range; any new removal path goes through `FMOHISMHarvestHelper::RemoveInstance`.

## Code conventions (full: `Docs/TECHNICAL_REFERENCE.md`)

- `RemoveAll(this)` / `RemoveDynamic` before binding delegates in `NativeConstruct()`.
- Input handling in C++ in `AMOPlayerController` (`SetupInputComponent`; raw keys in `InputKey`), never in Blueprint.
- A header declaring `DECLARE_DYNAMIC_MULTICAST_DELEGATE` at file scope needs a USTRUCT / UCLASS / UENUM; template users need full includes.
- UI: CommonUI bases (`UCommonActivatableWidget`); Menu input mode; buttons are the WBP `MOButton`, never the native class as a node;
  UMG is scripted with `python Tools/ue.py ui ...` (`Docs/UI_TOOLING.md`), not clicked.
- Voxel only through the `MOVoxel` facade; no upward includes across modules (Core contract + registry instead).
- 5.8: override `SetBase(FMovementBaseInterfaceData*)`; no `GetSubsystem()` of siblings in `Initialize()` (the cook dies).
- Warning log level for important flow events, Log for routine ones; debug file loggers default off.
- Authority: mutate on the server (`HasAuthority()`), clients send intents; a new player action = a controller-owned RPC into an
  authority-gated component; timed actions on the pawn's `UMOTimedActionComponent`.

## Router

| Topic | File |
|---|---|
| Every live doc | `Docs/INDEX.md` |
| Open work / Wes's calls and forks / traps | `Docs/BACKLOG.md` / `Docs/DECISIONS.md` / `Docs/TRAPS.md` |
| Tools, build, tests, DataTables, console verbs, Steam patch notes | `Docs/TOOLING.md` (deep manual: `Docs/AUTONOMOUS_TOOLING.md`) |
| Vision, pillars, scope | `Docs/VISION.md` |
| Architecture, conventions, replication, plugins | `Docs/TECHNICAL_REFERENCE.md` |
| Status of each track | `Docs/PROJECT_STATUS.md` |
| World generation, biomes, terrain material | `Docs/Terrain_Generation_Exploration.md` |
| Caves, geology, ore, dig time | `Docs/Caves_And_Geology.md` |
| Vegetation, PCG, removals, loading reveal | `Docs/PCG.md` |
| POIs, cliffs, world features | `Docs/World_Features_Architecture.md` |
| Voxel plugin | `Docs/Voxel_Plugin_Reference.md` |
| Ocean, lakes, swimming / drinking, illness | `Docs/Water.md` / `Docs/Water_And_Illness.md` |
| Light, darkness, exposure | `Docs/Light_Sources.md` |
| Weather, clock, time | `Docs/Weather_And_Time.md` |
| World creation (shape, pace multipliers, starting biome) | `Docs/World_Creation.md` |
| Medical, survival, death | `Docs/Medical_And_Survival.md` |
| Progression design (authority) / crafting code | `Docs/PROGRESSION_ROADMAP.md` / `Docs/Crafting_And_Progression.md` |
| Technology history (research) | `Docs/Technology_History.md` |
| Item art and icons | `Docs/Item_Art_Pipeline.md` |
| Building / terraform and excavation | `Docs/Building.md` / `Docs/Terraform_Excavation_Plan.md` |
| AI, survivors, colony | `Docs/AI_And_Colony.md` |
| Co-op, sessions, RPC, interaction | `Docs/Coop_And_Networking.md` |
| UI architecture / building UI | `Docs/UI.md` / `Docs/UI_TOOLING.md` |
| Saves, identity, bug reports | `Docs/Persistence.md` |
| Audio / performance / packaging | `Docs/Audio.md` / `Docs/Performance.md` / `Docs/Packaging.md` |
| Engine patches / pause policy | `Docs/Engine_Patches.md` / `Docs/PAUSE_POLICY.md` |
| Codex coordination | `claude-codex-coop.md`, `Docs/agent/` |
| CS 399 practicum | `Docs/CS399/` |
