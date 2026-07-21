# MO57

MO57 is an ultra-realistic procedural open-world survival game built with Unreal Engine 5.8. Its core pillars are real-world simulation, emergent settlement growth, mutable voxel terrain, small-group cooperative play, and a framework designed for extension and modding.

The project is under active development. Start with [Project Status](Docs/PROJECT_STATUS.md) for the current implementation state and known-issue tracker rather than treating this README as a feature-completeness claim.

## Requirements

- Windows development environment
- Unreal Engine 5.8 source build
- Voxel Plugin open-source `dev-phy` build, vendored at `Plugins/Voxel`
- Rider for Unreal Engine is the primary IDE, although it is not required by the build
- Python 3 for repository utilities

The project descriptor declares `EngineAssociation: 5.8`. The default local engine location is `D:\UnrealEngine\UE_5.8`; set `UE_ENGINE_ROOT` when using another installation.

## Repository structure

| Path | Responsibility |
|---|---|
| `Source/MO57` | Project runtime module and game-specific entry points |
| `Plugins/MOFramework/Source/MOFrameworkCore` | Shared types, contracts, delegates, and policy-free runtime services |
| `Plugins/MOFramework/Source/MOFrameworkMedical` | Anatomy, vitals, metabolism, mental state, and medical simulation |
| `Plugins/MOFramework/Source/MOFramework` | Gameplay systems, persistence, UI, AI, PCG, voxel integration, and test support |
| `Plugins/MOFramework/Source/MOFrameworkEditor` | Editor-only toolbox and widget utilities |
| `Content/Python` | In-editor automation bridge, multi-frame test sequences, and scenario scripts |
| `Tools` | Unified Unreal CLI, DataTable utilities, build metadata, and content helpers |
| `Docs` | Active technical, design, policy, status, and audit documentation |

The Voxel Plugin API is intentionally isolated behind the `MOVoxel` facade. Code outside that facade should not include Voxel headers directly.

## Development setup

1. Clone the repository with its required plugin content.
2. Confirm that `MO57.uproject` resolves to the intended UE 5.8 installation.
3. Generate/open the project through Unreal or Rider.
4. Review [Project Status](Docs/PROJECT_STATUS.md), [Technical Reference](Docs/TECHNICAL_REFERENCE.md), and the relevant system-specific document before changing a subsystem.
5. Use the unified tooling entry point from the repository root:

```powershell
python Tools/ue.py status
```

To inspect the resolved project, engine, MCP, and bridge locations without starting Unreal:

```powershell
python Tools/ue.py config
```

`Tools/ue.py` discovers the repository, `.uproject`, engine association, and `.mcp.json` automatically. Optional overrides are:

| Environment variable | Purpose |
|---|---|
| `MO57_ROOT` | Repository root |
| `MO57_UPROJECT` | Explicit `.uproject` path |
| `UE_ENGINE_ROOT` | Unreal Engine installation root |
| `MO57_MCP_URL` | Unreal MCP streamable-HTTP endpoint |
| `MO57_BRIDGE_DIR` | Directory containing bridge command/output files |

## Build and validation

Before compiling, close Unreal Editor. Live Coding can block command-line builds. The unified CLI refuses a normal build while it detects the editor:

```powershell
python Tools/ue.py build
```

Useful validation commands include:

```powershell
# Offline tests for the local tooling
python -m unittest discover -s Tools/tests -p "test_*.py" -v

# Headless Unreal automation; editor must be closed
python Tools/ue.py auto

# Runtime regression suite; editor and bridge must be running
python Tools/ue.py test

# Two-client listen-server smoke test
python Tools/ue.py mptest
```

A successful compile proves type/build correctness, not gameplay correctness. Every behavior change should include a concrete reproduction and runtime verification appropriate to the affected system. Do not commit or push unless explicitly requested.

## Unreal MCP and autonomous tooling

The UE 5.8 experimental Model Context Protocol server is configured by `.mcp.json` and normally runs at `http://127.0.0.1:8000/mcp`. `Tools/ue.py` is the preferred MO57-facing orchestration layer for MCP asset operations, DataTables, editor lifecycle, builds, PIE, logs, and tests.

The MCP Programmatic toolset and `Content/Python/claude_bridge.py` can execute arbitrary local Python. They are trusted developer tooling:

- keep the MCP bound to loopback;
- never expose it directly to an untrusted network;
- never ship the Python bridge as a gameplay feature;
- prefer semantic project commands and readback-verified wrappers over arbitrary execution.

See [Autonomous Tooling](Docs/AUTONOMOUS_TOOLING.md) and [Agent PIE Testing](Docs/Agent_PIE_Testing.md) for the operating model.

## DataTable authoring

Do not edit DataTable CSV files directly.

- For live row authoring, use `python Tools/ue.py rows set <refPath> --file <rows.json>`. It deep-merges nested structures, reads changes back, and saves explicitly.
- For Unreal DataTable JSON exports, use `Tools/ue_json_utils.py`.
- If CSV work is unavoidable, use `Tools/ue_csv_utils.py`; add columns through its `add-column` command.

Raw MCP `set_rows` replaces nested structs. Omitting fields can reset them to defaults, which is why the safe wrapper must be used for partial updates.

## Architectural policies

- Diagnose the layer that creates incorrect state before proposing a fix.
- Centralize behavior shared by multiple systems rather than copying it.
- Treat defaults as project policy.
- Keep failure modes distinguishable where they originate.
- Preserve real simulation time while eliminating repetitive UX gestures through batching or pawn delegation.
- Input action handlers belong in `AMOPlayerController::SetupInputComponent()`.
- UI uses CommonUI and `UMOCommonButton`.
- The game never pauses. Read [Pause Policy](Docs/PAUSE_POLICY.md) before touching pause or input-mode behavior.

The complete mandatory engineering rules live in `AGENTS.md`.

## Documentation map

| Document | Use it for |
|---|---|
| [Project Status](Docs/PROJECT_STATUS.md) | Current progress, audit tracker, recent work, and known issues |
| [Technical Reference](Docs/TECHNICAL_REFERENCE.md) | Architecture patterns, APIs, networking, performance, and subsystem guidance |
| [Master Plan](Docs/MO57_Master_Plan.md) | Staged implementation plans |
| [Autonomous Tooling](Docs/AUTONOMOUS_TOOLING.md) | MCP, editor bridge, build, PIE, and validation workflow |
| [UI Overhaul Architecture](Docs/UI_Overhaul_Architecture.md) | CommonUI work and known pitfalls |
| [World Features Architecture](Docs/World_Features_Architecture.md) | Caves, rivers, POIs, resources, and player-built world features |
| [Voxel Plugin Reference](Docs/Voxel_Plugin_Reference.md) | Voxel integration, sculpting, caves, and mining |
| [Audit Session State](Docs/agent/SESSION_STATE.md) | Current repository/MCP audit state and exact continuation point |

Documentation under `Docs/Archive` is historical and should not guide new implementation.

## Current phase

The current focus is the solo survival foundation followed by pawn discovery, autonomous pawn jobs, settlement growth, cooperative multiplayer hardening, and broader modding support. Consult the project status and issue tracker for evidence-backed completion rather than the phase names alone.

## License

No repository license has been documented yet. Do not assume redistribution rights for the project or bundled third-party assets/plugins.
