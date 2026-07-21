# UI Audit Touch Log

Append-only chronological evidence log. Add an entry whenever a source, configuration, Blueprint-support script, test, or active UI document is inspected or changed.

## 2026-07-12 — Touch 001: controlling documentation

- **Touched:** `Docs/PROJECT_STATUS.md`, `Docs/TECHNICAL_REFERENCE.md`, `Docs/UI_Overhaul_Architecture.md`, `Docs/MO57_Master_Plan.md`, repository audit state.
- **Purpose:** Establish intended UI architecture and prior consolidation progress.
- **Evidence:** Generic bases and CommonUI foundation were planned/created; Stages 4A-7A were documented as pending. The plan explicitly names recipe/building/skill/queue entries and lists as migration candidates.
- **Conclusion:** Treat plan status as a hypothesis. Verify current source inheritance and behavior before recommending or implementing migration.
- **Changes:** Dedicated audit documentation only; no product UI changes.
- **Next touch:** Current UI class/inheritance inventory.

## 2026-07-12 — Touch 002: UI inheritance inventory

- **Touched:** All `Plugins/MOFramework/Source/MOFramework/Public/*.h` class declarations; UI-related source filename inventory.
- **Purpose:** Verify whether the planned generic-base migration actually happened.
- **Evidence:** `UMORecipeEntryWidget` and `UMOBuildingRecipeEntryWidget` inherit `UMOListEntryBase`; `UMORecipeListWidget` and `UMOBuildingRecipeListWidget` inherit `UMOScrollListBase`; `UMOHarvestProgressWidget` and `UMOInspectionProgressWidget` inherit `UMOProgressWidgetBase`; confirmation/text-input classes use `UMOConfirmationBase`. In contrast, `UMOSkillEntryWidget`, crafting/building queue widgets and entries, character/job/task/quest entries, inventory grids/slots, and several panels still inherit directly from `UUserWidget` or another one-off base.
- **Conclusion:** The status documents are stale: migration is partially complete. The clearest consolidation seam currently covers crafting/building recipe catalogs, while skills and queues were not migrated.
- **Graphify:** Required architecture query attempted first and failed because the active Python still lacks the `graphify` module.
- **Changes:** Audit documentation only.
- **Next touch:** Read all generic base implementations and determine whether concrete subclasses reuse behavior or merely inherit type identity.

## 2026-07-12 — Touch 003: generic UI foundations

- **Touched:** `MOListEntryBase.h/.cpp`, `MOScrollListBase.h/.cpp`, `MODetailPanelBase.h/.cpp`, `MORecipeDetailPanelBase.h/.cpp`, `MOProgressWidgetBase.h/.cpp`, `MOConfirmationBase.h/.cpp`, `MOActivatableWidget`, `MOMenuWidget`, `MOMenuWidgetBase`, `MOUIControllerBase`, `MOGameUIManagerSubsystem`, `MOPrimaryGameLayout`.
- **Purpose:** Determine which abstractions carry reusable behavior and which are unused scaffolding.
- **Evidence:** List/entry bases own real creation, binding, selection, enabled state, and visual lifecycle and have exactly two concrete list families (crafting recipe and building recipe). `UMORecipeDetailPanelBase` independently owns the richer shared recipe/building detail workflow. `UMODetailPanelBase` has zero subclasses or consumers; its `SetActionText()` is a no-op and its soft icon helper loads synchronously. Progress and confirmation bases own substantial shared lifecycle and are actively used.
- **Conclusion:** Do not make `UMORecipeDetailPanelBase` inherit the unused generic detail base merely for consistency. The generic detail base is currently aspirational/dead scaffolding, while the recipe-specific base is the proven abstraction. Any consolidation should evolve the proven domain base or replace the unused generic base with a presentation contract only after concrete consumers exist.
- **Additional observations:** `UMOScrollListBase` documentation claims filtering but implements no filter API; it recreates all entry widgets on population and performs linear ID lookup. Those are truthful-scope/performance issues, not reasons to discard the base.
- **Changes:** Audit documentation only.
- **Next touch:** End-to-end crafting catalog flow.

## 2026-07-12 — Touch 004: crafting catalog and queue flow

- **Touched:** `MOCraftingUIController`, `MOCraftingMenu`, `MORecipeListWidget`, `MORecipeEntryWidget`, `MORecipeDetailPanel`, `MORecipeDetailPanelBase`, `MOCraftingQueueWidget`, `MOCraftingQueueEntryWidget` headers/implementations and delegate/filter callsites.
- **Flow verified:** Controller pushes `UMOCraftingMenu` to `Layer_Menu`; menu owns data-source references and filtering, composes recipe list/detail/queue, routes selection into details, and executes through `UMOCraftingQueueComponent` or `UMOCraftingSubsystem`.
- **Evidence:** Although `UMORecipeListWidget` inherits `UMOScrollListBase`, it reimplements population, clearing, entry storage, selection, and click routing using parallel private state. `UMORecipeEntryWidget::SetupEntry()` never calls `SetEntryId`, so the inherited generic ID remains `NAME_None`; the concrete list listens to the legacy `OnEntryClicked`, not inherited `OnEntrySelected`. Menu code also listens to legacy `OnRecipeSelected` rather than standard `OnRecipeSelection`.
- **Conclusion:** The recipe list/entry migration is nominal, not behavioral. The generic base currently removes almost no concrete duplication.
- **Additional evidence:** Filter state exists in both menu and list, but list setters have no consumers and do not apply filters; the menu owns the real query/filter policy. Crafting queue owns a useful component binding but duplicates entry creation/progress/empty/cancel presentation later found in building.
- **Changes:** Audit documentation only.
- **Next touch:** Compare building catalog, detail, and queue flow against crafting.

