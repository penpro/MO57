# Consolidation Session Checklist

## Files Created

### New Utility Classes
- [x] `MOContextMenuBase.h/cpp` - Base class for context menus
- [x] `MOViewpointUtils.h/cpp` - Viewpoint resolution and line-of-sight utilities
- [x] `MOUIDelegates.h` - Standard UI delegate library

## Files Modified

### Context Menus (Now Inherit from UMOContextMenuBase)
- [x] `MOGhostContextMenu.h/cpp` - Building ghost context menu
- [x] `MOStationContextMenu.h/cpp` - Crafting station context menu
- [x] `MOKeepOnHarvestContextMenu.h/cpp` - Harvest context menu

### Database Settings
- [x] `MOItemDatabaseSettings.h/cpp` - Added lazy caching for item definitions

### Component Deprecation
- [x] `MOInteractorComponent.h` - Deprecated `FindInteractTarget()` method

### Standard Delegate Migration (Added FMOUIRequestClose, FMOUICraftRequest, FMOUIRecipeSelected)
- [x] `MORecipeDetailPanel.h/cpp` - Added `OnCraftAction` (FMOUICraftRequest)
- [x] `MOBuildingDetailPanel.h/cpp` - Added `OnBuildAction` (FMOUICraftRequest)
- [x] `MORecipeListWidget.h/cpp` - Added `OnRecipeSelection` (FMOUIRecipeSelected)
- [x] `MORecipeEntryWidget.h/cpp` - Added `OnEntrySelected` (FMOUIRecipeSelected)
- [x] `MOCraftingMenu.h/cpp` - Added `OnCloseRequested` (FMOUIRequestClose)
- [x] `MOInventoryMenu.h/cpp` - Added `OnCloseRequested` (FMOUIRequestClose)

### Subsystems Using MOViewpointUtils
- [x] `MOInteractionSubsystem.cpp` - Uses ResolveViewpointForController, HasLineOfSight
- [x] `MOPossessionSubsystem.cpp` - Uses ResolveViewpointForPlayerController, HasLineOfSightSimple
- [x] `MOTerraformingComponent.cpp` - Uses ResolveViewpointForController, ResolveViewpointForPawn

## Compile Verification Needed

### Check for Compilation Errors
1. Build `MO57Editor` target in Development mode
2. Look for errors related to:
   - Missing includes
   - Virtual function override mismatches
   - Delegate signature mismatches
   - Template instantiation errors

### Likely Issues to Watch For
- [x] `UMOContextMenuBase` might need forward declarations for `UMOCommonButton` - FIXED: Added full include
- [x] `OnRequestClose` vs `OnCloseRequested` - Both delegates work (backward compatible)
- [ ] Template method `BindButtonClick` might have issues if `UMOCommonButton::OnClicked()` signature differs

## Lessons Learned (DO NOT REPEAT THESE MISTAKES)

### UHT Delegate Files Need a USTRUCT/UCLASS/UENUM
**Problem**: Created `MOUIDelegates.h` with only `DECLARE_DYNAMIC_MULTICAST_DELEGATE` macros. UHT failed to find the delegates when other headers included them.

**Root Cause**: Unreal Header Tool (UHT) only processes `.h` files that contain at least one `UCLASS`, `USTRUCT`, or `UENUM`. Without one of these, the `.generated.h` file is empty and delegates are not registered.

**Solution**: Added a dummy USTRUCT to force UHT processing:
```cpp
USTRUCT()
struct FMOUIDelegatesModule
{
    GENERATED_BODY()
};
```

**Rule**: Any header file that declares dynamic delegates at file scope MUST have at least one UCLASS/USTRUCT/UENUM to trigger UHT processing.

### Template Methods Need Full Includes, Not Forward Declarations
**Problem**: `MOContextMenuBase.h` had `class UMOCommonButton;` forward declaration but used it in a template method `BindButtonClick<>()`.

**Root Cause**: Template methods are instantiated at compile time in the header. Forward declarations don't provide enough type information for template instantiation.

**Solution**: Changed from forward declaration to `#include "MOCommonButton.h"`.

**Rule**: If a header uses a type in a template method, you MUST include the full header, not forward declare.

## Runtime Testing Needed

### Context Menu Functionality
- [ ] Ghost context menu opens and closes correctly (Escape/Tab)
- [ ] Station context menu opens and closes correctly
- [ ] Harvest context menu opens and closes correctly
- [ ] Mouse leave grace period works (if enabled)
- [ ] All button clicks still function

### Item Database Caching
- [ ] Items load correctly on first access
- [ ] Items load from cache on subsequent access
- [ ] No crashes when DataTable is not configured
- [ ] Cache invalidation works if needed

### Deprecation Warning
- [ ] `FindInteractTarget()` shows deprecation warning in editor
- [ ] Code using `FindInteractTarget()` compiles but warns

## Blueprint Compatibility

### Verify These Work in Blueprints
- [ ] `SetPopupPosition` still callable from BP
- [ ] `OnRequestClose` delegate still bindable in BP
- [ ] `OnCloseRequested` delegate available in BP (new)
- [ ] Context menus still work when created via `CreateWidget`

## Future Refactoring (Lower Priority)

