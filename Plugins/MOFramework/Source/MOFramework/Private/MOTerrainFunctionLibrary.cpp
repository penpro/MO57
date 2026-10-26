#include "MOTerrainFunctionLibrary.h"
#include "MOFeatureTerrain.h"
#include "MOFramework.h"
#include "VoxelGraphQuery.h"
#include "Surface/VoxelSurfaceTypeInterface.h"
#include "Surface/VoxelSurfaceTypeBlendBuilder.h"
#include "Misc/ScopeRWLock.h"

VOXEL_REGISTER_FUNCTION(UMOTerrainFunctionLibrary, MOTerrain);
VOXEL_REGISTER_FUNCTION(UMOTerrainFunctionLibrary, MOTerrainBounds);
VOXEL_REGISTER_FUNCTION(UMOTerrainFunctionLibrary, MOTerrainCaves);
VOXEL_REGISTER_FUNCTION(UMOTerrainFunctionLibrary, MOTerrainCavesBounds);

namespace
{
	/**
	 * Per-biome ready-made surface blends, built on the game thread (FVoxelSurfaceType construction registers the asset) and read on worker
	 * threads. Indexed like FMOWorldGenParams::Biomes of the snapshot it was built from; Hash ties it to that snapshot so a stale table
	 * can never be applied to a different biome list.
	 */
	struct FSurfaceTable
	{
		uint32 ParamsHash = 0;
		TArray<FVoxelSurfaceTypeBlend> BiomeBlends;   // NumLayers == 0 => the biome paints nothing
		TArray<FVoxelSurfaceType> BiomeSurfaces;      // the same surfaces as types, for mixing biomes at a soft boundary (FMOWorldGen::SampleSurfaceBiomeWeights)
		TArray<bool> bHasSurface;
		FSoftObjectPath LakeBedPath;                  // the path the lake-bed blend was built from (a changed setting is noticed)
		FVoxelSurfaceTypeBlend LakeBedBlend;
		bool bHasLakeBed = false;
	};

	FRWLock GSurfaceLock;
	TSharedPtr<const FSurfaceTable, ESPMode::ThreadSafe> GSurfaceTable;
}

void UMOTerrainFunctionLibrary::RefreshSurfaceTable(const FMOWorldGenParams& Params)
{
	check(IsInGameThread());

	TSharedRef<FSurfaceTable, ESPMode::ThreadSafe> Table = MakeShared<FSurfaceTable, ESPMode::ThreadSafe>();
	Table->ParamsHash = Params.GetHash();
	Table->BiomeBlends.SetNum(Params.Biomes.Num());
	Table->BiomeSurfaces.SetNum(Params.Biomes.Num());
	Table->bHasSurface.Init(false, Params.Biomes.Num());

	int32 NumSurfaces = 0;
	for (int32 I = 0; I < Params.Biomes.Num(); ++I)
	{
		const FSoftObjectPath& Path = Params.Biomes[I].GroundSurfaceType;
		if (Path.IsNull())
		{
			continue;
		}
		UVoxelSurfaceTypeInterface* Surface = Cast<UVoxelSurfaceTypeInterface>(Path.TryLoad());
		if (!Surface)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTerrain] Biome '%s': GroundSurfaceType '%s' is not a UVoxelSurfaceTypeInterface asset"),
				*Params.Biomes[I].Id.ToString(), *Path.ToString());
			continue;
		}
		Table->BiomeSurfaces[I] = FVoxelSurfaceType(Surface);
		FVoxelSurfaceTypeBlendBuilder Builder;
		Builder.AddLayer(Table->BiomeSurfaces[I], 1.0f);
		Builder.Build(Table->BiomeBlends[I]);
		Table->bHasSurface[I] = true;
		++NumSurfaces;
	}

	// The bed of the upland lakes (see FMOWorldGen::UplandLakeBedWeight).
	Table->LakeBedPath = Params.UplandLakeBedSurfaceType;
	if (!Params.UplandLakeBedSurfaceType.IsNull())
	{
		if (UVoxelSurfaceTypeInterface* Surface = Cast<UVoxelSurfaceTypeInterface>(Params.UplandLakeBedSurfaceType.TryLoad()))
		{
			FVoxelSurfaceTypeBlendBuilder Builder;
			Builder.AddLayer(FVoxelSurfaceType(Surface), 1.0f);
			Builder.Build(Table->LakeBedBlend);
			Table->bHasLakeBed = true;
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTerrain] UplandLakeBedSurfaceType '%s' is not a UVoxelSurfaceTypeInterface asset: lakes keep the biome ground"), *Params.UplandLakeBedSurfaceType.ToString());
		}
	}

	FWriteScopeLock Lock(GSurfaceLock);
	GSurfaceTable = Table;
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerrain] Surface table: %d of %d biomes have a ground surface; upland lake bed surface: %s"), NumSurfaces, Params.Biomes.Num(), Table->bHasLakeBed ? *Params.UplandLakeBedSurfaceType.ToString() : TEXT("none"));
}

