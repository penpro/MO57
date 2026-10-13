#include "MOWorldReadinessSubsystem.h"
#include "ContentStreaming.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "MOFramework.h"
#include "MOVoxelReadinessSubsystem.h"
#include "Subsystems/PCGSubsystem.h"
#include "TimerManager.h"
#include "VoxelWorld.h"

namespace
{
	TAutoConsoleVariable<int32> CVarEnabled(TEXT("mo.Readiness.Enabled"), 1, TEXT("0 = every readiness cycle is ready at once (harnesses that do not want the extra wait)."));
	TAutoConsoleVariable<float> CVarPcgQuietSeconds(TEXT("mo.Readiness.PcgQuietSeconds"), 1.5f, TEXT("Seconds with no PCG graph executing before vegetation counts as populated."));
	TAutoConsoleVariable<float> CVarMinPcgSeconds(TEXT("mo.Readiness.MinPcgSeconds"), 2.0f, TEXT("Floor for the vegetation stage once PCG was seen working."));
	TAutoConsoleVariable<float> CVarNoPcgActivityGraceSeconds(TEXT("mo.Readiness.PcgNoActivityGraceSeconds"), 4.0f, TEXT("Floor for the vegetation stage when PCG was never seen working (the generation source may not exist yet)."));
	TAutoConsoleVariable<float> CVarMaxPcgSeconds(TEXT("mo.Readiness.MaxPcgSeconds"), 15.0f, TEXT("Cap for the vegetation stage. The PCG scheduler generates nearest-first, so after this long the near and mid field is in and only the far field is still filling behind the reveal."));
	TAutoConsoleVariable<int32> CVarNearInstanceMin(TEXT("mo.Readiness.NearInstanceMin"), 200, TEXT("Past mo.Readiness.MaxPcgSeconds the vegetation stage keeps holding until this many spawned instances exist near the player (0 = off), up to mo.Readiness.MaxPcgHardSeconds."));
	TAutoConsoleVariable<float> CVarNearInstanceRadiusCm(TEXT("mo.Readiness.NearInstanceRadiusCm"), 6000.0f, TEXT("Radius (cm) around the camera for the near-vegetation count."));
	TAutoConsoleVariable<float> CVarMaxPcgHardSeconds(TEXT("mo.Readiness.MaxPcgHardSeconds"), 32.0f, TEXT("Hard cap for the vegetation stage when the near-vegetation count is still below its minimum."));
	TAutoConsoleVariable<float> CVarMaxStreamingSeconds(TEXT("mo.Readiness.MaxStreamingSeconds"), 4.0f, TEXT("Cap for the streaming stage: a pool-limited scene never reaches zero textures wanting resources."));
	TAutoConsoleVariable<int32> CVarStreamingWantMax(TEXT("mo.Readiness.StreamingWantMax"), 8, TEXT("Textures still wanting resources that the streaming stage tolerates."));
	TAutoConsoleVariable<float> CVarMaxTotalSeconds(TEXT("mo.Readiness.MaxTotalSeconds"), 60.0f, TEXT("Cap for the whole cycle: force-ready (and a warning) instead of a black screen that never lifts."));

	constexpr float PollIntervalSeconds = 0.2f;
	constexpr float TerrainEnd = 0.40f;
	constexpr float VegetationEnd = 0.85f;
}

UMOWorldReadinessSubsystem* UMOWorldReadinessSubsystem::Get(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMOWorldReadinessSubsystem>() : nullptr;
}

bool UMOWorldReadinessSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}

void UMOWorldReadinessSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimerHandle);
	}
	OnWorldReady.Clear();
	Super::Deinitialize();
}

FText UMOWorldReadinessSubsystem::GetStatusText() const
{
	switch (Stage)
	{
	case EMOWorldReadyStage::Terrain:    return NSLOCTEXT("MOReadiness", "Terrain", "Raising the terrain");
	case EMOWorldReadyStage::Vegetation: return NSLOCTEXT("MOReadiness", "Vegetation", "Growing the land");
	case EMOWorldReadyStage::Streaming:  return NSLOCTEXT("MOReadiness", "Streaming", "Settling in");
	default:                             return FText::GetEmpty();
	}
}

