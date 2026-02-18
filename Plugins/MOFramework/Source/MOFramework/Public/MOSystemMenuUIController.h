#pragma once

#include "CoreMinimal.h"
#include "MOUIControllerBase.h"
#include "MOSystemMenuUIController.generated.h"

class UMOInGameMenu;
class UMOPossessionMenu;
class UMOPawnEntryWidget;
class UMOConfirmationDialog;

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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|System Menu|Confirmation", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 ConfirmationDialogZOrder = 200;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOConfirmationDialog> ConfirmationDialogWidget;

	/** Context string for the pending confirmation (e.g., "ExitToMainMenu", "Save:SlotName") */
	FString PendingConfirmationContext;

	UFUNCTION()
	void HandleConfirmationConfirmed();

	UFUNCTION()
	void HandleConfirmationCancelled();
};
