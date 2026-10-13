#include "MOPCGBiomeSpawnerSettings.h"
#include "MOFramework.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOWorldGen.h"
#include "MOWorldGenSettings.h"
#include "MOWorldSeedSubsystem.h"
#include "MOPCGWaterMask.h"
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
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"

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
	/** Live multiplier on the NEAR ground cover's density (grass, flowers, debris), on top of the authored per-hectare rates: the one knob for "thin out the grass". Takes effect on the next generation (pcg.RuntimeGeneration.Refresh). */
	TAutoConsoleVariable<float> CVarGroundCoverScale(TEXT("mo.PCG.GroundCoverScale"), 1.0f, TEXT("Multiplier on near ground-cover species density (0..2)."), ECVF_Default);

	/** Trees, rocks, bushes and shrubs are the LARGE species (placed on the larger hi-gen grid); grass, flowers and debris are ground cover. By HISMTag so a new species needs no flag. */
	bool IsLargeSpecies(const FMOBiomeSpeciesEntry& Sp)
	{
		static const FName Tags[] = { FName(TEXT("MOResource_Tree")), FName(TEXT("MOResource_Rock")), FName(TEXT("MOResource_Bush")), FName(TEXT("MOBiomeCover_Shrub")), FName(TEXT("MOBiomeCover_Rock")) };
		for (const FName& T : Tags)
		{
			if (Sp.HISMTag == T)
			{
				return true;
			}
		}
		return false;
	}

	/** Far-tier stand-ins (MOFar_Grass tufts, MOFar_Tree proxies) are tagged with the MOFar_ prefix; they belong to the far nodes only. */
	bool IsFarGroundCover(const FMOBiomeSpeciesEntry& Sp) { return Sp.HISMTag == FName(TEXT("MOFar_Grass")); }
	bool IsFarLargeTag(const FName& Tag) { return Tag == FName(TEXT("MOFar_Tree")); }
	bool IsFarLarge(const FMOBiomeSpeciesEntry& Sp) { return IsFarLargeTag(Sp.HISMTag); }
	/** The third far tier: solid crown proxies for the forest 3-9 km away (tag MOFar_Canopy). Like every far tag it belongs to its own node only. */
	bool IsFarCanopyTag(const FName& Tag) { return Tag == FName(TEXT("MOFar_Canopy")); }
	bool IsFarCanopy(const FMOBiomeSpeciesEntry& Sp) { return IsFarCanopyTag(Sp.HISMTag); }
	/** Saplings and seedlings carry the tree tag (they are harvestable) but stand 0.5-3 m tall: they belong with the shrubs on the large-species grid, not out on the tree grid. */
	bool IsYoungTree(const FMOBiomeSpeciesEntry& Sp)
	{
		const FString Path = Sp.Mesh.ToString();
		return Path.Contains(TEXT("Sapling")) || Path.Contains(TEXT("Seedling"));
	}

	/** Trees stand on a grid of their own (TreesOnly): a forest has to be seen from much farther than the undergrowth, rocks and shrubs around it. */
	bool IsTreeSpecies(const FMOBiomeSpeciesEntry& Sp) { return Sp.HISMTag == FName(TEXT("MOResource_Tree")) && !IsYoungTree(Sp); }

	/** A biome's MAIN species (Species): which tier plants them. */
	bool PassesSpeciesFilter(EMOBiomeSpeciesFilter Filter, const FMOBiomeSpeciesEntry& Sp)
	{
		if (Filter == EMOBiomeSpeciesFilter::WaterEdgeOnly)
		{
			return false;   // the shore tier plants only the biome rows' water-edge list
		}
		const bool bFarGround = IsFarGroundCover(Sp), bFarLarge = IsFarLarge(Sp), bFarCanopy = IsFarCanopy(Sp);
		const bool bAnyFar = bFarGround || bFarLarge || bFarCanopy;
		switch (Filter)
		{
		case EMOBiomeSpeciesFilter::All:                return true;
		case EMOBiomeSpeciesFilter::FarGroundCoverOnly: return bFarGround;
		case EMOBiomeSpeciesFilter::FarLargeOnly:       return bFarLarge;
		case EMOBiomeSpeciesFilter::FarCanopyOnly:      return bFarCanopy;
		case EMOBiomeSpeciesFilter::TreesOnly:          return IsTreeSpecies(Sp);
		case EMOBiomeSpeciesFilter::LargeOnly:          return !bAnyFar && !IsTreeSpecies(Sp) && IsLargeSpecies(Sp);
		default:                                        return !bAnyFar && !IsLargeSpecies(Sp);   // GroundCoverOnly
		}
	}

	/** A biome's WATER-EDGE species (WaterEdgeSpecies): only the dense shore tier (and the legacy All node) plants them. */
	bool PassesWaterEdgeFilter(EMOBiomeSpeciesFilter Filter)
	{
		return Filter == EMOBiomeSpeciesFilter::All || Filter == EMOBiomeSpeciesFilter::WaterEdgeOnly;
	}

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
		TArray<UStaticMesh*> LoadedMeshes;       // parallel to Row->Species
		TArray<UStaticMesh*> LoadedEdgeMeshes;   // parallel to Row->WaterEdgeSpecies
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
			for (const FMOBiomeSpeciesEntry& Sp : Row->WaterEdgeSpecies)
			{
				UStaticMesh* Loaded = Sp.Mesh.LoadSynchronous();
				if (!Loaded)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOBiomeSpawner] water-edge species mesh FAILED to load: %s (biome %s)"), *Sp.Mesh.ToString(), *Id.ToString());
				}
				E.LoadedEdgeMeshes.Add(Loaded);
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

	// Climate source. Shared generator (default): the SAME function and terrain seed the MO Terrain voxel node uses, so planting follows the
	// terrain's own climate/biome map. Refresh the snapshot first so DT_Biomes / tuning edits made since the last generation are seen.
	FMOWorldGenParamsRef GenParams = FMOWorldGenParamsProvider::MakeDefault();
	int32 TerrainSeed = Seed;
	if (Settings->bUseWorldGenerator)
	{
		GenParams = UMOWorldGenSettings::RefreshPublishedParams();
		const UMOWorldSeedSubsystem* SeedSubsystem = UMOWorldSeedSubsystem::Get(TargetActor);
		TerrainSeed = (SeedSubsystem && SeedSubsystem->HasActiveSeed()) ? SeedSubsystem->GetActiveTerrainSeed() : Seed;
	}
	// Water (the sea, rivers, ponds, upland lakes): the same mask the elevation-bands node uses, built from THIS node's params and terrain seed.
	const FMOPCGWaterMask WaterMask = Settings->bUseWorldGenerator ? FMOPCGWaterMask(GenParams, TerrainSeed) : FMOPCGWaterMask();
	// The same mask with the bank margin shrunk to the waterline: the WATER-EDGE species (reeds, rushes) stand right at the water, in the strip between the shore and the 1.05 margin that keeps every other species away.
	const FMOPCGWaterMask EdgeMask = Settings->bUseWorldGenerator ? FMOPCGWaterMask(GenParams, TerrainSeed, 1.008f) : FMOPCGWaterMask();
	// Moisture + temperature (temperature already includes the lapse rate for L.Z in generator mode).
	auto ClimateAt = [&](const FVector& L, float& OutMoisture, float& OutTemperature)
	{
		if (Settings->bUseWorldGenerator)
		{
			const FMOClimateSample C = FMOWorldGen::SampleClimate(*GenParams, L.X, L.Y, TerrainSeed);
			OutMoisture = C.Moisture;
			OutTemperature = FMOWorldGen::TemperatureAtHeight(*GenParams, C.TemperatureSeaLevel, (float)L.Z);
		}
		else
		{
			OutMoisture = UMOBiomeDatabaseSettings::ClimateNoise(L, Settings->MoistureNoisePeriod, Seed);
			OutTemperature = UMOBiomeDatabaseSettings::ClimateNoise(L, Settings->TemperatureNoisePeriod, Seed + 7919);
		}
	};

	// Far tiers keep out of the dense near zone: where the viewer is at generation time (the camera, so the main menu flyby counts too).
	FVector2D ViewerXY = FVector2D::ZeroVector;
	bool bHaveViewer = false;
	if (Settings->MinDistanceFromViewerCm > 0.0f && TargetActor && TargetActor->GetWorld())
	{
		if (const APlayerController* ViewerPC = TargetActor->GetWorld()->GetFirstPlayerController())
		{
			FVector ViewLocation;
			FRotator ViewRotation;
			ViewerPC->GetPlayerViewPoint(ViewLocation, ViewRotation);
			ViewerXY = FVector2D(ViewLocation.X, ViewLocation.Y);
			bHaveViewer = true;
		}
	}
	const float MinViewerDistSq = FMath::Square(Settings->MinDistanceFromViewerCm);

	const double PlantStartTime = FPlatformTime::Seconds();
	// (mesh, biome) -> bucket of accepted transforms
	TMap<FString, FSpeciesBucket> Buckets;
	int32 TotalIn = 0, TotalAccepted = 0, TotalSuppressed = 0;
	FBox InputBounds(ForceInit);   // diagnostics: where the sampler actually put this execution's points

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	// The shore tier is dense (about one point per square metre): skip a cell outright when no water can be near it (the mask samples a 10 m grid, the points would each cost a height sample).
	if (Settings->SpeciesFilter == EMOBiomeSpeciesFilter::WaterEdgeOnly && EdgeMask.IsActive())
	{
		FBox2D CellBox(ForceInit);
		for (const FPCGTaggedData& Input : Inputs)
		{
			if (const UPCGPointData* Data = Cast<UPCGPointData>(Input.Data))
			{
				for (const FPCGPoint& Point : Data->GetPoints())
				{
					CellBox += FVector2D(Point.Transform.GetLocation().X, Point.Transform.GetLocation().Y);
				}
			}
		}
		if (CellBox.bIsValid && !EdgeMask.MayHaveShore(CellBox.Min.X, CellBox.Min.Y, CellBox.Max.X, CellBox.Max.Y, 3000.0))
		{
			return true;
		}
	}
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
			InputBounds += Location;

			if (bHaveViewer && FVector2D::DistSquared(FVector2D(Location.X, Location.Y), ViewerXY) < MinViewerDistSq)
			{
				continue;   // inside the near zone: the dense tier owns it
			}

			if (TerrainMod && TerrainMod->IsLocationModified(Location))
			{
				++TotalSuppressed;
				continue;
			}

			// Caves: nothing is planted in a sinkhole's rim zone, and a point far below the ground height is a CAVE surface (wall/floor of a tunnel,
			// cavern or shaft), not a place for meadow grass or trees. Same generator, same terrain seed as the voxel stamp that carved the cave.
			bool bBankStrip = false;   // dry ground in the first metres past an upland lake's shore: only the water-edge species may stand here
			if (Settings->bUseWorldGenerator)
			{
				const float GroundZ = FMOWorldGen::SampleHeightCm(*GenParams, Location.X, Location.Y, TerrainSeed, GenParams->Tuning.VoxelSizeCm);
				// ... and nothing is planted under water: rivers, lakes and the sea are terrain below SeaLevelCm, and an upland lake's water is the ellipse of its bowl (FMOPCGWaterMask).
				if (Location.Z < GroundZ - 200.0f || EdgeMask.IsInWater(Location.X, Location.Y, Location.Z)
					|| FMOWorldGen::DistanceToSinkholeRimZone(*GenParams, Location.X, Location.Y, TerrainSeed) <= 0.0f)
				{
					++TotalSuppressed;
					continue;
				}
				bBankStrip = WaterMask.IsInWater(Location.X, Location.Y, Location.Z);
			}

			// Shared mask math (UMOBiomeDatabaseSettings) so tests/tools can
			// query the same field this spawner realizes.
			float Moisture = 0.0f, Temperature = 0.0f;
			ClimateAt(Location, Moisture, Temperature);
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
			// (the far tiers skip the feather: it costs four climate samples per point and moves a density ramp nobody can see from kilometres away)
			const bool bFarTierFilter = Settings->SpeciesFilter == EMOBiomeSpeciesFilter::FarCanopyOnly || Settings->SpeciesFilter == EMOBiomeSpeciesFilter::FarLargeOnly || Settings->SpeciesFilter == EMOBiomeSpeciesFilter::FarGroundCoverOnly;
			if (BlendW > 1.0f && !bFarTierFilter)
			{
				int32 Agree = 0;
				const float R = BlendW * 0.5f;
				static const FVector2D Taps[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
				for (const FVector2D& T : Taps)
				{
					const FVector Probe(Location.X + T.X * R, Location.Y + T.Y * R, Location.Z);
					float M2 = 0.0f, T2 = 0.0f;
					ClimateAt(Probe, M2, T2);
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

			// The candidates for this slot: the biome's species (density scaled by the climate edge feather), then -- on dry ground at the water's edge -- the biome's WATER-EDGE species (density scaled by how
			// close to the waterline the point is: FMOPCGWaterMask::ShoreFactor).
			struct FCandidate { const FMOBiomeSpeciesEntry* Species; UStaticMesh* Mesh; float Scale; };
			TArray<FCandidate, TInlineAllocator<96>> Candidates;
			const int32 NumSpecies = bBankStrip ? 0 : FMath::Min(Chosen->Row->Species.Num(), 64);
			for (int32 i = 0; i < NumSpecies; ++i)
			{
				if (PassesSpeciesFilter(Settings->SpeciesFilter, Chosen->Row->Species[i]))
				{
					Candidates.Add({ &Chosen->Row->Species[i], Chosen->LoadedMeshes[i], EdgeScale });
				}
			}
			if (Chosen->Row->WaterEdgeSpecies.Num() > 0 && EdgeMask.IsActive())
			{
				const float Shore = EdgeMask.ShoreFactor(Location.X, Location.Y, Location.Z, Chosen->Row->WaterEdgeBandCm);
				if (Shore > 0.0f)
				{
					const int32 NumEdge = FMath::Min(Chosen->Row->WaterEdgeSpecies.Num(), 32);
					for (int32 i = 0; i < NumEdge; ++i)
					{
						if (PassesWaterEdgeFilter(Settings->SpeciesFilter))
						{
							Candidates.Add({ &Chosen->Row->WaterEdgeSpecies[i], Chosen->LoadedEdgeMeshes[i], Shore });
						}
					}
				}
			}

			// Density-scaled species pick: each point is one scatter slot;
			// expected instances/point for species s = Density_s / InputPPH.
			// ClusterGroup >= 0: species in the same group sample ONE shared
			// clump field, spawning only where field >= their ClusterCore —
			// canopy trees at clump cores, undergrowth in the wider ring
			// around them (oasis/top-cap look). Acceptance is rescaled by the
			// surviving area fraction so authored per-hectare rates hold.
			TArray<float, TInlineAllocator<96>> Acceptance;
			Acceptance.SetNumZeroed(Candidates.Num());
			float TotalAcceptance = 0.0f;
			for (int32 i = 0; i < Candidates.Num(); ++i)
			{
				const FMOBiomeSpeciesEntry& Sp = *Candidates[i].Species;
				float P = (Candidates[i].Mesh ? Candidates[i].Scale * Sp.DensityPerHectare / Settings->InputPointsPerHectare : 0.0f);
				if (P > 0.0f && !IsLargeSpecies(Sp) && !IsFarGroundCover(Sp) && !IsFarLarge(Sp))
				{
					P *= FMath::Clamp(CVarGroundCoverScale.GetValueOnAnyThread(), 0.0f, 2.0f);
				}
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
			for (int32 i = 0; i < Candidates.Num(); ++i)
			{
				if (Pick < Acceptance[i]) { Selected = i; break; }
				Pick -= Acceptance[i];
				Selected = i;
			}
			const FMOBiomeSpeciesEntry& Sp = *Candidates[Selected].Species;
			UStaticMesh* Mesh = Candidates[Selected].Mesh;
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
				// Rocks/debris: sit ON the surface normal the sampler gave the point, spun about THEIR OWN up axis. (The old `FQuat(WorldUp, Yaw) * PointRotation` spun the already
				// aligned rock about WORLD up, which swings its up axis around the normal: on a slope every rock leaned in a random direction, up to twice the slope angle off the ground.)
				Xf.SetRotation(Xf.GetRotation() * FQuat(FVector::UpVector, Yaw));
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

			// Burial: aligned species (rocks, debris) are first lifted so the mesh BOTTOM (whatever its pivot) touches the surface, then sunk SinkFraction of their height into it.
			// Both along the instance's own up axis, so a rock on a slope goes into the slope, not into the air below it.
			{
				const FBoxSphereBounds MeshBounds = Mesh->GetBounds();
				const float HeightCm = MeshBounds.BoxExtent.Z * 2.0f * Scale;
				const float SinkFraction = Sp.SinkFraction >= 0.0f ? Sp.SinkFraction : (Sp.bAlignToSurfaceNormal ? 0.25f : 0.0f);
				float Offset = -(SinkFraction * HeightCm + Sp.SinkMinCm);
				if (Sp.bAlignToSurfaceNormal)
				{
					Offset -= (MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z) * Scale;   // pivot to bottom: negative bottom (a centred pivot) lifts the mesh onto the surface
				}
				if (!FMath::IsNearlyZero(Offset))
				{
					Xf.AddToTranslation(Xf.GetRotation().GetUpVector() * Offset);
				}
			}

			const FString Key = FString::Printf(TEXT("%s|%s"), *Mesh->GetPathName(), *Chosen->Id.ToString());
			FSpeciesBucket& Bucket = Buckets.FindOrAdd(Key);
			Bucket.Mesh = Mesh;
			Bucket.HISMTag = Sp.HISMTag;
			Bucket.BiomeId = Chosen->Id;
			Bucket.ResourceNodeId = Sp.ResourceNodeId;
			Bucket.bAutoSweep = Sp.bAutoSweepOnTerraform;
			Bucket.Collision = Sp.Collision;
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
		Params.Descriptor.BodyInstance.SetCollisionProfileName(Settings->DecorativeCollisionProfile.Name);   // overridden below for harvestable species
		Params.Descriptor.bCastShadow = Settings->bCastShadows;
		Params.Descriptor.bAffectDistanceFieldLighting = false;
		Params.Descriptor.bAffectDynamicIndirectLighting = false;

		// What is worth rendering depends on the tier. The FAR tiers (stand-in trees out to 4 km, far grass tufts) are seen as silhouettes: no shadow of their own (their shadows would be the
		// biggest line in the shadow pass for nothing visible), no ray-tracing / reflection presence, no wind, and never drawn inside the distance the NEAR tiers own: InstanceMinDrawDistance is
		// the engine's per-instance minimum, so a far cell that becomes near leaves no oversized ghost trees beside the real ones whatever the generation-time viewer position was.
		// GROUND COVER (grass, flowers, debris) casts no shadow and stays out of the ray-traced scene either: millions of blades add cost to those passes and no visible shading.
		const bool bFarTier = IsFarLargeTag(Bucket.HISMTag) || IsFarCanopyTag(Bucket.HISMTag) || Bucket.HISMTag == FName(TEXT("MOFar_Grass"));
		const bool bGroundCover = Bucket.HISMTag == FName(TEXT("MOBiomeCover_Grass")) || Bucket.HISMTag == FName(TEXT("MOBiomeCover_Flower")) || Bucket.HISMTag == FName(TEXT("MOBiomeCover_Debris"));
		if (bFarTier || bGroundCover)
		{
			Params.Descriptor.bCastShadow = false;
			Params.Descriptor.bVisibleInRayTracing = false;
			Params.Descriptor.bVisibleInReflections = false;
		}
		if (bFarTier)
		{
			Params.Descriptor.bEvaluateWorldPositionOffset = false;
			Params.Descriptor.bUseAsOccluder = false;
			Params.Descriptor.InstanceMinDrawDistance = FMath::RoundToInt(Settings->MinDistanceFromViewerCm);
		}

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
			Params.Descriptor.BodyInstance.SetCollisionProfileName(ResourceDef->GetPlacementCollisionProfile());
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
		// The species' own say wins over the resource type's default (see EMOSpeciesCollision): saplings are harvestable trees you can still walk through.
		if (Bucket.Collision == EMOSpeciesCollision::Passable)
		{
			Params.Descriptor.BodyInstance.SetCollisionProfileName(TEXT("OverlapAllDynamic"));
		}
		else if (Bucket.Collision == EMOSpeciesCollision::Solid)
		{
			Params.Descriptor.BodyInstance.SetCollisionProfileName(TEXT("BlockAll"));
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
		TEXT("[MOBiomeSpawner] in=%d accepted=%d suppressed=%d components=%d biomes=%d ms=%.0f inBounds=%s |%s"),
		TotalIn, TotalAccepted, TotalSuppressed, Components, Biomes.Num(), (FPlatformTime::Seconds() - PlantStartTime) * 1000.0,
		InputBounds.IsValid ? *FString::Printf(TEXT("(%.0f,%.0f)-(%.0f,%.0f)"), InputBounds.Min.X, InputBounds.Min.Y, InputBounds.Max.X, InputBounds.Max.Y) : TEXT("none"),
		*BucketSummary);

	return true;
}

#undef LOCTEXT_NAMESPACE
