/**
 * MOVoxelReadinessSubsystem.cpp - Voxel terrain readiness signal
 */

#include "MOVoxelReadinessSubsystem.h"
#include "MOFramework.h"
#include "VoxelWorld.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

UMOVoxelReadinessSubsystem* UMOVoxelReadinessSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	UWorld* World = WorldContextObject->GetWorld();
	if (!World) return nullptr;
	return World->GetSubsystem<UMOVoxelReadinessSubsystem>();
}

void UMOVoxelReadinessSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TM = World->GetTimerManager();
		TM.ClearTimer(PollTimerHandle);
		TM.ClearTimer(CollisionDelayTimerHandle);
	}
	OnVoxelReady.Clear();
	Super::Deinitialize();
}

void UMOVoxelReadinessSubsystem::BeginPolling()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOVoxelReadiness] BeginPolling: no world"));
		return;
	}

	// Reset any in-flight cycle. Idempotent and safe across new-game/load.
	FTimerManager& TM = World->GetTimerManager();
	TM.ClearTimer(PollTimerHandle);
	TM.ClearTimer(CollisionDelayTimerHandle);

	CurrentPhase = EMOVoxelReadinessPhase::WaitingForRuntime;

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOVoxelReadiness] BeginPolling — phase=WaitingForRuntime, interval=%.2fs, postReadyDelay=%.1fs"),
		PollIntervalSeconds, CollisionGenerationDelay);

	// Poll immediately on next tick, then on interval. If voxel was somehow
	// already ready when BeginPolling was called, the first poll catches it.
	TM.SetTimer(PollTimerHandle, this, &UMOVoxelReadinessSubsystem::PollVoxelReady,
		PollIntervalSeconds, /*bLoop=*/true, /*FirstDelay=*/0.0f);
}

AVoxelWorld* UMOVoxelReadinessSubsystem::FindVoxelWorld() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void UMOVoxelReadinessSubsystem::PollVoxelReady()
{
	if (CurrentPhase != EMOVoxelReadinessPhase::WaitingForRuntime)
	{
		// Phase advanced or polling cancelled — stop ticking.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PollTimerHandle);
		}
		return;
	}

	AVoxelWorld* VoxelWorld = FindVoxelWorld();
	if (!VoxelWorld)
	{
		UE_LOG(LogMOFramework, Verbose,
			TEXT("[MOVoxelReadiness] No AVoxelWorld in level yet — continuing poll"));
		return;
	}

	if (!VoxelWorld->IsVoxelWorldReady())
	{
		UE_LOG(LogMOFramework, Verbose,
			TEXT("[MOVoxelReadiness] Voxel runtime not ready yet"));
		return;
	}

	// Voxel runtime is ready. Stop the poll, start the collision delay.
	UWorld* World = GetWorld();
	World->GetTimerManager().ClearTimer(PollTimerHandle);
	CurrentPhase = EMOVoxelReadinessPhase::WaitingForCollision;

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOVoxelReadiness] Voxel runtime ready — waiting %.1fs for collision generation"),
		CollisionGenerationDelay);

	if (CollisionGenerationDelay > 0.0f)
	{
		World->GetTimerManager().SetTimer(CollisionDelayTimerHandle, this,
			&UMOVoxelReadinessSubsystem::OnCollisionDelayElapsed,
			CollisionGenerationDelay, /*bLoop=*/false);
	}
	else
	{
		// Zero delay configured — broadcast immediately.
		OnCollisionDelayElapsed();
	}
}

void UMOVoxelReadinessSubsystem::OnCollisionDelayElapsed()
{
	CurrentPhase = EMOVoxelReadinessPhase::Ready;

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOVoxelReadiness] Ready — broadcasting OnVoxelReady to %d listener(s)"),
		OnVoxelReady.GetAllocatedSize() > 0 ? 1 : 0); // approximate; multicast doesn't expose count

	OnVoxelReady.Broadcast();
}
