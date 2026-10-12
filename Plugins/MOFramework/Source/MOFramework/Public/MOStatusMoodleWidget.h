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
class UWidget;

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
	 * Severity is shown by colour AND shape (never colour alone): an optional glyph (kit ic_sev_*: circle-check / diamond / triangle / octagon) and, for Critical, the kit's double border that pulses
	 * (alpha 1 -> 0.55 over `motion.pulse.critical`; static when reduced motion sets the token to 0). The tint colours are tokens (color.severity.*, colour-blind block honoured), not properties.
	 */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> SeverityShapeImage;

	/** Optional: any widget; shown only while the moodle is Critical and pulsed. The kit `critical_border` 9-slice, padded -6 around the pill. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> CriticalBorder;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** Last data set — applied to widgets on construct / SetMoodle. */
	UPROPERTY(Transient)
	FMOStatusMoodle CurrentMoodle;

	/** Cached severity to detect changes so OnSeverityChanged only fires on actual transitions. */
	EMOStatusMoodleSeverity LastAppliedSeverity = EMOStatusMoodleSeverity::Info;

	static FName ColorTokenForSeverity(EMOStatusMoodleSeverity Severity);
	static FName ShapeIconForSeverity(EMOStatusMoodleSeverity Severity);
	FLinearColor TintForSeverity(EMOStatusMoodleSeverity Severity) const;

	float PulseClock = 0.0f;
};
