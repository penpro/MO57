# UI Overhaul Architecture — Audit & Recommendations

**Audited:** 2026-03-31
**Source Document:** `UI_Overhaul_Architecture.md`
**Purpose:** Feed this into Claude Code alongside the architecture doc. It contains a detailed audit of the UI refactor plan, flags risks, and provides concrete remediation guidance grounded in UE5.7 CommonUI best practices and community-validated patterns.

---

## Audit Verdict: PASS WITH CONDITIONS

The architecture document is above-average for a CommonUI migration. The staged approach with checklists is disciplined. The class hierarchy split between activatable menus and ephemeral context menus is correct. The pitfall catalog is genuinely useful. However, there are **7 issues** ranging from latent bugs to missing implementation detail that must be addressed before Stage 3A is considered complete.

---

## ISSUE 1: `ECommonInputMode::All` Leaks Gameplay Input Into Menus

**Severity:** HIGH — Latent bug, will ship if not addressed
**Location:** `UMOMenuWidgetBase::GetDesiredInputConfig()`, architecture doc line ~289

### The Problem

The architecture doc uses `ECommonInputMode::All` so that Enhanced Input toggle keys (I, C, B) continue to reach the PlayerController while a menu is open. This is a valid pattern — it's the documented way to achieve "Game and UI" style input with CommonUI. However, `ECommonInputMode::All` means **all** game input is live while menus are up: WASD movement, mouse look, ability bindings, attack actions, interaction keys, etc.

The architecture doc does not document what suppresses these unwanted gameplay actions during menus. This is a gap.

### Why This Matters (UE5.7 Context)

CommonUI's input routing works through the `CommonGameViewportClient` and the `CommonUIActionRouter` subsystem. When `ECommonInputMode::All` is active, the action router passes input through to both the UI layer and the game layer simultaneously. The X157 dev notes emphasize that you should never circumvent CommonUI's input routing — all input mode changes must flow through `FUIInputConfig` and the action router. The Tomasz Merda integration guide (UE5.5+) confirms that `SetInputMode` calls break CommonUI entirely, so the only lever you have is input mapping context management.

The standard Lyra pattern for this problem is to use separate `UInputMappingContext` assets with priorities. Gameplay movement actions live in `IMC_Gameplay`, menu toggle actions live in `IMC_UIToggle` (or similar), and when a menu opens you remove `IMC_Gameplay` from the Enhanced Input subsystem while keeping `IMC_UIToggle` active.

### Recommended Fix

Add an `IMC_MenuToggle` mapping context that contains ONLY the menu toggle actions (I, C, B, Escape). In `NativeOnActivated` / `NativeOnDeactivated` on `UMOMenuWidgetBase`, swap mapping contexts:

```cpp
void UMOMenuWidgetBase::NativeOnActivated()
{
    Super::NativeOnActivated();

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (auto* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                // Remove gameplay actions (movement, combat, interaction)
                Subsystem->RemoveMappingContext(IMC_Gameplay);
                // Keep menu toggle context active (or add it if not present)
                Subsystem->AddMappingContext(IMC_MenuToggle, 1);
            }
        }
    }
}

void UMOMenuWidgetBase::NativeOnDeactivated()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (auto* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                // Restore gameplay mapping context
                Subsystem->AddMappingContext(IMC_Gameplay, 0);
            }
        }
    }

    Super::NativeOnDeactivated();
}
```

**Alternative (simpler but less granular):** Guard your gameplay action handlers in the PlayerController with a check against the menu stack state:

```cpp
void AMOPlayerController::HandleMoveForward(const FInputActionValue& Value)
{
    // Bail if any menu is on the Game layer stack
    if (IsAnyMenuOpen()) return;
    // ... normal movement
}
```

This is less clean than mapping context swapping but is a fast stopgap.

### Action Items

