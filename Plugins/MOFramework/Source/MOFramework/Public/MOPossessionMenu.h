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
 * Shows all player-owned pawns with their status.
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
