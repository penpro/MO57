# CommonUI Migration Plan — Audit & Recommendations

**Audited:** 2026-03-31
**Source Document:** `CommonUI_Migration_Plan.md`
**Purpose:** Feed this into Claude Code alongside the migration plan. Contains a detailed audit of every architectural decision, validated against UE5.7 CommonUI best practices, Lyra patterns, and community-tested approaches.

---

## Audit Verdict: STRONG PLAN — 4 CORRECTIONS, 3 WARNINGS, 5 ENHANCEMENTS

The core thesis is correct: **CommonUI is all-or-nothing, and straddling old and new systems is the root cause of your bugs.** This is confirmed by every authoritative source. The plan to go fully CommonUI with stack-managed widgets is the right call. However, there are specific decisions in the plan that conflict with how CommonUI actually works, and several gaps that will cause new problems if not addressed.

---

## CORRECTIONS (Must Fix — These Will Cause Bugs)

### CORRECTION 1: `ECommonInputMode::Menu` Will Break Toggle Keys

**Location:** Rule 3, `UMOMenuWidget::GetDesiredInputConfig()`, line ~427
**The Plan Says:**
```cpp
return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
```

**The Problem:**

`ECommonInputMode::Menu` blocks ALL game input from reaching the PlayerController. This means Enhanced Input actions for toggle keys (I for inventory, C for crafting, B for building) will stop working when any menu is open. The plan's own validation checklist at line 488 says "Toggle keys work (I for inventory, C for crafting, etc.)" — but that will fail with `ECommonInputMode::Menu`.

This is the exact same issue documented in the previous architecture doc's Pitfall 1. The plan appears to have over-corrected: the old system used `ECommonInputMode::All` which leaked gameplay input, and now the plan swings to `ECommonInputMode::Menu` which blocks too much.

**What the Sources Say:**

The Milton Candelero "CommonUI Demystified" post (August 2025) explains the three input modes clearly: `Game` sends input only to the game, `Menu` sends input only to the UI, and `All` sends to both. There is no built-in "Game for some actions, Menu for others" mode.

The Tomasz Merda UE5.5 integration guide uses `ECommonInputMode::All` for gameplay menus where you need both UI and game input, combined with mapping context management to control which game actions are actually live.

The Lyra pattern uses `ECommonInputMode::Menu` for system menus (settings, pause) where game input should be fully blocked, and a different config for HUD elements.

**The Fix:**

You need TWO input mode strategies, not one:

```cpp
// UMOMenuWidget (inventory, crafting, building, skills)
// Uses All so toggle keys reach the PlayerController
TOptional<FUIInputConfig> UMOMenuWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(
        ECommonInputMode::All,
        EMouseCaptureMode::NoCapture,
        EMouseLockMode::DoNotLock,
        false  // show cursor
    );
}

// UMOModalWidget (confirmation dialogs, in-game menu, pause)
// Uses Menu to fully block game input
TOptional<FUIInputConfig> UMOModalWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(
        ECommonInputMode::Menu,
        EMouseCaptureMode::NoCapture,
        EMouseLockMode::DoNotLock,
        false  // show cursor
    );
}
```

To suppress unwanted gameplay actions (WASD, abilities) while keeping toggle keys alive under `ECommonInputMode::All`, use Enhanced Input mapping context swapping:

```cpp
// When a menu using All mode activates:
// Remove IMC_Gameplay (movement, combat, interaction)
// Keep  IMC_MenuToggle (I, C, B, Escape toggle bindings)

// When the last menu deactivates:
// Restore IMC_Gameplay
```

The X157 dev notes on Lyra Input confirm this is the standard approach: CommonUI controls WHETHER input reaches the game, Enhanced Input mapping contexts control WHAT happens when it does.

**Action Items:**
- [ ] Split `GetDesiredInputConfig()` between `UMOMenuWidget` (All) and `UMOModalWidget` (Menu)
- [ ] Create `IMC_MenuToggle` mapping context with only toggle actions
- [ ] Implement mapping context swap in `NativeOnActivated`/`NativeOnDeactivated`
- [ ] Test: open inventory → press C → crafting opens (toggle works)
- [ ] Test: open inventory → press WASD → character does NOT move

---

### CORRECTION 2: `DeactivateWidget()` Is Wrong for Menu Closing

