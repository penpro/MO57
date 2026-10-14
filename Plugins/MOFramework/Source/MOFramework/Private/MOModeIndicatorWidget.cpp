#include "MOModeIndicatorWidget.h"
#include "MOUITokens.h"
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
	const UMOWorldSeedSubsystem* Seeds = Pawn ? UMOWorldSeedSubsystem::Get(Pawn) : nullptr;
	if (!Seeds || !Seeds->HasActiveSeed() || !FMOWorldGenParamsProvider::HasPublished())
	{
		return FText::GetEmpty();
	}
	const FMOWorldGenParamsRef P = FMOWorldGenParamsProvider::Get();
	const FVector L = Pawn->GetActorLocation();
	const int32 Seed = Seeds->GetActiveTerrainSeed();
	const FMOTerrainSample S = FMOWorldGen::SampleColumn(*P, L.X, L.Y, Seed, P->Tuning.VoxelSizeCm);
	FMOWorldGen::FBiomeWeight Weights[1];
	const int32 Biome = FMOWorldGen::SampleSurfaceBiomeWeights(*P, L.X, L.Y, Seed, S.HeightCm, S.Moisture, S.Temperature, Weights, 1) > 0 ? Weights[0].Biome : S.BiomeIndex;
	const FName Id = P->GetBiomeId(Biome);
	if (Id.IsNone())
	{
		return FText::GetEmpty();
	}
	const UMOBiomeDatabaseSettings* Settings = GetDefault<UMOBiomeDatabaseSettings>();
	const UDataTable* Table = Settings ? Settings->GetBiomeDefinitionsDataTable() : nullptr;
	const FMOBiomeDefinitionRow* Row = Table ? Table->FindRow<FMOBiomeDefinitionRow>(Id, TEXT("MOModeIndicator"), /*bWarnIfRowMissing=*/false) : nullptr;
	return Row && !Row->DisplayName.IsEmpty() ? Row->DisplayName : FText::FromString(FName::NameToDisplayString(Id.ToString(), false));
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
