#include "MOCompositionGrounding.h"
#include "MOFramework.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	struct FPiece
	{
		UInstancedStaticMeshComponent* Ism = nullptr;
		int32 Index = INDEX_NONE;
		FTransform World;
		FBox Bounds;
		EMOCompositionPiece Kind = EMOCompositionPiece::Rock;
		FVector Moved = FVector::ZeroVector;
	};

	bool NameHasAny(const FString& Lower, std::initializer_list<const TCHAR*> Words)
	{
		for (const TCHAR* Word : Words)
		{
			if (Lower.Contains(Word))
			{
				return true;
			}
		}
		return false;
	}

	/** The nearest hit on one of the composition's ROCK components along Start -> End, looking past everything else (terrain collision where it exists,
	 *  grass, the composition's plants): the rocks are what a plant may stand on and what a rock may rest on. */
	bool TraceRocks(UWorld* World, const TSet<const UPrimitiveComponent*>& Rocks, const FVector& Start, const FVector& End, FHitResult& OutHit)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MOCompositionGrounding), false);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		for (int32 Layer = 0; Layer < 8; ++Layer)
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params))
			{
				return false;
			}
			if (Rocks.Contains(Hit.GetComponent()))
			{
				OutHit = Hit;
				return true;
			}
			Params.AddIgnoredComponent(Hit.GetComponent());
		}
		return false;
	}
}

FString FMOCompositionGroundingStats::Describe() const
{
	return FString::Printf(TEXT("plants %d (floating %d, worst %.0f cm; buried %d, worst %.0f cm), rocks %d (gap under %d, worst %.0f cm), clinging %d, moved %d"),
		Foliage, FoliageFloating, MaxFoliageFloatCm, FoliageBuried, MaxFoliageBuryCm, Rocks, RocksFloating, MaxRockGapCm, Clinging, Moved);
}

EMOCompositionPiece FMOCompositionGrounding::Classify(const UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return EMOCompositionPiece::Ignore;
	}
	if (Mesh->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/")))
	{
		return EMOCompositionPiece::Ignore;
	}
	const FString Name = Mesh->GetName().ToLower();
	// Clinging first: "SM_CliffVine_08" is a vine, not a cliff; "SM_EuropeanHornbeam_..._Vine_00" is the ivy on a tree.
	if (NameHasAny(Name, { TEXT("vine"), TEXT("ivy"), TEXT("moss"), TEXT("creeper"), TEXT("lichen"), TEXT("decal") }))
	{
		return EMOCompositionPiece::Clinging;
	}
	if (NameHasAny(Name, { TEXT("tree"), TEXT("hornbeam"), TEXT("beech"), TEXT("alder"), TEXT("birch"), TEXT("oak"), TEXT("pine"), TEXT("spruce"), TEXT("fir_"),
		TEXT("maple"), TEXT("willow"), TEXT("hazel"), TEXT("holly"), TEXT("fern"), TEXT("bush"), TEXT("shrub"), TEXT("plant"), TEXT("grass"), TEXT("flower"),
		TEXT("sapling"), TEXT("bramble"), TEXT("nettle"), TEXT("weed"), TEXT("stump"), TEXT("log_"), TEXT("_log"), TEXT("branch"), TEXT("twig"), TEXT("mushroom"),
		TEXT("foliage"), TEXT("reed"), TEXT("bracken") }))
	{
		return EMOCompositionPiece::Foliage;
	}
	return EMOCompositionPiece::Rock;
}

