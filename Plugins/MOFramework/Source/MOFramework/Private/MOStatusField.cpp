#include "MOStatusField.h"
#include "MOFramework.h"
#include "MOUITokens.h"
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
	SetInfo(Info);   // a row nobody described shows no "i"
}

void UMOStatusField::SetInfo(const FText& InInfo)
{
	Info = InInfo;
	if (IconImage)
	{
		IconImage->SetToolTipText(Info);
		// Visible (hit-testable), so the pointer finds the "i" and its tooltip shows.
		IconImage->SetVisibility(Info.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
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
	// Authored colours, unless the row follows the design tokens (the default).
	const bool bTokens = bUseSeverityTokens && UMOUITokens::IsLoaded();
	const FLinearColor Healthy = bTokens ? UMOUITokens::Color("color.severity.ok") : HealthyColor;
	const FLinearColor Warning = bTokens ? UMOUITokens::Color("color.severity.warning") : WarningColor;
	const FLinearColor Critical = bTokens ? UMOUITokens::Color("color.severity.critical") : CriticalColor;

	FLinearColor Color = Healthy;

	if (bInvertThresholds)
	{
		
		// Higher values are worse (e.g., infection severity)
		if (NormalizedValue >= (1.0f - CriticalThreshold))
		{
			Color = Critical;
		}
		else if (NormalizedValue >= (1.0f - WarningThreshold))
		{
			Color = Warning;
		}
	}
	else
	{
		// Lower values are worse (e.g., health, hunger)
		if (NormalizedValue <= CriticalThreshold)
		{
			Color = Critical;
		}
		else if (NormalizedValue <= WarningThreshold)
		{
			Color = Warning;
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
