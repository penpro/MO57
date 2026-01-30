#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOworldSaveGame.h"
#include "MOPawnEntryWidget.generated.h"

class UTextBlock;
class UMOCommonButton;
class UImage;
class UProgressBar;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOPawnEntryPossessSignature, const FGuid&, PawnGuid);

/**
 * Widget representing a single pawn entry in the possession menu.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOPawnEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Initialize this entry with pawn data. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Possession")
	void InitializeEntry(const FMOPersistedPawnRecord& PawnRecord);

	/** Get the GUID of the pawn this entry represents. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Possession")
	FGuid GetPawnGuid() const { return CachedPawnGuid; }

	/** Check if this entry represents a deceased pawn. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Possession")
	bool IsDeceased() const { return bIsDeceased; }

	/** Called when possess button is clicked. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Possession")
	FMOPawnEntryPossessSignature OnPossessClicked;

protected:
	virtual void NativeConstruct() override;

	/** Blueprint event when entry is initialized. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|Possession")
	void OnEntryInitialized(const FMOPersistedPawnRecord& PawnRecord);

	// Bound widgets
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> AgeText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> GenderText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LocationText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LastPlayedText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> PossessButton;

private:
	UFUNCTION()
	void HandlePossessClicked();

	FGuid CachedPawnGuid;
	bool bIsDeceased = false;
};
