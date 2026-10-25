# Traps: things that cost a debugging round

> **What this is:** the cross-cutting gotchas of MO57 on this machine, one line each: **symptom** -> cause -> what to do (source).
> Collected on 2026-10-17 from agent memory, CLAUDE.md, the dated handoffs / overnight logs and the system docs. A system doc keeps the
> full story; this file is for the moment something behaves strangely. Add a line when a trap costs you a round; mark a line
> **[SUPERSEDED date: why]** instead of deleting it.
> **Read when:** before touching a subsystem for the first time in a session, and whenever a result makes no sense.
> **Layer-1 subset:** the ten that bite every session are repeated in `CLAUDE.md` § Traps.

## 1. Git, shell and Windows

- **A commit is missing files the build needs / the remote doesn't compile** -> `git add` fails SILENTLY here: the worktree is case-insensitive
  but the index is case-sensitive, and one bad pathspec in `git add a b c` stages NOTHING -> before EVERY commit run `git status --short` and
  check the staged column; never pipe `git add` stderr to /dev/null; copy paths from `git status`; after a push `git show HEAD --stat`. (memory git-staging-verification)
- **Codex's work lands in your commit** -> `git add -A` in the shared tree -> stage your exact paths only. (multi-agent-coop)
- **A partly staged file lands with a block in the wrong function** -> `git diff -U0` + `git apply --cached --unidiff-zero` of SOME hunks places a pure insertion by its new-side line number, which counts the hunks you left out (Oct 20: the gait warm-up landed inside `_client_carcass`; caught before the push, amended) -> to commit part of a file, build its blob from the base with the chosen changes (difflib on lines, anchored to the base) and `git hash-object -w` + `git update-index --cacheinfo`; then read the staged file (`git show :path`) where the change should be.
- **Stray assets / MegaMaterialCache in the staged list** -> the editor's source control auto-stages every asset it saves and rewrites
  `Content/Developers/penum/MegaMaterialCache.uasset` -> `git rm --cached` strays; never stage MegaMaterialCache or `.claude/settings.local.json`. (animation-retarget)
- **"2 insertions" on a binary asset** -> that is the git-lfs POINTER (oid + size) -> `git lfs status`, not a red flag. (dev_status)
- **`/MOFramework/...` or `/Game/...` arrives as `C:/Program Files/Git/...`** -> Git Bash path conversion -> `MSYS_NO_PATHCONV=1` (ue.py repairs its own args). (many)
- **Quotes vanish from native args** -> PowerShell 5.1 -> run `ue.py` from bash; PowerShell recipes are the fallback (`Docs/TOOLING.md`). (ue-unified-cli; Wes N3)
- **UTF-8 mojibake in a file** -> content round-tripped through PowerShell `Get-Content` / `Set-Content` -> never write file content through
  PowerShell; Write/Edit tools, Git Bash or sed. Reading a log tail is fine. (global rules)
- **A script with apostrophes, `'''`, backslashes or `\U...` paths breaks in a bash heredoc** -> the tool's shell quoting -> write the file
  with the Write tool, then run it. (cs399, swimming, UI_Kit; confirmed 2026-10-17)
- **Windows-Store python can't read `AppData\Roaming` / writes `/tmp` to `C:\tmp`** -> use explicit Windows paths. (cs399)
- **`-ExecCmds="a;b"` runs nothing after `;`; `Quit` doesn't exit an editor-cmd** -> commands split on COMMAS: `py x.py,QUIT_EDITOR`; cvar values
  with spaces can't go through -ExecCmds. (overnight-polish, swimming, oceanology)
- **WinError 206 from ue.py** -> request bodies over ~30 KB overran the command line -> fixed (`_curl` sends the body on stdin). (overnight-polish)
- **A background probe keeps running and teleports the shared instance** -> launched with `&` inside the Bash tool -> use `run_in_background`. (oct16)

## 2. Engine, build and C++ (UE 5.8.3)

- **The package build fails on a test that the editor build compiled** -> the GAME target compiles the automation tests too
  (`WITH_DEV_AUTOMATION_TESTS`), without `WITH_EDITOR`; `ue.py build` builds only the editor -> a test that calls editor-only code
  (an `#if WITH_EDITOR` function: `FMOWorldFeatureCook::CollectCookPackages`) goes inside `#if WITH_EDITOR` itself. (Oct 25)
- **Engine upgrade: a new stock network channel would silently vanish** -> `Config/DefaultEngine.ini` replaces the whole
  `[/Script/Engine.NetDriver]` ChannelDefinitions list (`!ClearArray`) to swap the Actor channel for `MONetStatsActorChannel`. On every engine
  upgrade, re-diff it against the new `BaseEngine.ini` list. Matched UE 5.8.3's four channels exactly on Oct 17. (Oct 17 review of afb9aabc)
- **An engine `.cpp` edit changes nothing** -> `D:\UnrealEngine\UE_5.8` is the LAUNCHER install (`Engine/Build/InstalledBuild.txt`) -> engine fixes
  are staged in `Docs/Engine_Patches.md` + `Docs/EnginePatches/`; check InstalledBuild.txt before proposing one. (engine-installed-build-patches)