**Location:** Phase 3, step 4 (line ~331) and Rule 6, step 5 (line ~474)
**The Plan Says:**
```cpp
void UMOInventoryUIController::CloseInventoryMenu()
{
    // Widget's DeactivateWidget() handles removal from stack
}
```
And Rule 6: "Widget's back action or controller calls `DeactivateWidget()`"

**The Problem:**

The X157 Activatable Widget documentation states explicitly that activatable widgets managed by a `UCommonActivatableWidgetContainerBase` (which includes `UCommonActivatableWidgetStack`) have their lifecycle managed by the container. The correct way to remove a widget from a stack is through the stack, not by calling `DeactivateWidget()` directly on the widget.

When you call `DeactivateWidget()` on a widget that's in a stack, the stack detects the deactivation and removes it — but this is the INDIRECT path. The direct path is `Stack->RemoveWidget(*Widget)`. More importantly, for back-action closes, `NativeOnHandleBackAction()` should call `DeactivateWidget()` which triggers the stack's auto-removal behavior. But for programmatic closes (like toggling to a different menu), the controller should go through the stack.

The Snorri Sturluson CommonUI Playground code demonstrates the canonical pattern: `PushModal` calls `Stack->AddWidget()`, `PopModal` calls `Stack->RemoveWidget()`.

Additionally, the previous architecture doc had a `RequestClose()` pattern with a delegate, and the plan doesn't address what replaces that communication channel between widget and controller.

**The Fix:**

```cpp
// For back-action closes (user presses Escape):
// This is handled automatically by bIsBackHandler → NativeOnHandleBackAction()
bool UMOMenuWidget::NativeOnHandleBackAction()
{
    // Option A: Let DeactivateWidget trigger stack removal (simple)
    DeactivateWidget();
    return true;

    // Option B: Broadcast a delegate so the controller can do cleanup first
    // OnMenuCloseRequested.Broadcast(this);
    // return true;
}

// For programmatic closes (controller wants to close a specific menu):
void UMOInventoryUIController::CloseInventoryMenu()
{
    if (UCommonActivatableWidgetContainerBase* Stack = GetMenuLayer())
    {
        // Find the active widget and remove it through the stack
        if (UCommonActivatableWidget* Active = Stack->GetActiveWidget())
        {
            Stack->RemoveWidget(*Active);
        }
    }
}

// For toggle closes (press I while inventory is open):
void UMOInventoryUIController::ToggleInventoryMenu()
{
    if (IsInventoryOpen())
    {
        CloseInventoryMenu();  // Goes through stack
    }
    else
    {
        OpenInventoryMenu();   // AddWidget to stack
    }
}
```

**Action Items:**
- [ ] Define the canonical close path: back action → `DeactivateWidget()` (auto-removed by stack)
- [ ] Define the programmatic close path: controller → `Stack->RemoveWidget()`
- [ ] Define how the controller knows a menu was closed (delegate from widget, or poll stack state)
- [ ] Ensure `IsInventoryOpen()` queries the stack, not a cached reference

---

### CORRECTION 3: Context Menus Should NOT Be on the Modal Layer

**Location:** Widget Hierarchy (line ~158), Layer System (line ~139), Migration Order (line ~356)
**The Plan Says:**
Context menus are under `UMOModalWidget` and the migration order says "Context menus - Modal layer testing"

**The Problem:**

Context menus and modal dialogs have fundamentally different behaviors:

- **Modal dialogs** block all input except their own buttons. They should seize focus and not let it escape. `bIsModal = true` is appropriate. They belong on the Modal layer.
- **Context menus** are ephemeral popups that should dismiss on click-outside, don't seize full input focus, and should NOT block the parent menu from being visible. They do NOT belong in a stack at all.

The `UCommonActivatableWidgetStack` shows ONLY ONE widget at a time. If you push a context menu onto the Modal stack, the parent menu on the Menu stack remains visible (stacks on different layers are independent), but if you push TWO context menus, the first disappears. More critically, the stack's activation/deactivation lifecycle doesn't match context menu semantics.

The Lyra pattern puts context menus on the GameOverlay layer, NOT the Modal layer. The previous architecture doc correctly identified context menus as `UCommonUserWidget` descendants (not activatable) that live on the overlay layer.

The Stajky CommonUIMenuTemplate and Dillon Bellefeuille UI Layers Manager both use the four-layer pattern (Game, GameMenu, Menu, Modal) with context menus handled separately from the stack system entirely.

