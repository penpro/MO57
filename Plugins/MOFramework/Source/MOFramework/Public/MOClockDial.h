/**
 * =============================================================================
 * MOClockDial.h - A 24-hour clock face whose hand is dragged to pick a time of day
 * =============================================================================
 *
 * PURPOSE:
 * Wes, Oct 15 2026: "just a clock where you can grab the hour hand and adjust the time and have it auto update with the clock hand so you can pick a time
 * that you want". One hand on a 24-hour face: midnight at the bottom, noon at the top, so the hand sweeps like the sun (up on the left in the morning, down on
 * the right in the evening). The daylight arc (sunrise to sunset, from the sky) is drawn round the rim, so golden hour is where the arc meets the night.
 *
 * Drag the hand (or click anywhere on the face) to move it; the wheel steps 15 minutes. OnMinutesChanged fires on every snapped step while dragging (the
 * time dialog previews the sky with it), OnMinutesCommitted when the hand is let go.
 *
 * It is a plain UWidget over a Slate leaf (SMOClockDial): it has its own hit-testing and paint, so it is a real node in a Widget Blueprint tree (unlike a native
 * UUserWidget, which would be an empty click-through spacer -- Docs/UI_TOOLING.md). Spec type: "/Script/MOFramework.MOClockDial".
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Fonts/SlateFontInfo.h"
#include "MOClockDial.generated.h"

class SMOClockDial;

DECLARE_MULTICAST_DELEGATE_OneParam(FMOClockDialMinutes, int32 /*MinutesOfDay*/);

UCLASS()
class MOFRAMEWORK_API UMOClockDial : public UWidget
{
	GENERATED_BODY()

public:
	UMOClockDial(const FObjectInitializer& ObjectInitializer);

	/** Put the hand at this time (0..1439, wrapped). Does not broadcast. */
	void SetMinutesOfDay(int32 Minutes);
	int32 GetMinutesOfDay() const { return MinutesOfDay; }

	/** The daylight arc round the rim (hours, fractional). */
	void SetDaylight(float InSunriseHour, float InSunsetHour);

	/** Every snapped step while the hand moves (drag, click, wheel). */
	FMOClockDialMinutes OnMinutesChanged;

	/** The hand was let go (or a wheel step). */
	FMOClockDialMinutes OnMinutesCommitted;

	/** The geometry, shared with the tests: the offset of a time from the face's centre on a unit circle (X right, Y down; midnight at the bottom). */
	static FVector2D MinutesToUnitOffset(float Minutes);

	/** The time a point on the face points at (offset from the centre, any length; Y down), snapped to SnapMinutes and wrapped to 0..1439. */
	static int32 OffsetToMinutes(const FVector2D& Offset, int32 SnapMinutes);

	/** The hand moves in steps of this many minutes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clock", meta=(ClampMin="1", ClampMax="60"))
	int32 SnapMinutes = 5;

	/** Width and height the dial asks for. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clock", meta=(ClampMin="64"))
	float Diameter = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clock")
	FLinearColor FaceColor = FLinearColor(0.02f, 0.025f, 0.035f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clock")
	FLinearColor DayColor = FLinearColor(0.95f, 0.70f, 0.25f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clock")
	FLinearColor NightColor = FLinearColor(0.16f, 0.22f, 0.45f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clock")
	FLinearColor TickColor = FLinearColor(0.55f, 0.55f, 0.58f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clock")
	FLinearColor HandColor = FLinearColor(0.93f, 0.93f, 0.90f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clock")
	FLinearColor LabelColor = FLinearColor(0.75f, 0.75f, 0.75f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clock")
	FSlateFontInfo LabelFont;

	// UWidget
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void HandleSlateMinutes(int32 Minutes, bool bCommit);

	TSharedPtr<SMOClockDial> MyDial;
	int32 MinutesOfDay = 12 * 60;
	float SunriseHour = 6.0f;
	float SunsetHour = 18.0f;
};
