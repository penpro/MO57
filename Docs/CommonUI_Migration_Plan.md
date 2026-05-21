# CommonUI Migration Plan: Unified UI Architecture

## Executive Summary

This document outlines a plan to migrate the MO57 UI system from a hybrid/workaround approach to proper CommonUI integration. The goal is to eliminate manual input handling workarounds and leverage UE5.7's CommonUI framework as designed.

**Revision History:**
- v1.0 (2026-03-31): Initial plan
- v1.1 (2026-03-31): Incorporated audit corrections (see `commonui_migration_audit.md`)

---

## Part 1: How We Got Here (Context for Future Reference)

### The Original Sin: Widget Caching + AddToViewport

The UI system was built with a caching pattern:

```cpp
// Pattern used throughout the codebase
if (!CachedInventoryMenu)
{
    CachedInventoryMenu = CreateWidget<UMOInventoryMenu>(PC, MenuClass);
    CachedInventoryMenu->OnRequestClose.AddDynamic(this, &HandleClose);
}
CachedInventoryMenu->InitializeMenu(InventoryComponent);
CachedInventoryMenu->AddToViewport(ZOrder);  // <-- THE PROBLEM
```

**Why this was done:**
1. Perceived performance benefit (no widget construction on each open)
2. State preservation (scroll position, selected tabs)
3. Delegate bindings persist across open/close cycles

**Why this broke CommonUI:**
- `AddToViewport()` bypasses CommonUI's widget stack system
- Widget stacks register widgets with `UCommonUIActionRouterBase`
- Without registration, `bIsBackHandler`, `GetDesiredInputConfig()`, and other CommonUI features don't work
- The widgets *inherited* from `UCommonActivatableWidget` but weren't *used* as CommonUI intended

### The Cascade of Workarounds

1. **Problem**: Escape/Tab don't close menus
   - **Workaround**: Added `NativeOnKeyDown()` handlers to each widget

2. **Problem**: Focus lost when clicking buttons
   - **Workaround**: Added `NativeOnPreviewKeyDown()` to base class

3. **Problem**: Input mode conflicts
   - **Workaround**: Manual `SetInputMode()` calls alongside CommonUI's `GetDesiredInputConfig()`

4. **Problem**: Inconsistent patterns across menus
   - Some use `PushWidgetInstanceToLayer()`
   - Some use `SetVisibility()` toggle (Status panel)
   - Some use direct `AddToViewport()`
   - **Workaround**: Added `SetFocus()` calls in multiple places

5. **Problem**: Tests failing because ActionRouter not handling back actions
   - **Workaround**: Changed tests to use Slate key events instead of ActionRouter

**Each workaround fixed the immediate symptom but added technical debt and fragility.**

### The False Assumption

The assumption was: "We need caching for performance/state, so we must use AddToViewport()."

**Reality:**
- Widget construction overhead is minimal for typical game UIs
- State can be stored externally and restored on widget creation
- CommonUI stacks handle activation/deactivation efficiently
- The "performance benefit" of caching was never measured or validated

---

## Part 2: Current Architecture Problems

### Problem 1: Dual Input Systems Fighting Each Other

```cpp
// In MOMenuWidgetBase - trying to use CommonUI
bIsBackHandler = true;  // Does nothing without ActionRouter registration
GetDesiredInputConfig();  // Partially works, conflicts with manual SetInputMode

// In MOUIManagerComponent - manual input handling
PlayerController->SetInputMode(FInputModeGameAndUI().SetWidgetToFocus(...));
```

### Problem 2: Three Different Menu Display Patterns

| Pattern | Used By | Focus Handling | Activation |
|---------|---------|----------------|------------|
| `PushWidgetInstanceToLayer()` | Inventory, Crafting, Building, Skills | Manual SetFocus | Manual ActivateWidget |
| `SetVisibility()` toggle | Status Panel | Was missing! | Was missing! |
| `AddToViewport()` direct | Various context menus | Inconsistent | Inconsistent |

### Problem 3: Widget Lifecycle Not Managed by CommonUI

- Widgets created once, cached forever
- `NativeOnActivated`/`NativeOnDeactivated` called manually (sometimes)
- No automatic cleanup or state management
- Memory held even when widgets will never be used again

### Problem 4: Testing Requires Workarounds