**The Fix:**

```
UCommonActivatableWidget (Engine)
└── UMOActivatableWidget (base class)
    ├── UMOMenuWidget (Menus - pushed to MenuLayer stack)
    │   ├── UMOInventoryMenu
    │   ├── UMOCraftingMenu
    │   └── ...
    └── UMOModalWidget (Modals - pushed to ModalLayer stack)
        └── UMOConfirmationDialog

UCommonUserWidget (Engine)
└── UMOContextMenuBase (NOT activatable, NOT in any stack)
    ├── UMOItemContextMenu
    ├── UMOGroundContextMenu
    └── ...
```

Context menus should be added directly to the GameOverlay layer as non-stack children (AddChild to an overlay panel, not pushed to a widget stack), with manual dismiss handling via a click-outside backdrop.

**Action Items:**
- [ ] Move context menus OUT of `UMOModalWidget` hierarchy
- [ ] Create `UMOContextMenuBase` inheriting from `UCommonUserWidget`
- [ ] Context menus are added to a non-stack overlay panel, not pushed to any `UCommonActivatableWidgetStack`
- [ ] Implement click-outside-to-dismiss with a transparent backdrop
- [ ] Escape handling via `NativeOnKeyDown` (correct for non-activatable widgets)

---

### CORRECTION 4: Rule 2 "No Widget Caching" Contradicts CommonUI's Design

**Location:** Rule 2 (line ~399), Part 2 Problem Statement (line ~63)
**The Plan Says:**
"No Widget Caching for Menus" — create fresh each time, let CommonUI manage lifecycle.

**The Problem:**

The X157 Activatable Widget documentation says the following about CommonUI's intended usage pattern:

> "Activatable Widgets are often not deleted, instead they're reused. OnActivated and OnDeactivated can be called often in a single lifetime of the widget."

CommonUI was DESIGNED for widget reuse. The activation/deactivation cycle IS the caching mechanism. The stack deactivates widgets when they're pushed down and reactivates them when they surface again. The `RootContentWidgetClass` on each stack is never destroyed — it persists for the life of the stack.

The plan correctly identifies that the OLD caching pattern (cache + `AddToViewport`) was broken. But the fix isn't "never cache" — it's "let CommonUI do the caching through its stack."

However, there IS a nuance. The `UCommonActivatableWidgetStack` source shows that when a widget deactivates and is removed from the stack (not just pushed down), it IS released. And `AddWidget` creates a new instance each time. So for the pattern where you push a menu, close it (removing from stack), then reopen it later — `AddWidget` will create a new one, which is correct.

The problem the plan is reacting to is the CONTROLLER caching pattern (`TWeakObjectPtr<UMOInventoryMenu> CachedInventoryMenu`), not CommonUI's internal behavior. That distinction matters.

The Epic forum thread "Common Activatable Widget Stack very inconsistent behavior" (March 2024) documents exactly the problem with pushing a previously-removed widget back onto a stack — stale state, broken focus, incorrect activation. The fix IS creating new instances via `AddWidget`, not reusing removed ones.

**The Fix:**

Refine Rule 2 to be precise about WHAT not to cache:

```
Rule 2: Don't Cache Widget Instances Across Open/Close Cycles

DO:
  // Let AddWidget create a fresh instance each time
  UMyMenu* Menu = Stack->AddWidget<UMyMenu>(MenuClass);

DON'T:
  // Don't hold a reference and try to re-push it
  if (!CachedMenu) CachedMenu = CreateWidget<>(...);
  Stack->AddWidget(CachedMenu); // BAD - stale state

OK (if you need state persistence):
  // Store STATE externally, create fresh widget, restore state
  FInventoryViewState SavedState = LastViewState;
  UMyMenu* Menu = Stack->AddWidget<UMyMenu>(MenuClass);
  Menu->RestoreViewState(SavedState);
```

The distinction: don't cache the WIDGET INSTANCE. Caching DATA/STATE in the controller is fine and expected.

**Action Items:**
- [ ] Reword Rule 2: "Don't cache widget instances" not "no caching"
- [ ] Add pattern for state preservation via controller-held data objects
- [ ] Clarify that `AddWidget` creates new instances (this is correct behavior)
- [ ] Add `SaveViewState()` / `RestoreViewState()` virtual methods to `UMOMenuWidget` for menus that need state persistence (scroll position, selected tab)

