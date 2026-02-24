#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MOSurvivorJobTypes.generated.h"

/**
 * Types of jobs that survivors can perform.
 */
UENUM(BlueprintType)
enum class EMOSurvivorJobType : uint8
{
	None,

	// Gathering jobs
	GatherWood,
	GatherStone,
	GatherFiber,

	// Foraging jobs
	ForageNearby,
	DigForSupplies,

	// Commands (immediate, not queued)
	FollowTarget,
	StayAtLocation,
	GoHome,

	// Future expansion
	// Hunt,
	// Farm,
	// Craft,
	// Build,
	// Guard,
};

/**
 * State of a survivor job in the queue.
 */
UENUM(BlueprintType)
enum class EMOSurvivorJobState : uint8
{
	Queued,			// Waiting in queue
	Active,			// Currently being processed
	MovingToTarget,	// Moving towards job target
	Performing,		// Executing the job action
	Returning,		// Returning to home/storage
	Completed,		// Job completed successfully
	Failed,			// Job failed
	Cancelled		// Job was cancelled
};

/**
 * A single entry in a survivor's job queue.
 * Uses FastArraySerializerItem for efficient network replication.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSurvivorJobEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Unique identifier for this job entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (IgnoreForMemberInitializationTest))
	FGuid JobId;

	/** Type of job to perform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	EMOSurvivorJobType JobType = EMOSurvivorJobType::None;

	/** Current state of this job. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	EMOSurvivorJobState State = EMOSurvivorJobState::Queued;

	/** Number of times to repeat this job. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	int32 RepeatCount = 1;

	/** Number of times this job has been completed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	int32 CompletedCount = 0;

	/** Target location for location-based jobs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	FVector TargetLocation = FVector::ZeroVector;

	/** Target actor for actor-based jobs (not replicated - resolved via GUID if needed). */
	TWeakObjectPtr<AActor> TargetActor;

	/** Target actor GUID for replication/persistence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (IgnoreForMemberInitializationTest))
	FGuid TargetActorGuid;

	/** Progress of current iteration (0.0 - 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	float Progress = 0.0f;

	/** When this job was started. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (IgnoreForMemberInitializationTest))
	FDateTime StartTime;

	FMOSurvivorJobEntry()
		: JobId(FGuid::NewGuid())
	{
	}

	bool IsValid() const { return JobType != EMOSurvivorJobType::None; }

	bool IsComplete() const { return State == EMOSurvivorJobState::Completed; }

	bool IsFailed() const { return State == EMOSurvivorJobState::Failed || State == EMOSurvivorJobState::Cancelled; }

	bool IsActive() const
	{
		return State == EMOSurvivorJobState::Active ||
			State == EMOSurvivorJobState::MovingToTarget ||
			State == EMOSurvivorJobState::Performing ||
			State == EMOSurvivorJobState::Returning;
	}

	/** Check if this job should repeat (has remaining iterations). */
	bool ShouldRepeat() const { return CompletedCount < RepeatCount; }

	/** Get remaining iterations. */
	int32 GetRemainingCount() const { return FMath::Max(0, RepeatCount - CompletedCount); }
};

// Forward declare the component for FastArray owner
class UMOSurvivorJobQueueComponent;

/**
 * FastArraySerializer container for survivor job queue.
 * Provides efficient network delta serialization.
 */
USTRUCT()
struct MOFRAMEWORK_API FMOSurvivorJobList : public FFastArraySerializer
{
	GENERATED_BODY()

	/** The list of jobs in the queue. */
	UPROPERTY()
	TArray<FMOSurvivorJobEntry> Jobs;

	/** Owner component for delegate callbacks. */
	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UMOSurvivorJobQueueComponent> OwnerComponent;

	// FastArraySerializer callbacks
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMOSurvivorJobEntry, FMOSurvivorJobList>(Jobs, DeltaParams, *this);
	}

	// Helper methods
	FMOSurvivorJobEntry* FindJobById(const FGuid& JobId);
	const FMOSurvivorJobEntry* FindJobById(const FGuid& JobId) const;
	int32 FindJobIndexById(const FGuid& JobId) const;
	FMOSurvivorJobEntry* GetCurrentJob();
	const FMOSurvivorJobEntry* GetCurrentJob() const;
};

template<>
struct TStructOpsTypeTraits<FMOSurvivorJobList> : public TStructOpsTypeTraitsBase2<FMOSurvivorJobList>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

/**
 * DataTable row defining a survivor job type.
 * Used for UI display, skill requirements, XP rewards, etc.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSurvivorJobDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Job type this row defines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	EMOSurvivorJobType JobType = EMOSurvivorJobType::None;

	/** Display name shown in UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	FText DisplayName;

	/** Description shown in UI tooltips. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	FText Description;

	/** Icon for the job (optional). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Category for grouping in task menu (e.g., "Gathering", "Foraging", "Commands"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	FName Category;

	/** Base duration for one iteration of this job (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (ClampMin = "0"))
	float BaseDuration = 30.0f;

	/** Search radius for finding resources (for gathering/foraging jobs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job", meta = (ClampMin = "0"))
	float SearchRadius = 500.0f;

	/** Skill ID that gains experience from this job. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skills")
	FName SkillId;

	/** Base XP awarded per completion of this job. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skills", meta = (ClampMin = "0"))
	float BaseXP = 5.0f;

	/** Tools required for this job (optional). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirements")
	TArray<FName> RequiredTools;

	/** Whether this job can be repeated (added multiple times to queue). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	bool bIsRepeatable = true;

	/** Whether this is a command (immediate) rather than a work task (queued). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Job")
	bool bIsCommand = false;

	/** Priority level for sorting in UI (higher = appears first). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	int32 UIPriority = 0;
};

/**
 * Save data for survivor job queue.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSurvivorJobQueueSaveData
{
	GENERATED_BODY()

	/** All jobs in the queue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	TArray<FMOSurvivorJobEntry> Jobs;

	/** Home location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FVector HomeLocation = FVector::ZeroVector;

	/** GUID of the home building (if any). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (IgnoreForMemberInitializationTest))
	FGuid HomeBuildingGuid;

	/** Whether this save data is valid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	bool bHasValidData = false;
};
