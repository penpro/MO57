/**
 * MOStatusMoodleWidget.cpp - One icon slot in the status effect strip
 */

#include "MOStatusMoodleWidget.h"
#include "MOFramework.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "MOUITokens.h"

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

	if (SeverityShapeImage)
	{
		SeverityShapeImage->SetBrushFromTexture(UMOUITokens::Icon(ShapeIconForSeverity(CurrentMoodle.Severity)), /*bMatchSize*/ false);
		SeverityShapeImage->SetColorAndOpacity(Tint);
	}
	if (CriticalBorder)
	{
		const bool bCritical = CurrentMoodle.Severity == EMOStatusMoodleSeverity::Critical;
		CriticalBorder->SetVisibility(bCritical ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bCritical)
		{
			CriticalBorder->SetRenderOpacity(1.0f);
		}
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

FName UMOStatusMoodleWidget::ColorTokenForSeverity(EMOStatusMoodleSeverity Severity)
{
	switch (Severity)
	{
	case EMOStatusMoodleSeverity::Warning:  return FName("color.severity.warning");
	case EMOStatusMoodleSeverity::Critical: return FName("color.severity.critical");
	case EMOStatusMoodleSeverity::Buff:     return FName("color.severity.ok");
	case EMOStatusMoodleSeverity::Info:
	default:                                return FName("color.severity.info");
	}
}

FName UMOStatusMoodleWidget::ShapeIconForSeverity(EMOStatusMoodleSeverity Severity)
{
	switch (Severity)
	{
	case EMOStatusMoodleSeverity::Warning:  return FName("ic_sev_warning");
	case EMOStatusMoodleSeverity::Critical: return FName("ic_sev_critical");
	case EMOStatusMoodleSeverity::Buff:     return FName("ic_sev_ok");
	case EMOStatusMoodleSeverity::Info:
	default:                                return FName("ic_sev_info");
	}
}

FLinearColor UMOStatusMoodleWidget::TintForSeverity(EMOStatusMoodleSeverity Severity) const
{
	return UMOUITokens::Color(ColorTokenForSeverity(Severity));
}

void UMOStatusMoodleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (CriticalBorder && CurrentMoodle.Severity == EMOStatusMoodleSeverity::Critical)
	{
		const float PeriodS = UMOUITokens::Seconds("motion.pulse.critical");
		if (PeriodS <= KINDA_SMALL_NUMBER)
		{
			CriticalBorder->SetRenderOpacity(1.0f);   // reduced motion: a static double border
			return;
		}
		PulseClock = FMath::Fmod(PulseClock + InDeltaTime, PeriodS);
		const float Phase = 0.5f + 0.5f * FMath::Cos(2.0f * PI * PulseClock / PeriodS);   // 1 -> 0 -> 1
		CriticalBorder->SetRenderOpacity(0.55f + 0.45f * Phase);
	}
}