---

## WARNINGS (Won't Break Immediately, But Will Bite You)

### WARNING 1: Stack Shows Only One Widget — Menu Switching Needs a Strategy

**The Problem:**

The plan puts Inventory, Crafting, Building, Skills, and Status ALL on the MenuLayer stack. A `UCommonActivatableWidgetStack` shows only one widget at a time. When you push Crafting while Inventory is open, Inventory gets deactivated and hidden.

This is probably fine if you want "one menu at a time." But you need to decide the behavior:

**Option A: Replace** — Opening Crafting while Inventory is open removes Inventory and pushes Crafting. Back action from Crafting returns to gameplay (no Inventory underneath).

**Option B: Stack** — Opening Crafting pushes it ON TOP of Inventory. Back action from Crafting returns to Inventory. This is how nested menus (Settings inside Pause Menu) work in Lyra.

**Option C: Swap** — Controller explicitly removes current menu and pushes new one. Clean stack, no nesting.

The Zerol dev notes warn that inter-stack communication is not built-in to CommonUI. If the Menu stack empties, there's no automatic notification to the Game stack.

**Action Items:**
- [ ] Decide: Replace, Stack, or Swap for menu-to-menu transitions
- [ ] If Swap: controller must `RemoveWidget` old before `AddWidget` new
- [ ] If Stack: test that Back returns to previous menu, not gameplay
- [ ] Document the chosen strategy in the migration plan

---

### WARNING 2: Toggle Key Handling Needs an "IsOpen" Check That Queries the Stack

**The Problem:**

The plan removes cached widget references from controllers. Good. But toggle keys (press I to open inventory, press I again to close) require knowing whether the menu is currently open. Without a cached reference, the controller needs another way to check.

**The Fix:**

```cpp
bool UMOInventoryUIController::IsInventoryOpen() const
{
    // Query the stack for any active widget of this type
    UCommonActivatableWidgetContainerBase* Stack = GetMenuLayer();
    if (!Stack) return false;

    // GetActiveWidget returns the top of the stack
    return Stack->GetActiveWidget() &&
           Stack->GetActiveWidget()->IsA<UMOInventoryMenu>();
}
```

Or, maintain a lightweight flag in the controller that's set on push and cleared on the widget's deactivation delegate:

```cpp
void UMOInventoryUIController::OpenInventoryMenu()
{
    UMOInventoryMenu* Menu = Stack->AddWidget<UMOInventoryMenu>(MenuClass);
    Menu->OnDeactivated().AddUObject(this, &ThisClass::OnInventoryDeactivated);
    bInventoryOpen = true;
}

void UMOInventoryUIController::OnInventoryDeactivated(UCommonActivatableWidget* Widget)
{
    bInventoryOpen = false;
}
```

**Action Items:**
- [ ] Add `IsMenuOpen()` pattern to the plan — either stack query or deactivation delegate
- [ ] Ensure toggle handlers check this before opening/closing
- [ ] Test: press I → inventory opens → press I → inventory closes → press I → inventory opens again

---

### WARNING 3: The `FObjectInitializer` Constructor Pattern May Be Required

**The Problem:**

The plan's new base classes use default constructors:
```cpp
UMOActivatableWidget();
```

The previous architecture doc's Pitfall 12 documents that `UCommonActivatableWidget` subclasses require the `FObjectInitializer` constructor pattern. The plan doesn't mention this. If it was a real issue before, it's still a real issue now.

**The Fix:**

```cpp
// All base classes should use FObjectInitializer
UMOActivatableWidget(const FObjectInitializer& ObjectInitializer);
UMOMenuWidget(const FObjectInitializer& ObjectInitializer);
UMOModalWidget(const FObjectInitializer& ObjectInitializer);
```

**Action Items:**
- [ ] Use `FObjectInitializer` constructor pattern on all new base classes
- [ ] Document this requirement for any future widget subclasses

---

## ENHANCEMENTS (Improvements to Make the Plan More Robust)

### ENHANCEMENT 1: Add a RootContentWidgetClass for Gameplay Input Restoration

**The Problem:**

The plan doesn't mention `RootContentWidgetClass`. When the last widget is removed from a stack, CommonUI needs a widget to restore input config FROM. Without a `RootContentWidgetClass`, input state becomes undefined when all menus close.

