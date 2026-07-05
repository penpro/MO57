#include "MOBiomeDatabaseSettings.h"
#include "MOFramework.h"
#include "Engine/DataTable.h"

TWeakObjectPtr<UDataTable> UMOBiomeDatabaseSettings::CachedDataTable;

UDataTable* UMOBiomeDatabaseSettings::GetBiomeDefinitionsDataTable() const
{
	if (CachedDataTable.IsValid())
	{
		return CachedDataTable.Get();
	}

	UDataTable* Table = BiomeDefinitionsDataTable.LoadSynchronous();
	if (Table)
	{
		CachedDataTable = Table;
	}
	else
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOBiomeDatabase] BiomeDefinitionsDataTable not configured or failed to load (%s)"),
			*BiomeDefinitionsDataTable.ToString());
	}
	return Table;
}

const FMOBiomeDefinitionRow* UMOBiomeDatabaseSettings::GetBiomeDefinition(FName BiomeId)
{
	const UMOBiomeDatabaseSettings* Settings = GetDefault<UMOBiomeDatabaseSettings>();
	UDataTable* Table = Settings ? Settings->GetBiomeDefinitionsDataTable() : nullptr;
	if (!Table || BiomeId.IsNone())
	{
		return nullptr;
	}
	return Table->FindRow<FMOBiomeDefinitionRow>(BiomeId, TEXT("GetBiomeDefinition"), /*bWarnIfRowMissing=*/false);
}

bool UMOBiomeDatabaseSettings::GetBiomeDefinitionBP(FName BiomeId, FMOBiomeDefinitionRow& OutDefinition)
{
	if (const FMOBiomeDefinitionRow* Row = GetBiomeDefinition(BiomeId))
	{
		OutDefinition = *Row;
		return true;
	}
	return false;
}

void UMOBiomeDatabaseSettings::GetAllBiomeIds(TArray<FName>& OutBiomeIds)
{
	OutBiomeIds.Reset();
	const UMOBiomeDatabaseSettings* Settings = GetDefault<UMOBiomeDatabaseSettings>();
	UDataTable* Table = Settings ? Settings->GetBiomeDefinitionsDataTable() : nullptr;
	if (Table)
	{
		OutBiomeIds = Table->GetRowNames();
	}
}

bool UMOBiomeDatabaseSettings::IsConfigured()
{
	const UMOBiomeDatabaseSettings* Settings = GetDefault<UMOBiomeDatabaseSettings>();
	return Settings && Settings->GetBiomeDefinitionsDataTable() != nullptr;
}

float UMOBiomeDatabaseSettings::ClimateNoise(const FVector& Location, float PeriodUU, int32 Seed)
{
	// Fold the seed into a domain offset — PerlinNoise2D has no seed param.
	const float OffsetX = (Seed % 8887) * 131.7f;
	const float OffsetY = ((Seed / 8887) % 8887) * 313.1f;
	const FVector2D Sample(Location.X / PeriodUU + OffsetX, Location.Y / PeriodUU + OffsetY);
	return FMath::Clamp(FMath::PerlinNoise2D(Sample) * 0.5f + 0.5f, 0.0f, 1.0f);
}

FName UMOBiomeDatabaseSettings::ResolveBiomeAt(FVector Location, float Height, float SlopeDeg,
	int32 Seed, float MoistureNoisePeriod, float TemperatureNoisePeriod)
{
	const float Moisture = ClimateNoise(Location, MoistureNoisePeriod, Seed);
	const float Temperature = ClimateNoise(Location, TemperatureNoisePeriod, Seed + 7919);

	TArray<FName> BiomeIds;
	GetAllBiomeIds(BiomeIds);

	FName Best = NAME_None;
	int32 BestPriority = INT32_MIN;
	for (const FName& Id : BiomeIds)
	{
		const FMOBiomeDefinitionRow* Row = GetBiomeDefinition(Id);
		if (Row && Row->Priority > BestPriority
			&& Row->Contains(Height, SlopeDeg, Moisture, Temperature))
		{
			Best = Id;
			BestPriority = Row->Priority;
		}
	}
	return Best;
}
