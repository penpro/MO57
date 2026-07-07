#include "MOSurvivorJobQueueComponent.h"
#include "MOFramework.h"
#include "MOSurvivorJobDatabaseSettings.h"
#include "MOIdentityComponent.h"
#include "MOIdentityRegistrySubsystem.h"
#include "Net/UnrealNetwork.h"

UMOSurvivorJobQueueComponent::UMOSurvivorJobQueueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOSurvivorJobQueueComponent::BeginPlay()
{
	Super::BeginPlay();

	// Set owner reference for FastArray callbacks
	JobQueue.OwnerComponent = this;

	// Verify job database is configured
	if (!UMOSurvivorJobDatabaseSettings::IsConfigured())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorJobQueue] BeginPlay on %s - Job database not configured! Check Project Settings > Plugins > MO Survivor Job Database"),
			*GetOwner()->GetName());
	}
	else
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobQueue] BeginPlay on %s - Job database ready"),
			*GetOwner()->GetName());
	}
}

void UMOSurvivorJobQueueComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMOSurvivorJobQueueComponent, JobQueue);
	DOREPLIFETIME(UMOSurvivorJobQueueComponent, HomeLocation);
	DOREPLIFETIME(UMOSurvivorJobQueueComponent, HomeBuildingGuid);
}

// ============================================================================
// QUEUE MANAGEMENT
// ============================================================================

FGuid UMOSurvivorJobQueueComponent::EnqueueJob(EMOSurvivorJobType JobType, int32 RepeatCount)
{
	if (JobType == EMOSurvivorJobType::None)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorJobQueue] Cannot enqueue job of type None"));
		return FGuid();
	}

	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorJobQueue] EnqueueJob called on non-authority"));
		return FGuid();
	}

	FMOSurvivorJobEntry NewJob = CreateJobEntry(JobType, RepeatCount);
	JobQueue.Jobs.Add(NewJob);
	MarkQueueDirty();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobQueue] Enqueued job %s (Type: %d, Repeat: %d)"),
		*NewJob.JobId.ToString(), static_cast<int32>(JobType), RepeatCount);

	OnQueueChanged.Broadcast();
	return NewJob.JobId;
}

FGuid UMOSurvivorJobQueueComponent::EnqueueJobAtLocation(EMOSurvivorJobType JobType, FVector Location, int32 RepeatCount)
{
	if (JobType == EMOSurvivorJobType::None)
	{
		return FGuid();
	}

	if (!GetOwner()->HasAuthority())
	{
		return FGuid();
	}

	FMOSurvivorJobEntry NewJob = CreateJobEntry(JobType, RepeatCount);
	NewJob.TargetLocation = Location;
	JobQueue.Jobs.Add(NewJob);
	MarkQueueDirty();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobQueue] Enqueued location job %s at %s"),
		*NewJob.JobId.ToString(), *Location.ToString());

	OnQueueChanged.Broadcast();
	return NewJob.JobId;
}

FGuid UMOSurvivorJobQueueComponent::EnqueueJobWithTarget(EMOSurvivorJobType JobType, AActor* Target, int32 RepeatCount)
{
	if (JobType == EMOSurvivorJobType::None || !IsValid(Target))
	{
		return FGuid();
	}

	if (!GetOwner()->HasAuthority())
	{
		return FGuid();
	}

	FMOSurvivorJobEntry NewJob = CreateJobEntry(JobType, RepeatCount);
	NewJob.TargetActor = Target;
	NewJob.TargetLocation = Target->GetActorLocation();
	JobQueue.Jobs.Add(NewJob);
	MarkQueueDirty();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobQueue] Enqueued target job %s for actor %s"),
		*NewJob.JobId.ToString(), *Target->GetName());

	OnQueueChanged.Broadcast();
	return NewJob.JobId;
}

