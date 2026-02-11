# UIManager Split - Testing Notes

## Overview

The monolithic `MOUIManagerComponent` (~4000 lines) has been split into 6 specialized UI controllers. Each controller handles a specific UI subsystem while UIManager maintains backward-compatible public API through delegation.

## Architecture

```
MOUIManagerComponent (Orchestrator)
    ├── MOUIControllerBase (Shared utilities)
    ├── MOCharacterUIController
    ├── MOBuildingUIController
    ├── MOCraftingUIController
    ├── MOSystemMenuUIController
    └── MOInventoryUIController
```

All controllers are sibling components on `AMOPlayerController`. They find each other via `GetOwner()->FindComponentByClass<T>()` with weak pointer caching.

---

## Controller Breakdown

### 1. MOUIControllerBase
**File**: `MOUIControllerBase.h/cpp`
**Role**: Base class providing shared utilities for all controllers

**Key Methods**:
- `ResolveOwningPlayerController()` - Get owning PlayerController
- `IsLocalOwningPlayerController()` - Check if local controller
- `GetUIManager()` - Get sibling UIManager component
- `HasValidPawn()` / `ShowNoPawnNotification()` - Pawn validation
- `ApplyInputModeForMenuOpen/Closed()` - Input mode management
- `ShowModalBackground()` / `HideModalBackground()` - Modal background
- `UpdateReticleVisibility()` - Reticle updates
- `GetCachedInventory/Skills/SurvivalStats/Vitals/etc.()` - Cached component access

**Testing**: Base class, tested indirectly through derived controllers

---

### 2. MOCharacterUIController
**File**: `MOCharacterUIController.h/cpp`
**Handles**: Skills panel, Status panel, Item inspection

**Public API**:
| Method | Description |
|--------|-------------|
| `ToggleSkillsPanel()` | Toggle skills panel visibility |
| `OpenSkillsPanel()` | Open skills panel |
| `CloseSkillsPanel()` | Close skills panel |
| `IsSkillsPanelOpen()` | Check if skills panel is open |
| `TogglePlayerStatus()` | Toggle status panel visibility |
| `SetPlayerStatusVisible(bool)` | Set status panel visibility |
| `IsPlayerStatusVisible()` | Check if status panel is visible |
| `RebindStatusPanelToCurrentPawn()` | Rebind after pawn change |
| `StartItemInspection(FName, FGuid)` | Start inspecting item |
| `CancelItemInspection()` | Cancel active inspection |
| `IsInspectionInProgress()` | Check if inspecting |

**Test Cases**:
- [ ] Open/close skills panel with Tab key
- [ ] Skills panel shows correct skills for current pawn
- [ ] Open/close status panel
- [ ] Status panel shows vitals/metabolism/mental state
- [ ] Status panel rebinds correctly after possessing different pawn
- [ ] Item inspection starts from inventory context menu "Inspect"
- [ ] Inspection progress widget shows correctly
- [ ] Inspection can be cancelled
- [ ] Inspection completion grants XP/knowledge

---

### 3. MOBuildingUIController
**File**: `MOBuildingUIController.h/cpp`
**Handles**: Building menu, Ghost context menu, Build widget

**Public API**:
| Method | Description |
|--------|-------------|
| `ToggleBuildingMenu()` | Toggle building menu |
| `OpenBuildingMenu()` | Open building menu |
| `CloseBuildingMenu()` | Close building menu |
| `IsBuildingMenuOpen()` | Check if building menu open |
| `ShowGhostContextMenu(AActor*, FVector)` | Show ghost context menu |
| `HideGhostContextMenu()` | Hide ghost context menu |
| `IsGhostContextMenuOpen()` | Check if ghost menu open |
| `ShowBuildWidget(...)` | Show build progress widget |
| `HideBuildWidget()` | Hide build widget |
| `IsBuildWidgetOpen()` | Check if build widget open |

**Test Cases**:
- [ ] Open/close building menu with B key
- [ ] Building menu lists available buildings
- [ ] Select building from menu starts placement mode
- [ ] Ghost context menu appears on right-click ghost
- [ ] Ghost context menu actions work (Rotate, Confirm, Cancel)
- [ ] Build progress widget shows during construction
- [ ] Build widget updates progress correctly