Tests had to bypass CommonUI's ActionRouter and simulate raw Slate key events because our widgets aren't properly registered.

---

## Part 3: Target Architecture

### Core Principle: Use CommonUI As Designed

```cpp
// Target pattern for ALL menus
void UMOInventoryUIController::OpenInventoryMenu()
{
    UCommonActivatableWidgetContainerBase* MenuStack = GetMenuLayerStack();

    UMOInventoryMenu* Menu = MenuStack->AddWidget<UMOInventoryMenu>(InventoryMenuClass);
    Menu->InitializeWithInventory(GetCachedInventory());

    // That's it. CommonUI handles:
    // - Focus management
    // - Input routing (Escape/Tab via bIsBackHandler)
    // - Input mode (via GetDesiredInputConfig)
    // - Activation/deactivation lifecycle
    // - Widget cleanup when removed from stack
}
```

### Layer System (Already Partially Implemented)

```
WBP_PrimaryGameLayout
├── HUDLayer (UCommonActivatableWidgetStack) - Z:0
│   └── Persistent HUD elements
├── GameLayer (UCommonActivatableWidgetStack) - Z:50
│   └── In-world UI, interaction prompts
├── GameOverlayLayer (UCommonActivatableWidgetStack) - Z:100
│   └── Notifications, tooltips
├── MenuLayer (UCommonActivatableWidgetStack) - Z:150
│   └── Inventory, Crafting, Building, Skills, Status
└── ModalLayer (UCommonActivatableWidgetStack) - Z:200
    └── Confirmations, dialogs, context menus
```

### Widget Hierarchy

```
UCommonActivatableWidget (Engine)
└── UMOActivatableWidget (NEW - Our base class)
    ├── UMOMenuWidget (Gameplay menus - uses ECommonInputMode::All)
    │   ├── UMOInventoryMenu
    │   ├── UMOCraftingMenu
    │   ├── UMOBuildingMenu
    │   ├── UMOSkillsPanel
    │   └── UMOStatusPanel
    ├── UMOOverlayWidget (Non-blocking overlays)
    │   ├── UMONotificationWidget
    │   └── UMOTooltipWidget
    └── UMOModalWidget (Modal dialogs - uses ECommonInputMode::Menu + bIsModal)
        ├── UMOInGameMenu
        └── UMOConfirmationDialog

UCommonUserWidget (Engine - NOT activatable)
└── UMOContextMenuBase (Context menus - NOT in any stack)
    ├── UMOItemContextMenu
    ├── UMOGroundContextMenu
    └── UMOGhostContextMenu
```

**Key Design Decisions (from audit):**
1. **Context menus are NOT activatable widgets** - They're ephemeral popups with click-outside-dismiss, not stack-managed widgets
2. **UMOMenuWidget uses `ECommonInputMode::All`** - So toggle keys (I, C, B) pass through to Enhanced Input
3. **UMOModalWidget uses `ECommonInputMode::Menu` + `bIsModal = true`** - Full input blocking for dialogs

### Data Flow: Separation of Concerns

```
┌─────────────────────────────────────────────────────────────┐
│                     Game Components                          │
│  UMOInventoryComponent, UMOSkillsComponent, UMOVitals, etc. │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    UI Controllers                            │
│  UMOInventoryUIController, UMOCharacterUIController, etc.   │
│  - Decide WHEN to show UI                                   │
│  - Provide data to widgets                                  │
│  - Handle widget callbacks (craft request, item use, etc.)  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                  CommonUI Widget Stacks                      │
│  UCommonActivatableWidgetContainerBase (managed by engine)  │
│  - Manages widget lifecycle                                 │
│  - Routes input via ActionRouter                            │
│  - Handles focus transitions                                │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      UI Widgets                              │
│  UMOInventoryMenu, UMOCraftingMenu, etc.                    │
│  - Display data passed to them                              │
│  - Emit events (OnItemClicked, OnCraftRequested)            │
│  - NO game logic, NO component references cached            │
└─────────────────────────────────────────────────────────────┘
```

---

## Part 4: Migration Steps

### Phase 1: Foundation (Create New Base Classes)

**Goal:** Create proper base classes that use CommonUI correctly.

#### 1.1 Create UMOActivatableWidget

