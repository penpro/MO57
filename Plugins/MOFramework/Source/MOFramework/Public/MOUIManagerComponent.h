/**
 * =============================================================================
 * MOUIManagerComponent.h - Central UI Controller Orchestrator
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] CONTROLLER DELEGATION: Most UI operations now delegate to specialized
 *   UI controllers (Inventory, Crafting, Building, Character, System). UIManager
 *   acts as orchestrator, not direct implementor.
 *
 * [2024-02] PAWN CACHING: CachePawnComponents() must be called on possession change.
 *   UI controllers access components via GetCached* methods.
 *
 * [2024-02] MODAL BACKGROUND: Reference counting for modal. Multiple menus can
 *   be open simultaneously. Background hides only when all close.
 *
 * [2024-02] FOCUS HINT: Timer-based update can cause issues if pawn changes.
 *   CachedInteractorComponent updated in CachePawnComponents().
 *
 * =============================================================================
 * RELATED FILES: MOUIControllerBase.h, MOInventoryUIController.h, etc.
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOInteractorComponent.h"
#include "MOUIManagerComponent.generated.h"

/**
 * =============================================================================
 * MOUIManagerComponent - Central UI Controller for MOFramework
 * =============================================================================
 *
 * PURPOSE:
 * This component lives on the PlayerController and serves as the central hub
 * for all UI operations. It manages menu creation, lifecycle, input modes,
 * and coordinates between UI widgets and game systems.
 *
 * OWNERSHIP:
 * - Owner: AMOPlayerController (created in constructor as default subobject)
 * - Lifespan: Exists for the duration of the PlayerController
 *
 * -----------------------------------------------------------------------------
 * UI ARCHITECTURE OVERVIEW
 * -----------------------------------------------------------------------------
 *
 * MENU HIERARCHY (from bottom to top Z-order):
 *   1. Modal Background (Z=10) - Click-to-close backdrop
 *   2. Menus/Panels (Z=50-100) - Inventory, Crafting, Skills, Building, etc.
 *   3. Context Menus (Z=150) - Item right-click menu
 *   4. Dialogs (Z=200) - Confirmation dialogs, inspection progress
 *   5. Notifications (Z=250) - Temporary notification messages
 *
 * MENU CATEGORIES:
 *   - Switchable Menus: Inventory, Crafting, Skills, Building, Status
 *     -> Opening one closes others (except in-game menu)
 *     -> Tab/Escape closes to gameplay
 *   - Overlay Menus: InGame, Possession
 *     -> Can overlay on top of switchable menus
 *     -> Escape closes hierarchically (focus panel first, then menu)
 *   - Modal Widgets: Context menu, Confirmation dialog, Build widget
 *     -> Block input to other widgets while open
 *
 * -----------------------------------------------------------------------------
 * DELEGATE FLOW PATTERNS
 * -----------------------------------------------------------------------------
 *
 * All menu widgets follow this delegate pattern:
 *
 *   Widget (OnRequestClose) --> UIManager (Handle*RequestClose) --> Close*()
 *
 * STANDARD DELEGATES USED BY WIDGETS:
 *   - OnRequestClose: Widget wants to close (Tab/Escape/Close button)
 *   - OnConfirmed/OnCancelled: Dialog result callbacks
 *   - On[Action]Requested: Widget requests an action (Save, Load, Craft, etc.)
 *
 * DELEGATE BINDING PATTERN:
 *   1. Remove any existing binding (prevents duplicates on re-open)
 *   2. Add new binding
 *   Example:
 *     Widget->OnRequestClose.RemoveDynamic(this, &ThisClass::HandleClose);
 *     Widget->OnRequestClose.AddDynamic(this, &ThisClass::HandleClose);
 *
 * -----------------------------------------------------------------------------
 * INPUT MODE MANAGEMENT
 * -----------------------------------------------------------------------------
 *
 * When menus open:
 *   - Input mode: FInputModeUIOnly or FInputModeGameAndUI
 *   - Mouse cursor: Shown
 *   - Movement/Look: Optionally locked (configurable)
 *
 * When all menus close:
 *   - Input mode: FInputModeGameOnly
 *   - Mouse cursor: Hidden
 *   - Movement/Look: Restored
 *
 * -----------------------------------------------------------------------------
 * MENU OPEN/CLOSE FLOW
 * -----------------------------------------------------------------------------
 *
 * OPEN FLOW:
 *   1. ToggleMenu() or OpenMenu() called
 *   2. Check if menu already open -> close if toggle
 *   3. Close switchable menus if opening another (CloseAllSwitchableMenus)
 *   4. Create widget if needed (CreateWidget<T>)
 *   5. Bind delegates (remove then add)
 *   6. Initialize widget with required components
 *   7. Show modal background
 *   8. Add to viewport
 *   9. Set input mode for menu
 *   10. Hide reticle
 *
 * CLOSE FLOW:
 *   1. Widget broadcasts OnRequestClose (user pressed Escape/Tab/Close)
 *   2. Handle*RequestClose() receives broadcast
 *   3. CloseMenu() called
 *   4. Remove from parent (not destroyed, cached)
 *   5. Hide modal background if no menus open
 *   6. Restore input mode if no menus open
 *   7. Show reticle
 *
 * -----------------------------------------------------------------------------
 * COMPONENT DEPENDENCIES
 * -----------------------------------------------------------------------------
 *
 * This component retrieves data from pawn components:
 *   - UMOInventoryComponent: Inventory data for menus
 *   - UMOSkillsComponent: Skill data for crafting/skills panels
 *   - UMOKnowledgeComponent: Knowledge for recipe filtering
 *   - UMOVitalsComponent: Health/stamina for status panel
 *   - UMOMetabolismComponent: Hunger/thirst for status panel
 *   - UMOMentalStateComponent: Mental state for status panel
 *   - UMOCraftingQueueComponent: Active crafts for crafting menu
 *   - UMORecipeDiscoveryComponent: Discovered recipes
 *
 * This component coordinates with:
 *   - UMONotificationComponent: Notification display (same owner)
 *   - UMOBuildingComponent: Building placement mode (same owner)
 *   - UMOPossessionSubsystem: Pawn possession system
 *   - UMOPersistenceSubsystem: Save/load operations
 *
 * =============================================================================
 */

