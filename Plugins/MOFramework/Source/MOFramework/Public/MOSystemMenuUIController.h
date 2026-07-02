/**
 * =============================================================================
 * MOSystemMenuUIController.h - System Menus and Dialogs UI
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Specialized UI controller for system-level interfaces. Manages in-game menu
 * (pause/save/load), possession menu for pawn selection, confirmation dialogs,
 * and survivor command menus. Part of the UIManager split architecture.
 *
 * KEY RESPONSIBILITIES:
 * 1. Manage In-Game menu (pause, save, load, options, exit)
 * 2. Manage Possession menu for pawn selection/creation
 * 3. Display confirmation dialogs with callback delegates
 * 4. Show survivor context menus and task assignment menus
 *
 * UI WIDGETS MANAGED:
 * - InGameMenu: Pause menu with save/load/exit options
 * - PossessionMenu: Pawn list with selection and create buttons
 * - ConfirmationDialog: Generic yes/no dialog
 * - SurvivorContextMenu: Right-click commands for survivors
 * - SurvivorTaskMenu: Full task assignment panel
 *
 * CONFIRMATION DIALOG PATTERN:
 * ShowConfirmationDialog(Title, Message, ConfirmText, CancelText)
 * -> User clicks confirm -> OnConfirmationConfirmed broadcast
 * -> User clicks cancel -> OnConfirmationCancelled broadcast
 * PendingConfirmationContext tracks which action requested it
 *
 * CRITICAL PATTERNS:
 * 1. In-Game Menu:
 *    ToggleInGameMenu() -> ESC key binding
 *    HandleSaveRequested/LoadRequested -> PersistenceSubsystem
 *    HandleExitToMainMenu -> Level transition
 *
 * 2. Possession Menu:
 *    OpenPossessionMenu() -> Query possessable pawns
 *    HandlePossessionMenuPawnSelected(Guid) -> PossessionSubsystem
 *    HandlePossessionMenuCreateCharacter() -> Spawn new pawn
 *
 * 3. Survivor Context Menu:
 *    ShowSurvivorContextMenu(Pawn, ScreenPos)
 *    -> Follow Me, Stay Here, Go Home, Open Tasks
 *    -> HandleSurvivorContextMenuOpenTasks() -> ShowSurvivorTaskMenu()
 *
 * KNOWN PITFALLS:
 * 1. CONFIRMATION CONTEXT: PendingConfirmationContext must be set before
 *    showing dialog. Handlers check context to determine action.
 *
 * 2. LEVEL PATHS: MainMenuLevelPath and GameplayLevelPath are configurable.
 *    Incorrect paths will cause level load failures.
 *
 * 3. POSSESSION TIMING: Possession menu refresh may conflict with
 *    ongoing possession. Wait for possession complete.
 *
 * 4. SURVIVOR WEAK REF: CurrentSurvivorTarget is weak pointer.
 *    Survivor can die while menu is open.
 *
 * RELATED FILES:
 * - MOUIControllerBase.h - Base class with shared utilities
 * - MOInGameMenu.h - Pause menu widget
 * - MOPossessionMenu.h - Pawn selection widget
 * - MOConfirmationDialog.h - Generic dialog widget
 * - MOSurvivorContextMenu.h - Survivor command widget
 * - MOSurvivorTaskMenu.h - Task assignment widget
 * - MOPossessionSubsystem.h - Pawn possession logic
 *
 * TESTING CHECKLIST:
 * [ ] ESC opens in-game menu
 * [ ] Save/load work from menu
 * [ ] Exit to main menu transitions correctly
 * [ ] Possession menu shows all possessable pawns
 * [ ] Pawn selection possesses correctly
 * [ ] Confirmation dialog callbacks fire
 * [ ] Survivor context menu shows commands
 * [ ] Task menu opens from context menu
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOUIControllerBase.h"
#include "MOSystemMenuUIController.generated.h"

class UMOInGameMenu;
class UMOPossessionMenu;
class UMOPawnEntryWidget;
class UMOConfirmationDialog;
class UMOSurvivorContextMenu;
class UMOSurvivorTaskMenu;
class UMODeathRecapWidget;
class AMOCharacter;
enum class EMOBodyPartType : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnConfirmationConfirmedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnConfirmationCancelledDelegate);

/**
 * Specialized UI controller for system-related UI.
 *
 * Handles:
 * - In-game menu (pause, save, load, options, exit)
 * - Possession menu (pawn selection, character creation)
 * - Confirmation dialogs
 *
 * This controller is extracted from MOUIManagerComponent to reduce its size
 * and provide clear ownership of the system menu UI subsystem.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOSystemMenuUIController : public UMOUIControllerBase
{
	GENERATED_BODY()

public:
	UMOSystemMenuUIController();

	// ==========================================================================
	// IN-GAME MENU
	// ==========================================================================

	/** Toggle in-game menu visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|System Menu")
	void ToggleInGameMenu();

	/** Open the in-game menu. */
	UFUNCTION(BlueprintCallable, Category="MO|System Menu")
	void OpenInGameMenu();

	/** Close the in-game menu. */
	UFUNCTION(BlueprintCallable, Category="MO|System Menu")
	void CloseInGameMenu();

	/** Check if in-game menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|System Menu")
	bool IsInGameMenuOpen() const;

	/** Get the in-game menu widget (may be null if not open). */
	UFUNCTION(BlueprintPure, Category="MO|System Menu")
	UMOInGameMenu* GetInGameMenu() const;

	// ==========================================================================
	// POSSESSION MENU
	// ==========================================================================

	/** Toggle possession menu visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|System Menu|Possession")
	void TogglePossessionMenu();

	/** Open the possession menu. */
	UFUNCTION(BlueprintCallable, Category="MO|System Menu|Possession")
	void OpenPossessionMenu();

	/** Close the possession menu. */
	UFUNCTION(BlueprintCallable, Category="MO|System Menu|Possession")
	void ClosePossessionMenu();

	/** Check if possession menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|System Menu|Possession")
	bool IsPossessionMenuOpen() const;

	/** Refresh the possession menu's pawn list. */
	UFUNCTION(BlueprintCallable, Category="MO|System Menu|Possession")
	void RefreshPossessionMenu();

	/** Get the possession menu widget (may be null if not open). */
	UFUNCTION(BlueprintPure, Category="MO|System Menu|Possession")
	UMOPossessionMenu* GetPossessionMenu() const;

	// ==========================================================================
	// CONFIRMATION DIALOGS
	// ==========================================================================

	/** Show a confirmation dialog. */
	UFUNCTION(BlueprintCallable, Category="MO|System Menu|Confirmation")
	void ShowConfirmationDialog(const FText& Title, const FText& Message, const FText& ConfirmText, const FText& CancelText);

	/** Delegates for confirmation results */
	UPROPERTY(BlueprintAssignable, Category="MO|System Menu|Events")
	FMOOnConfirmationConfirmedDelegate OnConfirmationConfirmed;

	UPROPERTY(BlueprintAssignable, Category="MO|System Menu|Events")
	FMOOnConfirmationCancelledDelegate OnConfirmationCancelled;

	// ==========================================================================
	// SURVIVOR CONTEXT MENU
	// ==========================================================================

	/** Show the survivor context menu for a recruited survivor. */
	UFUNCTION(BlueprintCallable, Category="MO|System Menu|Survivor")
	void ShowSurvivorContextMenu(APawn* Survivor, FVector2D ScreenPosition);

	/** Close the survivor context menu. */
	UFUNCTION(BlueprintCallable, Category="MO|System Menu|Survivor")
	void CloseSurvivorContextMenu();

	/** Check if survivor context menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|System Menu|Survivor")
	bool IsSurvivorContextMenuOpen() const;

	/** Show the survivor task menu for assigning jobs. */
	UFUNCTION(BlueprintCallable, Category="MO|System Menu|Survivor")
	void ShowSurvivorTaskMenu(APawn* Survivor);

	/** Close the survivor task menu. */
	UFUNCTION(BlueprintCallable, Category="MO|System Menu|Survivor")
	void CloseSurvivorTaskMenu();

	/** Check if survivor task menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|System Menu|Survivor")
	bool IsSurvivorTaskMenuOpen() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Rebind death-recap subscription when the player swaps pawns. We listen
	 * to AMOCharacter::OnPawnDied on whichever character the local PC is
	 * currently possessing — when the player dies, we push the recap UI.
	 */
	virtual void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn) override;