The Tomasz Merda guide (UE5.5+) says explicitly: "To properly handle the InputConfigs in Stack make sure that there's a default RootContentWidgetClass that handles setting it to Gameplay configuration."

The Milton Candelero post confirms: "If you only had one activatable that had a 'Menu Only Config', once you deactivate it, you won't get back to a 'Game Config'! Keep a bottom 'HUD' activatable that you never deactivate."

The previous architecture doc called this `WBP_GameplayStackStub`. The plan should include it.

**The Fix:**

```cpp
UCLASS()
class UMOGameplayInputStub : public UCommonActivatableWidget
{
    GENERATED_BODY()
public:
    UMOGameplayInputStub(const FObjectInitializer& ObjectInitializer);
protected:
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override
    {
        // Restore gameplay input when stack empties
        return FUIInputConfig(
            ECommonInputMode::Game,
            EMouseCaptureMode::CaptureDuringMouseDown,
            EMouseLockMode::DoNotLock,
            true  // hide cursor
        );
    }
};
```

Set this as `RootContentWidgetClass` on each `UCommonActivatableWidgetStack` in `WBP_PrimaryGameLayout`.

**Action Items:**
- [ ] Create `UMOGameplayInputStub` (or equivalent)
- [ ] Create `WBP_GameplayInputStub` Blueprint child
- [ ] Set as `RootContentWidgetClass` on ALL layer stacks
- [ ] Test: open menu → close menu → cursor hidden, movement restored

---

### ENHANCEMENT 2: Add the Activatable Tree Dump to Your Debug Workflow

**The Problem:**

The plan mentions testing but doesn't include CommonUI's built-in debugging tools.

The Milton Candelero post details `CommonUI.DumpActivatableTree` — a console command that shows every activatable widget, whether it's activated, its layer ID, and how many input bindings it has. The "Active Root" section shows which widget tree is actually receiving input.

**The Fix:**

Add to the plan's debug section:

```
Debug Commands (run in PIE console):
  CommonUI.DumpActivatableTree     — Shows all activatable widgets and their state
  CommonUI.DebugInputRouter 1      — Shows real-time input routing decisions
  Slate.ShowFocusedWidget 1        — Shows which widget currently has focus

If a widget isn't responding to input:
  1. Run DumpActivatableTree
  2. Verify your widget appears ABOVE the *** Active Root *** line
  3. Verify IsActivated? [true]
  4. Verify LayerId is correct for your target layer
  5. Check Normal Bindings count matches expected
```

**Action Items:**
- [ ] Add debug commands section to the plan
- [ ] Add `DumpActivatableTree` check to every migration step's verification

---

### ENHANCEMENT 3: Address the `bIsModal` Flag for True Modal Behavior

**The Problem:**

The plan creates `UMOModalWidget` but doesn't set `bIsModal` on it. In CommonUI, `bIsModal` is a specific flag on `UCommonActivatableWidget` that makes the widget consume ALL input events, preventing them from cascading to any other activatable widget in the tree. This is different from `bIsBackHandler`.

The Milton Candelero post explains: "There is a special case, `bIsModal` which makes an activatable handle all requests and stop cascading to its children. Effectively eating all inputs that come their way."

For a confirmation dialog, you likely WANT `bIsModal = true` so that no other widget can receive input while the dialog is showing.

**The Fix:**

```cpp
UMOModalWidget::UMOModalWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bIsBackHandler = true;
    bIsBackActionDisplayedInActionBar = false;
    bIsModal = true;  // Block ALL input from reaching other widgets
    SetIsFocusable(true);
}
```

**Action Items:**
- [ ] Set `bIsModal = true` on `UMOModalWidget`
- [ ] Do NOT set `bIsModal` on `UMOMenuWidget` (toggle keys need to pass through)
- [ ] Test: open confirmation dialog → press I → inventory should NOT open

---

### ENHANCEMENT 4: Add NativeGetDesiredFocusTarget to Base Classes

**The Problem:**

The plan mentions focus management ("Focus automatically set when menu opens" in the validation checklist) but doesn't include the implementation.

The Milton Candelero post explains the focus resolution chain in detail: when an activatable widget becomes the foremost active widget, CommonUI asks it for `GetDesiredFocusTarget()`. If it doesn't return a valid target, you get increasingly bad fallback behavior, ending with "focus lost in the void."

The X157 Activatable Widget notes confirm: the result of `GetDesiredFocusTarget` determines where focus goes on activation.