## 2026-07-12 — Touch 005: building catalog, ghost, and queue flow

- **Touched:** `MOBuildingUIController`, `MOBuildingMenu`, `MOBuildingRecipeListWidget`, `MOBuildingRecipeEntryWidget`, `MOBuildingDetailPanel`, `MOBuildingQueueWidget`, `MOBuildingQueueEntryWidget`, `MOBuildWidget`, `MOBuildingEntryWidget`, plus consumers and active UI docs.
- **Flow verified:** Controller pushes `UMOBuildingMenu` to `Layer_Menu`; menu filters building recipes, composes list/detail, and broadcasts a placement request. After placement, the controller uses `UMOGhostContextMenu`, not `UMOBuildWidget`, for build configuration/deposit/progress.
- **Evidence:** Craft/build recipe entry implementations differ almost entirely by names (`bCanCraft` versus `bCanBuild`); source diff is 22 insertions/23 deletions. Recipe list implementations show the same pattern (59/59 line substitutions). Crafting/building queue widgets are also near-parallel (115/119 substitutions), as are queue entries.
- **Orphan candidates:** `UMOBuildWidget` and `UMOBuildingEntryWidget` have no C++/config/Python consumers beyond their own definitions/includes; live flow uses `UMOBuildingMenu`, `UMOBuildingRecipeEntryWidget`, and `UMOGhostContextMenu`. Blueprint asset references must be checked before any deprecation/deletion.
- **Conclusion:** This is confirmed structural duplication, not merely similar visuals. Crafting/building catalogs should share one recipe-catalog list/entry implementation and parameterize terminology/style/policy. Queue presentation should share a base/view-model adapter while retaining different domain component adapters.
- **Layer discrepancy:** Active controllers push crafting/building menus to `Layer_Menu`, while planning docs describe gameplay menus on `Layer_Game`. This requires a deliberate policy decision and runtime focus/input verification, not a blind move.
- **Changes:** Audit documentation only.
- **Next touch:** Skills panel end-to-end, then compare against the catalog pattern.

## 2026-07-12 — Touch 006: skills and knowledge workspace

- **Touched:** `MOCharacterUIController`, `MOSkillsPanel`, `MOSkillEntryWidget`, skills/knowledge component event bindings, and UIManager forwarding callsites.
- **Flow verified:** Controller pushes `UMOSkillsPanel` to `Layer_Menu`; panel owns two data modes (skills/knowledge), filtering, sorting/population, selection, embedded detail fields, and component subscriptions. Entries own skill rendering/progress/flash animation and use legacy `UButton`.
- **Evidence:** The panel directly combines catalog shell, provider/query policy, entry factory, selection, detail presentation, mode tabs, and live subscription. `UMOSkillEntryWidget` does not use `UMOListEntryBase`. The panel has no separate detail subwidget, so the same left-list/detail layout is hardwired into this one class.
- **Conclusion:** Skills can share a catalog shell and selection lifecycle, but should not be forced into recipe-specific amount/action/queue behavior. Its provider supplies `FMOSkillDisplayData` and detail content; the skill entry retains progress/flash specialization. Knowledge should get its own display model/provider rather than masquerading as skill data long-term.
- **Changes:** Audit documentation only.
- **Next touch:** Quest, possession, survivor task, and colony list/detail surfaces to test the breadth of a catalog-workspace abstraction.

## 2026-07-12 — Touch 007: quest, survivor-task, possession, and colony surfaces

- **Touched:** `MOQuestLogPanel/Entry`, `MOSurvivorTaskMenu/TaskEntry/JobEntry`, `MOPossessionMenu/PawnEntryWidget`, `MOColonyOverviewWidget/ColonyPortrait`, related controller callsites.
- **Quest evidence:** `UMOQuestLogPanel` is another complete left-list/right-detail/action workspace. It directly owns entry creation, selected ID, detail/objective population, track/abandon actions, and subsystem binding; it does not reuse generic list/detail primitives.
- **Survivor-task evidence:** `UMOSurvivorTaskMenu` explicitly mirrors crafting layout: available-task list on the left and live job queue on the right. It reimplements both entry factories and queue refresh/cancel wiring rather than sharing catalog/queue presentation.
- **Possession evidence:** `UMOPossessionMenu` is list + per-row action without a selected detail panel. It can reuse lower-level collection/entry lifecycle but should not be forced into a three-pane workspace.
- **Colony evidence:** `UMOColonyOverviewWidget` is a testable V1 prototype that constructs rows in C++ and directly performs nearest-station/storage queries plus job enqueueing. This mixes presentation, world query, and domain command policy and is not the target model for the final colony UI.
- **Conclusion:** A composed workspace abstraction is justified across crafting, building, skills, and quests; survivor task uses catalog + queue slots. The abstraction must be compositional/optional, not a monolithic base requiring every menu to have details and actions.
- **Changes:** Audit documentation only.
- **Next touch:** Unified inventory/item details and controller/menu routing consistency.

