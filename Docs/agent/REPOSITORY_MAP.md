# Repository Map

Status: Initial skeleton; evidence-backed expansion in progress.

## Confirmed entry points

| Area | Path | Current interpretation |
|---|---|---|
| Unreal project | `MO57.uproject` | Primary project descriptor; engine association pending verification. |
| Game source | `Source/MO57` | Project-level module. |
| Framework plugin | `Plugins/MOFramework` | Main custom plugin with runtime, core, medical, and editor modules. |
| MCP configuration | `.mcp.json` | Root MCP client/server configuration; contents pending inspection. |
| Developer tooling | `Tools` | Documented home of `ue.py` and DataTable utilities; inventory pending. |
| Documentation | `Docs` | Active project status, technical reference, plans, policies, and this audit. |
| Generated graph | `graphify-out` | Gitignored local AST knowledge graph; query before architecture grep. |

## Unreal/module boundaries

- `MO57` is the project runtime module and currently depends on Engine, Enhanced Input, AI, StateTree, UMG, and Slate.
- `MOFrameworkCore` is the lower-level contracts/services runtime module and explicitly avoids dependencies on gameplay, medical, UI, and persistence policy.
- `MOFrameworkMedical` is a runtime physiological-simulation layer depending on Core.
- `MOFramework` is the large upper runtime module containing gameplay, persistence, UI, AI, PCG, voxel integration, testing helpers, and editor-gated dependencies.
- `MOFrameworkEditor` is an editor-only toolbox loaded at `PostEngineInit`.
- The primary game and editor targets use `DefaultBuildSettings = V7`; their include-order setting remains named `Unreal5_7` under UE 5.8 and needs compatibility verification rather than assumption.

## Tooling boundaries

- `Tools/ue.py`: unified local orchestrator for bridge, MCP, build, PIE, DataTables, assets, automation, and multiplayer smoke.
- `Content/Python/claude_bridge.py`: fixed-file, Slate-tick command/Python executor.
- `Content/Python/claude_seq.py`: multi-frame sequence runner.
- `Content/Python/agent_test_lib.py` plus `test_*.py`: runtime/PIE scenarios.
- `Tools/ue_csv_utils.py`: mandatory CSV manipulation path.
- `Tools/ue_json_utils.py`: Unreal DataTable JSON export editing utility.
- `UMOEditorTestHelper`: PIE world selection, subsystem access, and multiplayer PIE configuration for Python.
- `UMODataImportCommandlet`: project data import/export commandlet; detailed safety/overlap review pending.
- `MOFrameworkEditor`: widget/editor utilities and toolbox commands; detailed inventory pending.
- `Private/Tests`: Unreal automation coverage for medical, core framework, and colony behavior.
- `Private/Testing`: console-driven UI test subsystem and commands.
- `MOCheatSubsystem`: broad runtime diagnostic and `MO.Test.*` semantic command surface; should be treated as an MCP reuse target, not duplicated.

## External engine integration

The MCP implementation is supplied by the UE 5.8 installation under `Engine/Plugins/Experimental/ModelContextProtocol` and `Engine/Plugins/Experimental/Toolsets/*`; the repository configures and orchestrates it but does not vendor its source.

## Follow-up areas

- Module dependencies and editor/runtime boundaries.
- Python bridge and MCP process ownership.
- Tests, automation, commandlets, console/debug tools, CI, content validation.
- Generated, stale, duplicated, or abandoned directories.

## Known generated/non-source areas

`Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`, `.vs`, and `Packaged` are build/runtime artifacts and are excluded from general source inventory. `graphify-out` is regenerable and gitignored.

No `.github`, GitLab, Azure Pipelines, or Jenkins entry point was found at repository root during the initial conventional-location check.