**The Fix:**

```cpp
// In UMOMenuWidget base:
virtual UWidget* NativeGetDesiredFocusTarget() const override
{
    // Subclasses should override to return their primary interactive element
    // (first button, first list item, search field, etc.)
    // The base implementation returns nullptr which triggers fallback behavior
    return nullptr;
}

// In a specific menu:
UWidget* UMOInventoryMenu::NativeGetDesiredFocusTarget() const
{
    // Focus the first inventory slot when menu opens
    if (InventoryGrid && InventoryGrid->GetChildrenCount() > 0)
    {
        return InventoryGrid->GetChildAt(0);
    }
    return Super::NativeGetDesiredFocusTarget();
}
```

Also set `bAutoRestoreFocus = true` in the base class constructor so that when a widget resurfaces on the stack (e.g., modal closes and menu below becomes active again), focus returns to where it was.

**Action Items:**
- [ ] Add `NativeGetDesiredFocusTarget()` override to `UMOMenuWidget` base
- [ ] Set `bAutoRestoreFocus = true` in base constructors
- [ ] Each menu subclass overrides to return its primary interactive element
- [ ] Test: open inventory → focus is on first slot → open modal → close modal → focus returns to where it was in inventory

---

### ENHANCEMENT 5: Dedicated Server Guard

**The Problem:**

Carried over from the previous audit — the plan doesn't mention dedicated server safety. Colony bars, survivor menus, and possession menus indicate a multiplayer game. Any code path that touches UI from gameplay code will crash on a dedicated server.

**The Fix:**

Add to the `UMOGameUIManagerSubsystem`:

```cpp
template<typename T>
T* UMOGameUIManagerSubsystem::PushMenu(TSubclassOf<T> MenuClass)
{
    // Safety: no UI on dedicated server
    if (IsRunningDedicatedServer()) return nullptr;

    UCommonActivatableWidgetContainerBase* Stack = GetMenuLayer();
    if (!Stack) return nullptr;

    return Cast<T>(Stack->AddWidget(MenuClass));
}
```

**Action Items:**
- [ ] Add `IsRunningDedicatedServer()` guard to all subsystem UI methods
- [ ] Add `GetOwningPlayer()` nullptr checks in widget initialization

---

## PLAN VALIDATION: WHAT'S CORRECT

These elements of the plan are confirmed correct by external sources:

**1. "CommonUI is all-or-nothing" (line 573)** — Confirmed. The Tomasz Merda guide, Milton Candelero post, X157 notes, and multiple Epic forum threads all confirm that mixing `SetInputMode()` with CommonUI, or using `AddToViewport()` for activatable widgets, breaks CommonUI's input routing. The `CommonGameViewportClient` routes input through the `UCommonUIActionRouterBase`, and widgets must be in the activatable tree to participate.

**2. Kill `AddToViewport()` for menus** — Confirmed. Every source agrees. Widgets must be pushed to a `UCommonActivatableWidgetStack` to register with the ActionRouter.

**3. Kill `SetInputMode()` calls** — Confirmed. The X157 Action Router documentation says explicitly: "DO NOT try to circumvent this in your own code. Instead, have your code create Common UI FUIInputConfig settings and send them to Common UI."

**4. Kill manual `ActivateWidget()` after push** — Confirmed. The Tomasz Merda guide says: "PushWidget Activates CommonActivatableWidget now, so you shouldn't call ActivateWidget yourself." This was a UE5.5 change.

**5. Layer system with 4-5 stacks** — Confirmed. Lyra uses 4 layers (Game, GameMenu, Menu, Modal). The Stajky CommonUIMenuTemplate uses the same 4. The Dillon Bellefeuille UI Layers Manager plugin (March 2026) uses 5 (adding Debug). Your 5-layer system (HUD, Game, GameOverlay, Menu, Modal) is well-aligned.

**6. Widget/Controller separation of concerns** — This is a sound architectural pattern. Widgets display data and emit events; controllers manage lifecycle and game logic. This is standard MVVM-adjacent architecture for game UI.

**7. Test using ActionRouter, not raw Slate events** — Correct. If tests need raw Slate events, that IS a sign the UI isn't properly integrated with CommonUI, as the plan states.

**8. Migration order (simplest first)** — The order of MOInGameMenu → Inventory → Crafting → Building → Skills → Status is well-reasoned. Starting with the simplest menu validates the pattern before tackling complex cases.