## 2026-07-12 — Touch 008: inventory composition and menu routing

- **Touched:** `MOUnifiedInventoryMenu`, `MOInventoryMenu`, `MOInventoryGrid`, `MOInventorySlot`, `MOItemInfoPanel`, `MONearbyItemsPanel`, `MOEquipmentPanel`, all specialized controller `PushWidgetToLayer` callsites, and `UMOUIManagerComponent::CloseAllSwitchableMenus`/in-game toggle orchestration.
- **Inventory evidence:** Unified inventory is a distinct drag/drop grid and tabbed workspace with two inventories, item details, nearby items, and equipment. It is large (~1,000 cpp lines), but its collection semantics, GUID identity, drag/drop, equipment slots, and transfer actions do not match a simple catalog list.
- **Conclusion:** Do not force inventory grids into the catalog abstraction. Reuse smaller presentation/focus/action conventions where useful; audit the inventory monolith separately after catalog consolidation.
- **Routing evidence:** Crafting, building, skills, status, quest, inventory, possession, survivor-task, and in-game menus are all pushed to `Layer_Menu`. `Layer_Game` is not used by these controller paths despite docs labeling it for switchable gameplay menus. Progress widgets use `Layer_GameOverlay`; confirmation uses `Layer_Modal`.
- **Additional drift:** `CloseAllSwitchableMenus` still calls the in-game menu a “pause” surface, contradicting the no-pause policy.
- **Conclusion:** Layer policy must be documented from current desired behavior. Either migrate switchable gameplay workspaces to `Layer_Game` with regression tests or update layer documentation/remove the unused distinction. Do not mix this policy migration into the first catalog refactor.
- **Changes:** Audit documentation only.
- **Next touch:** Blueprint assets/support scripts and UI tests to assess migration safety and current coverage.

## 2026-07-12 — Touch 009: Widget Blueprint assets, redirectors, automation scripts, and UI tests

- **Touched:** UI asset filename inventory, binary parent/reference strings for representative WBPs, `create_generic_widgets.py`, configuration/validation scripts, `MOUITestSubsystem` and console test registrations.
- **Asset evidence:** Concrete crafting/building assets are parented to their concrete C++ classes (`WBP_CraftingRecipeList → UMORecipeListWidget`, `WBP_BuildingRecipeList → UMOBuildingRecipeListWidget`, etc.). Generic `WBP_MOScrollList`, `WBP_MOListEntry`, and `WBP_MODetailPanel` are referenced only by themselves in the scanned assets; no concrete menu composes them. `BP_MOPlayerController` references concrete menu/list assets.
- **Redirector evidence:** Several apparent duplicate root assets are redirectors (for example root `WBP_CraftingQueue`, `WBP_InventoryMenu`, and `WBP_InventoryGrid`) pointing toward organized folders; do not count them as independent implementations or delete silently.
- **Test evidence:** UI tests cover opening/closing/toggling, layer setup, input state, focus restoration, nested modals/context menus, switching, and stress. No tests were found for recipe/skill/quest list population, selection-to-detail propagation, filtering, action enablement, queue row mapping, or cancellation routing.
- **Conclusion:** Existing tests protect CommonUI shell behavior but not the content workspace contract being refactored. Add content-level contract tests before migration. Generic WBPs are scaffolding, not evidence that consolidation is operational.
- **Changes:** Audit documentation only.
- **Next touch:** Quantify delegate/lifecycle duplication and formulate the smallest production-ready target architecture.

## 2026-07-12 — Touch 010: selection semantics, delegates, binding lifecycle, and controller transactions

- **Touched:** `MOListEntryBase`, recipe/building recipe entries and lists, crafting/building queue widgets and entries, `MOUIDelegates`, `MOUIControllerBase`, and specialized controller open/close callsites.
- **Selection evidence:** Craft/build availability is passed to `SetEntryEnabled`, which disables the selection button. Known-but-unavailable recipes therefore cannot reliably open their detail/reason view. Selectability and executability are incorrectly represented as one state.
- **Delegate evidence:** Concrete entries/lists use legacy domain delegates while inherited generic selection remains parallel. Base `EntryId` is never set by recipe setup, making the inherited event invalid (`NAME_None`). Header documentation incorrectly claims programmatic `SetSelected()` broadcasts.
- **Binding evidence:** Crafting/building queue widgets and entries add cancel-button callbacks during `NativeConstruct()` without first removing them or removing them in `NativeDestruct()`. Component delegates are cleaned up, but button delegates are not.
- **Controller evidence:** Controllers repeat the same menu-open transaction around shared base primitives. `RegisterCachedMenu` adds an untracked deactivation lambda each time it is called, so repeated registration can accumulate callbacks.
- **Diagnosis:** The prior consolidation added base types without transferring ownership of the state machine. That created two APIs and two stores instead of one reusable mechanism.
- **Direction:** Correct state semantics first, then make one collection lifecycle authoritative, then consolidate queue presentation through adapters. Centralize controller lifecycle separately so catalog work does not also change routing policy.
- **Changes:** Audit documentation only.
- **Next touch:** Finalize staged migration and continuity handoff.

