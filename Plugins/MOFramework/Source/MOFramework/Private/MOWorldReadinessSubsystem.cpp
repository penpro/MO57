#include "MOWorldReadinessSubsystem.h"
#include "ContentStreaming.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "MOFramework.h"
#include "MOBugReport.h"
#include "Grid/PCGPartitionActor.h"
#include "MOVoxelReadinessSubsystem.h"
#include "PCGComponent.h"
#include "Subsystems/PCGSubsystem.h"
#include "TimerManager.h"
#include "VoxelWorld.h"

namespace
{
	TAutoConsoleVariable<int32> CVarEnabled(TEXT("mo.Readiness.Enabled"), 1, TEXT("0 = every readiness cycle is ready at once (harnesses that do not want the extra wait)."));
	TAutoConsoleVariable<float> CVarPcgQuietSeconds(TEXT("mo.Readiness.PcgQuietSeconds"), 1.5f, TEXT("Seconds with no PCG graph executing before vegetation counts as populated."));
	TAutoConsoleVariable<float> CVarMinPcgSeconds(TEXT("mo.Readiness.MinPcgSeconds"), 2.0f, TEXT("Floor for the vegetation stage once PCG was seen working."));
	TAutoConsoleVariable<float> CVarNoPcgActivityGraceSeconds(TEXT("mo.Readiness.PcgNoActivityGraceSeconds"), 4.0f, TEXT("Floor for the vegetation stage when PCG was never seen working (the generation source may not exist yet)."));
	TAutoConsoleVariable<float> CVarMaxPcgSeconds(TEXT("mo.Readiness.MaxPcgSeconds"), 15.0f, TEXT("Cap for the vegetation stage. Past it the stage keeps holding for mo.Readiness.NearInstanceMin: the PCG scheduler generates the LARGEST grid first, so the ground cover around the player arrives last."));
	TAutoConsoleVariable<int32> CVarHoldGridsFrom(TEXT("mo.Readiness.HoldGridsFrom"), 512, TEXT("While a readiness cycle runs, PCG hi-gen grids of this UNSCALED size (512 = GRID512, the far-canopy tier) and larger generate nothing; the authored radii come back at Ready. The scheduler always generates the largest grid first, so a far tier on a big grid would hold back every near cell. 0 = off."));
	TAutoConsoleVariable<int32> CVarNearInstanceMin(TEXT("mo.Readiness.NearInstanceMin"), 200, TEXT("Past mo.Readiness.MaxPcgSeconds the vegetation stage keeps holding until this many spawned instances exist near the player (0 = off), up to mo.Readiness.MaxPcgHardSeconds."));
	TAutoConsoleVariable<float> CVarNearInstanceRadiusCm(TEXT("mo.Readiness.NearInstanceRadiusCm"), 6000.0f, TEXT("Radius (cm) around the camera for the near-vegetation count."));
	TAutoConsoleVariable<float> CVarMaxPcgHardSeconds(TEXT("mo.Readiness.MaxPcgHardSeconds"), 32.0f, TEXT("Hard cap for the vegetation stage when the near-vegetation count is still below its minimum."));
	TAutoConsoleVariable<float> CVarMaxStreamingSeconds(TEXT("mo.Readiness.MaxStreamingSeconds"), 4.0f, TEXT("Cap for the streaming stage: a pool-limited scene never reaches zero textures wanting resources."));
	TAutoConsoleVariable<int32> CVarStreamingWantMax(TEXT("mo.Readiness.StreamingWantMax"), 8, TEXT("Textures still wanting resources that the streaming stage tolerates."));
	TAutoConsoleVariable<float> CVarPcgFrameTimeMs(TEXT("mo.Readiness.PcgFrameTimeMs"), 20.0f, TEXT("PCG's per-frame budget (pcg.FrameTime, normally 5 ms) during the vegetation stage, behind the loading overlay: the time-sliced spawners build a dense forest in 5 ms slices otherwise. Restored when the stage ends. 0 = leave pcg.FrameTime alone."));
	TAutoConsoleVariable<float> CVarMaxTotalSeconds(TEXT("mo.Readiness.MaxTotalSeconds"), 60.0f, TEXT("Cap for the whole cycle: force-ready (and a warning) instead of a black screen that never lifts."));

	constexpr float PollIntervalSeconds = 0.2f;
	constexpr float TerrainEnd = 0.40f;
	constexpr float VegetationEnd = 0.85f;

