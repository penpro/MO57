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
#include "MOPossessionTypes.h"
#include "MOPawnEntryWidget.generated.h"

class UTextBlock;
class UMOCommonButton;
class UCommonButtonBase;
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

	/**
	 * Who is in this survivor now (the server's possession row): "Current survivor" for the one you are playing, "Played by <name>" for another
	 * player's (their Steam name on Steam). Either way the Possess button is off -- you cannot take your own or someone else's.
	 */
	void SetOccupancy(EMOPawnOccupancy InOccupancy, const FString& OccupantName);

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

	/**
	 * The row-wide button the entry blueprint lays OVER the row (its overlay draws it above PossessButton): every real click on the row, the Select
	 * button included, lands on it (MO.Test.HitTest: "BLOCKED ... EntryButton"). Unbound until Oct 18 2026, so the possession window took no click at
	 * all ("still can't select any pawns from the possession window", Wes). It does what Select does: the whole row of a free survivor selects it.
	 */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> EntryButton;

	/** "Current survivor" / "Played by <name>" (SetOccupancy); without it the status line says so. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> OccupantText;

private:
	UFUNCTION()
	void HandlePossessClicked();

	FGuid CachedPawnGuid;
	bool bIsDeceased = false;
	EMOPawnOccupancy Occupancy = EMOPawnOccupancy::Free;
};