- [ ] Document which `UInputMappingContext` assets exist and what actions they contain
- [ ] Decide between mapping context swapping (preferred) or action handler guards (stopgap)
- [ ] If using mapping context swapping, ensure `NativeOnActivated`/`NativeOnDeactivated` manage contexts correctly for nested menus (only restore `IMC_Gameplay` when the LAST menu closes, not when a nested menu deactivates beneath another)
- [ ] Add a test case: "Open inventory → press WASD → character should NOT move"
- [ ] Add a test case: "Open inventory → press I/C/B → menu should toggle correctly"

---

## ISSUE 2: Context Menu Dismiss Behavior Is Underspecified

**Severity:** MEDIUM — Missing implementation detail for Stage 5A
**Location:** Architecture doc, Stage 5A / Context Menus section

### The Problem

The architecture doc says context menus should migrate from `UCommonActivatableWidget` to `UCommonUserWidget` and "keep `NativeOnKeyDown` for Escape handling." This is architecturally correct — ephemeral popups should not participate in the activation stack. However, the doc is silent on the most critical context menu behavior: **click-outside-to-dismiss**.

### Why This Matters

`UCommonUserWidget` does not participate in CommonUI's input routing or back action system. It inherits from `UUserWidget` and adds CommonUI styling support but no activation semantics. This means:

- No `bIsBackHandler` — you're correct to use `NativeOnKeyDown` for Escape
- No `GetDesiredInputConfig()` — the context menu does not affect input mode
- No automatic deactivation — you must manually handle the lifecycle

The community pattern for context menu dismissal in UMG (which applies equally to `UCommonUserWidget`) is one of:

**Pattern A: Full-screen invisible hit-test backdrop.** Spawn a transparent full-screen `UButton` or `UBorder` behind the context menu. When it receives a click, dismiss the context menu. This is the most common UMG pattern and is the approach most forum posts converge on.

**Pattern B: Focus-loss callback.** Override `NativeOnFocusLost` or `NativeOnMouseLeave` + a delayed dismiss. This is fragile because Slate focus is notoriously unreliable for this purpose — clicking a button inside the parent menu may not fire a focus-lost on the context menu if focus routing decides the context menu's child had focus.

**Pattern C: Global pointer event listener.** Register a global `FSlateApplication::Get().GetOnGlobalPointerPress()` delegate and check if the click was inside the context menu bounds. Dismiss if outside. This is more robust but requires cleanup.

### Recommended Fix

Use Pattern A. The architecture doc should specify this explicitly for Stage 5A:

```cpp
// In MOContextMenuBase:
// 1. When showing the context menu, spawn a full-screen backdrop BEHIND it
// 2. The backdrop captures all clicks not on the context menu
// 3. Backdrop click → dismiss context menu

UPROPERTY()
TObjectPtr<UMOContextMenuBackdrop> Backdrop;

void UMOContextMenuBase::Show(FVector2D ScreenPosition)
{
    // Create and add backdrop to GameOverlay layer (or parent canvas)
    // Backdrop covers entire screen, is transparent, but hit-testable
    // Backdrop->OnClicked.AddDynamic(this, &UMOContextMenuBase::Dismiss);

    // Position and show context menu on top of backdrop
    // ...
}

void UMOContextMenuBase::Dismiss()
{
    // Remove backdrop
    // Remove self from parent
    // Broadcast OnDismissed delegate
}
```

### Action Items

- [ ] Document the click-outside-to-dismiss pattern in the architecture doc
- [ ] Decide between backdrop (recommended), focus-loss, or global pointer
- [ ] Ensure Escape via `NativeOnKeyDown` also removes the backdrop
- [ ] Ensure the backdrop does NOT interfere with the parent menu's activation state
- [ ] Add test case: "Open inventory → right-click item → click outside context menu → context menu closes, inventory stays"

---

## ISSUE 3: `MOConfirmationDialog` Should Be Migrated, Not Exempted

**Severity:** MEDIUM — Architecture gap
**Location:** "NOT Migrated (By Design)" table

