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

Correlation design: every bridge call is bracketed with begin/end markers in
`ue_out.txt`, so output is attributed to *the* command — plus the game-log
delta from the command's start offset (`--grep` to filter). No more guessing
which tail lines were yours. Shell note: quote args from bash/git-bash;
PowerShell 5.1 mangles embedded quotes in native args.

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
| Blueprint graphs / widget setup | **MCP** `BlueprintTools` | was a hard gap pre-5.8; verify scope per task |
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
