#include "MOPCGBiomeSpawnerSettings.h"
#include "MOSpeciesPlacement.h"
#include "MOFramework.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOWorldGen.h"
#include "MOWorldGenSettings.h"
#include "MOWorldSeedSubsystem.h"
#include "MOPCGWaterMask.h"
#include "MOWorldFeatureSubsystem.h"
#include "MOResourceNodeDefinitionRow.h"
#include "MOPCGInteractionSubsystem.h"
#include "MOTerrainModificationSubsystem.h"
#include "MOWorldRemovalSubsystem.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGActorHelpers.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "Engine/CollisionProfile.h"
#include "ISMPartition/ISMComponentDescriptor.h"
#include "Async/ParallelFor.h"
#include "Helpers/PCGHelpers.h"
#include <atomic>

#define LOCTEXT_NAMESPACE "MOPCGBiomeSpawner"

// ============================================================================
// SETTINGS
// ============================================================================

namespace MOPCGClump
{
	/** The clump field's statistics, measured once from the engine's own PerlinNoise2D (thread-safe static init: the spawner's workers share it). */
	struct FStats
	{
		float Coverage[101];        // Coverage[i] = fraction of the ground where the field >= i / 100
		float LegacyMean = 0.55f;   // mean of clamp(2 * noise + 0.5, 0, 2)

		FStats()
		{
			// Random points (fixed seed) over one 256 x 256 period of the noise's permutation table: a regular grid would sample only a few fixed
			// positions inside each lattice cell and bias the result.
			constexpr int32 Samples = 1 << 18;
			FRandomStream Stream(0x5EED);
			int32 Hist[101] = { 0 };
			double LegacySum = 0.0;
			for (int32 I = 0; I < Samples; ++I)
			{
				const float Noise = FMath::PerlinNoise2D(FVector2D(Stream.FRandRange(0.0f, 256.0f), Stream.FRandRange(0.0f, 256.0f)));
				++Hist[FMath::Clamp(FMath::FloorToInt(FMath::Clamp(Noise * 0.5f + 0.5f, 0.0f, 1.0f) * 100.0f), 0, 100)];
				LegacySum += FMath::Clamp(Noise * 2.0f + 0.5f, 0.0f, 2.0f);
			}
			int32 Above = 0;
			for (int32 I = 100; I >= 0; --I)
			{
				Above += Hist[I];
				Coverage[I] = float(Above) / float(Samples);
			}
			LegacyMean = float(LegacySum / double(Samples));
		}
	};

	const FStats& Stats()
	{
		static const FStats Measured;
		return Measured;
	}
}

float UMOPCGBiomeSpawnerSettings::ClumpField01(const FVector& Location, int32 ClusterGroup, float ClusterRadius)
{
	const float Period = FMath::Max(ClusterRadius * 4.0f, 1000.0f);
	return FMath::Clamp(FMath::PerlinNoise2D(FVector2D(Location.X / Period + ClusterGroup * 53.7f, Location.Y / Period - ClusterGroup * 71.3f)) * 0.5f + 0.5f, 0.0f, 1.0f);
}

float UMOPCGBiomeSpawnerSettings::ClumpCoverage(float Core)
{
	const float X = FMath::Clamp(Core, 0.0f, 1.0f) * 100.0f;
	const int32 Lo = FMath::Min(FMath::FloorToInt(X), 99);
	const MOPCGClump::FStats& S = MOPCGClump::Stats();
	return FMath::Lerp(S.Coverage[Lo], S.Coverage[Lo + 1], X - Lo);
}

float UMOPCGBiomeSpawnerSettings::ClumpCompensation(float Core)
{
	return FMath::Min(1.0f / FMath::Max(ClumpCoverage(Core), KINDA_SMALL_NUMBER), MaxClumpConcentration);
}