- **Build fails in ~5 s: `CS0117 ... V7` / RulesError** -> the UE_5.7 UBT -> use the UE_5.8 UBT (`Docs/TOOLING.md` § Build) or `ue.py build` / `cycle`. (build_engine_path)
- **"Unable to build while Live Coding is active" / LNK1104** -> the editor (or a crashed editor's CrashReportClientEditor window) holds the
  DLL -> `ue.py editor stop` (graceful: QUIT_EDITOR, refuses with unsaved packages); close a crash reporter by PID. Check the DLL mtime: Live
  Coding sometimes hot-patched instead. (user_preferences, Handoff_2026-10-13) Crashed `-game` INSTANCES leave one too: every instance starts a
  `CrashReportClientEditor -RespawnedInstance -MONITOR=<its pid>` that stays open after a crash (nine piled up on Oct 21, one holding
  UnrealEditor-Voxel.dll) -> after crash-heavy test runs list them (`Get-CimInstance Win32_Process -Filter "Name='CrashReportClientEditor.exe'"`,
  start times = the instance launches) and stop those PIDs.
- **A one-off build or shader-compile failure** -> may be the downclocked 14900K -> retry once before debugging. (user_preferences)
- **Build breaks right after committing new files** -> adaptive unity moves committed files into unity blobs and exposes duplicate
  anonymous-namespace names -> `python Tools/check_unity_collisions.py` before committing C++. (swimming, world-features)
- **Tables are rowless / gates collapse after an editor restart** -> a module carve moved DT-row / enum / class types without CoreRedirects
  (class paths drop the U prefix) -> add redirects BEFORE the commit, restart, run a table-touching gate. (dev_status)
- **Your `SetBase` override is never called** -> 5.8 changed it to `FMovementBaseInterfaceData*` -> override the new signature. (CLAUDE 5.8 notes)
- **Packaging dies on a fatal ensure** -> a subsystem `Initialize()` called `GetSubsystem()` for a sibling absent in the cook world -> bind in `OnWorldBeginPlay`. (CLAUDE 5.8 notes)
- **Delegates declared in a header aren't found elsewhere** -> the header has no USTRUCT/UCLASS/UENUM, so UHT skips it -> add one. (CLAUDE)
- **Template compile errors on a forward-declared type** -> templates need the full include. (CLAUDE)
- **A downcast of a plain C++ interface pointer fails** -> no RTTI in UE -> iterate the concrete actor type. (dev_status)
- **A default-constructed `FMOWorldGenParamsRef` kills the editor at first use** -> a TSharedRef default is FATAL -> `FMOWorldGenParamsProvider::MakeDefault()`. (world-features)
- **Editor and packaged builds disagree on `MO.WorldGen.Checksum`** -> Editor targets compile /fp:precise, Game /fp:fast -> `FPSemantics = Precise`
  in `MOFrameworkCore.Build.cs`; rerun the checksum in BOTH after any build-rule or toolchain change. (caves-and-lighting)
- 5.8 API changes met: `GAverageFPS` is gone; `LexToString(EBuildConfiguration)` replaced `EBuildConfigurations::ToString`; `UEditableTextBox::SetWidgetStyle`
  keeps a pointer to the caller's local (`Docs/UI.md` §4); `UGameInstanceSubsystem` is ClassWithin UGameInstance (tests need that outer). (crash-endpoint, ue58)

## 3. Editor, Python bridge and MCP

- **`unreal.Rotator(0, 0, yaw)` does not set the yaw in 5.8 editor Python** -> spawned items came out rolled over and physics
  dropped them upside down -> always pass keywords: `unreal.Rotator(pitch=0, yaw=y, roll=0)`. (Oct 17)
- **`HitResult` has no attributes in 5.8 Python** (`.blocking_hit`, `.impact_point` raise) -> `hit.to_tuple()`: [0] blocking,
  [5] impact point. **`Rotator` has no `rotate_vector`** -> `rot.quaternion().rotate_vector(v)`. (Oct 17)
- **An imported asset is named after the source file's internal names, and renaming from editor Python hangs the editor** -> fix
  names before import (`Tools/glb_item_prep_blender.py` does it for GLBs; `Docs/Item_Art_Pipeline.md` section 8). (Oct 17)
- **A "Server" RPC called in a test runs on the client** -> editor Python (and any console command it dispatches synchronously) runs EVERY RPC
  LOCAL (`GAllowActorScriptExecutionInEditor`) -> test verbs that send RPCs defer a tick (RunOnNextTick); assert on the logs the GAME writes. (coop-pawn-lifecycle, dev_status)
  It includes RPCs fired as a SIDE EFFECT on the host: an `inflict_damage` from `_probe(host, ...)` played the wolf's flinch on the host
  and never sent its multicast (Oct 21, three rounds blamed on the network; the proof is `log LogNet VeryVerbose`: "GetFunctionCallspace
  ScriptExecutionInEditor: MulticastAnimCue" where a sent one says "Multicast" then LogNetTraffic "Sent RPC") -> `MO.Test.HitCreature`.
- **Editor stops answering after an asset delete / rename from Python** -> `ForceDeleteObjects` ensure -> CrashReportClientEditor -> never
  delete or rename assets in editor Python; delete files on disk with the editor CLOSED. (animation-retarget, swimming)
- **The bridge never comes up after a crash** -> the editor sits on the package-restore dialog -> `ue.py editor stop`, move `Saved/Autosaves`
  aside, start again. (far-trees, Handoff)
- **"bridge not responding"** -> (a) the editor is building Nanite / textures (15-30 min after new ED assets), (b) a Python exception inside
  `ue.py py --file` (no result file) -> small steps with `unreal.log("TAG")`, or try/except, (c) first launch after a rebuild -> `ue.py editor wait` then `boot`. (overnight-polish, animation-retarget, ue-ui-toolset)
- **A long `ue.py py` call that times out is still running** -> the editor took it and finishes it; a re-run queues behind it and runs
  right after (Oct 25 2026: `fix_mesh_collision.py` took 33 s against the 20 s default, the re-run's second pass crashed the editor in a
  mesh save, `ThumbnailHelpers.cpp:595`, and the unsaved level went with it) -> `bridge_run` now exits 3 "STILL RUNNING" when the call was
  picked up; never re-run on it; give long scripts `--timeout 300`. (dark-ruins cliffs)
- **A call hangs while the bridge still answers** -> a modal dialog (reload_packages on a dirty asset, `fill_data_table_from_json_string` with a
  missing field) -> pass every field; Windows Security (pickerhost) blocking clicks: post Enter to the dialog's HWND. (dev_status, world-features)
- **Asset saves silently don't happen** -> the editor refuses saves during PIE (save_assets still answers True); a running `inst` locks the
  .uasset files it loaded -> `ue.py` refuses saves while PIE runs; save after `pie end`; stop the instance first. (AUTONOMOUS_TOOLING, overnight-polish)
  `rows set` reads back the editor's MEMORY, so a refused save still verified: since Oct 25 2026 its status line says "VERIFIED n/m --
  NOT SAVED" and it exits 1 (BronzeHelmet01's WornTransform, written in PIE, never reached disk; the commit guard caught it).
- **`save_loaded_asset` skips your change** -> it skips clean packages and a CDO change doesn't dirty them -> `only_if_is_dirty=False`; `importlib.reload(mo_ui)` after editing it. (UI_Kit)
- **GUID / key matching silently finds nobody** -> `str()` of a wrapped unreal struct is the wrapper ADDRESS -> `to_string()` / `export_text()`. (dev_status)
- **Python names don't match C++** -> Python strips the E prefix of enums and the b prefix of bools; `unreal.MaterialLibrary`; a b-UPROPERTY shadows a
  same-named method; a bool UFUNCTION with an out param returns None when false; no `actor.is_hidden()` (use `get_editor_property('hidden')`);
  `unreal.Rotator(a,b,c)` is (roll, pitch, yaw); `line_trace_single` returns None on a miss. (dev_status, oct16, world-features, Handoff)
- **Python can't read a property** -> `EditDefaultsOnly` / BindWidget-only / BlueprintReadOnly C++ props, UDS BP variables with spaces ->
  call UFUNCTIONs, attribute access or a C++ accessor; `get_editor_property('Apply Interior Adjustments')` with the exact name; UDS "Time of Day" via `MO.Weather.SkyTime`. (ue-multiprocess, carried-light)
- **Struct-array edits don't stick** -> elements read from `get_editor_property` are COPIES -> rebuild the structs and set the whole array. (Handoff)
- **`MaterialEditingLibrary.delete_material_expression` asserts `!IsRooted`; `StaticMeshDescription().create_polygon_group()` null-derefs** -> don't;
  import meshes as OBJ via AssetImportTask; structural material changes go through the pristine-graph revert (`Docs/Terrain_Generation_Exploration.md` header). (far-trees)
- **Editor Python has no PIL**; env vars don't reach `ue.py py` scripts; `inst do NAME py` returns only `out(...)`, never `print`. (terrain-v5, pcg-load, oct16)
- **The MCP "died"** -> almost always external (editor closed for a build) -> `ue.py status` re-starts it through the bridge; drive it with curl
  when the harness client is disconnected (Python urllib gets an EMPTY body for `tools/call`). (ue-editor-mcp)
- **The editor stops ticking after an asset script in PIE** -> `EditorAssetLibrary` refuses every call during play (the log's "The Editor is currently in a play mode"), so `does_asset_exist` says no and `create_asset` opens a modal "Overwrite Existing Object" dialog per call, which blocks the game thread (Oct 24, `item_tint.make` on existing MIs) -> end PIE before any create / save (`item_tint.make` now refuses in PIE). To clear dialogs already open: post WM_CLOSE to the editor's "Overwrite Existing Object" windows (cancel keeps the old asset). Spawning in PIE from Python: no deferred spawn or `add_component_by_class` binding; the engine cheat `summon StaticMeshActor` works, then set its mesh.
- **An FBX export of a Nanite mesh is a coarse stand-in** -> `StaticMeshExporterFBX` writes render LOD0, which for a Nanite mesh is its
  fallback (BronzeHelmet01: 2,257 of 24,999 triangles; the thin shell's inside poked through its outside in the icon) -> export a transient
  non-Nanite copy built from `get_static_mesh_description(0)` (`icon_studio_export._full_detail`). (Oct 24)
- **`duplicate_asset` returns None onto a path you deleted on disk** (log: "pathspec ... did not match any files") -> the editor's git source
  control still tracks the deleted file and refuses the new one -> commit the deletion first (`git rm --cached <uasset>`, commit), then rerun
  the build script. (Oct 24, BP_Campfire_StoneRing rebuild)
- **A recipe places the wrong actor (a plain `MOContainerActor`) after an asset rebuild** -> the DataTable loaded while its referenced Blueprint
  was missing and keeps a null class in memory, even once the BP is back -> re-set the row (`ue.py rows set` with the class path) and check
  the readback. (Oct 24, BuildCampfire)
- **PIE wedges (can't begin_play)** -> unroot-caused flake after back-to-back sequences -> restart the editor. (dev_status)
- **`MaterialEditingLibrary.set_material_instance_scalar_parameter_value` returned False and the value WAS set** (a parameter the instance
  did not override yet) -> never chain sets with `and`; set each, then read back (`get_material_instance_scalar_parameter_value`). (Oct 25)
- **A flag passed through `os.environ` to an editor script sticks** -> the editor's Python is ONE process: a dry-run flag set for one
  `ue.py py` call is still set for the next, which then silently dry-runs too -> a script consumes its flag (`os.environ.pop`). (Oct 25)
- **A second `ue.py py --file` "just to read the output" runs the script AGAIN** -> the bridge queues every request; killing the client
  cancels nothing -> read the first run's output (or MO57.log), never re-send. (Oct 25: a harmless double substrate-pin pass)
- **Two editors / a stray UE 5.7 window appear** -> computer-use `open_application "Unreal Editor"` -> never use it; `request_access(["UnrealEditor.exe"])`, foreground with SetForegroundWindow. (ue_pie_computer_testing)
- **Clicks on editor UI leak keystrokes into the game** -> PIE holds CapturePermanently + LockOnCapture -> Shift+F1 first; stop PIE with Shift+F1 + Stop, never Escape. (ue_pie_computer_testing)
- **Injected keys don't trigger Enhanced Input actions in PIE** -> computer-use / AHK keys miss Enhanced Input -> drive in-process (`MO.Test.PressKey` / `MO.Test.Input`). (ue_pie_computer_testing)

## 4. DataTables

- **Authored rows overwrite someone's live data** -> written from the stale `*_JSON.json` / CSV mirrors -> `ue.py rows get` the LIVE table
  first, add only what is missing; for shared data post the rows in the coop log first (M21 lost Codex's in-editor lore). (multi-agent-coop)
- **A row write empties TMap columns in EVERY row** -> the MCP re-imports the whole table and the export nests `TMap<FName,FString>` values ->
  `ue.py rows set` snapshots and re-sends maps and fails if one comes back empty; recover from the HEAD LFS blob (`git show HEAD:<asset> | git lfs smudge`). (ue-unified-cli, world-features)
- **`add_rows` adds nothing** -> batch add is ALL-OR-NOTHING (one existing name rejects it); a large multi-row `set_rows` fails SILENTLY ->
  one row at a time + readback; `ue.py rows set` bulk (> 20 rows) silently wrote nothing with a new row in the batch (fixed Oct 17): verify nested arrays by readback anyway. (ue-editor-mcp, wes-calls-oct17)
- **Omitted fields reset to defaults** -> MCP `set_rows` REPLACES whole nested structs -> `rows_set_safe` deep-merges; never call raw set_rows with a partial struct. (CLAUDE)
- **A retried MCP call reports "Rows do not exist"** -> the first attempt (empty response) already applied -> read back. (AUTONOMOUS_TOOLING)
- **The game doesn't see new rows** -> static caches -> `ue.py refresh-data`. (ue-editor-mcp)
- **A new row-struct field won't write** -> only after the C++ rebuild; JSON keys lowerCamel for row fields, PascalCase inside nested entries. (biome-aware)
- **A material "exists" in analysis but not in the world** -> only species- or spawner-placed resource rows exist -> read `unplaced_resources` first. (progression-pass1)
- **CSV round-trips crash UE on reimport** -> UE exports the row-name column unquoted, the utility quotes all -> `Docs/TOOLING.md` § 4 (DataTables), `Tools/UE_CSV_FORMAT.md`. (CLAUDE)

## 5. Harness: ue.py, instances, nettest, tests

- **One driver per instance** (Oct 22): the bridge is a command FILE each call appends to (`ue_cmd.txt`). Two processes sending to one
  instance at once (a probe from a shell while `review_shots.py` drives it) can interleave their lines: a `HighResShot` never ran and the
  shot was lost. While a tool drives an instance, start your own (`ue.py inst start <other name>`) instead of sending to its.
  A driver script started with `&` from an agent's shell is NOT stopped when the shell returns (Oct 23): it lived on untracked, its instance
  "already running" refused the rerun, and once the rerun started both drove the instance and fought over one screenshot. Start drivers as
  tracked background tasks; to find a stray one, its command line (`Get-CimInstance Win32_Process`), and stop it by that PID.
- **`MO.Test.HitTest` says BLOCKED by `WBP_LoadingOverlay` right after `MO.Test.State` reports `inGame=YES`** -> the overlay holds until
  readiness is READY (tens of seconds after the pawn is possessed) -> wait for `[MOReadiness] READY` in the log before hit-tests and clicks. (Oct 17)
- **`MO.Save.LoadFrom <slot>` typed at the main menu loads the save INTO the menu level** (it reloads in place, no travel) -> to load
  into gameplay from the menu: `ke * LoadGame <slot>` (`AMOMainMenuPlayerController::LoadGame`, the menu's own path). (Oct 17)
- **A `-game` instance near the spawn has no vegetation** -> the editor was open when it started -> `ue.py editor stop` before any visual / PCG / harvest instance test. (ue-multiprocess)
- **`-game` of the editor binary crashes** -> no GEditor: `EditorLevelLibrary`, Voxel `WITH_EDITOR` code -> console verbs (`MO.Spawn.Try`); the Voxel GEditor guard covers the real map for `nettest game|hostsave|soak`. (ue-multiprocess, dev_status)
- **A `-game` instance dies on an asset edit sent with `inst do NAME py`** (access violation reading 0x2780: `EditorAssetLibrary.duplicate_asset`,
  `MaterialEditingLibrary.update_material_instance`; windhost twice, Oct 18, and the crash reporter pops up on Wes's screen) -> edit assets in
  the editor, then restart the instance; in -game read assets (`load_asset`, `get_material_function_expressions`) or change runtime objects.
  `ue.py py` now refuses those calls (save / duplicate / delete / rename / create asset, every MaterialEditingLibrary setter, update and
  recompile) when its bridge is an instance's. (Oct 18)
- **Instance screenshots lie** -> the window defaults to 640x360 (minimum DPI scale) -> `r.setres 1920x1080w` first; HighResShot shows no UMG (use `ui shot`). (first-ten-minutes, ue-ui-toolset)
- **"PASS" from `MO.Test.ClickWidget` / a simulated click** -> SimulateClick ignores hit-testing; for non-MO widgets it logs SENT -> read the state it should change; do one real click (`MO.Test.HitTest`). (first-ten-minutes, ue-ui-toolset)
- **Wait for the world in an instance** -> the SECOND `[MOReadiness] READY` line after Start (the first is the menu world). (first-ten-minutes)
- **A "LAN" test ran on Steam** -> argparse REMAINDER swallowed a sibling's options -> real subparsers; never reuse `p` in main(). (ue-multiprocess)
- **A crashed host's log is gone** -> the next start overwrites game.log -> `Saved/Crashes/UECC-*/game.log`. (ue-multiprocess)
- **`ue.py seq` says FAILED with no reason** -> read `inst_<name>/ue_out.txt`; bridge probe echo lines `[py-ok]/[py-err]` contain your marker: exclude them. (ue-multiprocess, ue_pie)
- **Log-delta verbs read the wrong log** -> a restarted editor writes `MO57_2.log` -> ue.py follows the freshest `MO57*.log`. (ue-multiprocess)
- **Long chains die** -> the background timeout (> 10 min) -> check for orphaned `UnrealEditor.exe`, `ue.py inst stop <name>`; stop only what you launched (pid file). (ue-multiprocess)
- **A test kit can't take items ("cannot hold")** -> the kit fills the inventory -> trim stacks; RunAll's DropPickup needs a fresh game. (progression-pass1, playtest-oct14)
- **Fast jobs are never seen as "current"** -> they start and finish between polls -> assert on evidence (stock deltas). (dev_status)
- **A teleported test pawn is shoved away** -> it landed inside a building's hull -> stand 2.5 m out. (dev_status)
- **A job + its quota slot wedge forever** -> `SetStayAtLocation` suspends job processing -> `AbortCurrentJob()` first. Un-recruited survivors don't run job move-legs -> FOUND + RECRUIT first. (dev_status, terraform)
- **Crafting gates can't be sped up with TimeScale** -> the queue ticks on real UtcNow -> read state immediately after resume. (dev_status)
- **Frame-dependent UI test fails synchronously** -> CommonUI reconciles next tick (F21) -> frame-stepped tests. (dev_status)
- **An intermittent failure moves with the seed** -> a world-dependent bug -> replay the seed (`MO.NewGame <seed>`). After movement / spawn / water changes run ALL nettest gates. (death-recap)
- **The harness instance is silent** -> instances are `-nosound` and `-NoCharacterCreator` by default -> `--sound`, `--creator`. (oct16)
- **`ue.py status` says editor DOWN while Codex is using it** -> sandboxed status can't see the protected process -> the coop log decides who holds the editor. (multi-agent-coop)
- **An editor save fails ("save_assets -> False"; the editor log says `MoveFile was unable to move ... .uasset`)** -> another process had
  the file open at that moment (Oct 18, DT_SurvivorJobs, while another session's two `-game` instances were starting; the holder wasn't
  identified) -> the edit stays in memory; retry `ue.py save <asset>` (it succeeded 80 s later with the same instances running) and check
  `git status` for the .uasset. `save_assets` now says so and lists any running instances.
- **"editor is RUNNING" / `editor start` refused while no editor was open** -> `editor_running()` counted the `-game` instances (same
  UnrealEditor.exe) -> it now reads command lines; `ue.py build` names the instances holding the DLLs. (Oct 18)

- **A scripted co-op test step "did nothing" (a hitch on a jogging joiner gave 0 corrections, 12 of 12 trials)** -> every `ue.py inst do` / `console`
  call takes ~1.5 s, so the `MO.Test.MoveFor 0 1 7` had ended before the hitch landed -> make the move outlast the chain of calls and record
  the state the step needs right before it (the pawn's speed); a trial whose precondition is not met does not count. (Oct 17)
- **A list row (a recipe in Hand Crafting) will not select from `MO.Test.ClickWidget`, and `-game` Python has no `WidgetBlueprintLibrary`, `SubsystemBlueprintLibrary` or `GameInstance.get_subsystem`** -> reach widgets and subsystems with `unreal.ObjectIterator(unreal.<Class>)`: `[l for l in unreal.ObjectIterator(unreal.MORecipeListWidget) if l.is_visible()][0].select_recipe('SelectClub')` (the list's own selection, what a click reaches), `unreal.ObjectIterator(unreal.MOPersistenceSubsystem)`. A real click (`win_input.ps1 click`) also works when Wes is away. (Oct 20)
- **`MO.Item.Menu` logs "opened" but no menu is there (a joiner, a background instance)** -> the context menu auto-closes when the REAL cursor is outside it for 0.2 s (`[ContextMenu] Auto-closing - mouse outside`), and the cursor is over another window -> drive the action itself (`MO.Test.Inspect <ItemId>`, `MO.Item.Do`), or keep that instance's window under the cursor. A resource menu needs the CAMERA within ~5 m of the target (the interaction trace starts at the shoulder camera): stand ~3 m away or you get the Forage menu. (Oct 20)
- **Smoothness measured on two local processes says little about Steam** -> loopback has no internet latency, loss or jitter, and both
  processes share one CPU / GPU (frame times and the round trip are frame-bound: ~35 ms rtt on loopback) -> `nettest netdiag` validates the
  instruments; real numbers come from a Steam session's two logs (`Tools/ue_netdiag.py logs`, `Docs/Coop_And_Networking.md` §7). (Wes, Oct 17)
- **A stopped background test batch keeps launching runs; a run's verbs land twice, "bridge not responding", 20+ unrelated failures** -> stopping
  a background Bash task (TaskStop) does not end a script's process tree on Windows: the bash script and its `ue.py nettest` lived on and drove
  the same `acthost` / `actclient` as the next batch (Oct 21: `[MOTEST] PickupNearest` logged twice in one frame) -> after stopping one, list
  the leftovers by command line (`Get-CimInstance Win32_Process` where CommandLine matches `nettest|<script name>`), stop those PIDs, then
  `ue.py inst stop <name>`. (Oct 21)
- **An instance never gets past "Waiting for ZenServer to be ready..." (the pair's boot step fails)** -> no Zen server was running (it exits
  with the process that launched it, `--owner-pid`: closing the editor or a crashed instance takes it down) and both instances brought it up
  at once -> `ue_inst.start()` now waits for the first instance's "ZenServer AutoLaunch initialization completed" (~16 s) before starting the
  next. (Oct 21)
- **`-ini:Engine:[ConsoleVariables]:r.ShaderCompiler.JobCacheDDC=0` on a game instance (full material shader maps, like a cooked build)** -> both
  instances assert compiling M_MS_Surface_Material (MaterialShader.cpp:2567, "missing expected shader type FAnisotropyPS": its cached
  conservative parameters disagree with the compile) -> don't; on-demand compile is the only working mode for the uncooked game here. (Oct 21)

## 6. Co-op and RPC

- **A joiner's own death showed nothing on the joiner** -> the death ran server-side only (anatomy events fire on the server) -> a
  replicated record + one "enter dead state" routine run by the server and by every client's OnRep (`FMODeathRecord`, Oct 18). Any
  state change a player must SEE on their own machine needs a replicated trigger, not a server-side broadcast.
- **A per-pawn delegate binding (the death recap) silently stopped firing on a joiner** -> a possession settle unpossessed and re-possessed
  the pawn and the client got no pawn-changed callback to rebind -> listen to a global event (`AMOCharacter::OnAnyPawnDied`) and filter.
- **A Client RPC "reached" the host instead of the joiner in a test** -> it was sent from inside an editor-Python call (trap 2 in §3:
  such RPCs run locally) -> drive it from a console verb that defers a tick (`MO.Test.SurvivorCommand`, `MO.Fire.Do`).
- **A joiner's UI keeps an item, job or condition the server already removed ("Simple Cordage (2/2)" with 0 carried)** -> FastArray
  `PostReplicatedAdd` / `PostReplicatedChange` / `PreReplicatedRemove` run while removed entries are still in the array, and there is no
  array-level `PostReplicatedRemove` (ours never ran) -> whole-list notifications go in `PostReplicatedReceive`, once per receive
  (`TECHNICAL_REFERENCE.md` Replication Patterns). A UI test on the host can't see this: check on a joiner. (Oct 18, BR18-31)
- Full list: `Docs/Coop_And_Networking.md` §6. The ones that bite: editor-Python RPCs run local (§3 above); **PIE can't test sessions** (use
  `ue.py inst` / `nettest`); **Steam is off in editor / PIE by design**; one Steam account can't join its own lobby; **the FIRST
  `NetDriverDefinitions` entry wins and a missing driver class falls back to IpNetDriver silently** (assert the driver class); the Null OSS
  answers only `bIsLANMatch=true`; **sessions outlive worlds** (`ReleaseStaleSession`); **a multi trace BY CHANNEL stops at the first blocking
  hit** (trees hid the terrain: use object-type traces); **a client predicts gravity for a simulated proxy with no floor** (never apply it);
  **vacuous checks** ("client z == server z" with no client terrain) need a negative control (`--skew-seed`); a UI request on the server's copy of
  a joiner's controller opens nowhere (`ForwardUIContractRequest`); dropped items need ECC_PhysicsBody in object queries.
- **`bSimGravityDisabled` on a simulated proxy zeroes its velocity** -> the engine's `SimulateMovement` treats that flag as "stuck" (no
  extrapolation; anything reading velocity sees a standing pawn). Set it only when the proxy has no floor on this machine
  (`AMOCharacter::OnUpdateSimulatedPosition`, N-160 fixed Oct 19); probe `MO.Test.RemotePawns`. (Oct 17 / 19)
- **`MO.Player.Face <x> <y> <z>` lands off target the first time** -> the shoulder camera swings around the pawn after the turn, so the eye
  the aim was computed from moves; repeat it 2-3 times (1 s apart) before a right-click / E that must hit a small target (a ghost, a stone).
  (Oct 19)
- **A translucent material on a Nanite mesh draws the engine's default grey** ("Invalid material ... used on Nanite static mesh") -> force
  the non-Nanite fallback while it is on (`SetForceDisableNanite`, as the building ghost does); a Nanite mesh whose material lacks "Used with
  Nanite" draws grey in -game / cooked builds only (the editor fixes it on the fly). (Oct 19)
- **A context popup in a BACKGROUND -game window closes itself in 0.15 s** -> `UMOGroundContextMenu` auto-closes when the real cursor is
  off it, and a window that isn't in front never gets the cursor warp: bring it to the front with the cursor on the popup
  (`win_input.ps1 -Action move`) before testing anything that needs the popup open; `MOUI_LOG` lines are compiled out of these builds, so
  prove a popup is open or closed with `MO.Test.FindWidget <its button text>`. (Oct 19)
- **A joiner hitch of 250 ms or more while moving = a rubber band** -> the host extrapolates a joiner that sends no moves
  (`AGameNetworkManager::MAXCLIENTUPDATEINTERVAL` 0.25) and corrects it when it comes back: 500 ms hitch at 450 cm/s -> 124 cm, 3/3 ->
  `set GameNetworkManager MAXCLIENTUPDATEINTERVAL 0` on the host leaves 42 cm (`p.NetServerMaxMoveDeltaTimeScalar` 1.75); fork F25. (Oct 17)
- **A joiner far from the host is corrected on every move by tens of cm, host higher** -> the voxel world has two collision layers (render-chunk
  "visibility" collision whose detail follows THIS machine's camera, and fine invoker chunks), both WorldStatic, and a pawn stands on the
  TOPMOST: 1 km from the host's camera the coarse one was up to ~1 m above the ground -> every machine refines the terrain around every other
  player (`AMOCharacter::RemotePlayerTerrainDetail`). (Oct 18)
- **A voxel invoker / LOD volume / camera invoker added at runtime from Python does nothing** (no effect, no cost) -> build it into the actor (a
  default subobject, toggled through `bEnabled`, which the plugin re-reads every tick). Three negative experiments came from this. (Oct 18)
- **Lining up two machines' logs by the replicated server clock is off by seconds** -> `AGameStateBase::GetServerWorldTimeSeconds` is WORLD
  time, which stops at 0.4 s a frame (`AWorldSettings::MaxUndilatedFrameTime`): a hitching host lost 3.3 s of it against the real clock and the
  joiner's copy sat 3-6 s apart -> line logs up by wall clock (`utc=` in every `[MONet*]` line, clocks synced). (Oct 17)
- **A protected engine setting cannot be set from Python** (`GameNetworkManager.MAXCLIENTUPDATEINTERVAL`: "protected and cannot be read") ->
  the `set <Class> <Property> <value>` / `get <Class> <Property>` console commands write / read the class default live (development builds).
- **Co-op shows no actors at all** -> the actor channel class in `[/Script/Engine.NetDriver]` ChannelDefinitions (`Config/DefaultEngine.ini`,
  `MONetStatsActorChannel`) failed to load -> every `[MONet] net driver ...` log line names the actor channel class; check it first.
- **A joiner's request about a PCG instance (a stick, a bush, a tree) arrives as null** -> PCG partition actors and their instanced-mesh
  components are spawned at runtime under names that differ per machine (two processes on one machine too), and indices shift: the host logs
  "Unable to resolve default guid from client: ObjectName: PCGPartitionActor_..." -> never put a PCG component pointer or an instance index in an
  RPC; send `FMOInstanceRef` (mesh + world location) and resolve each machine's own copy. (Oct 18)
- **Gameplay run from a widget's timer works for the host only** -> the harvest and the inspection ran in UI timers on the clicking machine: a
  joiner's inventory refused every yield ("Inventory Full!") and what a joiner learned lived only in its own copy -> server action timed on the
  pawn (`UMOTimedActionComponent`), result back by client RPC (`Docs/Coop_And_Networking.md` §5). Grep a new action for its widget timer. (Oct 18)
- **A forward to the owning client must never be reachable from that client RPC's own body** -> where an RPC runs locally (the dev bridge's
  Python, a controller without a connection) forward -> RPC -> forward recursed and the host died of a stack overflow with no crash dump
  (`MO.Test.GrantRecipe`) -> the RPC body calls a forward-free "Here" function (`UMONotificationComponent::ShowNotificationHere`). (Oct 18)

## 7. Packaging, cook and Shipping

- Full list: `Docs/Packaging.md` §2. In short: cook-world `GetSubsystem` fatal; non-`ECVF_Scalability` cvars in DefaultScalability.ini are
  fatal; Substrate legacy shading-model pins (`fix_substrate_shading_model_pin.py` after every ED / Megascans migration); soft-referenced plugin
  content not cooked (`DirectoriesToAlwaysCook`) but too-wide dirs cook DX11-incompatible Niagara (X3662); SLW materials need the legacy output
  node; the MCP listener in the cook child (`bAutoStartServer=False`); the Update-BuildInfo two-writer race; `DefaultGameTarget=MO57`.
- **Shipping writes no log** -> package Development for diagnosis; `MO57ShipDiag`; bug-report fields. Shipping reads Wes's REAL
  `%LOCALAPPDATA%/MO57/Saved` settings -> back up byte for byte, restore + compare. The staged `MO57.exe` is a launcher (stop the tree);
  packaged CSV profiles are wiped by the next `--package`; `-log` opens a console over the game.
- **A packaged build has NO world-feature cliffs** -> the rows name their graphs / PCG data assets in TEXT (ActorProperties), which the
  cooker cannot follow (a real `TSoftClassPtr` it does follow) -> `FMOWorldFeatureCook` adds what the rows reach on the cook's
  ModifyCook delegate (`Docs/Packaging.md` § 2). Any new system that loads content from a path in a string needs the same. (Oct 25)

## 8. Voxel and world generator

- **"Cannot query positions here"** -> `OutputHeight.Bounds` / `.HeightRange` are evaluated with no position -> feed them from the no-input `MO Terrain Bounds`. (terrain-gen-exploration)
- **Terrain misaligned** -> stamp transform not identity / parameter not named exactly `Seed`. Editing an MO node's pins orphans links in saved graphs (break + re-add). (terrain-gen-exploration)
- **Whole terrain is the grey WorldGrid checker** -> the graph's SurfaceType pin is unwired. **A new surface type never paints** -> it isn't in the mega material's `surface_types`. (caves-and-lighting, terrain-v5)
- **Far mountains show a checkerboard / a 12 km tear** -> chunk HOLES from a world-spanning volume stamp -> every volume stamp gets tight bounds AND a LOD range (cave stamp `LODRange {0, CaveMaxLOD}`). (volume-stamp-holes)
- **Caves don't match the ground** -> only the height stamp was seeded -> seed both (`ApplySeedToStamps`). **Meshing artefacts in caves** -> a hard gate in the field -> continuous penalties; test max |gradient|. (caves-and-lighting)
- **"INVALID STAMP"** -> a stray VoxelStampActor with no stamp in the level. (caves-and-lighting)
- **A voxel metadata the material samples reads 0 everywhere** -> no stamp writes it, or the asset has no Guid (one made with
  `create_asset` and no factory hit an ensure in `FVoxelMetadataSingleton::RegisterFromAssetData`) -> give it one
  (`unreal.GuidLibrary.new_guid`) and save. A stamp-level metadata override is a constant blended by the stamp's alpha; a per-position
  value needs the graph's Output metadata pin, and voxel graphs cannot be edited from Python (the node classes are private). (Oct 19)
- **Skinny-tower mountains / walls at patch borders** -> every wavelength in `FMOWorldGenTuning` is in CENTIMETRES -> check units first. (terrain-v6)
- **A coarse LOD is slow** -> a per-cell search cheap at LOD 0 runs per sample at coarse LODs -> LOD-cull every generator feature by its sample step. (pcg-load)
- **A new default breaks the v1 golden anchor** -> `RunProbe(false)` must restore it. Any geometry change bumps `CurrentGeneratorVersion` + re-pins. (worldgen doc)
- **Flatness / relief tests measure borders** -> keep a test inside one climate class; a biome hill scale < 1 must also flatten the continent spline. (terrain-v5)
- **No cave mouths found** -> a mouth is a few metres wide -> scan at 4 m; field tests run with karst OFF. (caves-and-lighting, geology)
- **The camera sees "water in a cave"** -> it is inside the rock, seeing the ocean plane under all terrain -> probe air with `MO.WorldGen.CaveSample`. After a terrain change place cameras from the pawn's REAL z. (geology, playtest-oct14)
- **A pawn swims inside terrain below sea level** -> the ocean plane fills everything under z = 0 -> `BuriedMarginCm` + `GetGroundZ`. (death-recap)
- **Lakes 80,000x too big / "invalid Role" on clients** -> SpawnActor with a level actor as Template composes its transform and copies its Role -> spawn a plain actor of the class. (worldgen v4)

## 9. PCG

- Full rules: `Docs/PCG.md` §2-§7. The traps: biome-blind native chains make every biome look the same; the Voxel Sampler places at most one
  point per CellSize cell (`CellSize <= ~70/sqrt(rho)`); radius fields are named by the UNSCALED grid and are PROTECTED (reflection +
  `ComputeHash()`); the clump field isn't uniform (pre-be6506f7 palette numbers are wrong); `biome_palettes.py` has drifted from the live
  DT_Biomes; the scheduler builds the LARGEST grid first; FName lookups per point cost 13-161 us; a disabled node passes its input through;
  a HiGenGridSize node with no edges stops ALL generation; `InstanceMinDrawDistance` needs `InstanceEndCullDistance > 0`; raising PCG throttles
  makes loading slower; change detection skips cells for a still camera; item clutter shares `VoxelSampler_0` with the ground tier (thin the
  ratio, never the sampler); PCG builds plain ISM components (iterate `UInstancedStaticMeshComponent`); a raw `ISM->RemoveInstance` regrows
  (`FMOHISMHarvestHelper::RemoveInstance` only); `MO.Save.LoadFrom` reloads in place; PCG graph assets can't be edited during PIE.
- **A far-field artefact** -> find what it is made of with `MO.PCG.MeshSurvey` / `HideMesh` / `HideTag` BEFORE theorising (an hour was lost
  blaming the biome tiers for 90k native Field_05 trees). Census by radius (`MO.PCG.Census`), never component bounds. (far-trees, biome-aware)
- **"Zero PCG in game"** -> the coastal spawn on bare sand -> LOOK at the player's view first. (shipping-diag)

## 10. World features and content import

- **A Blueprint composition ignores its seed / sweep** -> its PCG component exists only after FinishSpawning -> bind and set after it;
  `CleanupLocal` before regenerating (GenerateLocal(force) ADDS instances). Editor-world PCG doesn't generate compositions: measure in PIE. (oct16, world-features)
- **A white orb in a POI** -> ED PCGAsset elements bake editor markers (/Engine/BasicShapes) -> the feature subsystem hides them. (oct16)
- **Players fall through rocks** -> migrated ED meshes have no collision / few hulls at 4x -> `fix_mesh_collision.py` (composition rocks
  complex-as-simple; re-run after `ede_migrate`: ED content is gitignored). Collision counts read -1 during PIE: audit outside PIE. (world-features, overnight-polish)
- **New ED folders get committed** -> `ede_migrate` only prints them -> add them to .gitignore BY HAND. (overnight-polish)
- **ED ForestGround / ForestTerrain look wrong as ground** -> they are mesh-UV atlases, not tileable surfaces. (overnight-polish)
- **Megascans MIs have no parent** -> library exports point at `/Game/MSPresets/M_MS_Default_Material` -> `fix_megascans_import.py` (reparent onto M_MS_Base). (terrain-v5)
- **A dark scan tinted pale comes out black** -> a tint MULTIPLIES -> start from a pale scan; a tint can't make metal (metallic lives in the ORM). (geology, fab-sourcing)
- **Fab panel imports eat keystrokes / re-import existing assets** -> check the front window first; right-column cards ignore clicks; check the folder id before wiring. (fab-sourcing)
- **A hull on a plant mesh blocks the player** -> check it isn't world foliage (referencers) before `item_mesh_prepare`. (fab-sourcing)
- **Icon studio crashes on HDR textures** -> an explicit exporter skips SupportsObject -> let UE pick by extension. (geology)
- **Retargeted clips move only the root / slide** -> a 5.8 IKRetargeter is an op stack (add_default_ops + auto_map_chains); root motion folds into the pelvis (zero the root on copies). `get_raw_track_data` is empty in 5.8. (animation-retarget)
- **New MO content disappears from git** -> `Plugins/MOFramework/Content/Characters/MetaHumans/` and `Content/PCG/` are gitignored. (swimming, package-cook-errors)
- **A case-only rename returns False** -> FName is case-insensitive -> go through a temporary name. (footsteps, animation-retarget)

## 11. Water

- Full list: `Docs/Water.md` §2. Oceanology's infinite-ocean box has no physics state (underwater look never applies) -> query-only body;
  lake planes are world-axis-aligned quad trees (never rotate, never scale big); Oceanology actors replicate by default (SetReplicates(false)
  for per-machine lakes); runtime-spawned lakes never register their wave solver (register + activate); presets overwrite the distortion cap
  every load (re-enforce on a tick); absorption = how far light CARRIES (bigger = clearer).
- **A lake setting changed after `FinishSpawning` never reaches the water** -> `AOceanologyLake::Init` runs from OnConstruction AND
  BeginPlay, both inside FinishSpawning, and pushes the waves / surface to the material there -> set things before FinishSpawning, or push
  again after (`UpdateWaves()` for the Gerstner waves). Measure the MATERIAL (its MID's Amplitude_N), not the solver. (Oct 25)

## 12. Light, rendering, materials

- **A tint that reads well on the icon can look too dark in game** -> the icon studio renders under studio light, while in-game sun
  on low-roughness metal mostly reflects the surroundings -> judge a tint in PIE: material instance edits show live there (saves are
  refused, so save after ending PIE). Steel needed Brightness 2.8 in game against 1.6 on the icon. (Oct 17)
- **A test light renders dark** -> UE point-light default units are UNITLESS -> `SetIntensityUnits(Candelas)` BEFORE `SetIntensity`; changing
  `intensity_units` on a live light doesn't refresh the proxy. (carried-light)
- **Caves glow** -> UDS Captured Scene Sky Light casts no shadow -> UDS interior adjustments. **A torch blows out white** -> the histogram meters
  the black majority -> the eye-adaptation floor. (carried-light)
- **Cave walls lit from below** -> a light below the horizon lights one-sided voxel surfaces; two-sided materials don't help Nanite shadows ->
  the intensity gate; `bAffectsWorld` can't change at runtime and UDS overwrites `SetVisibility`. (caves-and-lighting)
- **Rows of snow patches / a far checker on slopes** -> one axis-aligned noise read repeating; detail normal maps at range -> rotated reads,
  fade detail normals with distance; run the shadows-off control before blaming shadows. ddx / ddy return 0 in the Nanite custom node. (terrain-material-far-tiling)
- **Beach sand over the whole coastal plain** -> a fixed 40 m height band -> band = 8 % of the edge's height above sea. (terrain-material-far-tiling)
- **A weather number 10x too big** -> `BP_WeatherBridge` hands Ultra Dynamic Weather's 0-10 values -> read the state only through
  `UMOWeatherIntegrationSubsystem::GetCurrentWeatherState` (scales by `ProviderIntensityScale`). **UDW's material wetness / snow ignore a
  changed duration at runtime** (cached at BeginPlay) -> the ground's state is MO's (`FMOGroundWeather`, `Docs/Weather_And_Time.md` §2a).
  **UDW BP variables read through Python (`Rain`, `Material Wetness`) stay at their defaults while the sky rains** -> read the parameter
  collection (`unreal.MaterialLibrary.get_scalar_parameter_value`) or `MO.Weather.Info`. (Oct 19)
- **A UI test passes without the key it names** -> the Escape / Tab helpers closed the menus through the UI manager whatever the key did ->
  since N-128 such a pass reads UNVERIFIED; run `MO.UI.RunAllTests` in a `-game` process, and wait for `AssertAnyMenuActive` before a key
  (sent on the open frame it does nothing). (Oct 19)
- **Everyone reads as indoors / sheltered** -> a trace from an actor's location starts inside its own query-only interaction sphere
  (blocks Visibility for the crosshair) and hits it at 0 cm -> weather shelter goes through `IsWeatherShelterHit` (solids only, not the
  start shape, not pawns). Any new "is X over / around this point" trace: skip query-only volumes and start-penetrating hits. (N-175, Oct 19)
- **Clouds blocky in a 4K shot** -> `r.VolumetricRenderTarget 0`; the sky only moves FORWARD in game time; `MO.Weather.Director 0` + `SetPreset Clear_Skies` for stable shots. (steam-store-art)
- **"STREAMING POOL OVER BUDGET"** -> Epic texture pool 1000 MB -> 4000 (Cine 6000) + LimitPoolSizeToVRAM. (overnight-polish)
- **Trees freeze, then snap, every 0.4-1.6 s, only far from the origin** -> a wind graph adds time to a WORLD-space term (a Pivot Painter
  pivot) in float: 5-15 km out the sum moves a whole texel at a time -> reduce the world term mod a period first
  (`foliage_wind_precision.py`, test `MOFramework.Data.FoliageWindPrecision`); graphs that scale it down first are fine. (`Docs/PCG.md` §9, Oct 18)
- **Foliage within ~1.6 m of the camera reads as jumping in `motion_probe` (and looks noisy with AA off)** -> the camera fade dithers every
  leaf 40-160 cm from the lens with a pattern that changes each frame -> measure foliage from >= 2.5 m (narrow the FOV instead). (Oct 18)
- **A harness instance dies in `Nanite::BasePass` right after content first renders (executing a heap address ending a24e00; "Invalid uniform
  buffer <any type> bound on ComputeShader at index 3")** -> an uncooked `-game` compiles material shader maps on demand; each arrival swaps
  the material's shader map and uniform buffer while a Nanite shading pipeline of a primitive the engine did not re-create still points at
  the old ones (N-185, `Docs/Engine_Patches.md` #2) -> `MOMaterialKeepAlive.cpp` keeps the replaced ones alive in uncooked game processes
  (`mo.Debug.MaterialKeepAlive`, default on: keep it on); a cooked build never swaps shader maps. The same race can hit PIE in the editor
  (not covered). (Oct 21)
- **Foliage smeared even with the camera still** -> wrong motion vectors: motion blur and TSR both read them. Test it: `r.AntiAliasingMethod 0`
  + `r.MotionBlurQuality 0` -- sharp = the velocity is wrong, not the AA. The PN flowers' MA_Flowers does it under Nanite (its wind): they stay
  classic (`flower_meshes_non_nanite.py`). A material's WPO that is fine classic can be wrong under Nanite: compare both before turning a pack Nanite. (Oct 25)
- **Ground cover reads dotted / broken while walking** -> the near-camera fade's temporal screen door, which TSR averages only when the camera
  holds still -> ground cover has its own narrow band (`foliage_camera_fade.py` GROUND_COVER_BAND). (Oct 25)
- **Grass glows, pale, self-lit, shadows don't show on the blades** -> Megascans Two Sided Foliage `Translucency Controls*` G = strength,
  B = exponent: shipped at 5 x the base colour with a square root, the transmitted light out-shines the lit side -> cap strength <= 0.5,
  exponent >= 1 (`Content/Python/fix_foliage_transmission.py`, rerun after importing plants; test FoliageTransmissionCapped). (Oct 25)

## 13. UI (CommonUI / UMG)

- Full list with fixes: `Docs/UI.md` §2-§5. The ones that bite: `ECommonInputMode::All` leaks WASD (Menu mode everywhere); a pushed widget is
  pending until its transition ends (`LayerTransitionSeconds` 0; never `RemoveWidget` a pending entry); a direct `SetInputMode` desyncs the
  router (`ApplyGameplayInput`); the router adopts a root only once PAINTED (`AssertInputConfig`); a native UserWidget class used as a NODE is
  click-through; `SetPositionInViewport` resets anchors; `NativeConstruct` re-runs on every re-add (read authored state in
  `NativeOnInitialized`); `SetIsSelected(false)` does nothing on a non-toggleable button; the HUD learns about a menu on activation
  (`NativeOnActivated` refresh); queries need a PAINTED PIE viewport; hex colours are sRGB but UMG wants linear; fonts only through `UFont`;
  BindWidget name mismatches show as `absent` in `ui check`; the engine console owns Tilde and Enhanced Input removed `BindKey` (`InputKey`).
- **A row's buttons take no real click** -> the entry blueprint lays a full-row button (EntryButton) over them in an overlay; it takes every click and
  nothing handled it (the possession window, Oct 18) -> `MO.Test.HitTest <Button>` names what a click really lands on; bind the row button or
  make it HitTestInvisible. A fix proven by an RPC or `SimulateClick` proves nothing about this. (Oct 18)
- **A pooled CommonUI widget builds its rows again on every showing** -> `PushWidgetToLayer` reuses instances and `NativeConstruct` re-runs,
  so code that adds children in it adds them again on top of the old ones (the status panel listed the vitals once per showing, the old rows
  bound to pawns played before) -> clear the containers first, or build once in `NativeOnInitialized`. (Oct 18)

## 14. Audio

- **Audio faults leave no log** -> `LogAudio=Warning` hides Display lines and the engine reports nothing -> the `[MOAudio] HB` heartbeat.
  **Muting doesn't fix a NaN mix** (NaN x 0 = NaN) -> bypass submix effects / stop components to bisect. **Full volume after a map load** ->
  `UEngine::LoadMap` resets the device volume -> re-apply. **A debug file logger costs ms** -> default off, gated, compiled out in Shipping. (audio-heartbeat, post-reveal)
- **A source effect never runs on a sound (its counters stay 0)** -> a source effect chain is NOT instantiated on a source with more channels than
  its presets support, and `USoundEffectSourcePreset::GetMaxSupportedChannels` defaults to 2: UDW's multichannel weather sounds ran unguarded ->
  override it (the NaN guard says 8). **An audio test buzzes on Wes's speakers** -> a harness instance takes focus when it opens, so the
  unfocused mute does not apply -> `-DeterministicAudio` (MO_INST_EXTRA_<NAME>): the non-realtime renderer mixes everything (the heartbeat
  reads it) and plays nothing. (Oct 22)
- **A guard in a submix's effect chain leaks / the mix goes silent through the reverb** -> a submix's DRY path carries its input around
  its effect chain, and the default -96 dB is 1.6e-5 linear, live -> close the dry path of every submix you guard (`CloseDryPath` in
  MOAudioNaNGuard.cpp: master and reverb). **Fatal "Trying to modify UObject map (FindOrAdd) that is currently being iterated"** ->
  something made a UObject inside `ForEachObjectOfClass` (a NewObject, or a Play that makes one) -> gather the objects first, act after
  the iteration. (Oct 22)
- **NaN / 1e24 samples from a sound whose own audio is clean** -> the engine's downmix of a MULTICHANNEL source into the reverb send
  (`au.AllowReverbForMultichannelSources`, default 1) made them for UDW's weather beds -> the project sets it to 0 (DefaultEngine.ini);
  a new multichannel sound gets no reverb send. (Oct 22)
- **Every pawn's footsteps as loud as your own** -> `PlaySoundAtLocation` with no attenuation plays an asset that has none of its own at full
  volume wherever it is -> `UMOAudioSubsystem::PlayOneShotAtLocation` gives such assets a profile (footsteps: `GetFootstepAttenuation`). (Oct 18)
- **A volume slider misses some sounds** -> (a) a sound with no class plays in the project DEFAULT class (was the engine Master; now SC_FX);
  (b) a plugin's own class (UDS_Weather, Oceanology's) sits outside SC_Master -> list it for its slider in `UMOAudioSettings`; (c) the
  volume lives on the LOADED class and an unreferenced class is garbage-collected and reloads at its authored volume -> hold it
  (`ApplySliderVolumes`). Test EverySoundOnASlider. (Oct 25)

## 15. Performance measurement

- **Hitch counters look normal** -> game / render / GPU counters lag the frame-time measurement by one frame -> log a ring of frames;
  `-hitchdetection` is compiled out. **Benchmarks swing** -> an editor is running (shared GPU) or stale far cells are loaded -> no editor,
  compare within one session, three runs. **PIE fps comparisons** -> PIE is frame-capped (~58) -> compare `gpu=` / `draws=`. **A `-game`
  instance runs the hardware benchmark** -> GIsEditor false -> `-NoQualityBenchmark`. **A low-tier cvar leaks into Epic** -> scalability writes
  only the applied level's cvars -> pin at all four levels. **`UMOGameSettings` never applied engine settings** -> it skipped
  `Super::ApplySettings` -> `ApplyNonResolutionSettings`. (hitch-hunt, far-trees, playtest-oct14, footsteps)
- **Periodic code costs a constant hitch** -> it searches the world (`TObjectIterator`, `GetAllActorsOfClass`) or recomputes a pure function each
  tick -> keep a registry; cache; throttle by movement; move pure work to a worker. (TECHNICAL_REFERENCE "Periodic work")

## 16. AI and gameplay

- **One survivor order froze the host for minutes** -> `UMOPCGInteractionSubsystem::GetItemIdForMesh` logged a Warning plus the whole
  mesh-to-item cache (~130 lines) for every unmapped mesh; a Gather Wood search over 200 m met 122 of them inside `EnqueueJob` (2 min 44 s) ->
  a miss is normal and logs one Verbose line (commented out since Oct 18 with every other Verbose line: TECHNICAL_REFERENCE "Code
  Conventions"). Never dump a table per miss in a lookup that runs over every instance. (Oct 18)
- **An overlap query never returns a PCG instance** -> `OverlapMultiByChannel(ECC_Visibility)` around the aim point came back with the terrain
  only, a stick 2 cm away (the near-the-aim-point pass, Oct 18): a small item flush with the ground was never a target -> find instances by
  their components' bounds (`TObjectIterator<UInstancedStaticMeshComponent>`, world + bounds filter) and `GetInstancesOverlappingSphere`, as
  `UMOInteractorComponent::FindInteractableNearAimPoint` and `FMOInstanceRef::Resolve` do. A line or sweep trace still hits them. (Oct 18)
- **Creatures are blind** -> a perception component with no senses at OnRegister is never registered -> `RequestStimuliListenerUpdate()` before
  `ConfigureSense`. **"IsInAttackRange does not exist"** -> blackboard values pushed before the BT bound the blackboard -> prime in Tick. (post-reveal, caves-and-lighting)
- **Creatures are deaf** -> the hearing sense filters by raw team ids, the engine's default solver calls NoTeam vs NoTeam Friendly, and every
  MO57 pawn is NoTeam: with `bDetectFriendlies` off no footstep was ever heard (sight checks per actor and worked) -> every creature sense
  uses `AMOCreatureController::SenseAllAffiliations()`; threats are decided in `OnTargetPerceptionUpdated`. (Oct 18)
- **A number in combat is 100x what the code says** -> a Blueprint component template overrides the C++ default (BP_MOMetaHuman_Female's
  `UnarmedAttackProfile.BaseDamage` = 500, a Feb debug value: one punch killed a deer) -> read the pawn BP's own template (a test can walk
  its `SimpleConstructionScript`, as `MOFramework.Combat.Damage.BareHandCannotDestroyVitalPart` does). (Oct 18)
- **A held spear fights like a fist** -> nothing linked the right-hand item to the combat profile, and `GetFinalDamage` multiplied by
  ArmorPenetration on unarmored bodies -> `UMOCombatComponent::SyncMainHandFromEquipment`; penetration only acts through armor. (Oct 18)
- **A spawn hitches on soft assets** -> `LoadSynchronous` at spawn -> declare in `GatherRuntimeAssets`. **Fallback deer under the sea** -> `MOSpawnClearance`. (post-reveal)
- **Survivor behaviour trees don't exist** -> survivors are `AMOSurvivorController`'s C++ state machine. (dev_status)
- **Ore mined with no pick** -> the tool gate lived only in the menu -> `HasToolForAction` in `BeginHarvest` (one gate). (progression-pass1)
- **A "stuck" craft queue** -> craft time = base / average tool effectiveness (a hammerstone makes 300 s into 1000 s). (progression-pass1)

## 17. Verification habits (the lessons behind many lines above)

`Docs/ENGINEERING_PRINCIPLES.md` "Verification lessons": LOOK before theorising; a control must be able to fail; rerun the probe on the fixed
build and read both machines; audit your own fixes; re-verify stale tracker claims; measure before redesigning; identify the exact asset first.

## Superseded (kept for search)

- **[SUPERSEDED 2026-10-11: fixed in EditorSettings.ini]** "PIE runs at ~3 fps when the editor window isn't foreground" -> `bThrottleCPUWhenNotForeground=False`,
  `bAllowSlateThrottling=False` in `%LOCALAPPDATA%\UnrealEngine\5.8\Saved\Config\WindowsEditor\EditorSettings.ini` (verified 2026-10-17).
- **[SUPERSEDED 2026-10-14: ue.py now quits gracefully]** "`ue.py editor stop` kills by image name -> leave with QUIT_EDITOR": `editor_stop` runs QUIT_EDITOR
  through the bridge, never force-kills, and refuses while packages are unsaved.
- **[SUPERSEDED 2026-10-11: Oceanology replaced Epic Water]** "The Water plugin's underwater post process only runs on a BLOCKING hit on its trace channel" (dedicated
  `WaterTrace` channel): dead config now.
- **[SUPERSEDED 2026-10-05: ue.py ui]** "Python cannot set Is Variable / rename widgets": `MOWidgetEditorUtils` exposes both.
- **[SUPERSEDED 2026-07-04]** "Cook failed with a clean log" from the MCP port: fixed by `bAutoStartServer=False` (kept in §7 as the cause to remember).
