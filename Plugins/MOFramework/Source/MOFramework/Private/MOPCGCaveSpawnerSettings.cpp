#include "MOPCGCaveSpawnerSettings.h"
#include "MOFramework.h"
#include "MOWorldGenSettings.h"
#include "MOWorldSeedSubsystem.h"
#include "MOTerrainModificationSubsystem.h"
#include "MOWorldRemovalSubsystem.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "HAL/IConsoleManager.h"
#include "Async/ParallelFor.h"

namespace
{
	TAutoConsoleVariable<float> CVarCaveDensityScale(TEXT("mo.PCG.CaveDensityScale"), 1.0f, TEXT("Multiplies every cave scatter row's density (MO Cave Spawner)."), ECVF_Default);

	/** How far behind the surface (cm) the rock is asked what it holds: inside the wall, past the voxel mesher's rounding. */
	constexpr double OreProbeDepthCm = 30.0;
}

FPCGElementPtr UMOPCGCaveSpawnerSettings::CreateElement() const
{
	return MakeShared<FMOPCGCaveSpawnerElement>();
}

#if WITH_EDITOR
FText UMOPCGCaveSpawnerSettings::GetNodeTooltipText() const
{
	return NSLOCTEXT("MOFramework", "MOCaveSpawnerTooltip",
		"Plants the cave scatter table (FMOCaveScatterRow) on the surfaces of the world generator's caves under this cell: rubble and gravel on floors, rocks on walls "
		"and ceilings, ore where the rock behind the wall holds it (coal seams, hematite, copper, tin, chert). Harvestable rows share the biome spawner's build.");
}
#endif

