/**
 * =============================================================================
 * MOTimeOfDayDialog.h - The dev `time` popup: drag a clock hand, watch the sky follow, apply
 * =============================================================================
 *
 * PURPOSE:
 * Wes, Oct 15 2026: "add a dusk and dawn time to the time menu for golden hour type screen shots as well as just a clock where you can grab the hour hand and
 * adjust the time and have it auto update with the clock hand so you can pick a time that you want". A 24-hour dial (UMOClockDial); dragging the hand
 * previews THIS machine's sky at that time (UMOWeatherIntegrationSubsystem::PreviewSkyAt, the game clock untouched); Dawn / Dusk put the hand on golden hour
 * (GetGoldenHourMinutesOfDay); Apply broadcasts OnChosen and the controller moves the server's clock forward to it (the clock never runs backwards: an
 * earlier time is tomorrow's). Cancel / Escape restores the sky to the clock.
 *
 * BLUEPRINT SETUP: WBP_TimeOfDayDialog (Content/Python/ui_specs/kit_time_dialog.py) with parent UMOTimeOfDayDialog. Required: Dial, TimeText, ApplyButton,
 * CancelButton. Optional: TitleText, MessageText, DawnButton, DuskButton.
 *
 * KNOWN PITFALLS
 * - Pooled by PushModalWidget: every delegate is cleared and the sky preview ended when the dialog deactivates, whatever closed it.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOTimeOfDayDialog.generated.h"

class UTextBlock;
class UMOCommonButton;
class UMOClockDial;

DECLARE_MULTICAST_DELEGATE_OneParam(FMOTimeOfDayChosen, int32 /*MinutesOfDay*/);
DECLARE_MULTICAST_DELEGATE(FMOTimeOfDayCancelled);

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOTimeOfDayDialog : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOTimeOfDayDialog(const FObjectInitializer& ObjectInitializer);

	/** Open on the game clock's time with the sky's sunrise and sunset (call after the push). */
	void Configure();

	/** Move the hand as a drag would (the sky previews it). For the Dawn / Dusk buttons, tests and tooling. */
	void SelectMinutes(int32 MinutesOfDay);

	int32 GetSelectedMinutes() const { return SelectedMinutes; }

	/** "06:30": the one formatting of a time of day in this dialog. */
	static FText FormatMinutes(int32 MinutesOfDay);

	/** Apply: the chosen time of day (minutes), just before the dialog closes. */
	FMOTimeOfDayChosen OnChosen;

	/** Cancel / Escape. */
	FMOTimeOfDayCancelled OnCancelled;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnCloseKeyRequested(const FKeyEvent& InKeyEvent) override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMOClockDial> Dial;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TimeText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMOCommonButton> ApplyButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMOCommonButton> CancelButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** Sunrise / sunset and what Apply does. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMOCommonButton> DawnButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMOCommonButton> DuskButton;

private:
	void HandleDialMinutes(int32 MinutesOfDay);
	void HandleApplyClicked();
	void HandleCancelClicked();
	void HandleGoldenHour(bool bEvening);
	void PreviewSky();
	void EndPreview();

	int32 SelectedMinutes = 12 * 60;
	int32 PreviewedMinutes = -1;
};
