# Tooling: every dev entry point

> **What this is:** the catalog of how to build, run, test, author data, inspect and ship MO57, one line per tool with a pointer. The deep
> manual (transport details, hit/miss history, recipes) is `Docs/AUTONOMOUS_TOOLING.md`; UI authoring is `Docs/UI_TOOLING.md`; the older
> bridge playbook is `Docs/Agent_PIE_Testing.md`. Built 2026-10-17 from CLAUDE.md (CLI, CSV, graphify sections, verbatim below), the
> memories ue-unified-cli / ue-multiprocess-testing / ue-editor-mcp / overnight-runner, and the Tools/ folder.
> **Rules:** bash / `ue.py` first; PowerShell recipes are the fallback (Wes, 2026-10-17: "powershell as backup since the tools aren't
> bulletproof, whatever keeps work moving along") but never WRITE file content through PowerShell. Close the editor when a build needs it
> (`ue.py editor stop`, graceful) unless Wes says otherwise; announce build / editor windows in `claude-codex-coop.md`.

## 1. `python Tools/ue.py` (start here)

Run from bash / git-bash at the repo root. Exit codes: 0 ok, 1 operation failed, 2 environment down. Paths are hard-coded to the main tree
and UE_5.8.

| Verb | Does |
|---|---|
| `status` | editor / bridge / MCP / PIE in one shot (also re-starts the MCP through the bridge for a hand-launched editor) |
| `run "<console cmd>" [--grep TAG]` | a console command, correlated (begin / end markers) + the MO57.log delta |
| `py -c '...'` / `py --file x.py` | editor Python (multi-line via --file) |
| `build [--target MO57Editor]` / `cycle [--boot --seed N] [--test]` | UBT 5.8 build / the whole loop: stop editor -> build -> relaunch -> wait bridge -> boot -> RunAll |
| `auto [--filter MOFramework]` | headless automation via UnrealEditor-Cmd -nullrhi (editor must be CLOSED); parses the report JSON |
| `test [--suite RunAll|ValidateData]` | in-editor runtime suites, exit code = pass / fail |
| `editor start|stop|wait` | `stop` = QUIT_EDITOR through the bridge, never a force-kill, refuses with unsaved packages |
| `boot [--seed N]`, `pie begin|end` | to in-game / PIE control |
| `seq file.py` | tick-driven multi-frame sequences (claude_seq); read `inst_<name>/ue_out.txt` for tracebacks |
| `rows get|set|list <table> [--file rows.json]` | DataTable authoring with readback verify, TMap guard, per-row add+set (section 4) |
| `refresh-data` | invalidate the item + recipe static caches after MCP edits |
| `mcp ...` | raw MCP toolsets (e.g. `mcp dt remove_rows`) |
| `asset assign|import|shot` | art assignment, file import, visual-QA shots |
| `ui contract|scaffold|build|check|dump|remove|menu|click|find|shot` | Widget Blueprints from spec files (`Docs/UI_TOOLING.md`) |
| `inst start|stop|do|list <name> [--sound] [--creator] [--res WxH] [--nosteam]` | extra `-game` processes with their own bridge + log; `inst do NAME py` refuses editor-only asset edits, which crash a -game process (TRAPS §5). A start keeps the last run's log as `game.prev.log` beside `game.log` (Oct 19: a one-off failure keeps its evidence through the restart that checks it) |
| `nettest lan|steam-host|game|hostsave|soak|actions|churn|features|packaged|packaged-suite|packaged-death|packaged-removal|bugreport` | real multi-process co-op / packaged gates (negative controls: `game --skew-seed / --withhold-seed / --withhold-sync`; `MO_ACTIONS_ONLY=_case,...` reruns named action cases) |
| `nettest netdiag [--seconds N] [--phases clean,lag50,loss1,ctl_ratecap,ctl_speedhack] [--keep]` | checks the co-op SMOOTHNESS instruments on two local processes (controls that must fail); loopback cannot show internet rubber banding: real sessions are read with `python -I Tools/ue_netdiag.py logs host.log joiner.log` (`Docs/Coop_And_Networking.md` §7) |
| `logs`, `results`, `save`, `config`, `mptest` | helpers (see `--help`). There is no top-level `shot`: in an instance run `ue.py inst do NAME run "shot showui"` (PNG in `Saved/Screenshots/WindowsEditor/`); widget shots are `ui shot` |

Older PowerShell helpers still work: `Tools\agent_boot_newgame.ps1 -Seed N` (one command to in-game), `Tools\agent_state.ps1` (editor + PIE
stage), `Tools\win_input.ps1 -ProcessId <pid> -Action shot|click|drag|key ...` (real input / window capture for instances and packaged windows; `drag -X -Y -X2 -Y2` is a real
UMG drag and drop). `MO.Test.InjectInput <IA_Name> [X Y] [Seconds]` feeds a REAL Enhanced Input action (from `/MOFramework/Input/`) through the
controller's own handlers -- tutorial events, menu gates -- which `MO.Test.Input` skips (it calls the pawn directly): use it for naive-player passes.
Crops (Oct 26): `MO.Test.AgeCrops <Days> [RadiusCm=1500]` (plots near player 0 grow that many more days; logs stage and growth), with
`MO.Test.WorldAction list|Sow:<CropId>|Harvest|ClearPlot` on the plot. A creative world tills a plot at once; `Gathering=0.1` shortens sowing / harvest.
Creatures (Oct 26): `MO.Test.SpawnCreature <Species|ClassPath> [DistanceCm=600] [SideCm=0]` (on the ground in front of player 0; Species = Pig ->
`/MOFramework/MOBS/Pig/BP_Pig`), `MO.Test.SpawnGroup <Species|ClassPath> [DistanceCm=1500]` (a whole group as the spawner makes it; logs its size and
spread). Python cannot spawn actors in a `-game` instance. Prey bolt from a player on sight: to photograph one, stop its brain
(`ctrl.brain_component.stop_logic("photo")`) and hide instanced foliage (`ShowFlag.InstancedStaticMeshes 0`, `...InstancedFoliage 0`, `...InstancedGrass 0`).
Survivor bases and the body (Oct 27): `MO.Test.Base <Id>` (player 0 takes a base as the creator gives it: meshes, grooms, the base's body),
`MO.Test.BodyComposition <fat %> [muscle kg]` (the metabolism's body, at a weight that holds; the body follows within 2 s),
`MO.Test.BodyBuild <Bust|Hips|Waist|Thighs> <-1..1>`, and the query `MO.Test.BodyShape` (`[MOQUERY] BODYSHAPE`, one line per MetaHuman pawn:
the base, the body it shows and every body-shape weight on the body and face; "(this machine's player)" marks the local player's own pawn).
Checks: `python Tools/check_creator_bases.py` (one instance with the creator: real clicks on the Base arrow and a build slider, the body read
back, the Hair style label, Begin, her hair kept on its strands in game through a round trip to the femme (the groom LOD bias read back;
6_in_game / 7_femme_round_trip show the hair at the third-person distance), the merged species spawning, a save and reload; screenshots in
`Saved/Screenshots/creator_bases/`), `python Tools/ue.py nettest bases` (a joiner draws the host's base, body fat, muscle and build on the body
and the face) and `python Tools/ue.py nettest packaged-bases [--package]` (the packaged Development game: Lillian's meshes and her own
groom were cooked, the heavy shape draws).
`MO.Test.ClickWidget <Name> at=<0..1>` presses that far across a non-button widget (a slider takes the value there).
**`MO.Save.LoadFrom` reloads in place without the post-load handoff** (re-grounding, re-possessing the last-played pawn: `AMOGameMode`'s
load path): the player is left with no pawn and every survivor under its AI controller. Read records after it, not the player's state (Oct 27).
Another project (Oct 26): `MO_PROJECT=D:/UEProjects/MO_Tools python Tools/ue.py build|editor start|py|run|inst ...` drives MO_Tools, the lab
(`D:/UEProjects/MO_Tools/CLAUDE.md`): its own log, its own bridge folder (%TEMP%/claude/MO_Tools), its own build target; "is the editor
running" only counts that project's processes. Voxel shader hooks: `ue.py` re-applies them (the plugin's ApplyVoxelShaderHooks commandlet)
before the editor, a -game instance or a package whenever a launcher action has wiped them (TRAPS).
Co-op traversal: `python Tools/ue.py nettest traversal` (a joiner hops up, grabs, climbs, lets go; host and joiner must agree; ~6 min).
Traversal (Oct 26): `MO.Test.SpawnLedge <HeightCm> [DepthCm=150] [Climbable=0] [DistanceCm=90] [PlayerIndex=0]` (a replicated `AMOTestLedge` in front of
that player's pawn -- run it on the host for a joiner, whose own client must then turn to it -- the pawn turns to
face it; use 60 cm: the rays reach capsule radius + 60), `MO.Test.ClearLedges`, `MO.Test.Traversal` (what a jump would do, and why not).

## 2. Build

`python Tools/ue.py build` (or `cycle`). The engine is the UE 5.8.4 LAUNCHER install (updated from 5.8.3 by the launcher Oct 26; an engine update rebuilds every module once, the Voxel plugin included): engine modules are precompiled; engine fixes are staged
in `Docs/Engine_Patches.md`. Before a build: the editor must be closed (Live Coding blocks UBT) -- `ue.py editor stop`; check the module DLL
mtime if a change seems not to apply (Live Coding may have hot-patched). Run `python Tools/check_unity_collisions.py` before committing new
.cpp files. After code changes: `python -m graphify update .`.
Every `build` / `cycle` first scraps the dev saves (`Saved/SaveGames/*.sav`) and the editor autosaves (`Saved/Autosaves*`): Wes, Oct 25 2026,
"if i want save games i'll play them on steam" (`scrap_dev_saves`; `--keep-saves` skips it; the packaged / Steam saves are never touched).
`ue.py py` exits 3 "STILL RUNNING" when the editor took a call that outlasted `--timeout`: it finishes on its own, never re-run it.
`ue.py py` exits 4 "GONE" when the editor took the call and then exited (a crash: `Saved/Crashes`); with no editor at all a call
reports "bridge not responding" within ~2 s instead of at the timeout. Waiting on a long call yourself: check the process is alive too.
The bridge never runs one call inside another (a slow-task dialog pumps Slate; Oct 26): a call sent meanwhile waits its turn.

PowerShell fallback (CLAUDE.md until 2026-10-17, verbatim):

Build commands (PowerShell):
```powershell
# Build Editor (Development)
powershell.exe -Command "& 'D:\UnrealEngine\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe' MO57Editor Win64 Development '-Project=D:\ueprojects\mo57\mo57.uproject'"

# Build Game (Development)
powershell.exe -Command "& 'D:\UnrealEngine\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe' MO57 Win64 Development '-Project=D:\ueprojects\mo57\mo57.uproject'"

# View build logs
powershell.exe -Command "Get-Content 'C:\Users\penum\AppData\Local\UnrealBuildTool\Log.txt' -Tail 50"
```

File operations (PowerShell; prefer the Glob / Read tools):
```powershell
# List directory contents
powershell.exe -Command "Get-ChildItem 'D:\ueprojects\mo57' -Directory"

# Find files recursively
powershell.exe -Command "Get-ChildItem 'D:\ueprojects\mo57' -Recurse -Filter '*.cpp'"
```

Git operations (**[SUPERSEDED 2026-10-17: never `git add -A` in the shared tree; stage explicit paths and check the staged column -- Wes N1]**):
```bash
git status
git add -A && git commit -m "message" && git push
git log --oneline -10
```
To commit PART of a file (two fixes in one file, one verified): `python Tools/git_stage_part.py <path> --func a,b` (Python functions) or
`--text <regex>` (changes whose new text matches), `--dry` first, then read `git show :<path>`. Not `git apply --unidiff-zero` of some hunks:
it misplaces insertions (TRAPS section 1).

Equivalent UBT call from bash: `"/d/UnrealEngine/UE_5.8/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" MO57Editor Win64 Development
-Project="D:/UEProjects/MO57/MO57.uproject" -NoHotReload` (~20-40 s for one .cpp). A clean `UE_5.7` path fails in ~5 s (`CS0117 ... V7`).

## 3. Tests

- Headless automation: `ue.py auto --filter MOFramework` (editor closed). Groups: `MOFramework.WorldGen.*`, `.PCG.*`, `.Water.*`, `.Medical.*`,
  `.Colony.*`, `.UI.*`, `.Audio.*`, `.Footsteps.*`, `.Light.*`, `.Graphics.*`, `.Inventory.*`, `.Crafting.*`, `.Survival.*`, `.BugReport.*` ...
- Runtime harness verbs (in PIE / an instance): `MO.Test.*` (38: State, FindWidget, ClickWidget, HitTest, PressKey, Input, Craft, GrantRecipe,
  Forage, DropPickup, Attack, Build, ValidateData, ProgressionMap, RemovalPersists, DeathFlow, SystemicDeath, RescueTrace, EmbedPawn,
  AfterReady ...; co-op instance verbs in MOCoopInstanceCheats.cpp: GroundItems, InstanceAt, InstanceInteract, ResourceNodes, Harvest, Inspect,
  SurvivorCommand, Sweepable, NearestSweepable -- instances named by mesh + place, since their names differ per machine); `MO.Delay` sequences commands after READY. `MO.UI.RunTest <name>` / `MO.UI.RunAllTests` for frame-stepped UI tests.
- Offline Python tests: `Tools/tests/` (test_ue_inst.py, test_ui_tooling.py ...).
- Co-op and packaged gates: `ue.py nettest <mode>` (section 1); `Docs/Coop_And_Networking.md` §6 for the rules.
- Naive-player passes: an instance driven by console verbs + `shot showui` (`r.setres 1920x1080w` first; wait for the SECOND READY line).

### Batch verification (Wes, 2026-10-18: one test pass per ~10 fixes, never per change)
1. **Write the batch first.** Collect about 10 fixes, each in its own files where possible (a header change costs a wide rebuild). Data rows
   (`ue.py rows set`), UI specs (`ue.py ui build`) and Python need no build: do them in one editor session, then close it.
2. **One build:** `ue.py build` (it names any `-game` instance still holding the DLLs). A compile error is fixed in place: still one build window.
3. **One automation run:** `ue.py auto --filter MOFramework` (~5.5 min since Oct 19: three brute-force cave tests were 18 of the 22 minutes on one
   thread; they now draw their random trials in order and evaluate them on every core, `RunTrials` in MOWorldGenCaveTests.cpp, with the
   serial results. Any new random-trial search over the generator: use it). It can run in the background while step 4 runs.
4. **One co-op pass:** `MO_ACTIONS_ONLY=_none_ python Tools/ue.py nettest actions --keep` pairs a host and a joiner on the real map (~3 min,
   a random world unless `--seed N`: `nettest game` / `actions` host that world through `MO.Session.Host <Name> 4 <Seed>`; until Oct 20 the
   flag was silently ignored. A random world can spawn on a bare shore, and every "CONTROL: none found" PCG step then fails: use a vegetated
   seed such as 670711536 when comparing runs); then a written checklist with one check per fix (real clicks through `Tools/win_input.ps1` wherever a player must
   click; give items to a joiner from the HOST's Python so they replicate). Screenshots go in the scratchpad, numbers in the commit.
   `MO_INST_EXTRA_<NAME>` adds launch flags to one instance of any harness run (`MO_INST_EXTRA_ACTCLIENT=-rhivalidation` on the joiner
   only: N-185). A case that needs a built world must call `_worlds_built` itself: `MO_ACTIONS_ONLY` may run it first after the join.
5. **Package once** only if a fix depends on cooked / Shipping behaviour (`Docs/Packaging.md`).
6. **Commit each verified fix on its own** with its docs; a fix that failed waits for the next batch (no rebuild for one fix).

### Where a batch's time goes, and the speed-ups (the loop deep dive, Oct 22 2026)
Measured on one batch (BR23 batch 2): build 48 s (37 actions, UBA local, adaptive unity); automation 313 s (351 tests); full host + joiner
nettest ~28 min (boot + join ~3 min, the pair READY in 47-50 s against 12-16 s for one instance alone); an editor start for a DataTable
write 2-3 min, and each editor start / stop around instances (an open editor starves an instance's vegetation, TRAPS 5) costs it again;
review shots ~1.5 min a spot (a 75 s settle each).

- **`python Tools/batch_verify.py`** does steps 2-4 in one command and runs the suite DURING the nettest (step 3 always allowed it): the
  suite's ~5 min disappear inside the nettest's ~28. Summary: both verdicts, every failing row, the slowest cases.
- **Per-case wall time:** the actions nettest prints `[nettest] <case>: N s` after each case and `[nettest] slowest cases: ...` at the end.
  The harness holds 213 fixed `time.sleep` calls totalling 702 s (~12 of the 28 minutes): the slowest cases are where to replace a sleep
  by a wait on the thing itself (`wait_log`, a polled probe).
- **Test-world flakes are named, not retried:** a case that depends on world content checks its precondition first (the ground-item E
  case skips a candidate whose line hits a plant; the ground-build case counts cobbles inside the build range), so a FAIL is the game's.
  Cases that lay something in front of a pawn, aim a click, walk straight ahead or need a wolf to reach a pawn stand on clear ground first (`_to_clear_ground`: dry,
  flat, nothing solid within 12 m; `_clear_around_host` calls it for the host; moving the joiner brings the host along), and the gait
  runs and the 150 s long run face the clearest heading (`_face_clear_heading`; the long run turns again when a 10 s sample shows under
  10 m) (Oct 24: the larger rock tiers put boulders round the coastal test site -- 9 cases ran into them, the long run stood 49 m out).
  The test site moves from run to run (the joiner's start differed by 19 m between two passes of one seed), so a case that picks a node or a
  bank has a fallback: the harvest cases skip a node that is used up (a loose-stone node can roll none), out of season or refused as out of
  the joiner's sight; the drink case names what blocks the view of the water and tries the sea's bank. An AI that must reach a pawn (the
  wolf's bite, the colony medic) logs `_nav_between` (both on the navmesh? a full path?), so a miss names the navmesh or the AI (N-205).
- **Real-cursor isolation:** harness instances launch with `-MOHoldContextMenus` (menus neither warp nor close on the real cursor): a
  person at the machine no longer fails a run, and the run no longer moves their mouse.
- **Bridge round trips** (measured Oct 22 on a running instance): `console()` cost 1.8 s, of which ~1 s waited for a `MOQUERY` line
  most commands never print (the bridge already returns only after the command ran): now 0.76 s. `wait_log` polls every 0.25 s (was
  1 s) and asks `tasklist` whether the process lives every 2 s instead of every poll; `ue.py`'s bridge polls its reply every 50 ms
  (was 250). Measured and NOT kept: running `ue.py` in-process instead of a subprocess per call (0.76 s either way: one process start
  is cheap; `ue.py inst do` from a shell spawns two). What is left per call is the game side's 0.25 s command poll and the command.
  `_ground_items_both` confirms only what a case uses (`want`, `keep`): `_trust_instance_far` confirmed all 40 listed items (107 s).
  **Measured, the same full pass (suite + nettest, batch_verify):** 35.9 min -> 27.8 min; the cases' own time 1,986 -> 1,508 s.
- **Next (measured, not built):** two host + joiner pairs in parallel on halves of the case list (128 GB RAM, 32 threads; the second pair
  needs its own listen port and session name) would cut ~12 min; a headless DataTable writer (an `-run=pythonscript` commandlet in place
  of the full editor + bridge) would save the 2-3 min editor start per data change.

## 4. DataTables

- **Live tables are the truth.** Edit them with `python Tools/ue.py rows set <table> --file rows.json` (per-row add + set, readback-verified,
  TMap snapshot guard; `rows get` each row first and add only what is missing). Never call the raw MCP `set_rows` with a partial struct
  (it REPLACES nested structs; `rows_set_safe` deep-merges). After MCP edits: `ue.py refresh-data`. Save after `pie end` (no saves in PIE).
- The `*_JSON.json` / CSV mirrors in `Plugins/MOFramework/Content/Data` have NOT tracked the live tables since early checkpoints (Items.csv is
  in `Data/archive/`). The `.claude/skills/ue-datatable-json` skill edits those JSON mirrors: use it only for a table whose JSON is known current.
- Progression content follows `Docs/PROGRESSION_ROADMAP.md` (the design authority; Wes N6). Verify with `MO.Test.ValidateData` + `MO.Test.ProgressionMap`.
- The CSV + sqlite utility (still valid for tables with a live CSV, e.g. Skills.csv; **Quests.csv went stale Oct 18**: DT_Quests is edited with
  `rows set` since (the Tutorial_BuildControls PlaceGhost objective, the Stick / Bark targets), so never reimport it from the CSV); verbatim from
  CLAUDE.md, with the stale paths marked:



**ALWAYS use `Tools/ue_csv_utils.py` when modifying CSV files!**

See `Tools/UE_CSV_FORMAT.md` for full documentation.

#### IMPORTANT: Schema Changes

**When modifying DataTable row structs (e.g., `FMOItemDefinitionRow`, `FMORecipeDefinitionRow`):**

**ADDING new fields to struct:**
```bash
# 1. Add columns to database with defaults
python Tools/ue_csv_utils.py add-column Tools/recipes.db recipes bIsBuilding False

# 2. Export back to CSV
python Tools/ue_csv_utils.py export Tools/recipes.db Plugins/MOFramework/Content/Data/Recipes.csv [STALE PATH 2026-10-17: Items.csv is in Data/archive/, recipes live in Recipes.uasset] recipes

# 3. Reimport in UE
```

**REMOVING or RENAMING fields:**
```bash
# 1. Use import-safe to preserve manual data (Icon, UI, WorldVisual)
python Tools/ue_csv_utils.py import-safe Items.csv Tools/items.db items

# 2. Export with new schema
python Tools/ue_csv_utils.py export Tools/items.db Items.csv items
```

**Check for drift:**
```bash
python Tools/ue_csv_utils.py check <csv> <db> [table]
```

**Protected fields (preserved automatically by import-safe):**
- `UI` - IconSmall, IconLarge, Tint
- `WorldVisual` - StaticMesh, MaterialOverride, WorldActorClass
- `Icon` - Recipe icons

**Row struct files to watch:**
- `MOItemDefinitionRow.h` → Items.csv → Tools/items.db
- `MORecipeDefinitionRow.h` → Recipes.csv → Tools/recipes.db
- `MOSkillDefinitionRow.h` → Skills.csv

#### Quick Reference

```bash
# Import CSV to SQLite database
python Tools/ue_csv_utils.py import Plugins/MOFramework/Content/Data/Items.csv [STALE PATH 2026-10-17: Items.csv is in Data/archive/, recipes live in Recipes.uasset] items.db

# Query items
python Tools/ue_csv_utils.py query items.db "SELECT ItemId, DisplayName FROM items WHERE Rarity='Rare'"

# Update items
python Tools/ue_csv_utils.py update items.db "UPDATE items SET MaxStackSize=50 WHERE ItemType='Resource'"

# Export back to CSV
python Tools/ue_csv_utils.py export items.db Plugins/MOFramework/Content/Data/Items.csv [STALE PATH 2026-10-17: Items.csv is in Data/archive/, recipes live in Recipes.uasset]
```

#### Critical CSV Rules
1. **Encoding**: Usually `utf-8-sig`, always detect first
2. **Field Size**: Set `csv.field_size_limit(sys.maxsize)`
3. **Quoting**: UE uses `QUOTE_ALL` - every field is quoted
4. **Quote Escaping**: Quotes inside quoted fields are doubled (`""`)
   - Write single quotes in Python → CSV writer doubles them → UE reads as single
   - NEVER manually double quotes or you get `""""` (broken)
5. **Row Name Column**: UE exports the first column (row name / `---`) WITHOUT quotes, but our utility quotes all fields. This can cause crashes when UE reimports.
   - **Workaround**: After modifying data via the utility, make edits in DataTable editor and re-export from UE
   - Complex struct arrays (like `TArray<FMOToolRequirement>`) should be set manually in the DataTable editor, not via CSV

#### Struct Array Format (TArray<FStructType>)
- Empty array: `""` (empty string) or `()`
- Single item: `((Field1=Value1,Field2=Value2))`
- Multiple items: `((Field1=A,Field2=B),(Field1=C,Field2=D))`
- All struct fields should be included with full precision floats (e.g., `1.000000`)
- Example working format for `TArray<FMOToolRequirement>`:
  ```
  ((ToolType="Axe",MinQuality=1.000000,DurabilityConsumed=1,bIsRequired=True,MissingToolTimeMultiplier=1.000000,MissingToolQualityMultiplier=1.000000))
  ```

#### Inspection Field Format (Current)
```
(Grants=((Id="SkillName",bIsKnowledge=False,XPAmount=5.0,MaxLevel=3),(Id="KnowledgeName",bIsKnowledge=True,XPAmount=100.0,MaxLevel=3)))
```

- Data pipeline as written in TECHNICAL_REFERENCE (May 2026): `C++ Row Struct → DataTable (UE Editor) → CSV Export → SQLite DB → Query/Update →
  CSV → Reimport` (`Tools/ue_csv_utils.py`). **[SUPERSEDED 2026-10-17 for Items / Recipes / Biomes / Resources: live `ue.py rows set`; see above.]**
- Format reference: `Tools/UE_CSV_FORMAT.md`.

## 5. Graphify (the code graph)

From CLAUDE.md (verbatim):

This project has a graphify knowledge graph at `graphify-out/` (17k nodes / 26k edges after the 2026-10-17 update; was 11k nodes,
741 code files; AST-extracted, fully local). Rules:
- For architecture / "what connects to what" questions, try the graph before
  grepping: `python -m graphify query "..."`, `explain "Symbol"`,
  `path "A" "B"`, `affected "X"` (run from repo root).
- After a session that modifies code, run `python -m graphify update .` to
  keep it current (AST-only, no LLM, ~1 min).
- `graphify-out/` is gitignored — regenerable, never commit it.
- The CLI has no PATH shim on this machine: use `python -m graphify`.

Codex resolves `python` to anaconda, where graphify is not installed: use the Windows-Store Python
(`C:\Users\penum\AppData\Local\Microsoft\WindowsApps\PythonSoftwareFoundation.Python.3.13_qbz5n2kfra8p0\python.exe -m graphify ...`) or
`pip install -e D:\UEProjects\graphify` into anaconda once (coop CL-0001). Graph size on 2026-10-17: ~16.7k nodes / 25.6k edges.

## 6. Content and art tools

| Tool | Does |
|---|---|
| `Tools/icon_studio.py` (+ `icon_studio_blender.py`), `Tools/icon_fix_process.py` | item icons from meshes; de-checker icons |
| `Tools/kitbash.py` (+ blender), `Content/Python/item_tint.py`, `item_mesh_prepare` | item meshes from scans (`Docs/Item_Art_Pipeline.md`) |
| `Tools/ede_migrate.py` | migrate sample-project groups (`--source ede` Electric Dreams, `--source darkruins` the Dark Ruins Megascans sample): gitignored content; appends .gitignore lines; never overwrites repaired files |
| `Content/Python/fix_mesh_collision.py`, `prep_rock_meshes.py`, `prep_tree_collision.py`, `worldfeature_meshes.py` | collision for scatter / composition meshes |
| `Content/Python/fix_substrate_shading_model_pin.py`, `fix_hornbeam_masters.py` | repair migrated masters so the cook works (re-run after every migration) |
| `Content/Python/fix_megascans_import.py` | reparent Megascans exports onto M_MS_Base |
| `Content/Python/upgrade_terrain_material.py` | build / update M_QuixelBase (pristine-graph revert for structure changes) |
| `Content/Python/biome_palettes.py` + `Docs/Ground_Coverage_2026-10-14.plan.json`, `native_grass_biomes.py`, `Tools/pcg_stocking.py`, `Tools/shore_dressing.py` | DT_Biomes palettes and experiments (`Docs/PCG.md` §5) |
| `Content/Python/cut_pcg_dead_branches.py`, `set_game_radii.py`, `thin_item_clutter.py`, `pcg_tier_caps.py`, `add_tree_tier_chain.py`, `add_far_tree_chain.py`, `tune_native_grass.py` | PCG graph edits from editor Python |
| `Content/Python/measure_feature_ground.py` | measure a composition's authored ground |
| `Content/Python/make_sand_material.py`, `make_alpine_material.py`, `dark_pebbles.py`, `Tools/gen_sand_textures.py`, `Tools/ed_surfaces_prep.py` | voxel surface types |
| `Content/Python/kit_import.py`, `kit_styles.py`, `Tools/kit_bake.py`, `ui_specs/kit_*.py` | the UI kit (`Docs/UI_Kit_Implementation.md`) |
| `Tools/ui_inventory.py` | regenerates `Docs/UI_Screen_Inventory.md` from the project (no editor): every WBP's kit spec, parent, input, focus, users, redirectors |
| `Tools/gen_moodle_icons.py`, `Content/Python/import_moodle_icons.py` | placeholder moodle icons |
| `Tools/steam_scout.py`, `steam_art_build.py`, `steam_wordmark.py`, `Tools/look_sheet.py`, `Tools/biome_eye.py`, `Tools/biome_tour.py` | screenshots and Steam art (`SteamArt/README.md`) |
| `Tools/terrain_lab/` | numpy replica of the world generator (iterate shapes in seconds) |
| `Tools/motion_probe.py capture|track`, `Content/Python/foliage_wind_precision.py`, `Content/Python/pivot_painter_textures.py` | does on-screen motion flow or hold-and-snap: a timestamped frame burst of a game window + sub-pixel patch tracking; per patch the step share and the **jump share** (travel in frames > 4x the median of the 10 frames around them: reads a staircase, not a gust; trust it over the step share on mostly-still foliage), how many jumps fall on hitch frames (frame pacing, not the material), and a plot scaled to fit each swing (foliage wind, `Docs/PCG.md` §9); the Pivot Painter wind precision patch (guard: `MOFramework.Data.FoliageWindPrecision`); the Pivot Painter data texture settings (no mips, never streamed, nearest, linear) |
| `Content/Python/flower_motion_capture.py` (claude_seq), `Tools/flower_sharpness.py` | a bug report's view re-shot still and mid-pan per AA / material setting, and the flowers' (or any crop's, `--all`) edge contrast as a number: does something blur, and only in motion? (BR23-10) |
| `Content/Python/near_rocks.py` (PIE; `NEARROCKS_AT` / `_R` / `_MIN`) | every big rock / cliff instance near a spot: mesh, scale, owner (a PCG partition = a palette species, a `MOWorldFeatureActor` = a composition), size, rotation -- which placement a report means |
| `Content/Python/make_western_cliff_composition.py`, `Tools/western_cliff_rows.py`, `Content/Python/trellis_cliffs_two_sided.py` | the western cliff band POI (graph + row) and the TRELLIS crags' two-sided materials |
| `Tools/import_metahuman_grooms.py` -> `Content/Python/resave_metahuman_grooms.py` -> `Content/Python/make_groom_bindings.py` | the MetaHuman groom library into the project (the creator's hair / beard / mustache styles, `Docs/UI.md` §3): copy the launcher's "MetaHuman Creator Core Data" grooms (only the UE 5.7 install has it here) to `/Game/MetaHumans/Common/Optional/Grooms`, resave them through temporary package redirects (the recipe is in the import script), then one binding per groom on the survivor's face (template head -> face transfer; `GROOMBIND_VERIFY=1` checks the root counts). `Content/MetaHumans` is not in git: a fresh clone re-runs the three |
| `Tools/groom_contact_sheet.py <inst> HairStyle|Beard|Mustache <steps> <out.png>` | a contact sheet of every style through the real creator in a game instance (its picker, then a screenshot per choice) for a by-eye check |
| Animation retarget (editor Python recipe) | `Docs/Water.md` §3 + `Docs/TRAPS.md` §10 (op-stack retargeters, root motion) |

## 7. Packaging, probing, profiling

`RunUAT BuildCookRun` recipe: `Docs/AUTONOMOUS_TOOLING.md` "Packaging a Development build"; system view and cook traps: `Docs/Packaging.md`.
`Tools/pkg_probe.py [--package] [--shipdiag] --cmds ... [--until <regex>] [--keep-log label]` (packaged runs, logs in `Saved/Logs/pkg_probe`);
`Tools/csv_frames.py`, `Tools/insights_export.py` (headless Insights), `Tools/perf_experiments.sh`; `MO.Perf.Tour`, `MO.Perf.Bench`, `MO.Perf.Csv`
(`Docs/Performance.md`). One-package cook: `Docs/Packaging.md` §2.

## 8. Crash and bug reports

`python -I Tools/crash_triage.py <crash-dumps dir> [--since DATE]`, `Tools/ue_crash_bundle.py`, `Tools/bugreport_receiver.py`,
`ue.py nettest bugreport --package`; diagnostics `Tools/pawnvis_probe.py`, `Tools/rescue_probe.py [--hunt N]`, `Tools/weather_probe.py`
(`Docs/Persistence.md` §4). Co-op smoothness: every report carries `Net.*` rows (the last 60 s, `Net.Events`, the server clock); two
machines' logs: `python -I Tools/ue_netdiag.py logs host.log joiner.log` (`Docs/Coop_And_Networking.md` §7).

## 9. Unattended runs (Tools/ov.bat)

Wes (2026-10-09): "there's not much point in doing overnight sessions if I have to click accept every 20 seconds". Rewrite `Tools/ov_task.sh`
(bash, runs from the repo root) with the Write tool, then run exactly `Tools/ov.bat` (never edited; `run_in_background` for long jobs).
`Tools/ov_run.sh` snapshots each task into `Saved/Logs/ov_history/<stamp>.sh` + `.log`. Approving the runner approves whatever the task holds:
keep to the standing rules (`Docs/DECISIONS.md` "Earlier": never Wes's saves, own PIDs only, local / free only).

## 10. Steam patch notes (Wes N9, 2026-10-17)

The old PATCH_NOTES.md is archived (`Docs/Archive/PATCH_NOTES.md`). When Wes asks for Steam patch notes: read the last entry of `Docs/STEAM_RELEASES.md` (date / time / commit of the
last Steam patch), generate the notes from `git log <that commit>..HEAD` (player-facing `feat` / `fix` commits, grouped by system; skip
`docs` / tooling), then append the new patch's date / time / commit to `Docs/STEAM_RELEASES.md` in the same commit.

## 11. Console verbs (289 registered `MO.*` / `mo.*` names on 2026-10-17)

The live, searchable list is the in-game developer menu (`devmenu [Group]`, tooltips = help text; dev mode in Shipping: Options > Gameplay >
Developer or `-devmode`). Namespaces (count): MO.Test (38), MO.UI (23), MO.WorldGen (19), MO.Colony (14), MO.Audio (13), MO.Water (12),
MO.Player (10), MO.Weather (8), MO.AI (7), MO.Mod (7), MO.PCG (7), MO.Clock (6), MO.Light (6), MO.Session (6), MO.Net (5), MO.BugReport (5),
MO.Footstep (5), MO.Save (5), MO.Fire (4), MO.HUD (4), MO.Medical (4), MO.Harvest (3), MO.Item (3), MO.Perf (3), MO.Readiness (3),
MO.Voxel (3), MO.WorldFeatures (3), MO.Gallery (3), MO.Cam (2), MO.Possess (2), MO.WorldSeed (2), and singles (MO.NewGame, MO.Delay,
MO.Cover, MO.Spawn, MO.Survival, MO.Console, MO.Help ...); cvars `mo.Readiness.*` (13), `mo.PCG.*` (8), `mo.Audio.*`, `mo.Lakes.*`,
`mo.Medical.*`, `mo.WorldFeatures.*`, `mo.Combat.MeleeAimAssist`, `mo.Foliage.RigidShadows`, `mo.Interact.Debug`. Each system doc lists its own
verbs in its header. Regenerate the list: `grep -rhoE 'TEXT\("(MO|mo)\.[A-Za-z0-9_.]+"\)' Plugins/MOFramework/Source | sort -u`.
