/**
 * =============================================================================
 * MOStatusMoodleWidget.h - One icon slot in the status effect strip
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * Single moodle entry widget shown inside UMOStatusEffectStripWidget. Owns
 * the icon + (optional) label + (optional) stack count + tint mapping for
 * the severity bucket.
 *
 * USAGE (Blueprint):
 *   1. Create WBP_StatusMoodle based on this class.
 *   2. Add UImage named "IconImage" (required BindWidget).
 *   3. Optional widgets:
 *        - LabelText (UTextBlock) — short label shown next to icon
 *        - StackText (UTextBlock) — count like "x3"
 *   4. Override OnSeverityChanged in BP to apply per-severity styling
 *      (border, glow, etc.) — or just rely on the tint applied here.
 *
 * The widget calls SetMoodle to populate. The strip widget creates entries
 * dynamically via TSubclassOf — see UMOStatusEffectStripWidget.
 *
 * =============================================================================
 * RELATED FILES:
 *   - MOStatusEffectStripWidget.h  (owner)
 *   - MOStatusMoodleTypes.h        (data struct)
 * LAST UPDATED: 2026-05-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOStatusMoodleTypes.h"
#include "MOStatusMoodleWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS()
class MOFRAMEWORK_API UMOStatusMoodleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Populate this entry from a moodle struct. Safe to call before NativeConstruct. */
	UFUNCTION(BlueprintCallable, Category="MO|HUD|Moodle")
	void SetMoodle(const FMOStatusMoodle& InMoodle);

	/** Read the current moodle (handy for tooltip composition in BP). */
	UFUNCTION(BlueprintPure, Category="MO|HUD|Moodle")
	const FMOStatusMoodle& GetMoodle() const { return CurrentMoodle; }

	/**
	 * Optional BP hook — called whenever SetMoodle changes the severity.
	 * Override in WBP_StatusMoodle to apply per-severity styling (glow,
	 * border, etc.) beyond the basic tint.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|HUD|Moodle")
	void OnSeverityChanged(EMOStatusMoodleSeverity NewSeverity);

protected:
	virtual void NativeConstruct() override;

	/** Apply CurrentMoodle to the bound widgets. No-op if widgets aren't bound yet. */
	void ApplyMoodleToWidgets();

	/** Required icon. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> IconImage;

	/** Optional short label (shown next to icon). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	/** Optional stack count (e.g. "x3"). Hidden when Stack == 0. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StackText;

	/**
	 * Severity → tint mapping. Designers can override per-WBP in Class
	 * Defaults to rebrand without C++ changes. Defaults follow standard
	 * status-bar conventions (info=neutral, warning=amber, critical=red,
	 * buff=green).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|HUD|Moodle|Style")
	FLinearColor InfoTint = FLinearColor(0.8f, 0.8f, 0.8f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|HUD|Moodle|Style")
	FLinearColor WarningTint = FLinearColor(1.f, 0.7f, 0.2f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|HUD|Moodle|Style")
	FLinearColor CriticalTint = FLinearColor(1.f, 0.25f, 0.25f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|HUD|Moodle|Style")
	FLinearColor BuffTint = FLinearColor(0.4f, 1.f, 0.5f, 1.f);

private:
	/** Last data set — applied to widgets on construct / SetMoodle. */
	UPROPERTY(Transient)
	FMOStatusMoodle CurrentMoodle;

	/** Cached severity to detect changes so OnSeverityChanged only fires on actual transitions. */
	EMOStatusMoodleSeverity LastAppliedSeverity = EMOStatusMoodleSeverity::Info;

	FLinearColor TintForSeverity(EMOStatusMoodleSeverity Severity) const;
};