## 2026-07-12 — Touch 011: audit synthesis and persistent handoff

- **Touched:** `Docs/agent/ui/UI_SYSTEM_MAP.md`, `CONSOLIDATION_FINDINGS.md`, `MIGRATION_PLAN.md`, and UI audit session state.
- **Purpose:** Convert source evidence into a production-safe target boundary and ordered implementation plan.
- **Conclusion:** Use composable collection/detail/queue seams, not a universal menu superclass. First implementation should be Stage 0 tests followed by Stage 1 state/binding correctness; recipe catalog ownership comes next. Inventory and layer policy remain separate projects.
- **Changes:** Documentation only; no C++, Blueprint, asset, config, build, or runtime mutation.
- **Next touch:** When implementation is authorized, begin at Migration Stage 0 and append every inspected/changed/tested artifact here before moving stages.

## 2026-07-12 — Touch 012: Stage 0/1 inspectable-row contract and authoritative availability

- **Touched:** `MOUIInteractionState.h`, `MOUIInteractionStateTests.cpp`, recipe/building recipe entry implementations, `MOListEntryBase.h`, recipe list/detail/menu flow, and `MOCraftingSubsystem` validation callsites.
- **Cause confirmed:** Recipe rows used immediate action availability as button selectability. Crafting list/detail also recomputed incomplete subsets of validation, omitting station/tool constraints already represented by `UMOCraftingSubsystem::CanCraftRecipe`.
- **Implementation:** Added a tested inspectable-entry interaction contract; recipe/building setup now initializes the inherited stable ID and keeps the row selectable while retaining unavailable visuals. Crafting list and detail action consume centralized validation with knowledge, skills, inventory, and current station. The detail base exposes an optional unavailable-reason surface.
- **Additional correction:** “Show All Known” previously passed `Station=None` to a query where `None` means hand crafting, so station recipes were excluded. It now intentionally queries the full craftable catalog, applies unlock availability, and lets current-context validation mark/explain non-executable entries.
- **Compatibility:** Existing C++ class names, Blueprint parents, and legacy delegates are preserved. `ActionUnavailableText` is `BindWidgetOptional`; existing assets remain valid, but adding that named text widget is required to render the consolidated reason directly.
- **Validation:** Static diff check passed. Two automation contracts were added but not executed because no build has been authorized yet.
- **Next touch:** Authoritative queue validation and reconstruct-safe bindings.

## 2026-07-12 — Touch 013: authoritative queue gate and binding lifecycle

- **Touched:** `MOCraftingQueueComponent`, crafting/building queue widgets and queue-entry widgets.
- **Cause confirmed:** `EnqueueCraft` consumed ingredients without calling the existing centralized validator, so a direct/RPC request could bypass station, knowledge, skill, and required-tool checks. Queue cancel buttons also added callbacks on every construct without teardown.
- **Implementation:** The server-authoritative enqueue boundary now fails closed through `CanCraftRecipe` before consumption. All four cancel/cancel-all widget paths remove before add and remove during `NativeDestruct`.
- **Validation:** Binding scan confirms every audited add is preceded by the matching removal. Editor status is DOWN. C++/UHT build and automation/PIE remain intentionally pending the required user compile confirmation.
- **Graphify:** Both the pre-edit query and required post-edit `python -m graphify update .` were attempted; both failed because the active Python environment lacks the `graphify` module.
- **Next touch:** Compile/UHT, automation tests, Blueprint compile/referencer audit, and PIE reproduction after user confirmation.

## 2026-07-12 — Touch 014: C++/UHT and headless automation gates

- **Touched:** `MO57Editor` build, `MOUIInteractionStateTests`, full MOFramework automation.
- **Results:** Initial editor build succeeded (26 actions). Focused catalog contracts passed 2/2. Full framework automation passed 119/119.
- **Environment:** The first sandboxed build attempt was denied access to the normal UBT log directory; the approved build rerun succeeded.
- **Changes:** No new product changes from this gate.
- **Next touch:** Live Blueprint compilation and Asset Registry references.

## 2026-07-12 — Touch 015: live Blueprint, Asset Registry, bridge, and MCP validation

- **Touched:** `Tools/validate_ui_stage1.py`; 12 organized crafting/building Widget Blueprints; legacy root recipe/queue assets; live Epic MCP server.
- **Results:** All 12 affected WBPs compiled with warnings-as-errors and retained expected native parents. Four legacy root assets each had zero Asset Registry referencers. MCP `SceneTools.get_current_level` succeeded. The bridge remained correlated and operational.
- **Note:** An initial validation-script attempt queried a non-exposed WidgetBlueprint property after all 12 compiles had already passed; the unsupported optional inspection was removed and the complete script reran successfully.
- **Changes:** Added a reusable read-only live validation script; no assets were saved or reparented.
- **Next touch:** PIE behavior contracts.