	/**
	 * The radius a held grid gets: negative by more than a cell. The scheduler walks ceil(radius / cell) cells around each generation source, so a negative radius walks none; 0, or anything
	 * above minus one cell, still generates the cell the viewer stands in (it compares SQUARED distances). Cleanup compares squared distances too, so cells of a held grid that already exist
	 * (a respawn far away) are left alone until the release, when the normal cleanup takes them.
	 */
	constexpr float HeldRadiusCm = -1.0e9f;

	/** The unscaled hi-gen grids that own a radius field (the fields are named by it: Radius51200 = GRID512, whatever the graph's grid-size multiplier); grids above 2048 derive from Radius204800. */
	constexpr int32 GridsWithRadiusField[] = { 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048 };

	/** The radius fields are PROTECTED UPROPERTYs of FPCGRuntimeGenerationRadii (UE 5.8): reached through reflection, as the editor and Python reach them. */
	const FStructProperty* RadiusFieldProperty(int32 UnscaledGrid)
	{
		return FindFProperty<FStructProperty>(FPCGRuntimeGenerationRadii::StaticStruct(), *FString::Printf(TEXT("Radius%d"), UnscaledGrid * 100));
	}
}

FPerQualityLevelFloat* UMOWorldReadinessSubsystem::GetMutableRadiusField(FPCGRuntimeGenerationRadii& Radii, int32 UnscaledGrid)
{
	const FStructProperty* P = RadiusFieldProperty(UnscaledGrid);
	return P ? P->ContainerPtrToValuePtr<FPerQualityLevelFloat>(&Radii) : nullptr;
}

const FPerQualityLevelFloat* UMOWorldReadinessSubsystem::GetRadiusField(const FPCGRuntimeGenerationRadii& Radii, int32 UnscaledGrid)
{
	const FStructProperty* P = RadiusFieldProperty(UnscaledGrid);
	return P ? P->ContainerPtrToValuePtr<FPerQualityLevelFloat>(&Radii) : nullptr;
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

void UMOWorldReadinessSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UMOBugReportSubsystem* Reports = UMOBugReportSubsystem::Get(&InWorld))
	{
		Reports->RegisterContributor(TEXT("WorldBuild"), [](const UWorld* World, FMOBugReportFields& Out)
		{
			if (const UMOWorldReadinessSubsystem* Self = World ? World->GetSubsystem<UMOWorldReadinessSubsystem>() : nullptr)
			{
				Self->DescribeForBugReport(Out);
			}
		});
	}
}

void UMOWorldReadinessSubsystem::DescribeForBugReport(TArray<TPair<FString, FString>>& Out) const
{
	Out.Emplace(TEXT("Stage"), UEnum::GetValueAsString(Stage));
	Out.Emplace(TEXT("LastReady"), LastFinishReason.IsEmpty() ? FString(TEXT("(not this session)")) : FString::Printf(TEXT("%s after %.1f s"), *LastFinishReason, LastFinishAfterSeconds));
	if (bLastVegetationCapped)
	{
		Out.Emplace(TEXT("VegetationCapped"), FString::Printf(TEXT("yes, %d instances near the camera then"), LastCappedNearCount));
	}
	Out.Emplace(TEXT("HeldGrids"), FString::FromInt(HeldRadii.Num()));   // more than 0 once ready = far grids never released
	if (const IConsoleVariable* FrameTime = IConsoleManager::Get().FindConsoleVariable(TEXT("pcg.FrameTime")))
	{
		Out.Emplace(TEXT("PcgFrameTimeMs"), FString::Printf(TEXT("%.1f"), FrameTime->GetFloat()));
	}
	bool bHasPcg = false;
	const bool bBusy = IsPcgBusy(bHasPcg);
	Out.Emplace(TEXT("PcgExecuting"), !bHasPcg ? TEXT("(no PCG subsystem)") : bBusy ? TEXT("yes") : TEXT("no"));
	int32 Components = 0, Generating = 0, Generated = 0, PartitionActors = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		PartitionActors += It->IsA<APCGPartitionActor>() ? 1 : 0;
		TInlineComponentArray<UPCGComponent*> Comps(*It);
		for (const UPCGComponent* Comp : Comps)
		{
			++Components;
			Generating += Comp->IsGenerating() ? 1 : 0;
			Generated += Comp->bGenerated ? 1 : 0;
		}
	}
	Out.Emplace(TEXT("PcgComponents"), FString::Printf(TEXT("%d (%d generated, %d generating) on %d partition actor(s)"), Components, Generated, Generating, PartitionActors));
	NextNearCountSeconds = 0.0;   // a fresh count, not the cached one
	Out.Emplace(TEXT("NearInstances"), FString::Printf(TEXT("%d within %.0f m"), CountNearPcgInstances(), CVarNearInstanceRadiusCm.GetValueOnGameThread() / 100.0f));
}

void UMOWorldReadinessSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimerHandle);
	}
	ReleaseFarGrids();
	RestorePcgBudget();
	OnWorldReady.Clear();
	Super::Deinitialize();
}

float UMOWorldReadinessSubsystem::GetHeldRadiusCm()
{
	return HeldRadiusCm;
}

void UMOWorldReadinessSubsystem::HoldRadii(FPCGRuntimeGenerationRadii& Radii, int32 HoldFromUnscaledGrid)
{
	if (HoldFromUnscaledGrid <= 0)
	{
		return;
	}
	for (const int32 Grid : GridsWithRadiusField)
	{
		FPerQualityLevelFloat* R = Grid >= HoldFromUnscaledGrid ? GetMutableRadiusField(Radii, Grid) : nullptr;
		if (R)
		{
			R->Default = HeldRadiusCm;
			for (TPair<int32, float>& Q : R->PerQuality)
			{
				Q.Value = HeldRadiusCm;
			}
		}
	}
	Radii.ComputeHash();   // the struct caches its hash ("call this anytime the values are changed"); the scheduler's change detector keys on it
}

void UMOWorldReadinessSubsystem::RestoreRadii(FPCGRuntimeGenerationRadii& Radii, const FPCGRuntimeGenerationRadii& Authored)
{
	// Field by field, values only: the hold changed no keys, so no map is rebuilt under a reader.
	for (const int32 Grid : GridsWithRadiusField)
	{
		FPerQualityLevelFloat* R = GetMutableRadiusField(Radii, Grid);
		const FPerQualityLevelFloat* A = GetRadiusField(Authored, Grid);
		if (!R || !A)
		{
			continue;
		}
		R->Default = A->Default;
		for (TPair<int32, float>& Q : R->PerQuality)
		{
			if (const float* AuthoredValue = A->PerQuality.Find(Q.Key))
			{
				Q.Value = *AuthoredValue;
			}
		}
	}
	Radii.ComputeHash();
}

void UMOWorldReadinessSubsystem::HoldFarGrids()
{
	const int32 HoldFrom = CVarHoldGridsFrom.GetValueOnGameThread();
	UWorld* World = GetWorld();
	if (HoldFrom <= 0 || !World || HeldRadii.Num() > 0)   // already held: a restarted cycle keeps the authored radii it saved first
	{
		return;
	}
	int32 Skipped = 0;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		TArray<UPCGComponent*> Comps;
		ActorIt->GetComponents<UPCGComponent>(Comps);
		for (UPCGComponent* Comp : Comps)
		{
			if (!Comp || Comp->IsLocalComponent() || !Comp->IsManagedByRuntimeGenSystem())
			{
				continue;
			}
			if (!Comp->bOverrideGenerationRadii)
			{
				++Skipped;   // radii come from the graph asset: not ours to edit at runtime
				continue;
			}
			FHeldGenerationRadii& Held = HeldRadii.AddDefaulted_GetRef();
			Held.Component = Comp;
			Held.Authored = Comp->GenerationRadii;
			HoldRadii(Comp->GenerationRadii, HoldFrom);
		}
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOReadiness] holding PCG grids >= %d on %d runtime component(s) until ready (%d without a radius override left alone)"), HoldFrom, HeldRadii.Num(), Skipped);
}