class APlayerController;
class UMOInventoryComponent;
class UMOReticleWidget;
class UCommonActivatableWidget;
class UMOModalBackground;
class UMONotificationWidget;
class UMONotificationComponent;
class UMOFPSCounterWidget;
class UMOModeIndicatorWidget;
class UMOToolHintWidget;
class UMOInteractorComponent;
enum class EMOGameplayMode : uint8;

// Cached pawn components
class UMOSkillsComponent;
class UMOKnowledgeComponent;
class UMOCraftingQueueComponent;
class UMORecipeDiscoveryComponent;
class UMOVitalsComponent;
class UMOMetabolismComponent;
class UMOMentalStateComponent;
class UMOSurvivalStatsComponent;

// Forward declarations for public API (widgets owned by controllers)
class UUserWidget;
class UMOMenuWidgetBase;
class UMOCraftingMenu;
class UMOSkillsPanel;
class UMOBuildingMenu;
class UMOStatusPanel;
class AMOBuildableActor;
class AMOWorldItem;
struct FMOInteractionTarget;

// UI Controllers (specialized UI management)
class UMOInventoryUIController;
class UMOCraftingUIController;
class UMOBuildingUIController;
class UMOCharacterUIController;
class UMOSystemMenuUIController;
class UMOQuestUIController;

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOUIManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOUIManagerComponent();

	// Called by your IA_Inventory binding (or any other UI action).
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void ToggleInventoryMenu();

	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void OpenInventoryMenu();

	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void CloseInventoryMenu();

	UFUNCTION(BlueprintCallable, Category="MO|UI")
	bool IsInventoryMenuOpen() const;

	// --- Unified Inventory Menu (with container support) ---

	/** Open inventory with a specific container in the right panel. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Inventory")
	void OpenInventoryWithContainer(AActor* ContainerActor);

	/** Set the active container without opening the menu. Container state persists across menu open/close. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Inventory")
	void SetActiveContainer(AActor* ContainerActor);

	/** Clear the active container. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Inventory")
	void ClearActiveContainer();

	/** Get the currently active container (may be null). */
	UFUNCTION(BlueprintPure, Category="MO|UI|Inventory")
	AActor* GetActiveContainer() const;

	/** Check if there is an active container. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Inventory")
	bool HasActiveContainer() const;

	// --- Nearby World Items ---

	/** Query for world items within the nearby radius. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Inventory")
	TArray<AMOWorldItem*> QueryNearbyWorldItems() const;

	/** Loot all nearby world items into the player's inventory. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Inventory")
	int32 LootAllNearbyItems();

	/** Get the radius used for nearby items query. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Inventory")
	float GetNearbyItemsQueryRadius() const;

	/** Set the radius used for nearby items query. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Inventory")
	void SetNearbyItemsQueryRadius(float NewRadius);

	/** Show or hide the reticle. */
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void SetReticleVisible(bool bVisible);

	/** Check if the reticle is currently visible. */
	UFUNCTION(BlueprintPure, Category="MO|UI")
	bool IsReticleVisible() const;

	/** Get the reticle widget (may be null if not created yet). */
	UFUNCTION(BlueprintPure, Category="MO|UI")
	UMOReticleWidget* GetReticleWidget() const;

	/** Refresh FPS counter visibility based on game settings. Call after changing bShowFPSCounter. */
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void RefreshFPSCounter();

	// --- Player Status Panel ---

	/** Toggle player status panel visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Status")
	void TogglePlayerStatus();

	/** Get the status panel widget (may be null if not created yet). */
	UFUNCTION(BlueprintPure, Category="MO|UI|Status")
	UMOStatusPanel* GetStatusPanel() const;

	/** Show or hide the player status panel. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Status")
	void SetPlayerStatusVisible(bool bVisible);

	/** Check if player status panel is visible. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Status")
	bool IsPlayerStatusVisible() const;

	/** Rebind the status panel to current pawn's medical components. Call after pawn changes. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Status")
	void RebindStatusPanelToCurrentPawn();

	// --- Pawn Component Caching ---

	/**
	 * Cache component references from the possessed pawn.
	 * Called automatically by MOPlayerController::OnPossess.
	 * This eliminates repeated FindComponentByClass calls during gameplay.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void CachePawnComponents(APawn* NewPawn);

	/** Clear all cached pawn component references. Called on unpossess. */
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void ClearCachedPawnComponents();

	// --- In-Game Menu ---

	/** Toggle in-game menu (Tab key behavior). Closes other menus first. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void ToggleInGameMenu();

	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void OpenInGameMenu();

	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void CloseInGameMenu();

	UFUNCTION(BlueprintPure, Category="MO|UI|InGameMenu")
	bool IsInGameMenuOpen() const;

	// --- Possession Menu ---

	/** Toggle possession menu visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Possession")
	void TogglePossessionMenu();

	/** Open the possession menu. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Possession")
	void OpenPossessionMenu();

	/** Close the possession menu. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Possession")
	void ClosePossessionMenu();

	/** Check if possession menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Possession")
	bool IsPossessionMenuOpen() const;

	/** Refresh the possession menu with current pawn data. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Possession")
	void RefreshPossessionMenu();

	// --- Crafting Menu ---

	/** Toggle crafting menu visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Crafting")
	void ToggleCraftingMenu();

	/** Open the crafting menu. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Crafting")
	void OpenCraftingMenu();

	/** Close the crafting menu. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Crafting")
	void CloseCraftingMenu();

	/** Check if crafting menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Crafting")
	bool IsCraftingMenuOpen() const;

	/** Get the crafting menu widget (may be null if not open). */
	UFUNCTION(BlueprintPure, Category="MO|UI|Crafting")
	UMOCraftingMenu* GetCraftingMenu() const;

	// --- Skills Panel ---

	/** Toggle skills panel visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Skills")
	void ToggleSkillsPanel();

	/** Open the skills panel. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Skills")
	void OpenSkillsPanel();

	/** Close the skills panel. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Skills")
	void CloseSkillsPanel();

	/** Check if skills panel is open. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Skills")
	bool IsSkillsPanelOpen() const;

	/** Get the skills panel widget (may be null if not open). */
	UFUNCTION(BlueprintPure, Category="MO|UI|Skills")
	UMOSkillsPanel* GetSkillsPanel() const;

	// --- Quest Log ---

	/** Toggle quest log visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Quest")
	void ToggleQuestLog();

	/** Open the quest log panel. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Quest")
	void OpenQuestLog();

	/** Close the quest log panel. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Quest")
	void CloseQuestLog();

	/** Check if quest log is open. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Quest")
	bool IsQuestLogOpen() const;

	// --- Building Menu ---

	/** Toggle building menu visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Building")
	void ToggleBuildingMenu();

	/** Open the building menu. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Building")
	void OpenBuildingMenu();

	/** Close the building menu. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Building")
	void CloseBuildingMenu();

	/** Check if building menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Building")
	bool IsBuildingMenuOpen() const;

	/** Get the building menu widget (may be null if not open). */
	UFUNCTION(BlueprintPure, Category="MO|UI|Building")
	UMOBuildingMenu* GetBuildingMenu() const;

	// --- Build Widget (for ghost interaction) ---

	/** Show the build widget for a ghost building. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Building")
	void ShowBuildWidget(AMOBuildableActor* Target);

	/** Hide the build widget. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Building")
	void HideBuildWidget();

	/** Check if build widget is open. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Building")
	bool IsBuildWidgetOpen() const;

	// --- Station Context Menu ---

	/** Show the station context menu for a crafting station. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|CraftingStation")
	void ShowStationContextMenu(AActor* StationActor, FVector WorldPosition);

	/** Hide the station context menu. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|CraftingStation")
	void HideStationContextMenu();

	/** Check if station context menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|UI|CraftingStation")
	bool IsStationContextMenuOpen() const;

	// --- KeepOnHarvest Context Menu ---

	/** Show the keep-on-harvest context menu for an ISM/HISM target. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Harvest")
	void ShowKeepOnHarvestContextMenu(const FMOInteractionTarget& Target);

	/** Hide the keep-on-harvest context menu. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Harvest")
	void HideKeepOnHarvestContextMenu();

	/** Check if keep-on-harvest context menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Harvest")
	bool IsKeepOnHarvestContextMenuOpen() const;

	/** Start a harvest operation on the current target. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Harvest")
	void StartHarvestOperation(FName RecipeId);

	/** Cancel any active harvest operation. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Harvest")
	void CancelHarvestOperation();

	/** Check if a harvest operation is in progress. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Harvest")
	bool IsHarvestInProgress() const;

	// --- Carcass Butchering ---

	/** Start butchering a carcass part. Shows progress and locks input. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Carcass")
	void StartCarcassButchering(class AMOCarcassActor* Carcass, FName PartId);

	/** Cancel any active carcass butchering. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Carcass")
	void CancelCarcassButchering();

	/** Check if carcass butchering is in progress. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Carcass")
	bool IsCarcassButcheringInProgress() const;

	// --- Inspection ---

	/** Start inspecting an item. Shows progress widget and grants knowledge on completion. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Inspection")
	void StartItemInspection(FName ItemDefinitionId, const FGuid& ItemGuid);

	/** Cancel any active inspection. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Inspection")
	void CancelItemInspection();

	/** Check if an inspection is currently in progress. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Inspection")
	bool IsInspectionInProgress() const;

	// --- Item Context Menu ---

	/** Show context menu for an inventory item at the given screen position. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ContextMenu")
	void ShowItemContextMenu(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid, int32 SlotIndex, FVector2D ScreenPosition);

	UFUNCTION(BlueprintCallable, Category="MO|UI|ContextMenu")
	void CloseItemContextMenu();

	UFUNCTION(BlueprintPure, Category="MO|UI|ContextMenu")
	bool IsItemContextMenuOpen() const;

	// --- Confirmation Dialogs ---

	/** Show a confirmation dialog. Returns immediately; result comes via delegates. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Confirmation")
	void ShowConfirmationDialog(const FText& Title, const FText& Message, const FText& ConfirmText, const FText& CancelText);

	/** Called when any confirmation is confirmed. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOConfirmationConfirmedSignature);
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Confirmation")
	FMOConfirmationConfirmedSignature OnConfirmationConfirmed;

	/** Called when any confirmation is cancelled. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOConfirmationCancelledSignature);
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Confirmation")
	FMOConfirmationCancelledSignature OnConfirmationCancelled;

	// --- Pawn Requirement System ---

	/** Called when a menu requires a pawn but none is possessed. Hook this to open possession menu. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMONoPawnForMenuSignature);
	UPROPERTY(BlueprintAssignable, Category="MO|UI")
	FMONoPawnForMenuSignature OnNoPawnForMenu;

	/** Check if the player controller currently has a valid pawn. */
	UFUNCTION(BlueprintPure, Category="MO|UI")
	bool HasValidPawn() const;

	// --- Menu Stack ---

	/** Check if any menu is currently open. */
	UFUNCTION(BlueprintPure, Category="MO|UI")
	bool IsAnyMenuOpen() const;

	/**
	 * Close the topmost active menu. Called by Tab/Escape handlers.
	 * Checks menus in priority order: Modal > InGame > Switchable.
	 * @return True if a menu was closed, false if no menus were open.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	bool CloseActiveMenu();

	/** Close all open menus. */
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void CloseAllMenus();

	/**
	 * Close all menus that participate in menu switching.
	 * Menus flagged with bClosesOnSwitch will be closed.
	 * Use this before opening a new switchable menu.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void CloseAllSwitchableMenus();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* ResolveOwningPlayerController() const;
	bool IsLocalOwningPlayerController() const;

	UMOInventoryComponent* ResolveCurrentPawnInventoryComponent() const;

	/** Helper to restore input state when all menus are closed. Called after closing a menu. */
	void RestoreInputStateIfNoMenusOpen();