## 2026-07-12 — Touch 016: scoped PIE catalog contracts

- **Touched:** `Tools/validate_ui_stage1_pie.py`, live crafting/building menus and their list/entry/detail objects.
- **Crafting result:** PASS. Unavailable rows stayed selectable; inherited/domain IDs matched; selection reached the detail panel; action remained disabled with a nonempty reason; show-all-known added cross-station rows (final run 6 to 9).
- **Building result:** PASS. All 13 rows loaded; 6 material-incomplete rows stayed selectable; selection reached details; placement remained permitted before materials were deposited.
- **Authoritative queue result:** A direct under-qualified enqueue was rejected at the queue boundary before consumption, proving the new gate was live.
- **Changes:** Added a reusable PIE validation script; no content assets changed.
- **Next touch:** Repair the regression helper exposed by the authoritative gate and rerun all gates.

## 2026-07-12 — Touch 017: regression-helper correction and final rerun

- **Touched:** `MOCheatSubsystem::RunCraftTest`, second `MO57Editor` build, focused/full automation, Blueprint validation, MCP smoke, PIE contracts, gameplay RunAll, and console UI aggregate.
- **Diagnosis:** The craft regression helper granted ingredients and the primary required skill but omitted discovery/knowledge gates. The queue previously bypassed these requirements; authoritative validation correctly exposed the stale helper.
- **Implementation:** Test setup now derives required skill, discovery level, required knowledge, and station from the recipe row before enqueueing.
- **Final results:** Rebuild succeeded; focused automation 2/2; full automation 119/119; Blueprint 12/12; MCP smoke passed; crafting/building PIE contracts passed; gameplay Craft/DropPickup/Attack passed.
- **Unrelated gates:** Gameplay RunAll remains red only for two existing data issues: two dangling recipe references and an unconfigured medical-treatment table.
- **Harness finding:** `MO.UI.RunAllTests` ran all tests in one frame and collided with deliberate same-frame debounce (17/79). Individual frame-separated crafting/building tests passed. Logged as F21; no product code was changed to accommodate an invalid runner.
- **Shutdown:** PIE ended and the editor instance started for validation was closed.
- **Graphify:** Required update attempted and failed (`No module named graphify`).
- **Next touch:** Migration Stage 2 authoritative recipe collection/selection lifecycle.

## 2026-07-12 — Touch 018: pre-overnight editor-only planning snapshot

- **Touched:** Generic foundation WBPs, affected crafting/building WBPs, legacy/generic Asset Registry referencers, bridge/MCP health, and `Tools/validate_ui_stage1.py`.
- **Purpose:** Complete every editor-only fact needed to construct an overnight work queue while leaving later planning/headless work free of editor contention.
- **Results:** 15/15 Widget Blueprints compiled with warnings-as-errors and retained expected native parents. Legacy root recipe/queue assets and generic `WBP_MOListEntry`, `WBP_MOScrollList`, and `WBP_MODetailPanel` each had zero Asset Registry referencers. MCP returned the current loading level successfully.
- **Changes:** Expanded the reusable validation script to cover generic foundation assets and their referencers. No assets were saved, reparented, or edited.
- **Shutdown:** Unreal Editor was closed immediately after the snapshot.
- **Next touch:** Static/headless overnight backlog sizing and ordering.

## 2026-07-12 — Touch 019: Claude/Codex coordination channel

- **Touched:** Root `claude-codex-coop.md`.
- **Purpose:** Provide an append-only, discoverable inter-agent channel for tool discovery, file claims, questions, concerns, and handoffs.
- **Seeded questions:** Exact working Graphify interpreter/environment; other undocumented wrappers/tools; active file claims; conflicts with UI Stage 2.
- **Seeded handoff:** Stage 0/1 validation outcome, remaining data gates, invalid synchronous UI aggregate, and zero-referencer generic/legacy assets.
- **Next touch:** Read and acknowledge Claude's appended responses before planning or claiming overnight work.

## 2026-07-12 — Touch 020: Claude response and Graphify recovery

- **Touched:** `claude-codex-coop.md`, Graphify CLI, root/UI session state.
- **Claude response:** Graphify is an editable install under the Windows Store Python 3.13, not Anaconda. Claude also disclosed current tool entry points, a committed crafting-queue timing/persistence overlap, and a proposed H23 harvest refactor.
- **Coordination:** Codex claimed only Stage 2 catalog/list/menu files and released `MOCraftingUIController`, `MOHarvestProgressWidget`, and `MOHarvestSubsystem` for H23, subject to announced scope. Claude may own M21 if the user selects it.
- **Graphify validation:** Direct query succeeded. Required update succeeded: 10,742 nodes, 15,724 edges, 871 communities. `graph.html` was correctly skipped due to the 5,000-node visualization limit.
- **Next touch:** Use Graphify first for Stage 2 architecture tracing and check the cooperation log before every new file claim.

## 2026-07-12 - Touch 021: authoritative catalog collection and selection lifecycle