FGuid UMOSurvivorJobQueueComponent::EnqueueCraftJob(FName RecipeId, AActor* Station, AActor* Storage, int32 RepeatCount)
{
	if (RecipeId.IsNone() || !IsValid(Station) || !IsValid(Storage))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorJobQueue] EnqueueCraftJob rejected: recipe/station/storage missing"));
		return FGuid();
	}

	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorJobQueue] EnqueueCraftJob called on non-authority"));
		return FGuid();
	}

	FMOSurvivorJobEntry NewJob = CreateJobEntry(EMOSurvivorJobType::CraftAtStation, RepeatCount);
	NewJob.TargetActor = Station;                     // station in the shared target slot
	NewJob.TargetLocation = Station->GetActorLocation();
	NewJob.CraftRecipeId = RecipeId;
	NewJob.StorageActor = Storage;

	// Best-effort GUIDs so a saved/replicated job can re-resolve its actors.
	if (const UMOIdentityComponent* StationId = Station->FindComponentByClass<UMOIdentityComponent>())
	{
		NewJob.TargetActorGuid = StationId->GetGuid();
	}
	if (const UMOIdentityComponent* StorageId = Storage->FindComponentByClass<UMOIdentityComponent>())
	{
		NewJob.StorageActorGuid = StorageId->GetGuid();
	}

	JobQueue.Jobs.Add(NewJob);
	MarkQueueDirty();

	UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorJobQueue] Enqueued craft job %s: %dx %s at %s, storage %s"),
		*NewJob.JobId.ToString(), RepeatCount, *RecipeId.ToString(), *Station->GetName(), *Storage->GetName());

	OnQueueChanged.Broadcast();
	return NewJob.JobId;
}

FGuid UMOSurvivorJobQueueComponent::EnqueueRefuelJob(AActor* Station, AActor* Storage, FName FuelItemId, int32 Quantity)
{
	if (!IsValid(Station) || !IsValid(Storage) || FuelItemId.IsNone() || Quantity <= 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorJobQueue] EnqueueRefuelJob rejected: station/storage/fuel missing"));
		return FGuid();
	}

	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorJobQueue] EnqueueRefuelJob called on non-authority"));
		return FGuid();
	}

	FMOSurvivorJobEntry NewJob = CreateJobEntry(EMOSurvivorJobType::RefuelStation, 1);
	NewJob.TargetActor = Station;                     // station in the shared target slot
	NewJob.TargetLocation = Station->GetActorLocation();
	NewJob.StorageActor = Storage;
	NewJob.FuelItemId = FuelItemId;
	NewJob.FuelQuantity = Quantity;

	// Best-effort GUIDs so a saved/replicated job can re-resolve its actors.
	if (const UMOIdentityComponent* StationId = Station->FindComponentByClass<UMOIdentityComponent>())
	{
		NewJob.TargetActorGuid = StationId->GetGuid();
	}
	if (const UMOIdentityComponent* StorageId = Storage->FindComponentByClass<UMOIdentityComponent>())
	{
		NewJob.StorageActorGuid = StorageId->GetGuid();
	}

	JobQueue.Jobs.Add(NewJob);
	MarkQueueDirty();

	UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorJobQueue] Enqueued refuel job %s: %dx %s to %s from %s"),
		*NewJob.JobId.ToString(), Quantity, *FuelItemId.ToString(), *Station->GetName(), *Storage->GetName());

	OnQueueChanged.Broadcast();
	return NewJob.JobId;
}

bool UMOSurvivorJobQueueComponent::CancelJob(const FGuid& JobId)
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}

	const int32 Index = JobQueue.FindJobIndexById(JobId);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	FMOSurvivorJobEntry& Job = JobQueue.Jobs[Index];
	Job.State = EMOSurvivorJobState::Cancelled;

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobQueue] Cancelled job %s"), *JobId.ToString());

	// Remove the cancelled job
	JobQueue.Jobs.RemoveAt(Index);
	MarkQueueDirty();

	OnQueueChanged.Broadcast();
	return true;
}

void UMOSurvivorJobQueueComponent::CancelAllJobs()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobQueue] Cancelling all %d jobs"), JobQueue.Jobs.Num());

	JobQueue.Jobs.Empty();
	MarkQueueDirty();

	OnQueueChanged.Broadcast();
}

bool UMOSurvivorJobQueueComponent::ReorderJob(const FGuid& JobId, int32 NewIndex)
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}

	const int32 CurrentIndex = JobQueue.FindJobIndexById(JobId);
	if (CurrentIndex == INDEX_NONE)
	{
		return false;
	}

	// Can't move ahead of an active job
	if (NewIndex == 0 && JobQueue.Jobs.Num() > 0 && JobQueue.Jobs[0].IsActive())
	{
		NewIndex = 1;
	}

	NewIndex = FMath::Clamp(NewIndex, 0, JobQueue.Jobs.Num() - 1);

	if (CurrentIndex == NewIndex)
	{
		return true;
	}

	FMOSurvivorJobEntry Job = JobQueue.Jobs[CurrentIndex];
	JobQueue.Jobs.RemoveAt(CurrentIndex);
	JobQueue.Jobs.Insert(Job, NewIndex);
	MarkQueueDirty();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobQueue] Reordered job %s from %d to %d"),
		*JobId.ToString(), CurrentIndex, NewIndex);

	OnQueueChanged.Broadcast();
	return true;
}

// ============================================================================
// JOB STATE
// ============================================================================