float UMOPCGBiomeSpawnerSettings::LegacyClumpMean()
{
	return MOPCGClump::Stats().LegacyMean;
}

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
	/** 1 (default) = the per-point work of a biome spawner execution runs on the task graph (ParallelFor); 0 = on the game thread alone, for A/B timing ([MOBiomeSpawner] ... ms=). */
	TAutoConsoleVariable<int32> CVarBiomeSpawnerParallel(TEXT("mo.PCG.BiomeSpawnerParallel"), 1, TEXT("1 = biome spawner per-point work in parallel (ParallelFor), 0 = single-threaded (A/B timing)."), ECVF_Default);
	/** 1 = add the per-stage CPU breakdown (generator / climate / edge blend / rest) to each [MOBiomeSpawner] line. */
	TAutoConsoleVariable<int32> CVarBiomeSpawnerProfile(TEXT("mo.PCG.BiomeSpawnerProfile"), 0, TEXT("1 = per-stage CPU breakdown in the [MOBiomeSpawner] log line."), ECVF_Default);
	/** 1 (default) = generator mode picks each point's biome by FMOWorldGen::PickSurfaceBiome (foliage thins out across the soft band the ground fades over); 0 = the old hard first match + 4-tap climate feather (A/B). */
	TAutoConsoleVariable<int32> CVarBiomeSoftPick(TEXT("mo.PCG.BiomeSoftPick"), 1, TEXT("1 = per-point biome pick across soft biome boundaries (matches the ground blend), 0 = hard first-match + climate feather."), ECVF_Default);

	/** Live multiplier on the NEAR ground cover's density (grass, flowers, debris), on top of the authored per-hectare rates: the one knob for "thin out the grass". Takes effect on the next generation (pcg.RuntimeGeneration.Refresh). */
	TAutoConsoleVariable<float> CVarGroundCoverScale(TEXT("mo.PCG.GroundCoverScale"), 1.0f, TEXT("Multiplier on near ground-cover species density (0..2)."), ECVF_Default);
	// Perf / look experiments ("how dense can a forest be and what does it cost"): scales EVERY species' density before the per-point pick. Points are scatter
	// slots (one instance each at most), so past the slot budget it saturates instead of growing. Needs pcg.RuntimeGeneration.Refresh (or a new world) to apply.
	TAutoConsoleVariable<float> CVarSpeciesDensityScale(TEXT("mo.PCG.SpeciesDensityScale"), 1.0f, TEXT("Dev: multiplier on every biome species' density (0..4); refresh PCG to apply."), ECVF_Default);
	TAutoConsoleVariable<float> CVarWidthScaleMultiplier(TEXT("mo.PCG.WidthScaleMultiplier"), 1.0f,
		TEXT("Dev: x every species' WidthScale (A/B a widened species: 0.4167 puts the rainforest trees back at their mesh's width); refresh PCG to apply."), ECVF_Default);
	TAutoConsoleVariable<int32> CVarLogCells(TEXT("mo.PCG.LogCells"), 0, TEXT("1 = one [MOBiomeSpawner] line per generated cell (in / accepted / suppressed / timings per bucket): density tuning."), ECVF_Default);

	// Wind (WPO) on shadow-casting foliage re-renders its virtual shadow map pages EVERY frame: in the dense Rainforest that was 7 of the 19 GPU ms, and skipping
	// those invalidations took the view from 20.1 to 14.3 ms (playtest 2026-10-14 #5, "frame rate dropped by half in this area"). Rigid: the trees still sway,
	// their cached shadows do not follow the sway. 0 = Auto (animated shadows, the full cost). Applies to the components already in the world at once.
	EShadowCacheInvalidationBehavior FoliageShadowCacheBehavior(int32 Value) { return Value != 0 ? EShadowCacheInvalidationBehavior::Rigid : EShadowCacheInvalidationBehavior::Auto; }
	TAutoConsoleVariable<int32> CVarFoliageRigidShadows(
		TEXT("mo.Foliage.RigidShadows"), 1,
		TEXT("1 = biome-scatter shadow casters keep their cached virtual shadows while the wind sways them (Rigid; ~-30 % GPU in dense forest), 0 = shadows re-render with the wind (Auto)."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Var)
		{
			const EShadowCacheInvalidationBehavior Behavior = FoliageShadowCacheBehavior(Var->GetInt());
			int32 Changed = 0;
			for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
			{
				UInstancedStaticMeshComponent* Hism = *It;   // PCG builds plain ISM components from the HISM descriptor
				if (!IsValid(Hism) || !Hism->GetWorld() || !Hism->CastShadow || !Hism->ComponentTags.ContainsByPredicate([](const FName& T) { return T.ToString().StartsWith(TEXT("MOBiome_")); }))
				{
					continue;
				}
				if (Hism->ShadowCacheInvalidationBehavior != Behavior)
				{
					Hism->ShadowCacheInvalidationBehavior = Behavior;
					Hism->MarkRenderStateDirty();
					++Changed;
				}
			}
			UE_LOG(LogMOFramework, Log, TEXT("[MOBiomeSpawner] mo.Foliage.RigidShadows=%d: %d biome-scatter components updated"), Var->GetInt(), Changed);
		}),
		ECVF_Default);

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

	// The tags as constants: FName(TEXT("...")) is a lookup in the global name table, and these helpers once ran for every species of every point -- under the parallel per-point loop
	// the threads queued on the name table's lock (161 us of CPU a point, Oct 13 2026). The per-point loop now reads a per-execution classification (FBiomeEntry::FilteredSpecies).
	const FName NAME_MOFarGrass(TEXT("MOFar_Grass"));
	const FName NAME_MOFarTree(TEXT("MOFar_Tree"));
	const FName NAME_MOFarCanopy(TEXT("MOFar_Canopy"));
	const FName NAME_MOResourceTree(TEXT("MOResource_Tree"));

	/** Far-tier stand-ins (MOFar_Grass tufts, MOFar_Tree proxies) are tagged with the MOFar_ prefix; they belong to the far nodes only. */
	bool IsFarGroundCover(const FMOBiomeSpeciesEntry& Sp) { return Sp.HISMTag == NAME_MOFarGrass; }
	bool IsFarLargeTag(const FName& Tag) { return Tag == NAME_MOFarTree; }
	bool IsFarLarge(const FMOBiomeSpeciesEntry& Sp) { return IsFarLargeTag(Sp.HISMTag); }
	/** The third far tier: solid crown proxies for the forest 3-9 km away (tag MOFar_Canopy). Like every far tag it belongs to its own node only. */
	bool IsFarCanopyTag(const FName& Tag) { return Tag == NAME_MOFarCanopy; }
	bool IsFarCanopy(const FMOBiomeSpeciesEntry& Sp) { return IsFarCanopyTag(Sp.HISMTag); }
	/** Saplings and seedlings carry the tree tag (they are harvestable) but stand 0.5-3 m tall: they belong with the shrubs on the large-species grid, not out on the tree grid. */
	bool IsYoungTree(const FMOBiomeSpeciesEntry& Sp)
	{
		const FString Path = Sp.Mesh.ToString();
		return Path.Contains(TEXT("Sapling")) || Path.Contains(TEXT("Seedling"));
	}

	/** Trees stand on a grid of their own (TreesOnly): a forest has to be seen from much farther than the undergrowth, rocks and shrubs around it. */
	bool IsTreeSpecies(const FMOBiomeSpeciesEntry& Sp) { return Sp.HISMTag == NAME_MOResourceTree && !IsYoungTree(Sp); }

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

void MOPCGBiomeSpawner::ApplyNearDrawFloor(FISMComponentDescriptorBase& Descriptor, float MinDistanceCm)
{
	if (MinDistanceCm <= 0.0f)
	{
		return;
	}
	Descriptor.InstanceMinDrawDistance = FMath::RoundToInt(MinDistanceCm);
	Descriptor.InstanceEndCullDistance = FMath::Max(Descriptor.InstanceEndCullDistance, NearFloorEndCullCm);
}

namespace
{
	std::atomic<int32> GBiomeSpawnerRuns{0};
	std::atomic<int64> GBiomeSpawnerAccepted{0};
}

void MOPCGBiomeSpawner::GetRunTotals(int32& OutRuns, int64& OutAccepted)
{
	OutRuns = GBiomeSpawnerRuns.load();
	OutAccepted = GBiomeSpawnerAccepted.load();
}

