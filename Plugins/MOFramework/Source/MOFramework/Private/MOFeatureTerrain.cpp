#include "MOFeatureTerrain.h"
#include "MOWorldFeatureSubsystem.h"
#include "MOFramework.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/ScopeLock.h"
#include <atomic>

namespace
{
	/** Sites are found per square tile of this edge (cm) on first use, then kept. */
	constexpr double TileCm = 100000.0;
	/** Sites are looked for this far past a tile's edge: an authored ground grid may reach beyond the row's footprint radius. */
	constexpr double TileMarginCm = 5000.0;

	struct FShapingSite
	{
		FTransform Root;
		FTransform InvRoot;
		const FMOWorldFeatureGround* Ground = nullptr;   // in the index's placer (lives as long as the index)
		FBox2D WorldBounds = FBox2D(ForceInit);
	};

	struct FTile
	{
		TArray<FShapingSite> Sites;
	};

	struct FIndex
	{
		uint32 ParamsHash = 0;
		int32 Seed = 0;
		uint32 Serial = 0;
		TUniquePtr<FMOWorldFeaturePlacer> Placer;   // touched only under BuildLock (its site cache is not thread-safe)
		FCriticalSection BuildLock;
		FRWLock TilesLock;
		TMap<FIntPoint, TSharedPtr<const FTile, ESPMode::ThreadSafe>> Tiles;
	};
	using FIndexPtr = TSharedPtr<FIndex, ESPMode::ThreadSafe>;
	using FTilePtr = TSharedPtr<const FTile, ESPMode::ThreadSafe>;

	FRWLock GLock;   // GCatalog, GIndex
	TArray<TPair<FName, FMOWorldFeatureDefinitionRow>> GCatalog;
	FIndexPtr GIndex;
	std::atomic<uint32> GSerial{0};
	std::atomic<bool> GHasShaping{false};

	FIndexPtr GetIndex(const FMOWorldGenParamsRef& Params, int32 Seed)
	{
		const uint32 Hash = Params->GetHash();
		const uint32 Serial = GSerial.load();
		{
			FReadScopeLock Read(GLock);
			if (GIndex && GIndex->ParamsHash == Hash && GIndex->Seed == Seed && GIndex->Serial == Serial)
			{
				return GIndex;
			}
		}
		FWriteScopeLock Write(GLock);
		if (GIndex && GIndex->ParamsHash == Hash && GIndex->Seed == Seed && GIndex->Serial == Serial)
		{
			return GIndex;
		}
		FIndexPtr Index = MakeShared<FIndex, ESPMode::ThreadSafe>();
		Index->ParamsHash = Hash;
		Index->Seed = Seed;
		Index->Serial = Serial;
		Index->Placer = MakeUnique<FMOWorldFeaturePlacer>(Params, Seed, GCatalog);
		GIndex = Index;
		UE_LOG(LogMOFramework, Log, TEXT("[MOFeatureTerrain] site index for terrain seed %d (%d catalog row(s))"), Seed, GCatalog.Num());
		return Index;
	}

	FTilePtr FindOrBuildTile(FIndex& Index, const FIntPoint& Key)
	{
		{
			FReadScopeLock Read(Index.TilesLock);
			if (const FTilePtr* Found = Index.Tiles.Find(Key))
			{
				return *Found;
			}
		}
		FScopeLock Build(&Index.BuildLock);
		{
			FReadScopeLock Read(Index.TilesLock);
			if (const FTilePtr* Found = Index.Tiles.Find(Key))
			{
				return *Found;
			}
		}
		TSharedPtr<FTile, ESPMode::ThreadSafe> Tile = MakeShared<FTile, ESPMode::ThreadSafe>();
		const FBox2D TileBox(FVector2D(Key.X * TileCm, Key.Y * TileCm), FVector2D((Key.X + 1) * TileCm, (Key.Y + 1) * TileCm));
		TArray<FMOWorldFeatureSite> Sites;
		Index.Placer->SitesTouching(TileBox.ExpandBy(TileMarginCm), Sites);
		const TArray<FMOWorldFeaturePlacer::FFeature>& Features = Index.Placer->GetFeatures();
		for (const FMOWorldFeatureSite& Site : Sites)
		{
			const int32 F = Index.Placer->GetFeatureIndex(Site.FeatureId);
			if (!Features.IsValidIndex(F) || !Features[F].Row.AuthoredGround.IsSet())
			{
				continue;
			}
			const FMOWorldFeatureDefinitionRow& Row = Features[F].Row;
			const FBox2D Grid = Row.AuthoredGround.GetBounds();
			for (const FMOWorldFeatureMember& Member : Site.Members)
			{
				FShapingSite S;
				S.Root = FMOFeatureTerrain::MemberRootTransform(Row, Member);
				S.InvRoot = S.Root.Inverse();
				S.Ground = &Row.AuthoredGround;
				for (const FVector2D& Corner : { Grid.Min, Grid.Max, FVector2D(Grid.Min.X, Grid.Max.Y), FVector2D(Grid.Max.X, Grid.Min.Y) })
				{
					S.WorldBounds += FVector2D(S.Root.TransformPosition(FVector(Corner, 0.0)));
				}
				if (S.WorldBounds.Intersect(TileBox))
				{
					Tile->Sites.Add(S);
				}
			}
		}
		FWriteScopeLock Write(Index.TilesLock);
		Index.Tiles.Add(Key, Tile);
		if (Tile->Sites.Num() > 0)
		{
			// DEBUG (MOFeatureTerrain, disabled Oct 18): re-enable to trace every feature tile
			// UE_LOG(LogMOFramework, Log, TEXT("[MOFeatureTerrain] tile (%d, %d): %d composition(s) shape the ground"), Key.X, Key.Y, Tile->Sites.Num());
		}
		return Tile;
	}
}

