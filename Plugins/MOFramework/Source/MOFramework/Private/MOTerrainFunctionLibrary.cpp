#include "MOTerrainFunctionLibrary.h"
#include "MOFramework.h"
#include "VoxelGraphQuery.h"
#include "Surface/VoxelSurfaceTypeInterface.h"
#include "Surface/VoxelSurfaceTypeBlendBuilder.h"
#include "Misc/ScopeRWLock.h"

VOXEL_REGISTER_FUNCTION(UMOTerrainFunctionLibrary, MOTerrain);

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
		TArray<bool> bHasSurface;
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
		FVoxelSurfaceTypeBlendBuilder Builder;
		Builder.AddLayer(FVoxelSurfaceType(Surface), 1.0f);
		Builder.Build(Table->BiomeBlends[I]);
		Table->bHasSurface[I] = true;
		++NumSurfaces;
	}

	FWriteScopeLock Lock(GSurfaceLock);
	GSurfaceTable = Table;
	UE_LOG(LogMOFramework, Log, TEXT("[MOTerrain] Surface table: %d of %d biomes have a ground surface"), NumSurfaces, Params.Biomes.Num());
}

void UMOTerrainFunctionLibrary::MOTerrain(
	const FVoxelDoubleVector2DBuffer& WorldPosition,
	const FVoxelSeed& Seed,
	FVoxelFloatBuffer& Height,
	FVoxelFloatBuffer& BiomeIndex,
	FVoxelFloatBuffer& Moisture,
	FVoxelFloatBuffer& Temperature,
	FVoxelSurfaceTypeBlendBuffer& SurfaceType,
	FVoxelBox2D& Bounds,
	FVoxelFloatRange& HeightRange) const
{
	const FVoxelGraphParameters::FLOD* LODParameter = Query->FindParameter<FVoxelGraphParameters::FLOD>();
	Compute(WorldPosition, Seed, LODParameter ? LODParameter->Value : 0, Height, BiomeIndex, Moisture, Temperature, SurfaceType, Bounds, HeightRange);
}

void UMOTerrainFunctionLibrary::Compute(
	const FVoxelDoubleVector2DBuffer& WorldPosition,
	const int32 Seed,
	const int32 LOD,
	FVoxelFloatBuffer& Height,
	FVoxelFloatBuffer& BiomeIndex,
	FVoxelFloatBuffer& Moisture,
	FVoxelFloatBuffer& Temperature,
	FVoxelSurfaceTypeBlendBuffer& SurfaceType,
	FVoxelBox2D& Bounds,
	FVoxelFloatRange& HeightRange)
{
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	const FMOWorldGenParams& P = *Params;

	TSharedPtr<const FSurfaceTable, ESPMode::ThreadSafe> Surfaces;
	{
		FReadScopeLock Lock(GSurfaceLock);
		Surfaces = GSurfaceTable;
	}
	// A table built for a different snapshot is ignored (biome indices would not line up).
	const bool bUseSurfaces = Surfaces.IsValid() && Surfaces->ParamsHash == P.GetHash() && Surfaces->BiomeBlends.Num() == P.Biomes.Num();

	const int32 Num = WorldPosition.Num();
	const float StepCm = P.Tuning.VoxelSizeCm * (float)(1 << FMath::Clamp(LOD, 0, 24));

	Height.Allocate(Num);
	BiomeIndex.Allocate(Num);
	Moisture.Allocate(Num);
	Temperature.Allocate(Num);
	SurfaceType.Allocate(Num);

	const FVoxelSurfaceTypeBlend EmptyBlend;
	for (int32 I = 0; I < Num; ++I)
	{
		const FMOTerrainSample S = FMOWorldGen::SampleColumn(P, WorldPosition.X[I], WorldPosition.Y[I], Seed, StepCm);
		Height.Set(I, S.HeightCm);
		BiomeIndex.Set(I, (float)S.BiomeIndex);
		Moisture.Set(I, S.Moisture);
		Temperature.Set(I, S.Temperature);
		SurfaceType.Set(I, (bUseSurfaces && S.BiomeIndex != INDEX_NONE) ? Surfaces->BiomeBlends[S.BiomeIndex] : EmptyBlend);
	}

	const float Half = P.Tuning.WorldHalfSizeCm;
	Bounds = FVoxelBox2D(FVector2D(-Half, -Half), FVector2D(Half, Half));
	HeightRange = FVoxelFloatRange(P.MinHeightCm, P.MaxHeightCm);
}
