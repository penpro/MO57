# UI Retirement Manifest

Updated: 2026-07-13 (America/Los_Angeles)

This manifest distinguishes active compatibility adapters from genuinely unused UI scaffolding. Project policy is deprecate, prove, and flag before deletion; this pass does not delete source or assets.

## Pass result

- Live Asset Registry: all seven candidate assets have zero referencers.
- Live Blueprint scan: `UMOBuildWidget` and `UMOBuildingEntryWidget` have no direct Blueprint children; `UMODetailPanelBase` has only the candidate `WBP_MODetailPanel` child.
- Native candidates carry compatibility-safe `UE_DEPRECATED(5.8, ...)` guidance without renaming their Unreal classes.
- Stale `MOBuildWidget` include/forward documentation was removed; the building controller now documents the real `UMOGhostContextMenu` flow.
- C++/UHT build passed 23/23 actions; catalog automation 7/7; full MOFramework 124/124; Widget Blueprints 15/15; live UI aggregate 79/79.
- Post-run state: zero menu-layer widgets, zero active menus, movement/look input restored, and zero dirty assets/maps.
- Graphify refreshed to 10,830 nodes, 15,846 edges, and 884 communities.

## Cleared asset candidates

| Asset | Reason | Retirement action |
|---|---|---|
| `/MOFramework/UI/WBP_RecipeEntry` | Legacy root asset; organized crafting entry is active | Fix redirector if applicable, then remove in a later asset-deletion pass |
| `/MOFramework/UI/WBP_RecipeList` | Legacy root asset; organized crafting list is active | Fix redirector if applicable, then remove later |
| `/MOFramework/UI/WBP_RecipeDetail` | Legacy root asset; organized crafting detail is active | Fix redirector if applicable, then remove later |
| `/MOFramework/UI/WBP_CraftingQueue` | Legacy root asset; organized crafting queue is active | Fix redirector if applicable, then remove later |
| `/MOFramework/UI/CommonUI/WBP_MOListEntry` | Unused generic scaffolding; active concrete entries inherit the native base directly | Retire asset only; keep `UMOListEntryBase` |
| `/MOFramework/UI/CommonUI/WBP_MOScrollList` | Unused generic scaffolding; active concrete lists inherit the native base directly | Retire asset only; keep `UMOScrollListBase` |
| `/MOFramework/UI/CommonUI/WBP_MODetailPanel` | Only consumer of the unused generic detail base | Retire with `UMODetailPanelBase` after deprecation window |

Prior live validation reported zero Asset Registry referencers for all seven. `Tools/validate_ui_retirement.py` rechecks this set and fails closed if any reference appears.

## Native class candidates

| Native class | Static evidence | Planned state |
|---|---|---|
| `UMODetailPanelBase` | No production subclass/consumer; its only direct WBP is the candidate above; `SetActionText` is a no-op | Deprecated; retain source for compatibility window |
| `UMOBuildingEntryWidget` | No C++ callsite, Graphify affected node, binary asset reference, or Blueprint child | Deprecated; retain source for compatibility window |
| `UMOBuildWidget` | No construction/callsite, Graphify affected node, binary asset reference, or Blueprint child | Deprecated; stale dependencies removed; retain source for compatibility window |

## Must remain active

- `UMOListEntryBase`, `UMOScrollListBase`, and `FMOListSelectionModel` own the shared catalog lifecycle.
- `UMORecipeListWidget` and `UMOBuildingRecipeListWidget` remain the native parents of referenced concrete Widget Blueprints.
- Concrete crafting/building recipe entries, lists, details, queues, and menus remain load-bearing presentation adapters.
- Crafting/building/survivor queue widgets remain until Migration Stage 3 supplies and validates the shared renderer/adapters.

## Completed gates

1. Live Asset Registry: passed.
2. Live Blueprint parent scan: passed.
3. Compatibility-safe native deprecation and stale-reference cleanup: passed.
4. C++/UHT build with editor closed: passed.
5. All 15 affected/generic Widget Blueprints, warnings as errors: passed.
6. Catalog 7/7, full MOFramework 124/124, and live UI 79/79: passed.
7. Graphify refresh and touch-log handoff: passed.

Actual file/asset deletion remains a separate explicit asset-aware pass after the compatibility window.
