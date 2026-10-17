# UI Overhaul Architecture Document

**Last Updated:** 2026-03-31
**Purpose:** Cross-reference document for UI system refactoring. Use this to verify implementations and identify pitfalls.

---

## Executive Summary

The UI system was refactored to use CommonUI's native input handling instead of manual `SetInputMode()` calls and per-widget `NativeOnKeyDown()` overrides. The core issue was that key events don't bubble to parent widgets when child widgets (buttons, slots) have focus.

**Key Changes:**
- Created `UMOMenuWidgetBase` as common base class for all menu widgets
- Removed `NativeOnKeyDown` Tab/Escape handling from individual widgets
- Use `bIsBackHandler = true` and `GetDesiredInputConfig()` for CommonUI integration
- ~~Reference-counted cursor/input state via `UIManager->NotifyMenuActivated/Deactivated()`~~ **DEPRECATED - See Stage 3A**

---

## CRITICAL: Stage 0 Checklist

**VERIFY BEFORE ANY CODE CHANGES.** Do not proceed without all items confirmed.

- [ ] `GameViewportClientClass` in Project Settings > Maps & Modes is set to `CommonGameViewportClient`
- [ ] `Enable Enhanced Input Support` is true in Project Settings > CommonInput
- [ ] `CommonUI.DumpActivatableTree` console command produces output in PIE (confirms CommonUI is active)
- [ ] `SetInputMode` is not called anywhere in UI code
- [ ] `ActivateWidget()` is not called manually after a `PushWidget` call
- [ ] Document GameViewportClient status in CLAUDE.md

**If CommonGameViewportClient is NOT set:**
CommonUI's input routing is completely inactive. `GetDesiredInputConfig()` has no effect, and all input handling silently falls through to manual fallback code.

```ini
; In DefaultEngine.ini:
[/Script/EngineSettings.GeneralProjectSettings]
GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient
```

---

## Architecture Overview

### Class Hierarchy

```
UCommonActivatableWidget (Engine)
    └── UMOMenuWidgetBase (Our base class)
            ├── UMOInventoryMenu
            ├── UMOCraftingMenu
            ├── UMOBuildingMenu
            ├── UMOSkillsPanel
            ├── UMOStatusPanel
            ├── UMOInGameMenu
            ├── UMOPossessionMenu
            └── UMOUnifiedInventoryMenu

UCommonUserWidget (Engine)  ← CORRECT for context menus
    └── UMOContextMenuBase (MIGRATE from UCommonActivatableWidget)
            ├── UMOItemContextMenu
            ├── UMOGroundContextMenu
            ├── UMOGhostContextMenu
            ├── UMOStationContextMenu
            ├── UMOSurvivorContextMenu
            ├── UMOSurvivorTaskMenu
            └── UMOKeepOnHarvestContextMenu

UUserWidget (Engine)
    └── UMOBuildWidget (Building configuration popup - see Migration Notes)
```

### Input Flow

```
User presses Tab/Escape
    → CommonUI's back action system (via bIsBackHandler)
    → NativeOnHandleBackAction() on MENU widget
    → Menu calls RequestClose()
    → OnRequestClose delegate fires
    → UIController closes menu
    → Widget removed from stack
    → CommonUI restores input config from next widget on stack
    → If stack empty, RootContentWidget (WBP_GameplayStackStub) provides gameplay config
```

---

## Stage 3A: CommonUI Layer Stack Setup (REQUIRED)

### The Problem: ActiveMenuCount is Redundant

The current `NotifyMenuActivated`/`NotifyMenuDeactivated` system manually tracks menus and fights CommonUI. This causes:
- Pitfall 5 ("ActiveMenuCount Mismatch")
- Cursor staying visible after all menus close
- Input state drift between manual tracking and CommonUI

### The Solution: WBP_GameplayStackStub

When the widget stack empties, CommonUI needs a widget to restore input config from. Without a `RootContentWidgetClass`, behavior is undefined.

**Create UMOGameplayStackStub:**

```cpp
// MOGameplayStackStub.h
#pragma once
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOGameplayStackStub.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOGameplayStackStub : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UMOGameplayStackStub(const FObjectInitializer& ObjectInitializer);

protected:
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
};
```