---

### 4. MOCraftingUIController
**File**: `MOCraftingUIController.h/cpp`
**Handles**: Crafting menu, Station context menu, KeepOnHarvest menu, Harvest operations

**Public API**:
| Method | Description |
|--------|-------------|
| `ToggleCraftingMenu()` | Toggle crafting menu |
| `OpenCraftingMenu()` | Open crafting menu |
| `CloseCraftingMenu()` | Close crafting menu |
| `IsCraftingMenuOpen()` | Check if crafting menu open |
| `ShowStationContextMenu(AActor*, FVector)` | Show station context menu |
| `HideStationContextMenu()` | Hide station context menu |
| `IsStationContextMenuOpen()` | Check if station menu open |
| `ShowKeepOnHarvestContextMenu(FMOInteractionTarget)` | Show harvest context menu |
| `HideKeepOnHarvestContextMenu()` | Hide harvest context menu |
| `IsKeepOnHarvestContextMenuOpen()` | Check if harvest menu open |
| `StartHarvestOperation(FName)` | Start harvest with recipe |
| `CancelHarvestOperation()` | Cancel active harvest |
| `IsHarvestInProgress()` | Check if harvesting |

**Test Cases**:
- [ ] Open/close crafting menu with C key
- [ ] Crafting menu shows available recipes
- [ ] Recipe details show ingredients and requirements
- [ ] Can queue crafting operations
- [ ] Station context menu appears when interacting with crafting station
- [ ] Station context menu "Open" opens station inventory
- [ ] Station context menu "Craft" opens crafting UI for station
- [ ] KeepOnHarvest context menu appears for harvestable ISM/HISM
- [ ] Harvest progress widget shows during harvest
- [ ] Harvest can be cancelled
- [ ] Harvest completion yields items

---

### 5. MOSystemMenuUIController
**File**: `MOSystemMenuUIController.h/cpp`
**Handles**: In-game menu, Possession menu, Confirmation dialogs

**Public API**:
| Method | Description |
|--------|-------------|
| `ToggleInGameMenu()` | Toggle in-game menu |
| `OpenInGameMenu()` | Open in-game menu |
| `CloseInGameMenu()` | Close in-game menu |
| `IsInGameMenuOpen()` | Check if in-game menu open |
| `TogglePossessionMenu()` | Toggle possession menu |
| `OpenPossessionMenu()` | Open possession menu |
| `ClosePossessionMenu()` | Close possession menu |
| `IsPossessionMenuOpen()` | Check if possession menu open |
| `RefreshPossessionMenu()` | Refresh pawn list |
| `ShowConfirmationDialog(...)` | Show confirmation dialog |

**Events**:
- `OnConfirmationConfirmed` - Broadcast when user confirms
- `OnConfirmationCancelled` - Broadcast when user cancels

**Test Cases**:
- [ ] Open/close in-game menu with Escape key
- [ ] In-game menu shows Resume, Save, Load, Options, Exit
- [ ] Save game works (new slot and overwrite existing)
- [ ] Load game works with confirmation
- [ ] Exit to main menu works with confirmation
- [ ] Exit game works with confirmation
- [ ] Options panel opens and settings persist
- [ ] Possession menu opens with P key (or configured key)
- [ ] Possession menu lists all controlled pawns
- [ ] Can possess different pawn from menu
- [ ] Can create new character from possession menu
- [ ] Confirmation dialogs show correct text
- [ ] Confirmation dialogs handle confirm/cancel correctly

---

### 6. MOInventoryUIController
**File**: `MOInventoryUIController.h/cpp`
**Handles**: Inventory menus, Item context menu, Nearby items, Transfer, Drop

