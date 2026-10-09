# MO57 Autonomous Tooling — Operating Manual

Goal: a toolset that can build *and verify* the game with minimal human input.
There are three "hands" (write) and one "eyes" (verify). The whole game of
autonomy is closing the loop between them.

## Unified CLI — `Tools/ue.py` (START HERE, 2026-07-02)

One entry point wraps all three layers. Prefer it over hand-rolled
`Add-Content; Start-Sleep; Get-Content -Tail` loops and per-script MCP clients —
it bakes in every transport lesson below.

| Command | Does | Notes |
|---------|------|-------|
| `python Tools/ue.py status` | editor / bridge / MCP / PIE state in one shot | exit 2 if env down |
| `ue.py run "MO.Test.X" [--grep MOTEST]` | console cmd via bridge, **correlated** | prints THIS call's output + the MO57.log delta |
| `ue.py py -c 'out(...)'` / `ue.py py --file s.py` | editor python; `--file` supports **multi-line scripts** (wrapped in exec) | exit 1 on py-err |
| `ue.py seq test.py` | claude_seq multi-frame sequence, waits for DONE | exit 1 on fail/timeout |
| `ue.py boot [--seed N] [--name S]` | menu→in-game (wraps agent_boot_newgame.ps1) | ~40 s |
| `ue.py pie begin\|end` | PIE lifecycle | |
| `ue.py test [--suite RunAll\|ValidateData]` | runs suite, waits for + prints `Saved/MOTestResults.txt` | exit code = pass/fail → CI-able |
| `ue.py auto [--filter MOFramework]` | **headless automation tests** (UnrealEditor-Cmd -unattended -nullrhi), parses report JSON | editor must be CLOSED; 91 tests ≈ 26 s; exit code = pass/fail |
| `ue.py mptest` | **2-client co-op PIE smoke** (`Content/Python/test_multiplayer.py` via claude_seq) | configures 2-player listen-server PIE (`UMOEditorTestHelper::ConfigurePIE`), targets host/client worlds by net mode, ALWAYS restores 1-player settings after |
| `ue.py rows list\|get\|set TABLE [--file rows.json]` | DataTable verbs; `set` is **one-row-at-a-time + readback verify + save** | avoids the silent batch failures |
| `ue.py mcp dt\|asset TOOL --args '{...}'` | raw MCP call, session cached, fail-fast curl | |
| `ue.py refresh-data` | invalidate item+recipe static caches after MCP edits | needed before PIE sees new rows |
| `ue.py build` | UBT 5.8 (refuses if editor running) | |
| `ue.py editor start\|stop\|wait` | lifecycle incl. wait-for-bridge | |
| `ue.py cycle [--boot --seed N] [--test]` | **the whole compile-verify loop as one command**: close → build → relaunch → wait bridge → boot → RunAll | |
| `ue.py ui build\|check\|dump\|contract\|scaffold\|menu\|click\|find\|shot\|stop ...` | **spec-driven Widget Blueprint toolset** — UI is data: spec file → idempotent build → C++ contract check → PIE click/find/shot | **no Designer clicking, no screen control**; guide: `Docs/UI_TOOLING.md` |

Correlation design: every bridge call is bracketed with begin/end markers in
`ue_out.txt`, so output is attributed to *the* command — plus the game-log
delta from the command's start offset (`--grep` to filter). No more guessing
which tail lines were yours. Shell note: quote args from bash/git-bash;
PowerShell 5.1 mangles embedded quotes in native args.

### Multi-process / online-session testing (2026-10-05) — `ue.py inst` + `ue.py nettest`

PIE puts every "client" in ONE process, so it can never exercise the online-subsystem path (create / find / join a
session). `Tools/ue_inst.py` launches extra standalone game processes (`UnrealEditor.exe <proj> -game`), each with its
own command bridge + log (`MO57_BRIDGE_DIR`, `MO57_GAMELOG`), and drives them with the same `ue.py` verbs.