- **Touched:** `MOListSelectionModel.h`, `MOScrollListBase.*`, crafting/building recipe list adapters, `MOCraftingMenu.*`, `MOBuildingMenu.*`, `MOBuildWidget.*`, and catalog automation tests.
- **Diagnosis:** Generic list inheritance was present, but each recipe list retained its own widget array and selected ID, while crafting and the older building workspace retained an additional menu-level selected ID. Repopulation destroyed list selection and callers reconstructed it manually, allowing redundant selection broadcasts and stale details after filtering.
- **Implementation:** Added one pure ID/selection model to the generic scroll-list base. It de-duplicates stable IDs, preserves a still-valid selection across repopulation without emitting another selection event, clears a removed selection exactly once, and rejects unknown IDs by clearing. The concrete recipe lists now configure/refresh typed entries through base hooks and forward compatibility delegates only. All live workspace consumers read the selected ID from the list and clear details when selection disappears.
- **Compatibility:** Existing concrete C++ class names, Blueprint parents, widget bindings, entry data structs, and legacy delegates remain intact. No assets were reparented or saved.
- **Tests added:** Stable unique IDs, idempotent selection, selection preservation across refresh/reorder, and exactly-one clear transition when a selected item disappears.
- **Static validation:** Scoped `git diff --check` passed with line-ending notices only; scans found no remaining private recipe arrays or menu-level selected-recipe members in the migrated consumers.
- **Next touch:** C++/UHT build and focused/full automation, followed by Blueprint and PIE validation.

## 2026-07-12 - Touch 022: frame-stepped UI batch runner

- **Touched:** `MOUITestSubsystem.h/.cpp`, `MOUITestConsoleCommands.cpp`.
- **Diagnosis:** `MO.UI.RunAllTests` and every patterned batch command executed all registered tests synchronously inside one console-command frame. The menu controllers intentionally reject a second toggle in the same `GFrameCounter`, so batch cleanup/open operations collided even though the same tests passed when invoked on separate frames.
- **Implementation:** Added deterministic asynchronous batch orchestration that closes to a clean state, executes exactly one registered test per frame, closes between tests, persists the final summary, and broadcasts the existing completion delegate. All batch console commands now start this runner and reject overlapping runs. Single-test execution remains synchronous and unchanged; legacy synchronous batch methods remain for compatibility but are no longer used by supported console entry points.
- **Product behavior:** No controller debounce, input routing, layer policy, or menu behavior changed to accommodate the harness.
- **Static validation:** Scoped `git diff --check` passed with line-ending notices only. Build and runtime aggregate validation are pending a coordinated editor release.
- **Graphify:** Post-code graph refresh completed at 10,773 nodes, 15,784 edges, and 910 communities before this isolated harness edit; another refresh is required after final code validation.
- **Next touch:** Coordinated shared-tree build, automation, live WBP compilation, catalog PIE contracts, and the repaired aggregate runner.

## 2026-07-12 - Touch 023: Stage 2 runtime lifecycle contract

- **Touched:** `Tools/validate_ui_stage1_pie.py`.
- **Purpose:** Extend the existing crafting/building PIE contract beyond inspectable unavailable rows to the authoritative Stage 2 lifecycle.
- **Added checks:** Reordering/repopulating the same catalog preserves the selected ID and displayed details; removing the selected ID clears both list selection and detail content; restoring the catalog allows the same entry to be selected again. The prior availability, stable-ID, show-all-known, and placement checks remain.
- **Changes:** Validation tooling only; no product behavior or assets changed.
- **Next touch:** Execute after the coordinated C++/UHT build and WBP compile gate.

## 2026-07-12 - Touch 024: shared-tree compile and headless automation

- **Touched:** `MO57Editor` build, focused catalog automation, full MOFramework automation.
- **Build:** Passed 23/23 actions, including all migrated catalog/menu files, the new selection model/tests, and the frame-stepped test subsystem.
- **Focused automation:** `MOFramework.UI.Catalog` passed 7/7 (the two prior interaction contracts plus five authoritative selection-model contracts).
- **Full automation:** `MOFramework` passed 124/124 with zero failures and zero not-run tests.
- **Coordination:** The live editor had zero dirty content/map packages and no PIE before Codex took the build window. Build and headless results were posted to the Claude/Codex cooperation log.
- **Git:** No staging, commit, or push performed; user confirmation/authorization remains required.
- **Next touch:** Live WBP compile, expanded catalog PIE checks, and repaired aggregate UI runner validation.

## 2026-07-13 - Touch 025: live Stage 2 catalog and Blueprint validation

- **Touched:** 15 affected/generic Widget Blueprints, crafting/building PIE catalog contracts, and the expanded Stage 2 validation script.
- **Blueprint gate:** 15/15 assets compiled with warnings treated as errors and retained their expected native parents. No assets were saved or reparented.
- **Crafting gate:** PASS. Context population began at 4 rows and show-all-known expanded to 7, including 4 inspectable unavailable rows. A selected row survived refresh/reorder, cleared exactly once when removed, cleared the details, and could be selected again when restored.
- **Building gate:** PASS. All 13 rows populated, 6 material-incomplete rows stayed inspectable, and selection/detail preservation, removal clearing, and restoration passed.
- **Next touch:** Execute and harden the repaired 79-test live CommonUI aggregate.

