#include "MOWorldClockWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "MOGameClockSubsystem.h"

FText UMOWorldClockWidget::FormatClock(const FDateTime& Time)
{
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Time.GetHour(), Time.GetMinute()));
}

FText UMOWorldClockWidget::FormatDate(const FDateTime& Time)
{
	static const TCHAR* Months[] = { TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"), TEXT("May"), TEXT("Jun"), TEXT("Jul"), TEXT("Aug"), TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec") };
	return FText::FromString(FString::Printf(TEXT("%d %s"), Time.GetDay(), Months[FMath::Clamp(Time.GetMonth() - 1, 0, 11)]));
}

void UMOWorldClockWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SinceRefresh = 1.0f;
	LastMinute = -1;
	Refresh();
}

void UMOWorldClockWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	SinceRefresh += InDeltaTime;
	if (SinceRefresh >= 0.25f)
	{
		SinceRefresh = 0.0f;
		Refresh();
	}
	if (TickIcon && TickGlow > 0.0f)
	{
		TickGlow = FMath::Max(0.0f, TickGlow - InDeltaTime * 2.0f);
		TickIcon->SetRenderOpacity(0.45f + 0.55f * TickGlow);
	}
}

void UMOWorldClockWidget::Refresh()
{
	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	if (!Clock)
	{
		return;
	}
	const FDateTime Now = Clock->GetGameDateTime();
	if (TimeText)
	{
		TimeText->SetText(FormatClock(Now));
	}
	if (DateText)
	{
		DateText->SetText(FormatDate(Now));
	}
	if (Now.GetMinute() != LastMinute)
	{
		LastMinute = Now.GetMinute();
		TickGlow = 1.0f;   // the clock icon blinks once per in-game minute: the world is visibly running
	}
}
