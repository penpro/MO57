# UI Consolidation Migration Plan

Status: Audit complete; Stages 0-3 implemented and validated; Stage 4 (skills/knowledge + quest collections) is next.

Implementation note (2026-07-13): Stages 0/1 corrected inspectability, validation authority, and binding lifecycle. Stage 2 moved stable IDs, entry storage, and selection preservation/clearing into one collection lifecycle while retaining concrete Blueprint-compatible adapters. Final gates passed: catalog 7/7, full automation 124/124, affected/generic WBPs 15/15, crafting/building PIE contracts, cold Escape 7/7, and the live CommonUI aggregate 79/79. Stage 3 queue presentation is next.

Implementation note (2026-07-20, Touch 030): Stage 3 is DONE. Queue presentation consolidated behind UMOQueueRendererBase/UMOQueueRowWidgetBase + the pure FMOQueueDisplayRow model; crafting/building are thin compat adapters (renderer emits cancel intents; adapters validate/execute; building gained a stable row id). Gates: headless 127/127 (+3 MOFramework.UI.Queue.*), WBP 15/15, cold Escape 7/7, live aggregate 84/84 (+5 Queue.*), queue PIE contract 6/6 (live progress, completion-as-removal, F18 menu-round-trip reconstruct). Survivor adapter deferred by plan; building cancel/refund fork logged for Wes.

## Target architecture

Do not create one universal menu or giant view-model interface. Consolidate the repeated lifecycle at three seams:

| Seam | Shared responsibility | Domain responsibility |
|---|---|---|
| Selectable collection | Entry creation/reuse, stable ID, selection, selection preservation, empty state, event | Query/filter/sort policy and typed entry presentation |
| Detail presenter | Common recipe material/skill/station layout where already proven | Skill, knowledge, quest, building placement, and action policy |
| Queue renderer | Row/empty/progress/current/cancel-intent presentation | Source translation, authority, cancel execution |

An optional catalog workspace may later compose catalog, detail, primary-action, and auxiliary/queue slots. It must not load DataTables, search actors, validate recipes, or execute gameplay.

## Stage 0 — Freeze behavior with content contract tests

- Add tests for row population, stable IDs, selecting unavailable recipes, selection-to-detail propagation, filters, selection preservation/clear rules, availability reasons, and one-click/one-intent cancellation.
- Add representative crafting, building, skills, quest, and survivor-task fixtures.
- Record Blueprint parent/referencer data through Asset Registry before changing classes or delegates.
- **Rollback boundary:** tests/docs only.

## Stage 1 — Correct state semantics and binding lifecycle

- Separate recipe-row selectability from craft/build action availability.
- Keep known unavailable recipes selectable and display the missing requirement/reason in details.
- Make audited queue button bindings idempotent and tear them down.
- Correct stale `UMOListEntryBase` documentation.
- Preserve all public class names and Blueprint delegates.
- **Validation:** unavailable recipe opens details; action is disabled with reason; repeated construct/destruct yields one cancel intent.
- **Rollback boundary:** small C++ behavior patch; no Blueprint reparenting.

## Stage 2 — Make recipe catalog selection authoritative

- Give every recipe/building row one valid base/catalog ID through the official bind path.
- Move population, entry storage, selected-ID state, and selection broadcast to one collection implementation.
- Convert concrete legacy events to compatibility forwarding shims; menus consume the authoritative event.
- Keep `UMORecipeListWidget` and `UMOBuildingRecipeListWidget` class names initially so existing WBPs remain safely parented.
- Consolidate their near-identical construction behind one recipe-catalog presenter/provider, parameterized by recipe kind and terminology/style.
- **Validation:** crafting/building retain row order/filter behavior; refresh preserves a visible selection; hidden/removed selection clears exactly once.
- **Status:** Complete. `FMOListSelectionModel` and `UMOScrollListBase` own this lifecycle; concrete recipe/building lists are compatibility adapters.

## Stage 3 — Consolidate queue presentation

- Introduce domain-neutral display rows: operation ID, title, icon, count, progress, remaining time, state, cancellability.
- A shared renderer owns widgets and visual state; adapters translate `UMOCraftingQueueComponent`, `UMOBuildProgressComponent`, and later `UMOSurvivorJobQueueComponent` events.
- Renderer emits cancel intents only. The domain adapter/controller validates and performs cancellation.
- Preserve queue WBP parents through thin compatibility subclasses until assets are migrated deliberately.
- **Validation:** initial rows, live progress, completion/removal, cancel-one/all, empty state, source swap/unbind, and reconstruct behavior.

## Stage 4 — Extract skills/knowledge and quest collections

- Move skills/knowledge list lifecycle out of `UMOSkillsPanel` while retaining specialized skill progress/flash entries.
- Introduce a knowledge-specific display model instead of forcing knowledge into `FMOSkillDisplayData`.
- Move quest list lifecycle out of `UMOQuestLogPanel`; retain objectives, tracking, abandonment, and subscriptions in quest-specific code.
- Do not require recipe amount/action/queue APIs.
- **Validation:** mode switching, filtering, XP updates, knowledge selection, quest status/objective changes, track/abandon.

## Stage 5 — Add optional workspace composition only if duplication remains

- After stages 2–4, measure remaining repeated composition.
- If justified, provide slots for header/filter, catalog, details, primary action, and auxiliary/queue content.
- Use it in crafting, building, skills, quests, and survivor tasks only where it removes verified lifecycle duplication.
- Possession may reuse collection primitives without the workspace. Inventory remains excluded.

## Stage 6 — Controller lifecycle and cleanup as a separate change set

- Centralize the shared push/cast/cache/failure rollback/close transaction in `UMOUIControllerBase`, leaving domain hooks explicit.
- Make `RegisterCachedMenu` idempotent and handle-based.
- Decide `Layer_Menu` versus `Layer_Game` from intended navigation/input behavior and update code or documentation with regression tests. Do not combine that policy change with catalog migration.
- Verify Asset Registry references for `UMOBuildWidget`, `UMOBuildingEntryWidget`, generic WBP scaffolding, and legacy delegates. Deprecate first; delete only in a later asset-aware cleanup.
- **Partial status:** The shared cache/stack/action-binding defects exposed by F21 are fixed and validated. Layer policy and asset deprecation remain separate.

## Per-stage release gates

1. User confirms Unreal Editor is closed before C++ compilation.
2. C++/UHT build succeeds.
3. Content contract and existing CommonUI shell tests pass.
4. PIE verifies behavior, focus, input restoration, layers, modal background, and controller/pawn changes.
5. Blueprint compile/referencer audit shows no broken parents or bindings.
6. Only after the user confirms compile/runtime success should the repository workflow's checkpoint commit/push occur.