## 2026-07-13 - Touch 026: F21 aggregate repair and shared CommonUI lifecycle fixes

- **Touched:** `MOUITestSubsystem.*`, `MOUITestConsoleCommands.cpp`, `MOUIControllerBase.*`, `MOUIManagerComponent.*`, `MOActivatableWidget.cpp`, `Tools/validate_ui_batch_pie.py`, `Tools/validate_ui_close_escape_pie.py`, and the cleanup diagnostic probe.
- **Harness diagnosis:** A one-test-per-frame loop was necessary but insufficient. Multi-action cases still performed user actions before CommonUI activation/transition completion, and inter-test cleanup could observe transient stack state.
- **Harness implementation:** Added declarative frame actions, retrying assertions, explicit activation barriers, two-frame clean-stack inter-test barriers, exact-name validation for `MO.UI.RunTest`, and a report-count/timeout checked batch driver.
- **Product diagnosis 1:** CommonUI deactivation is also emitted when an activatable widget is covered/suspended. Cache invalidation treated this as actual closure, so covered menus lost ownership and could not be reconciled safely.
- **Product fix 1:** Controller cache registration is idempotent/weak, preserves ownership while a widget is covered, uses top-of-owning-stack status for open state, finds the actual owning layer, and explicitly removes inactive covered entries.
- **Product diagnosis 2:** Closing one menu and pushing another in the same frame races CommonUI's asynchronous transitions, allowing covered predecessor entries to accumulate and reactivate later.
- **Product fix 2:** `UMOUIManagerComponent` schedules one centralized next-tick menu-layer reconciliation after close/switch transactions. It keeps the newest controller-owned top entry and removes covered predecessors; if no controller owns a menu entry, it clears the layer.
- **Product diagnosis 3:** `RemoveActionBinding` removed a widget's routing reference but did not unregister the global `FUIActionBinding`, so pooled activatable widgets accumulated duplicate close-action registrations.
- **Product fix 3:** `UMOActivatableWidget` calls `CloseActionBinding.Unregister()` before removing and recreating the binding. The exported UE 5.8 primitives were used after the incremental build proved a direct non-exported helper could not link.
- **Compile:** Final incremental `MO57Editor` build passed 5/5 actions.
- **Next touch:** Definitive cold Escape, full live aggregate, post-run state, headless automation, static hygiene, and graph refresh.

## 2026-07-13 - Touch 027: definitive validation and durable handoff

- **Cold live gate:** All `*.CloseEscape` tests passed 7/7 from a fresh editor/PIE lifecycle.
- **Definitive live aggregate:** 79 passed, 0 failed; the 79 persisted results matched all 79 registered tests. No cleanup-timeout or duplicate-action-binding signature occurred in the definitive run.
- **Post-run diagnostics:** `MenuLayer: 0 widgets`, `Active Menu Count: 0`, `Any Menu Open: No`, move/look input both restored, and no active menu widget instances remained.
- **Editor hygiene:** PIE ended with zero dirty content packages and zero dirty map packages; the editor was closed before headless validation.
- **Cold headless gates:** `MOFramework.UI.Catalog` passed 7/7 and full `MOFramework` passed 124/124, with zero failed and zero not-run tests.
- **Graphify:** Required post-code update completed at 10,815 nodes, 15,833 edges, and 867 communities.
- **Continuity:** Updated `SESSION_STATE.md`, F21, the migration status, and `claude-codex-coop.md`. No staging, commit, or push performed.
- **Next touch:** Migration Stage 3 queue-presentation consolidation, starting with a fresh Graphify trace and explicit Claude/Codex file claims.

## 2026-07-13 - Touch 028: retirement candidate proof and manifest

- **Touched:** Graphify affected queries, source/binary reference scans, `Tools/validate_ui_retirement.py`, `RETIREMENT_MANIFEST.md`, and the Claude/Codex coordination log.
- **Static result:** Graphify found no affected nodes for `UMOBuildWidget`, `UMOBuildingEntryWidget`, or `UMODetailPanelBase`. Source scans found no construction/callsite for the three candidates; `UMOBuildWidget` retained only a stale include/forward/documentation reference. Binary asset scans found no native-class reference for the two building candidates.
- **Asset boundary:** Seven previously live-verified zero-referencer assets are recorded for retirement. The generic list/entry assets may retire, but their active native bases must remain. Concrete crafting/building adapters remain load-bearing.
- **Safety:** Added a read-only live validation that fails on any candidate referencer or unexpected direct Blueprint child. No assets or source files were deleted.
- **Coordination:** Claude owns the current terraform/H23 build-editor window. UI source editing and live validation are paused until Claude posts a release.
- **Next touch:** Run the live retirement gate, add native deprecation metadata/remove stale dependencies, then execute compile and regression gates.

## 2026-07-13 - Touch 029: controlled widget retirement completed