### Widgets That Could Still Migrate to Standard Delegates (FMOUIRequestClose)
- [ ] `MOBuildingMenu` - Has `FMOBuildingMenuRequestCloseSignature`
- [ ] `MOBuildWidget` - Has `FMOBuildWidgetRequestCloseSignature`
- [ ] `MOInGameMenu` - Has `FMOInGameMenuRequestCloseSignature`
- [ ] `MOLoadPanel` - Has `FMOLoadPanelRequestCloseSignature`
- [ ] `MOSavePanel` - Has `FMOSavePanelRequestCloseSignature`
- [ ] `MOOptionsPanel` - Has `FMOOptionsPanelRequestCloseSignature`
- [ ] `MOPossessionMenu` - Has `FMOPossessionMenuRequestCloseSignature`
- [ ] `MOSkillsPanel` - Has `FMOSkillsPanelRequestCloseSignature`
- [ ] `MOStatusPanel` - Has `FMOStatusPanelRequestCloseSignature`
- [ ] `MOPlayerStatusWidget` - Has `FMOPlayerStatusRequestCloseSignature`
- [ ] `MOUnifiedInventoryMenu` - Has `FMOOnUnifiedInventoryMenuClosed`

### Building Widgets That Could Use FMOUIRecipeSelected
- [ ] `MOBuildingMenu` - Has `FMOOnBuildingSelectedSignature`
- [ ] `MOBuildingEntryWidget` - Has `FMOOnBuildingEntryClickedSignature`
- [ ] `MOBuildingRecipeListWidget` - Has `FMOBuildRecipeSelectedSignature`
- [ ] `MOBuildingRecipeEntryWidget` - Has `FMOBuildRecipeEntryClickedSignature`
- [ ] `MOBuildWidget` - Has `FMOBuildingSelectedSignature`

### Additional Consolidation Opportunities
- [ ] Generic detail panel data structures
- [ ] Template base class for Database Settings (deferred - UE reflection limitations)

## UIManager Split Research (Task #4 Preparation)

**Current State**: `MOUIManagerComponent` has 50+ methods handling all UI operations, creating a bottleneck.

**Proposed Split into Specialized Controllers**:

| Controller | Responsibilities | Current Methods |
|------------|------------------|-----------------|
| `MOInventoryUIController` | Inventory menu, container UI, item context menus | `OpenInventoryMenu`, `CloseInventoryMenu`, `OpenInventoryWithContainer`, `ShowItemContextMenu` |
| `MOCraftingUIController` | Crafting menu, station context menus | `OpenCraftingMenu`, `ShowStationContextMenu`, `ShowKeepOnHarvestContextMenu` |
| `MOBuildingUIController` | Building menu, ghost context, build widget | `OpenBuildingMenu`, `ShowBuildWidget`, ghost menu handlers |
| `MOCharacterUIController` | Skills panel, status panels | `OpenSkillsPanel`, `HandleStatusPanelRequestClose` |
| `MOSystemMenuController` | In-game menu, possession menu | `OpenInGameMenu`, `OpenPossessionMenu` |
| `MONotificationComponent` | Already extracted | `ShowNotification`, `ShowSkillIncreaseNotification` |

**Shared Utilities to Keep in UIManager**:
- `CloseAllMenus()` - Orchestration across controllers
- `ApplyInputModeForMenuOpen/Closed()` - Input state management
- Modal background handling
- Focus/activation management

**Benefits**:
- Smaller, focused components (~100-150 lines each vs 1000+ monolithic)
- Easier to test individual UI systems
- Clear ownership of related widgets
- Reduces coupling - each system only knows its own UI

**Migration Strategy**:
1. Create new controller components
2. Move methods one category at a time
3. UIManager delegates to controllers
4. Eventually UIManager becomes thin orchestrator

**Risk**: Breaking Blueprint bindings if methods move. Need deprecation period.

## Code Removed (Consolidated)

### From MOGhostContextMenu
- `SetPopupPosition()` implementation
- `NativeOnKeyDown()` implementation
- `NativeOnMouseEnter()` implementation
- `NativeOnMouseLeave()` implementation
- Mouse tracking member variables

### From MOStationContextMenu
- `SetPopupPosition()` implementation
- `NativeOnKeyDown()` implementation

### From MOKeepOnHarvestContextMenu
- `SetPopupPosition()` implementation
- `NativeOnKeyDown()` implementation

## Estimated Code Reduction
- ~200 lines removed across 3 context menus
- Common patterns now in 1 base class (~75 lines)
- Net reduction: ~125 lines of duplicate code
- ~170+ delegate declarations could be consolidated (many migrated)

## Session Summary
- Created base class for context menus with shared functionality
- Created viewpoint utility class (used by 4 subsystems/components)
- Created standard UI delegate library with 15 delegate types
- Migrated 6 widgets to use standard delegates (backward compatible)
- Added caching to item database settings
- Deprecated legacy interaction method
- Maintained backward compatibility with existing delegates

## Standard Delegates Available (MOUIDelegates.h)

### Generic UI Delegates
- `FMOUIRequestClose` - Menu/panel close request
- `FMOUIActionTriggered` - Generic action with ActionId
- `FMOUIActionWithTarget` - Action with ActionId + GUID
- `FMOUIActionWithRecipe` - Action with ActionId + RecipeId
- `FMOUISelectionChanged` - Selection changed with FName
- `FMOUISelectionChangedGuid` - Selection changed with FGuid
- `FMOUIQuantityChanged` - Quantity changed (crafting amounts)
- `FMOUIConfirmationResult` - Dialog result (bool)
- `FMOUIProgressUpdate` - Progress + time remaining
- `FMOUIVisibilityChanged` - Visibility state

### Inventory Delegates
- `FMOUISlotClicked` - Slot clicked (index + GUID)
- `FMOUIDragStarted` - Drag started (source slot + GUID)
- `FMOUIDropCompleted` - Drop completed (source/target slot + GUID)

### Crafting Delegates
- `FMOUICraftRequest` - Craft request (RecipeId + Quantity)
- `FMOUIRecipeSelected` - Recipe selected (RecipeId)

### Building Delegates
- `FMOUIBuildRequest` - Build request (RecipeId + Location + Rotation)

### Notification Delegates
- `FMOUINotificationDismissed` - Notification dismissed (NotificationId)