```cpp
// MOActivatableWidget.h
UCLASS(Abstract)
class UMOActivatableWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()
public:
    // IMPORTANT: FObjectInitializer pattern required for CommonUI widgets
    UMOActivatableWidget(const FObjectInitializer& ObjectInitializer);

protected:
    // Standardized input config - subclasses override
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    // Focus management - subclasses return their primary interactive element
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

    // Logging for debugging lifecycle issues
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
};

// MOActivatableWidget.cpp
UMOActivatableWidget::UMOActivatableWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Restore focus when this widget resurfaces after a modal closes
    bAutoRestoreFocus = true;
}
```

#### 1.2 Create UMOMenuWidget

```cpp
// MOMenuWidget.h - For gameplay menus (inventory, crafting, building, skills, status)
UCLASS(Abstract)
class UMOMenuWidget : public UMOActivatableWidget
{
    GENERATED_BODY()
public:
    UMOMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:
    // CRITICAL: Uses ECommonInputMode::All so toggle keys pass through
    // Combined with mapping context management to block WASD
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    virtual bool NativeOnHandleBackAction() override;
};

// MOMenuWidget.cpp
UMOMenuWidget::UMOMenuWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bIsBackHandler = true;  // Escape/Tab close
    bIsBackActionDisplayedInActionBar = false;
    SetIsFocusable(true);
}

TOptional<FUIInputConfig> UMOMenuWidget::GetDesiredInputConfig() const
{
    // ECommonInputMode::All = Input goes to BOTH UI and game
    // This allows toggle keys (I, C, B) to reach PlayerController
    // We block WASD via mapping context stripping, not input mode
    return FUIInputConfig(
        ECommonInputMode::All,
        EMouseCaptureMode::NoCapture,
        EMouseLockMode::DoNotLock,
        false  // bHideCursorDuringViewportCapture
    );
}

bool UMOMenuWidget::NativeOnHandleBackAction()
{
    // Let DeactivateWidget trigger stack auto-removal
    DeactivateWidget();
    return true;
}
```

#### 1.3 Create UMOModalWidget

```cpp
// MOModalWidget.h - For modal dialogs (in-game menu, confirmations)
UCLASS(Abstract)
class UMOModalWidget : public UMOActivatableWidget
{
    GENERATED_BODY()
public:
    UMOModalWidget(const FObjectInitializer& ObjectInitializer);

protected:
    // CRITICAL: Uses ECommonInputMode::Menu for full input blocking
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    virtual bool NativeOnHandleBackAction() override;
};

// MOModalWidget.cpp
UMOModalWidget::UMOModalWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bIsBackHandler = true;  // Escape closes
    bIsBackActionDisplayedInActionBar = false;
    bIsModal = true;  // CRITICAL: Block ALL input from reaching other widgets
    SetIsFocusable(true);
}

TOptional<FUIInputConfig> UMOModalWidget::GetDesiredInputConfig() const
{
    // ECommonInputMode::Menu = Input goes ONLY to UI, not game
    // Toggle keys won't work while modal is open (intentional)
    return FUIInputConfig(
        ECommonInputMode::Menu,
        EMouseCaptureMode::NoCapture,
        EMouseLockMode::DoNotLock,
        false
    );
}

bool UMOModalWidget::NativeOnHandleBackAction()
{
    DeactivateWidget();
    return true;
}
```

#### 1.4 Create UMOContextMenuBase (NOT Activatable)

```cpp
// MOContextMenuBase.h - For ephemeral context menus
UCLASS(Abstract)
class UMOContextMenuBase : public UCommonUserWidget
{
    GENERATED_BODY()
public:
    UMOContextMenuBase(const FObjectInitializer& ObjectInitializer);

    // Add to overlay panel (NOT a stack)
    void ShowAtPosition(const FVector2D& ScreenPosition);

    // Remove from overlay
    void Dismiss();

protected:
    // Handle Escape key to dismiss
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    // Click-outside detection via invisible backdrop
    UPROPERTY()
    TObjectPtr<class UMOContextMenuBackdrop> Backdrop;
};
```

#### 1.5 Create RootContentWidgetClass for Input Restoration