FMOCompositionGroundingStats FMOCompositionGrounding::Ground(AActor* Owner, const TFunctionRef<float(double X, double Y)>& GroundZ, bool bApply)
{
	FMOCompositionGroundingStats Stats;
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		return Stats;
	}

	// Every visible generated instance, by kind.
	TArray<FPiece> Pieces;
	TSet<const UPrimitiveComponent*> RockComponents;
	TSet<UInstancedStaticMeshComponent*> Touched;
	TInlineComponentArray<UInstancedStaticMeshComponent*> Isms(Owner);
	for (UInstancedStaticMeshComponent* Ism : Isms)
	{
		const UStaticMesh* Mesh = Ism ? Ism->GetStaticMesh() : nullptr;
		const EMOCompositionPiece Kind = FMOCompositionGrounding::Classify(Mesh);
		if (!Mesh || Kind == EMOCompositionPiece::Ignore || !Ism->IsVisible())
		{
			continue;
		}
		if (Kind == EMOCompositionPiece::Rock && Ism->IsCollisionEnabled())
		{
			RockComponents.Add(Ism);
		}
		const FBox MeshBox = Mesh->GetBounds().GetBox();
		for (int32 I = 0; I < Ism->GetInstanceCount(); ++I)
		{
			FPiece& P = Pieces.AddDefaulted_GetRef();
			P.Ism = Ism;
			P.Index = I;
			P.Kind = Kind;
			Ism->GetInstanceTransform(I, P.World, true);
			P.Bounds = MeshBox.TransformBy(P.World);
		}
	}

	auto Move = [&Stats, &Touched](FPiece& P, const FVector& Delta)
	{
		P.World.AddToTranslation(Delta);
		P.Bounds = P.Bounds.ShiftBy(Delta);
		P.Moved += Delta;
		P.Ism->UpdateInstanceTransform(P.Index, P.World, true, false, true);
		Touched.Add(P.Ism);
		++Stats.Moved;
	};

	// 1. Rocks: the gap between each one's underside and what is under it (the terrain, or a rock of the composition it rests on), at five points of
	//    its footprint. A point outside the rock's real shape (a corner of its box) finds no underside and does not count.
	for (FPiece& P : Pieces)
	{
		if (P.Kind != EMOCompositionPiece::Rock)
		{
			continue;
		}
		++Stats.Rocks;
		const FVector C = P.Bounds.GetCenter();
		const FVector E = P.Bounds.GetExtent();
		float WorstGap = -1.0f;
		for (const FVector2D& K : { FVector2D(0.0, 0.0), FVector2D(0.35, 0.0), FVector2D(-0.35, 0.0), FVector2D(0.0, 0.35), FVector2D(0.0, -0.35) })
		{
			const double X = C.X + K.X * 2.0 * E.X, Y = C.Y + K.Y * 2.0 * E.Y;
			const float TerrainZ = GroundZ(X, Y);
			if (TerrainZ > P.Bounds.Max.Z)
			{
				continue;   // buried right through here
			}
			FHitResult Underside;
			if (!TraceRocks(World, RockComponents, FVector(X, Y, TerrainZ - 10.0), FVector(X, Y, P.Bounds.Max.Z + 10.0), Underside) || Underside.bStartPenetrating)
			{
				continue;
			}
			float Support = TerrainZ;
			FHitResult Below;
			if (TraceRocks(World, RockComponents, Underside.ImpactPoint - FVector(0.0, 0.0, 2.0), FVector(X, Y, TerrainZ - 10.0), Below))
			{
				Support = FMath::Max(Support, static_cast<float>(Below.ImpactPoint.Z));   // resting on another rock of the set
			}
			WorstGap = FMath::Max(WorstGap, static_cast<float>(Underside.ImpactPoint.Z) - Support);
		}
		if (WorstGap > RockGapToleranceCm)
		{
			++Stats.RocksFloating;
			Stats.MaxRockGapCm = FMath::Max(Stats.MaxRockGapCm, WorstGap);
			if (bApply)
			{
				const float Sink = FMath::Min(WorstGap + RockSinkCm * P.World.GetScale3D().Z, RockMaxSinkFraction * 2.0f * E.Z);
				Move(P, FVector(0.0, 0.0, -Sink));
			}
		}
	}

	// 2. Plants: stand on what is under the base -- a rock of the composition (authored on a ledge), else the terrain. Seen from just above the base, so a
	//    base buried up to BuryDetectCm comes up too.
	for (FPiece& P : Pieces)
	{
		if (P.Kind != EMOCompositionPiece::Foliage)
		{
			continue;
		}
		++Stats.Foliage;
		const FVector Base = P.World.GetLocation();
		const float Scale = P.World.GetScale3D().Z;
		float Support = GroundZ(Base.X, Base.Y);
		FHitResult Ledge;
		if (TraceRocks(World, RockComponents, Base + FVector(0.0, 0.0, BuryDetectCm), Base - FVector(0.0, 0.0, 5000.0), Ledge))
		{
			Support = FMath::Max(Support, static_cast<float>(Ledge.ImpactPoint.Z));
		}
		const float Gap = static_cast<float>(Base.Z) - Support;
		const bool bFloating = Gap > FloatToleranceCm;
		const bool bBuried = -Gap > BuryToleranceCm && -Gap <= BuryDetectCm;
		if (bFloating)
		{
			++Stats.FoliageFloating;
			Stats.MaxFoliageFloatCm = FMath::Max(Stats.MaxFoliageFloatCm, Gap);
		}
		if (bBuried)
		{
			++Stats.FoliageBuried;
			Stats.MaxFoliageBuryCm = FMath::Max(Stats.MaxFoliageBuryCm, -Gap);
		}
		if (bApply && (bFloating || bBuried))
		{
			Move(P, FVector(0.0, 0.0, Support - FoliageSinkCm * Scale - Base.Z));
		}
	}

	// 3. Clinging pieces go with what they hang on: the moved piece whose pivot they share (a tree's ivy) or whose box holds them (a cliff's vine).
	for (FPiece& P : Pieces)
	{
		if (P.Kind != EMOCompositionPiece::Clinging)
		{
			continue;
		}
		++Stats.Clinging;
		const FVector Pivot = P.World.GetLocation();
		const FPiece* Host = nullptr;
		double Best = TNumericLimits<double>::Max();
		for (const FPiece& Q : Pieces)
		{
			if (Q.Kind == EMOCompositionPiece::Clinging || Q.Moved.IsNearlyZero())
			{
				continue;
			}
			const double Shared = FVector::Dist(Q.World.GetLocation() - Q.Moved, Pivot);
			const double Score = Shared < 50.0 ? Shared : (Q.Bounds.ShiftBy(-Q.Moved).IsInsideOrOn(Pivot) ? 1000.0 + Shared : TNumericLimits<double>::Max());
			if (Score < Best)
			{
				Best = Score;
				Host = &Q;
			}
		}
		if (bApply && Host)
		{
			Move(P, Host->Moved);
		}
	}

	for (UInstancedStaticMeshComponent* Ism : Touched)
	{
		Ism->MarkRenderStateDirty();
	}
	return Stats;
}