### The Problem

The architecture doc exempts `MOConfirmationDialog` from migration with the rationale: "Enter/Escape for Yes/No buttons. Needs special key handling." This rationale does not hold up. Modal confirmation dialogs are one of the strongest use cases for CommonUI's layer stack system.

### Why This Matters

The architecture doc already defines a Modal layer (Z-Order 200) for confirmation dialogs. Lyra's own architecture pushes confirmation-style widgets onto `UI.Layer.Modal` as activatable widgets. The standard CommonUI pattern for a confirmation dialog is:

- Push it onto the Modal layer stack
- `bIsBackHandler = true` → Escape triggers `NativeOnHandleBackAction` → mapped to "Cancel/No"
- Confirm/Enter is handled via a `UCommonButtonBase` bound to the confirm action
- The dialog's `GetDesiredInputConfig()` returns `ECommonInputMode::Menu` (since you DO want to block game input during a modal — unlike regular menus where you want toggle keys)

The "special key handling" for Enter/Escape is exactly what CommonUI is designed to do. Enter is the default confirm action in CommonUI's input data table. Escape is the back action. The dialog doesn't need `NativeOnKeyDown` overrides at all.

### Recommended Fix

Migrate `MOConfirmationDialog` to inherit from `UMOMenuWidgetBase` (or a new `UMOModalWidgetBase` if you want a separate base class for modals). Push it to the Modal layer.

```cpp
UCLASS()
class MOFRAMEWORK_API UMOConfirmationDialog : public UMOMenuWidgetBase
{
    GENERATED_BODY()

public:
    UMOConfirmationDialog(const FObjectInitializer& ObjectInitializer);

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConfirmationResult, bool, bConfirmed);
    UPROPERTY(BlueprintAssignable)
    FOnConfirmationResult OnResult;

protected:
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    virtual bool NativeOnHandleBackAction() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonButtonBase> ConfirmButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonButtonBase> CancelButton;
};

// Implementation:
TOptional<FUIInputConfig> UMOConfirmationDialog::GetDesiredInputConfig() const
{
    // Modals SHOULD block game input — use Menu mode, not All
    FUIInputConfig Config(
        ECommonInputMode::Menu,         // Block game input during modal
        EMouseCaptureMode::NoCapture,
        EMouseLockMode::DoNotLock,
        false
    );
    return Config;
}

bool UMOConfirmationDialog::NativeOnHandleBackAction()
{
    // Escape = Cancel
    OnResult.Broadcast(false);
    RequestClose();
    return true;
}
```

Note the use of `ECommonInputMode::Menu` here — modals are the one place where blocking game input entirely IS the correct behavior. Toggle keys should not work while a "Are you sure you want to delete this?" dialog is showing.

### Action Items

- [ ] Remove `MOConfirmationDialog` from the "NOT Migrated" list
- [ ] Add it to Stage 5A or create a new Stage 5B for modal migration
- [ ] Consider creating `UMOModalWidgetBase` if modals need `ECommonInputMode::Menu` while regular menus use `ECommonInputMode::All`
- [ ] Add test case: "Trigger confirmation dialog → press Escape → dialog closes with Cancel result"
- [ ] Add test case: "Trigger confirmation dialog → press WASD → character should NOT move"
- [ ] Add test case: "Trigger confirmation dialog → press I/C/B → menu should NOT toggle"

---

## ISSUE 4: Widget Re-Creation GC Pressure (Pitfall 13, Option A)

**Severity:** LOW-MEDIUM — Performance risk for frequent menus
**Location:** Pitfall 13, Option A

### The Problem

Pitfall 13 offers two options for dealing with stale widget state on reopen. Option A says "create new widget instance each time" and notes that "GC cleans up when removed." The doc recommends Option A for infrequent menus and Option B (reset state in `NativeOnActivated`) for frequent menus. This is correct as a guideline, but Option A needs a stronger warning.