void UMOTerrainFunctionLibrary::MOTerrain(
	const FVoxelDoubleVector2DBuffer& WorldPosition,
	const FVoxelSeed& Seed,
	FVoxelFloatBuffer& Height,
	FVoxelFloatBuffer& BiomeIndex,
	FVoxelFloatBuffer& Moisture,
	FVoxelFloatBuffer& Temperature,
	FVoxelSurfaceTypeBlendBuffer& SurfaceType) const
{
	const FVoxelGraphParameters::FLOD* LODParameter = Query->FindParameter<FVoxelGraphParameters::FLOD>();
	Compute(WorldPosition, Seed, LODParameter ? LODParameter->Value : 0, Height, BiomeIndex, Moisture, Temperature, SurfaceType);
}

void UMOTerrainFunctionLibrary::MOTerrainBounds(FVoxelBox2D& Bounds, FVoxelFloatRange& HeightRange) const
{
	ComputeBounds(Bounds, HeightRange);
}

void UMOTerrainFunctionLibrary::MOTerrainCaves(
	const FVoxelDoubleVectorBuffer& WorldPosition,
	const FVoxelSeed& Seed,
	FVoxelFloatBuffer& Distance) const
{
	const FVoxelGraphParameters::FLOD* LODParameter = Query->FindParameter<FVoxelGraphParameters::FLOD>();
	ComputeCaves(WorldPosition, Seed, LODParameter ? LODParameter->Value : 0, Distance);
}

void UMOTerrainFunctionLibrary::MOTerrainCavesBounds(FVoxelBox& Bounds) const
{
	ComputeCavesBounds(Bounds);
}

void UMOTerrainFunctionLibrary::ComputeCavesBounds(FVoxelBox& Bounds)
{
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	const double Half = Params->Tuning.WorldHalfSizeCm;
	const FVector2D C = Params->GetWorldCentre();   // the island in WORLD space (its centre is off the origin when the start was moved there)
	Bounds = FVoxelBox(FVector(C.X - Half, C.Y - Half, Params->CaveMinZCm), FVector(C.X + Half, C.Y + Half, Params->CaveMaxZCm));
}

void UMOTerrainFunctionLibrary::ComputeCaves(
	const FVoxelDoubleVectorBuffer& WorldPosition,
	const int32 Seed,
	const int32 LOD,
	FVoxelFloatBuffer& Distance)
{
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	const FMOWorldGenParams& P = *Params;

	const int32 Num = WorldPosition.Num();
	const float StepCm = P.Tuning.VoxelSizeCm * (float)(1 << FMath::Clamp(LOD, 0, 24));
	Distance.Allocate(Num);

	// A chunk is a 3D grid: the same (X, Y) column repeats once per Z layer and the ground height (~2 us with the water pass) is the same for all of them.
	// Memoised per DISTINCT column (exact XY bits), so each column's ground is computed once per call whatever the sample order. The direct-mapped
	// 4096-slot table this replaced recomputed it on hash collisions between a chunk's own columns, every Z layer.
	TMap<TTuple<uint64, uint64>, float> Grounds;
	Grounds.Reserve(FMath::Min(Num, 8192));
	uint64 LastBitsX = 0, LastBitsY = 0;
	float LastGround = 0.0f;
	bool bHaveLast = false;

	for (int32 I = 0; I < Num; ++I)
	{
		const double X = WorldPosition.X[I], Y = WorldPosition.Y[I], Z = WorldPosition.Z[I];
		if (Z > P.CaveMaxZCm || Z < P.CaveMinZCm)
		{
			Distance.Set(I, FMOWorldGen::CaveFarCm);   // outside the band caves can occupy: skip even the height sample
			continue;
		}

		uint64 BitsX, BitsY;
		FMemory::Memcpy(&BitsX, &X, sizeof(BitsX));
		FMemory::Memcpy(&BitsY, &Y, sizeof(BitsY));
		if (!bHaveLast || BitsX != LastBitsX || BitsY != LastBitsY)   // consecutive samples of one column (Z innermost) skip the map
		{
			const TTuple<uint64, uint64> Key(BitsX, BitsY);
			if (const float* Found = Grounds.Find(Key))
			{
				LastGround = *Found;
			}
			else
			{
				LastGround = FMOWorldGen::SampleHeightCm(P, X, Y, Seed, StepCm);
				Grounds.Add(Key, LastGround);
			}
			LastBitsX = BitsX;
			LastBitsY = BitsY;
			bHaveLast = true;
		}
		Distance.Set(I, FMOWorldGen::SampleCaveDistanceAtGround(P, X, Y, Z, LastGround, Seed, StepCm));
	}
}