```cpp
// MOGameplayStackStub.cpp
#include "MOGameplayStackStub.h"

UMOGameplayStackStub::UMOGameplayStackStub(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

TOptional<FUIInputConfig> UMOGameplayStackStub::GetDesiredInputConfig() const
{
    // Gameplay config: game receives input, mouse captured during mouse down
    FUIInputConfig Config(
        ECommonInputMode::Game,
        EMouseCaptureMode::CaptureDuringMouseDown,
        EMouseLockMode::DoNotLock,
        true  // Hide cursor during viewport capture (gameplay mode)
    );
    return Config;
}
```

**Blueprint Setup:**
1. Create `WBP_GameplayStackStub` (parent: `UMOGameplayStackStub`)
2. In `WBP_MOPrimaryGameLayout`, set `RootContentWidgetClass` on each stack to `WBP_GameplayStackStub`

### Code to DELETE After Stage 3A

**From MOUIManagerComponent.h:**
```cpp
// DELETE these:
void NotifyMenuActivated(UMOMenuWidgetBase* Menu);
void NotifyMenuDeactivated(UMOMenuWidgetBase* Menu);
int32 ActiveMenuCount = 0;
```

**From MOUIManagerComponent.cpp:**
```cpp
// DELETE entire implementations of NotifyMenuActivated and NotifyMenuDeactivated
```

**From MOMenuWidgetBase.h:**
```cpp
// DELETE:
void ApplyMenuInputState(bool bActivating);
```

**From MOMenuWidgetBase.cpp:**
```cpp
// DELETE ApplyMenuInputState implementation

// SIMPLIFY NativeOnActivated/NativeOnDeactivated:
void UMOMenuWidgetBase::NativeOnActivated()
{
    Super::NativeOnActivated();
    UE_LOG(LogMOFramework, Log, TEXT("[MOMenuWidgetBase] Activated: %s"), *GetName());
    // NO manual input state management - CommonUI handles it
}

void UMOMenuWidgetBase::NativeOnDeactivated()
{
    UE_LOG(LogMOFramework, Log, TEXT("[MOMenuWidgetBase] Deactivated: %s"), *GetName());
    Super::NativeOnDeactivated();
    // NO manual input state management - CommonUI handles it
}
```

### Replace IsAnyMenuOpen()

```cpp
// OLD (delete):
bool bAnyMenuOpen = UIManager->IsAnyMenuOpen();

// NEW (query layer stack directly):
bool bAnyMenuOpen = !MenuLayerStack->GetWidgetList().IsEmpty();

// OR via subsystem:
bool bAnyMenuOpen = UMOGameUIManagerSubsystem::Get(World)->IsGameLayerOccupied();
```

### Stage 3A Completion Checklist

- [ ] Created `UMOGameplayStackStub` C++ class
- [ ] Created `WBP_GameplayStackStub` Blueprint
- [ ] Set `WBP_GameplayStackStub` as `RootContentWidgetClass` on ALL layer stacks in `WBP_MOPrimaryGameLayout`
- [ ] Deleted `NotifyMenuActivated`, `NotifyMenuDeactivated`, `ActiveMenuCount` from `UMOUIManagerComponent`
- [ ] Simplified `NativeOnActivated`/`NativeOnDeactivated` in `UMOMenuWidgetBase`
- [ ] Verified colony bar target layer is `MO.UI.Layer.HUD` (not `MO.UI.Layer.Game`)
- [ ] Test in PIE: open inventory, close inventory, verify cursor hides and movement restores

---

## UMOMenuWidgetBase Implementation

### Header (MOMenuWidgetBase.h)

```cpp
UCLASS(Abstract)
class MOFRAMEWORK_API UMOMenuWidgetBase : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UMOMenuWidgetBase(const FObjectInitializer& ObjectInitializer);

    // Standard close delegate - bind to this for close events
    UPROPERTY(BlueprintAssignable, Category = "MO|UI|Menu")
    FMOUIRequestClose OnRequestClose;

    // Call to request menu close (broadcasts OnRequestClose)
    UFUNCTION(BlueprintCallable, Category = "MO|UI|Menu")
    virtual void RequestClose();

    bool DoesBlockGameInput() const { return bBlocksGameInput; }

protected:
    // CommonUI input configuration - replaces SetInputMode()
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    // Back action handler (Escape/Tab) - replaces NativeOnKeyDown
    virtual bool NativeOnHandleBackAction() override;

    // Focus management
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

    // Activation callbacks - logging only after Stage 3A
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;

    // Configuration
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MO|UI|Menu")
    bool bBlocksGameInput = true;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "MO|UI|Menu")
    TObjectPtr<UWidget> DefaultFocusWidget;
};
```