private:
	// --- Reticle ---

	/** Widget class for the reticle. If not set, uses the default UMOReticleWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOReticleWidget> ReticleWidgetClass;

	/** Z-order for the reticle widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 ReticleZOrder = 0;

	/** Whether to create the reticle automatically on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(AllowPrivateAccess="true"))
	bool bCreateReticleOnBeginPlay = true;

	/** Whether to hide the reticle when inventory menu is open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(AllowPrivateAccess="true"))
	bool bHideReticleWhenMenuOpen = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOReticleWidget> ReticleWidget;

	void CreateReticle();

	// --- FPS Counter ---

	/** Widget class for the FPS counter. Must be set to display FPS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|FPS", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOFPSCounterWidget> FPSCounterWidgetClass;

	/** Z-order for the FPS counter widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|FPS", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 FPSCounterZOrder = 1000;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOFPSCounterWidget> FPSCounterWidget;

	/** Create the FPS counter widget (called on BeginPlay if setting is enabled). */
	void CreateFPSCounter();

	/** Refresh FPS counter visibility based on current settings. */
	void RefreshFPSCounterVisibility();

	// --- Helpers ---

	void UpdateReticleVisibility();

	/** Drop item from inventory into world in front of player by GUID. */
	void DropItemToWorldByGuid(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid);

	// --- Modal Background ---

	/** Shows the modal background behind menus. Click to close all menus. */
	void ShowModalBackground();

	/** Hides the modal background. */
	void HideModalBackground();

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOModalBackground> ModalBackgroundWidget;

	// Must be BELOW PrimaryGameLayout (Z=0) so clicks on menus reach the menu first.
	// Clicks in transparent layout areas pass through to the modal background, triggering close-on-outside.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(AllowPrivateAccess="true"))
	int32 ModalBackgroundZOrder = -1;

	UFUNCTION()
	void HandleModalBackgroundClicked();

	// --- No Pawn Notification ---

	/** Optional custom widget class for no-pawn notification. If not set, uses default UMONotificationWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|NoPawn", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMONotificationWidget> NoPawnNotificationClass;

	/** Message to display when trying to open a pawn-required menu without a pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|NoPawn", meta=(AllowPrivateAccess="true"))
	FText NoPawnMessage = NSLOCTEXT("MO", "NoPawnMessage", "Please select a character to view their information");

	/** Duration to show the no-pawn notification (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|NoPawn", meta=(AllowPrivateAccess="true", ClampMin="0.5"))
	float NoPawnNotificationDuration = 3.0f;

	/** Z-order for the no-pawn notification. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|NoPawn", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 NoPawnNotificationZOrder = 250;

	/** Shows a centered notification that a pawn is required. Broadcasts OnNoPawnForMenu. */
	void ShowNoPawnNotification();

	/** Hides the no-pawn notification if visible. */
	void HideNoPawnNotification();

	/** Timer handle for auto-hiding the notification. */
	FTimerHandle NoPawnNotificationTimerHandle;

	/** The notification widget (simple text display). */
	UPROPERTY(Transient)
	TWeakObjectPtr<UMONotificationWidget> NoPawnNotificationWidget;

	// --- Notifications (delegated to UMONotificationComponent) ---