### Why This Matters

UE's garbage collector does not run every frame. By default, it runs roughly every 60 seconds (configurable via `gc.TimeBetweenPurgingPendingKillObjects`). When you create a widget via `CreateWidget`, it constructs the entire widget tree including all child widgets. For a menu like `MOInventoryMenu` with potentially 16+ inventory slot widgets, each containing icons, text blocks, and buttons — that's hundreds of UObjects per creation.

Community reports on the Epic forums confirm that creating and orphaning complex widget trees during rapid toggling (e.g., a player spam-opening inventory) causes visible memory growth between GC cycles and can contribute to frame hitches when GC does run, especially on consoles.

Additionally, `UCommonActivatableWidgetStack` has its own internal widget pooling behavior. When a widget deactivates and is removed from the stack, CommonUI may hold a reference depending on the container configuration. Creating new widgets while the stack holds orphaned references compounds the issue.

### Recommended Fix

The document should be stronger:

**Option A is ONLY for menus opened once or twice per gameplay session** (possession menu, death screen). It should be explicitly marked as inappropriate for inventory, crafting, or any menu that a player might open 50+ times per session.

**Option B should be the default recommendation.** The document should provide a template for state reset:

```cpp
void UMOMenuWidgetBase::NativeOnActivated()
{
    Super::NativeOnActivated();
    ResetMenuState();  // Virtual — subclasses override to clear their state
}

// Default implementation (no-op, subclasses override)
void UMOMenuWidgetBase::ResetMenuState()
{
    // Subclasses: reset scroll position, clear selection,
    // refresh data bindings, etc.
}
```

### Action Items

- [ ] Change the recommendation: Option B is the default, Option A is the exception
- [ ] Add `ResetMenuState()` virtual to `UMOMenuWidgetBase` so subclasses have a clear hook
- [ ] Add a comment in the architecture doc that Option A for frequent menus is a bug, not a tradeoff
- [ ] If using Option A anywhere, ensure no UPROPERTY references to the old widget survive (or the GC will never collect it)

---

## ISSUE 5: `IsAnyMenuOpen()` Replacement Queries Only One Layer

**Severity:** MEDIUM — Logic bug in the replacement code
**Location:** "Replace IsAnyMenuOpen()" section, Stage 3A

### The Problem

The architecture doc offers this replacement for the deprecated `ActiveMenuCount`:

```cpp
bool bAnyMenuOpen = !MenuLayerStack->GetWidgetList().IsEmpty();
```

But the architecture doc defines multiple layers that contain menus: Game (inventory, crafting, building, skills), Menu (in-game menu, possession menu), and Modal (confirmation dialogs). If a confirmation dialog is open on the Modal layer, the above query against `MenuLayerStack` (presumably the Game layer) will return `false`.

The doc also offers an alternative:

```cpp
bool bAnyMenuOpen = UMOGameUIManagerSubsystem::Get(World)->IsGameLayerOccupied();
```

But `IsGameLayerOccupied()` also sounds like it queries only one layer.

### Why This Matters

`IsAnyMenuOpen()` is likely used in gameplay code to suppress actions, disable camera controls, or show/hide the HUD. If it only checks one layer, a modal dialog on the Modal layer won't suppress gameplay.

### Recommended Fix

Provide a single canonical function that queries ALL relevant layers:

```cpp
bool UMOGameUIManagerSubsystem::IsAnyMenuOpen() const
{
    // Check all layers that should suppress gameplay
    if (GameLayerStack && !GameLayerStack->GetWidgetList().IsEmpty())
        return true;
    if (MenuLayerStack && !MenuLayerStack->GetWidgetList().IsEmpty())
        return true;
    if (ModalLayerStack && !ModalLayerStack->GetWidgetList().IsEmpty())
        return true;
    return false;
}
```

Deprecate the per-layer queries for external use. Internal code that needs to know about a specific layer can still query directly, but gameplay code should always go through the subsystem.