private:
	/** Frame-based debounce to prevent double-toggle from ECommonInputMode::All */
	uint64 LastToggleFrame = 0;

	// --- In-Game Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|System Menu", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOInGameMenu> InGameMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|System Menu", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 InGameMenuZOrder = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|System Menu", meta=(AllowPrivateAccess="true"))
	FString MainMenuLevelPath = TEXT("/Game/Penumbra/Maps/LoadingLevel");

	/** Path to the gameplay level (for reloading when loading saves in-game). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|System Menu", meta=(AllowPrivateAccess="true"))
	FString GameplayLevelPath = TEXT("/Game/VoxelExamples/PCGScattering/MOPCGScattering");

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|System Menu|Possession", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOPossessionMenu> PossessionMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|System Menu|Possession", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 PossessionMenuZOrder = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|System Menu|Possession", meta=(AllowPrivateAccess="true"))
	TSubclassOf<APawn> DefaultPawnClassForNewCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|System Menu|Possession", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOPawnEntryWidget> PawnEntryWidgetClass;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOPossessionMenu> PossessionMenuWidget;

	UFUNCTION()
	void HandlePossessionMenuRequestClose();

	UFUNCTION()
	void HandlePossessionMenuPawnSelected(const FGuid& PawnGuid);

	UFUNCTION()
	void HandlePossessionMenuCreateCharacter();

	// --- Confirmation Dialog ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|System Menu|Confirmation", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOConfirmationDialog> ConfirmationDialogClass;
	// (ConfirmationDialogZOrder removed -- L7 vestige: dialogs go through the CommonUI
	// Modal layer stack now, which owns ordering; the property was read by nothing.)

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOConfirmationDialog> ConfirmationDialogWidget;

	/** Context string for the pending confirmation (e.g., "ExitToMainMenu", "Save:SlotName") */
	FString PendingConfirmationContext;

	UFUNCTION()
	void HandleConfirmationConfirmed();

	UFUNCTION()
	void HandleConfirmationCancelled();

	// --- Survivor Context Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|System Menu|Survivor", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOSurvivorContextMenu> SurvivorContextMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|System Menu|Survivor", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 SurvivorContextMenuZOrder = 60;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOSurvivorContextMenu> SurvivorContextMenuWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|System Menu|Survivor", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOSurvivorTaskMenu> SurvivorTaskMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|System Menu|Survivor", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 SurvivorTaskMenuZOrder = 100;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOSurvivorTaskMenu> SurvivorTaskMenuWidget;

	/** Current survivor being commanded. */
	TWeakObjectPtr<APawn> CurrentSurvivorTarget;

	UFUNCTION()
	void HandleSurvivorContextMenuRequestClose();

	UFUNCTION()
	void HandleSurvivorContextMenuOpenTasks(APawn* Survivor);

	UFUNCTION()
	void HandleSurvivorContextMenuOpenInventory(APawn* Survivor);

	UFUNCTION()
	void HandleSurvivorTaskMenuRequestClose();

	// --- Death Recap ---
	//
	// Subscribes to OnPawnDied on the player's currently-possessed AMOCharacter.
	// When the player dies, builds a recap snapshot, pushes the recap widget to
	// Layer_Modal, and lets the user either return to main menu or continue
	// (continue path defers to OnContinueRequested — currently no respawn
	// implementation, so the button can stay hidden in the WBP).
	//
	// The subscription rebinds when OnPossessedPawnChanged fires so the recap
	// always tracks the live possessed pawn rather than the BeginPlay-time one.

	/** Blueprint class for the death recap modal (assign WBP_DeathRecap here). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|System Menu|Death", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMODeathRecapWidget> DeathRecapWidgetClass;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMODeathRecapWidget> DeathRecapWidget;

	/** Last MOCharacter we subscribed OnPawnDied on — held weakly for safe unbind. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AMOCharacter> SubscribedDeathPawn;

	UFUNCTION()
	void HandlePawnDied(AMOCharacter* DeadCharacter, EMOBodyPartType CausePart);

	/** Bind OnPawnDied on Pawn (rebinds if already subscribed elsewhere). */
	void BindDeathListenerToPawn(AMOCharacter* Character);

	/** Unbind OnPawnDied from whatever pawn we subscribed to last. */
	void UnbindDeathListener();

	/**
	 * Centralized "open the main menu level" path — called by both the
	 * in-game-menu exit confirmation flow and the death recap "Return"
	 * button. Single source of truth for the exit sequence.
	 */
	void ReturnToMainMenuImmediate();
};
