#include "MOStatusField.h"
#include "MOFramework.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"

void UMOStatusField::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize progress bar visibility based on whether it exists
	if (ValueBar)
	{
		// Default to hidden until SetFieldData is called with a valid normalized value
		ValueBar->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMOStatusField::SetFieldData(const FText& Title, const FText& Value, float NormalizedValue)
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}

	SetValue(Value, NormalizedValue);

	OnFieldDataChanged(Title, Value, NormalizedValue);
}

void UMOStatusField::SetValue(const FText& Value, float NormalizedValue)
{
	if (ValueText)
	{
		ValueText->SetText(Value);
	}

	// Update progress bar if we have a valid normalized value
	if (NormalizedValue >= 0.0f && NormalizedValue <= 1.0f)
	{
		if (ValueBar)
		{
			ValueBar->SetVisibility(ESlateVisibility::Visible);
			ValueBar->SetPercent(NormalizedValue);
		}

		// Auto-set color based on thresholds
		SetStatusFromNormalizedValue(NormalizedValue);
	}
}

void UMOStatusField::SetStatusColor(FLinearColor Color)
{
	if (ValueText)
	{
		ValueText->SetColorAndOpacity(FSlateColor(Color));
	}

	if (ValueBar)
	{
		ValueBar->SetFillColorAndOpacity(Color);
	}
}

void UMOStatusField::SetStatusFromNormalizedValue(float NormalizedValue)
{
	FLinearColor Color = HealthyColor;

	if (bInvertThresholds)
	{
		// Higher values are worse (e.g., infection severity)
		if (NormalizedValue >= (1.0f - CriticalThreshold))
		{
			Color = CriticalColor;
		}
		else if (NormalizedValue >= (1.0f - WarningThreshold))
		{
			Color = WarningColor;
		}
	}
	else
	{
		// Lower values are worse (e.g., health, hunger)
		if (NormalizedValue <= CriticalThreshold)
		{
			Color = CriticalColor;
		}
		else if (NormalizedValue <= WarningThreshold)
		{
			Color = WarningColor;
		}
	}

	SetStatusColor(Color);
}

void UMOStatusField::SetProgressBarVisible(bool bVisible)
{
	if (ValueBar)
	{
		ValueBar->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
