// The live in-game clock for menu headers (kit: "menus never pause": the world keeps running behind every menu, and the in-game menu leads with a ticking "world continues" readout).
//
// Binds: TimeText (required, "17:42"), optional DateText ("12 Oct"), LabelText (static text in the WBP, untouched), TickIcon (any widget: pulses once each in-game minute).
// Reads UMOGameClockSubsystem (TimeScale 1.0 = real time), refreshed 4 times a second while visible.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOWorldClockWidget.generated.h"

class UTextBlock;
class UWidget;

UCLASS()
class MOFRAMEWORK_API UMOWorldClockWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** "17:42" (24 h, zero padded): pure, tested. */
	static FText FormatClock(const FDateTime& Time);

	/** "12 Oct". */
	static FText FormatDate(const FDateTime& Time);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> TimeText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DateText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> TickIcon;

private:
	void Refresh();

	float SinceRefresh = 1.0f;
	int32 LastMinute = -1;
	float TickGlow = 0.0f;
};
