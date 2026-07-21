# UI Consolidation Findings

Status: Audit complete; implementation not started.

## Evaluation rule

A shared abstraction is justified only when multiple widgets repeat the same lifecycle and presentation responsibility. Domain validation, data queries, authority, and gameplay execution should remain in their owning system/controller rather than moving into a generic widget.

## Questions under audit

1. Do the generic bases currently own real behavior or only duplicate BindWidget fields?
2. Which concrete entries/lists/details actually inherit them?
3. Are crafting and building separate because of true domain differences, or historical copying?
4. Can list population and selection be expressed through typed view models without unsafe reflection/string dispatch?
5. Who currently owns selection state, refresh triggers, filtering, action enablement, and queue binding?
6. Are CommonUI activation, focus, and close paths consistent across these menus?
7. Can a reusable composed “catalog workspace” reduce duplication without becoming a god-widget?

Confirmed findings will cite paths, symbols, current behavior, failure/maintenance cost, and proposed boundary.

## Confirmed early findings

### F1 — The “generic detail panel” is unused scaffolding

- **Evidence:** `UMODetailPanelBase` has no subclass or callsite. `SetActionText()` intentionally does nothing. `UMORecipeDetailPanelBase` separately implements the actual recipe/building detail lifecycle.
- **Implication:** Reparenting working recipe/building panels to `UMODetailPanelBase` would add inheritance complexity without removing behavior. Treat it as a candidate for redesign/deprecation, not a migration destination.

### F2 — List/entry consolidation partially landed and is functional

- **Evidence:** Crafting and building recipe lists/entries use `UMOScrollListBase`/`UMOListEntryBase`; the bases create entries, bind selection, retain selected IDs, and update visual state.
- **Gap:** Skills and queue families still use one-off widgets. The base advertises filtering but does not implement it.

### F3 — Progress and confirmation bases are mature shared mechanisms

- **Evidence:** Harvest/inspection progress and confirmation/text-input families actively inherit the bases, which own timing/input/delegate lifecycle.
- **Implication:** These are patterns to preserve; the catalog cleanup should emulate their clear division between shared lifecycle and domain hooks.

### F4 — Recipe list/entry inheritance is nominal

- **Evidence:** Both recipe list subclasses maintain their own arrays, IDs, selection, population, and delegates instead of using `UMOScrollListBase`. Entry setup does not populate the base `EntryId`; concrete lists bind legacy concrete delegates.
- **Cost:** Two state machines exist in each object. Future fixes to base selection/population do not fix the concrete lists, and maintainers can reasonably but incorrectly assume the base owns behavior.
- **Proposed direction:** Make one state machine authoritative. Prefer the base population/selection path with virtual construction/configuration hooks or replace it with a typed catalog component; do not retain parallel base and subclass stores.

### F5 — Crafting and building catalogs are copy variants

- **Evidence:** Recipe entry/list diffs are dominated by craft/build terminology. Both use the same recipe IDs, definition rows, icons, selected/available state, and selection flow. Both details already share `UMORecipeDetailPanelBase`.
- **Proposed direction:** One recipe catalog presentation and view-data type. Domain policy supplies query/filter and availability reason; style supplies labels/colors. Building remains a recipe kind, not a separate list framework.

### F6 — Queue presentation is duplicated around different domain sources

- **Evidence:** Crafting and building queue widgets repeat container management, entry creation, progress/current/remaining/empty state, cancel-all, ticking, and formatting. Their true difference is the source (`UMOCraftingQueueComponent` versus `UMOBuildProgressComponent`) and source event/data shape.
- **Proposed direction:** Shared queue presentation driven by immutable display rows and a small adapter/provider contract. Keep cancellation and authority calls in the domain adapter/controller.

### F7 — Legacy/orphan building UI may be inflating the apparent system

- **Evidence:** Current controller flow uses `UMOBuildingMenu` plus `UMOGhostContextMenu`. `UMOBuildWidget` and `UMOBuildingEntryWidget` have no source/config/Python consumers.
- **Constraint:** Unreal assets can hold class references invisibly to source grep. Verify Asset Registry/referencers before marking deprecated or removing.

### F8 — Skills repeats the catalog shell but not recipe policy

