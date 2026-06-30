// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Scatter/VoxelScatterCacheManager.h"
#include "Scatter/VoxelScatterFunctionLibrary.h"
#include "VoxelQuery.h"
#include "VoxelDependency.h"
#include "Graphs/VoxelStampGraphParameters.h"

VOXEL_CONSOLE_VARIABLE(
	VOXEL_API, bool, GVoxelScatterCacheDebug, false,
	"voxel.scatter.cache.Debug",
	"Enables cache debugging, which will always compute input and compare it with cached one.");

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelScatterCacheManager::FVoxelScatterCacheManager()
	: Dependency(FVoxelDependency3D::Create("VoxelScatterCacheManager"))
{
}

TSharedRef<const FVoxelPointSet> FVoxelScatterCacheManager::GetCachedChunk(
	const FVoxelGraphEnvironment& Environment,
	const FVoxelCompiledTerminalGraph& TerminalGraph,
	FVoxelDependencyCollector& DependencyCollector,
	FVoxelInvalidationQueue* InvalidationQueue,
	const FVoxelLayers& Layers,
	const FVoxelSurfaceTypeTable& SurfaceTypeTable,
	const TVoxelObjectPtr<AVoxelScatterActor>& WeakActor,
	const float BoundsExtension,
	const TVoxelSet<FName>& AttributesToKeep,
	const FVoxelBox& ChunkBounds,
	const FGuid NodeGuid,
	const uint32 ChunkKey,
	const FVoxelNode::TPinRef_Input<FVoxelPointSet>& Pin,
	bool& bOutWasCached)
{
	DependencyCollector.AddDependency(*Dependency, ChunkBounds.Extend(BoundsExtension));

	bOutWasCached = false;
	if (const TSharedPtr<const FVoxelPointSet> Points = FindCachedChunk(
		WeakActor,
		NodeGuid,
		ChunkKey))
	{
		bOutWasCached = true;
		if (GVoxelScatterCacheDebug)
		{
			TSharedPtr<const FVoxelPointSet> ComputedPoints;
			TSharedPtr<FVoxelDependencyTracker> DependencyTracker;
			ComputePoints(
				Environment,
				TerminalGraph,
				InvalidationQueue,
				Layers,
				SurfaceTypeTable,
				WeakActor,
				NodeGuid,
				ChunkKey,
				ChunkBounds,
				BoundsExtension,
				AttributesToKeep,
				Pin,
				ComputedPoints,
				DependencyTracker);

			ensure(Points->Equals(*ComputedPoints));
		}

		return Points.ToSharedRef();
	}

	TSharedPtr<const FVoxelPointSet> Points;
	TSharedPtr<FVoxelDependencyTracker> DependencyTracker;
	ComputePoints(
		Environment,
		TerminalGraph,
		InvalidationQueue,
		Layers,
		SurfaceTypeTable,
		WeakActor,
		NodeGuid,
		ChunkKey,
		ChunkBounds,
		BoundsExtension,
		AttributesToKeep,
		Pin,
		Points,
		DependencyTracker);

	StorePoints(
		WeakActor,
		NodeGuid,
		ChunkKey,
		Points.ToSharedRef(),
		DependencyTracker.ToSharedRef());

	return Points.ToSharedRef();
}