**Public API**:
| Method | Description |
|--------|-------------|
| `ToggleInventoryMenu()` | Toggle inventory menu |
| `OpenInventoryMenu()` | Open inventory menu |
| `CloseInventoryMenu()` | Close inventory menu |
| `IsInventoryMenuOpen()` | Check if inventory open |
| `OpenInventoryWithContainer(AActor*)` | Open with container |
| `SetActiveContainer(AActor*)` | Set active container |
| `ClearActiveContainer()` | Clear container |
| `GetActiveContainer()` | Get current container |
| `HasActiveContainer()` | Check if container set |
| `ShowItemContextMenu(...)` | Show item context menu |
| `CloseItemContextMenu()` | Close context menu |
| `IsItemContextMenuOpen()` | Check if context menu open |
| `QueryNearbyWorldItems()` | Get nearby lootable items |
| `LootAllNearbyItems()` | Loot all nearby |
| `HandleQuickTransfer(FGuid, UMOInventoryComponent*)` | Quick transfer item |
| `DropItemToWorldByGuid(...)` | Drop item to world |
| `GetNearbyItemsQueryRadius()` | Get loot radius |
| `SetNearbyItemsQueryRadius(float)` | Set loot radius |

**Test Cases**:
- [ ] Open/close inventory with Tab key
- [ ] Inventory shows current pawn's items
- [ ] Items display correct icon, name, quantity
- [ ] Right-click item shows context menu
- [ ] Context menu "Use" consumes edible items
- [ ] Context menu "Drop" drops item to world
- [ ] Context menu "Inspect" starts inspection
- [ ] Context menu "Split Stack" splits stackable items
- [ ] Context menu "Craft" opens crafting menu
- [ ] Context menu "Details" shows item details tab
- [ ] Context menu "Transfer" quick-transfers to/from container
- [ ] Open inventory with container shows both inventories
- [ ] Can drag items between player and container
- [ ] Shift+click quick-transfers items
- [ ] "Loot All" button picks up all nearby items
- [ ] Dropped items appear in world with physics
- [ ] Dropped items land on ground (not floating)

---

## Cross-Controller Integration Tests

- [ ] Escape key closes any open menu (inventory, crafting, skills, etc.)
- [ ] Opening one menu closes others appropriately
- [ ] Modal background shows/hides correctly
- [ ] Input mode switches correctly (game vs UI)
- [ ] Mouse cursor shows/hides correctly
- [ ] Reticle visibility updates when menus open/close
- [ ] `CloseAllMenus()` closes everything
- [ ] `IsAnyMenuOpen()` returns correct state

---

## Regression Tests

These should work exactly as before:

- [ ] All keyboard shortcuts work (Tab=Inventory, C=Crafting, B=Building, Esc=Menu, etc.)
- [ ] All UI widgets appear at correct Z-order
- [ ] No duplicate widget instances created
- [ ] Widgets properly cleaned up on level change
- [ ] Save/Load preserves all game state
- [ ] Pawn possession updates all UI correctly
- [ ] No crashes when rapidly opening/closing menus

---

## Files Modified

### New Files Created:
- `MOUIControllerBase.h/cpp` - Base controller class
- `MOCharacterUIController.h/cpp` - Character UI controller
- `MOBuildingUIController.h/cpp` - Building UI controller
- `MOCraftingUIController.h/cpp` - Crafting UI controller
- `MOSystemMenuUIController.h/cpp` - System menu controller
- `MOInventoryUIController.h/cpp` - Inventory UI controller (updated from stub)

### Files Modified:
- `MOUIManagerComponent.h` - Added controller getters, forward declarations
- `MOUIManagerComponent.cpp` - Delegated methods to controllers
- `MOFramework.Build.cs` - No changes needed (same module)

---

## Known Limitations

1. **Blueprint Compatibility**: Widget class properties on controllers need to be set in Blueprints derived from BP_MOPlayerController
2. **Deprecation Period**: UIManager still has wrapper methods for backward compatibility
3. **Widget State**: Some widget references remain in UIManager for `CloseAllMenus()` to work

---

## Future Cleanup (Phase 4)

After testing confirms everything works:
1. Remove deprecated wrapper methods from UIManager
2. Remove unused member variables from UIManager header
3. Update CLAUDE.md architecture documentation
4. Consider splitting MOFramework module into submodules

---

*Created: 2026-02-11*
*Related Plan: cheerful-napping-wand.md*