- **Evidence:** `UMOSkillsPanel` owns list population, mode/filter state, selection, embedded detail bindings, and live data subscriptions in one class; `UMOSkillEntryWidget` is a one-off entry implementation.
- **Boundary:** Reuse composition and selection, not recipe semantics. A shared workspace needs optional detail/action/auxiliary slots and a provider/view-model seam; skill progress/flash behavior remains specialized.
- **Data issue:** Knowledge display is forced into `FMOSkillDisplayData`, coupling unrelated concepts and leaving fields semantically ambiguous.

### F9 — Quest log independently implements the same workspace

- **Evidence:** `UMOQuestLogPanel` creates entries, stores selected ID, updates selection visuals, populates details/objectives, and exposes track/abandon actions in a left-list/right-detail layout.
- **Direction:** Reuse workspace composition/selection and preserve quest-specific objective/action presenter logic.

### F10 — Survivor task confirms optional catalog + queue composition

- **Evidence:** `UMOSurvivorTaskMenu` has available jobs on the left and active queue on the right, each with custom one-off entry creation and cancellation.
- **Direction:** The shared shell should accept independently optional catalog, details, primary action, and auxiliary/queue regions. Queue adapters should expose display rows/events, not gameplay mutation through a generic widget.

### F11 — Colony prototype owns domain operations in the widget

- **Evidence:** `UMOColonyOverviewWidget::HandleAssignJob` searches world actors and enqueues jobs directly.
- **Risk:** Presentation becomes coupled to world-query policy and cannot support alternate station/storage selection, authority validation feedback, or reuse cleanly.
- **Direction:** Move job-resolution/assignment orchestration to colony/controller/domain service; widget emits an intent with stable pawn/job identifiers.

### F12 — Inventory is compositionally related but behaviorally distinct

- **Evidence:** `UMOUnifiedInventoryMenu` composes grids, slots, item details, equipment, nearby items, tabs, drag/drop, transfers, and context actions.
- **Direction:** Exclude inventory grids from the first catalog abstraction. A later inventory-specific split should extract transfer/action orchestration and tab panels without replacing GUID/grid semantics with list IDs.

### F13 — UI layer policy and implementation disagree

- **Evidence:** All interactive menu controllers currently push to `Layer_Menu`; planning/header docs assign crafting/building/skills/inventory to `Layer_Game`.
- **Risk:** A consolidation that follows docs blindly may change stack ordering, input restoration, modal background behavior, and toggle semantics.
- **Direction:** Decide/test layer policy independently. The catalog workspace should not hardcode its destination layer.

### F14 — Generic Widget Blueprints are unused scaffolding

- **Evidence:** Binary reference scan found `WBP_MOScrollList`, `WBP_MOListEntry`, and `WBP_MODetailPanel` only referencing themselves; concrete menus use concrete C++-parented WBPs. The generic detail C++ base likewise has no consumer.
- **Direction:** Do not build more generic assets until a concrete composition contract consumes them. Preserve them during the audit; later mark experimental/deprecated only after Asset Registry verification.

### F15 — Tests protect the shell, not catalog behavior

- **Evidence:** `MOUITestSubsystem` extensively covers activation/input/focus/layers and switchable menus but has no recipe/skill/quest selection, detail, filtering, queue, or action-state tests.
- **Direction:** Before refactoring, add deterministic tests for provider rows, selection propagation, selected-ID preservation after refresh, availability reasons, detail presenter output, action intent, and queue adapter updates/cancellation.

### F16 — Recipe availability is incorrectly coupled to row selectability

- **Evidence:** `UMORecipeEntryWidget::SetupEntry()` and `UMOBuildingRecipeEntryWidget::SetupEntry()` pass `bCanCraft`/`bCanBuild` to `UMOListEntryBase::SetEntryEnabled()`. The base disables `EntryButton` and suppresses its selection delegate when false.
- **Wrong state first appears:** The entry view model collapses two independent concepts—"may inspect/select this known recipe" and "may execute it now"—into one enabled flag.
- **User impact:** A known recipe that lacks materials, tools, station, or skill can be visible but unselectable, preventing the detail panel from explaining what is missing.
- **Direction:** Rows for known recipes remain selectable. Represent execution availability and a reason separately; the detail action consumes that state and shows the reason. Reserve row disabled state for a genuinely non-interactive row.

### F17 — Delegate consolidation is incomplete and masks invalid base state