### Constructor Settings

```cpp
UMOMenuWidgetBase::UMOMenuWidgetBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Enable back action handling (Escape/Tab triggers NativeOnHandleBackAction)
    bIsBackHandler = true;
    bIsBackActionDisplayedInActionBar = false;

    // Enable focus restoration when widget reactivated
    bAutoRestoreFocus = true;

    // Widget is focusable
    SetIsFocusable(true);
}
```

### GetDesiredInputConfig Pattern (UE5.7)

**IMPORTANT:** `FUIInputConfig` members are protected in UE5.7. Use constructors.

```cpp
TOptional<FUIInputConfig> UMOMenuWidgetBase::GetDesiredInputConfig() const
{
    // UE5.7: Use constructor - members are protected
    // Parameters: InputMode, MouseCaptureMode, MouseLockMode, bHideCursorDuringViewportCapture
    //
    // CRITICAL: Use ECommonInputMode::All, NOT Menu
    // This allows Enhanced Input actions (toggle keys I, C, B) to still reach PlayerController
    // while CommonUI handles UI navigation and back actions
    FUIInputConfig Config(
        ECommonInputMode::All,           // Allow game input alongside UI
        EMouseCaptureMode::NoCapture,    // Don't capture mouse for game
        EMouseLockMode::DoNotLock,       // Don't lock mouse to viewport
        false                            // bHideCursorDuringViewportCapture
    );

    return Config;
}
```

**FUIInputConfig Constructors (UE5.7):**
```cpp
FUIInputConfig();  // Default
FUIInputConfig(ECommonInputMode, EMouseCaptureMode, bool bHideCursor = true);
FUIInputConfig(ECommonInputMode, EMouseCaptureMode, EMouseLockMode, bool bHideCursor = true);
```

---

## Migration Status

### Fully Migrated Widgets

| Widget | Base Class | Legacy Delegate | NativeOnKeyDown | Notes |
|--------|------------|-----------------|-----------------|-------|
| MOInventoryMenu | MOMenuWidgetBase | OnLegacyRequestClose | Removed | Standard migration |
| MOCraftingMenu | MOMenuWidgetBase | OnLegacyRequestClose | Removed | Standard migration |
| MOBuildingMenu | MOMenuWidgetBase | OnLegacyRequestClose | Removed | Standard migration |
| MOSkillsPanel | MOMenuWidgetBase | OnLegacyRequestClose | KEPT (Q/E only) | Category cycling |
| MOStatusPanel | MOMenuWidgetBase | OnLegacyRequestClose | KEPT (Q/E/←/→ only) | Category cycling |
| MOInGameMenu | MOMenuWidgetBase | OnLegacyRequestClose | Removed | Uses NativeOnHandleBackAction override |
| MOPossessionMenu | MOMenuWidgetBase | OnLegacyRequestClose | Removed | Standard migration |
| MOUnifiedInventoryMenu | MOMenuWidgetBase | OnLegacyRequestClose | N/A (never had) | Added close support |

### Pending Migration (Stage 5A)

| Widget | Current Parent | Target Parent | Notes |
|--------|---------------|---------------|-------|
| MOContextMenuBase | UCommonActivatableWidget | **UCommonUserWidget** | Ephemeral popups don't need activation machinery |
| MOItemContextMenu | MOContextMenuBase | (inherits) | |
| MOGroundContextMenu | MOContextMenuBase | (inherits) | |
| MOGhostContextMenu | MOContextMenuBase | (inherits) | |
| MOStationContextMenu | MOContextMenuBase | (inherits) | |
| MOSurvivorContextMenu | MOContextMenuBase | (inherits) | |
| MOSurvivorTaskMenu | MOContextMenuBase | (inherits) | |
| MOKeepOnHarvestContextMenu | MOContextMenuBase | (inherits) | |