### Action Items

- [ ] Decide which layers count as "menu open" (Game + Menu + Modal, probably NOT HUD)
- [ ] Implement `IsAnyMenuOpen()` on `UMOGameUIManagerSubsystem` that checks all relevant layers
- [ ] Deprecate `IsGameLayerOccupied()` or make it internal-only
- [ ] Find all callers of the old `IsAnyMenuOpen()` and migrate them
- [ ] Add test case: "Open confirmation dialog on Modal layer → `IsAnyMenuOpen()` returns true"

---

## ISSUE 6: No Dedicated Server / Multiplayer Guard

**Severity:** LOW-MEDIUM — Will crash on dedicated server if not addressed
**Location:** Global — applies to all widget code

### The Problem

The architecture doc contains colony bars, survivor menus, possession menus, and station context menus — all indicators of a multiplayer game. There is no mention anywhere of dedicated server guards or `GetOwningPlayer()` validity checks.

### Why This Matters

On a dedicated server:
- There is no `ULocalPlayer`
- There is no viewport
- `GetOwningPlayer()` returns nullptr
- `CreateWidget` requires a valid player controller
- Any code path that touches UI from gameplay code (e.g., "notify the UI that an item was picked up") will crash if it doesn't check for a valid local player first

### Recommended Fix

Add a section to the architecture doc:

```cpp
// In any code that creates or references UI widgets:
if (!IsRunningDedicatedServer())
{
    // Safe to create/access widgets
}

// OR check at the subsystem level:
UMOGameUIManagerSubsystem* UISub = UMOGameUIManagerSubsystem::Get(World);
if (UISub && UISub->IsLocalPlayerValid())
{
    // Safe to interact with UI
}

// GetOwningPlayer() can return nullptr - always check:
APlayerController* PC = GetOwningPlayer();
if (!PC) return;  // Dedicated server or widget not properly initialized
```

### Action Items

- [ ] Audit all entry points where gameplay code touches UI (item pickup, health change, dialogue trigger, etc.)
- [ ] Add `IsRunningDedicatedServer()` or `GetOwningPlayer()` nullptr checks at each entry point
- [ ] Add this as a global rule in the architecture doc, not per-widget
- [ ] If using a UI subsystem, add a `IsLocalPlayerValid()` convenience method

---

## ISSUE 7: Legacy `OnLegacyRequestClose` Delegate Cleanup

**Severity:** LOW — Technical debt, not a bug yet
**Location:** Migration Status table, all migrated widgets

### The Problem

Every migrated widget in the migration table still has `OnLegacyRequestClose` listed. The architecture doc mentions Stage 6A as "Cleanup" but doesn't explicitly state that this stage removes the legacy delegates and migrates all controller bindings to `OnRequestClose`.

### Why This Matters

Dual delegates are Pitfall 3 in the architecture doc's own pitfall catalog. As long as both delegates exist, any new code has a 50/50 chance of binding to the wrong one, causing the exact double-fire bug the migration was supposed to fix. Additionally, having two delegates for the same purpose makes the codebase confusing for new developers.

### Recommended Fix

Make Stage 6A explicit:

```
Stage 6A Cleanup Checklist:
- [ ] Remove FMOUILegacyRequestClose delegate type from MOMenuWidgetBase.h
- [ ] Remove OnLegacyRequestClose UPROPERTY from MOMenuWidgetBase.h
- [ ] Remove all OnLegacyRequestClose.Broadcast() calls from MOMenuWidgetBase.cpp
- [ ] Find-and-replace all OnLegacyRequestClose bindings in controller code → OnRequestClose
- [ ] Compile and fix all errors
- [ ] Grep for "LegacyRequestClose" — should return zero results
- [ ] Update migration table to remove the "Legacy Delegate" column
```

### Action Items