bool UMOWorldReadinessSubsystem::WorldHasVoxel() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		return true;
	}
	return false;
}

bool UMOWorldReadinessSubsystem::IsPcgBusy(bool& bOutHasPcg) const
{
	UPCGSubsystem* Pcg = UPCGSubsystem::GetInstance(GetWorld());
	bOutHasPcg = Pcg != nullptr;
	return Pcg && Pcg->IsAnyGraphCurrentlyExecuting();
}

int32 UMOWorldReadinessSubsystem::CountNearPcgInstances() const
{
	const double Now = FPlatformTime::Seconds();
	if (Now < NextNearCountSeconds)
	{
		return LastNearCount;
	}
	NextNearCountSeconds = Now + 1.0;
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return LastNearCount;
	}
	const FVector C = PC->PlayerCameraManager->GetCameraLocation();
	const double Radius = CVarNearInstanceRadiusCm.GetValueOnGameThread();
	int32 Count = 0;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		TArray<UInstancedStaticMeshComponent*> Comps;
		ActorIt->GetComponents<UInstancedStaticMeshComponent>(Comps);
		for (UInstancedStaticMeshComponent* Ism : Comps)
		{
			const int32 Num = Ism->GetInstanceCount();
			if (Num == 0 || !Ism->GetStaticMesh() || Ism->ComponentHasTag(TEXT("MOFar_Tree")) || Ism->ComponentHasTag(TEXT("MOFar_Grass")))
			{
				continue;
			}
			// A partition cell's component is local to its cell: skip the ones whose bounds cannot reach the circle.
			const FBox2D CellBox(FVector2D(Ism->Bounds.Origin.X - Ism->Bounds.BoxExtent.X, Ism->Bounds.Origin.Y - Ism->Bounds.BoxExtent.Y), FVector2D(Ism->Bounds.Origin.X + Ism->Bounds.BoxExtent.X, Ism->Bounds.Origin.Y + Ism->Bounds.BoxExtent.Y));
			const FVector2D Nearest(FMath::Clamp(C.X, CellBox.Min.X, CellBox.Max.X), FMath::Clamp(C.Y, CellBox.Min.Y, CellBox.Max.Y));
			if (FVector2D::Distance(Nearest, FVector2D(C.X, C.Y)) > Radius)
			{
				continue;
			}
			FTransform Xf;
			for (int32 I = 0; I < Num; ++I)
			{
				if (Ism->GetInstanceTransform(I, Xf, /*bWorldSpace=*/true) && FVector2D::Distance(FVector2D(Xf.GetLocation().X, Xf.GetLocation().Y), FVector2D(C.X, C.Y)) <= Radius)
				{
					++Count;
				}
			}
		}
	}
	LastNearCount = Count;
	return Count;
}

void UMOWorldReadinessSubsystem::BeginTracking(bool bStartVoxelPolling)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(PollTimerHandle);

	bStartedVoxelPolling = bStartVoxelPolling;
	Progress = 0.0f;
	PcgQuietSeconds = 0.0;
	bPcgSeenBusy = false;
	CycleStartSeconds = FPlatformTime::Seconds();
	LastPollSeconds = CycleStartSeconds;
	NextTraceSeconds = 0.0;
	Stage = EMOWorldReadyStage::Inactive;   // EnterStage logs the transition from a clean slate

	if (CVarEnabled.GetValueOnGameThread() == 0)
	{
		Finish(TEXT("mo.Readiness.Enabled=0"));
		return;
	}

	EnterStage(EMOWorldReadyStage::Terrain);
	World->GetTimerManager().SetTimer(PollTimerHandle, this, &UMOWorldReadinessSubsystem::Poll, PollIntervalSeconds, /*bLoop=*/true, /*FirstDelay=*/0.0f);
}

void UMOWorldReadinessSubsystem::StopTracking()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimerHandle);
	}
	Stage = EMOWorldReadyStage::Inactive;
	Progress = 0.0f;
}

void UMOWorldReadinessSubsystem::ForceReady(const TCHAR* Reason)
{
	if (IsTracking())
	{
		Finish(Reason);
	}
}