bool FMOPCGCaveSpawnerElement::Plant(FMOPCGBiomeSpawnerContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGCaveSpawnerElement::Plant);
	const double Start = FPlatformTime::Seconds();
	const UMOPCGCaveSpawnerSettings* Settings = Context->GetInputSettings<UMOPCGCaveSpawnerSettings>();
	AActor* TargetActor = Context->GetTargetActor(nullptr);
	const UPCGComponent* Source = Cast<UPCGComponent>(Context->ExecutionSource.Get());
	if (!Settings || !TargetActor || !Source)
	{
		return false;
	}
	if (!Settings->CaveScatterTable || Settings->CaveScatterTable->GetRowStruct() != FMOCaveScatterRow::StaticStruct())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCaveSpawner] no cave scatter table (FMOCaveScatterRow) on the node"));
		return false;
	}
	// The cave field needs the world's own terrain seed (the same one the voxel stamp carves with): no seed, no caves to find.
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(TargetActor);
	if (!Seeds || !Seeds->HasActiveSeed())
	{
		return false;
	}
	const int32 TerrainSeed = Seeds->GetActiveTerrainSeed();
	const FMOWorldGenParamsRef GenParams = UMOWorldGenSettings::RefreshPublishedParams();
	if (!GenParams->Tuning.bEnableCaves)
	{
		return false;
	}

	struct FRow
	{
		FName Name;
		const FMOCaveScatterRow* Row = nullptr;
		UStaticMesh* Mesh = nullptr;
		UMaterialInterface* Material = nullptr;
		double BottomCm = 0.0;   // the mesh's pivot above its lowest point (Megascans pivots are centred): lifts it onto the surface before the sink
	};
	TArray<FRow> Rows;
	Settings->CaveScatterTable->ForeachRow<FMOCaveScatterRow>(TEXT("MOCaveSpawner"), [&Rows](const FName& Name, const FMOCaveScatterRow& Row)
	{
		UStaticMesh* Mesh = Row.Mesh.LoadSynchronous();   // main thread
		if (!Mesh)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCaveSpawner] row %s: mesh %s failed to load"), *Name.ToString(), *Row.Mesh.ToString());
			return;
		}
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		Rows.Add({ Name, &Row, Mesh, Row.Material.LoadSynchronous(), -(Bounds.Origin.Z - Bounds.BoxExtent.Z) });
	});
	if (Rows.Num() == 0)
	{
		return false;
	}

	const FBox Cell = Source->GetGridBounds();
	if (!Cell.IsValid)
	{
		return false;
	}
	TArray<FMOCaveSurfacePoint> Points;
	const FBox2D Area(FVector2D(Cell.Min), FVector2D(Cell.Max));
	FMOWorldGen::FindCaveSurfaces(*GenParams, TerrainSeed, Area, Settings->ColumnSpacingCm, Points);
	// Outcrops (bare rock on steep ground) only when a row places something there: the surface half of the geology (seam outcrops, malachite stains).
	if (Rows.ContainsByPredicate([](const FRow& R) { return R.Row->Surface == EMOCaveSurfaceKind::Outcrop; }))
	{
		FMOWorldGen::FindOutcrops(*GenParams, TerrainSeed, Area, Settings->OutcropSpacingCm, Settings->OutcropMinSlopeDeg, Points);
	}
	const double FoundTime = FPlatformTime::Seconds();
	if (Points.Num() == 0)
	{
		return false;   // no cave and no bare rock under this cell (the common case): nothing to build
	}

	// What the rock is and holds at every point, in parallel: the bedrock of its column, the ore just behind the surface.
	TArray<EMOBedrock> Rock;
	TArray<FMOOreSample> Ore;
	Rock.SetNumUninitialized(Points.Num());
	Ore.SetNumUninitialized(Points.Num());
	ParallelFor(Points.Num(), [&](int32 I)
	{
		const FMOCaveSurfacePoint& P = Points[I];
		Rock[I] = FMOWorldGen::SampleBedrock(*GenParams, P.Location.X, P.Location.Y, TerrainSeed);
		const FVector In = P.Location - P.AirNormal * OreProbeDepthCm;
		Ore[I] = FMOWorldGen::SampleOreIn(*GenParams, Rock[I], In.X, In.Y, In.Z, TerrainSeed);
	});
	const double GeologyTime = FPlatformTime::Seconds();

	UMOTerrainModificationSubsystem* TerrainMod = nullptr;
	if (Settings->bRespectTerrainModifications)
	{
		if (UWorld* World = TargetActor->GetWorld())
		{
			TerrainMod = World->GetSubsystem<UMOTerrainModificationSubsystem>();
		}
	}
	const UMOWorldRemovalSubsystem* Removal = UMOWorldRemovalSubsystem::Get(TargetActor);
	const bool bAnyRemoved = Removal && Removal->GetNumRemoved() > 0;
	const float Density = Settings->DensityScale * FMath::Clamp(CVarCaveDensityScale.GetValueOnGameThread(), 0.0f, 10.0f);
	const uint32 Seed = (uint32)(Context->GetSeed() + Settings->SeedOffset);

	TArray<FSpeciesBucket> Buckets;
	Buckets.SetNum(Rows.Num());
	for (int32 R = 0; R < Rows.Num(); ++R)
	{
		FSpeciesBucket& B = Buckets[R];
		B.Mesh = Rows[R].Mesh;
		B.Material = Rows[R].Material;
		B.HISMTag = Rows[R].Row->HISMTag;
		B.BiomeId = FName(*(TEXT("Cave_") + Rows[R].Name.ToString()));   // one managed component per row
		B.ResourceNodeId = Rows[R].Row->ResourceNodeId;
		B.Collision = Rows[R].Row->Collision;
	}
	int32 Accepted = 0, Suppressed = 0, OrePoints = 0;
	int32 KindCount[4] = { 0, 0, 0, 0 };
	for (int32 I = 0; I < Points.Num(); ++I)
	{
		const FMOCaveSurfacePoint& P = Points[I];
		++KindCount[(int32)P.Kind];
		OrePoints += Ore[I].Kind != EMOOreKind::None ? 1 : 0;
		if (TerrainMod && TerrainMod->IsLocationModified(P.Location))
		{
			++Suppressed;   // worked rock: the player dug here
			continue;
		}
		// One random stream per (point, row), from the point's place: the same instance on every generation and every co-op machine.
		const uint32 PointHash = HashCombineFast(GetTypeHash(FIntVector(FMath::RoundToInt32(P.Location.X), FMath::RoundToInt32(P.Location.Y), FMath::RoundToInt32(P.Location.Z))), Seed);
		for (int32 R = 0; R < Rows.Num(); ++R)
		{
			const FMOCaveScatterRow& Row = *Rows[R].Row;
			if (Row.Surface != P.Kind || (Row.Bedrock.Num() > 0 && !Row.Bedrock.Contains(Rock[I])))
			{
				continue;
			}
			if (Row.Ore != EMOOreKind::None && (Ore[I].Kind != Row.Ore || Ore[I].Grade < Row.MinOreGrade))
			{
				continue;
			}
			FRandomStream Rand((int32)HashCombineFast(PointHash, (uint32)R * 0x9E3779B9u + 0x2545F491u));
			if (Rand.FRand() >= Row.PerSquareMetre * Density * P.AreaM2)
			{
				continue;
			}
			const float Scale = Rand.FRandRange(Row.ScaleMin, FMath::Max(Row.ScaleMin, Row.ScaleMax));
			const FVector Up = FMath::Lerp(FVector::UpVector, P.AirNormal, (double)Row.AlignToSurface).GetSafeNormal(1.0e-4, FVector::UpVector);
			const FQuat Rotation = FQuat(Up, Rand.FRandRange(0.0f, 2.0f * UE_PI)) * FQuat::FindBetweenNormals(FVector::UpVector, Up);
			const FVector Location = P.Location + Up * (Rows[R].BottomCm * Scale) - P.AirNormal * (double)(Row.SinkCm * Scale);
			if (bAnyRemoved && Removal->IsRemoved(Location, Rows[R].Mesh))
			{
				++Suppressed;   // quarried for good (UMOWorldRemovalSubsystem)
				continue;
			}
			Buckets[R].Transforms.Emplace(Rotation, Location, FVector(Scale));
			++Accepted;
		}
	}
	FString Stage = FString::Printf(TEXT("caves %.0f geology %.0f plant %.0f; surfaces %d floor / %d wall / %d ceiling / %d outcrop, %d on ore"),
		(FoundTime - Start) * 1000.0, (GeologyTime - FoundTime) * 1000.0, (FPlatformTime::Seconds() - GeologyTime) * 1000.0, KindCount[0], KindCount[1], KindCount[2], KindCount[3], OrePoints);
	FString BoundsText = FString::Printf(TEXT("cave cell (%.0f,%.0f)-(%.0f,%.0f)"), Cell.Min.X, Cell.Min.Y, Cell.Max.X, Cell.Max.Y);
	return SubmitBuckets(Context, TargetActor, MoveTemp(Buckets), Points.Num(), Accepted, Suppressed, Rows.Num(), (FPlatformTime::Seconds() - Start) * 1000.0, MoveTemp(Stage), MoveTemp(BoundsText));
}