void FVoxelScatterCacheManager::ClearOldCache(
	const TVoxelObjectPtr<AVoxelScatterActor>& WeakActor,
	const FGuid NodeGuid,
	const float CacheSize)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	TVoxelMap<FGuid, FCache>* NodeGuidToCache = ActorToNodeGuidToCache_RequiresLock.Find(WeakActor);
	if (!NodeGuidToCache)
	{
		return;
	}

	FCache* Cache = NodeGuidToCache->Find(NodeGuid);
	if (!Cache)
	{
		return;
	}

	if (Cache->AllocatedSize < CacheSize * 1024 * 1024)
	{
		return;
	}

	struct FKey
	{
		uint32 Key;
		double LastAccess;
	};
	TVoxelArray<FKey> OrderedKeys;
	OrderedKeys.Reserve(Cache->ChunkKeyToChunk.Num());
	for (const auto& It : Cache->ChunkKeyToChunk)
	{
		OrderedKeys.Add_GetRef(FKey(It.Key, It.Value.LastAccess));
	}
	OrderedKeys.Sort([](const FKey& A, const FKey& B)
	{
		return A.LastAccess < B.LastAccess;
	});

	const float ClearCacheUpTo = CacheSize * 1024 * 1024 * 0.5f;
	for (const FKey& Key : OrderedKeys)
	{
		FChunk Chunk;
		if (!ensure(Cache->ChunkKeyToChunk.RemoveAndCopyValue(Key.Key, Chunk)))
		{
			continue;
		}

		if (Chunk.Points)
		{
			Cache->AllocatedSize -= Chunk.Points->GetAllocatedSize();
		}

		if (Cache->AllocatedSize <= ClearCacheUpTo * 1024 * 1024)
		{
			break;
		}
	}
}

void FVoxelScatterCacheManager::ResetCache(const TVoxelObjectPtr<AVoxelScatterActor>& WeakActor)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	ActorToNodeGuidToCache_RequiresLock.Remove(WeakActor);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

uint64 FVoxelScatterCacheManager::GetAllocatedSize() const
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	uint64 AllocatedSize = 0;
	for (const auto& It : ActorToNodeGuidToCache_RequiresLock)
	{
		for (const auto& InnerIt : It.Value)
		{
			AllocatedSize += InnerIt.Value.AllocatedSize;
		}
	}
	return AllocatedSize;
}

uint64 FVoxelScatterCacheManager::GetAllocatedSize(const FGuid NodeGuid) const
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	uint64 AllocatedSize = 0;
	for (const auto& It : ActorToNodeGuidToCache_RequiresLock)
	{
		const FCache* NodeCache = It.Value.Find(NodeGuid);
		if (!NodeCache)
		{
			continue;
		}

		AllocatedSize += NodeCache->AllocatedSize;
	}
	return AllocatedSize;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<const FVoxelPointSet> FVoxelScatterCacheManager::FindCachedChunk(
	const TVoxelObjectPtr<AVoxelScatterActor>& WeakActor,
	const FGuid NodeGuid,
	const uint32 ChunkKey)
{
	VOXEL_SCOPE_LOCK(CriticalSection);
	TVoxelMap<FGuid, FCache>* NodeGuidToCache = ActorToNodeGuidToCache_RequiresLock.Find(WeakActor);
	if (!NodeGuidToCache)
	{
		return nullptr;
	}

	FCache* Cache = NodeGuidToCache->Find(NodeGuid);
	if (!Cache)
	{
		return nullptr;
	}

	FChunk* Chunk = Cache->ChunkKeyToChunk.Find(ChunkKey);
	if (!Chunk)
	{
		return nullptr;
	}

	if (Chunk->Tracker->IsInvalidated())
	{
		Cache->AllocatedSize -= Chunk->Points->GetAllocatedSize();
		Cache->ChunkKeyToChunk.Remove(ChunkKey);
		return nullptr;
	}

	Chunk->LastAccess = FPlatformTime::Seconds();
	return Chunk->Points;
}

