# Unreal MCP Audit

Status: In progress. Findings are not considered verified until tied to configuration, source, or runtime evidence.

## 1. Current architecture

Verified static architecture:

1. `MO57.uproject` enables Epic's experimental `ModelContextProtocol`, `MCPClientToolset`, `EditorToolset`, `Terminal`, `PythonScriptPlugin`, and `EditorScriptingUtilities` plugins.
2. `.mcp.json` configures one streamable-HTTP endpoint at `http://127.0.0.1:8000/mcp`.
3. `Tools/ue.py` launches `UnrealEditor.exe` with `-ModelContextProtocolStartServer`, performs MCP initialize/initialized handshakes, caches `Mcp-Session-Id`, and dispatches through the `call_tool` meta-tool.
4. Epic's `FModelContextProtocolServer` binds POST/GET/DELETE routes and marshals completion callbacks to the game thread (`ModelContextProtocolServer.cpp:416-441, 853-907` in the UE 5.8 installation).
5. Epic's `ModelContextProtocolEditor` adapts ToolsetRegistry tools into MCP. Most editor capabilities are Python toolsets under `EditorToolset/Content/Python/editor_toolset/toolsets`.
6. Separately, `Content/Python/init_unreal.py` auto-loads `claude_bridge.py` and `claude_seq.py`. The bridge polls fixed files under `%TEMP%/claude` on a Slate post-tick callback and executes either console commands or arbitrary Python in the selected PIE/editor world.

The MCP and file bridge are complementary but distinct transports. MCP is primarily the semantic editor/asset surface; the bridge is MO57's correlated runtime-test and arbitrary editor-Python driver.

## 2. Command and capability inventory

- MCP discovery/dispatch: `list_toolsets`, `describe_toolset`, `call_tool` (documented and wrapped by `Tools/ue.py`).
- EditorToolset families documented in `Docs/AUTONOMOUS_TOOLING.md`: AgentSkill, EditorApp, Logs, Actor, Asset, Blueprint, CurveTable, DataAsset, DataTable, Material, MaterialInstance, Object, Primitive, Scene, SkeletalMesh, StaticMesh, StringTable, Programmatic, Texture.
- MO57 safe wrappers: DataTable list/get/set with deep merge and readback; asset save, screenshot, assignment, import; editor lifecycle; build; PIE; automation; multiplayer smoke; log/result collection.
- Runtime semantic commands live behind `MO.Test.*`, `MO.AI.*`, and project console subsystems; complete inventory remains in progress.

## 3. Integration with existing toolset

Strong reuse already exists: `Tools/ue.py` composes MCP asset/DataTable operations with the bridge, project console commands, `UMOEditorTestHelper`, UBT, and automation. The correct target is to extend these semantic wrappers and project domain APIs, not duplicate them inside a second MCP implementation.

## 4. What is working well

- Server is loopback-bound; server code recognizes localhost/loopback hosts.
- Game-thread completion is asserted and off-thread results are marshaled back.
- Native MCP tool libraries wrap invocation in `FScopedTransaction` (`ModelContextProtocolToolLibrary.cpp:251`).
- MCP utility code includes path traversal protection (`SafeConvertRelativePathToFull`).
- `Tools/ue.py` correlates bridge output, captures log deltas, uses timeouts, reconnects stale MCP sessions, deep-merges nested DataTable structs, reads back mutations, and saves explicitly.
- Editor startup is explicit, avoiding the documented port collision when cook commandlets inherit auto-start.

## 5. Confirmed defects

- `Tools/ue.py` is machine-specific: project, engine, temporary-file, and endpoint paths are hardcoded (`ROOT`, `UBT`, `EDITOR_EXE`, `TMP`, `MCP_URL`). This blocks clean onboarding and alternate worktrees/machines.
- Graphify is documented as available through `python -m graphify`, but the active Python resolves to Anaconda and cannot import it. The architecture-first workflow is currently broken in this shell.
- The root README is materially stale: it describes the earlier monolithic module, old audit paths/dates, CSV-first authoring, and automatic commit/push workflow that conflict with current project rules and the verified four-module plugin/tooling design.

Actions taken 2026-07-12:

- Removed hardcoded repository/engine/MCP configuration from `Tools/ue.py`; added discovery plus explicit environment overrides and `ue.py config`.
- Replaced heuristic first-line SSE/error handling with complete-event parsing and structured `MCPError` results.
- Restricted reconnect behavior to transport failures and the engine's verified `Unknown session id ... client should reinitialize` response.
- Namespaced MCP session/body files by project and endpoint.
- Added 15 offline contract tests, including DataTable deep-merge invariants. Live editor compatibility validation remains pending.

## 6. Architectural risks

- Two overlapping automation transports have different security, correlation, transaction, and error semantics. Callers need a documented routing policy.
- `ProgrammaticToolset` and `claude_bridge.py` expose arbitrary Python execution. They are powerful development escape hatches, not safe semantic MCP tools.
- The file bridge uses shared fixed filenames and no per-client authentication/locking. Concurrent agents or editor instances can interleave commands and results despite per-call markers.
- MCP responses are normalized heuristically by `Tools/ue.py`; `_mcp_parse` assumes the first content element and first SSE data line, losing structured error detail and potentially mishandling multi-event responses.
- Bulk DataTable writes remain non-atomic from MO57's perspective: merge, write, readback, and save are separate calls. A failure can leave an in-memory dirty asset or a partially verified operation.

## 9. Security and safety concerns

- Keep the server bound to loopback. Do not expose port 8000 to LAN/WAN without authentication and an allowlisted tool surface.
- Disable or exclude Programmatic arbitrary execution from any remotely reachable profile.
- Treat `claude_bridge.py` as trusted-local development tooling; its own header correctly states it must never ship.
- Mutating workflows should prefer semantic wrappers with preview/readback over raw `call_tool` or `py:` execution.

## 10. Reliability and recovery concerns

- Fixed temp paths and a single cached session file are not instance-safe.
- The session/body collision is fixed. Bridge command/output files retain a compatible shared default; callers can isolate them with `MO57_BRIDGE_DIR`, but automatic per-editor isolation is not yet implemented.
- Bridge acknowledgements report command submission/execution exceptions, not transactional rollback or semantic success.
- No verified cancellation/progress contract has yet been found for long-running EditorToolset calls.
- Editor close invalidates sessions; reconnect is handled by `ue.py`, but external MCP clients may not recover automatically.

## 11. Recommended target architecture

Keep Epic's MCP server and EditorToolset as infrastructure. Make `Tools/ue.py` the MO57 orchestration facade, split into configuration, MCP transport, bridge transport, semantic domain commands, and validation/reporting. Prefer project-owned high-level operations that call existing subsystems/utilities. Gate all broad mutations behind preview + explicit apply, return changed objects/packages/files, and retain raw/programmatic execution only as a clearly unsafe local escape hatch.

## 14. Proposed testing strategy

- Unit-test MCP response parsing, session renewal, configuration discovery, deep merge, and error normalization outside Unreal.
- Add contract smoke tests for tool discovery and representative read-only calls.
- For each mutating family: preview, apply, readback, dirty-package report, save, reload verification, and Undo where supported.
- Exercise Editor, PIE standalone, listen server/client, compile-in-progress, map transition, and shutdown states.
- Continue runtime semantic verification through `MO.Test.*` and the 2-client harness.

## 15. Open questions requiring human direction

- Should arbitrary Programmatic/bridge execution remain available to all local MCP clients, or should MO57 introduce an allowlisted default profile?
- Is multi-worktree or simultaneous multi-editor operation a required workflow? This determines the urgency/design of port and temp-file namespacing.
- Which Python environment owns Graphify on this workstation?
- Should CI be introduced now, and on what runner with licensed UE 5.8 access?

## Audit checklist

- Startup and project/editor discovery
- Transport and correlation
- Tool registration and schema validation
- Object/asset addressing and stable identifiers
- Editor state, world context, PIE, compile, shutdown safety
- Game-thread execution, GC safety, transactions, dirty packages, saving, registry synchronization
- Progress, cancellation, timeout, recovery, idempotency
- Structured logging, errors, payload size, security, path validation
- Existing tool reuse and duplicate capability
- Tests and production-readiness roadmap