```cpp
// MOGameplayInputStub.h - Restores gameplay input when all menus close
UCLASS()
class UMOGameplayInputStub : public UCommonActivatableWidget
{
    GENERATED_BODY()
public:
    UMOGameplayInputStub(const FObjectInitializer& ObjectInitializer);

protected:
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
};

// MOGameplayInputStub.cpp
UMOGameplayInputStub::UMOGameplayInputStub(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // This widget is never removed from stacks
}

TOptional<FUIInputConfig> UMOGameplayInputStub::GetDesiredInputConfig() const
{
    // Restore full gameplay input when no menus are open
    return FUIInputConfig(
        ECommonInputMode::Game,
        EMouseCaptureMode::CaptureDuringMouseDown,
        EMouseLockMode::DoNotLock,
        true  // Hide cursor
    );
}
```

Set `WBP_GameplayInputStub` as `RootContentWidgetClass` on each `UCommonActivatableWidgetStack` in `WBP_PrimaryGameLayout`.

### Phase 1.5: Mapping Context Management

**Goal:** Suppress gameplay input (WASD, combat) while keeping toggle keys active.

Since `UMOMenuWidget` uses `ECommonInputMode::All`, Enhanced Input actions still fire. We need to control WHICH actions fire via mapping context management:

#### Create IMC_MenuToggle

A mapping context with only toggle actions:
- IA_ToggleInventory (I key)
- IA_ToggleCrafting (C key)
- IA_ToggleBuilding (B key)
- IA_ToggleSkills
- IA_ToggleStatus

#### Implement Context Swapping

```cpp
// In UMOMenuWidget::NativeOnActivated()
void UMOMenuWidget::NativeOnActivated()
{
    Super::NativeOnActivated();

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            // Remove gameplay contexts (movement, combat, interaction)
            Subsystem->RemoveMappingContext(IMC_Gameplay);
            Subsystem->RemoveMappingContext(IMC_Combat);

            // Keep toggle context active
            // (IMC_MenuToggle should already be added at game start)
        }
    }
}

// In UMOMenuWidget::NativeOnDeactivated()
void UMOMenuWidget::NativeOnDeactivated()
{
    Super::NativeOnDeactivated();

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ...)
        {
            // Restore gameplay contexts
            Subsystem->AddMappingContext(IMC_Gameplay, 0);
            Subsystem->AddMappingContext(IMC_Combat, 1);
        }
    }
}
```

**Note:** `UMOModalWidget` uses `ECommonInputMode::Menu` which blocks ALL game input, so no context management needed there.

### Phase 2: Infrastructure (Layer System)

**Goal:** Ensure the CommonUI layer system is fully functional.

#### 2.1 Complete WBP_PrimaryGameLayout Blueprint

- Add all 5 layer stacks as `UCommonActivatableWidgetStack` components
- Bind to C++ properties via `BindWidgetOptional`
- Test that widgets can be pushed/popped from each layer

#### 2.2 Update UMOGameUIManagerSubsystem

```cpp
// Provide easy access to layer stacks
UCommonActivatableWidgetContainerBase* GetHUDLayer();
UCommonActivatableWidgetContainerBase* GetGameLayer();
UCommonActivatableWidgetContainerBase* GetMenuLayer();
UCommonActivatableWidgetContainerBase* GetModalLayer();

// Convenience methods with dedicated server guard
template<typename T>
T* PushMenu(TSubclassOf<T> MenuClass)
{
    // Safety: no UI on dedicated server
    if (IsRunningDedicatedServer()) return nullptr;

    UCommonActivatableWidgetContainerBase* Stack = GetMenuLayer();
    if (!Stack) return nullptr;

    return Cast<T>(Stack->AddWidget(MenuClass));
}

template<typename T>
T* PushModal(TSubclassOf<T> ModalClass)
{
    if (IsRunningDedicatedServer()) return nullptr;

    UCommonActivatableWidgetContainerBase* Stack = GetModalLayer();
    if (!Stack) return nullptr;

    return Cast<T>(Stack->AddWidget(ModalClass));
}
```

#### 2.3 Remove Manual Input Mode Handling

Delete from `UMOUIControllerBase`:
- `ApplyInputModeForMenuOpen()`
- `ApplyInputModeForMenuClosed()`

These will be handled automatically by CommonUI's `GetDesiredInputConfig()`.

### Phase 3: Widget Migration (One at a Time)

**Goal:** Migrate each menu widget to the new pattern.

