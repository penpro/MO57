#include "MOModeIndicatorWidget.h"
#include "Components/TextBlock.h"

void UMOModeIndicatorWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateDisplay();
}

void UMOModeIndicatorWidget::SetMode(EMOGameplayMode NewMode)
{
	if (CurrentMode == NewMode)
	{
		return;
	}

	EMOGameplayMode OldMode = CurrentMode;
	CurrentMode = NewMode;

	UpdateDisplay();
	OnModeChanged.Broadcast(OldMode, NewMode);
}

FText UMOModeIndicatorWidget::GetModeDisplayName(EMOGameplayMode Mode)
{
	switch (Mode)
	{
	case EMOGameplayMode::Explore:
		return NSLOCTEXT("MO", "ModeExplore", "Explore");
	case EMOGameplayMode::Build:
		return NSLOCTEXT("MO", "ModeBuild", "Build");
	case EMOGameplayMode::Terraform:
		return NSLOCTEXT("MO", "ModeTerraform", "Terraform");
	case EMOGameplayMode::Combat:
		return NSLOCTEXT("MO", "ModeCombat", "Combat");
	default:
		return NSLOCTEXT("MO", "ModeUnknown", "Unknown");
	}
}

FLinearColor UMOModeIndicatorWidget::GetModeColor(EMOGameplayMode Mode) const
{
	switch (Mode)
	{
	case EMOGameplayMode::Explore:
		return ExploreColor;
	case EMOGameplayMode::Build:
		return BuildColor;
	case EMOGameplayMode::Terraform:
		return TerraformColor;
	case EMOGameplayMode::Combat:
		return CombatColor;
	default:
		return FLinearColor::White;
	}
}

void UMOModeIndicatorWidget::UpdateDisplay_Implementation()
{
	if (ModeText)
	{
		ModeText->SetText(GetModeDisplayName(CurrentMode));
		ModeText->SetColorAndOpacity(FSlateColor(GetModeColor(CurrentMode)));
	}
}
