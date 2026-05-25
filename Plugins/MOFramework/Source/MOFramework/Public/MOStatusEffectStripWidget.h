/**
 * =============================================================================
 * MOStatusEffectStripWidget.h - Generic moodle bar for HUD status indicators
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * Horizontal strip of UMOStatusMoodleWidget instances. Sources of game state
 * (vitals component, weather integration, quest system, etc.) push moodles
 * in via AddOrUpdateMoodle and remove them via RemoveMoodle. The strip
 * hides itself entirely when empty.
 *
 * Reusable for every "show this small status icon on the HUD" use case so we
 * don't end up with N bespoke widgets per status type. Wet, bleeding,
 * hungry, well-fed, buffs from food/items — all the same widget class with
 * different FMOStatusMoodle data.
 *
 * USAGE (Blueprint):
 *   1. Create WBP_StatusEffectStrip based on this class.
 *   2. Add UHorizontalBox named "MoodleBox" (required BindWidget).
 *   3. Set MoodleEntryClass = WBP_StatusMoodle in BP defaults.
 *   4. Drop WBP_StatusEffectStrip into WBP_HUDRoot (tick "Is Variable" with
 *      name "StatusStrip" so the C++ BindWidgetOptional picks it up).
 *
 * Sources call:
 *   UMOStatusEffectStripWidget::AddOrUpdateMoodle(Strip, Moodle)
 *   UMOStatusEffectStripWidget::RemoveMoodle(Strip, MoodleId)
 *
 * =============================================================================
 * RELATED FILES:
 *   - MOStatusMoodleWidget.h    (entry widget)
 *   - MOStatusMoodleTypes.h     (data struct)
 *   - MOHUDRootWidget.h         (host)
 * LAST UPDATED: 2026-05-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOStatusMoodleTypes.h"
#include "MOStatusEffectStripWidget.generated.h"

class UHorizontalBox;
class UMOStatusMoodleWidget;

UCLASS()
class MOFRAMEWORK_API UMOStatusEffectStripWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Add a new moodle or update an existing one (matched by Moodle.Id).
	 * Updating an existing slot avoids the remove-and-recreate flicker
	 * sources would otherwise hit when refreshing periodic state.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|HUD|StatusStrip")
	void AddOrUpdateMoodle(const FMOStatusMoodle& Moodle);

	/** Remove a moodle by Id. No-op if Id isn't registered. */
	UFUNCTION(BlueprintCallable, Category="MO|HUD|StatusStrip")
	void RemoveMoodle(FName MoodleId);

	/** Drop every moodle. */
	UFUNCTION(BlueprintCallable, Category="MO|HUD|StatusStrip")
	void ClearAllMoodles();

	/** Is a moodle with this Id currently shown? */
	UFUNCTION(BlueprintPure, Category="MO|HUD|StatusStrip")
	bool HasMoodle(FName MoodleId) const;

	/** Diagnostic count for cheat commands / debug overlays. */
	UFUNCTION(BlueprintPure, Category="MO|HUD|StatusStrip")
	int32 GetMoodleCount() const { return ActiveMoodles.Num(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Required horizontal box that hosts every moodle entry. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UHorizontalBox> MoodleBox;

	/**
	 * Subclass for each entry. Set this in the WBP defaults to WBP_StatusMoodle.
	 * Required — without it AddOrUpdateMoodle logs a warning and bails.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|HUD|StatusStrip")
	TSubclassOf<UMOStatusMoodleWidget> MoodleEntryClass;

	/**
	 * Auto-hide the strip when no moodles are active. Defaults true — most
	 * HUD designs don't want an empty box drawing layout space.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|HUD|StatusStrip")
	bool bAutoCollapseWhenEmpty = true;

private:
	/** Active moodles by stable Id — the source of truth for what's shown. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UMOStatusMoodleWidget>> ActiveMoodles;

	void UpdateVisibility();
};
