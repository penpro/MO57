#include "MOPlayerStatusWidget.h"
#include "MOSurvivalStatsComponent.h"
#include "MOFramework.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

UMOPlayerStatusWidget::UMOPlayerStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UMOPlayerStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogMOFramework, Log, TEXT("[PlayerStatus] NativeConstruct - HealthBar=%s, HungerBar=%s, ThirstBar=%s"),
		HealthBar ? TEXT("valid") : TEXT("NULL"),
		HungerBar ? TEXT("valid") : TEXT("NULL"),
		ThirstBar ? TEXT("valid") : TEXT("NULL"));

	SetKeyboardFocus();
	UpdateAllDisplays();
}

FReply UMOPlayerStatusWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();
	if (PressedKey == EKeys::Tab || PressedKey == EKeys::Escape)
	{
		OnRequestClose.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMOPlayerStatusWidget::NativeDestruct()
{
	// Unbind from survival stats
	if (UMOSurvivalStatsComponent* Stats = SurvivalStats.Get())
	{
		Stats->OnStatChanged.RemoveDynamic(this, &UMOPlayerStatusWidget::HandleStatChanged);
	}

	Super::NativeDestruct();
}

void UMOPlayerStatusWidget::InitializeStatus(UMOSurvivalStatsComponent* InSurvivalStats)
{
	// Unbind from old
	if (UMOSurvivalStatsComponent* OldStats = SurvivalStats.Get())
	{
		OldStats->OnStatChanged.RemoveDynamic(this, &UMOPlayerStatusWidget::HandleStatChanged);
	}

	SurvivalStats = InSurvivalStats;

	// Bind to new
	if (IsValid(InSurvivalStats))
	{
		InSurvivalStats->OnStatChanged.AddDynamic(this, &UMOPlayerStatusWidget::HandleStatChanged);
	}

	UpdateAllDisplays();
}

void UMOPlayerStatusWidget::RefreshAllStats()
{
	UpdateAllDisplays();
}

void UMOPlayerStatusWidget::HandleStatChanged(FName StatName, float OldValue, float NewValue)
{
	UpdateStatDisplay(StatName);
}

void UMOPlayerStatusWidget::UpdateAllDisplays()
{
	UpdateStatDisplay(FName("Health"));
	UpdateStatDisplay(FName("Stamina"));
	UpdateStatDisplay(FName("Hunger"));
	UpdateStatDisplay(FName("Thirst"));
	UpdateStatDisplay(FName("Energy"));
	UpdateStatDisplay(FName("Temperature"));
}

void UMOPlayerStatusWidget::UpdateStatDisplay(FName StatName)
{
	UMOSurvivalStatsComponent* Stats = SurvivalStats.Get();

	float Percent = 0.0f;
	float Current = 0.0f;
	float Max = 100.0f;

	if (IsValid(Stats))
	{
		Percent = Stats->GetStatPercent(StatName);
		Current = Stats->GetStatCurrent(StatName);

		// Get max from the stat
		if (StatName == FName("Health")) Max = Stats->Health.Max;
		else if (StatName == FName("Stamina")) Max = Stats->Stamina.Max;
		else if (StatName == FName("Hunger")) Max = Stats->Hunger.Max;
		else if (StatName == FName("Thirst")) Max = Stats->Thirst.Max;
		else if (StatName == FName("Energy")) Max = Stats->Energy.Max;
		else if (StatName == FName("Temperature")) Max = Stats->Temperature.Max;
	}

	// Determine color based on thresholds
	FLinearColor BarColor = HealthyColor;
	if (Percent <= CriticalThreshold)
	{
		BarColor = CriticalColor;
	}
	else if (Percent <= WarningThreshold)
	{
		BarColor = WarningColor;
	}

	// Update progress bar
	UProgressBar* Bar = GetProgressBarForStat(StatName);
	if (Bar)
	{
		Bar->SetPercent(Percent);
		Bar->SetFillColorAndOpacity(BarColor);
	}

	// Update text
	UTextBlock* Text = GetTextBlockForStat(StatName);
	if (Text)
	{
		if (bShowPercentage)
		{
			Text->SetText(FText::Format(NSLOCTEXT("MOStatus", "Percent", "{0}%"), FText::AsNumber(FMath::RoundToInt(Percent * 100.0f))));
		}
		else
		{
			Text->SetText(FText::Format(NSLOCTEXT("MOStatus", "CurrentMax", "{0}/{1}"),
				FText::AsNumber(FMath::RoundToInt(Current)),
				FText::AsNumber(FMath::RoundToInt(Max))));
		}
		Text->SetColorAndOpacity(FSlateColor(BarColor));
	}
}

UProgressBar* UMOPlayerStatusWidget::GetProgressBarForStat(FName StatName) const
{
	if (StatName == FName("Health")) return HealthBar;
	if (StatName == FName("Stamina")) return StaminaBar;
	if (StatName == FName("Hunger")) return HungerBar;
	if (StatName == FName("Thirst")) return ThirstBar;
	if (StatName == FName("Energy")) return EnergyBar;
	if (StatName == FName("Temperature")) return TemperatureBar;
	return nullptr;
}

UTextBlock* UMOPlayerStatusWidget::GetTextBlockForStat(FName StatName) const
{
	if (StatName == FName("Health")) return HealthText;
	if (StatName == FName("Stamina")) return StaminaText;
	if (StatName == FName("Hunger")) return HungerText;
	if (StatName == FName("Thirst")) return ThirstText;
	if (StatName == FName("Energy")) return EnergyText;
	if (StatName == FName("Temperature")) return TemperatureText;
	return nullptr;
}
