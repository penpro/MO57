# Recommendations

Status: Backlog under construction.

Each item will include priority (P0-P3), affected files/systems, evidence, current behavior, risk, proposed change, benefit, difficulty, dependencies, validation, and timing.

## Execution sequence

1. Immediate stabilization
2. MCP foundation
3. MCP capability expansion
4. Architecture cleanup
5. Testing and automation
6. Documentation and onboarding
7. Future enhancements

## Initial prioritized backlog

### R1 — Isolate arbitrary execution from semantic MCP tools

- **Priority:** P1
- **Affected:** `Content/Python/claude_bridge.py`, Epic EditorToolset Programmatic tools, MCP configuration/docs
- **Evidence:** Both surfaces execute arbitrary Python; MCP is HTTP and currently loopback-bound.
- **Risk:** Accidental or future remote exposure grants full editor/user-context code execution.
- **Change:** Keep loopback-only; document trust boundary; create an allowlisted profile for any future remote/packaged use; never ship the bridge.
- **Difficulty:** Medium; depends on intended remote-access policy.
- **Validation:** Bind-address check, packaged plugin inventory, negative calls against disabled tools.
- **Timing:** Now for documentation, before any remote expansion for enforcement.

### R2 — Make `Tools/ue.py` environment-configurable and instance-safe

- **Priority:** P2
- **Affected:** `Tools/ue.py`, setup documentation
- **Evidence:** Hardcoded project, engine, temp, endpoint, and session paths.
- **Risk:** Onboarding failure; wrong worktree/editor targeting; concurrent-session collisions.
- **Change:** Discover repo from script path, read engine association/config with environment overrides, and namespace temp/session files by project plus editor instance/port.
- **Difficulty:** Medium.
- **Validation:** Unit tests plus two worktrees/editor-port configurations.
- **Timing:** Implemented in part on 2026-07-12. Configuration discovery, overrides, diagnostics, and MCP file namespacing are complete and offline-tested. Automatic bridge isolation remains conditional on owner requirements; `MO57_BRIDGE_DIR` supports explicit isolation now.

### R3 — Harden MCP parsing and structured errors

- **Priority:** P2
- **Affected:** `Tools/ue.py` `_curl`, `_mcp_parse`, `mcp_call`
- **Evidence:** First-SSE-line/first-content assumptions and `{}`-means-stale-session heuristic discard JSON-RPC error categories.
- **Risk:** False retries, opaque failures, incomplete results.
- **Change:** Parse complete SSE events, preserve JSON-RPC errors, classify transport/session/tool/validation failures, return remediation.
- **Difficulty:** Medium.
- **Validation:** Recorded response fixtures for success, multi-event, error, timeout, and stale session.
- **Timing:** Implemented and covered by offline fixtures on 2026-07-12. A live MCP compatibility probe remains pending because the editor was down.

### R4 — Reconcile the root README with current architecture and authoring policy

- **Priority:** P2
- **Affected:** `README.md`, links to active docs
- **Evidence:** README still describes monolithic architecture, CSV-first edits, obsolete audit path/date, and unconditional commit/push.
- **Risk:** New contributors follow unsafe or obsolete workflows.
- **Change:** Make README a concise entry point to current status, technical reference, autonomous tooling, DataTable policy, setup, validation, and no-pause/design policies.
- **Difficulty:** Low.
- **Validation:** Link/path check and review against descriptors/tool source.
- **Timing:** Completed 2026-07-12; README links and documented paths validated offline.

### R5 — Restore Graphify CLI discoverability

- **Priority:** P2
- **Affected:** developer environment/setup docs
- **Evidence:** `python -m graphify` fails because active `python` is Anaconda without the module, despite a populated `graphify-out` graph.
- **Risk:** Architecture work falls back to slower/incomplete grep and violates the intended workflow.
- **Change:** Document or script the correct interpreter/environment; add a diagnostic command.
- **Difficulty:** Low.
- **Validation:** `python -m graphify query ...` succeeds from repo root.
- **Timing:** Now/next; requires owner environment input if interpreter cannot be discovered locally.
