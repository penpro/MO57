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

class UVerticalBox;
class UTextBlock;
class UWidget;
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

	/**
	 * Compact (icon only, default) or expanded (labels): applies to every entry, present and future. The HUD root expands it while the player status panel is open.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|HUD|StatusStrip")
	void SetExpanded(bool bInExpanded);

	UFUNCTION(BlueprintPure, Category="MO|HUD|StatusStrip")
	bool IsExpanded() const { return bExpanded; }

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

	/** Required vertical box that hosts every moodle entry (kit: a stack in the bottom-left corner, critical nearest the corner). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UVerticalBox> MoodleBox;

	/** Optional "+N more" pill (kit: more than 6 moodles shows the 6 worst and this pill). Put it in the box in the WBP; the strip places it last. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> OverflowPill;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> OverflowText;

	/** How many moodles show before the rest fold into the "+N more" pill. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|HUD|StatusStrip")
	int32 MaxVisibleMoodles = 6;

	/** The first (worst) moodle sits nearest the corner and the list grows UPWARD: the box is filled in reverse. Off = top-down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|HUD|StatusStrip")
	bool bStackUpward = true;

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

	bool bExpanded = false;

	/** Sort by severity (Critical, Warning, Info, Buff, then Id), keep the first MaxVisibleMoodles, fold the rest into the overflow pill, and lay them out in the box. */
	void RebuildOrder();
};
