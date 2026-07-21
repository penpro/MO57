# Architecture Findings

Status: In progress.

Findings will separate verified facts, reasonable inferences, and recommendations, and will cite concrete paths and symbols. Review focus: subsystem ownership, identity, persistence, authority/replication, tools, editor/runtime separation, initialization, delegates, data-driven systems, and MCP integration boundaries.

## Verified findings

### A1 — MCP is infrastructure, not MO57 domain architecture

Epic's experimental MCP and EditorToolset provide generic editor operations. MO57's domain-aware behavior is already concentrated in `Tools/ue.py`, project console commands, Python scenarios, `UMOEditorTestHelper`, and gameplay subsystems. New AI-facing capabilities should wrap those seams rather than reproduce inventory, persistence, crafting, medical, or interaction logic in Python toolsets.

### A2 — Runtime module has editor-only conditional dependencies

`MOFramework.Build.cs` conditionally adds UnrealEd/UMGEditor/ContentBrowser/AssetTools/ToolMenus when `Target.bBuildEditor`; `MOFrameworkEditor` also exists as a dedicated editor module. This is legal but creates a blurred ownership boundary. Existing editor utilities compiled inside the runtime module should be inventoried and migrated only when there is a concrete cook/dependency or maintenance benefit.

### A3 — Module split direction is sound but incomplete

`MOFrameworkCore` and `MOFrameworkMedical` encode explicit dependency rules, while the upper `MOFramework` module remains broad. This is an incremental production-oriented split, not grounds for a rewrite. Future extraction should follow stable ownership seams and preserve the current dependency direction.

### A4 — Tooling has a trusted-local security model

Both `Content/Python/claude_bridge.py` and EditorToolset's Programmatic tools can execute arbitrary Python. That is appropriate for a local developer loop, but any future packaged/runtime or remote MCP design must use a separate allowlisted semantic surface.
