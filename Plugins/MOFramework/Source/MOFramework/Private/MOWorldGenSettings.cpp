#include "MOWorldGenSettings.h"
#include "MOFramework.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOBiomeDefinitionRow.h"
#include "MOTerrainFunctionLibrary.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// A biome test world (MO.BiomeTest): every column is this biome. Read when the params are built, i.e. at the next world start.
	TAutoConsoleVariable<FString> CVarWorldGenForceBiome(TEXT("mo.WorldGen.ForceBiome"), TEXT(""),
		TEXT("Dev: a biome test world -- every column is this biome (DT_Biomes row id); empty = off. Takes effect at the next world start (MO.BiomeTest does both)."), ECVF_Default);

	FMOWorldShape GActiveWorldShape;
	FVector2D GActiveWorldOrigin = FVector2D::ZeroVector;
}

void UMOWorldGenSettings::SetActiveWorldShape(const FMOWorldShape& Shape)
{
	check(IsInGameThread());
	if (Shape != GActiveWorldShape)
	{
		FMOWorldCreationSettings Described;
		Described.Shape = Shape;
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldGen] world shape: %s"), *FMOWorldCreation::Describe(Described));
	}
	GActiveWorldShape = Shape;
}

const FMOWorldShape& UMOWorldGenSettings::GetActiveWorldShape()
{
	return GActiveWorldShape;
}

void UMOWorldGenSettings::SetActiveWorldOrigin(const FVector2D& Origin)
{
	check(IsInGameThread());
	if (Origin != GActiveWorldOrigin)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldGen] world origin: (%.0f, %.0f) cm in the generator's field"), Origin.X, Origin.Y);
	}
	GActiveWorldOrigin = Origin;
}

const FVector2D& UMOWorldGenSettings::GetActiveWorldOrigin()
{
	return GActiveWorldOrigin;
}

FMOWorldGenParamsRef UMOWorldGenSettings::BuildParams()
{
	check(IsInGameThread());

	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Params = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	if (const UMOWorldGenSettings* Settings = GetDefault<UMOWorldGenSettings>())
	{
		Params->Tuning = Settings->Tuning;
		Params->UplandLakeBedSurfaceType = Settings->UplandLakeBedSurfaceType;
	}
	FMOWorldCreation::ApplyShape(GActiveWorldShape, Params->Tuning);   // this world's biome size, height, mountains, lakes, caves, vegetation

	TArray<FName> BiomeIds;
	UMOBiomeDatabaseSettings::GetAllBiomeIds(BiomeIds);
	for (const FName& Id : BiomeIds)
	{
		const FMOBiomeDefinitionRow* Row = UMOBiomeDatabaseSettings::GetBiomeDefinition(Id);
		if (!Row)
		{
			continue;
		}
		FMOWorldGenBiome& B = Params->Biomes.AddDefaulted_GetRef();
		B.Id = Id;
		B.HeightMin = Row->HeightMin;
		B.HeightMax = Row->HeightMax;
		B.SlopeMinDeg = Row->SlopeMinDeg;
		B.SlopeMaxDeg = Row->SlopeMaxDeg;
		B.MoistureMin = Row->MoistureMin;
		B.MoistureMax = Row->MoistureMax;
		B.TemperatureMin = Row->TemperatureMin;
		B.TemperatureMax = Row->TemperatureMax;
		B.Priority = Row->Priority;
		B.EdgeBlendWidth = Row->EdgeBlendWidth;
		B.ReliefScale = Row->ReliefScale;
		B.MountainScale = Row->MountainScale;
		B.GroundSurfaceType = Row->GroundSurfaceType.ToSoftObjectPath();
		B.FootstepSet = Row->FootstepSet.IsNone() ? FName(TEXT("GrassTread")) : Row->FootstepSet;
	}

	const FString Forced = CVarWorldGenForceBiome.GetValueOnGameThread().TrimStartAndEnd();
	Params->ForcedBiomeId = Forced.IsEmpty() ? NAME_None : FName(*Forced);
	Params->Origin = GActiveWorldOrigin;   // before Finalize: it is part of the hash (when moved)
	Params->Finalize();
	if (!Params->ForcedBiomeId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldGen] BIOME TEST WORLD: every column is %s%s"), *Forced,
			Params->ForcedBiomeIndex == INDEX_NONE ? TEXT(" -- NOT a biome of DT_Biomes, ignored") : TEXT(""));
	}
	return FMOWorldGenParamsRef(Params);
}

FMOWorldGenParamsRef UMOWorldGenSettings::RefreshPublishedParams()
{
	const FMOWorldGenParamsRef Params = BuildParams();

	// Called from the PCG spawner once per partition and from every world start: do nothing (and stay quiet) when nothing changed.
	if (FMOWorldGenParamsProvider::HasPublished())
	{
		const FMOWorldGenParamsRef Current = FMOWorldGenParamsProvider::Get();
		if (Current->GetHash() == Params->GetHash())
		{
			return Current;
		}
	}

	FMOWorldGenParamsProvider::Publish(Params);
	UMOTerrainFunctionLibrary::RefreshSurfaceTable(*Params);
	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldGen] Published params: %d biomes, height range %.0f..%.0f cm, hash %08x, generator v%u"),
		Params->Biomes.Num(), Params->MinHeightCm, Params->MaxHeightCm, Params->GetHash(), Params->GeneratorVersion);
	return Params;
}
