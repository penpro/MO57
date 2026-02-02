#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
class UMOInventoryMenu;
class UMOReticleWidget;
class UMOInGameMenu;
class UMOItemContextMenu;
class UMOConfirmationDialog;
class UCommonActivatableWidget;
class UMOStatusPanel;
class UMOModalBackground;
class UMOVitalsComponent;
class UMOMetabolismComponent;
class UMOMentalStateComponent;
class UMONotificationWidget;
class UMOPossessionMenu;
class UMOPawnEntryWidget;
class UMOCraftingMenu;
class UMOSkillsPanel;
class UMOSkillsComponent;
class UMOKnowledgeComponent;
class UMOCraftingQueueComponent;
class UMORecipeDiscoveryComponent;
class UMOInspectionProgressWidget;
class UMONotificationComponent;
class UMOBuildingMenu;
class UMOBuildWidget;
class AMOBuildableActor;
struct FMOInspectionResult;

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

	/** Show or hide the reticle. */
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void SetReticleVisible(bool bVisible);

	/** Check if the reticle is currently visible. */
	UFUNCTION(BlueprintPure, Category="MO|UI")
	bool IsReticleVisible() const;

	/** Get the reticle widget (may be null if not created yet). */
	UFUNCTION(BlueprintPure, Category="MO|UI")
	UMOReticleWidget* GetReticleWidget() const;

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

	void ApplyInputModeForMenuClosed(APlayerController* PlayerController) const;

	UFUNCTION()
	void HandleInventoryMenuRequestClose();

	UFUNCTION()
	void HandleInventoryMenuSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition);

private:
	// Set this in the component defaults (or on the component instance in your PlayerController BP).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOInventoryMenu> InventoryMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 InventoryMenuZOrder = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(AllowPrivateAccess="true"))
	bool bShowMouseCursorWhileMenuOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(AllowPrivateAccess="true"))
	bool bLockMovementWhileMenuOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(AllowPrivateAccess="true"))
	bool bLockLookWhileMenuOpen = true;

	// Weak pointer so we do not keep dead widgets alive.
	UPROPERTY(Transient)
	TWeakObjectPtr<UMOInventoryMenu> InventoryMenuWidget;

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

	// --- Player Status Panel ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOStatusPanel> StatusPanelClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 StatusPanelZOrder = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(AllowPrivateAccess="true"))
	bool bCreateStatusPanelOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(AllowPrivateAccess="true"))
	bool bHideStatusPanelWhenMenuOpen = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOStatusPanel> StatusPanelWidget;

	/** Tracks whether status panel is currently visible (avoids querying widget visibility) */
	bool bStatusPanelVisible = false;

	void CreateStatusPanel();

	UFUNCTION()
	void HandleStatusPanelRequestClose();

	/** Get medical components from current pawn (null-safe). */
	void GetCurrentPawnMedicalComponents(UMOVitalsComponent*& OutVitals, UMOMetabolismComponent*& OutMetabolism, UMOMentalStateComponent*& OutMental) const;

	// --- In-Game Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|InGameMenu", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOInGameMenu> InGameMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|InGameMenu", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 InGameMenuZOrder = 100;

	/** Level to open when exiting to main menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|InGameMenu", meta=(AllowPrivateAccess="true"))
	FString MainMenuLevelPath = TEXT("/Game/Penumbra/Maps/LoadingLevel");

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOInGameMenu> InGameMenuWidget;

	UFUNCTION()
	void HandleInGameMenuRequestClose();

	UFUNCTION()
	void HandleInGameMenuExitToMainMenu();

	UFUNCTION()
	void HandleInGameMenuExitGame();

	UFUNCTION()
	void HandleSaveRequested(const FString& SlotName);

	UFUNCTION()
	void HandleLoadRequested(const FString& SlotName);

	// --- Possession Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Possession", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOPossessionMenu> PossessionMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Possession", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOPawnEntryWidget> PawnEntryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Possession", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 PossessionMenuZOrder = 100;

	/** Default pawn class to spawn when creating new character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Possession", meta=(AllowPrivateAccess="true"))
	TSubclassOf<APawn> DefaultPawnClassForNewCharacter;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOPossessionMenu> PossessionMenuWidget;

	UFUNCTION()
	void HandlePossessionMenuRequestClose();

	UFUNCTION()
	void HandlePossessionMenuPawnSelected(const FGuid& PawnGuid);

	UFUNCTION()
	void HandlePossessionMenuCreateCharacter();

	// --- Crafting Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Crafting", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOCraftingMenu> CraftingMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Crafting", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 CraftingMenuZOrder = 50;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOCraftingMenu> CraftingMenuWidget;

	UFUNCTION()
	void HandleCraftingMenuRequestClose();

	// --- Skills Panel ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Skills", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOSkillsPanel> SkillsPanelClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Skills", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 SkillsPanelZOrder = 50;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOSkillsPanel> SkillsPanelWidget;

	UFUNCTION()
	void HandleSkillsPanelRequestClose();

	// --- Building Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Building", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOBuildingMenu> BuildingMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Building", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 BuildingMenuZOrder = 50;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOBuildingMenu> BuildingMenuWidget;

	UFUNCTION()
	void HandleBuildingMenuRequestClose();

	UFUNCTION()
	void HandleBuildingSelected(FName RecipeId);

	// --- Build Widget ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Building", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOBuildWidget> BuildWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Building", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 BuildWidgetZOrder = 60;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOBuildWidget> BuildWidgetWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<AMOBuildableActor> CurrentBuildTarget;

	UFUNCTION()
	void HandleBuildWidgetRequestClose();

	UFUNCTION()
	void HandleBuildWidgetStartBuild();

	// --- Inspection ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Inspection", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOInspectionProgressWidget> InspectionProgressWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Inspection", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 InspectionProgressZOrder = 200;

	/** Duration of item inspection in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Inspection", meta=(ClampMin="1.0", AllowPrivateAccess="true"))
	float InspectionDuration = 15.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOInspectionProgressWidget> InspectionProgressWidget;

	/** The item GUID currently being inspected. */
	FGuid InspectingItemGuid;

	UFUNCTION()
	void HandleInspectionCompleted(bool bCompleted, const FMOInspectionResult& Result);

	UFUNCTION()
	void HandleInspectionCancelled();

	// --- Item Context Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|ContextMenu", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOItemContextMenu> ItemContextMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|ContextMenu", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 ItemContextMenuZOrder = 150;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOItemContextMenu> ItemContextMenuWidget;

	UFUNCTION()
	void HandleContextMenuClosed();

	UFUNCTION()
	void HandleContextMenuAction(FName ActionId, const FGuid& ItemGuid);

	// --- Confirmation Dialog ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Confirmation", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOConfirmationDialog> ConfirmationDialogClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Confirmation", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 ConfirmationDialogZOrder = 200;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOConfirmationDialog> ConfirmationDialogWidget;

	/** Stores context for pending confirmations. */
	FString PendingConfirmationContext;

	UFUNCTION()
	void HandleConfirmationConfirmed();

	UFUNCTION()
	void HandleConfirmationCancelled();

	// --- Helpers ---

	void ApplyInputModeForMenuOpen(APlayerController* PlayerController, UUserWidget* MenuWidget) const;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 ModalBackgroundZOrder = 10;

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

private:
	/** Cached reference to notification component on same owner. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UMONotificationComponent> CachedNotificationComponent;

	/** Find or cache the notification component. */
	UMONotificationComponent* ResolveNotificationComponent() const;
};
