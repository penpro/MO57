# UI Audit Session State

Updated: 2026-07-20 (America/Los_Angeles) — Stage 3 completed by Claude (Touch 030)

## Objective

Consolidate MO57's repeated catalog/detail/action/queue UI without forcing unrelated domains into a universal menu. Maintain a durable, append-only record so work survives context and token boundaries.

## Current outcome

- The deep audit is complete and recorded in `UI_SYSTEM_MAP.md`, `CONSOLIDATION_FINDINGS.md`, and `MIGRATION_PLAN.md`.
- Migration Stages 0/1 are implemented and validated: inspectable unavailable rows, authoritative crafting validation, corrected show-all-known behavior, and reconstruct-safe queue bindings.
- Migration Stage 2's state boundary is implemented and validated: `UMOScrollListBase` owns stable IDs, entry storage, and one authoritative selection lifecycle through `FMOListSelectionModel`.
- Crafting and both building workspaces consume the list's selected ID instead of retaining parallel menu state. Existing concrete C++ class names, Blueprint parents, bindings, and compatibility delegates remain intact.
- F21 is resolved. The console UI suite is frame-stepped, waits for actual stack activation, and reconciles to a clean state between tests.
- Shared CommonUI lifecycle defects uncovered by F21 were fixed at their source:
  - covered/suspended widgets no longer lose controller cache ownership;
  - same-frame close-old/push-new transitions are reconciled centrally on the menu layer;
  - pooled activatable widgets unregister their global close-action binding before rebinding.
- Controlled retirement pass completed: `UMODetailPanelBase`, `UMOBuildWidget`, and `UMOBuildingEntryWidget` are compatibility-safe deprecated; seven zero-referencer legacy/generic assets are recorded for later deletion; active native bases and concrete adapters remain intact.
- No Widget Blueprint or DataTable assets were edited or saved by this UI pass.
- No files were staged, committed, or pushed.

## Architecture decisions

- Reuse three composable seams: selectable collection, domain-appropriate details, and queue renderer/adapters.
- Keep recipe/building detail reuse in `UMORecipeDetailPanelBase`; do not force skill/quest details into recipe semantics.
- Keep inventory outside the catalog consolidation because its GUID identity, grids, drag/drop, equipment, and transfer behavior are materially different.
- Preserve CommonUI navigation and input handling in C++; presentation assets remain Blueprint-compatible adapters during migration.
- Treat CommonUI deactivation as ambiguous: it can mean stack coverage/suspension rather than closure. Top-of-owning-stack status determines whether a cached menu is currently open.
- Reconcile asynchronous stack transitions centrally instead of adding per-menu timing workarounds.

## Validation evidence

- Shared-tree `MO57Editor` build: 23/23 actions passed for Stage 2 and the frame-stepped runner.
- Final incremental `MO57Editor` build after lifecycle fixes: 5/5 actions passed.
- `MOFramework.UI.Catalog`: 7 passed, 0 failed, 0 not run.
- Full `MOFramework`: 124 passed, 0 failed, 0 not run.
- Affected/generic Widget Blueprints: 15/15 compiled with warnings treated as errors and retained expected native parents.
- Crafting PIE lifecycle: selection preservation/clear/restore passed; population changed from 4 context rows to 7 known rows, with 4 unavailable rows inspectable.
- Building PIE lifecycle: 13 rows populated; 6 material-incomplete rows remained inspectable; selection lifecycle passed.
- Cold Escape-close suite: 7/7 passed.
- Definitive live aggregate: 79 passed, 0 failed, result count matched the 79 registered tests.
- Post-suite diagnostics: menu layer 0, active menu count 0, any menu open No, move input ignored No, look input ignored No, and no active `UMOActivatableWidget` menu instances.
- Final Graphify refresh: 10,830 nodes, 15,846 edges, 884 communities.
- Scoped text diff checks and Python validation-script compilation pass at handoff.

## Known boundaries and remaining work

- Stage 3 is DONE (Touch 030): queue presentation consolidated behind `UMOQueueRendererBase`/`UMOQueueRowWidgetBase` + the pure `FMOQueueDisplayRow` model; crafting and building are thin compat adapters (cancel executes at the adapter boundary, renderer emits intents only). Gates: headless 127/127, WBP 15/15, cold Escape 7/7, live aggregate 84/84, queue PIE contract 6/6. Deferred by design: the survivor-queue adapter (the contract admits it — FGuid ids, pull progress, completion-as-removal); the building cancel/refund-semantics fork (queue path full-refund-world-drops vs ghost-menu skill-partial — Wes's call); wiring the shared renderer into a LIVE building surface (the building queue widget remains dormant).
- Stage 4 should extract skills/knowledge and quest collection lifecycle while preserving their specialized presenters and subscriptions.
- Optional workspace composition should wait until Stages 3/4 quantify what duplication remains.
- Layer policy (`Layer_Menu` versus the unused `Layer_Game`) remains a separate design decision.
- `ActionUnavailableText` is optional in C++; existing crafting detail assets need the named optional widget only if direct unavailable-reason text should render there.
- Seven generic/legacy list/detail Widget Blueprints report zero Asset Registry referencers and are cleared in `RETIREMENT_MANIFEST.md` for a later explicit asset-deletion pass.

## Exact resume point

1. Read `TOUCH_LOG.md` from the newest entry (Touch 030) and check `claude-codex-coop.md` for fresh claims.
2. Begin Migration Stage 4: extract skills/knowledge and quest collection lifecycle (see MIGRATION_PLAN.md Stage 4) — a Graphify trace of `UMOSkillsPanel` and `UMOQuestLogPanel` data/event flows first.
3. Alternatively resolve the deferred Stage-3 forks: survivor-queue adapter over `UMOQueueRendererBase`, the building cancel/refund-semantics decision (needs Wes), or wiring the shared renderer into a live building surface.
4. Keep appending each coherent inspection, edit, and validation unit to `TOUCH_LOG.md`. Gate numbers are now: headless 127, live aggregate 84, catalog+queue headless 10, WBP 15, cold Escape 7.
