#include "MOModeIndicatorWidget.h"
#include "MOUITokens.h"
#include "MOUIUtils.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOBiomeDefinitionRow.h"
#include "MOWorldGen.h"
#include "MOWorldSeedSubsystem.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

void UMOModeIndicatorWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshBiome();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(BiomeTimer, FTimerDelegate::CreateUObject(this, &UMOModeIndicatorWidget::RefreshBiome), 1.0f, /*bLoop=*/true);
	}
	UpdateDisplay();
}

void UMOModeIndicatorWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BiomeTimer);
	}
	Super::NativeDestruct();
}

FText UMOModeIndicatorWidget::GetBiomeUnderfoot() const
{
	const APawn* Pawn = GetOwningPlayerPawn();
	return Pawn ? UMOUIUtils::GetBiomeDisplayNameAt(Pawn, Pawn->GetActorLocation()) : FText::GetEmpty();
}

void UMOModeIndicatorWidget::RefreshBiome()
{
	const FText Now = GetBiomeUnderfoot();
	if (!Now.EqualTo(BiomeName))
	{
		BiomeName = Now;
		UpdateDisplay();
	}
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
		return UMOUITokens::Color("color.text.primary");
	case EMOGameplayMode::Build:
		return UMOUITokens::Color("color.accent");
	case EMOGameplayMode::Terraform:
		return UMOUITokens::Color("color.severity.notice");
	case EMOGameplayMode::Combat:
		return UMOUITokens::Color("color.severity.critical");
	default:
		return UMOUITokens::Color("color.text.primary");
	}
}

void UMOModeIndicatorWidget::UpdateDisplay_Implementation()
{
	if (ModeText)
	{
		const FText Label = (CurrentMode == EMOGameplayMode::Explore && !BiomeName.IsEmpty())
			? FText::Format(NSLOCTEXT("MO", "ModeExploringBiome", "Exploring {0}"), BiomeName)
			: GetModeDisplayName(CurrentMode);
		ModeText->SetText(Label.ToUpper());   // kit label type: upper case
		ModeText->SetColorAndOpacity(FSlateColor(GetModeColor(CurrentMode)));
	}
}