public:
	/**
	 * Show a notification message. Delegates to UMONotificationComponent if available.
	 * @note Prefer using UMONotificationComponent directly for new code.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Notifications")
	void ShowNotification(const FText& Message, float Duration = 3.0f);

	/** Show skill increase notification. Delegates to UMONotificationComponent. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Notifications")
	void ShowSkillIncreaseNotification(FName SkillId, float XPAmount);

	/** Show recipe unlocked notification. Delegates to UMONotificationComponent. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Notifications")
	void ShowRecipeUnlockedNotification(FName RecipeId);

	/** Get the notification component (may be null). */
	UFUNCTION(BlueprintPure, Category="MO|UI|Notifications")
	UMONotificationComponent* GetNotificationComponent() const;

	// --- UI Controller Support ---
	// These methods are exposed for use by specialized UI controllers (MOUIControllerBase subclasses).
	// They provide access to shared UI operations that need to be centralized.

	// --- Specialized UI Controllers ---
	// These controllers handle specific UI subsystems and are created on BeginPlay.

	/** Get the inventory UI controller. Handles inventory menu, containers, item context menus. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Controllers")
	UMOInventoryUIController* GetInventoryController() const;

	/** Get the crafting UI controller. Handles crafting menu, station context, harvest operations. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Controllers")
	UMOCraftingUIController* GetCraftingController() const;

	/** Get the building UI controller. Handles building menu, ghost context menu. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Controllers")
	UMOBuildingUIController* GetBuildingController() const;

	/** Get the character UI controller. Handles skills panel, status panel, inspection. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Controllers")
	UMOCharacterUIController* GetCharacterController() const;

	/** Get the system menu UI controller. Handles in-game menu, possession menu, confirmations. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Controllers")
	UMOSystemMenuUIController* GetSystemMenuController() const;

	/** Get the quest UI controller. Handles quest log panel and HUD tracker. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Controllers")
	UMOQuestUIController* GetQuestController() const;

	/** Show the modal background. Used by UI controllers when opening menus. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Internal")
	void RequestShowModalBackground();

	/** Hide the modal background. Used by UI controllers when closing menus. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Internal")
	void RequestHideModalBackground();

	// NOTE: RequestInputModeForMenuOpen/Closed have been removed.
	// CommonUI handles input mode automatically via GetDesiredInputConfig() on widgets.

	/** Request reticle visibility update based on menu state. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Internal")
	void RequestUpdateReticleVisibility();

	// --- Mode Indicator (bottom-right HUD showing current gameplay mode) ---

	/** Set the current gameplay mode. Updates the mode indicator. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Mode")
	void SetGameplayMode(EMOGameplayMode NewMode);

	/** Get the current gameplay mode. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Mode")
	EMOGameplayMode GetGameplayMode() const;

	/** Get the mode indicator widget (may be null if not created). */
	UFUNCTION(BlueprintPure, Category="MO|UI|Mode")
	UMOModeIndicatorWidget* GetModeIndicator() const;

	// --- Tool Hint (small popup showing current tool) ---

	/** Show a tool hint message. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ToolHint")
	void ShowToolHint(const FText& HintText, float Duration = 0.0f);

	/**
	 * Hide the current tool hint, clearing BOTH persistent and transient state.
	 * Use this from the persistent owner's exit path (e.g. terraform mode toggle off).
	 * Internal focus-hint code should use EndTransientToolHint instead so it doesn't
	 * wipe a persistent indicator owned by gameplay mode.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ToolHint")
	void HideToolHint();

	/**
	 * End any in-flight transient hint. If a persistent hint was previously
	 * set via ShowToolHint(text, < 0), it re-displays. If not, the widget hides.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ToolHint")
	void EndTransientToolHint();

	/** Get the tool hint widget (may be null if not created). */
	UFUNCTION(BlueprintPure, Category="MO|UI|ToolHint")
	UMOToolHintWidget* GetToolHintWidget() const;

	// --- Focus Hint (shows item name when looking at interactable objects) ---

	/** Enable or disable the focus hint display. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|FocusHint")
	void SetFocusHintEnabled(bool bEnabled);

	/** Check if focus hint is enabled. */
	UFUNCTION(BlueprintPure, Category="MO|UI|FocusHint")
	bool IsFocusHintEnabled() const { return bFocusHintEnabled; }

	/** Get the current focus hint text (what player is looking at). */
	UFUNCTION(BlueprintPure, Category="MO|UI|FocusHint")
	FText GetCurrentFocusHintText() const { return CurrentFocusHintText; }

