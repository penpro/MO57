// The breath meter: a thin bar low on the screen that appears while the player's airway is closed (head under water) or the reserve is still recovering, and fades away when the lungs are full.
//
// Procedural (no Widget Blueprint): the whole tree is built in NativeOnInitialized, so the HUD works without any designer asset. It reads the local pawn's UMORespirationComponent (the oxygen reserve is
// replicated to the owner, so clients show it too) and colours itself by how much time is left (kit tokens): info blue, a warning amber once the urge to breathe is strong, a critical red that pulses when the blackout is close. Drawn as the kit's
// segmented bar (10 segments x 36 px, gap 4) on a HUD plate; severity also reads from how many segments are lit.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBreathMeterWidget.generated.h"

class UImage;
class UTextBlock;
class UMORespirationComponent;

UCLASS()
class MOFRAMEWORK_API UMOBreathMeterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Colour / visibility for a reserve: pure so the automation tests check the thresholds. Returns the bar colour; OutPulse = 0..1 red pulse strength. */
	static FLinearColor ColorForReserve(float Reserve01, float UrgeReserve, float& OutPulse);

	/** Should the meter show: head under water (airway closed) or the reserve not yet back to full. */
	static bool ShouldShow(bool bAirwayClosed, float Reserve01);

	/** A segment's brush. Untinted (white): each tick's colour (track -> severity) is the whole colour, since an image draws brush tint x colour. */
	static FSlateBrush SegmentBrush();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	static constexpr int32 SegmentCount = 10;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> Segments;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	float Alpha = 0.0f;
	float PulseTime = 0.0f;
};
