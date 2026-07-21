# UI System Map

Status: Source-backed static audit complete.

## Intended ownership layers

| Layer | Expected responsibility |
|---|---|
| Player controller input | Centralized input actions and menu toggles |
| UI manager component | Backward-compatible facade and cross-controller orchestration |
| Specialized UI controllers | Character, building, crafting, inventory, system menu, and quest flows |
| `UMOGameUIManagerSubsystem` | CommonUI primary layout and layer-stack ownership |
| Menus/panels | Compose lists, details, queue/action areas, focus, and close requests |
| List widgets | Populate/filter/select domain entries |
| Entry widgets | Render one item and publish selection/activation |
| Detail widgets | Present selected data and expose domain actions |
| Gameplay systems | Validate and execute crafting/building/skills/inventory behavior |

## Generic primitives to verify

- `UMOListEntryBase`
- `UMOScrollListBase`
- `UMODetailPanelBase`
- `UMOProgressWidgetBase`
- `UMOConfirmationBase`
- `UMOMenuWidgetBase` / `UMOActivatableWidget`
- `UMOUIControllerBase`

## Representative composed flows

- Crafting menu: recipe list → recipe details → craft quantity/action → crafting queue.
- Building menu: building recipe list → details/material state → place/build action → building queue/status.
- Skills panel: skill list → selected skill details/progression.
- Inventory: item slots/list → selected item details → context/transfer actions.
- Colony/possession/task menus: character/job list → details → assignment/action.

Implementation and dependency evidence will be added after source tracing.

## Verified crafting flow

`AMOPlayerController input → UMOUIManagerComponent facade → UMOCraftingUIController → Layer_Menu → UMOCraftingMenu → UMORecipeListWidget → UMORecipeEntryWidget → UMORecipeDetailPanel → UMOCraftingQueueComponent/UMOCraftingSubsystem`, with `UMOCraftingQueueWidget` observing the queue component.

## Verified building flow

`AMOPlayerController input → UMOUIManagerComponent facade → UMOBuildingUIController → Layer_Menu → UMOBuildingMenu → UMOBuildingRecipeListWidget → UMOBuildingRecipeEntryWidget → UMOBuildingDetailPanel → placement request → UMOBuildingComponent`. After ghost placement: `UMOBuildingUIController → UMOGhostContextMenu → AMOBuildableActor/UMOBuildProgressComponent`.

`UMOBuildWidget` is not in the verified C++ flow.

## Verified skills flow

`AMOPlayerController input → UMOUIManagerComponent facade → UMOCharacterUIController → Layer_Menu → UMOSkillsPanel`. The panel directly owns skills/knowledge tabs, list population, `UMOSkillEntryWidget` instances, selected-ID state, embedded detail widgets, filtering, and component subscriptions.

## Other verified workspace shapes

- Quest: list → selected quest details/objectives → track/abandon actions.
- Survivor tasks: available-task catalog → add intent, plus active-job queue → cancel intent.
- Possession: pawn collection → per-row possess action; no selected detail surface.
- Colony V1: roster rows → direct assign action; currently mixes presentation and domain orchestration.

## Verified layer routing

| Layer | Current controller usage |
|---|---|
| `Layer_Menu` | Crafting, building, skills, status, quest, inventory, unified inventory, in-game, possession, survivor task |
| `Layer_GameOverlay` | Harvest and inspection progress |
| `Layer_Modal` | Confirmation dialogs |
| `Layer_Game` | No verified controller menu push in the audited callsites |

This differs from the documented intent that switchable gameplay menus use `Layer_Game`.

## Responsibility matrix

| Surface | Collection/selection owner today | Detail/action owner today | Live/queue source | Recommended reuse level |
|---|---|---|---|---|
| Crafting | `UMOCraftingMenu` + `UMORecipeListWidget` | `UMORecipeDetailPanel`/menu | `UMOCraftingQueueComponent` | Shared recipe catalog + recipe detail base + queue adapter |
| Building | `UMOBuildingMenu` + `UMOBuildingRecipeListWidget` | `UMOBuildingDetailPanel`/controller | `UMOBuildProgressComponent` after placement | Same recipe catalog + recipe detail base + queue adapter |
| Skills/knowledge | `UMOSkillsPanel` | Same panel | Skills/knowledge components | Shared collection only; specialized details/entry visuals |
| Quests | `UMOQuestLogPanel` | Same panel | Quest subsystem | Shared collection; quest-specific details/actions |
| Survivor tasks | `UMOSurvivorTaskMenu` | Per-row add/cancel | `UMOSurvivorJobQueueComponent` | Shared collection + queue renderer/adapters |
| Possession | `UMOPossessionMenu` | Per-row possess | Possession controller/system | Collection lifecycle only |
| Colony V1 | `UMOColonyOverviewWidget` | Same widget, including world query/command | Colony/job systems | Extract domain orchestration; reuse roster later |
| Inventory | Inventory grids/menu | Item/context/transfer panels | Inventory components | Excluded from catalog; separate decomposition |

## Target dependency direction

`Controller/domain adapter → typed display rows and availability reasons → collection/detail/queue presentation → stable intent events → controller/domain command`

Widgets must not query the world for targets or execute authoritative gameplay merely because they render an action. Presentation requests; the owning controller/system resolves and reports the result.
