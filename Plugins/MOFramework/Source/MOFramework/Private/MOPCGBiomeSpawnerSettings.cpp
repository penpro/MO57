#include "MOPCGBiomeSpawnerSettings.h"
#include "MOFramework.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOTerrainModificationSubsystem.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGActorHelpers.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "MOPCGBiomeSpawner"

// ============================================================================
// SETTINGS
// ============================================================================

UMOPCGBiomeSpawnerSettings::UMOPCGBiomeSpawnerSettings()
{
#if WITH_EDITOR
	Category = LOCTEXT("Category", "MO");
#endif
}

FPCGElementPtr UMOPCGBiomeSpawnerSettings::CreateElement() const
{
	return MakeShared<FMOPCGBiomeSpawnerElement>();
}

#if WITH_EDITOR
FText UMOPCGBiomeSpawnerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Biome-driven scatter over voxel-surface points (DT_Biomes).\n\n"
		"Per point: resolve biome (height/slope + seeded moisture/temperature\n"
		"noise, highest Priority wins), pick a palette species by density,\n"
		"then spawn tagged HISM instances. All variety lives in DT_Biomes.");
}
#endif

TArray<FPCGPinProperties> UMOPCGBiomeSpawnerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point, true, true);
	return Properties;
}

TArray<FPCGPinProperties> UMOPCGBiomeSpawnerSettings::OutputPinProperties() const
{
	// Terminal spawner node.
	return {};
}

// ============================================================================
// ELEMENT
// ============================================================================

namespace
{
	/** Seeded 0..1 Perlin sample over world XY with the given period. */
	float SampleClimateNoise(const FVector& Location, float PeriodUU, int32 Seed)
	{
		// Fold the seed into a domain offset — PerlinNoise2D has no seed param.
		const float OffsetX = (Seed % 8887) * 131.7f;
		const float OffsetY = ((Seed / 8887) % 8887) * 313.1f;
		const FVector2D Sample(Location.X / PeriodUU + OffsetX, Location.Y / PeriodUU + OffsetY);
		return FMath::Clamp(FMath::PerlinNoise2D(Sample) * 0.5f + 0.5f, 0.0f, 1.0f);
	}

	/** Surface slope in degrees from the point's up vector (identity = flat). */
	float PointSlopeDeg(const FPCGPoint& Point)
	{
		const FVector Up = Point.Transform.GetRotation().GetUpVector();
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Up.Z, -1.0f, 1.0f)));
	}

	bool BiomeContains(const FMOBiomeDefinitionRow& Biome, float Height, float SlopeDeg, float Moisture, float Temperature)
	{
		return Height >= Biome.HeightMin && Height <= Biome.HeightMax
			&& SlopeDeg >= Biome.SlopeMinDeg && SlopeDeg <= Biome.SlopeMaxDeg
			&& Moisture >= Biome.MoistureMin && Moisture <= Biome.MoistureMax
			&& Temperature >= Biome.TemperatureMin && Temperature <= Biome.TemperatureMax;
	}
}

bool FMOPCGBiomeSpawnerElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGBiomeSpawnerElement::Execute);

	const UMOPCGBiomeSpawnerSettings* Settings = Context->GetInputSettings<UMOPCGBiomeSpawnerSettings>();
	check(Settings);

	AActor* TargetActor = Context->GetTargetActor(nullptr);
	if (!TargetActor)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBiomeSpawner] No target actor found"));
		return true;
	}

	// Load the biome catalog once per execution.
	TArray<FName> BiomeIds;
	UMOBiomeDatabaseSettings::GetAllBiomeIds(BiomeIds);
	if (BiomeIds.Num() == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBiomeSpawner] DT_Biomes empty or not configured"));
		return true;
	}
	struct FBiomeEntry
	{
		FName Id;
		const FMOBiomeDefinitionRow* Row = nullptr;
		TArray<UStaticMesh*> LoadedMeshes;   // parallel to Row->Species
	};
	TArray<FBiomeEntry> Biomes;
	for (const FName& Id : BiomeIds)
	{
		if (const FMOBiomeDefinitionRow* Row = UMOBiomeDatabaseSettings::GetBiomeDefinition(Id))
		{
			FBiomeEntry& E = Biomes.AddDefaulted_GetRef();
			E.Id = Id;
			E.Row = Row;
			E.LoadedMeshes.Reserve(Row->Species.Num());
			for (const FMOBiomeSpeciesEntry& Sp : Row->Species)
			{
				UStaticMesh* Loaded = Sp.Mesh.LoadSynchronous();   // main thread — ok
				if (!Loaded)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOBiomeSpawner] species mesh FAILED to load: %s (biome %s)"),
						*Sp.Mesh.ToString(), *Id.ToString());
				}
				E.LoadedMeshes.Add(Loaded);
			}
		}
	}
	// Highest priority first so the first band match wins.
	Biomes.Sort([](const FBiomeEntry& A, const FBiomeEntry& B) { return A.Row->Priority > B.Row->Priority; });

	UMOTerrainModificationSubsystem* TerrainMod = nullptr;
	if (Settings->bRespectTerrainModifications)
	{
		if (UWorld* World = TargetActor->GetWorld())
		{
			TerrainMod = World->GetSubsystem<UMOTerrainModificationSubsystem>();
		}
	}

	const int32 Seed = Context->GetSeed() + Settings->SeedOffset;
	FRandomStream RandomStream(Seed);

	// (mesh, biome) -> bucket of accepted transforms
	TMap<FString, FSpeciesBucket> Buckets;
	int32 TotalIn = 0, TotalAccepted = 0, TotalSuppressed = 0;

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGPointData* InputPointData = Cast<UPCGPointData>(Input.Data);
		if (!InputPointData)
		{
			continue;
		}
		for (const FPCGPoint& Point : InputPointData->GetPoints())
		{
			++TotalIn;
			const FVector Location = Point.Transform.GetLocation();

			if (TerrainMod && TerrainMod->IsLocationModified(Location))
			{
				++TotalSuppressed;
				continue;
			}

			const float Moisture = SampleClimateNoise(Location, Settings->MoistureNoisePeriod, Seed);
			const float Temperature = SampleClimateNoise(Location, Settings->TemperatureNoisePeriod, Seed + 7919);
			const float SlopeDeg = PointSlopeDeg(Point);

			// Highest-priority biome whose bands contain this sample.
			const FBiomeEntry* Chosen = nullptr;
			for (const FBiomeEntry& E : Biomes)
			{
				if (BiomeContains(*E.Row, Location.Z, SlopeDeg, Moisture, Temperature))
				{
					Chosen = &E;
					break;
				}
			}
			if (!Chosen)
			{
				continue;
			}

			// Density-scaled species pick: each point is one scatter slot;
			// expected instances/point for species s = Density_s / InputPPH,
			// cluster noise concentrates (x2 in clumps, 0 between).
			float Acceptance[64];
			float TotalAcceptance = 0.0f;
			const int32 NumSpecies = FMath::Min(Chosen->Row->Species.Num(), 64);
			for (int32 i = 0; i < NumSpecies; ++i)
			{
				const FMOBiomeSpeciesEntry& Sp = Chosen->Row->Species[i];
				float P = (Chosen->LoadedMeshes[i] ? Sp.DensityPerHectare / Settings->InputPointsPerHectare : 0.0f);
				if (P > 0.0f && Sp.ClusterRadius > 0.0f)
				{
					const float Clump = FMath::PerlinNoise2D(FVector2D(
						Location.X / FMath::Max(Sp.ClusterRadius * 4.0f, 1.0f) + i * 17.3f,
						Location.Y / FMath::Max(Sp.ClusterRadius * 4.0f, 1.0f) - i * 29.1f));
					P *= FMath::Clamp(Clump * 2.0f + 0.5f, 0.0f, 2.0f);
				}
				Acceptance[i] = P;
				TotalAcceptance += P;
			}
			if (TotalAcceptance <= 0.0f)
			{
				continue;
			}

			const float Roll = RandomStream.FRand();
			if (Roll >= FMath::Min(TotalAcceptance, 1.0f))
			{
				continue;   // slot stays empty — that's most points
			}
			// Pick which species filled the slot.
			float Pick = Roll * (TotalAcceptance / FMath::Min(TotalAcceptance, 1.0f));
			int32 Selected = 0;
			for (int32 i = 0; i < NumSpecies; ++i)
			{
				if (Pick < Acceptance[i]) { Selected = i; break; }
				Pick -= Acceptance[i];
				Selected = i;
			}
			const FMOBiomeSpeciesEntry& Sp = Chosen->Row->Species[Selected];
			UStaticMesh* Mesh = Chosen->LoadedMeshes[Selected];
			if (!Mesh)
			{
				continue;
			}

			FTransform Xf = Point.Transform;
			const float Scale = RandomStream.FRandRange(Sp.MinScale, Sp.MaxScale);
			Xf.SetScale3D(FVector(Scale));
			// Random yaw so scatter doesn't read as a grid.
			Xf.SetRotation(FQuat(FVector::UpVector, RandomStream.FRandRange(0.0f, 2.0f * PI)) * Xf.GetRotation());

			const FString Key = FString::Printf(TEXT("%s|%s"), *Mesh->GetPathName(), *Chosen->Id.ToString());
			FSpeciesBucket& Bucket = Buckets.FindOrAdd(Key);
			Bucket.Mesh = Mesh;
			Bucket.HISMTag = Sp.HISMTag;
			Bucket.BiomeId = Chosen->Id;
			Bucket.Transforms.Add(Xf);
			++TotalAccepted;
		}
	}

	// Create managed HISMs per (mesh, biome).
	UPCGComponent* SourceComponent = Cast<UPCGComponent>(Context->ExecutionSource.Get());
	if (!SourceComponent)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBiomeSpawner] No source PCG component"));
		return true;
	}

	int32 Components = 0;
	for (auto& Pair : Buckets)
	{
		FSpeciesBucket& Bucket = Pair.Value;
		if (!Bucket.Mesh || Bucket.Transforms.Num() == 0)
		{
			continue;
		}

		FPCGISMComponentBuilderParams Params;
		Params.Descriptor.StaticMesh = Bucket.Mesh;
		Params.Descriptor.ComponentClass = UHierarchicalInstancedStaticMeshComponent::StaticClass();
		Params.Descriptor.BodyInstance.SetCollisionProfileName(Settings->CollisionProfile.Name);
		Params.Descriptor.bCastShadow = Settings->bCastShadows;
		Params.Descriptor.bAffectDistanceFieldLighting = false;
		Params.Descriptor.bAffectDynamicIndirectLighting = false;
		Params.Descriptor.ComponentTags.Add(Bucket.HISMTag);
		Params.Descriptor.ComponentTags.Add(FName(*FString::Printf(TEXT("MOBiome_%s"), *Bucket.BiomeId.ToString())));
		Params.NumCustomDataFloats = 0;
		// LOAD-BEARING, two layers (verified UE5.8 sources + live dumps):
		// 1. FISMComponentDescriptor equality/hash EXCLUDES ComponentTags, so
		//    descriptor-equal components merge across chains sharing a mesh.
		// 2. SettingsCrc alone is NOT enough: nodes with an INVALID crc (the
		//    native Static Mesh Spawners in this graph) take the crc-less
		//    lookup path and reclaim ANY descriptor-equal managed resource —
		//    they wiped this node's BlackAlder components every generation.
		// RayTracingGroupId IS part of descriptor equality, so a unique
		// per-(biome,species) id makes the descriptor untouchable by other
		// chains; SettingsCrc additionally scopes reuse within this node.
		// (HWRT grouping impact: none today; revisit in P5 Lumen/HWRT tuning.)
		const uint32 BucketHash = (GetTypeHash(Bucket.BiomeId) ^ (GetTypeHash(Bucket.HISMTag) * 31) ^ 0x4D4F4249u) | 1u;
		Params.Descriptor.RayTracingGroupId = (int32)(BucketHash & 0x7fffffffu);
		Params.SettingsCrc = FPCGCrc(BucketHash);

		UInstancedStaticMeshComponent* ISM = UPCGActorHelpers::GetOrCreateISMC(
			TargetActor, SourceComponent, Params, Context);
		if (!ISM)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOBiomeSpawner] Failed HISM for %s"), *Bucket.Mesh->GetName());
			continue;
		}
		ISM->AddInstances(Bucket.Transforms, false, true);
		++Components;
	}

	// Per-bucket breakdown so density tuning reads from the log, not guesses.
	FString BucketSummary;
	for (const auto& Pair : Buckets)
	{
		BucketSummary += FString::Printf(TEXT(" %s/%s=%d"),
			*Pair.Value.BiomeId.ToString(),
			Pair.Value.Mesh ? *Pair.Value.Mesh->GetName() : TEXT("null"),
			Pair.Value.Transforms.Num());
	}
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOBiomeSpawner] in=%d accepted=%d suppressed=%d components=%d biomes=%d |%s"),
		TotalIn, TotalAccepted, TotalSuppressed, Components, Biomes.Num(), *BucketSummary);

	return true;
}

#undef LOCTEXT_NAMESPACE