void UMOTerrainFunctionLibrary::ComputeBounds(FVoxelBox2D& Bounds, FVoxelFloatRange& HeightRange)
{
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	const double Half = Params->Tuning.WorldHalfSizeCm;
	const FVector2D C = Params->GetWorldCentre();   // the island in WORLD space (its centre is off the origin when the start was moved there)
	Bounds = FVoxelBox2D(FVector2D(C.X - Half, C.Y - Half), FVector2D(C.X + Half, C.Y + Half));
	HeightRange = FVoxelFloatRange(Params->MinHeightCm, Params->MaxHeightCm);
}

void UMOTerrainFunctionLibrary::Compute(
	const FVoxelDoubleVector2DBuffer& WorldPosition,
	const int32 Seed,
	const int32 LOD,
	FVoxelFloatBuffer& Height,
	FVoxelFloatBuffer& BiomeIndex,
	FVoxelFloatBuffer& Moisture,
	FVoxelFloatBuffer& Temperature,
	FVoxelSurfaceTypeBlendBuffer& SurfaceType)
{
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	const FMOWorldGenParams& P = *Params;

	TSharedPtr<const FSurfaceTable, ESPMode::ThreadSafe> Surfaces;
	{
		FReadScopeLock Lock(GSurfaceLock);
		Surfaces = GSurfaceTable;
	}
	// A table built for a different snapshot is ignored (biome indices would not line up).
	const int32 Num = WorldPosition.Num();
	const float StepCm = P.Tuning.VoxelSizeCm * (float)(1 << FMath::Clamp(LOD, 0, 24));

	const bool bUseSurfaces = Surfaces.IsValid() && Surfaces->ParamsHash == P.GetHash() && Surfaces->BiomeBlends.Num() == P.Biomes.Num();
	// The bed is painted only where the height carves the bowl: a LOD too coarse for upland lakes has neither (and the bed lookup is the same per-cell lake search).
	const bool bLakeBeds = bUseSurfaces && Surfaces->bHasLakeBed && Surfaces->LakeBedPath == P.UplandLakeBedSurfaceType && P.Tuning.bEnableWater && P.Tuning.bEnableUplandLakes
		&& FMOWorldGen::IsWaterTierResolved(P, 2, StepCm);

	Height.Allocate(Num);
	BiomeIndex.Allocate(Num);
	Moisture.Allocate(Num);
	Temperature.Allocate(Num);
	SurfaceType.Allocate(Num);

	const FVoxelSurfaceTypeBlend EmptyBlend;
	FVoxelSurfaceTypeBlendBuilder Mix;
	for (int32 I = 0; I < Num; ++I)
	{
		const FMOTerrainSample S = FMOWorldGen::SampleColumn(P, WorldPosition.X[I], WorldPosition.Y[I], Seed, StepCm);
		// Raised to the authored ground of a composition placed here (a cliff assembly's plateau): the landform it was made for (FMOFeatureTerrain).
		Height.Set(I, FMOFeatureTerrain::Apply(Params, Seed, WorldPosition.X[I], WorldPosition.Y[I], S.HeightCm));
		BiomeIndex.Set(I, (float)S.BiomeIndex);
		Moisture.Set(I, S.Moisture);
		Temperature.Set(I, S.Temperature);
		if (bLakeBeds && FMOWorldGen::UplandLakeBedWeight(P, WorldPosition.X[I], WorldPosition.Y[I], Seed) >= 0.5f)
		{
			SurfaceType.Set(I, Surfaces->LakeBedBlend);   // the bowl and its damp rim: rock / mud, not the forest floor
			continue;
		}
		if (!bUseSurfaces)
		{
			SurfaceType.Set(I, EmptyBlend);
			continue;
		}
		// Soft biome boundaries (FMOWorldGen::SampleSurfaceBiomeWeights / SoftThreshold): inside a biome one weight -> its ready-made blend; in a boundary band the
		// biomes' surfaces mixed by weight, so the ground fades over the band with a noisy edge instead of switching at a line.
		FMOWorldGen::FBiomeWeight Weights[3];
		const int32 NumWeights = FMOWorldGen::SampleSurfaceBiomeWeights(P, WorldPosition.X[I], WorldPosition.Y[I], Seed, S.HeightCm, S.Moisture, S.Temperature, Weights, 3);
		if (NumWeights == 1 || (NumWeights > 1 && Weights[0].Weight >= 0.999f))
		{
			SurfaceType.Set(I, Surfaces->BiomeBlends[Weights[0].Biome]);
			continue;
		}
		if (NumWeights == 0)
		{
			SurfaceType.Set(I, S.BiomeIndex != INDEX_NONE ? Surfaces->BiomeBlends[S.BiomeIndex] : EmptyBlend);
			continue;
		}
		Mix.Reset();
		for (int32 K = 0; K < NumWeights; ++K)
		{
			if (Surfaces->bHasSurface[Weights[K].Biome])
			{
				Mix.AddLayer(Surfaces->BiomeSurfaces[Weights[K].Biome], Weights[K].Weight);
			}
		}
		FVoxelSurfaceTypeBlend Blend;
		Mix.Build(Blend);
		SurfaceType.Set(I, Blend);
	}

}
