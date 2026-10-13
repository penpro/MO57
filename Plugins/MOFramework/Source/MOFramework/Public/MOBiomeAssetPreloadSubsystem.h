/**
 * MOBiomeAssetPreloadSubsystem.h - every biome species mesh loaded ASYNCHRONOUSLY at world start, and kept
 *
 * The biome spawner resolves its species meshes with LoadSynchronous on the game thread. A mesh that is not in memory -- the first visit to a biome, or one
 * garbage-collected since -- turns that into a blocking load, and a blocking load while anything else is streaming flushes ALL async loading: an 814 ms
 * hitch in the middle of a packaged perf bench (Oct 13 2026: "setup 289 ms ... FlushAsyncLoading"). This subsystem requests every species mesh of every
 * DT_Biomes row (main and water-edge lists) as one async batch when a game world begins play -- during the terrain stage, behind the loading screen --
 * and holds strong references, so the spawner's LoadSynchronous is always a lookup and the meshes are never evicted between visits.
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/StreamableManager.h"
#include "MOBiomeAssetPreloadSubsystem.generated.h"

class UStaticMesh;

UCLASS()
class MOFRAMEWORK_API UMOBiomeAssetPreloadSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Whether the batch finished (true once every requested mesh is resident). */
	bool IsComplete() const { return bComplete; }

	/** How many meshes the batch holds / requested. */
	int32 GetNumLoaded() const { return Loaded.Num(); }
	int32 GetNumRequested() const { return NumRequested; }

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	/** The strong references: what keeps the meshes resident. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> Loaded;

	TSharedPtr<FStreamableHandle> Handle;
	int32 NumRequested = 0;
	bool bComplete = false;
	double RequestTime = 0.0;

	void HandleLoaded();
};
