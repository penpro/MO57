#include "MOWorldGenSettings.h"
#include "MOFramework.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOBiomeDefinitionRow.h"
#include "MOTerrainFunctionLibrary.h"

FMOWorldGenParamsRef UMOWorldGenSettings::BuildParams()
{
	check(IsInGameThread());

	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Params = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	if (const UMOWorldGenSettings* Settings = GetDefault<UMOWorldGenSettings>())
	{
		Params->Tuning = Settings->Tuning;
	}

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
		B.GroundSurfaceType = Row->GroundSurfaceType.ToSoftObjectPath();
	}

	Params->Finalize();
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
