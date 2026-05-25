/**
 * =============================================================================
 * MODeathRecapWidget.h - Post-Death Recap Screen
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * Full-screen modal shown when the local player's possessed pawn dies. Displays
 * a snapshot of the moment of death — character name, cause, location, and
 * (eventually) achievements/skills earned during this life. Routes "Return
 * to Main Menu" and "Continue" intents via delegates so MOSystemMenuUIController
 * can decide what happens next (the widget owns presentation only).
 *
 * DATA FLOW:
 *   AMOCharacter::HandleInstantDeath
 *     -> broadcasts OnPawnDied(DeadCharacter, CausePart)
 *   UMOSystemMenuUIController::HandlePawnDied
 *     -> builds FMODeathRecapData from the pawn's components
 *     -> pushes UMODeathRecapWidget to Layer_Modal
 *     -> calls SetRecapData(...)
 *   User clicks button
 *     -> OnReturnToMenuRequested / OnContinueRequested fires
 *     -> MOSystemMenuUIController routes to MOGameMode / level transition
 *
 * USAGE (Blueprint):
 *   1. Create WBP_DeathRecap based on UMODeathRecapWidget.
 *   2. Add UTextBlocks named:
 *        CharacterNameText   (required BindWidget)
 *        CauseOfDeathText    (required BindWidget)
 *        LocationText        (optional)
 *        TimeOfDeathText     (optional)
 *   3. Add UMOCommonButtons named:
 *        ReturnToMenuButton  (required BindWidget)
 *        ContinueButton      (optional — hide if no respawn path yet)
 *   4. Set TSubclassOf<UMODeathRecapWidget> DeathRecapWidgetClass on
 *      UMOSystemMenuUIController in BP defaults.
 *
 * =============================================================================
 * RELATED FILES:
 *   - MOCharacter.h  (FMOOnPawnDied delegate)
 *   - MOSystemMenuUIController.h  (subscriber that pushes this widget)
 *   - MOModalWidget.h  (base class)
 * LAST UPDATED: 2026-05-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOModalWidget.h"
#include "MOBodyPartTypes.h"
#include "MODeathRecapWidget.generated.h"

class UTextBlock;
class UMOCommonButton;
class AMOCharacter;

/**
 * Snapshot of the moment-of-death state needed to render the recap.
 * Built by UMOSystemMenuUIController from the dying pawn's components,
 * passed to UMODeathRecapWidget::SetRecapData.
 *
 * All FText fields are pre-formatted so the widget can SetText directly
 * without doing any data lookup. Keep the struct lean — additions are
 * cheap and we'll grow it as more sources of death info appear (combat,
 * starvation, exposure).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMODeathRecapData
{
	GENERATED_BODY()

	/** Display name of the pawn that died. Falls back to actor name if no IdentityComponent. */
	UPROPERTY(BlueprintReadWrite, Category="MO|DeathRecap")
	FText CharacterName;

	/** Formatted cause-of-death string (e.g. "Head destroyed", "Heart pierced"). */
	UPROPERTY(BlueprintReadWrite, Category="MO|DeathRecap")
	FText CauseOfDeath;

	/** Pre-formatted world location at moment of death (e.g. "(123, 456, 789)"). */
	UPROPERTY(BlueprintReadWrite, Category="MO|DeathRecap")
	FText LocationText;

	/** In-game time/date of death if a game clock subsystem is available, otherwise empty. */
	UPROPERTY(BlueprintReadWrite, Category="MO|DeathRecap")
	FText TimeOfDeathText;
};

/**
 * Full-screen modal showing the death recap. Inherits from UMOModalWidget for
 * full input blocking + Menu input mode. Buttons emit delegates only; routing
 * (respawn, return to menu) is the caller's responsibility.
 */
UCLASS()
class MOFRAMEWORK_API UMODeathRecapWidget : public UMOModalWidget
{
	GENERATED_BODY()

public:
	/** User clicked "Return to Main Menu". */
	DECLARE_MULTICAST_DELEGATE(FMOOnDeathRecapReturnToMenu);
	FMOOnDeathRecapReturnToMenu OnReturnToMenuRequested;

	/** User clicked "Continue" (respawn / possess next pawn). Only fires if ContinueButton is present. */
	DECLARE_MULTICAST_DELEGATE(FMOOnDeathRecapContinue);
	FMOOnDeathRecapContinue OnContinueRequested;

	/**
	 * Populate the recap from a death snapshot. Safe to call before
	 * NativeConstruct (cached and applied once the widget tree exists).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|DeathRecap")
	void SetRecapData(const FMODeathRecapData& InData);

	/**
	 * Helper that builds a FMODeathRecapData snapshot from a dying character.
	 * Pulls identity name, cause text from CausePart, and world location.
	 * Future: pull final vitals/skills/inventory summary.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|DeathRecap")
	static FMODeathRecapData BuildRecapData(AMOCharacter* DeadCharacter, EMOBodyPartType CausePart);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Apply RecapData to the text widgets. No-op if widgets aren't bound yet. */
	void ApplyDataToWidgets();

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> CharacterNameText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> CauseOfDeathText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LocationText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeOfDeathText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> ReturnToMenuButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> ContinueButton;

private:
	/** Last data set — applied to widgets on construct (and again on SetRecapData). */
	UPROPERTY(Transient)
	FMODeathRecapData RecapData;

	UFUNCTION() void HandleReturnToMenuClicked();
	UFUNCTION() void HandleContinueClicked();
};
