/**
 * =============================================================================
 * MOPawnEntryWidget.h - Pawn Selection Entry Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Individual pawn entry in the possession menu. Shows pawn info (name, age,
 * health, status) and allows possession via button click.
 *
 * DISPLAYS:
 * - NameText: Character name
 * - AgeText: Character age
 * - GenderText: Gender display
 * - HealthBar: Current health percentage
 * - StatusText: Current activity/status
 * - LocationText: Where the pawn currently is
 * - LastPlayedText: When this pawn was last possessed
 * - PortraitImage: Character portrait/thumbnail
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] DECEASED PAWNS: bIsDeceased flag disables possess button. May still
 *   show in list for memorial/history purposes.
 *
 * [2024-02] GUID IDENTITY: Uses FGuid from FMOPersistedPawnRecord, not actor
 *   pointers. Pawn may not be spawned when entry is created.
 *
 * [2024-02] BUTTON BINDING: Uses UMOCommonButton. Bind with OnClicked().AddUObject().
 *
 * =============================================================================
 * RELATED FILES: MOPossessionMenu.h, MOPossessionSubsystem.h, MOWorldSaveGame.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

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
	virtual void NativeDestruct() override;

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