---

## SUMMARY OF ALL ACTION ITEMS

### Critical (Corrections — Must Fix)

1. **[CRITICAL]** Split `GetDesiredInputConfig`: `All` for menus (toggle keys work), `Menu` for modals (full block)
2. **[CRITICAL]** Implement Enhanced Input mapping context swapping to suppress WASD etc. under `All` mode
3. **[CRITICAL]** Define close paths: back action → `DeactivateWidget()`, programmatic → `Stack->RemoveWidget()`
4. **[CRITICAL]** Move context menus OUT of modal hierarchy — use `UCommonUserWidget`, non-stack overlay
5. **[CRITICAL]** Refine "no caching" rule: don't cache widget instances, DO cache state in controllers

### High Priority (Warnings)

6. **[HIGH]** Decide menu switching strategy: Replace, Stack, or Swap
7. **[HIGH]** Add `IsMenuOpen()` pattern for toggle key handling (stack query or deactivation delegate)
8. **[HIGH]** Use `FObjectInitializer` constructor pattern on all base classes

### Medium Priority (Enhancements)

9. **[MEDIUM]** Create `RootContentWidgetClass` stub for gameplay input restoration
10. **[MEDIUM]** Add `CommonUI.DumpActivatableTree` to debug workflow
11. **[MEDIUM]** Set `bIsModal = true` on `UMOModalWidget` for true input blocking
12. **[MEDIUM]** Add `NativeGetDesiredFocusTarget()` and `bAutoRestoreFocus` to base classes
13. **[MEDIUM]** Add dedicated server guards to all subsystem UI methods

### Testing Additions

14. [ ] Open inventory → press C → crafting opens (toggle key works through `All` mode)
15. [ ] Open inventory → press WASD → character does NOT move (mapping context stripped)
16. [ ] Open confirmation dialog → press I → inventory does NOT open (`Menu` mode blocks)
17. [ ] Open menu → close menu → cursor hidden, movement restored (`RootContentWidgetClass`)
18. [ ] Open inventory → right-click → context menu appears → click outside → dismisses
19. [ ] `CommonUI.DumpActivatableTree` shows correct hierarchy after each migration step
20. [ ] Open inventory → open modal → close modal → focus returns to inventory (`bAutoRestoreFocus`)
21. [ ] Press I → opens → press I → closes → press I → opens (toggle cycle, no stale state)

---

## REFERENCES

- **Milton Candelero — "CommonUI Demystified: Focus, Input Routing, and Activatable Widgets" (Aug 2025):** The most detailed community explanation of how CommonUI's activatable tree, input routing, focus resolution, and `bIsModal` actually work. Critically important for understanding why `GetDesiredFocusTarget` matters and how input cascades through the tree.
- **X157 Dev Notes — Common UI Activatable Widget:** States that activatable widgets "are often not deleted, instead they're reused" and that `GetDesiredInputConfig` results are sent to the Action Router. Confirms the `FObjectInitializer` pattern and activation lifecycle.
- **X157 Dev Notes — Common UI Action Router:** "DO NOT try to circumvent this in your own code." Confirms all input mode changes must flow through CommonUI.
- **Tomasz Merda — Integrating CommonUI and Enhanced Input (UE5.5):** Confirms `PushWidget` auto-activates, `SetInputMode` breaks CommonUI, `RootContentWidgetClass` is mandatory.
- **Snorri Sturluson — CommonUI Playground (GitHub):** Clean reference implementation showing `PushModal`/`PopModal` via `AddWidget`/`RemoveWidget` on a `UCommonActivatableWidgetContainerBase`.
- **Stajky — CommonUIMenuTemplate (GitHub):** Confirms 4-layer pattern, one widget per stack, GameplayTag-based messaging between widgets.
- **Dillon Bellefeuille — UI Layers Manager Plugin (March 2026, UE5.6):** Most recent community reference confirming the local player subsystem + gameplay tag layer pattern.
- **Epic Forums — "Common Activatable Widget Stack very inconsistent behavior" (March 2024):** Documents stale state bugs when re-pushing previously removed widgets. Confirms `AddWidget` creating fresh instances is the correct pattern.
- **Epic UE5.7 API Docs — UCommonActivatableWidgetContainerBase:** Described as "intentionally meant to be black boxes that do not expose child/slot modification."