void UMOSurvivorJobQueueComponent::ResolveJobActorRefs(FMOSurvivorJobEntry& Job) const
{
	const bool bNeedTarget = Job.TargetActorGuid.IsValid() && !Job.TargetActor.IsValid();
	const bool bNeedStorage = Job.StorageActorGuid.IsValid() && !Job.StorageActor.IsValid();
	if (!bNeedTarget && !bNeedStorage)
	{
		return; // live-session fast path — refs already valid, or the job has no actor GUIDs
	}

	const UWorld* World = GetWorld();
	UMOIdentityRegistrySubsystem* Registry = World ? World->GetSubsystem<UMOIdentityRegistrySubsystem>() : nullptr;
	if (!Registry)
	{
		return;
	}
	if (bNeedTarget)
	{
		Job.TargetActor = Registry->ResolveActorOrNull(Job.TargetActorGuid);
	}
	if (bNeedStorage)
	{
		Job.StorageActor = Registry->ResolveActorOrNull(Job.StorageActorGuid);
	}
}

FMOSurvivorJobEntry UMOSurvivorJobQueueComponent::GetCurrentJob() const
{
	const FMOSurvivorJobEntry* Current = JobQueue.GetCurrentJob();
	if (Current)
	{
		FMOSurvivorJobEntry Copy = *Current;
		// After a disk load the weak ptrs are null but the GUIDs survive —
		// rehydrate at consumption (ordering-immune: the world is fully loaded
		// by the time a job is picked up). No-op in the live session.
		ResolveJobActorRefs(Copy);
		return Copy;
	}
	return FMOSurvivorJobEntry();
}

AActor* UMOSurvivorJobQueueComponent::GetCurrentJobTargetActor() const
{
	return GetCurrentJob().TargetActor.Get();
}

TArray<FMOSurvivorJobEntry> UMOSurvivorJobQueueComponent::GetAllJobs() const
{
	return JobQueue.Jobs;
}

void UMOSurvivorJobQueueComponent::SetJobState(const FGuid& JobId, EMOSurvivorJobState NewState)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	FMOSurvivorJobEntry* Job = JobQueue.FindJobById(JobId);
	if (!Job)
	{
		return;
	}

	const EMOSurvivorJobState OldState = Job->State;
	if (OldState == NewState)
	{
		return;
	}

	Job->State = NewState;

	if (NewState == EMOSurvivorJobState::Active && OldState == EMOSurvivorJobState::Queued)
	{
		Job->StartTime = FDateTime::UtcNow();
	}

	MarkQueueDirty();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobQueue] Job %s state: %d -> %d"),
		*JobId.ToString(), static_cast<int32>(OldState), static_cast<int32>(NewState));

	OnJobStateChanged.Broadcast(JobId, NewState);
}

void UMOSurvivorJobQueueComponent::UpdateJobProgress(const FGuid& JobId, float Progress)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	FMOSurvivorJobEntry* Job = JobQueue.FindJobById(JobId);
	if (!Job)
	{
		return;
	}

	Job->Progress = FMath::Clamp(Progress, 0.0f, 1.0f);
	MarkQueueDirty();
}

void UMOSurvivorJobQueueComponent::CompleteCurrentJob(bool bSuccess)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	FMOSurvivorJobEntry* CurrentJob = JobQueue.GetCurrentJob();
	if (!CurrentJob)
	{
		return;
	}

	const FGuid JobId = CurrentJob->JobId;
	CurrentJob->State = bSuccess ? EMOSurvivorJobState::Completed : EMOSurvivorJobState::Failed;
	CurrentJob->Progress = bSuccess ? 1.0f : CurrentJob->Progress;

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobQueue] Job %s completed (Success: %s)"),
		*JobId.ToString(), bSuccess ? TEXT("true") : TEXT("false"));

	// Remove the job from queue
	const int32 Index = JobQueue.FindJobIndexById(JobId);
	if (Index != INDEX_NONE)
	{
		JobQueue.Jobs.RemoveAt(Index);
	}

	MarkQueueDirty();

	OnJobCompleted.Broadcast(JobId, bSuccess);
	OnQueueChanged.Broadcast();
}

void UMOSurvivorJobQueueComponent::CompleteCurrentIteration()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	FMOSurvivorJobEntry* CurrentJob = JobQueue.GetCurrentJob();
	if (!CurrentJob)
	{
		return;
	}

	CurrentJob->CompletedCount++;
	CurrentJob->Progress = 0.0f;

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobQueue] Job %s iteration %d/%d completed"),
		*CurrentJob->JobId.ToString(), CurrentJob->CompletedCount, CurrentJob->RepeatCount);

	if (!CurrentJob->ShouldRepeat())
	{
		// All iterations done
		CompleteCurrentJob(true);
	}
	else
	{
		// More iterations remaining
		MarkQueueDirty();
		OnQueueChanged.Broadcast();
	}
}