**Why UCommonUserWidget for Context Menus:**
- Ephemeral popups that quickly appear/disappear
- Don't need to seize input from the rest of the UI
- Should NOT affect input routing
- Keep `NativeOnKeyDown` for Escape handling (correct for UUserWidget-style)

### NOT Migrated (By Design)

| Widget | Reason |
|--------|--------|
| MOBuildWidget | UUserWidget for building configuration popup. Used for ghost building config with 3D placement preview. Different interaction model than menu widgets. Keep as-is. |
| MOConfirmationDialog | Enter/Escape for Yes/No buttons. Needs special key handling. |

---

## Layer Constraints

### Widget Stack Behavior

`UCommonActivatableWidgetStack` only shows **one widget at a time**. Pushing a second widget onto the same stack collapses the first.

### Layer Assignment Guidelines

| Layer | Z-Order | Purpose | Widgets |
|-------|---------|---------|---------|
| HUD | 0 | Always-visible gameplay UI | Colony bar, health bar, compass |
| Game | 50 | Gameplay menus (one at a time) | Inventory, Crafting, Building, Skills |
| GameOverlay | 100 | Overlays on top of Game | Context menus (not using stack) |
| Menu | 150 | System menus | In-game menu, Possession menu |
| Modal | 200 | Modal dialogs | Confirmation dialogs |

**CRITICAL:** Colony bar MUST be on HUD layer, not Game layer. If on Game layer, opening inventory will collapse it.

---

## Debugging Tools

### CommonUI.DumpActivatableTree

Console command that dumps the currently active widget tree to output log.

**Use this to diagnose:**
- Wrong stack (widget pushed to wrong layer)
- Widget not activating (shows as inactive in tree)
- Input config not applying (wrong widget on top)
- Back action not firing (bIsBackHandler not set)

**Run this command whenever a CommonUI behavior is unexpected before looking at code.**

### Other Useful Commands

```
CommonUI.DebugInputRouter 1    ; Shows input routing decisions
Slate.ShowFocusedWidget 1      ; Shows currently focused widget
```

---

## Known Pitfalls and Edge Cases

### PITFALL 1: ECommonInputMode::Menu vs All

**Problem:** Using `ECommonInputMode::Menu` blocks Enhanced Input actions.
**Symptom:** Toggle keys (I, C, B) stop working when menu is open.
**Solution:** Use `ECommonInputMode::All` in `GetDesiredInputConfig()`.

### PITFALL 2: Missing bIsBackHandler

**Problem:** Widget never receives back action.
**Symptom:** Escape/Tab don't close menu.
**Solution:** Ensure constructor sets `bIsBackHandler = true`.

### PITFALL 3: Double Delegate Binding

**Problem:** Code binds to BOTH OnRequestClose AND OnLegacyRequestClose.
**Symptom:** Close handler fires twice.
**Solution:** Bind to only ONE delegate. Prefer base class OnRequestClose for new code.

### PITFALL 4: Focus Lost to Child Widget

**Problem:** After clicking button/slot, focus moves to child.
**Symptom:** NativeOnKeyDown-based close doesn't work.
**Solution:** FIXED by using `bIsBackHandler` instead of `NativeOnKeyDown`. CommonUI's back action works regardless of focus.

### PITFALL 5: ActiveMenuCount Mismatch (DEPRECATED)

**Problem:** Manual menu counting drifts out of sync with CommonUI.
**Symptom:** Cursor stays visible, movement stays locked after all menus closed.
**Solution:** DELETE ActiveMenuCount system entirely. Use WBP_GameplayStackStub as RootContentWidgetClass. See Stage 3A.

### PITFALL 6: Toggle Key During Animation

**Problem:** Pressing toggle key while menu is animating in/out.
**Symptom:** Menu state gets out of sync.
**Solution:** UIController should check `IsActivated()` before acting on toggle.

### PITFALL 7: RequestClose vs DeactivateWidget

**Problem:** Calling `DeactivateWidget()` directly instead of `RequestClose()`.
**Symptom:** OnRequestClose delegate doesn't fire, controller doesn't clean up.
**Solution:** Always call `RequestClose()`. It broadcasts delegate, then controller calls `DeactivateWidget()`.

### PITFALL 8: Subclass Forgets Super::RequestClose

**Problem:** Overriding RequestClose without calling Super.
**Symptom:** Base class OnRequestClose delegate doesn't fire.
**Solution:** Always call `Super::RequestClose()` in override.

