/**
 * MOTimeOfDayDialog.cpp - The dev `time` popup: drag a clock hand, watch the sky follow, apply
 */

#include "MOTimeOfDayDialog.h"
#include "MOClockDial.h"
#include "MOCommonButton.h"
#include "MOGameClockSubsystem.h"
#include "MOWeatherIntegrationSubsystem.h"
#include "MOFramework.h"
#include "Components/TextBlock.h"
#include "CommonInputModeTypes.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "MOTimeOfDayDialog"

UMOTimeOfDayDialog::UMOTimeOfDayDialog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Dragging the hand near the panel's edge must not close it.
	bClosesOnOutsideClick = false;
}

void UMOTimeOfDayDialog::NativeConstruct()
{
	Super::NativeConstruct();
	if (Dial)
	{
		Dial->OnMinutesChanged.RemoveAll(this);
		Dial->OnMinutesChanged.AddUObject(this, &UMOTimeOfDayDialog::HandleDialMinutes);
		Dial->OnMinutesCommitted.RemoveAll(this);
		Dial->OnMinutesCommitted.AddUObject(this, &UMOTimeOfDayDialog::HandleDialMinutes);
	}
	if (ApplyButton)
	{
		ApplyButton->OnClicked().RemoveAll(this);
		ApplyButton->OnClicked().AddUObject(this, &UMOTimeOfDayDialog::HandleApplyClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOTimeOfDayDialog::HandleCancelClicked);
	}
	if (DawnButton)
	{
		DawnButton->OnClicked().RemoveAll(this);
		DawnButton->OnClicked().AddUObject(this, &UMOTimeOfDayDialog::HandleGoldenHour, false);
	}
	if (DuskButton)
	{
		DuskButton->OnClicked().RemoveAll(this);
		DuskButton->OnClicked().AddUObject(this, &UMOTimeOfDayDialog::HandleGoldenHour, true);
	}
}

void UMOTimeOfDayDialog::NativeDestruct()
{
	EndPreview();
	if (Dial)
	{
		Dial->OnMinutesChanged.RemoveAll(this);
		Dial->OnMinutesCommitted.RemoveAll(this);
	}
	for (UMOCommonButton* Button : { ApplyButton.Get(), CancelButton.Get(), DawnButton.Get(), DuskButton.Get() })
	{
		if (Button)
		{
			Button->OnClicked().RemoveAll(this);
		}
	}
	OnChosen.Clear();
	OnCancelled.Clear();
	Super::NativeDestruct();
}

void UMOTimeOfDayDialog::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	// Whatever closed it, the sky goes back to the clock, and a pooled instance carries no past consumer's bindings into the next open.
	EndPreview();
	OnChosen.Clear();
	OnCancelled.Clear();
}

UWidget* UMOTimeOfDayDialog::NativeGetDesiredFocusTarget() const
{
	return ApplyButton;
}

bool UMOTimeOfDayDialog::NativeOnCloseKeyRequested(const FKeyEvent& InKeyEvent)
{
	HandleCancelClicked();
	return true;
}

TOptional<FUIInputConfig> UMOTimeOfDayDialog::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, EMouseLockMode::DoNotLock, false);
}

FText UMOTimeOfDayDialog::FormatMinutes(int32 MinutesOfDay)
{
	const int32 M = ((MinutesOfDay % 1440) + 1440) % 1440;
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), M / 60, M % 60));
}

void UMOTimeOfDayDialog::Configure()
{
	PreviewedMinutes = -1;
	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	const FDateTime Now = Clock ? Clock->GetGameDateTime() : FDateTime(2026, 6, 1, 12, 0, 0);
	float Sunrise = 6.0f, Sunset = 18.0f;
	if (const UMOWeatherIntegrationSubsystem* Weather = GetWorld() ? GetWorld()->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr)
	{
		Weather->GetSunriseSunsetHours(Sunrise, Sunset);
	}
	SelectedMinutes = Now.GetHour() * 60 + Now.GetMinute();
	if (Dial)
	{
		Dial->SetDaylight(Sunrise, Sunset);
		Dial->SetMinutesOfDay(SelectedMinutes);
	}
	if (TitleText)
	{
		TitleText->SetText(LOCTEXT("Title", "Time of day"));
	}
	if (MessageText)
	{
		MessageText->SetText(FText::Format(LOCTEXT("Message", "Sunrise {0} · sunset {1}. Drag the hand: the sky follows. Apply moves the clock forward to it."),
			FormatMinutes(FMath::RoundToInt(Sunrise * 60.0f)), FormatMinutes(FMath::RoundToInt(Sunset * 60.0f))));
	}
	if (TimeText)
	{
		TimeText->SetText(FormatMinutes(SelectedMinutes));
	}
}

void UMOTimeOfDayDialog::SelectMinutes(int32 MinutesOfDay)
{
	if (Dial)
	{
		Dial->SetMinutesOfDay(MinutesOfDay);
	}
	HandleDialMinutes(((MinutesOfDay % 1440) + 1440) % 1440);
}

void UMOTimeOfDayDialog::HandleDialMinutes(int32 MinutesOfDay)
{
	SelectedMinutes = MinutesOfDay;
	if (TimeText)
	{
		TimeText->SetText(FormatMinutes(SelectedMinutes));
	}
	PreviewSky();
}

void UMOTimeOfDayDialog::PreviewSky()
{
	if (SelectedMinutes == PreviewedMinutes)
	{
		return;   // one push per minute the hand actually moved to
	}
	UMOWeatherIntegrationSubsystem* Weather = GetWorld() ? GetWorld()->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr;
	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	if (!Weather || !Clock)
	{
		return;
	}
	// The day Apply would land on (the clock only moves forward), so a sky that simulates the real sun shows the right season too.
	const FDateTime Now = Clock->GetGameDateTime();
	FDateTime Target = Now.GetDate() + FTimespan::FromMinutes(SelectedMinutes);
	if (Target <= Now)
	{
		Target += FTimespan::FromDays(1);
	}
	Weather->PreviewSkyAt(Target);
	PreviewedMinutes = SelectedMinutes;
}

void UMOTimeOfDayDialog::EndPreview()
{
	if (PreviewedMinutes < 0)
	{
		return;
	}
	PreviewedMinutes = -1;
	if (UMOWeatherIntegrationSubsystem* Weather = GetWorld() ? GetWorld()->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr)
	{
		Weather->EndSkyPreview();
	}
}

void UMOTimeOfDayDialog::HandleGoldenHour(bool bEvening)
{
	const UMOWeatherIntegrationSubsystem* Weather = GetWorld() ? GetWorld()->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr;
	SelectMinutes(Weather ? Weather->GetGoldenHourMinutesOfDay(bEvening) : (bEvening ? 17 * 60 + 30 : 6 * 60 + 30));
}

void UMOTimeOfDayDialog::HandleApplyClicked()
{
	const int32 Minutes = SelectedMinutes;
	UE_LOG(LogMOFramework, Log, TEXT("[MOTimeDialog] apply %s"), *FormatMinutes(Minutes).ToString());
	OnChosen.Broadcast(Minutes);   // the server moves the clock; the preview ends with the deactivation below and the sky follows the clock there
	DeactivateWidget();
}

void UMOTimeOfDayDialog::HandleCancelClicked()
{
	OnCancelled.Broadcast();
	DeactivateWidget();
}

#undef LOCTEXT_NAMESPACE