void UMOWorldReadinessSubsystem::EnterStage(EMOWorldReadyStage NewStage)
{
	const double Now = FPlatformTime::Seconds();
	UE_LOG(LogMOFramework, Warning, TEXT("[MOReadiness] %s -> %s at +%.1fs"),
		*UEnum::GetValueAsString(Stage), *UEnum::GetValueAsString(NewStage), Now - CycleStartSeconds);
	Stage = NewStage;
	StageStartSeconds = Now;
	PcgQuietSeconds = 0.0;
}

void UMOWorldReadinessSubsystem::SetProgress(float NewProgress)
{
	Progress = FMath::Max(Progress, FMath::Clamp(NewProgress, 0.0f, 1.0f));   // monotonic: a PCG task flickering back on must not pull the bar back
}

void UMOWorldReadinessSubsystem::Finish(const TCHAR* Reason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimerHandle);
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOReadiness] READY after %.1fs (%s)"), FPlatformTime::Seconds() - CycleStartSeconds, Reason);
	Stage = EMOWorldReadyStage::Ready;
	Progress = 1.0f;
	OnWorldReady.Broadcast();
}

void UMOWorldReadinessSubsystem::Poll()
{
	if (!IsTracking())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PollTimerHandle);
		}
		return;
	}

	const double Now = FPlatformTime::Seconds();
	const double Dt = Now - LastPollSeconds;
	LastPollSeconds = Now;

	if (CVarEnabled.GetValueOnGameThread() == 0)
	{
		Finish(TEXT("mo.Readiness.Enabled=0"));
		return;
	}
	if (Now - CycleStartSeconds >= CVarMaxTotalSeconds.GetValueOnGameThread())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOReadiness] giving up at the %.0fs cap - %s"), Now - CycleStartSeconds, *DescribeState());
		Finish(TEXT("total cap"));
		return;
	}

	const double StageElapsed = Now - StageStartSeconds;

	// A once-a-second trace of every raw signal: the staging is tuned against this, not guessed.
	if (Now >= NextTraceSeconds)
	{
		NextTraceSeconds = Now + 1.0;
		UE_LOG(LogMOFramework, Log, TEXT("[MOReadiness] +%.1fs %s"), Now - CycleStartSeconds, *DescribeState());
	}

	if (Stage == EMOWorldReadyStage::Terrain)
	{
		UMOVoxelReadinessSubsystem* Voxel = UMOVoxelReadinessSubsystem::Get(this);
		if (!Voxel || !WorldHasVoxel())
		{
			EnterStage(EMOWorldReadyStage::Vegetation);   // no voxel terrain on this map: nothing to wait for
		}
		else
		{
			if (bStartedVoxelPolling && Voxel->GetPhase() == EMOVoxelReadinessPhase::Inactive)
			{
				Voxel->BeginPolling();
			}
			switch (Voxel->GetPhase())
			{
			case EMOVoxelReadinessPhase::Ready:
				SetProgress(TerrainEnd);
				EnterStage(EMOWorldReadyStage::Vegetation);
				break;
			case EMOVoxelReadinessPhase::WaitingForCollision:
				SetProgress(TerrainEnd * 0.75f);
				break;
			default:
				SetProgress(TerrainEnd * 0.25f * (1.0f - FMath::Exp(-static_cast<float>(StageElapsed) / 6.0f)) + 0.02f);
				break;
			}
		}
		return;
	}

	if (Stage == EMOWorldReadyStage::Vegetation)
	{
		bool bHasPcg = false;
		const bool bBusy = IsPcgBusy(bHasPcg);
		if (!bHasPcg)
		{
			EnterStage(EMOWorldReadyStage::Streaming);
			return;
		}
		if (bBusy)
		{
			PcgQuietSeconds = 0.0;
			bPcgSeenBusy = true;
		}
		else
		{
			PcgQuietSeconds += Dt;
		}
		const double Floor = bPcgSeenBusy ? CVarMinPcgSeconds.GetValueOnGameThread() : CVarNoPcgActivityGraceSeconds.GetValueOnGameThread();
		const double Cap = CVarMaxPcgSeconds.GetValueOnGameThread();
		const bool bQuiet = PcgQuietSeconds >= CVarPcgQuietSeconds.GetValueOnGameThread() && StageElapsed >= Floor;
		// At the cap, keep holding while the ground right around the player is still bare (the near tier arrives last) -- up to the hard cap.
		const int32 NearMin = CVarNearInstanceMin.GetValueOnGameThread();
		const int32 NearCount = (NearMin > 0 && StageElapsed >= Cap) ? CountNearPcgInstances() : NearMin;
		const bool bCapped = StageElapsed >= Cap && (NearCount >= NearMin || StageElapsed >= CVarMaxPcgHardSeconds.GetValueOnGameThread());
		// No denominator exists for "how many PCG cells are left", so the bar eases toward the stage end over the cap and completes when PCG goes quiet (or the cap hits).
		SetProgress(TerrainEnd + (VegetationEnd - TerrainEnd) * 0.9f * (1.0f - FMath::Exp(-static_cast<float>(StageElapsed) / FMath::Max(static_cast<float>(Cap) * 0.4f, 1.0f))));
		if (bQuiet || bCapped)
		{
			if (bCapped && !bQuiet)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOReadiness] vegetation stage capped at %.0fs with PCG still working (%d spawned instances within %.0f m; far field keeps filling behind the reveal)"), StageElapsed, NearCount, CVarNearInstanceRadiusCm.GetValueOnGameThread() / 100.0f);
			}
			SetProgress(VegetationEnd);
			EnterStage(EMOWorldReadyStage::Streaming);
		}
		return;
	}

	if (Stage == EMOWorldReadyStage::Streaming)
	{
		const int32 Wanting = IStreamingManager::Get().GetNumWantingResources();
		const float Cap = CVarMaxStreamingSeconds.GetValueOnGameThread();
		SetProgress(VegetationEnd + (1.0f - VegetationEnd) * FMath::Clamp(static_cast<float>(StageElapsed) / FMath::Max(Cap, 0.1f), 0.0f, 0.95f));
		if (Wanting <= CVarStreamingWantMax.GetValueOnGameThread())
		{
			Finish(TEXT("streaming settled"));
		}
		else if (StageElapsed >= Cap)
		{
			Finish(TEXT("streaming cap"));
		}
	}
}