#### Migration Checklist Per Widget:

1. **Change base class** from `UMOMenuWidgetBase` to `UMOMenuWidget`

2. **Remove cached instance pattern** in controller:
   ```cpp
   // DELETE this pattern:
   TWeakObjectPtr<UMOInventoryMenu> CachedInventoryMenu;

   // REPLACE with:
   // No caching - let CommonUI manage lifecycle
   ```

3. **Update open method**:
   ```cpp
   void UMOInventoryUIController::OpenInventoryMenu()
   {
       if (UMOGameUIManagerSubsystem* UI = UMOGameUIManagerSubsystem::Get(this))
       {
           UMOInventoryMenu* Menu = UI->PushMenu<UMOInventoryMenu>(InventoryMenuClass);
           Menu->InitializeWithInventory(GetCachedInventory());
           // Done. No AddToViewport, no SetFocus, no manual activation.
       }
   }
   ```

4. **Update close method** (two paths):
   ```cpp
   // Path A: Back action (Escape/Tab) - handled automatically by widget
   // Widget::NativeOnHandleBackAction() calls DeactivateWidget()
   // Stack auto-removes the widget

   // Path B: Programmatic close (controller wants to close menu)
   void UMOInventoryUIController::CloseInventoryMenu()
   {
       if (UMOGameUIManagerSubsystem* UI = UMOGameUIManagerSubsystem::Get(this))
       {
           if (UCommonActivatableWidgetContainerBase* Stack = UI->GetMenuLayer())
           {
               if (UCommonActivatableWidget* Active = Stack->GetActiveWidget())
               {
                   if (Active->IsA<UMOInventoryMenu>())
                   {
                       Stack->RemoveWidget(*Active);
                   }
               }
           }
       }
   }
   ```

5. **Add IsMenuOpen() check** (for toggle handling):
   ```cpp
   bool UMOInventoryUIController::IsInventoryOpen() const
   {
       // Query the stack, don't cache widget references
       if (UMOGameUIManagerSubsystem* UI = UMOGameUIManagerSubsystem::Get(this))
       {
           if (UCommonActivatableWidgetContainerBase* Stack = UI->GetMenuLayer())
           {
               if (UCommonActivatableWidget* Active = Stack->GetActiveWidget())
               {
                   return Active->IsA<UMOInventoryMenu>();
               }
           }
       }
       return false;
   }

   void UMOInventoryUIController::ToggleInventoryMenu()
   {
       if (IsInventoryOpen())
       {
           CloseInventoryMenu();
       }
       else
       {
           OpenInventoryMenu();
       }
   }
   ```

6. **Remove from widget**:
   - `NativeOnKeyDown()` overrides for Escape/Tab (bIsBackHandler handles this)
   - `NativeOnPreviewKeyDown()` (no longer needed)
   - Manual `SetFocus()` calls
   - Manual input mode changes

7. **Keep in widget**:
   - `NativeOnKeyDown()` for non-close keys (Q/E for category cycling in Status)
   - Data display logic
   - Event delegates (OnItemClicked, etc.)
   - `NativeGetDesiredFocusTarget()` override returning primary interactive element

#### Menu Switching Strategy: SWAP

When opening Menu B while Menu A is open, we use the **Swap** pattern:
1. Remove Menu A from stack
2. Add Menu B to stack
3. No menu stacking for gameplay menus

```cpp
void UMOInventoryUIController::HandleSwitchToCrafting()
{
    // Swap strategy: remove current, add new
    if (UMOGameUIManagerSubsystem* UI = UMOGameUIManagerSubsystem::Get(this))
    {
        if (UCommonActivatableWidgetContainerBase* Stack = UI->GetMenuLayer())
        {
            // Remove current menu first
            if (UCommonActivatableWidget* Active = Stack->GetActiveWidget())
            {
                Stack->RemoveWidget(*Active);
            }

            // Then open crafting
            UI->PushMenu<UMOCraftingMenu>(CraftingMenuClass);
        }
    }
}
```

**Note:** Modals stack on top of menus normally (confirmation over inventory). Only gameplay menus swap.

#### Migration Order:

1. **MOInGameMenu** - Simplest, good test case
2. **MOInventoryMenu** - Core menu, validates pattern
3. **MOCraftingMenu** - Similar to inventory
4. **MOBuildingMenu** - Similar to crafting
5. **MOSkillsPanel** - Has category cycling to preserve
6. **MOStatusPanel** - Has category cycling and visibility pattern to fix
7. **Context menus** - Modal layer testing
8. **Confirmation dialogs** - Modal layer testing

### Phase 4: Cleanup

#### 4.1 Delete Obsolete Code

- `UMOMenuWidgetBase` (replaced by `UMOMenuWidget`)
- `PushWidgetInstanceToLayer()` methods
- Manual `SetFocus()` workarounds
- `ApplyInputModeForMenuOpen/Closed()`

#### 4.2 Update Tests

- Tests can use ActionRouter's `ProcessInput()` again
- Remove Slate key event workarounds
- Add tests for widget stack behavior

#### 4.3 Documentation

- Update CLAUDE.md with new patterns
- Add "How to create a new menu" guide
- Document layer system usage

---

## Part 5: Guidelines to Prevent Future Issues

### Rule 1: Use CommonUI Stacks for All UI

**DO:**
```cpp
UCommonActivatableWidgetContainerBase* Stack = GetMenuLayer();
UMyWidget* Widget = Stack->AddWidget<UMyWidget>(WidgetClass);
```

**DON'T:**
```cpp
Widget->AddToViewport();  // Bypasses CommonUI
Widget->SetVisibility(Visible);  // For cached widgets
```

### Rule 2: Don't Cache Widget INSTANCES (State Caching is OK)

CommonUI's `AddWidget` creates fresh instances each time - this is intentional.
Re-pushing previously removed widgets causes stale state and broken focus.

**DO:**
```cpp
// Create fresh each time, let CommonUI manage lifecycle
UMyMenu* Menu = Stack->AddWidget<UMyMenu>(MenuClass);
Menu->InitializeWithData(Data);
```

**DON'T:**
```cpp
// Cache widget instance and re-push it
if (!CachedMenu) CachedMenu = CreateWidget<>(...)
Stack->AddWidget(CachedMenu);  // BAD - stale state bugs!
```

**State preservation IS allowed:**
```cpp
// Store STATE in controller, not widget instance
struct FInventoryViewState
{
    int32 SelectedTabIndex;
    FVector2D ScrollPosition;
};
FInventoryViewState CachedViewState;

// On close - widget saves state to controller
void UMOInventoryMenu::NativeOnDeactivated()
{
    Super::NativeOnDeactivated();
    if (UMOInventoryUIController* Controller = GetController())
    {
        Controller->CachedViewState.SelectedTabIndex = CurrentTabIndex;
        Controller->CachedViewState.ScrollPosition = ScrollBox->GetScrollOffset();
    }
}

// On open - fresh widget restores state from controller
void UMOInventoryMenu::RestoreViewState(const FInventoryViewState& State)
{
    SelectTab(State.SelectedTabIndex);
    ScrollBox->SetScrollOffset(State.ScrollPosition);
}
```

### Rule 3: Let CommonUI Handle Input (Two-Tier Strategy)

**For Gameplay Menus (Inventory, Crafting, Building, Skills, Status):**
```cpp
// Uses All mode so toggle keys work
TOptional<FUIInputConfig> GetDesiredInputConfig() const override
{
    return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture);
}

// Block WASD via mapping context stripping in NativeOnActivated()
```

**For Modal Dialogs (In-Game Menu, Confirmation):**
```cpp
// Uses Menu mode for full input blocking
TOptional<FUIInputConfig> GetDesiredInputConfig() const override
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}
// Also set bIsModal = true in constructor
```

**DON'T:**
```cpp
PlayerController->SetInputMode(...);  // Conflicts with CommonUI
NativeOnKeyDown() { if (Escape) Close(); }  // Redundant with bIsBackHandler
```

### Rule 4: Widgets Display Data, Controllers Manage Logic

**Widget responsibilities:**
- Display data passed to it
- Emit events when user interacts
- Handle visual state (animations, hover, etc.)

**Controller responsibilities:**
- Decide when to open/close widgets
- Provide data to widgets
- Handle widget events (craft request, item use, etc.)
- Coordinate between widgets

### Rule 5: Test Using CommonUI APIs