### PITFALL 9: Blueprint Widget Missing BindWidget

**Problem:** Blueprint widget doesn't have required BindWidget properties.
**Symptom:** Compile error or null pointer crash.
**Solution:** Check header for `meta=(BindWidget)` properties and ensure Blueprint has matching widgets.

### PITFALL 10: NativeOnHandleBackAction Return Value

**Problem:** Returning false from NativeOnHandleBackAction.
**Symptom:** Back action propagates to other widgets unexpectedly.
**Solution:** Return true to consume the back action, false to let it propagate.

### PITFALL 11: FUIInputConfig Direct Member Access (UE5.7)

**Problem:** Trying to set FUIInputConfig members directly.
**Symptom:** Compilation error "cannot access protected member".
**Solution:** Use constructors instead of direct assignment.

```cpp
// WRONG (UE5.7)
FUIInputConfig Config;
Config.InputMode = ECommonInputMode::All;  // Error: protected

// CORRECT (UE5.7)
FUIInputConfig Config(ECommonInputMode::All, EMouseCaptureMode::NoCapture, EMouseLockMode::DoNotLock, false);
```

### PITFALL 12: Missing FObjectInitializer Constructor

**Problem:** Subclass uses default constructor instead of FObjectInitializer pattern.
**Symptom:** Compilation error "no appropriate default constructor available".
**Solution:** All MOMenuWidgetBase subclasses must use the FObjectInitializer constructor.

```cpp
// WRONG
UMOYourMenu::UMOYourMenu() { }

// CORRECT
UMOYourMenu::UMOYourMenu(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}
```

### PITFALL 13: Widget Re-Push State Corruption

**Problem:** Same widget instance pushed to stack after being popped retains stale state.
**Symptom:** Focus wrong on second open, input config not applying, visual state stale.
**Solution Options:**

**Option A (Simple):** Create new widget instance each time:
```cpp
UMOInventoryMenu* Menu = CreateWidget<UMOInventoryMenu>(PC, InventoryMenuClass);
MenuLayerStack->AddWidget(Menu);
// Don't cache - GC cleans up when removed
```

**Option B (Performant for frequent menus):** Reset state in NativeOnActivated:
```cpp
void UMOInventoryMenu::NativeOnActivated()
{
    Super::NativeOnActivated();
    ScrollBox->ScrollToStart();
    ClearSelection();
    // Reset any state that shouldn't persist
}
```

**Recommendation:** Option A for infrequent menus (possession, in-game). Option B for frequent menus (inventory, crafting).

### PITFALL 14: CommonGameViewportClient Not Set

**Problem:** Project using default `GameViewportClient` instead of `CommonGameViewportClient`.
**Symptom:** `GetDesiredInputConfig()` has no effect. All CommonUI input routing silently inactive.
**Solution:** Set in Project Settings > Maps & Modes, or in DefaultEngine.ini.

### PITFALL 15: Missing RootContentWidgetClass

**Problem:** Widget stack has no `RootContentWidgetClass` set.
**Symptom:** Input goes dead, cursor stays visible, or movement locked after all menus close.
**Solution:** Create `WBP_GameplayStackStub` and set as `RootContentWidgetClass` on all layer stacks.

---

## Testing Checklist

### Stage 0 Verification
- [ ] CommonGameViewportClient is set
- [ ] Enhanced Input Support is enabled in CommonInput
- [ ] `CommonUI.DumpActivatableTree` produces output

### Basic Functionality
- [ ] Open inventory (I key) - opens
- [ ] Press Tab while inventory open - closes
- [ ] Press Escape while inventory open - closes
- [ ] Press I while inventory open - closes (via Enhanced Input)
- [ ] Click inventory slot, then press Tab - STILL CLOSES (focus fix)
- [ ] Click button in menu, then press Escape - STILL CLOSES (focus fix)

### Menu Switching
- [ ] Open inventory (I), press C - inventory closes, crafting opens
- [ ] Open crafting (C), press B - crafting closes, building opens
- [ ] Open building (B), press I - building closes, inventory opens

### Nested UI
- [ ] Open in-game menu (Escape in-game)
- [ ] Click Options button - Options panel opens
- [ ] Press Escape - Options panel closes, menu stays open
- [ ] Press Escape again - Menu closes