void FMOFeatureTerrain::SetCatalog(const TArray<TPair<FName, FMOWorldFeatureDefinitionRow>>& Rows)
{
	FWriteScopeLock Write(GLock);
	GCatalog = Rows;
	GIndex.Reset();
	bool bShaping = false;
	for (const TPair<FName, FMOWorldFeatureDefinitionRow>& Row : Rows)
	{
		bShaping |= Row.Value.AuthoredGround.IsSet();
	}
	GHasShaping.store(bShaping);
	GSerial.fetch_add(1);
}

bool FMOFeatureTerrain::HasShapingRows()
{
	return GHasShaping.load();
}

FTransform FMOFeatureTerrain::MemberRootTransform(const FMOWorldFeatureDefinitionRow& Row, const FMOWorldFeatureMember& Member)
{
	// The sink grows with the composition: a gap under a 4x outcrop is 4x the gap under the authored one. On a slope it also goes down to the
	// lowest ground round its core (SlopeSinkCm).
	return FTransform(FRotator(0.0f, Member.YawDegrees, 0.0f), Member.Location - FVector(0.0, 0.0, Row.SinkCm * Member.Scale + Member.SlopeSinkCm), FVector(Member.Scale));
}

float FMOFeatureTerrain::Apply(const FMOWorldGenParamsRef& Params, int32 TerrainSeed, double X, double Y, float BaseHeightCm)
{
	if (!GHasShaping.load(std::memory_order_relaxed))
	{
		return BaseHeightCm;
	}
	// A voxel chunk asks for thousands of columns of the same tile: the last index and tile per thread skip every lock.
	struct FLast
	{
		FIndexPtr Index;
		FTilePtr Tile;
		FIntPoint Key = FIntPoint(MAX_int32, MAX_int32);
		uint32 Hash = 0;
		int32 Seed = 0;
		uint32 Serial = MAX_uint32;
	};
	thread_local FLast Last;
	const uint32 Hash = Params->GetHash();
	const uint32 Serial = GSerial.load(std::memory_order_relaxed);
	if (!Last.Index || Last.Hash != Hash || Last.Seed != TerrainSeed || Last.Serial != Serial)
	{
		Last.Index = GetIndex(Params, TerrainSeed);
		Last.Tile.Reset();
		Last.Key = FIntPoint(MAX_int32, MAX_int32);
		Last.Hash = Hash;
		Last.Seed = TerrainSeed;
		Last.Serial = Serial;
	}
	const FIntPoint Key(FMath::FloorToInt32(X / TileCm), FMath::FloorToInt32(Y / TileCm));
	if (!Last.Tile || Last.Key != Key)
	{
		Last.Tile = FindOrBuildTile(*Last.Index, Key);
		Last.Key = Key;
	}

	float Result = BaseHeightCm;
	const FVector2D P(X, Y);
	for (const FShapingSite& S : Last.Tile->Sites)
	{
		if (!S.WorldBounds.IsInsideOrOn(P))
		{
			continue;
		}
		const FVector Local = S.InvRoot.TransformPosition(FVector(X, Y, 0.0));
		float Height = 0.0f, Weight = 0.0f;
		if (!S.Ground->Sample(Local.X, Local.Y, Height, Weight) || Weight <= 0.0f)
		{
			continue;
		}
		const float Target = static_cast<float>(S.Root.TransformPosition(FVector(Local.X, Local.Y, Height)).Z);
		if (Target > BaseHeightCm)
		{
			Result = FMath::Max(Result, BaseHeightCm + (Target - BaseHeightCm) * FMath::Clamp(Weight, 0.0f, 1.0f));
		}
	}
	return Result;
}