| Command | Does |
|---------|------|
| `ue.py inst start NAME [--nosteam] [--pos X,Y] [--res WxH]` | launch an instance, wait for bridge + world |
| `ue.py inst do NAME run "MO.Session.Status"` | run any `ue.py` command against it (`py`, `run`, ...) |
| `ue.py inst stop NAME` / `inst list` | graceful `quit`, then terminate by recorded pid only |
| `ue.py nettest lan` | **host + client, Null OSS (`-NoSteam`), LAN beacon**: host → find → join → both see 2 players. PASS/FAIL + exit code |
| `ue.py nettest steam-host` | one instance on **real Steam**: init, license, lobby create, listen-server travel, asserts the Steam net driver |
| `ue.py nettest game` | **the REAL gameplay map, the way a player hits it**: Host-button path, late join, first-join pawn spawn, joiner control setup, **world seed handoff + ground-height comparison (settle time series, 3 spots, 11x11 grid)**, a WALKING client vs the server's view of it, a remote pawn 150 m from the host, leave -> pawn stays idle -> rejoin reuses it, the possession menu's list + possess requests for a remote client, and the first tutorial popup. Add `--keep` to leave both windows up for hands-on checks |
| `ue.py nettest game --skew-seed` / `--withhold-seed` | **negative controls**: the host publishes seed+1 (client terrain must differ: ~147 m) / publishes nothing (client must apply nothing). A check that has never been seen failing proves nothing |
| `ue.py nettest hostsave` | the Load panel's **Host** button end to end: fresh world + recruited colonist -> save -> menu -> `MO.Session.HostSave` -> client joins; asserts the save loads, the host gets its pawn back, the saved seed is what is published, terrain matches, the joiner takes a saved colonist. Deletes its `zz_nettest_hostsave` slot afterwards |
| `ue.py nettest game --withhold-sync` | **negative control for clock/weather sync**: the host stops publishing; the client's clock AND the sky's own time of day must differ from the host's and the client must run no MO weather sync (the weather LABEL still follows: see traps) |
| `ue.py nettest actions` | **gameplay verbs from a real CLIENT process**, each proved on the HOST with a control: pickup returns the SAME item GUID (control: not in the inventory while on the ground), craft (control: refused without ingredients/skill; after the host's `MO.Test.GrantRecipe 1 <recipe>` the joiner's queue grows), attack puts the joiner's pawn in combat. **Trust boundary** (a client must not be able to make the server do what the player cannot): `ServerPlaceBuilding` near (accepted) vs 200 m away (refused; control `MO.Building.ServerReach.Disable 1` lets the far one through, so the refusal is the check's doing), `ServerPickUpWorldItem` for an item 45 m away (`MO.Test.PickupNearest <radius> <minDistance>`; server reach 30 m), possessing the host's pawn (refused, both players stay put), `ServerApplyTerraform` 3 m ahead (changes the ground: control) vs 100 m away (refused; reach 25 m). Open (logged, not fixed): no collision/slope/rate limit on placement, no server-side timer or tool requirement on terraform |
| `ue.py nettest packaged [--package] [--withhold-sync]` | **two copies of the PACKAGED Development game** (host via `-ExecCmds="MO.Session.Host ..."`, client by address), judged from their logs alone: no crash/ensure, same seed, the client follows the host's clock and weather, only the host's spawn manager spawns. The editor-binary `-game` processes are not the shipped game: this found a startup crash (intro timer), client-side creature spawning and a blank save thumbnail by hand. `--package` re-packages first (~90 s) |
| `ue.py nettest bugreport [--package]` | **the bug report upload with the packaged game and a local stand-in for the crash endpoint** (`Tools/bugreport_receiver.py`): (1) accepted: host a world, `MO.BugReport.SendTest -shot` -> the receiver holds a valid CR1 bundle marked as a bug report with the state fields, the log tail, a JPEG and **no user/computer name anywhere**; (2) the endpoint answers 500 -> NOT SENT, the bundle is kept in `Saved/BugReports` and parses; (3) nothing listens -> same fallback with a message that says where the file is. Judging is a pure function (`analyse_bugreport_upload`, unit-tested with a case that must fail per check) |
| `ue.py nettest churn [--rounds N]` | host + one client on the real map: N (default 3) leave/rejoin cycles (the host keeps the leaver's pawn as an idle colonist and hands the SAME pawn back, no second pawn, two players each with a pawn), races for one dropped item between the host's pawn and the joiner's (equidistant, both fire `MO.Test.PickupNearest` at once, skew logged: exactly one may end up holding it; a trial only counts when both targeted the same item), a save while connected (`MO.Save.SaveAs zz_nettest_churn`, deleted afterwards), and over the whole run on BOTH machines: no fall-through rescue, no crash/ensure, no Error from the game's code. The second racer is the host's own pawn: a third editor-binary process does not fit in 24 GB next to the desktop |
| `ue.py nettest soak --rounds N` | repeats fresh-random-seed host+join N times with the same two processes and compares the ground each time (intermittent bugs need a rate, not one run) |
| `ue.py nettest features` | one real-map instance, fresh hosted world: a new game starts at 08:00 under `Clear_Skies` (polled until the transition finishes), `starter` adds 50 sticks + 20 stones (inventory counted), the Tilde popup opens on a real Slate key event and does not stack, `MO.Console.Run` strips a leading tilde and reports a result, the engine console no longer owns Tilde |

Facts learned by running it (each was a silent failure before):
- **Never fire an RPC from editor-Python, or from a console command that runs synchronously under it.** The editor script
  guard (`GAllowActorScriptExecutionInEditor`) makes `AActor::GetFunctionCallspace` return LOCAL for every function, so
  a "Server" RPC called from a client just runs on the client (it even looks like it worked: logs appear, on the wrong
  machine). `MOCheatSubsystem::RunOnNextTick` exists for exactly this. Use `MO.Possess.List` / `MO.Possess.Take <guid>`
  (deferred a tick) or the real UI. This cost a long detour once: Python-driven "RPCs never reach the server".
- **`GAllowActorScriptExecutionInEditor` also makes server-only calls misleading from Python in general**: prefer
  asserting on the log lines the GAME writes (host and client logs) over values read back through Python.
- **A real keypress/click into a `-game` window (computer-use)** is the ground truth for UI paths. `request_access` for
  `UnrealEditor.exe`; the first click may not bring the window forward (the tool reports another app as frontmost) -
  click inside the window once more, then send the key. The grant also covers the user's editor: click only inside the
  test window.
- **Client-side "fall-through rescue" teleported other players' pawns under the terrain**: `AMOCharacter::CheckFallThroughSafety`
  ran on every machine; a client has voxel collision only near its own camera, so it saw other pawns as falling with
  no terrain and teleported them to `SafetyTeleportHeight` (Z=200), under the real surface, every ~2.5 s (237 times in
  ~6 min, 0 on the host). Authority-only now. The server-side fallback (no terrain anywhere -> teleport to Z=200) still
  exists and can bury a pawn when the real cause is terrain not generated yet: seen once on the HOST's own pawn
  (47 teleports in one run, 1 in the next). Open item: freeze the pawn instead of teleporting.
- **OnPossess/OnUnPossess run only on the server.** A remote client learns its pawn through replication
  (OnRep_Pawn -> `AController::SetPawn`), so pawn-control setup (`CachedControllablePawn`, UI pawn caches) must also run
  from `AMOPlayerController::SetPawn` on the owning client, or the joiner cannot control the pawn he sees.
- **`Possess()` is authority-only**: the possession menu used to call it on the client (silent no-op). It now asks the
  server (`AMOPlayerController::ServerPossessPawnByGuid`), whose answer is built from the SERVER's pawns.
- **The engine destroys a leaving player's pawn** (`APlayerController::PawnLeavingGame`). `AMOPlayerController` overrides
  it so a recruited colonist stays as an idle AI-run pawn, and joiners reuse an available pawn before spawning one.
- **Tutorial popup order was hash-order.** `GetActiveTutorialHint` returned the first active quest in `TMap` iteration
  order and ignored `SortOrder`; it now picks the lowest `SortOrder` (ties by QuestId), pinned by an automation test.
- **A single Steam account cannot join its own lobby**, so `steam-host` proves everything up to the second player. The
  real two-player test needs a second account/machine, both licensed for the App ID (or Spacewar 480).
- **Steam never starts inside the editor/PIE process** (`FOnlineSubsystemSteam::IsEnabled` is false under `UE_EDITOR`
  unless running as a game/server), so editor logs show Null/offline by design. Steam works in `-game` processes.
- **`-game` of the editor binary crashes in a voxel world** (Voxel's `WITH_EDITOR` `EnsureViewportIsUpToDate` calls
  `GEditor`, null there). The tests host on `/Game/Penumbra/Maps/TestMap` (no voxel content, real `BP_MOGameMode`; it
  spawns no pawn). A packaged game is unaffected. `--map ''` uses the real gameplay map (will crash today).
- **Never call `unreal.EditorLevelLibrary.*` in a `-game` process** — it dereferences GEditor and kills the process.
  `claude_bridge.py` finds the world there via the asset registry + `find_object` instead.
- `EditDefaultsOnly` properties (e.g. `GameplayLevelPath`) are not reachable from Python; call the UFUNCTION
  (`unreal.MOSessionSubsystem.get(world).host_session(name, max, map)`).
- **A simulated click proves nothing about hit-testing.** `MO.Test.ClickWidget` uses `SimulateClick` for MO buttons, so
  a row whose button had no widget tree (clicks passed straight through) looked perfectly clickable until a real
  mouse was used. For anything a player must click, do one real click: `request_access` for `UnrealEditor.exe`, take a
  screenshot, click the widget in the `ue.py inst` window, then read the result from the instance log. The test
  instances log `[MOJoinGamePanel] Buttons: ... -> Join enabled|DISABLED` for exactly this.
- **The in-game Host button travels to the REAL gameplay map.** That used to crash a `-game` editor-binary window
  (Voxel's `EnsureViewportIsUpToDate` dereferenced the null `GEditor`); the guard in
  `Plugins/Voxel/.../VoxelSystemUtilities.cpp` fixed it, and `nettest game` / `nettest hostsave` now run the real map.
  `nettest lan` still hosts the non-voxel TestMap by default (faster boot).
- **Sessions outlive worlds.** The session lives in the online subsystem, so quit-to-menu / disconnect left a stale
  session (host: "Already in a session" and a ghost Steam lobby). `UMOSessionSubsystem::ReleaseStaleSession()` runs
  when the main menu loads; Host/Join also replace a stale session first. `nettest lan` covers it (quit both to the
  menu, then host and join again).
- **Every machine generates its OWN voxel terrain**; ground collision exists only where the Voxel world has an invoker.
  (1) The seed used to be applied only in host-only `AMOGameMode`, so a client had no terrain (the level's `AVoxelWorld` has
  `bCreateRuntimeOnBeginPlay` off): now `AMOGameState::WorldSeed` + `UMOWorldSeedSubsystem`. (2) The default invoker is each
  machine's camera, so a joining client's ground kept changing for ~7-9 s after "ready" (up to 3.7 m, either direction; 3 of
  6 fresh joins) and a remote pawn away from the host camera had no ground on the server (20 fall-through rescues in 32 s):
  `AMOCharacter::VoxelCollisionInvoker` (`bWaitForVoxelWorld`), enabled in `NotifyControllerChanged` while a human drives the pawn.
- **Measure ground with `MO.Voxel.SurfaceGrid` / `SurfaceZ`, which accept only voxel-collision hits.** Two checks I wrote
  were vacuous and had to be retracted: a first-hit trace "agreed" at z=30000 (the trace origin: a PCGVolume brush) on both
  machines; and "client pawn z == server z" passes with NO client terrain (the client just reports the server's position).
- **Clock vs sky vs weather in co-op.** The game clock is per machine: `UMOWorldSyncSubsystem` publishes the host's clock (+ time scale) in
  `AMOGameState` and clients snap/adopt it; without it a mid-day joiner ran its own 08:00 (control `--withhold-sync`). The SKY is checked
  separately (`MO.Weather.SkyTime` asks the bridge what the Ultra Dynamic Sky actor shows): a clock label can agree while the sky does not.
  **Weather is carried by Ultra Dynamic Weather's own replication** (`Ultra_Dynamic_Sky_C` / `Ultra_Dynamic_Weather_C` replicate; `BP_WeatherBridge_C`
  does not): with the actors' replication switched off the client got the host's preset through the bridge (`SetWeatherPreset ... dispatching to
  provider`) and stayed on its own weather, because UDW's Change Weather is server-only. So the weather LABEL is not a negative control; the
  `--withhold-sync` weather control asserts only that the MO sync did not run. Open fork: a client-side apply in `BP_WeatherBridge` would let the
  bridge be the single path (Wes prefers the bridge to native UDS).
- **Stuck in a tree.** Spawns/teleports used a thin line trace; the pawn's capsule overlapped trunks. `MOSpawnClearance` ("does a capsule fit
  here?", ring search to the nearest clear ground) is used by the join spawn, first spawn, fall-through rescue; `AMOCharacter::CheckEmbeddedSafety`
  frees a human-driven pawn that STAYS inside solid geometry (>= 3 s). Test: `MO.Test.EmbedPawn` wraps the joiner in a cube **and pins it
  (MOVE_None)** -- unpinned, character movement's depenetration freed it from the same block in one run and not in the next (829 cm vs 22 cm), so
  the control was flaky. Pinned: control stays 0 cm, rescue moves ~840 cm. `MO.EmbeddedRescue.Disable 1` is the control switch.
- **Do not trust a PASS you have not seen FAIL**: every new check here has a control that must fail (`--skew-seed`).
- **Loading a save**: pawns respawned from a save fell ~70 m while the voxel terrain regenerated (z 1019 -> -6123) and the host's
  pawn was teleported km away. `AMOCharacter::SetLoadHold` (no gravity, no rescue) now spans WaitForVoxelAndRegroundPawns -> FinishLoadHandoff.
- **`claude_seq` sequences work in `-game` processes** (its `_worlds()` reuses the bridge's resolution). `ue.py seq` prints only
  DONE/FAILED: read `inst_<name>/ue_out.txt` for a traceback. `str(<enum>)` in Python is `<MovementMode.MOVE_WALKING: 1>`.
- `ue.py` follows the **freshest `MO57*.log`**: a restarted editor writes `MO57_2.log` while the old process is still
  shutting down, which used to blind every log-delta verb.
- Net-driver config traps (pinned by `Tools/tests/test_ue_inst.py`): the engine takes the FIRST `NetDriverDefinitions`
  entry per DefName (so `!NetDriverDefinitions=ClearArray` is required), and a driver class that does not exist in
  this engine version silently falls back to `IpNetDriver` (5.8's Steam driver is `/Script/SteamSockets.SteamSocketsNetDriver`).
- `bIsLANMatch` must be true on the Null OSS or the host never answers a search — now derived in ONE place,
  `UMOSessionSubsystem::IsLanMode()`.

## The three layers

| Layer | Reaches | Editor state |
|-------|---------|--------------|
| **C++** (UnrealBuildTool CLI) | compiled game code: systems, components, structs, RPCs/replication, save/load, AI | editor **CLOSED** (Live Coding blocks CLI builds) |
| **In-editor MCP** (`http://127.0.0.1:8000/mcp`) | assets, DataTables, materials, level, object properties, Blueprints, string tables, PIE control | editor **OPEN** |
| **Test harness** (PIE + computer-use / `EditorAppToolset` PIE control + `LogsToolset`) | runtime behavior — the only way to know a change actually *works* | editor **OPEN** |

> **The orchestration constraint:** C++ builds need the editor closed; MCP and
> PIE need it open. The autonomous loop is therefore a state machine:
> **edit (C++ via Bash / data+assets via MCP) → if C++ changed: close editor, UBT build, reopen → MCP/PIE verify → read logs → iterate.**
> C++ and MCP are the write hands; the harness is the only eyes. "Compiles" ≠ "works."

## Decision table — pick the tool by task

| Task | Tool | Notes |
|------|------|-------|
| Gameplay logic, components, **structs, RPCs, replication, save/load, AI** | **C++** | the audit-fix campaign was all here |
| Console commands / cheats (definition) | **C++** | then invoke via harness/MCP |
| **DataTable content** — quests, recipes, skills, treatments, body parts, resources | **MCP** `DataTableTools` | `get_rows`/`set_rows`; runtime truth; no CSV reimport / no row-name-quote crash |
| New DataTable, CSV→DT import | **MCP** `DataTableTools.create` / `import_file` | |
| Material flags (Nanite/ISM), params | **MCP** `Material*` / `ObjectTools` | |
| Asset/object props, `UDeveloperSettings`, soft refs | **MCP** `ObjectTools` | |
| Level: place/remove/swap actors, lighting, camera | **MCP** `Scene/Actor/PrimitiveTools` | |
| **Widget Blueprints (UMG layout, BindWidget wiring, class defaults)** | **`ue.py ui build <spec>`** (editor Python, `Content/Python/mo_ui.py`) | the MCP has no UMG toolset; this does the whole tree + Is Variable + compile + contract check — `Docs/UI_TOOLING.md` |
| Blueprint graphs | **MCP** `BlueprintTools` | was a hard gap pre-5.8; verify scope per task |
| Localization / string tables | **MCP** `StringTableTools` | |
| `.ini` config | **Bash/Edit** | text file |
| Build / cook / package | **Bash** (UBT / RunUAT) | **editor must be closed** |
| **Verify at runtime** — spawn, pickup, save/load round-trip, weather, death | **Test harness** (PIE) | |
| **Multiplayer / co-op** verification | **Test harness** (2-client PIE) | C++ alone cannot prove networking |
| Visual check (grass, lighting) | **MCP** `EditorAppToolset` CaptureViewport / harness screenshots | |
| Input → gameplay testing | **Test harness** (real input) | synthetic key injection does NOT reach Enhanced Input |

## How to reach the in-editor MCP (when the harness client is disconnected)

The MCP is an HTTP (streamable) server the editor's ModelContextProtocol plugin
hosts on `127.0.0.1:8000/mcp` (config: `.mcp.json`). It drops when the editor
closes for builds and the harness may not auto-reconnect. You can drive it
directly with `curl` (handshake + JSON-RPC):

1. `POST initialize` → capture the `Mcp-Session-Id` **response header**.
2. `POST notifications/initialized` with that header.
3. `POST tools/call` with the header; meta-tools: `list_toolsets`,
   `describe_toolset {toolset_name}`, `call_tool {toolset_name, tool_name (BARE,
   no prefix), arguments}`.

19 toolsets: AgentSkill, EditorApp, Logs, Actor, Asset, **Blueprint**, CurveTable,
DataAsset, **DataTable**, Material, MaterialInstance, Object, Primitive, Scene,
SkeletalMesh, StaticMesh, **StringTable**, Programmatic, Texture.

Content-fix loop (proven): `DataTableTools.get_rows` → mutate JSON →
`DataTableTools.set_rows` (values = stringified JSON) → `AssetTools.save_assets`
→ `.uasset` changes on disk → commit.

## Hit / miss record (first session, 2026-06-30)

**HITS**
- MCP server reachable + healthy over HTTP even with the harness client dropped.
- Manual curl handshake; session id persists across calls.
- `list_toolsets`, `describe_toolset`, `call_tool` dispatch.
- `DataTableTools.list_rows` / `get_rows` — read rows incl. nested struct arrays.
- `DataTableTools.set_rows` — **wrote** a nested-struct-array fix to 10 rows;
  FText/NSLOCTEXT + arrays round-tripped intact.
- `AssetTools.save_assets` — persisted to disk (confirmed via git).
- `DataTableTools.add_rows` (creates blank rows by name) + `set_rows` —
  **authored 33 brand-new rows from scratch** (22 body parts + 11 medical
  items); enum fields, NSLOCTEXT names, and nested vectors all round-trip.
- Reads RUNTIME ground truth → caught CSV↔DataTable drift (H40a already fixed
  in DT_Quests; only the CSV was stale).

Note: `add_rows` takes `{data_table, row_names[]}` ONLY (no values) — it makes
blanks; populate them with a following `set_rows`. Different shape from `set_rows`.

**MISSES / FRICTION**
- Harness MCP client did not auto-reconnect on editor reopen (server fine) → curl workaround.
- `call_tool` needs `toolset_name` + a **bare** `tool_name`; full-prefixed name → "not found".
- Response format mixed: `tools/list` = plain JSON, `tools/call` = SSE (`event:`/`data:`) — parse both.
- `returnValue` is double-nested (`content[0].text` → `{"returnValue": "<json string>"}` → parse again).
- **Python `urllib` gets an empty body for `tools/call`** (SSE/chunked handling) — the HTTP client must shell out to `curl`.

## #136 (data integrity) — COMPLETE via MCP
The CSV-based audit was partly stale; the real pass was verify-via-MCP against
the live DataTables, all done:
- **H40a** — verified already-correct (BuildCampfire id was right in DT_Quests; CSV drift).
- **H40b** — verified self-consistent (`SnareWire01` grants `SnareSettin`, which IS the consumed id; ugly but not broken).
- **H40c** — phantom harvest skills (Woodcutting/Mining/Knapping) remapped to real skills.
- **H40d** — 8 harvest actions hard-gated on non-existent knowledge → ungated (`NAME_None`).
- **M21** — 22 missing body-part rows (kidneys/fingers/toes) + 11 missing medical items authored via `add_rows`.

Final re-scan: zero broken knowledge gates, zero dangling treatment item refs.

FOLLOW-ON (gameplay content, NOT data integrity): the 11 medical items have no
production chain — they need recipes/loot to be obtainable, and treatment-consume
logic should keep the reusable splint/blanket/tourniquet. Icons/meshes are
placeholder pending art.

## Hit / miss record — A3 visual-QA rung (2026-07-04)

`ue.py asset shot <target> <out.png>` — the loop can now SEE. Three capture
paths probed on `EditorToolset.EditorAppToolset`:

**HITS**
- `CaptureAssetImage {assetPath}` → base64 PNG thumbnail of any mesh/material/
  texture. Gate proof: Megascans stick renders and *reads as a stick*.
  Exposed as `ue.py asset shot /Game/... out.png`.
- **`HighResShot` via bridge = THE PIE game-view capture** (`ue.py asset shot
  pie out.png`): boot → shot → 3.2MB 1920x1080 of the possessed survivor on
  voxel terrain. First-ever agent visual QA of the running game — and it
  immediately surfaced a real finding: a mirrored-terrain band across the sky
  (water-plane/reflection artifact, logged as a P-track lead).
- Param quirk: the toolset bindings REQUIRE every param key present —
  `{}` → "needs a default value"; pass explicit `null` for optionals.

**MISSES / FRICTION**
- `CaptureViewport` shoots the **editor scene view only** — during PIE it
  returns the empty editor world (black + axis gizmo), NOT the game. The A3
  gap-risk fallback (HighResShot) is therefore the canonical PIE path.
- Git Bash mangles `/Game/...` into `C:/Program Files/Git/Game/...` — call
  with `MSYS_NO_PATHCONV=1` (or from PowerShell).

## Packaging vs. the MCP port (2026-07-04)

Editor-UI packaging ("Cook failed" with a clean-looking log) traced to ONE
error line: `HttpListener unable to bind to 127.0.0.1:8000`. With
`bAutoStartServer=True` (EditorPerProjectUserSettings), EVERY editor process
auto-starts the MCP server — including the cook commandlet the editor spawns
for packaging. Parent editor holds 8000 → child bind fails → one Error-level
line → commandlet exit 1 → cook failed.

Fix (no engine rebuild — UE_5.8 is an INSTALLED build, project builds cannot
recompile engine modules): `bAutoStartServer=False` in per-project user
settings; `ue.py editor start` passes `-ModelContextProtocolStartServer`
instead. Loop-launched editors keep the MCP hands; commandlets stay silent;
editor-UI packaging works with the editor open (verified: cook green while
editor running). A dormant `!IsRunningCommandlet()` guard is also patched
into the engine source (ModelContextProtocolEditor.cpp) for whenever the
engine gets rebuilt. If the editor is ever launched OUTSIDE ue.py and needs
MCP: console `ModelContextProtocol.StartServer`.

## Hit / miss record — P2 PCG biome layer (2026-07-04)

**HITS**
- **PCG graph authoring from editor-Python WORKS — the "MCP-PCG graph gap" did
  not materialize.** `PCGGraph.add_node_of_type`, `PCGNode.add_edge_to(
  "Out", node, "In")`, `remove_edge_to`, `node.get_settings().set_editor_property`,
  save_asset: the MO Biome Spawner was added to MOPCG_StampScatter1, rewired
  twice, and instance-tuned entirely from the loop. Graph surgery is scriptable.
- Node topology introspection: `node.input_pins` -> pin `.get_editor_property
  ("edges")` -> upstream node — enough to map a 50-node production graph.

**TRAPS (cost real iterations — check these FIRST next time)**
- **`get_components_by_class(HISM)` does NOT return plain ISM components**, and
  PCG's GetOrCreateISMC may create ISM even when the descriptor asks for HISM
  (grass got HISM, trees/rocks got ISM — same node, same execution). Probe with
  the ISM BASE class or half the scatter is invisible. This false "trees
  vanish" signal burned ~5 debugging iterations.
- **`FISMComponentDescriptor` equality/hash EXCLUDES ComponentTags** (UE5.8
  ISMComponentDescriptor.cpp): descriptor-equal components merge/reclaim
  across chains sharing a mesh, and crc-less nodes (native Static Mesh
  Spawners) reuse ANY descriptor-equal managed resource. Custom spawner nodes
  must set a unique `Descriptor.RayTracingGroupId` + `SettingsCrc` (see
  MOPCGBiomeSpawnerSettings.cpp).
- **Biome bands must PARTITION the sample space** (catch-all + priorities):
  voxel-surface normals REFINE across PCG re-generations, so points that
  matched narrow slope bands on the first pass fall out on later passes and
  the layer visibly thins as cells re-execute.
- The scattering cells re-generate repeatedly while the voxel world settles —
  transient world-state probes must poll, and per-execution [MOBiomeSpawner]
  bucket logs are the ground truth for what the node produced.

## P4 look-feedback round (2026-07-04) — probe deterministic math, don't travel

Wes's look-review drove 5 changes (harvest registration, upright trees,
prairie density /4, oasis clusters, 10-50x biome scale). Verification lessons:
- **Query the mask, don't teleport to it.** Proving "biomes are big regions"
  by flying a probe pawn 175k UU failed three ways (ocean headings, voxel-gen
  latency, teleport churn fighting streaming). The mask is pure math — it's
  now exposed as `UMOBiomeDatabaseSettings.ResolveBiomeAt` (shared with the
  spawner so they can't drift) and the gate samples a 16x16 grid: 2 biomes,
  contiguity 0.86, in milliseconds.
- Scatter never spawns below sea level now (biome HeightMin=-100): the first
  ocean-probe teleport revealed the Meadow catch-all would have carpeted the
  seafloor in grass.
- Leads filed: spawn placement doesn't avoid tree clumps (survivor can spawn
  inside a canopy); forest clump compensation (~3.3x local density) may want
  tuning once aerial-vantage shot tooling exists.

## MO.Test.* runtime harness + closed-loop verification (2026-07-01, UE 5.8)

The eyes now close the loop with **zero screenshots**. Two pieces:

**1. `MO.Test.*` console harness** (C++, `MOCheatSubsystem`) — each logs a greppable marker:

| Command | Marker | Checks |
|---------|--------|--------|
| `MO.Test.State` | `[MOQUERY] STATE` | netmode / level / inGame / possessed pawn (menu→NO, in-game→YES) |
| `MO.Test.FindWidget [sub]` | `[MOQUERY] WIDGET` | live CommonUI widgets matching the substring, with on-screen center/rect — the "where's the button" locator |
| `MO.Test.DropPickup [item]` | `[MOTEST] PASS/FAIL` | give→drop→pick-up round-trip; GUID identity preserved |
| `MO.Test.Attack` | `[MOTEST] PASS/FAIL` | StartLightAttack → combat state |
| `MO.Test.Craft [recipe]` | `[MOTEST] INFO` | EnqueueCraft (false on a fresh pawn = no recipe/mats — expected) |
| `MO.Test.MPSuite` | — | runs the three above via ConsoleCommand |
| `MO.Test.RunAll` / `MO.Test.ValidateData` | `Saved/MOTestResults.txt` | full regression suite / DataTable-integrity gate; results FILE, not log scraping |
| `MO.Test.Input <action>` | `[MOTEST] PASS/FAIL` | drives IMOControllableInterface::Execute_Request* (Move/Look/Jump/Sprint/Interact/Primary/Secondary/Terraform...) — the post-Enhanced-Input seam, so #144 doesn't apply. Move/Look are per-frame: drive via claude_seq |
| `MO.Test.ClickWidget <name>` | `[MOTEST] PASS/FAIL` | REAL UI click: UMOCommonButton -> guarded SimulateClick(); others -> synthesized Slate pointer click at screen center. Locate names with FindWidget first |
| `MO.AI.DumpBlackboard <pawnSub>` | `[MOQUERY] BB` | every blackboard key+value (DescribeKeyValue over the asset chain) |
| `MO.AI.SetKey <pawnSub> <key> <val...>` | `[MOQUERY] BB` | typed write (bool/float/int/vector/name/string/enum/object-by-actor-name) + readback |

PIE runs ~3 fps when the editor window is unfocused (background throttling) — each claude_seq
`yield` frame is then ~0.3 s of wall/game time; sample transient states (jumps, montages)
per-frame rather than waiting N frames and checking once.

**2. The file-I/O bridge is the driver** (`Content/Python/claude_bridge.py`, auto-loaded by
`init_unreal.py`; **survived the 5.7→5.8 upgrade**). A whole verification session is PowerShell + grep —
no clicks, no screenshots: foreground the editor → append bridge lines to `%TEMP%\claude\ue_cmd.txt`
(`py:import agent_test_lib as atl; atl.begin_pie(out)` → `atl.skip_intro(world,out)` →
`atl.start_new_game(world,out,seed=N)` → `MO.Test.X`) → grep `Saved/Logs/MO57.log` for
`[MOQUERY]`/`[MOTEST]`. Full playbook: `Docs/Agent_PIE_Testing.md`. The bridge IS tracked in git
(`Content/Python/claude_bridge.py`, alongside `claude_seq.py`, the tick-driven multi-frame
sequence runner) — dev-machine tooling, never ship. Prefer driving all of this through
`Tools/ue.py` (see top) rather than raw file appends.

**Verified live (2026-07-01):**
- **#159** (H21 pickup regression): `MO.Test.DropPickup` → **PASS**, GUID intact. The new
  `UMOInteractorComponent::ServerPickUpWorldItem` (HasAuthority-gated, distance-validated, no crosshair
  re-trace) fixes UI-driven pickups (nearby menu / loot-all / drag) that H21 had accidentally aim-gated.
- **#160** (FindWidget): dropping the `IsInViewport()` gate → **18 matches** at the main menu incl.
  NewGameButton / LoadGameButton — CommonUI widgets live on activatable stacks, never `AddToViewport`.
- `MO.Test.Attack` → PASS; `MO.Test.State` correct at menu (inGame=NO) and in-game (inGame=YES + pawn).

**Fallback when NOT using the bridge:** the CommonUI menu captures viewport keyboard input, so the
in-game `~` console can't receive typed text over a menu — run `MO.Test.*` from the **editor's Output Log
console command box** (press Shift+F1 first if in-game to free the mouse) and read the Output Log search filter.

## Packaging a Development build for real-account testing (2026-10-07)

Editor closed, nothing from `ue.py inst` running (the cook wants the machine). Development config, because a Shipping build writes no
log and a real two-account test without logs cannot be diagnosed:

```powershell
& 'D:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun -project='D:\UEProjects\MO57\MO57.uproject' `
  -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -iostore -compressed `
  -stagingdirectory='D:\UEProjects\MO57\Saved\StagedBuilds_DevTest' -unattended -utf8output *> Saved\Logs\package.log
```

~1-3 min with a warm DDC. The exe is `StagedBuilds_DevTest\Windows\MO57.exe` (a stub; the game is `MO57\Binaries\Win64\MO57.exe`).
Its logs land in `StagedBuilds_DevTest\Windows\MO57\Saved\Logs\MO57.log` (a second copy writes `MO57_2.log`), and its saves in the
staged folder's own `Saved`, NOT the project's. Smoke test it with `-NoSteam -log -windowed`; a second copy joins a first with
`MO57.exe 127.0.0.1 -NoSteam` (direct connect; the same world/seed/pawn logic as a session join).

**Build race (fixed):** `Tools/Update-BuildInfo.ps1` is a PreBuildStep that runs once PER TARGET, and `BuildCookRun` builds
`MO57Editor` and `MO57` together, so two copies wrote `MOBuildInfoGenerated.h` (timestamped, so never identical) at the same
moment: "The process cannot access the file ... being used by another process", `BUILD FAILED` (twice in a row after one lucky
pass). It now serialises writers with a named mutex and retries on IOException.

## In-game bug report form and the crash endpoint (2026-10-09)

Players report from the in-game menu ("Bug Report" -> form: title, category, what happened, steps, optional contact, checkboxes for the log tail and a screenshot, "Show What Will Be Sent",
Send, Open Discord). The report is a **CR1 bundle** -- the crash reporter's own upload format -- POSTed to `[CrashReportClient] DataRouterUrl`, so the website needed no change: it stores
whatever arrives. Code: `MOBugReportBundle` (pure format/scrubber), `UMOBugReportSubsystem` (state collection via registered contributors, screenshot, upload, on-disk fallback), `UMOBugReportPanel` (the form).
Spec: `Content/Python/ui_specs/bug_report_panel.py`. Memory/detail: `crash-endpoint-and-bug-reports`.

| Tool | Does |
|------|------|
| `python Tools/ue_crash_bundle.py <file.bin>` | summary of one upload (kind, title, category, contact, build, fields); `parse_bundle` / `write_bundle` for scripts. Parses all 72 real crash uploads |
| `python -I Tools/crash_triage.py <crash-dumps dir> [--bugreports\|--crashes] [--since D]` | crashes grouped by signature (exe, mode, what failed, top frames) + bug reports newest first |
| `python Tools/bugreport_receiver.py --port P --out DIR [--status 500]` | local stand-in for the endpoint (stores `<ver>/<date>/<uuid>.bin` + `.json` like the site) |
| `ue.py nettest bugreport [--package]` | packaged game x3 phases (accepted / server 500 / unreachable); see the table above |
| console: `MO.BugReport.Preview`, `MO.BugReport.SendTest [-shot] [-delay=S] [title]`, `MO.BugReport.EndpointOverride <url>` (loopback only), `MO.BugReport.CooldownSeconds <n>` | the same `Submit` the Send button calls |

Rules the code keeps (each has a test): automatically collected values and the log are scrubbed of the OS user name, computer name and `C:\Users\<name>` paths (UE writes the first two into its
own log header -- the nettest asserts the raw log DOES contain them, so "scrubbed" is a real result); typed text is sent as typed and shown in the preview; contact is opt-in; one successful send per 60 s;
bundle capped at 4 MB (screenshot dropped first, then log); an unsent report is kept in `Saved/BugReports/<id>.uecrash` and the player is told where. Add state to every report with
`UMOBugReportSubsystem::Get(this)->RegisterContributor("MySystem", [](const UWorld*, FMOBugReportFields& Out){ Out.Emplace("Key", "Value"); })` -- the rows appear as `MySystem.Key`.

### Rescue / spawn diagnostics (2026-10-09, the "client can't see the host's pawn" bug)

| Tool | Does |
|------|------|
| `python Tools/pawnvis_probe.py [--stop]` / `--look` | host + client joined; BOTH machines list every character (existence, position, controller, mesh visibility). Leaves the instances running for a look at the windows (computer-use on `UnrealEditor.exe`); `--look` asks again |
| `python Tools/rescue_probe.py [depth ...]` | one hosted world: bury the host's pawn at several depths with the OLD rule (`MO.Rescue.FirstHitOnly 1`) and the fixed one, log what the rescue's traces see |
| `python Tools/rescue_probe.py --hunt N` | host N fresh random-seed worlds with ONE process; per world the spawn z, the settle line, the pawn's standing height over the real ground and the rescue count (expect: settle found ground, ~85 cm, 0 rescues) |
| `MO.Test.RescueTrace [player]` | log what the rescue's traces hit from a pawn's spot (below / up / down from 50 km / every blocking hit on the sky line / the voxel-only answer) |
| `MO.Test.RoofAbove [player] [height] [halfExtent]` / `MO.Test.ClearRoof` | hang a solid block over a player's spot (a stand-in for a tree's collision, a roof, a prop) |
| `MO.Rescue.FirstHitOnly 1` | TEST ONLY control: the rescue's original single first-hit trace |

**Trap (cost an evening):** a multi trace BY CHANNEL (`LineTraceMultiByChannel`) ends at the first BLOCKING hit; an object-type multi trace (`LineTraceMultiByObjectType`) walks every object on the line. "Is there terrain under this spot?" must use the second (`MOSpawnClearance::TraceVoxelGround`), otherwise a tree or a roof over the ground hides it. A control that passes at an arbitrary spot says nothing about the spot that failed: reproduce the failing COLUMN (`MO.Test.RoofAbove`).

**Unattended runs:** `Tools/ov.bat` (never edited) runs `Tools/ov_task.sh` (rewritten per job, untracked) from a snapshot kept in `Saved/Logs/ov_history/<stamp>.sh` + `.log`.
