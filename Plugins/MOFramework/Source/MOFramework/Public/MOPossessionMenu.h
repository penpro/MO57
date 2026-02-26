/**
 * =============================================================================
 * MOPossessionMenu.h - Pawn Selection Interface
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Menu for selecting which pawn to possess. Shows all player-controlled pawns
 * with status info. Allows switching control between pawns or creating new
 * characters.
 *
 * DISPLAY:
 * - List of possessed pawns (recruited survivors)
 * - Status indicators (health, location, activity)
 * - "Create New Character" button (if enabled)
 *
 * WORKFLOW:
 * 1. Player opens possession menu (Tab key or in-game menu)
 * 2. Menu populated from MOPersistenceSubsystem pawn records
 * 3. Player selects pawn -> OnPawnSelected fires
 * 4. MOPossessionSubsystem handles actual possession switch
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] PAWN RECORDS: Uses FMOPersistedPawnRecord from save system. May
 *   include dead pawns (greyed out, not selectable).
 *
 * [2024-02] SPAWNED VS RECORD: Some pawns may be in records but not spawned
 *   in world. Possession system handles spawning if needed.
 *
 * [2024-02] ACTIVATABLE WIDGET: Extends UCommonActivatableWidget. Uses
 *   activation stack for input mode management.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOPossessionSubsystem.h - Handles possession switching
 * - MOPersistenceSubsystem.h - Provides pawn records
 * - MOPawnEntryWidget.h - Individual pawn list entry
 * - MOSystemMenuUIController.h - Opens this menu
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOworldSaveGame.h"
#include "MOPossessionMenu.generated.h"

class UScrollBox;
class UMOCommonButton;
class UTextBlock;
class UMOPawnEntryWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOPossessionMenuRequestCloseSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOPossessionMenuPawnSelectedSignature, const FGuid&, PawnGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOPossessionMenuCreateCharacterSignature);

/**
 * Menu for selecting which pawn to possess.
 * See file header for workflow and pitfalls.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOPossessionMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Populate the menu with pawn records. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Possession")
	void PopulatePawnList(const TArray<FMOPersistedPawnRecord>& PawnRecords);

	/** Clear all entries from the list. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Possession")
	void ClearPawnList();

	/** Set whether the "Create New Character" button is visible. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Possession")
	void SetCreateCharacterVisible(bool bVisible);

	/** Ensure internal button bindings are set up. Call when reopening cached menu. */
	void EnsureButtonBindings();

	/** Check if there are any living pawns in the list. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Possession")
	bool HasLivingPawns() const { return LivingPawnCount > 0; }

	/** Get the count of living pawns. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Possession")
	int32 GetLivingPawnCount() const { return LivingPawnCount; }

	/** Called when user wants to close the menu. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Possession")
	FMOPossessionMenuRequestCloseSignature OnRequestClose;

	/** Called when user selects a pawn to possess. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Possession")
	FMOPossessionMenuPawnSelectedSignature OnPawnSelected;

	/** Called when user clicks "Create New Character". */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Possession")
	FMOPossessionMenuCreateCharacterSignature OnCreateCharacter;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Widget class for pawn entries. Must be set in Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Possession")
	TSubclassOf<UMOPawnEntryWidget> PawnEntryWidgetClass;

	// Bound widgets
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UScrollBox> PawnListScrollBox;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CreateCharacterButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CloseButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyListText;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleCreateCharacterClicked();

	UFUNCTION()
	void HandlePawnEntryPossessClicked(const FGuid& PawnGuid);

	/** Created entry widgets. */
	UPROPERTY()
	TArray<TObjectPtr<UMOPawnEntryWidget>> EntryWidgets;

	int32 LivingPawnCount = 0;
};