void UMOWorldReadinessSubsystem::ReleaseFarGrids()
{
	if (HeldRadii.Num() == 0)
	{
		return;
	}
	int32 Released = 0;
	for (const FHeldGenerationRadii& Held : HeldRadii)
	{
		if (UPCGComponent* Comp = Held.Component.Get())
		{
			RestoreRadii(Comp->GenerationRadii, Held.Authored);
			++Released;
		}
	}
	HeldRadii.Reset();
	UE_LOG(LogMOFramework, Warning, TEXT("[MOReadiness] released the held PCG grids on %d component(s) at +%.1fs"), Released, FPlatformTime::Seconds() - CycleStartSeconds);
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
			if (Num == 0 || !Ism->GetStaticMesh() || Ism->ComponentHasTag(TEXT("MOFar_Tree")) || Ism->ComponentHasTag(TEXT("MOFar_Canopy")) || Ism->ComponentHasTag(TEXT("MOFar_Grass")))
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
	bLastVegetationCapped = false;
	CycleStartSeconds = FPlatformTime::Seconds();
	LastPollSeconds = CycleStartSeconds;
	NextTraceSeconds = 0.0;
	Stage = EMOWorldReadyStage::Inactive;   // EnterStage logs the transition from a clean slate

	if (CVarEnabled.GetValueOnGameThread() == 0)
	{
		Finish(TEXT("mo.Readiness.Enabled=0"));
		return;
	}

	HoldFarGrids();
	EnterStage(EMOWorldReadyStage::Terrain);
	World->GetTimerManager().SetTimer(PollTimerHandle, this, &UMOWorldReadinessSubsystem::Poll, PollIntervalSeconds, /*bLoop=*/true, /*FirstDelay=*/0.0f);
}

void UMOWorldReadinessSubsystem::StopTracking()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimerHandle);
	}
	ReleaseFarGrids();
	RestorePcgBudget();
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
	if (Stage == EMOWorldReadyStage::Vegetation)
	{
		RaisePcgBudget();
	}
	else
	{
		RestorePcgBudget();
	}
}

void UMOWorldReadinessSubsystem::RaisePcgBudget()
{
	const float BudgetMs = CVarPcgFrameTimeMs.GetValueOnGameThread();
	IConsoleVariable* FrameTime = IConsoleManager::Get().FindConsoleVariable(TEXT("pcg.FrameTime"));
	if (bPcgBudgetRaised || BudgetMs <= 0.0f || !FrameTime)
	{
		return;
	}
	SavedPcgFrameTimeMs = FrameTime->GetFloat();
	if (BudgetMs > SavedPcgFrameTimeMs)
	{
		FrameTime->Set(BudgetMs, ECVF_SetByCode);
		bPcgBudgetRaised = true;
		UE_LOG(LogMOFramework, Log, TEXT("[MOReadiness] pcg.FrameTime %.1f -> %.1f ms for the vegetation stage"), SavedPcgFrameTimeMs, BudgetMs);
	}
}

void UMOWorldReadinessSubsystem::RestorePcgBudget()
{
	if (!bPcgBudgetRaised)
	{
		return;
	}
	bPcgBudgetRaised = false;
	if (IConsoleVariable* FrameTime = IConsoleManager::Get().FindConsoleVariable(TEXT("pcg.FrameTime")))
	{
		FrameTime->Set(SavedPcgFrameTimeMs, ECVF_SetByCode);
	}
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
	LastFinishReason = Reason;
	LastFinishAfterSeconds = FPlatformTime::Seconds() - CycleStartSeconds;
	ReleaseFarGrids();
	RestorePcgBudget();
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
				bLastVegetationCapped = true;
				LastCappedNearCount = NearCount;
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
	// The near count is what the vegetation cap waits for: traced through the whole stage, so a slow reveal shows WHEN the near tiers arrived (not only the count at the cap).
	const int32 Near = Stage == EMOWorldReadyStage::Vegetation ? CountNearPcgInstances() : -1;
	// The voxel world's own work after its first ready state (a new state when the viewer moves, far LODs): the Voxel PCG samplers run on the same
	// BackgroundLow task pool, so a busy voxel world can starve the near tiers -- traced so the gap between "voxel ready" and "near tiers in" is explained.
	int32 VoxelPending = -1;
	bool bVoxelNewState = false;
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AVoxelWorld> It(World); It; ++It)
		{
			VoxelPending = It->GetNumPendingTasks();
			bVoxelNewState = It->IsProcessingNewState();
			break;
		}
	}
	return FString::Printf(TEXT("stage=%s progress=%.2f voxelPhase=%s voxelNewState=%d voxelPending=%d pcg=%s pcgQuiet=%.1fs pcgSeenBusy=%d near=%d heldComps=%d streamWanting=%d"),
		*UEnum::GetValueAsString(Stage), Progress,
		Voxel ? *UEnum::GetValueAsString(Voxel->GetPhase()) : TEXT("none"),
		bVoxelNewState ? 1 : 0, VoxelPending,
		!bHasPcg ? TEXT("none") : bBusy ? TEXT("busy") : TEXT("idle"),
		PcgQuietSeconds, bPcgSeenBusy ? 1 : 0, Near, HeldRadii.Num(), IStreamingManager::Get().GetNumWantingResources());
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