- **Live proof:** All seven candidate assets again reported zero referencers. `UMOBuildWidget` and `UMOBuildingEntryWidget` had no direct Blueprint children; `UMODetailPanelBase` had only the candidate `WBP_MODetailPanel` child.
- **Implementation:** Added compatibility-safe `UE_DEPRECATED(5.8, ...)` annotations and replacement guidance to the three unused native widget families. Removed the stale `MOBuildWidget` include/forward reference and corrected building-controller documentation to name the live ghost-context flow.
- **Build correction:** The first attempt used Unreal's `Deprecated` UCLASS specifier, which UHT correctly rejected because it requires a serialized class rename to `UDEPRECATED_*`. It was replaced with the engine-standard C++ annotation, preserving Unreal class names and serialized compatibility. Final build passed 23/23 actions.
- **Regression gates:** Catalog automation 7/7; full MOFramework 124/124; affected/generic Widget Blueprints 15/15 with warnings as errors; live CommonUI aggregate 79/79.
- **Post-run state:** Menu layer 0, active menus 0, movement/look input restored, no dirty content/map packages, editor closed.
- **Graphify:** Post-code refresh completed at 10,830 nodes, 15,846 edges, and 884 communities.
- **Deletion boundary:** No source or assets were deleted. Seven assets are cleared and recorded in `RETIREMENT_MANIFEST.md` for a separate explicit asset-aware deletion pass after the compatibility window.
- **Next touch:** Migration Stage 3 queue renderer/adapters, or a separately authorized physical asset-deletion/fixup pass.

## 2026-07-20 - Touch 030: Migration Stage 3 — queue presentation consolidated (Claude)

- **Handoff:** Wes directed Claude to resume the UI migration at the Touch-029 resume point. Coop claim CL-0023; conventions preserved (append-only log units, no WBP/DataTable edits, no asset deletions).
- **Trace:** 4-agent map of both queue stacks, Stage-2 conventions, and the test harness. Key liveness finding: `UMOBuildingQueueWidget` has NO live consumer (only the deprecated `UMOBuildWidget` initializes it; the live building surface is `UMOGhostContextMenu`) — building coverage is therefore compat-correct but dormant.
- **Implementation:** New shared layer `MOQueueDisplayTypes.h/.cpp` (pure `FMOQueueDisplayRow` {RowId FGuid, SourceId, Title, Icon, raw counts + texts, Progress, RemainingSeconds, State{Queued/Active/Paused}, bCancellable}, `FMOQueueHeaderDisplay`, `MOQueueDisplay` formatting chokepoint), `UMOQueueRowWidgetBase` (renders a row, emits cancel INTENTS, F18 idempotent dual-button lifecycle, 3-state fill color), `UMOQueueRendererBase` (owns row lifecycle/tick-poll/header/empty state; BlueprintNativeEvent domain hooks HasQueueSource/BuildDisplayRows/GetHeaderDisplay/GetActiveRowLiveProgress/ExecuteCancelRow/ExecuteCancelAll; zero gameplay in the shared layer). All legacy binding names + color properties moved UP verbatim so WBP bindings and overridden defaults survive by name-based serialization.
- **Compat adapters:** `UMOCraftingQueueWidget/EntryWidget` and `UMOBuildingQueueWidget/EntryWidget` reparented as thin adapters; class names, `InitializeQueue` signatures, BlueprintCallable APIs, BlueprintImplementableEvents, legacy display structs, and legacy `OnCancelRequested` delegates preserved. Cancel EXECUTION moved out of shared presentation into the domain hooks (refund policy now lives at the adapter boundary). Building adapter mints a STABLE row id (buildable identity GUID; the legacy per-refresh `FGuid::NewGuid()` could not round-trip a cancel intent) and verifies it before cancelling.
- **Deliberate parity notes:** queued-row ETA still base `CraftTime * Count` (tool bonuses not applied — pre-existing); building cancel semantics NOT unified with the ghost-menu path (design fork for Wes: full-refund-world-drops+ghost-survives vs skill-partial-to-inventory+ghost-destroyed); crafting header title now falls back to the recipe-id text when the recipe row is missing (previously stale text); an unbound renderer uniformly clears the numeric header on the tick path (building's legacy no-component early-return diverged trivially).
- **Tests:** Headless `MOFramework.UI.Queue.*` x3 (formatting chokepoint, crafting row mapping incl. queued-ETA parity + id passthrough, building state mapping). Live `Queue.*` x5 in the MO.UI suite (CraftingRows, CancelOneIntent, CancelAllEmptyState, SourceSwapUnbind, ReconstructOneIntent) with a self-adapting fixture (iterates the recipe DataTable, grants ingredients, lets authoritative EnqueueCraft validation pick a craftable recipe). New `Tools/validate_ui_queue_pie.py` covers the frame-dependent checklist items: live progress advance, completion-as-removal, and the F18 MENU-round-trip reconstruct (same-frame close+reopen legitimately races CommonUI per F21 — the sync test covers row-level reconstruct via double RefreshQueue instead).
- **Gates (final binary):** build clean; headless MOFramework 127/127 (was 124; +3 Queue) incl. MOFramework.UI 10/10; WBP parents/compile 15/15 warnings-as-errors; cold Escape 7/7; live aggregate 84/84 (was 79; +5 Queue); queue PIE contract 6/6. Editor closed after headless.
- **Next touch:** Stage 4 (skills/knowledge + quest collection extraction), the deferred design forks above, or wiring the shared renderer into a live building surface (separate decision).