private:
	/** Cached reference to notification component on same owner. Mutable to allow caching in const methods. */
	UPROPERTY(Transient)
	mutable TWeakObjectPtr<UMONotificationComponent> CachedNotificationComponent;

	/** Find or cache the notification component. */
	UMONotificationComponent* ResolveNotificationComponent() const;

	// =========================================================================
	// SPECIALIZED UI CONTROLLERS
	// =========================================================================
	// These controller components handle specific UI subsystems.
	// They are sibling components on the same PlayerController owner.
	// References are resolved on first access and cached.

	/** Cached reference to inventory UI controller. */
	mutable TWeakObjectPtr<UMOInventoryUIController> CachedInventoryController;

	/** Cached reference to crafting UI controller. */
	mutable TWeakObjectPtr<UMOCraftingUIController> CachedCraftingController;

	/** Cached reference to building UI controller. */
	mutable TWeakObjectPtr<UMOBuildingUIController> CachedBuildingController;

	/** Cached reference to character UI controller. */
	mutable TWeakObjectPtr<UMOCharacterUIController> CachedCharacterController;

	/** Cached reference to system menu UI controller. */
	mutable TWeakObjectPtr<UMOSystemMenuUIController> CachedSystemMenuController;

	/** Cached reference to quest UI controller. */
	mutable TWeakObjectPtr<UMOQuestUIController> CachedQuestController;

	// --- Mode Indicator ---

	/** Widget class for the mode indicator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Mode", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOModeIndicatorWidget> ModeIndicatorClass;

	/** Z-order for the mode indicator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Mode", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 ModeIndicatorZOrder = 5;

	/** Whether to create the mode indicator on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Mode", meta=(AllowPrivateAccess="true"))
	bool bCreateModeIndicatorOnBeginPlay = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOModeIndicatorWidget> ModeIndicatorWidget;

	EMOGameplayMode CurrentGameplayMode;

	void CreateModeIndicator();

	// --- Tool Hint ---

	/** Widget class for tool hints. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|ToolHint", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOToolHintWidget> ToolHintClass;

	/** Z-order for tool hints. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|ToolHint", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 ToolHintZOrder = 5;

	/** Whether to create the tool hint widget on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|ToolHint", meta=(AllowPrivateAccess="true"))
	bool bCreateToolHintOnBeginPlay = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOToolHintWidget> ToolHintWidget;

	void CreateToolHint();

	// --- Input Mode Settings ---

	/** Whether to show mouse cursor when a menu is open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|InputMode", meta=(AllowPrivateAccess="true"))
	bool bShowMouseCursorWhileMenuOpen = true;

	/** Whether to lock movement input when a menu is open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|InputMode", meta=(AllowPrivateAccess="true"))
	bool bLockMovementWhileMenuOpen = true;

	/** Whether to lock look input when a menu is open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|InputMode", meta=(AllowPrivateAccess="true"))
	bool bLockLookWhileMenuOpen = true;

	// --- Focus Hint ---

	/** Whether focus hint is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|FocusHint", meta=(AllowPrivateAccess="true"))
	bool bFocusHintEnabled = true;

	/** Update interval for focus hint line trace (in seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|FocusHint", meta=(AllowPrivateAccess="true", ClampMin="0.05", ClampMax="1.0"))
	float FocusHintUpdateInterval = 0.2f;

	/** Timer handle for focus hint updates. */
	FTimerHandle FocusHintTimerHandle;

	/** Current focus hint text. */
	FText CurrentFocusHintText;

	/** Cached interactor component from pawn. */
	UPROPERTY(Transient)
	TWeakObjectPtr<class UMOInteractorComponent> CachedInteractorComponent;

	/** Update focus hint based on what player is looking at. Called on timer. */
	void UpdateFocusHint();

	/** Start the focus hint timer. */
	void StartFocusHintTimer();

	/** Stop the focus hint timer. */
	void StopFocusHintTimer();

	// ============================================================================
	// CACHED PAWN COMPONENTS
	// ============================================================================
	// These are cached on possession change to avoid repeated FindComponentByClass calls.
	// Use the accessor methods (GetCached*) which return nullptr if invalid.

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> CachedPawn;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOInventoryComponent> CachedInventoryComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOSkillsComponent> CachedSkillsComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOKnowledgeComponent> CachedKnowledgeComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOCraftingQueueComponent> CachedCraftingQueueComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMORecipeDiscoveryComponent> CachedRecipeDiscoveryComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOVitalsComponent> CachedVitalsComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOMetabolismComponent> CachedMetabolismComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOMentalStateComponent> CachedMentalStateComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOSurvivalStatsComponent> CachedSurvivalStatsComponent;