// ============================================================================
// HOME LOCATION
// ============================================================================

void UMOSurvivorJobQueueComponent::SetHome(FVector Location, FGuid BuildingGuid)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	HomeLocation = Location;
	HomeBuildingGuid = BuildingGuid;

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobQueue] Home set to %s (Building: %s)"),
		*Location.ToString(), BuildingGuid.IsValid() ? *BuildingGuid.ToString() : TEXT("None"));
}

void UMOSurvivorJobQueueComponent::ClearHome()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	HomeLocation = FVector::ZeroVector;
	HomeBuildingGuid = FGuid();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobQueue] Home cleared"));
}

// ============================================================================
// JOB DEFINITIONS
// ============================================================================

bool UMOSurvivorJobQueueComponent::GetJobDefinition(EMOSurvivorJobType JobType, FMOSurvivorJobDefinitionRow& OutDefinition) const
{
	return UMOSurvivorJobDatabaseSettings::GetJobDefinitionBP(JobType, OutDefinition);
}

const FMOSurvivorJobDefinitionRow* UMOSurvivorJobQueueComponent::GetJobDefinitionPtr(EMOSurvivorJobType JobType) const
{
	return UMOSurvivorJobDatabaseSettings::GetJobDefinition(JobType);
}

TArray<FMOSurvivorJobDefinitionRow> UMOSurvivorJobQueueComponent::GetAllJobDefinitions(bool bExcludeCommands) const
{
	TArray<FMOSurvivorJobDefinitionRow> Result;

	if (bExcludeCommands)
	{
		UMOSurvivorJobDatabaseSettings::GetAllWorkTasks(Result);
	}
	else
	{
		UMOSurvivorJobDatabaseSettings::GetAllJobDefinitions(Result);
	}

	return Result;
}

// ============================================================================
// SAVE/LOAD
// ============================================================================

void UMOSurvivorJobQueueComponent::BuildSaveData(FMOSurvivorJobQueueSaveData& OutSaveData) const
{
	OutSaveData.Jobs = JobQueue.Jobs;
	OutSaveData.HomeLocation = HomeLocation;
	OutSaveData.HomeBuildingGuid = HomeBuildingGuid;
	OutSaveData.bHasValidData = true;
}

bool UMOSurvivorJobQueueComponent::ApplySaveDataAuthority(const FMOSurvivorJobQueueSaveData& InSaveData)
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}

	if (!InSaveData.bHasValidData)
	{
		return false;
	}

	JobQueue.Jobs = InSaveData.Jobs;
	HomeLocation = InSaveData.HomeLocation;
	HomeBuildingGuid = InSaveData.HomeBuildingGuid;

	// Best-effort eager rehydration of actor refs from their persisted GUIDs
	// (weak ptrs aren't serialized). Any target not yet spawned at restore
	// time is caught lazily by GetCurrentJob. (H39 residual)
	for (FMOSurvivorJobEntry& Job : JobQueue.Jobs)
	{
		ResolveJobActorRefs(Job);
	}

	MarkQueueDirty();
	OnQueueChanged.Broadcast();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorJobQueue] Applied save data: %d jobs, Home: %s"),
		JobQueue.Jobs.Num(), HasHome() ? TEXT("Yes") : TEXT("No"));

	return true;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

FMOSurvivorJobEntry UMOSurvivorJobQueueComponent::CreateJobEntry(EMOSurvivorJobType JobType, int32 RepeatCount)
{
	FMOSurvivorJobEntry Entry;
	Entry.JobId = FGuid::NewGuid();
	Entry.JobType = JobType;
	Entry.State = EMOSurvivorJobState::Queued;
	Entry.RepeatCount = FMath::Max(1, RepeatCount);
	Entry.CompletedCount = 0;
	Entry.Progress = 0.0f;
	return Entry;
}

void UMOSurvivorJobQueueComponent::CleanupFinishedJobs()
{
	// Remove completed, failed, or cancelled jobs
	JobQueue.Jobs.RemoveAll([](const FMOSurvivorJobEntry& Job)
	{
		return Job.State == EMOSurvivorJobState::Completed ||
			Job.State == EMOSurvivorJobState::Failed ||
			Job.State == EMOSurvivorJobState::Cancelled;
	});
}

void UMOSurvivorJobQueueComponent::MarkQueueDirty()
{
	JobQueue.MarkArrayDirty();
}