FString UMOWorldReadinessSubsystem::DescribeState() const
{
	const UMOVoxelReadinessSubsystem* Voxel = UMOVoxelReadinessSubsystem::Get(this);
	bool bHasPcg = false;
	const bool bBusy = IsPcgBusy(bHasPcg);
	return FString::Printf(TEXT("stage=%s progress=%.2f voxelPhase=%s pcg=%s pcgQuiet=%.1fs pcgSeenBusy=%d streamWanting=%d"),
		*UEnum::GetValueAsString(Stage), Progress,
		Voxel ? *UEnum::GetValueAsString(Voxel->GetPhase()) : TEXT("none"),
		!bHasPcg ? TEXT("none") : bBusy ? TEXT("busy") : TEXT("idle"),
		PcgQuietSeconds, bPcgSeenBusy ? 1 : 0, IStreamingManager::Get().GetNumWantingResources());
}

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorldAndArgs GReadinessDumpCmd(
		TEXT("MO.Readiness.Dump"),
		TEXT("Dev: print the world readiness stage, progress and raw signals."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMOWorldReadinessSubsystem* R = UMOWorldReadinessSubsystem::Get(World))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] Readiness %s"), *R->DescribeState());
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] Readiness: no subsystem in this world"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GReadinessRestartCmd(
		TEXT("MO.Readiness.Restart"),
		TEXT("Dev: restart the readiness cycle (voxel polling is started when it is idle). Usage: MO.Readiness.Restart"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMOWorldReadinessSubsystem* R = UMOWorldReadinessSubsystem::Get(World))
			{
				R->BeginTracking(/*bStartVoxelPolling=*/true);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GReadinessForceCmd(
		TEXT("MO.Readiness.Force"),
		TEXT("Dev: declare the world ready now (lifts a held loading overlay)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMOWorldReadinessSubsystem* R = UMOWorldReadinessSubsystem::Get(World))
			{
				R->ForceReady(TEXT("MO.Readiness.Force"));
			}
		}));
}
#endif