namespace
{
	/** Instances added per AddInstances call while building: the unit of work between two frame-budget checks. */
	TAutoConsoleVariable<int32> CVarBiomeSpawnerBatch(TEXT("mo.PCG.BiomeSpawnerBatch"), 512, TEXT("Most instances per AddInstances call while a biome spawner builds (one batch, then the PCG frame budget is checked)."), ECVF_Default);
	TAutoConsoleVariable<float> CVarBiomeSpawnerBatchMs(TEXT("mo.PCG.BiomeSpawnerBatchMs"), 3.0f, TEXT("Target game-thread ms per AddInstances batch: the batch size adapts per bucket (a tree with a physics body and a navigation update costs far more than a grass tuft), so one batch cannot blow the frame between two budget checks."), ECVF_Default);
}

FPCGContext* FMOPCGBiomeSpawnerElement::CreateContext()
{
	return new FMOPCGBiomeSpawnerContext();
}

bool FMOPCGBiomeSpawnerElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGBiomeSpawnerElement::Execute);
	FMOPCGBiomeSpawnerContext* Context = static_cast<FMOPCGBiomeSpawnerContext*>(InContext);
	if (!Context->bPlanted)
	{
		Context->bPlanted = true;
		if (!Plant(Context))
		{
			return true;   // nothing to build
		}
		if (Context->AsyncState.ShouldStop())
		{
			return false;   // the planting spent this frame's budget: build from the next frame
		}
	}
	return BuildSlice(Context);
}

