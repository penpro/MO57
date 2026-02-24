#include "MOSurvivorJobTypes.h"
#include "MOSurvivorJobQueueComponent.h"

void FMOSurvivorJobList::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	if (UMOSurvivorJobQueueComponent* Owner = OwnerComponent.Get())
	{
		Owner->OnQueueChanged.Broadcast();
	}
}

void FMOSurvivorJobList::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	if (UMOSurvivorJobQueueComponent* Owner = OwnerComponent.Get())
	{
		for (int32 Index : ChangedIndices)
		{
			if (Jobs.IsValidIndex(Index))
			{
				const FMOSurvivorJobEntry& Job = Jobs[Index];
				Owner->OnJobStateChanged.Broadcast(Job.JobId, Job.State);
			}
		}
		Owner->OnQueueChanged.Broadcast();
	}
}

void FMOSurvivorJobList::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	if (UMOSurvivorJobQueueComponent* Owner = OwnerComponent.Get())
	{
		Owner->OnQueueChanged.Broadcast();
	}
}

FMOSurvivorJobEntry* FMOSurvivorJobList::FindJobById(const FGuid& JobId)
{
	for (FMOSurvivorJobEntry& Job : Jobs)
	{
		if (Job.JobId == JobId)
		{
			return &Job;
		}
	}
	return nullptr;
}

const FMOSurvivorJobEntry* FMOSurvivorJobList::FindJobById(const FGuid& JobId) const
{
	for (const FMOSurvivorJobEntry& Job : Jobs)
	{
		if (Job.JobId == JobId)
		{
			return &Job;
		}
	}
	return nullptr;
}

int32 FMOSurvivorJobList::FindJobIndexById(const FGuid& JobId) const
{
	for (int32 Index = 0; Index < Jobs.Num(); ++Index)
	{
		if (Jobs[Index].JobId == JobId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FMOSurvivorJobEntry* FMOSurvivorJobList::GetCurrentJob()
{
	// Current job is the first active job, or the first queued job
	for (FMOSurvivorJobEntry& Job : Jobs)
	{
		if (Job.IsActive())
		{
			return &Job;
		}
	}
	for (FMOSurvivorJobEntry& Job : Jobs)
	{
		if (Job.State == EMOSurvivorJobState::Queued)
		{
			return &Job;
		}
	}
	return nullptr;
}

const FMOSurvivorJobEntry* FMOSurvivorJobList::GetCurrentJob() const
{
	for (const FMOSurvivorJobEntry& Job : Jobs)
	{
		if (Job.IsActive())
		{
			return &Job;
		}
	}
	for (const FMOSurvivorJobEntry& Job : Jobs)
	{
		if (Job.State == EMOSurvivorJobState::Queued)
		{
			return &Job;
		}
	}
	return nullptr;
}