- [ ] Add the above checklist to Stage 6A in the architecture doc
- [ ] Consider doing this BEFORE Stage 3A — it reduces surface area for bugs during the more complex stage
- [ ] Add a `UE_DEPRECATED` macro to `OnLegacyRequestClose` immediately as a compile-time signal

---

## ADDITIONAL OBSERVATIONS

### Mouse Capture Mode for Gameplay Stub

The `WBP_GameplayStackStub` uses `EMouseCaptureMode::CaptureDuringMouseDown`. Verify this is the intended behavior for your game. Options:

- `CaptureDuringMouseDown` — mouse is captured only while a button is held. Good for games where the player clicks to interact with the world and right-click-drags to rotate camera.
- `CapturePermanently` — mouse is captured always during gameplay. Good for first/third person games where mouse look is always active. If your game uses mouse look for camera control, this is probably what you want.
- `CaptureDuringRightMouseDown` — mouse captured only during right-click hold. Good for RTS-style games.

The Zerol dev notes specifically call out that for top-down games with click-to-move, using `CapturePermanently` or similar can cause a double-click requirement for single clicks. Test your specific gameplay feel.

**Action item:** [ ] Verify mouse capture mode matches gameplay camera/interaction model

### Alt-Tab / Focus Loss Edge Case

The testing checklist is thorough but missing one important scenario: what happens when the player alt-tabs out of the game with a menu open, then returns?

When the game window loses and regains focus, Slate may fire spurious focus events that can confuse CommonUI's activation system. If the active widget on the stack receives a deactivation/reactivation cycle due to window focus change, it may re-trigger `NativeOnActivated` and `NativeOnDeactivated`.

**Action item:** [ ] Add test case: "Open inventory → Alt-Tab away → Alt-Tab back → inventory should still be functional with correct input state"

### `PushWidget` Auto-Activates in UE5.5+

The architecture doc correctly notes in the Stage 0 checklist that `ActivateWidget()` should not be called manually after `PushWidget`. This is because starting in UE5.5, pushing a widget onto a `UCommonActivatableWidgetStack` automatically activates it. Calling `ActivateWidget` a second time can break the activation state machine. The Tomasz Merda integration guide and Epic forum posts both confirm this behavioral change. This is correctly captured but worth highlighting for anyone coming from pre-5.5 code.

### `UCommonUIActionRouterBase` Override Opportunity

The architecture doc doesn't mention the possibility of overriding the action router. The X157 dev notes document that you can derive from `UCommonUIActionRouterBase` and override `ApplyUIInputConfig` to completely customize how input modes work. This could be a cleaner long-term solution for the `ECommonInputMode::All` issue (Issue 1) — you could override the router to strip out specific input types when the top widget is a menu, rather than swapping mapping contexts.

This is an advanced pattern and not required for the current migration, but worth noting for future iterations.

**Action item:** [ ] Consider `UCommonUIActionRouterBase` override as a Phase 2 improvement if mapping context swapping becomes unwieldy

### Colony Bar Layer Assignment

The architecture doc correctly flags that the colony bar MUST be on the HUD layer, not the Game layer. This is critical because `UCommonActivatableWidgetStack` only shows one widget at a time — pushing inventory onto the Game layer would collapse the colony bar if it were on the same layer.

The Lyra pattern confirms this: HUD-layer widgets are persistent and not affected by menu-layer pushes. The four-layer pattern (Game/HUD, GameMenu, Menu, Modal) from Lyra is exactly what the architecture doc implements with its five layers (HUD, Game, GameOverlay, Menu, Modal).

**Action item:** [ ] This is correctly designed — just verify it's implemented in `WBP_MOPrimaryGameLayout`

---

## SUMMARY OF ALL ACTION ITEMS

### Before Stage 3A (Do These First)

1. **[HIGH]** Resolve `ECommonInputMode::All` gameplay input leak — implement mapping context swapping or action handler guards
2. **[MEDIUM]** Add `IsAnyMenuOpen()` subsystem method that queries all layers
3. **[LOW]** Add `UE_DEPRECATED` to `OnLegacyRequestClose` and plan Stage 6A

