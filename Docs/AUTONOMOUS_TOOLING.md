# MO57 Autonomous Tooling — Operating Manual

Goal: a toolset that can build *and verify* the game with minimal human input.
There are three "hands" (write) and one "eyes" (verify). The whole game of
autonomy is closing the loop between them.

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
- Reads RUNTIME ground truth → caught CSV↔DataTable drift (H40a already fixed
  in DT_Quests; only the CSV was stale).

**MISSES / FRICTION**
- Harness MCP client did not auto-reconnect on editor reopen (server fine) → curl workaround.
- `call_tool` needs `toolset_name` + a **bare** `tool_name`; full-prefixed name → "not found".
- Response format mixed: `tools/list` = plain JSON, `tools/call` = SSE (`event:`/`data:`) — parse both.
- `returnValue` is double-nested (`content[0].text` → `{"returnValue": "<json string>"}` → parse again).
- **Python `urllib` gets an empty body for `tools/call`** (SSE/chunked handling) — the HTTP client must shell out to `curl`.

## Implications for #136 (data integrity)
Much of the audit's #136 was CSV-based and partly stale. The correct pass is
verify-via-MCP against the live DataTables: H40a already correct; **H40c
fixed** (this commit); remaining — H40d (6 item-ids-used-as-knowledge gates →
"None" or real knowledge id; design call), M21 (treatment item refs, 22 missing
body-part rows), H40b (`SnareSettin` knowledge typo). All MCP-doable now that the
read/write/save loop is proven.