- **Evidence:** Recipe/building entries broadcast both inherited `OnEntrySelected` and local `OnEntryClicked`; concrete lists subscribe to the local event. Lists broadcast local `OnRecipeSelected` while the standard selection path is unused. `SetupEntry()` does not call `SetEntryId()`, so the inherited event carries `NAME_None` even though the local event carries the correct recipe ID.
- **Additional drift:** `UMOListEntryBase` declares its own same-shape delegate despite including `MOUIDelegates.h`; its header says `SetSelected()` broadcasts, but the implementation only updates visuals. Only a click broadcasts.
- **Direction:** Establish one selection event and one authoritative ID store. Preserve legacy Blueprint-facing delegates as temporary forwarding shims until Asset Registry/Blueprint usage is verified, then deprecate them.

### F18 — Queue button bindings are not reconstruct-safe

- **Evidence:** Crafting/building queue widgets and their entry widgets add cancel-button bindings in `NativeConstruct()` without removing the receiver first. Their `NativeDestruct()` methods remove component delegates but not button delegates; entry widgets have no corresponding teardown.
- **Risk:** Reconstruct/reuse can accumulate callbacks, violating the project's binding convention and causing duplicate cancel requests.
- **Direction:** Independently of queue consolidation, make every binding idempotent (`RemoveAll(this)`/`RemoveDynamic` before add) and remove it during teardown. Add a reconstruct test that asserts one click produces one intent.

### F19 — Controller menu lifecycle is repeated despite a shared base

- **Evidence:** Specialized controllers repeatedly perform close-other-menu checks, modal-background show/hide, layer push/cast/error cleanup, cache registration, delegate rebinding, domain initialization, reticle refresh, and close cleanup. `UMOUIControllerBase` centralizes individual operations but not the open/close transaction.
- **Risk:** Failure cleanup and modal reference handling can drift between menus; adding another switchable workspace requires copied controller code.
- **Direction:** Add a narrow lifecycle helper/template for push, validated cast, cache registration, failure rollback, and common close bookkeeping. Keep domain preconditions, initialization, and domain delegate wiring in explicit controller hooks.

### F20 — `RegisterCachedMenu` can accumulate deactivation lambdas

- **Evidence:** The template unconditionally calls `Widget->OnDeactivated().AddLambda(...)` and stores no delegate handle. Some controller paths can register a reused/re-pushed widget again.
- **Risk:** Re-registration leaves multiple callbacks pointing at the same cache. They are mostly idempotent today, but the helper cannot explicitly unbind and does not uphold one-registration ownership.
- **Direction:** Make registration idempotent or track/remove a handle.

### F21 — The console UI suite is synchronously invalid against frame debouncing

- **Status:** Resolved and validated 2026-07-13.
- **Original evidence:** `MO.UI.RunAllTests` executed 79 menu tests synchronously in one engine frame. Controllers intentionally reject repeated toggles in the same frame. The batch produced 17/79 while individually issued, frame-separated tests passed.
- **Root causes found during repair:** The runner had no frame or activation barriers; controller cache invalidation treated CommonUI coverage as closure; same-frame close-old/push-new transitions could leave covered menu entries; pooled activatable widgets removed their routing binding without unregistering the global binding.
- **Resolution:** The aggregate is now frame-stepped with explicit activation/assertion retries and clean-stack inter-test barriers. Cache ownership is top-of-stack aware, menu-layer reconciliation is centralized after switch/close transactions, and close bindings unregister before removal/rebind.
- **Validation:** Cold Escape-close 7/7; definitive live aggregate 79/79; post-run menu layer and active menu count both zero; move/look input restored; no duplicate-binding or cleanup-timeout signature in the definitive run.

## Consolidation boundary

The common unit is not a universal menu superclass. It is a small set of composed presentation mechanisms:

1. An authoritative selectable collection owning entry lifecycle, stable IDs, selection preservation, empty state, and selection events.
2. Domain-specific entry presenters/view data retaining specialized visuals such as skill progress and recipe availability.
3. The proven `UMORecipeDetailPanelBase` for recipe/building details; skill and quest details remain their own presenters.
4. A shared queue renderer driven by domain-neutral display rows, with crafting/building/survivor adapters translating source events and handling commands.
5. An optional workspace composition layer only after the collection/detail/queue seams work. It supplies slots, not gameplay policy.

Inventory grids, drag/drop, equipment, possession rows, world queries, validation, and authority calls stay outside this catalog abstraction.