**DO:**
```cpp
// Test back action via ActionRouter
ActionRouter->ProcessInput(EKeys::Escape, IE_Pressed);
```

**DON'T:**
```cpp
// Bypass CommonUI with raw Slate events
FSlateApplication::Get().ProcessKeyDownEvent(KeyEvent);
```

If tests need raw Slate events, that's a sign the UI isn't properly integrated with CommonUI.

### Rule 6: One Pattern for All Menus

Every menu should follow the exact same pattern:
1. Controller calls `Stack->AddWidget<>(Class)`
2. Controller calls `Widget->Initialize(Data)`
3. User interacts, widget emits events
4. Controller handles events
5. Widget's back action or controller calls `DeactivateWidget()`
6. CommonUI removes from stack, cleans up

No special cases. No "this menu is different because..."

---

## Part 6: Validation Checklist

Before considering migration complete:

### Functional Tests
- [ ] All menus open via stack push
- [ ] Escape closes all menus (via bIsBackHandler)
- [ ] Tab closes appropriate menus
- [ ] Toggle keys work (I for inventory, C for crafting, etc.)
- [ ] Focus automatically set when menu opens
- [ ] Focus restored when modal closes (bAutoRestoreFocus)
- [ ] Input blocked appropriately per menu type
- [ ] Cursor shows/hides appropriately

### Toggle Key Tests (Critical - from audit)
- [ ] Open inventory → press C → crafting opens (toggle through All mode)
- [ ] Open inventory → press WASD → character does NOT move (mapping context stripped)
- [ ] Open confirmation dialog → press I → inventory does NOT open (Menu mode blocks)
- [ ] Press I → opens → press I → closes → press I → opens (full toggle cycle, no stale state)

### Focus Tests (from audit)
- [ ] Open inventory → focus is on first slot (NativeGetDesiredFocusTarget)
- [ ] Open inventory → open modal → close modal → focus returns to inventory (bAutoRestoreFocus)

### Input Restoration (from audit)
- [ ] Open menu → close menu → cursor hidden, movement restored (RootContentWidgetClass)

### Context Menu Tests (from audit)
- [ ] Open inventory → right-click item → context menu appears
- [ ] Click outside context menu → context menu dismisses
- [ ] Escape key → context menu dismisses

### Debugging (from audit)
- [ ] `CommonUI.DumpActivatableTree` shows correct hierarchy after each migration step
- [ ] All widgets appear ABOVE the "*** Active Root ***" line
- [ ] All widgets show `IsActivated? [true]` when open

### Code Quality
- [ ] No `AddToViewport()` calls for menus
- [ ] No `SetInputMode()` calls (CommonUI handles this)
- [ ] No manual `SetFocus()` workarounds
- [ ] No `NativeOnKeyDown` for Escape/Tab handling
- [ ] No cached widget instances in controllers (state caching OK)
- [ ] All gameplay menus inherit from `UMOMenuWidget`
- [ ] All modal dialogs inherit from `UMOModalWidget`
- [ ] Context menus inherit from `UMOContextMenuBase` (NOT activatable)
- [ ] All base classes use `FObjectInitializer` constructor pattern
- [ ] All subsystem UI methods have `IsRunningDedicatedServer()` guard

### Tests
- [ ] All 69+ UI tests pass
- [ ] Tests use ActionRouter, not raw Slate events
- [ ] New tests for widget stack behavior

---

## Part 7: Debugging Tools

Use these CommonUI debug commands in PIE console:

```
CommonUI.DumpActivatableTree     - Shows all activatable widgets and their state
CommonUI.DebugInputRouter 1      - Shows real-time input routing decisions
Slate.ShowFocusedWidget 1        - Shows which widget currently has focus
```

### Troubleshooting: Widget Not Responding to Input

1. Run `CommonUI.DumpActivatableTree`
2. Verify your widget appears ABOVE the `*** Active Root ***` line
3. Verify `IsActivated? [true]` for your widget
4. Verify `LayerId` matches your expected layer (150 for Menu, 200 for Modal)
5. Check `Normal Bindings` count matches expected key bindings

### Troubleshooting: Focus Lost

1. Run `Slate.ShowFocusedWidget 1`
2. If focus is on an unexpected widget, check `NativeGetDesiredFocusTarget()` implementation
3. Verify `bAutoRestoreFocus = true` in base class constructor
4. Check for competing `SetFocus()` calls from Blueprint event handlers

