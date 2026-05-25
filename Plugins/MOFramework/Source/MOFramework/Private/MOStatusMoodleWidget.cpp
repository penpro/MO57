/**
 * MOStatusMoodleWidget.cpp - One icon slot in the status effect strip
 */

#include "MOStatusMoodleWidget.h"
#include "MOFramework.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UMOStatusMoodleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyMoodleToWidgets();
}

void UMOStatusMoodleWidget::SetMoodle(const FMOStatusMoodle& InMoodle)
{
	CurrentMoodle = InMoodle;
	ApplyMoodleToWidgets();
}

void UMOStatusMoodleWidget::ApplyMoodleToWidgets()
{
	const FLinearColor Tint = TintForSeverity(CurrentMoodle.Severity);

	if (IconImage)
	{
		if (CurrentMoodle.Icon)
		{
			IconImage->SetBrushFromTexture(CurrentMoodle.Icon, /*bMatchSize*/ false);
		}
		IconImage->SetColorAndOpacity(Tint);
	}

	if (LabelText)
	{
		LabelText->SetText(CurrentMoodle.Label);
		// Collapse the label widget when there's no text rather than render an
		// empty slot — keeps the strip tight when most moodles are icon-only.
		LabelText->SetVisibility(CurrentMoodle.Label.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (StackText)
	{
		if (CurrentMoodle.Stack > 0)
		{
			StackText->SetText(FText::Format(NSLOCTEXT("MO", "MoodleStackFormat", "x{0}"),
				FText::AsNumber(CurrentMoodle.Stack)));
			StackText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			StackText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Tooltip — UE's UserWidget supports SetToolTipText directly.
	if (!CurrentMoodle.Tooltip.IsEmpty())
	{
		SetToolTipText(CurrentMoodle.Tooltip);
	}

	// Fire the BP hook only on actual severity transitions so it doesn't
	// re-run every tick when SetMoodle is called to refresh stack/label.
	if (CurrentMoodle.Severity != LastAppliedSeverity)
	{
		LastAppliedSeverity = CurrentMoodle.Severity;
		OnSeverityChanged(CurrentMoodle.Severity);
	}
}

FLinearColor UMOStatusMoodleWidget::TintForSeverity(EMOStatusMoodleSeverity Severity) const
{
	switch (Severity)
	{
	case EMOStatusMoodleSeverity::Warning:  return WarningTint;
	case EMOStatusMoodleSeverity::Critical: return CriticalTint;
	case EMOStatusMoodleSeverity::Buff:     return BuffTint;
	case EMOStatusMoodleSeverity::Info:
	default:                                return InfoTint;
	}
}