### During Stage 3A

4. **[MEDIUM]** Verify mouse capture mode on `WBP_GameplayStackStub` matches gameplay model
5. **[MEDIUM]** Add dedicated server guards to all UI entry points
6. **[LOW]** Change Pitfall 13 recommendation: Option B default, Option A exception only

### During Stage 5A (Context Menus)

7. **[MEDIUM]** Document and implement click-outside-to-dismiss pattern (recommend backdrop approach)

### During Stage 5B (New — Modal Migration)

8. **[MEDIUM]** Migrate `MOConfirmationDialog` to `UMOMenuWidgetBase` or new `UMOModalWidgetBase`
9. **[MEDIUM]** Use `ECommonInputMode::Menu` for modals (not `All`)

### During Stage 6A (Cleanup)

10. **[LOW]** Remove all `OnLegacyRequestClose` delegates and bindings
11. **[LOW]** Remove "Legacy Delegate" column from migration table

### Testing Additions

12. [ ] "Open inventory → press WASD → character should NOT move"
13. [ ] "Open inventory → press I/C/B → menu should toggle correctly"
14. [ ] "Open confirmation dialog → press WASD → character should NOT move"
15. [ ] "Open confirmation dialog → press I/C/B → menu should NOT toggle"
16. [ ] "Open confirmation dialog on Modal layer → `IsAnyMenuOpen()` returns true"
17. [ ] "Open inventory → right-click item → click outside context menu → context menu closes, inventory stays"
18. [ ] "Open inventory → Alt-Tab away → Alt-Tab back → inventory still functional"

---

## REFERENCES

These are the external sources consulted during this audit. Use them for deeper dives on specific patterns:

- **X157 Dev Notes — CommonUI Action Router:** Covers overriding `UCommonUIActionRouterBase::ApplyUIInputConfig` for custom input routing. Key takeaway: never bypass CommonUI for input mode changes.
- **X157 Dev Notes — Lyra Default UI Policy:** Documents the four-layer stack pattern (Game, GameMenu, Menu, Modal) with `UCommonActivatableWidgetStack`. Confirms only one widget active per stack.
- **X157 Dev Notes — Lyra Input:** Covers how Lyra combines CommonUI + Enhanced Input via mapping context management. Key takeaway: CommonUI controls whether input reaches the game; Enhanced Input controls what happens when it does.
- **Tomasz Merda — Integrating CommonUI and Enhanced Input (UE5.5):** Community-validated guide. Key findings: `PushWidget` auto-activates (don't call `ActivateWidget` manually), `SetInputMode` breaks CommonUI, `RootContentWidgetClass` is mandatory for proper input restoration.
- **Zerol Dev Notes — CommonUI Setup (UE5.4.2):** Covers mouse capture pitfalls for top-down games, the double-click issue with permanent capture, and Lyra's two-stack (Game + Menu) pattern.
- **Epic Forums — Widget GC:** Multiple threads confirm that widgets created via `CreateWidget` but not rooted by a UPROPERTY reference or viewport addition will be collected by GC, but NOT immediately — GC runs on a timer (~60s default). Rapid create/abandon cycles cause memory growth between collections.
- **Dillon Bellefeuille — UI Layers Manager Plugin (UE5.6, March 2026):** Recent community plugin confirming the layer manager subsystem pattern with gameplay tag-based layer identification, matching the architecture doc's approach.
- **James Roha — Gamepad UI Navigation with Enhanced Input:** Covers the `Started` vs `Triggered` event pitfall where Enhanced Input fires continuously while held, causing double-registration in UI navigation.
- **Epic UE5.7 Documentation — CommonUI Input Technical Guide:** Official reference for input routing fundamentals (content was sparse at time of audit but confirms the `CommonGameViewportClient` requirement).