---

## Part 8: Risk Assessment

### Low Risk
- New base classes (additive, doesn't break existing)
- Layer system completion (already partially done)

### Medium Risk
- Individual widget migration (can be done one at a time)
- Controller updates (localized changes)

### High Risk
- Removing caching (potential for subtle state bugs)
- Deleting workaround code (must ensure all widgets migrated first)

### Mitigation
- Migrate one widget at a time
- Keep old code until new pattern validated
- Run full test suite after each widget migration
- Manual testing of each menu's full interaction flow

---

## Appendix: Files to Modify

### New Files
- `MOActivatableWidget.h/cpp` - Base class for all activatable widgets
- `MOMenuWidget.h/cpp` - Base for gameplay menus (All input mode)
- `MOModalWidget.h/cpp` - Base for modal dialogs (Menu input mode + bIsModal)
- `MOContextMenuBase.h/cpp` - Base for context menus (NOT activatable)
- `MOGameplayInputStub.h/cpp` - RootContentWidgetClass for input restoration
- `IMC_MenuToggle` - Input mapping context with only toggle actions

### Major Modifications
- `MOGameUIManagerSubsystem.h/cpp` - Add layer access helpers
- `MOInventoryUIController.cpp` - Remove caching, use stacks
- `MOCraftingUIController.cpp` - Remove caching, use stacks
- `MOBuildingUIController.cpp` - Remove caching, use stacks
- `MOCharacterUIController.cpp` - Remove caching, use stacks
- `MOSystemMenuUIController.cpp` - Remove caching, use stacks

### Deletions (After Migration Complete)
- `MOMenuWidgetBase.h/cpp` - Replaced by new base classes
- `MOUIControllerBase::ApplyInputModeForMenuOpen/Closed`
- `MOUIControllerBase::PushWidgetInstanceToLayer`

### Widget Updates (Change Base Class)

**To UMOMenuWidget (gameplay menus):**
- `MOInventoryMenu.h`
- `MOCraftingMenu.h`
- `MOBuildingMenu.h`
- `MOSkillsPanel.h`
- `MOStatusPanel.h`

**To UMOModalWidget (modal dialogs):**
- `MOInGameMenu.h`
- `MOConfirmationDialog.h`

**To UMOContextMenuBase (NOT activatable):**
- `MOItemContextMenu.h`
- `MOGroundContextMenu.h`
- `MOGhostContextMenu.h`

---

## Summary

The path from here to proper CommonUI integration is:

1. **Create new base classes** that use CommonUI correctly
2. **Complete the layer system** infrastructure
3. **Migrate widgets one by one** to the new pattern
4. **Delete workaround code** after migration
5. **Establish guidelines** to prevent regression

The key insight: **CommonUI is an all-or-nothing framework.** Either use it as designed (widget stacks, ActionRouter, automatic input handling) or don't use it at all. The hybrid approach we had created compounding complexity and fragility.

This migration will result in:
- Less code (removing workarounds)
- More predictable behavior (one pattern for all menus)
- Easier debugging (CommonUI handles the complex parts)
- Better maintainability (following engine conventions)

---

## Audit Acknowledgment

This plan was audited against:
- Milton Candelero - "CommonUI Demystified" (Aug 2025)
- X157 Dev Notes - Common UI Activatable Widget & Action Router
- Tomasz Merda - Integrating CommonUI and Enhanced Input (UE5.5)
- Snorri Sturluson - CommonUI Playground (GitHub)
- Stajky - CommonUIMenuTemplate (GitHub)
- Dillon Bellefeuille - UI Layers Manager Plugin (March 2026)
- Epic Forums - CommonUI bug reports and discussions

Key corrections from audit:
1. **Input mode split**: `All` for menus (toggle keys work), `Menu` for modals (full block)
2. **Close paths defined**: Back action → `DeactivateWidget()`, programmatic → `Stack->RemoveWidget()`
3. **Context menus extracted**: NOT activatable, use `UCommonUserWidget` base
4. **Caching clarified**: Don't cache widget instances, state caching OK
5. **RootContentWidgetClass added**: For proper input restoration when all menus close

See `commonui_migration_audit.md` for full audit details.
