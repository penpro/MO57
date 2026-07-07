#include "MOPCGBiomeSpawnerSettings.h"
#include "MOFramework.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOResourceNodeDefinitionRow.h"
#include "MOPCGInteractionSubsystem.h"
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
	/** Surface slope in degrees from the point's up vector (identity = flat). */
	float PointSlopeDeg(const FPCGPoint& Point)
	{
		const FVector Up = Point.Transform.GetRotation().GetUpVector();
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Up.Z, -1.0f, 1.0f)));
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

			// Shared mask math (UMOBiomeDatabaseSettings) so tests/tools can
			// query the same field this spawner realizes.
			const float Moisture = UMOBiomeDatabaseSettings::ClimateNoise(Location, Settings->MoistureNoisePeriod, Seed);
			const float Temperature = UMOBiomeDatabaseSettings::ClimateNoise(Location, Settings->TemperatureNoisePeriod, Seed + 7919);
			const float SlopeDeg = PointSlopeDeg(Point);

			// Highest-priority biome whose bands contain this sample.
			const FBiomeEntry* Chosen = nullptr;
			for (const FBiomeEntry& E : Biomes)
			{
				if (E.Row->Contains(Location.Z, SlopeDeg, Moisture, Temperature))
				{
					Chosen = &E;
					break;
				}
			}
			if (!Chosen)
			{
				continue;
			}

			// EDGE BLEND (P4 round 2): feather density near biome boundaries
			// instead of hard cuts. Four taps at half the blend width — the
			// fraction that agree with this point's biome scales acceptance,
			// so density ramps across EdgeBlendWidth rather than stepping.
			float EdgeScale = 1.0f;
			const float BlendW = Chosen->Row->EdgeBlendWidth;
			if (BlendW > 1.0f)
			{
				int32 Agree = 0;
				const float R = BlendW * 0.5f;
				static const FVector2D Taps[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
				for (const FVector2D& T : Taps)
				{
					const FVector Probe(Location.X + T.X * R, Location.Y + T.Y * R, Location.Z);
					const float M2 = UMOBiomeDatabaseSettings::ClimateNoise(Probe, Settings->MoistureNoisePeriod, Seed);
					const float T2 = UMOBiomeDatabaseSettings::ClimateNoise(Probe, Settings->TemperatureNoisePeriod, Seed + 7919);
					const FBiomeEntry* NeighborBiome = nullptr;
					for (const FBiomeEntry& E : Biomes)
					{
						// Same Z/slope as the center: the blend is a CLIMATE
						// feather; elevation/slope band edges stay crisp (a
						// cliff line should not dither).
						if (E.Row->Contains(Location.Z, SlopeDeg, M2, T2))
						{
							NeighborBiome = &E;
							break;
						}
					}
					if (NeighborBiome == Chosen)
					{
						++Agree;
					}
				}
				// interior = 1.0; deep edge (0-1 agreeing) thins toward 0.25
				EdgeScale = FMath::Lerp(0.25f, 1.0f, Agree / 4.0f);
			}

			// Density-scaled species pick: each point is one scatter slot;
			// expected instances/point for species s = Density_s / InputPPH.
			// ClusterGroup >= 0: species in the same group sample ONE shared
			// clump field, spawning only where field >= their ClusterCore —
			// canopy trees at clump cores, undergrowth in the wider ring
			// around them (oasis/top-cap look). Acceptance is rescaled by the
			// surviving area fraction so authored per-hectare rates hold.
			float Acceptance[64];
			float TotalAcceptance = 0.0f;
			const int32 NumSpecies = FMath::Min(Chosen->Row->Species.Num(), 64);
			for (int32 i = 0; i < NumSpecies; ++i)
			{
				const FMOBiomeSpeciesEntry& Sp = Chosen->Row->Species[i];
				float P = (Chosen->LoadedMeshes[i] ? EdgeScale * Sp.DensityPerHectare / Settings->InputPointsPerHectare : 0.0f);
				if (P > 0.0f && Sp.ClusterGroup >= 0)
				{
					// Shared field per (biome, group): offsets depend on the
					// GROUP, not the species, so group members co-locate.
					const float Period = FMath::Max(Sp.ClusterRadius * 4.0f, 1000.0f);
					const float Clump01 = FMath::Clamp(FMath::PerlinNoise2D(FVector2D(
						Location.X / Period + Sp.ClusterGroup * 53.7f,
						Location.Y / Period - Sp.ClusterGroup * 71.3f)) * 0.5f + 0.5f, 0.0f, 1.0f);
					if (Clump01 < Sp.ClusterCore)
					{
						P = 0.0f;
					}
					else
					{
						// Compensate for the culled area (clamped: the clump
						// cores shouldn't exceed 4x local concentration).
						P *= FMath::Min(1.0f / FMath::Max(1.0f - Sp.ClusterCore, 0.25f), 4.0f);
					}
				}
				else if (P > 0.0f && Sp.ClusterRadius > 0.0f)
				{
					// Ungrouped legacy clustering: per-species clump noise.
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
			const float Yaw = RandomStream.FRandRange(0.0f, 2.0f * PI);
			if (Sp.bAlignToSurfaceNormal)
			{
				// Rocks/debris: sit on the surface as sampled, random yaw.
				Xf.SetRotation(FQuat(FVector::UpVector, Yaw) * Xf.GetRotation());
			}
			else
			{
				// Trees & upright plants: grow toward WORLD UP with only a
				// small random tilt. Inheriting the surface normal makes
				// slope trees lean wonky — real trunks grow against gravity.
				const float TiltRad = FMath::DegreesToRadians(RandomStream.FRandRange(0.0f, Sp.MaxRandomTiltDeg));
				const float TiltDir = RandomStream.FRandRange(0.0f, 2.0f * PI);
				const FQuat Tilt(FVector(FMath::Cos(TiltDir), FMath::Sin(TiltDir), 0.0f), TiltRad);
				Xf.SetRotation(Tilt * FQuat(FVector::UpVector, Yaw));
			}

			const FString Key = FString::Printf(TEXT("%s|%s"), *Mesh->GetPathName(), *Chosen->Id.ToString());
			FSpeciesBucket& Bucket = Buckets.FindOrAdd(Key);
			Bucket.Mesh = Mesh;
			Bucket.HISMTag = Sp.HISMTag;
			Bucket.BiomeId = Chosen->Id;
			Bucket.ResourceNodeId = Sp.ResourceNodeId;
			Bucket.bAutoSweep = Sp.bAutoSweepOnTerraform;
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

		// HARVESTABLE species: apply the exact tag bundle the native resource
		// spawner derives from DT_ResourceNodes (Name/MOResource_/Action_/
		// Gives_/RequiresTool_/KeepOnHarvest + ResourceNode_<Id>) and register
		// the interaction mapping. Biome trees/rocks are REAL interaction
		// targets, not scenery. Decorative species fall back to HISMTag.
		const FMOResourceNodeDefinitionRow* ResourceDef = nullptr;
		if (!Bucket.ResourceNodeId.IsNone() && Settings->ResourceNodeDataTable)
		{
			ResourceDef = Settings->ResourceNodeDataTable->FindRow<FMOResourceNodeDefinitionRow>(
				Bucket.ResourceNodeId, TEXT("MOBiomeSpawner"), /*bWarnIfRowMissing=*/false);
			if (!ResourceDef)
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MOBiomeSpawner] ResourceNodeId '%s' not in ResourceNodeDataTable - species stays decorative"),
					*Bucket.ResourceNodeId.ToString());
			}
		}
		if (ResourceDef)
		{
			for (const FName& Tag : ResourceDef->GetAllTags())
			{
				Params.Descriptor.ComponentTags.AddUnique(Tag);
			}
			Params.Descriptor.ComponentTags.AddUnique(
				FName(*FString::Printf(TEXT("ResourceNode_%s"), *Bucket.ResourceNodeId.ToString())));
			if (Settings->bRegisterWithSubsystem)
			{
				if (UWorld* World = TargetActor->GetWorld())
				{
					if (UMOPCGInteractionSubsystem* PCGSubsystem = World->GetSubsystem<UMOPCGInteractionSubsystem>())
					{
						PCGSubsystem->RegisterTagItemMapping(
							FName(*FString::Printf(TEXT("MOResource_%s"), *Bucket.ResourceNodeId.ToString())),
							Bucket.ResourceNodeId);
					}
				}
			}
		}
		else if (!Bucket.HISMTag.IsNone())
		{
			Params.Descriptor.ComponentTags.Add(Bucket.HISMTag);
		}
		Params.Descriptor.ComponentTags.Add(FName(*FString::Printf(TEXT("MOBiome_%s"), *Bucket.BiomeId.ToString())));
		if (Bucket.bAutoSweep)
		{
			// Opt decorative ground cover into the terrain-mod subsystem's
			// terraform sweep (its AutoSweepTags allowlist — "grass").
			// Harvestable species stay un-tagged and persist as interaction
			// targets when nearby ground is dug.
			Params.Descriptor.ComponentTags.Add(FName(TEXT("grass")));
		}
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