bool FMOPCGBiomeSpawnerElement::Plant(FMOPCGBiomeSpawnerContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGBiomeSpawnerElement::Plant);
	const double ExecStartTime = FPlatformTime::Seconds();   // diagnostics: the setup (biome catalog + mesh resolves + generator params) before the per-point work

	const UMOPCGBiomeSpawnerSettings* Settings = Context->GetInputSettings<UMOPCGBiomeSpawnerSettings>();
	check(Settings);

	AActor* TargetActor = Context->GetTargetActor(nullptr);
	if (!TargetActor)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBiomeSpawner] No target actor found"));
		return false;
	}

	// Load the biome catalog once per execution.
	TArray<FName> BiomeIds;
	UMOBiomeDatabaseSettings::GetAllBiomeIds(BiomeIds);
	if (BiomeIds.Num() == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBiomeSpawner] DT_Biomes empty or not configured"));
		return false;
	}
	struct FBiomeEntry
	{
		FName Id;
		const FMOBiomeDefinitionRow* Row = nullptr;
		TArray<UStaticMesh*> LoadedMeshes;       // parallel to Row->Species
		TArray<UStaticMesh*> LoadedEdgeMeshes;   // parallel to Row->WaterEdgeSpecies
		// Classified ONCE per execution (the node's filter does not change between points): the species indices this node plants (first 64, in row order), and per species whether
		// its density follows mo.PCG.GroundCoverScale. The per-point loop reads these instead of re-deriving them from tags and mesh paths for every point.
		TArray<int32> FilteredSpecies;
		TArray<bool> GroundCoverScaled;          // parallel to Row->Species
		TArray<bool> EdgeGroundCoverScaled;      // parallel to Row->WaterEdgeSpecies
		TArray<bool> FeatureRingOk;              // parallel to Row->Species: may grow between a world feature's core and its footprint
		TArray<bool> EdgeFeatureRingOk;          // parallel to Row->WaterEdgeSpecies
	};
	auto ScalesWithGroundCover = [](const FMOBiomeSpeciesEntry& Sp) { return !IsLargeSpecies(Sp) && !IsFarGroundCover(Sp) && !IsFarLarge(Sp); };
	// Round a world feature, between its solid core and its footprint, only low growth: ground cover, shrubs, bushes -- no trees, no rocks
	// (UMOWorldFeatureSubsystem::FFootprint; Wes, Oct 23 2026, BR23-3: "a huge swath of completely blank terrain all around it").
	auto GrowsInFeatureRing = [](const FMOBiomeSpeciesEntry& Sp)
	{
		static const FName Shrub(TEXT("MOBiomeCover_Shrub")), Bush(TEXT("MOResource_Bush"));
		return (!IsLargeSpecies(Sp) || Sp.HISMTag == Shrub || Sp.HISMTag == Bush) && !IsFarGroundCover(Sp) && !IsFarLarge(Sp) && !IsFarCanopy(Sp);
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
			for (int32 i = 0; i < Row->Species.Num(); ++i)
			{
				const FMOBiomeSpeciesEntry& Sp = Row->Species[i];
				UStaticMesh* Loaded = Sp.Mesh.LoadSynchronous();   // main thread — ok
				if (!Loaded)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOBiomeSpawner] species mesh FAILED to load: %s (biome %s)"),
						*Sp.Mesh.ToString(), *Id.ToString());
				}
				E.LoadedMeshes.Add(Loaded);
				E.GroundCoverScaled.Add(ScalesWithGroundCover(Sp));
				E.FeatureRingOk.Add(GrowsInFeatureRing(Sp));
				if (i < 64 && PassesSpeciesFilter(Settings->SpeciesFilter, Sp))
				{
					E.FilteredSpecies.Add(i);
				}
			}
			for (const FMOBiomeSpeciesEntry& Sp : Row->WaterEdgeSpecies)
			{
				UStaticMesh* Loaded = Sp.Mesh.LoadSynchronous();
				if (!Loaded)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOBiomeSpawner] water-edge species mesh FAILED to load: %s (biome %s)"), *Sp.Mesh.ToString(), *Id.ToString());
				}
				E.LoadedEdgeMeshes.Add(Loaded);
				E.EdgeGroundCoverScaled.Add(ScalesWithGroundCover(Sp));
				E.EdgeFeatureRingOk.Add(GrowsInFeatureRing(Sp));
			}
		}
	}
	const bool bPlantsWaterEdge = PassesWaterEdgeFilter(Settings->SpeciesFilter);
	const float GroundCoverScale = FMath::Clamp(CVarGroundCoverScale.GetValueOnGameThread(), 0.0f, 2.0f);
	const float SpeciesDensityScale = FMath::Clamp(CVarSpeciesDensityScale.GetValueOnGameThread(), 0.0f, 4.0f);
	const float WidthMultiplier = FMath::Clamp(CVarWidthScaleMultiplier.GetValueOnGameThread(), 0.1f, 4.0f);
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

	const int32 Seed = Context->GetSeed() + Settings->SeedOffset;   // combined with each point's own seed (one random stream per point, see the parallel loop)

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
	const float VegetationScale = FMath::Max(0.0f, GenParams->Tuning.VegetationDensityScale);   // the world's Vegetation knob (MOWorldCreationTypes.h)
	// Soft biome boundaries (generator mode): the generator's biome index -> this node's entry, for FMOWorldGen::PickSurfaceBiome.
	const bool bSoftPick = Settings->bUseWorldGenerator && CVarBiomeSoftPick.GetValueOnGameThread() != 0;
	TArray<int32> SpawnerIndexOfGenBiome;
	SpawnerIndexOfGenBiome.Init(INDEX_NONE, GenParams->Biomes.Num());
	for (int32 B = 0; B < Biomes.Num(); ++B)
	{
		const int32 GenIndex = GenParams->FindBiomeIndex(Biomes[B].Id);
		if (GenIndex != INDEX_NONE)
		{
			SpawnerIndexOfGenBiome[GenIndex] = B;
		}
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

	// (No viewer test here. Far tiers once dropped their points within MinDistanceFromViewerCm of where the viewer stood at GENERATION time; a cell is generated once, so walking away left
	// a bald circle around the old position (up to the canopy tier's 2.2 km) that nothing ever refilled. The near floor is a per-frame DRAW distance now: see the descriptor below.)

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
			return false;
		}
	}
	// The per-point work (height, water, sinkhole and climate samples, the biome bands, the edge-feather taps, the clump noise) runs IN PARALLEL: it was 75-220 ms of game-thread time per
	// cell (13.5 us a point on the 512 m tree tier), the whole budget of a load and a 3 fps stutter while the far field filled in behind the reveal (packaged, Oct 13 2026). Everything it
	// reads is const for the duration (the generator's caches are thread_local, the water masks are value types, the terrain-mod zones only change on the game thread, which waits here).
	// Each point rolls its own random stream from its PCG seed, so the result does not depend on the order the workers take the points; the buckets are filled afterwards, in input order.
	TArray<const FPCGPoint*> Points;
	for (const FPCGTaggedData& Input : Inputs)
	{
		if (const UPCGPointData* InputPointData = Cast<UPCGPointData>(Input.Data))
		{
			for (const FPCGPoint& Point : InputPointData->GetPoints())
			{
				Points.Add(&Point);
				InputBounds += Point.Transform.GetLocation();
			}
		}
	}
	TotalIn = Points.Num();
	// World features (POIs) are self-contained, hand-tuned compositions: the scatter plants nothing inside one's footprint (UMOWorldFeatureSubsystem,
	// a pure function of the seed -- the same exclusion on every machine, whether or not the feature's actor is spawned yet). Gathered once, here on the game thread.
	TArray<UMOWorldFeatureSubsystem::FFootprint> FeatureFootprints;
	if (UMOWorldFeatureSubsystem* Features = UMOWorldFeatureSubsystem::Get(TargetActor); Features && InputBounds.IsValid)
	{
		Features->GetFootprints(FBox2D(FVector2D(InputBounds.Min), FVector2D(InputBounds.Max)), FeatureFootprints);
	}
	enum class EPointOutcome : uint8 { Empty, Suppressed, Planted };
	struct FPointResult
	{
		EPointOutcome Outcome = EPointOutcome::Empty;
		int32 BiomeIndex = INDEX_NONE;
		const FMOBiomeSpeciesEntry* Species = nullptr;
		UStaticMesh* Mesh = nullptr;
		FTransform Xf;
	};
	TArray<FPointResult> Results;
	Results.SetNum(Points.Num());
	const bool bSingleThread = CVarBiomeSpawnerParallel.GetValueOnAnyThread() == 0;
	// mo.PCG.BiomeSpawnerProfile 1: CPU time per stage, summed over the workers, in the [MOBiomeSpawner] line (which query to make cheaper). One accumulator per task (no shared
	// counters: 24 threads adding to one cache line would be a cost of its own); off by default.
	struct FStageCycles { uint64 Total = 0, Gen = 0, Climate = 0, Blend = 0; };
	struct FCyScope
	{
		uint64* Acc;
		uint64 T0;
		FCyScope(uint64& InAcc, bool bOn) : Acc(bOn ? &InAcc : nullptr), T0(bOn ? FPlatformTime::Cycles64() : 0) {}
		~FCyScope() { if (Acc) { *Acc += FPlatformTime::Cycles64() - T0; } }
	};
	const bool bProfile = CVarBiomeSpawnerProfile.GetValueOnGameThread() != 0;
	TArray<FStageCycles> StageContexts;
	ParallelForWithTaskContext(StageContexts, Points.Num(), [&](FStageCycles& Cy, int32 PointIndex)
	{
		FCyScope WholePoint(Cy.Total, bProfile);
		const FPCGPoint& Point = *Points[PointIndex];
		FPointResult& Result = Results[PointIndex];
		FRandomStream RandomStream(PCGHelpers::ComputeSeed(Seed, Point.Seed));
		{
			const FVector Location = Point.Transform.GetLocation();

			if (TerrainMod && TerrainMod->IsLocationModified(Location))
			{
				Result.Outcome = EPointOutcome::Suppressed;
				return;
			}
			bool bFeatureRing = false;   // inside a world feature's footprint but outside its core: low growth only
			for (const UMOWorldFeatureSubsystem::FFootprint& Footprint : FeatureFootprints)
			{
				const double DistSq = FMath::Square(Location.X - Footprint.X) + FMath::Square(Location.Y - Footprint.Y);
				if (DistSq < Footprint.CoreSq)
				{
					Result.Outcome = EPointOutcome::Suppressed;
					return;
				}
				bFeatureRing |= DistSq < Footprint.RadiusSq;
			}

			// Caves: nothing is planted in a sinkhole's rim zone, and a point far below the ground height is a CAVE surface (wall/floor of a tunnel,
			// cavern or shaft), not a place for meadow grass or trees. Same generator, same terrain seed as the voxel stamp that carved the cave.
			bool bBankStrip = false;   // dry ground in the first metres past an upland lake's shore: only the water-edge species may stand here
			if (Settings->bUseWorldGenerator)
			{
				FCyScope GenStage(Cy.Gen, bProfile);
				const float GroundZ = FMOWorldGen::SampleHeightCm(*GenParams, Location.X, Location.Y, TerrainSeed, GenParams->Tuning.VoxelSizeCm);
				// ... and nothing is planted under water: rivers, lakes and the sea are terrain below SeaLevelCm, and an upland lake's water is the ellipse of its bowl (FMOPCGWaterMask).
				if (Location.Z < GroundZ - 200.0f || EdgeMask.IsInWater(Location.X, Location.Y, Location.Z)
					|| FMOWorldGen::DistanceToSinkholeRimZone(*GenParams, Location.X, Location.Y, TerrainSeed) <= 0.0f)
				{
					Result.Outcome = EPointOutcome::Suppressed;
					return;
				}
				bBankStrip = WaterMask.IsInWater(Location.X, Location.Y, Location.Z);
			}

			// Shared mask math (UMOBiomeDatabaseSettings) so tests/tools can
			// query the same field this spawner realizes.
			float Moisture = 0.0f, Temperature = 0.0f;
			const float SlopeDeg = PointSlopeDeg(Point);
			const FBiomeEntry* Chosen = nullptr;
			int32 ChosenIndex = INDEX_NONE;
			{
				FCyScope ClimateStage(Cy.Climate, bProfile);
				ClimateAt(Location, Moisture, Temperature);
				if (bSoftPick)
				{
					// The ground's own soft boundary (FMOWorldGen::SampleSurfaceBiomeWeights): inside the band each point rolls for its biome by the ground's mix, so the two biomes' plants interleave and thin out
					// where their ground does.
					const int32 GenIndex = FMOWorldGen::PickSurfaceBiome(*GenParams, Location.X, Location.Y, TerrainSeed, (float)Location.Z, SlopeDeg, Moisture, Temperature);
					ChosenIndex = SpawnerIndexOfGenBiome.IsValidIndex(GenIndex) ? SpawnerIndexOfGenBiome[GenIndex] : INDEX_NONE;
					Chosen = ChosenIndex != INDEX_NONE ? &Biomes[ChosenIndex] : nullptr;
				}
				else
				{
					// Highest-priority biome whose bands contain this sample.
					for (int32 B = 0; B < Biomes.Num(); ++B)
					{
						if (Biomes[B].Row->Contains(Location.Z, SlopeDeg, Moisture, Temperature))
						{
							Chosen = &Biomes[B];
							ChosenIndex = B;
							break;
						}
					}
				}
			}
			if (!Chosen)
			{
				return;
			}

			// EDGE BLEND (P4 round 2): feather density near biome boundaries
			// instead of hard cuts. Four taps at half the blend width — the
			// fraction that agree with this point's biome scales acceptance,
			// so density ramps across EdgeBlendWidth rather than stepping.
			// (Not with the soft pick: the per-point roll already spreads the boundary, and the feather would thin both biomes in the band.)
			float EdgeScale = 1.0f;
			const float BlendW = Chosen->Row->EdgeBlendWidth;
			// (the far tiers skip the feather: it costs four climate samples per point and moves a density ramp nobody can see from kilometres away)
			const bool bFarTierFilter = Settings->SpeciesFilter == EMOBiomeSpeciesFilter::FarCanopyOnly || Settings->SpeciesFilter == EMOBiomeSpeciesFilter::FarLargeOnly || Settings->SpeciesFilter == EMOBiomeSpeciesFilter::FarGroundCoverOnly;
			if (BlendW > 1.0f && !bFarTierFilter && !bSoftPick)
			{
				FCyScope BlendStage(Cy.Blend, bProfile);
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
			struct FCandidate { const FMOBiomeSpeciesEntry* Species; UStaticMesh* Mesh; float Scale; bool bGroundCoverScaled; };
			TArray<FCandidate, TInlineAllocator<96>> Candidates;
			if (!bBankStrip)
			{
				for (const int32 i : Chosen->FilteredSpecies)
				{
					const FMOBiomeSpeciesEntry& Sp = Chosen->Row->Species[i];
					if (SlopeDeg >= Sp.MinSlopeDeg && SlopeDeg <= Sp.MaxSlopeDeg && (!bFeatureRing || Chosen->FeatureRingOk[i]))   // the species' own slope band (a plate on gentle ground, a cliff slab on steep)
					{
						Candidates.Add({ &Sp, Chosen->LoadedMeshes[i], EdgeScale, Chosen->GroundCoverScaled[i] });
					}
				}
			}
			if (bPlantsWaterEdge && Chosen->Row->WaterEdgeSpecies.Num() > 0 && EdgeMask.IsActive())
			{
				const float Shore = EdgeMask.ShoreFactor(Location.X, Location.Y, Location.Z, Chosen->Row->WaterEdgeBandCm);
				if (Shore > 0.0f)
				{
					const int32 NumEdge = FMath::Min(Chosen->Row->WaterEdgeSpecies.Num(), 32);
					for (int32 i = 0; i < NumEdge; ++i)
					{
						const FMOBiomeSpeciesEntry& Sp = Chosen->Row->WaterEdgeSpecies[i];
						if (SlopeDeg >= Sp.MinSlopeDeg && SlopeDeg <= Sp.MaxSlopeDeg && (!bFeatureRing || Chosen->EdgeFeatureRingOk[i]))
						{
							Candidates.Add({ &Sp, Chosen->LoadedEdgeMeshes[i], Shore, Chosen->EdgeGroundCoverScaled[i] });
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
			// surviving area fraction (measured, not assumed uniform) so authored per-hectare rates hold.
			TArray<float, TInlineAllocator<96>> Acceptance;
			Acceptance.SetNumZeroed(Candidates.Num());
			float TotalAcceptance = 0.0f;
			for (int32 i = 0; i < Candidates.Num(); ++i)
			{
				const FMOBiomeSpeciesEntry& Sp = *Candidates[i].Species;
				float P = (Candidates[i].Mesh ? Candidates[i].Scale * Sp.DensityPerHectare * SpeciesDensityScale * VegetationScale / Settings->InputPointsPerHectare : 0.0f);
				if (P > 0.0f && Candidates[i].bGroundCoverScaled)
				{
					P *= GroundCoverScale;
				}
				if (P > 0.0f && Sp.ClusterGroup >= 0)
				{
					// Shared field per (biome, group): offsets depend on the GROUP, not the species, so group members co-locate. Compensated by the
					// field's MEASURED coverage above the core (ClumpCompensation), so the authored per-hectare rate holds.
					if (UMOPCGBiomeSpawnerSettings::ClumpField01(Location, Sp.ClusterGroup, Sp.ClusterRadius) < Sp.ClusterCore)
					{
						P = 0.0f;
					}
					else
					{
						P *= UMOPCGBiomeSpawnerSettings::ClumpCompensation(Sp.ClusterCore);
					}
				}
				else if (P > 0.0f && Sp.ClusterRadius > 0.0f)
				{
					// Ungrouped legacy clustering: per-species clump noise.
					const float Clump = FMath::PerlinNoise2D(FVector2D(
						Location.X / FMath::Max(Sp.ClusterRadius * 4.0f, 1.0f) + i * 17.3f,
						Location.Y / FMath::Max(Sp.ClusterRadius * 4.0f, 1.0f) - i * 29.1f));
					P *= FMath::Clamp(Clump * 2.0f + 0.5f, 0.0f, 2.0f) / UMOPCGBiomeSpawnerSettings::LegacyClumpMean();   // patchy, but the average holds
				}
				Acceptance[i] = P;
				TotalAcceptance += P;
			}
			if (TotalAcceptance <= 0.0f)
			{
				return;
			}

			const float Roll = RandomStream.FRand();
			if (Roll >= FMath::Min(TotalAcceptance, 1.0f))
			{
				return;   // slot stays empty — that's most points
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
				return;
			}

			FTransform Xf = Point.Transform;
			const float Scale = RandomStream.FRandRange(Sp.MinScale, Sp.MaxScale);
			const float Width = Scale * Sp.WidthScale * WidthMultiplier;
			Xf.SetScale3D(FVector(Width, Width, Scale));
			const float Yaw = RandomStream.FRandRange(0.0f, 2.0f * PI);
			const FVector SurfaceNormal = Point.Transform.GetRotation().GetUpVector();
			if (Sp.bFaceDownhill)
			{
				// Open-backed cliff slabs (BR18-14 / BR18-19): upright, the rock face turned down the slope (a little jitter so a cluster is not a fence).
				const float Jitter = FMath::DegreesToRadians(RandomStream.FRandRange(-10.0f, 10.0f));
				Xf.SetRotation(FMOSpeciesPlacement::FaceDownhillRotation(Sp, SurfaceNormal, Jitter, Yaw));
			}
			else if (Sp.bAlignToSurfaceNormal)
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

			// Burial (FMOSpeciesPlacement: the gallery places the same way): along the instance's own up axis, so a rock on a slope goes into the slope, not into the air below it.
			{
				const float SlopeRad = FMath::Acos(FMath::Clamp(static_cast<float>(SurfaceNormal.Z), -1.0f, 1.0f));
				const float Offset = FMOSpeciesPlacement::BurialOffsetCm(Sp, Mesh->GetBounds(), Scale) - FMOSpeciesPlacement::FaceDropCm(Sp, Mesh->GetBounds(), Scale, SlopeRad);
				if (!FMath::IsNearlyZero(Offset))
				{
					Xf.AddToTranslation(Xf.GetRotation().GetUpVector() * Offset);
				}
			}

			Result.Outcome = EPointOutcome::Planted;
			Result.BiomeIndex = ChosenIndex;
			Result.Species = &Sp;
			Result.Mesh = Mesh;
			Result.Xf = Xf;
		}
	}, bSingleThread ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
	const double ParallelDoneTime = FPlatformTime::Seconds();

	// Merge in input order (the buckets' instance order, and so the components' instance indices, stay deterministic).
	// A tree felled or a boulder quarried stays gone when its cell is generated again (UMOWorldRemovalSubsystem).
	const UMOWorldRemovalSubsystem* Removal = UMOWorldRemovalSubsystem::Get(TargetActor);
	const bool bAnyRemoved = Removal && Removal->GetNumRemoved() > 0;
	for (const FPointResult& R : Results)
	{
		if (R.Outcome == EPointOutcome::Suppressed)
		{
			++TotalSuppressed;
			continue;
		}
		if (R.Outcome != EPointOutcome::Planted)
		{
			continue;
		}
		if (bAnyRemoved && Removal->IsRemoved(R.Xf.GetLocation(), R.Mesh))
		{
			++TotalSuppressed;
			continue;
		}
		const FBiomeEntry& Chosen = Biomes[R.BiomeIndex];
		const FMOBiomeSpeciesEntry& Sp = *R.Species;
		const FString Key = FString::Printf(TEXT("%s|%s"), *R.Mesh->GetPathName(), *Chosen.Id.ToString());
		FSpeciesBucket& Bucket = Buckets.FindOrAdd(Key);
		Bucket.Mesh = R.Mesh;
		Bucket.HISMTag = Sp.HISMTag;
		Bucket.BiomeId = Chosen.Id;
		Bucket.ResourceNodeId = Sp.ResourceNodeId;
		Bucket.bAutoSweep = Sp.bAutoSweepOnTerraform;
		Bucket.Collision = Sp.Collision;
		Bucket.Transforms.Add(R.Xf);
		++TotalAccepted;
	}

	FString StageSummary = FString::Printf(TEXT("setup %.0f parallel %.0f merge %.0f"),
		(PlantStartTime - ExecStartTime) * 1000.0, (ParallelDoneTime - PlantStartTime) * 1000.0, (FPlatformTime::Seconds() - ParallelDoneTime) * 1000.0);
	if (bProfile)
	{
		FStageCycles Sum;
		for (const FStageCycles& C : StageContexts)
		{
			Sum.Total += C.Total; Sum.Gen += C.Gen; Sum.Climate += C.Climate; Sum.Blend += C.Blend;
		}
		const double CyMs = FPlatformTime::GetSecondsPerCycle64() * 1000.0;
		StageSummary += FString::Printf(TEXT("; cpu %.0f ms: gen %.0f climate %.0f blend %.0f other %.0f"), Sum.Total * CyMs, Sum.Gen * CyMs, Sum.Climate * CyMs, Sum.Blend * CyMs,
			(double)(Sum.Total - Sum.Gen - Sum.Climate - Sum.Blend) * CyMs);
	}

	// The build runs in BuildSlice, a batch at a time inside PCG's frame budget.
	TArray<FSpeciesBucket> Ordered;
	Ordered.Reserve(Buckets.Num());
	for (auto& Pair : Buckets)
	{
		Ordered.Add(MoveTemp(Pair.Value));
	}
	return SubmitBuckets(Context, TargetActor, MoveTemp(Ordered), TotalIn, TotalAccepted, TotalSuppressed, Biomes.Num(), (FPlatformTime::Seconds() - ExecStartTime) * 1000.0,
		MoveTemp(StageSummary), InputBounds.IsValid ? FString::Printf(TEXT("(%.0f,%.0f)-(%.0f,%.0f)"), InputBounds.Min.X, InputBounds.Min.Y, InputBounds.Max.X, InputBounds.Max.Y) : FString(TEXT("none")));
}

bool FMOPCGBiomeSpawnerElement::SubmitBuckets(FMOPCGBiomeSpawnerContext* Context, AActor* TargetActor, TArray<FSpeciesBucket>&& Buckets, int32 TotalIn, int32 TotalAccepted,
	int32 TotalSuppressed, int32 NumBiomes, double PlantMs, FString&& StageSummary, FString&& BoundsText)
{
	UPCGComponent* SourceComponent = Cast<UPCGComponent>(Context->ExecutionSource.Get());
	if (!SourceComponent)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBiomeSpawner] No source PCG component"));
		return false;
	}
	Context->TargetActor = TargetActor;
	Context->SourceComponent = SourceComponent;
	Context->Buckets.Reserve(Buckets.Num());
	for (FSpeciesBucket& Bucket : Buckets)
	{
		Context->BucketSummary += FString::Printf(TEXT(" %s/%s=%d"), *Bucket.BiomeId.ToString(), Bucket.Mesh ? *Bucket.Mesh->GetName() : TEXT("null"), Bucket.Transforms.Num());
		if (Bucket.Mesh && Bucket.Transforms.Num() > 0)
		{
			Context->Buckets.Add(MoveTemp(Bucket));
		}
	}
	Context->TotalIn = TotalIn;
	Context->TotalAccepted = TotalAccepted;
	Context->TotalSuppressed = TotalSuppressed;
	Context->NumBiomes = NumBiomes;
	Context->PlantMs = PlantMs;
	Context->StageSummary = MoveTemp(StageSummary);
	Context->BoundsText = MoveTemp(BoundsText);
	return true;
}

bool FMOPCGBiomeSpawnerElement::BuildSlice(FMOPCGBiomeSpawnerContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGBiomeSpawnerElement::Build);
	const double SliceStart = FPlatformTime::Seconds();
	const UMOPCGBiomeSpawnerSettings* Settings = Context->GetInputSettings<UMOPCGBiomeSpawnerSettings>();
	AActor* TargetActor = Context->TargetActor.Get();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (!Settings || !TargetActor || !SourceComponent)
	{
		return true;   // torn down between two slices
	}
	const int32 BatchSize = FMath::Max(CVarBiomeSpawnerBatch.GetValueOnGameThread(), 1);
	++Context->Slices;

	while (Context->NextBucket < Context->Buckets.Num())
	{
		FSpeciesBucket& Bucket = Context->Buckets[Context->NextBucket];
		const bool bFarTier = IsFarLargeTag(Bucket.HISMTag) || IsFarCanopyTag(Bucket.HISMTag) || Bucket.HISMTag == NAME_MOFarGrass;
		const bool bGroundCover = Bucket.HISMTag == FName(TEXT("MOBiomeCover_Grass")) || Bucket.HISMTag == FName(TEXT("MOBiomeCover_Flower")) || Bucket.HISMTag == FName(TEXT("MOBiomeCover_Debris"));

		UInstancedStaticMeshComponent* ISM = Context->CurrentISM.Get();
		if (!ISM)
		{
			// First batch of this bucket: the managed HISM per (mesh, biome).
			FPCGISMComponentBuilderParams Params;
			Params.Descriptor.StaticMesh = Bucket.Mesh;
			if (Bucket.Material)
			{
				Params.Descriptor.OverrideMaterials.Init(Bucket.Material, FMath::Max(Bucket.Mesh->GetStaticMaterials().Num(), 1));
			}
			Params.Descriptor.ComponentClass = UHierarchicalInstancedStaticMeshComponent::StaticClass();
			Params.Descriptor.BodyInstance.SetCollisionProfileName(Settings->DecorativeCollisionProfile.Name);   // overridden below for harvestable species
			Params.Descriptor.bCastShadow = Settings->bCastShadows;
			Params.Descriptor.bAffectDistanceFieldLighting = false;
			Params.Descriptor.bAffectDynamicIndirectLighting = false;

			// What is worth rendering depends on the tier. The FAR tiers (stand-in trees, canopy clumps, far grass tufts) are seen as silhouettes: no shadow of their own (their shadows would be the
			// biggest line in the shadow pass for nothing visible), no ray-tracing / reflection presence, no wind, and never drawn inside the distance the NEAR tiers own (below).
			// GROUND COVER (grass, flowers, debris) casts no shadow and stays out of the ray-traced scene either: millions of blades add cost to those passes and no visible shading.
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
			}
			else if (Settings->WindDisableDistanceCm > 0)
			{
				Params.Descriptor.WorldPositionOffsetDisableDistance = Settings->WindDisableDistanceCm;   // swaying instances re-render their shadow pages every frame
			}
			if (!bFarTier)
			{
				Params.Descriptor.ShadowCacheInvalidationBehavior = FoliageShadowCacheBehavior(CVarFoliageRigidShadows.GetValueOnAnyThread());   // see mo.Foliage.RigidShadows
			}
			// The near floor is a per-frame DRAW distance: a far cell plants its whole area and the camera decides what is drawn, so a cell generated around one position still fills in when the
			// player walks away (the generation-time cut it replaced left bald circles). See ApplyNearDrawFloor for the end distance the engine needs before it honours the floor at all.
			MOPCGBiomeSpawner::ApplyNearDrawFloor(Params.Descriptor, Settings->MinDistanceFromViewerCm);

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
			// One policy (FMOSpeciesPlacement::CollisionProfile -- the gallery's drop test checks the same answer): resource type, decorative default, the species' own say.
			Params.Descriptor.BodyInstance.SetCollisionProfileName(FMOSpeciesPlacement::CollisionProfile(Bucket.Collision, ResourceDef, Settings->DecorativeCollisionProfile.Name));
			// Far tiers are draw-only, whatever the palette row says: no physics body per instance (there are a hundred thousand of them) and nothing for a trace to hit around the player, where
			// they now stand undrawn under the near tiers.
			if (bFarTier)
			{
				Params.Descriptor.BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			}
			if (bFarTier || bGroundCover)
			{
				Params.Descriptor.bCanEverAffectNavigation = false;
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
			const uint32 BucketHash = (GetTypeHash(Bucket.BiomeId) ^ (GetTypeHash(Bucket.HISMTag) * 31) ^ (Bucket.Material ? GetTypeHash(Bucket.Material->GetFName()) * 131 : 0u) ^ 0x4D4F4249u) | 1u;
			Params.Descriptor.RayTracingGroupId = (int32)(BucketHash & 0x7fffffffu);
			Params.SettingsCrc = FPCGCrc(BucketHash);

			ISM = UPCGActorHelpers::GetOrCreateISMC(TargetActor, SourceComponent, Params, Context);
			if (!ISM)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOBiomeSpawner] Failed HISM for %s"), *Bucket.Mesh->GetName());
				++Context->NextBucket;
				Context->NextTransform = 0;
				continue;
			}
			Context->CurrentISM = ISM;
			++Context->Components;
			// A collidable species (trees, rocks: one physics body + a navigation update per instance) starts with small batches; the
			// measured time per batch then sizes the rest (below).
			const bool bCollides = !(bFarTier || bGroundCover) && ISM->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
			Context->BucketBatch = bCollides ? FMath::Min(64, BatchSize) : BatchSize;
		}

		// Navigation: only what can block a walker matters to the nav mesh. Far stand-ins (draw-only) and ground cover (passable) skip the per-instance navigation update.
		const int32 Count = FMath::Min(FMath::Max(Context->BucketBatch, 1), Bucket.Transforms.Num() - Context->NextTransform);
		const TArray<FTransform> Batch(Bucket.Transforms.GetData() + Context->NextTransform, Count);
		const double BatchStart = FPlatformTime::Seconds();
		ISM->AddInstances(Batch, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true, /*bUpdateNavigation=*/!(bFarTier || bGroundCover));
		const double BatchMs = (FPlatformTime::Seconds() - BatchStart) * 1000.0;
		Context->WorstBatchMs = FMath::Max(Context->WorstBatchMs, BatchMs);
		// Size the next batch of this bucket from what this one cost: about TargetMs each, between 8 and BatchSize instances.
		const double TargetMs = FMath::Max(CVarBiomeSpawnerBatchMs.GetValueOnGameThread(), 0.25f);
		if (BatchMs > 0.0)
		{
			Context->BucketBatch = FMath::Clamp(FMath::RoundToInt(Count * TargetMs / BatchMs), 8, BatchSize);
		}
		Context->NextTransform += Count;
		if (Context->NextTransform >= Bucket.Transforms.Num())
		{
			Bucket.Transforms.Empty();
			++Context->NextBucket;
			Context->NextTransform = 0;
			Context->CurrentISM = nullptr;
		}
		if (Context->NextBucket < Context->Buckets.Num() && Context->AsyncState.ShouldStop())
		{
			Context->BuildMs += (FPlatformTime::Seconds() - SliceStart) * 1000.0;
			return false;   // the rest next frame
		}
	}
	Context->BuildMs += (FPlatformTime::Seconds() - SliceStart) * 1000.0;

	++GBiomeSpawnerRuns;
	GBiomeSpawnerAccepted += Context->TotalAccepted;
	// Per-bucket breakdown so density tuning reads from the log, not guesses. ms = the planting slice; build = the game-thread time of every build slice (slices = frames it took).
	// Off by default (mo.PCG.LogCells 1): one line per generated cell flooded every log at Warning (1,290 lines in one nettest host, Oct 22).
	if (CVarLogCells.GetValueOnAnyThread() != 0)
	UE_LOG(LogMOFramework, Log,
		TEXT("[MOBiomeSpawner] in=%d accepted=%d suppressed=%d components=%d biomes=%d ms=%.0f (build %.1f in %d slice(s) worst batch %.1f; %s) inBounds=%s |%s"),
		Context->TotalIn, Context->TotalAccepted, Context->TotalSuppressed, Context->Components, Context->NumBiomes, Context->PlantMs, Context->BuildMs, Context->Slices, Context->WorstBatchMs,
		*Context->StageSummary, *Context->BoundsText, *Context->BucketSummary);
	return true;
}

#undef LOCTEXT_NAMESPACE