void FVoxelScatterCacheManager::ComputePoints(
	const FVoxelGraphEnvironment& Environment,
	const FVoxelCompiledTerminalGraph& TerminalGraph,
	FVoxelInvalidationQueue* InvalidationQueue,
	const FVoxelLayers& Layers,
	const FVoxelSurfaceTypeTable& SurfaceTypeTable,
	const TVoxelObjectPtr<AVoxelScatterActor>& WeakActor,
	const FGuid NodeGuid,
	const uint32 ChunkKey,
	const FVoxelBox& Bounds,
	const float BoundsExtension,
	const TVoxelSet<FName>& AttributesToKeep,
	const FVoxelNode::TPinRef_Input<FVoxelPointSet>& Pin,
	TSharedPtr<const FVoxelPointSet>& OutPoints,
	TSharedPtr<FVoxelDependencyTracker>& OutDependencyTracker)
{
	FVoxelDependencyCollector DependencyCollector(STATIC_FNAME("FVoxelNode_CachePoints Chunk"));

	FVoxelGraphContext Context(
		Environment,
		TerminalGraph,
		DependencyCollector);
	FVoxelGraphQueryImpl& Query = Context.MakeQuery();
	FVoxelQuery VoxelQuery = FVoxelQuery(
		0,
		Layers,
		SurfaceTypeTable,
		DependencyCollector);
	Query.AddParameter<FVoxelGraphParameters::FQuery>(VoxelQuery);
	Query.AddParameter<FVoxelGraphParameters::FPointSetChunkBounds>(Bounds, true);
	{
		FVoxelGraphParameters::FScatterCacheManager& Parameter = Query.AddParameter<FVoxelGraphParameters::FScatterCacheManager>();
		Parameter.CacheManager = SharedThis(this);
		Parameter.WeakActor = WeakActor;
		Parameter.InvalidationQueue = InvalidationQueue;
	}

	OutPoints = INLINE_LAMBDA -> TSharedRef<const FVoxelPointSet>
	{
		const TSharedRef<const FVoxelPointSet> Points = Pin.GetSynchronous(Query);
		if (Points->Num() == 0)
		{
			return MakeShared<FVoxelPointSet>();
		}

		const FVoxelDoubleVectorBuffer* PositionBuffer = Points->Find<FVoxelDoubleVectorBuffer>(FVoxelPointAttributes::Position);
		if (!PositionBuffer)
		{
			return MakeShared<FVoxelPointSet>();
		}

		const FVoxelBoolBuffer* IsNeighborBuffer = Points->Find<FVoxelBoolBuffer>(FVoxelPointAttributes::IsNeighbor);

		TVoxelArray<int32> IndicesToKeep;
		IndicesToKeep.Reserve(Points->Num());

		const FVoxelBox ExtendedBounds = Bounds.Extend(BoundsExtension);
		for (int32 Index = 0; Index < Points->Num(); Index++)
		{
			if (!ExtendedBounds.Contains((*PositionBuffer)[Index]))
			{
				continue;
			}

			if (IsNeighborBuffer &&
				(*IsNeighborBuffer)[Index])
			{
				continue;
			}

			IndicesToKeep.Add(Index);
		}

		TSharedRef<FVoxelPointSet> NewPoints = Points->Gather(IndicesToKeep);
		NewPoints->KeepAttributes(AttributesToKeep);

		if (NewPoints->Num() == 0)
		{
			return MakeShared<FVoxelPointSet>();
		}

		return NewPoints;
	};

	OutDependencyTracker = DependencyCollector.Finalize(
		InvalidationQueue,
		MakeWeakPtrLambda(this, [this, Bounds](const FVoxelInvalidationCallstack& Callstack)
		{
			Dependency->Invalidate(Bounds);
		}));
}

void FVoxelScatterCacheManager::StorePoints(
	const TVoxelObjectPtr<AVoxelScatterActor>& WeakActor,
	const FGuid NodeGuid,
	const uint32 ChunkKey,
	const TSharedRef<const FVoxelPointSet>& Points,
	const TSharedRef<FVoxelDependencyTracker>& DependencyTracker)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_LOCK(CriticalSection);

	FCache& Cache = ActorToNodeGuidToCache_RequiresLock.FindOrAdd(WeakActor).FindOrAdd(NodeGuid);

	FChunk& Chunk = Cache.ChunkKeyToChunk.FindOrAdd(ChunkKey);
	if (Chunk.Points)
	{
		Cache.AllocatedSize -= Chunk.Points->GetAllocatedSize();
	}

	Chunk.Points = Points;
	Chunk.Tracker = DependencyTracker;
	Chunk.LastAccess = FPlatformTime::Seconds();
	Cache.AllocatedSize += Chunk.Points->GetAllocatedSize();
}