### Context Menus
- [ ] Open inventory, right-click item - context menu appears
- [ ] Press Escape - context menu closes, inventory stays
- [ ] Press Tab - nothing happens (context menus ignore Tab)
- [ ] Click outside context menu - context menu closes

### Input State (After Stage 3A)
- [ ] Open menu - cursor visible (via GetDesiredInputConfig)
- [ ] Close menu - cursor hidden (via WBP_GameplayStackStub config)
- [ ] Open two menus (e.g., inventory then nested) - cursor still visible
- [ ] Close all menus - cursor hidden, movement restored
- [ ] NO manual PC input state manipulation occurring

### Widget Reuse
- [ ] Open inventory, close it, open again - state is fresh
- [ ] Selection cleared on reopen
- [ ] Scroll position reset on reopen

---

## Migration Stages Summary

| Stage | What | Key Actions |
|-------|------|-------------|
| 0 | Verification | Confirm CommonGameViewportClient, Enhanced Input, document in CLAUDE.md |
| 1 | Base Class | Create UMOMenuWidgetBase (DONE) |
| 2 | Widget Migration | Migrate menus to UMOMenuWidgetBase (DONE) |
| 3A | Layer Stack | Create WBP_GameplayStackStub, set RootContentWidgetClass, DELETE ActiveMenuCount |
| 3B | Layer Blueprint | Complete WBP_MOPrimaryGameLayout with all layers |
| 4A | Controller Updates | Update UIControllers to use layer stacks instead of AddToViewport |
| 5A | Context Menus | Migrate UMOContextMenuBase to UCommonUserWidget |
| 6A | Cleanup | Remove legacy delegates, simplify code |

---

## Files Reference

### New Files to Create
- `MOGameplayStackStub.h/cpp` - Gameplay input config widget
- `WBP_GameplayStackStub` - Blueprint child

### Files to Modify
- `MOUIManagerComponent.h/cpp` - DELETE NotifyMenuActivated/Deactivated/ActiveMenuCount
- `MOMenuWidgetBase.h/cpp` - DELETE ApplyMenuInputState, simplify activation callbacks
- `MOContextMenuBase.h/cpp` - Change parent to UCommonUserWidget (Stage 5A)
- `WBP_MOPrimaryGameLayout` - Set RootContentWidgetClass on all stacks

### Already Modified (Stage 1-2 Complete)
- `MOMenuWidgetBase.h/cpp` - Created
- `MOInventoryMenu.h/cpp` - Migrated
- `MOCraftingMenu.h/cpp` - Migrated
- `MOBuildingMenu.h/cpp` - Migrated
- `MOSkillsPanel.h/cpp` - Migrated
- `MOStatusPanel.h/cpp` - Migrated
- `MOInGameMenu.h/cpp` - Migrated
- `MOPossessionMenu.h/cpp` - Migrated
- `MOUnifiedInventoryMenu.h/cpp` - Migrated

---

## Quick Reference

### "My menu doesn't close on Tab/Escape"

1. Does widget inherit from `UMOMenuWidgetBase`?
2. Is `bIsBackHandler = true` set in constructor?
3. Is `GetDesiredInputConfig()` returning valid config?
4. Is `CommonGameViewportClient` set in Project Settings?
5. Is widget activated (not just added to viewport)?
6. Run `CommonUI.DumpActivatableTree` - is widget in the tree?

### "My toggle key doesn't work"

1. Is `GetDesiredInputConfig()` using `ECommonInputMode::All`?
2. Is `CommonGameViewportClient` set? (CRITICAL)
3. Is Enhanced Input mapping context still active?
4. Is PlayerController's input action handler firing?

### "Cursor stays visible after closing"

1. Is `WBP_GameplayStackStub` set as `RootContentWidgetClass`?
2. Is the widget stack actually empty? (`CommonUI.DumpActivatableTree`)
3. Is another menu still open?
4. DELETE any manual `ActiveMenuCount` code - use CommonUI's system

### "Close handler fires twice"

1. Are you binding to BOTH OnRequestClose AND OnLegacyRequestClose?
2. Choose one - prefer OnRequestClose for new code.

### "Widget state is stale on reopen"

1. Are you reusing the same widget instance?
2. Either create new instance each time, or reset state in NativeOnActivated.