public:
	// =========================================================================
	// CACHED PAWN COMPONENT ACCESSORS
	// =========================================================================
	// Public accessors for UI controllers and other systems that need
	// access to the cached pawn components. These validate the weak pointer
	// before returning (returns nullptr if invalid).

	UFUNCTION(BlueprintPure, Category="MO|UI|Components")
	UMOInventoryComponent* GetCachedInventory() const;

	UFUNCTION(BlueprintPure, Category="MO|UI|Components")
	UMOSkillsComponent* GetCachedSkills() const;

	UFUNCTION(BlueprintPure, Category="MO|UI|Components")
	UMOKnowledgeComponent* GetCachedKnowledge() const;

	UFUNCTION(BlueprintPure, Category="MO|UI|Components")
	UMOCraftingQueueComponent* GetCachedCraftingQueue() const;

	UFUNCTION(BlueprintPure, Category="MO|UI|Components")
	UMORecipeDiscoveryComponent* GetCachedRecipeDiscovery() const;

	UFUNCTION(BlueprintPure, Category="MO|UI|Components")
	UMOVitalsComponent* GetCachedVitals() const;

	UFUNCTION(BlueprintPure, Category="MO|UI|Components")
	UMOMetabolismComponent* GetCachedMetabolism() const;

	UFUNCTION(BlueprintPure, Category="MO|UI|Components")
	UMOMentalStateComponent* GetCachedMentalState() const;

	UFUNCTION(BlueprintPure, Category="MO|UI|Components")
	UMOSurvivalStatsComponent* GetCachedSurvivalStats() const;
};
