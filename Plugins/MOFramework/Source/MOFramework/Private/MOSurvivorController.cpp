#include "MOSurvivorController.h"
#include "MOBlackboardKeys.h"
#include "MOFramework.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOSurvivorJobDatabaseSettings.h"
#include "MOSkillsComponent.h"
#include "MOForagingSubsystem.h"
#include "MOPCGInteractionSubsystem.h"
#include "MOHarvestSubsystem.h"
#include "MOHISMHarvestHelper.h"
#include "MOItemDatabaseSettings.h"
#include "MOInventoryComponent.h"
#include "MOTerraformingComponent.h"   // (unit 3) TerraformAtLocationEx + excavation config
#include "MODesignationSubsystem.h"    // (unit 3) dig/dump zone lookup + budget
#include "MOKnowledgeComponent.h"
#include "MOCraftingSubsystem.h"
#include "MOCraftingQueueComponent.h"
#include "MOCraftingStationActor.h"
#include "MORecipeDatabaseSettings.h"
#include "MOInventoryHolderInterface.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "EngineUtils.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

AMOSurvivorController::AMOSurvivorController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AMOSurvivorController::BeginPlay()
{
	Super::BeginPlay();
}

void AMOSurvivorController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update follow behavior
	if (bIsFollowing)
	{
		UpdateFollowBehavior(DeltaTime);
	}

	// Update simple job execution
	if (bIsProcessingJob && SimpleJobState > 0)
	{
		UpdateSimpleJobExecution(DeltaTime);
	}
}

void AMOSurvivorController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!InPawn)
	{
		return;
	}

	// Cache components
	CachedJobQueue = InPawn->FindComponentByClass<UMOSurvivorJobQueueComponent>();
	CachedSkillsComponent = InPawn->FindComponentByClass<UMOSkillsComponent>();

	// Bind to job queue changes
	if (UMOSurvivorJobQueueComponent* JobQueue = CachedJobQueue.Get())
	{
		JobQueue->OnQueueChanged.RemoveDynamic(this, &AMOSurvivorController::HandleJobQueueChanged);
		JobQueue->OnQueueChanged.AddDynamic(this, &AMOSurvivorController::HandleJobQueueChanged);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Possessed %s (JobQueue: %s, Skills: %s)"),
		*InPawn->GetName(),
		CachedJobQueue.IsValid() ? TEXT("Yes") : TEXT("No"),
		CachedSkillsComponent.IsValid() ? TEXT("Yes") : TEXT("No"));

	// Run survivor behavior tree if set
	if (DefaultSurvivorBehaviorTree.IsValid())
	{
		if (UBehaviorTree* BT = DefaultSurvivorBehaviorTree.LoadSynchronous())
		{
			RunBehaviorTree(BT);
			SetupBlackboardForSurvivor();
		}
	}
	else if (DefaultBehaviorTree.IsValid())
	{
		// Fall back to base default
		if (UBehaviorTree* BT = DefaultBehaviorTree.LoadSynchronous())
		{
			RunBehaviorTree(BT);
			SetupBlackboardForSurvivor();
		}
	}
}

void AMOSurvivorController::OnUnPossess()
{
	// Unbind from job queue
	if (UMOSurvivorJobQueueComponent* JobQueue = CachedJobQueue.Get())
	{
		JobQueue->OnQueueChanged.RemoveDynamic(this, &AMOSurvivorController::HandleJobQueueChanged);
	}

	// Clear state
	ClearAllCommands();
	CachedJobQueue.Reset();
	CachedSkillsComponent.Reset();

	Super::OnUnPossess();
}

// ============================================================================
// COMMANDS
// ============================================================================

void AMOSurvivorController::SetFollowTarget(AActor* Target)
{
	if (!IsValid(Target))
	{
		StopFollowing();
		return;
	}

	// Clear conflicting commands
	bShouldStay = false;
	StayLocation = FVector::ZeroVector;
	bIsGoingHome = false;
	bIsProcessingJob = false;

	FollowTarget = Target;
	bIsFollowing = true;

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Now following %s"), *Target->GetName());

	SetupBlackboardForSurvivor();
	BroadcastCommandChange(TEXT("Follow"));
}

void AMOSurvivorController::StopFollowing()
{
	if (!bIsFollowing)
	{
		return;
	}

	bIsFollowing = false;
	FollowTarget.Reset();
	StopCurrentMovement();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Stopped following"));

	ClearSurvivorBlackboardData();
	BroadcastCommandChange(TEXT("Idle"));
}

void AMOSurvivorController::SetStayAtLocation(FVector Location)
{
	// Clear conflicting commands
	StopFollowing();
	bIsGoingHome = false;
	bIsProcessingJob = false;

	bShouldStay = true;
	StayLocation = Location;

	// Move to the stay location
	MoveToLocationWithRadius(Location, AcceptanceRadius);

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Staying at %s"), *Location.ToString());

	SetupBlackboardForSurvivor();
	BroadcastCommandChange(TEXT("Stay"));
}

void AMOSurvivorController::ClearStayLocation()
{
	if (!bShouldStay)
	{
		return;
	}

	bShouldStay = false;
	StayLocation = FVector::ZeroVector;
	StopCurrentMovement();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Cleared stay location"));

	ClearSurvivorBlackboardData();
	BroadcastCommandChange(TEXT("Idle"));
}

void AMOSurvivorController::GoToHome()
{
	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	if (!JobQueue || !JobQueue->HasHome())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Cannot go home - no home assigned"));
		return;
	}

	// Clear conflicting commands
	StopFollowing();
	bShouldStay = false;
	StayLocation = FVector::ZeroVector;
	bIsProcessingJob = false;

	bIsGoingHome = true;
	const FVector HomeLocation = JobQueue->HomeLocation;

	// Move to home
	MoveToLocationWithRadius(HomeLocation, AcceptanceRadius);

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Going home to %s"), *HomeLocation.ToString());

	SetupBlackboardForSurvivor();
	BroadcastCommandChange(TEXT("GoHome"));
}

void AMOSurvivorController::ClearAllCommands()
{
	bIsFollowing = false;
	FollowTarget.Reset();
	bShouldStay = false;
	StayLocation = FVector::ZeroVector;
	bIsGoingHome = false;
	bIsProcessingJob = false;
	CurrentJob = FMOSurvivorJobEntry();

	StopCurrentMovement();
	ClearSurvivorBlackboardData();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Cleared all commands"));

	BroadcastCommandChange(TEXT("Idle"));
}

// ============================================================================
// STATE QUERIES
// ============================================================================

UMOSurvivorJobQueueComponent* AMOSurvivorController::GetJobQueue() const
{
	if (CachedJobQueue.IsValid())
	{
		return CachedJobQueue.Get();
	}

	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn)
	{
		return ControlledPawn->FindComponentByClass<UMOSurvivorJobQueueComponent>();
	}

	return nullptr;
}

UMOSkillsComponent* AMOSurvivorController::GetSkillsComponent() const
{
	if (CachedSkillsComponent.IsValid())
	{
		return CachedSkillsComponent.Get();
	}

	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn)
	{
		return ControlledPawn->FindComponentByClass<UMOSkillsComponent>();
	}

	return nullptr;
}

FName AMOSurvivorController::GetActiveCommandName() const
{
	if (bIsFollowing)
	{
		return TEXT("Following");
	}
	if (bShouldStay)
	{
		return TEXT("Staying");
	}
	if (bIsGoingHome)
	{
		return TEXT("Going Home");
	}
	if (bIsProcessingJob)
	{
		// Return job type name
		if (CurrentJob.IsValid())
		{
			UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
			if (JobQueue)
			{
				const FMOSurvivorJobDefinitionRow* JobDef = JobQueue->GetJobDefinitionPtr(CurrentJob.JobType);
				if (JobDef)
				{
					return *JobDef->DisplayName.ToString();
				}
			}
			// Fallback to enum name
			return FName(*UEnum::GetValueAsString(CurrentJob.JobType));
		}
	}
	return TEXT("Idle");
}

// ============================================================================
// JOB PROCESSING
// ============================================================================

void AMOSurvivorController::ProcessNextJob()
{
	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	if (!JobQueue || !JobQueue->HasJobs())
	{
		bIsProcessingJob = false;
		CurrentJob = FMOSurvivorJobEntry();
		return;
	}

	// Clear other commands
	StopFollowing();
	bShouldStay = false;
	StayLocation = FVector::ZeroVector;
	bIsGoingHome = false;

	// Get next job
	CurrentJob = JobQueue->GetCurrentJob();
	if (!CurrentJob.IsValid())
	{
		bIsProcessingJob = false;
		return;
	}

	bIsProcessingJob = true;

	// Mark job as active
	JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Active);

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Processing job: %s (Type: %d)"),
		*CurrentJob.JobId.ToString(), static_cast<int32>(CurrentJob.JobType));

	// Setup blackboard for behavior tree
	SetupBlackboardForSurvivor();
	BroadcastCommandChange(TEXT("Working"));

	// For simple job types, use direct execution instead of BT
	if (CanExecuteSimply(CurrentJob.JobType))
	{
		StartSimpleJobExecution();
	}
}

void AMOSurvivorController::AbortCurrentJob()
{
	if (!bIsProcessingJob)
	{
		return;
	}

	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	if (JobQueue && CurrentJob.IsValid())
	{
		JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Failed);
	}

	bIsProcessingJob = false;
	CurrentJob = FMOSurvivorJobEntry();
	StopCurrentMovement();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Aborted current job"));

	ClearSurvivorBlackboardData();
	BroadcastCommandChange(TEXT("Idle"));
}

void AMOSurvivorController::AwardJobExperience(EMOSurvivorJobType JobType)
{
	UMOSkillsComponent* Skills = GetSkillsComponent();
	if (!Skills)
	{
		return;
	}

	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	if (!JobQueue)
	{
		return;
	}

	const FMOSurvivorJobDefinitionRow* JobDef = JobQueue->GetJobDefinitionPtr(JobType);
	if (!JobDef || JobDef->SkillId.IsNone())
	{
		return;
	}

	Skills->AddExperience(JobDef->SkillId, JobDef->BaseXP);

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Awarded %.1f XP to skill %s for job type %d"),
		JobDef->BaseXP, *JobDef->SkillId.ToString(), static_cast<int32>(JobType));
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void AMOSurvivorController::UpdateFollowBehavior(float DeltaTime)
{
	if (!bIsFollowing || !FollowTarget.IsValid())
	{
		StopFollowing();
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	AActor* Target = FollowTarget.Get();
	const float DistanceToTarget = FVector::Dist(ControlledPawn->GetActorLocation(), Target->GetActorLocation());

	// Only move if we're too far from the follow distance
	if (DistanceToTarget > FollowStartDistance)
	{
		// Log movement debug info periodically
		static float LastLogTime = 0.f;
		UWorld* World = GetWorld();
		if (World && (World->GetTimeSeconds() - LastLogTime) > 2.0f)
		{
			const FVector Velocity = ControlledPawn->GetVelocity();
			UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Following: distance=%.1f, velocity=%.1f (%.1f, %.1f, %.1f)"),
				DistanceToTarget, Velocity.Size(), Velocity.X, Velocity.Y, Velocity.Z);
			LastLogTime = World->GetTimeSeconds();
		}

		// Move towards target with follow distance as acceptance radius
		MoveToActorWithRadius(Target, FollowDistance);
	}
}

void AMOSurvivorController::SetupBlackboardForSurvivor()
{
	// Only access blackboard if it has a valid asset
	if (!BlackboardComponent || !BlackboardComponent->GetBlackboardAsset())
	{
		return;
	}

	// Set survivor-specific blackboard keys
	BlackboardComponent->SetValueAsBool(MOBlackboardKeys::IsFollowing, bIsFollowing);
	BlackboardComponent->SetValueAsBool(MOBlackboardKeys::ShouldStay, bShouldStay);
	BlackboardComponent->SetValueAsBool(MOBlackboardKeys::IsGoingHome, bIsGoingHome);
	BlackboardComponent->SetValueAsBool(MOBlackboardKeys::IsProcessingJob, bIsProcessingJob);

	if (bIsFollowing && FollowTarget.IsValid())
	{
		BlackboardComponent->SetValueAsObject(MOBlackboardKeys::FollowTarget, FollowTarget.Get());
	}
	else
	{
		BlackboardComponent->ClearValue(MOBlackboardKeys::FollowTarget);
	}

	if (bShouldStay)
	{
		BlackboardComponent->SetValueAsVector(MOBlackboardKeys::StayLocation, StayLocation);
	}
	else
	{
		BlackboardComponent->ClearValue(MOBlackboardKeys::StayLocation);
	}

	if (bIsGoingHome)
	{
		UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
		if (JobQueue && JobQueue->HasHome())
		{
			BlackboardComponent->SetValueAsVector(MOBlackboardKeys::HomeLocation, JobQueue->HomeLocation);
		}
	}
	else
	{
		BlackboardComponent->ClearValue(MOBlackboardKeys::HomeLocation);
	}

	if (bIsProcessingJob && CurrentJob.IsValid())
	{
		BlackboardComponent->SetValueAsInt(MOBlackboardKeys::CurrentJobType, static_cast<int32>(CurrentJob.JobType));
		BlackboardComponent->SetValueAsVector(MOBlackboardKeys::JobTargetLocation, CurrentJob.TargetLocation);
		if (CurrentJob.TargetActor.IsValid())
		{
			BlackboardComponent->SetValueAsObject(MOBlackboardKeys::JobTargetActor, CurrentJob.TargetActor.Get());
		}
	}
	else
	{
		BlackboardComponent->ClearValue(MOBlackboardKeys::CurrentJobType);
		BlackboardComponent->ClearValue(MOBlackboardKeys::JobTargetLocation);
		BlackboardComponent->ClearValue(MOBlackboardKeys::JobTargetActor);
	}
}

void AMOSurvivorController::ClearSurvivorBlackboardData()
{
	// Only access blackboard if it has a valid asset
	if (!BlackboardComponent || !BlackboardComponent->GetBlackboardAsset())
	{
		return;
	}

	BlackboardComponent->SetValueAsBool(MOBlackboardKeys::IsFollowing, false);
	BlackboardComponent->SetValueAsBool(MOBlackboardKeys::ShouldStay, false);
	BlackboardComponent->SetValueAsBool(MOBlackboardKeys::IsGoingHome, false);
	BlackboardComponent->SetValueAsBool(MOBlackboardKeys::IsProcessingJob, false);
	BlackboardComponent->ClearValue(MOBlackboardKeys::FollowTarget);
	BlackboardComponent->ClearValue(MOBlackboardKeys::StayLocation);
	BlackboardComponent->ClearValue(MOBlackboardKeys::HomeLocation);
	BlackboardComponent->ClearValue(MOBlackboardKeys::CurrentJobType);
	BlackboardComponent->ClearValue(MOBlackboardKeys::JobTargetLocation);
	BlackboardComponent->ClearValue(MOBlackboardKeys::JobTargetActor);
}

void AMOSurvivorController::BroadcastCommandChange(FName CommandName)
{
	OnCommandChanged.Broadcast(CommandName);
}

void AMOSurvivorController::HandleJobQueueChanged()
{
	// Skip if already processing a job
	if (bIsProcessingJob)
	{
		return;
	}

	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	if (!JobQueue || !JobQueue->HasJobs())
	{
		return;
	}

	// If following/staying/going home, clear those commands to start job processing
	// Jobs explicitly assigned via task menu should override commands
	if (bIsFollowing || bShouldStay || bIsGoingHome)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Queue changed while in command state - clearing commands to process jobs"));

		// Clear command states (ProcessNextJob will also do this, but be explicit)
		bIsFollowing = false;
		FollowTarget.Reset();
		bShouldStay = false;
		StayLocation = FVector::ZeroVector;
		bIsGoingHome = false;
		StopCurrentMovement();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Queue changed - starting job processing"));
	ProcessNextJob();
}

bool AMOSurvivorController::CanExecuteSimply(EMOSurvivorJobType JobType) const
{
	// Jobs that can be executed without a behavior tree
	switch (JobType)
	{
	case EMOSurvivorJobType::DigForSupplies:
	case EMOSurvivorJobType::ForageNearby:
	case EMOSurvivorJobType::GatherWood:
	case EMOSurvivorJobType::GatherStone:
	case EMOSurvivorJobType::GatherFiber:
	case EMOSurvivorJobType::CraftAtStation:
	case EMOSurvivorJobType::RefuelStation:
	case EMOSurvivorJobType::ExcavateAndHaul:
		return true;
	default:
		return false;
	}
}

bool AMOSurvivorController::IsGatherJob(EMOSurvivorJobType JobType) const
{
	switch (JobType)
	{
	case EMOSurvivorJobType::GatherWood:
	case EMOSurvivorJobType::GatherStone:
	case EMOSurvivorJobType::GatherFiber:
		return true;
	default:
		return false;
	}
}

void AMOSurvivorController::StartSimpleJobExecution()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		CompleteSimpleJob(false);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		CompleteSimpleJob(false);
		return;
	}

	// Craft jobs run their own multi-leg machine (states 10-15). This is also
	// the repeat entry point, so each craft iteration restarts the full
	// withdraw -> craft -> deposit loop.
	if (CurrentJob.JobType == EMOSurvivorJobType::CraftAtStation)
	{
		StartCraftJobExecution();
		return;
	}
	// Refuel jobs likewise (states 20-23).
	if (CurrentJob.JobType == EMOSurvivorJobType::RefuelStation)
	{
		StartRefuelJobExecution();
		return;
	}
	// Excavate-and-haul jobs run the dig->haul->dump machine (states 30-33).
	if (CurrentJob.JobType == EMOSurvivorJobType::ExcavateAndHaul)
	{
		StartExcavateJobExecution();
		return;
	}

	// (H24) Default the effective action duration to the configured constant.
	// Recipe-based gather harvests override this below with the resource
	// definition's real BaseActionTime.
	ActiveJobActionDuration = SimpleJobActionDuration;

	// For gather jobs, find an HISM resource instead of random point
	if (IsGatherJob(CurrentJob.JobType))
	{
		if (FindNearestGatherResource(CurrentJob.JobType))
		{
			SimpleJobState = 1; // Moving
			SimpleJobTimer = 0.0f;

			// (H24) Resolve the real per-action harvest duration from the resource
			// definition (recipe harvests); falls back to the flat constant otherwise.
			ActiveJobActionDuration = ResolveGatherActionDuration();

			// Start moving to the target
			MoveToLocation(SimpleJobTargetLocation, 100.0f);

			UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Moving to gather resource %s at %s"),
				*GatherTargetItemId.ToString(), *SimpleJobTargetLocation.ToString());
		}
		else
		{
			// No resources found
			UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] No gather resources found for job type %d"),
				static_cast<int32>(CurrentJob.JobType));
			CompleteSimpleJob(false);
			return;
		}
	}
	else
	{
		// For dig/forage jobs, pick a random nearby point
		UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
		if (!NavSystem)
		{
			// No nav system - just perform action at current location
			SimpleJobTargetLocation = ControlledPawn->GetActorLocation();
			SimpleJobState = 2; // Skip to performing
			SimpleJobTimer = 0.0f;
			UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] No nav system - performing job at current location"));
		}
		else
		{
			FNavLocation NavLocation;
			FVector PawnLocation = ControlledPawn->GetActorLocation();

			// Try to find a random point nearby
			if (NavSystem->GetRandomReachablePointInRadius(PawnLocation, 300.0f, NavLocation))
			{
				SimpleJobTargetLocation = NavLocation.Location;
				SimpleJobState = 1; // Moving
				SimpleJobTimer = 0.0f;

				// Start moving to the target
				MoveToLocation(SimpleJobTargetLocation, 50.0f);

				UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Moving to job location: %s"),
					*SimpleJobTargetLocation.ToString());
			}
			else
			{
				// Can't find nav point - perform at current location
				SimpleJobTargetLocation = PawnLocation;
				SimpleJobState = 2; // Performing
				SimpleJobTimer = 0.0f;
				UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] No reachable point - performing at current location"));
			}
		}
	}

	// Update job state
	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	if (JobQueue)
	{
		JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::MovingToTarget);
	}
}

void AMOSurvivorController::UpdateSimpleJobExecution(float DeltaTime)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		CompleteSimpleJob(false);
		return;
	}

	// Excavate-leg (30-33), refuel-leg (20-23), and craft-leg (10-15) states live
	// in their own machines. Dispatch highest band first — the >= checks below
	// would otherwise swallow the higher bands.
	if (SimpleJobState >= 30)
	{
		UpdateExcavateJobExecution(DeltaTime);
		return;
	}
	if (SimpleJobState >= 20)
	{
		UpdateRefuelJobExecution(DeltaTime);
		return;
	}
	if (SimpleJobState >= 10)
	{
		UpdateCraftJobExecution(DeltaTime);
		return;
	}

	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();

	switch (SimpleJobState)
	{
	case 1: // Moving to target
		{
			float DistanceToTarget = FVector::Dist(ControlledPawn->GetActorLocation(), SimpleJobTargetLocation);

			if (DistanceToTarget <= 100.0f)
			{
				// Arrived - start performing
				StopMovement();
				SimpleJobState = 2;
				SimpleJobTimer = 0.0f;

				if (JobQueue)
				{
					JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Performing);
				}

				UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Arrived at job location, starting work"));
			}
			else
			{
				// Check if movement stopped (path blocked)
				EPathFollowingStatus::Type MoveStatus = GetMoveStatus();
				if (MoveStatus != EPathFollowingStatus::Moving)
				{
					// Movement failed - try performing where we are
					SimpleJobTargetLocation = ControlledPawn->GetActorLocation();
					SimpleJobState = 2;
					SimpleJobTimer = 0.0f;

					if (JobQueue)
					{
						JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Performing);
					}

					UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Movement stopped - performing at current location"));
				}
			}
			break;
		}

	case 2: // Performing action
		{
			SimpleJobTimer += DeltaTime;

			// (H24) Use the per-job effective duration (real BaseActionTime for recipe
			// harvests) instead of the flat SimpleJobActionDuration constant.
			const float EffectiveDuration = FMath::Max(0.1f, ActiveJobActionDuration);

			// Update progress
			float Progress = FMath::Clamp(SimpleJobTimer / EffectiveDuration, 0.0f, 1.0f);
			if (JobQueue)
			{
				JobQueue->UpdateJobProgress(CurrentJob.JobId, Progress);
			}

			if (SimpleJobTimer >= EffectiveDuration)
			{
				// Action complete - perform the actual foraging/digging
				PerformSimpleJobAction();

				// Check if we need to repeat
				CurrentJob.CompletedCount++;
				if (CurrentJob.CompletedCount >= CurrentJob.RepeatCount)
				{
					CompleteSimpleJob(true);
				}
				else
				{
					// Start next repeat
					StartSimpleJobExecution();
				}
			}
			break;
		}

	default:
		// Invalid state
		CompleteSimpleJob(false);
		break;
	}
}

void AMOSurvivorController::CompleteSimpleJob(bool bSuccess)
{
	SimpleJobState = 0;
	SimpleJobTimer = 0.0f;
	SimpleJobTargetLocation = FVector::ZeroVector;

	// Clear gather state
	GatherTargetHISMComponent.Reset();
	GatherTargetInstanceIndex = INDEX_NONE;
	GatherTargetItemId = NAME_None;

	// Clear harvest state
	HarvestRecipeId = NAME_None;
	bIsRecipeHarvest = false;

	// Clear craft-job state
	CraftStationActor.Reset();
	CraftStorageActor.Reset();
	CraftJobRecipeId = NAME_None;

	// Clear excavate-job scratch (unit 3) — else spoil/zone state leaks into the next job.
	ExcavateDigZoneId.Invalidate();
	ExcavateDumpZoneId.Invalidate();
	ExcavateSpoilItemId = NAME_None;
	ExcavateCarried = 0;
	ExcavateBiteVolumeM3 = 0.0f;
	ExcavateDumpLocation = FVector::ZeroVector;

	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	if (JobQueue && CurrentJob.IsValid())
	{
		JobQueue->CompleteCurrentJob(bSuccess);
	}

	bIsProcessingJob = false;
	CurrentJob = FMOSurvivorJobEntry();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Simple job completed (Success: %s)"),
		bSuccess ? TEXT("Yes") : TEXT("No"));

	ClearSurvivorBlackboardData();
	BroadcastCommandChange(TEXT("Idle"));

	// Check for more jobs
	if (JobQueue && JobQueue->HasJobs())
	{
		ProcessNextJob();
	}
}

void AMOSurvivorController::PerformSimpleJobAction()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Perform action based on job type
	switch (CurrentJob.JobType)
	{
	case EMOSurvivorJobType::DigForSupplies:
		{
			UMOForagingSubsystem* ForagingSubsystem = World->GetSubsystem<UMOForagingSubsystem>();
			if (!ForagingSubsystem)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] No ForagingSubsystem found"));
				return;
			}

			int32 ForagingLevel = ForagingSubsystem->GetForagingLevel(ControlledPawn);
			int32 ItemsAdded = ForagingSubsystem->DigForSuppliesToInventory(
				SimpleJobTargetLocation, ForagingLevel, ControlledPawn);
			UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Dug up %d items directly to inventory at %s"),
				ItemsAdded, *SimpleJobTargetLocation.ToString());

			// Award XP
			AwardJobExperience(CurrentJob.JobType);
			break;
		}

	case EMOSurvivorJobType::ForageNearby:
		{
			UMOForagingSubsystem* ForagingSubsystem = World->GetSubsystem<UMOForagingSubsystem>();
			if (!ForagingSubsystem)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] No ForagingSubsystem found"));
				return;
			}

			int32 ForagingLevel = ForagingSubsystem->GetForagingLevel(ControlledPawn);
			float SearchRadius = ForagingSubsystem->CalculateSearchRadius(ForagingLevel);
			int32 ItemsAdded = ForagingSubsystem->ForageToInventory(
				SimpleJobTargetLocation, SearchRadius, ControlledPawn);
			UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Foraged %d items directly to inventory in radius %.0f"),
				ItemsAdded, SearchRadius);

			// Award XP
			AwardJobExperience(CurrentJob.JobType);
			break;
		}

	case EMOSurvivorJobType::GatherWood:
	case EMOSurvivorJobType::GatherStone:
	case EMOSurvivorJobType::GatherFiber:
		{
			UInstancedStaticMeshComponent* ISMComp = GatherTargetHISMComponent.Get();

			// Handle recipe-based harvest (for KeepOnHarvest targets like trees)
			if (bIsRecipeHarvest && !HarvestRecipeId.IsNone())
			{
				UMOHarvestSubsystem* HarvestSubsystem = World->GetSubsystem<UMOHarvestSubsystem>();
				if (!HarvestSubsystem)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] No HarvestSubsystem found"));
					break;
				}

				if (ISMComp && GatherTargetInstanceIndex != INDEX_NONE)
				{
					UMOInventoryComponent* Inventory = ControlledPawn->FindComponentByClass<UMOInventoryComponent>();
					UMOSkillsComponent* Skills = ControlledPawn->FindComponentByClass<UMOSkillsComponent>();

					// (H24) The action time was already simulated via SimpleJobTimer using
					// the resource definition's real BaseActionTime (ActiveJobActionDuration,
					// resolved in StartSimpleJobExecution) — NOT a flat 4s constant. So by
					// the time we reach PerformSimpleJobAction the correct duration has
					// elapsed; Begin + Complete back-to-back here just applies the yields.
					if (HarvestSubsystem->BeginHarvest(ISMComp, GatherTargetInstanceIndex, HarvestRecipeId, Inventory))
					{
						// (H23/H24) bAllowSkipTimer=true: the survivor already simulated the
						// real action duration via its job timer, so bypass the subsystem's
						// wall-clock gate (Begin + Complete fire in the same frame here).
						FMOCraftResult Result = HarvestSubsystem->CompleteHarvest(Inventory, Skills, /*bAllowSkipTimer=*/true);

						if (Result.bSuccess)
						{
							UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Completed harvest recipe %s"),
								*HarvestRecipeId.ToString());

							// Award XP
							AwardJobExperience(CurrentJob.JobType);
						}
						else
						{
							UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Harvest recipe %s failed"),
								*HarvestRecipeId.ToString());
						}
					}
					else
					{
						UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Failed to begin harvest recipe %s"),
							*HarvestRecipeId.ToString());
					}
				}
				else
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] No valid ISM target for harvest recipe"));
				}
			}
			else
			{
				// Simple pickup (non-KeepOnHarvest items)
				UMOPCGInteractionSubsystem* PCGSubsystem = World->GetSubsystem<UMOPCGInteractionSubsystem>();
				if (!PCGSubsystem)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] No PCGInteractionSubsystem found"));
					break;
				}

				if (ISMComp && GatherTargetInstanceIndex != INDEX_NONE)
				{
					FName HarvestedItemId;
					bool bSuccess = PCGSubsystem->HarvestISMInstance(ISMComp, GatherTargetInstanceIndex, ControlledPawn, HarvestedItemId);

					if (bSuccess)
					{
						UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Picked up %s from ISM instance %d"),
							*HarvestedItemId.ToString(), GatherTargetInstanceIndex);

						// Award XP
						AwardJobExperience(CurrentJob.JobType);
					}
					else
					{
						UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Failed to pick up ISM instance %d"),
							GatherTargetInstanceIndex);
					}
				}
				else
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] No valid ISM target for gather job"));
				}
			}

			// Clear gather target
			GatherTargetHISMComponent.Reset();
			GatherTargetInstanceIndex = INDEX_NONE;
			GatherTargetItemId = NAME_None;
			HarvestRecipeId = NAME_None;
			bIsRecipeHarvest = false;
			break;
		}

	default:
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] PerformSimpleJobAction called for unhandled job type: %d"),
			static_cast<int32>(CurrentJob.JobType));
		break;
	}
}

// ============================================================================
// CRAFT-AT-STATION JOB (V0 village vertical slice)
// ============================================================================

UMOInventoryComponent* AMOSurvivorController::GetActorInventory(AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}
	if (Actor->Implements<UMOInventoryHolderInterface>())
	{
		if (UMOInventoryComponent* Inv = IMOInventoryHolderInterface::Execute_GetInventory(Actor))
		{
			return Inv;
		}
	}
	return Actor->FindComponentByClass<UMOInventoryComponent>();
}

void AMOSurvivorController::StartCraftJobExecution()
{
	// Resolve job data. TargetActor carries the station, StorageActor the
	// container (see EnqueueCraftJob).
	CraftStationActor = CurrentJob.TargetActor;
	CraftStorageActor = CurrentJob.StorageActor;
	CraftJobRecipeId = CurrentJob.CraftRecipeId;

	AActor* Station = CraftStationActor.Get();
	AActor* Storage = CraftStorageActor.Get();
	if (!Station || !Storage || CraftJobRecipeId.IsNone() ||
		!UMORecipeDatabaseSettings::GetRecipeDefinition(CraftJobRecipeId))
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOSurvivorController] Craft job invalid (station=%s storage=%s recipe=%s)"),
			Station ? *Station->GetName() : TEXT("null"),
			Storage ? *Storage->GetName() : TEXT("null"),
			*CraftJobRecipeId.ToString());
		CompleteSimpleJob(false);
		return;
	}

	SimpleJobState = 10; // moving to storage (withdraw leg)
	SimpleJobTimer = 0.0f;
	SimpleJobTotalSeconds = 0.0f;
	MoveLegBestDistance = FLT_MAX;
	MoveLegNoProgressSeconds = 0.0f;
	SimpleJobTargetLocation = Storage->GetActorLocation();
	MoveToLocation(SimpleJobTargetLocation, 150.0f);

	if (UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue())
	{
		JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::MovingToTarget);
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Craft job started: %s at %s (storage %s)"),
		*CraftJobRecipeId.ToString(), *Station->GetName(), *Storage->GetName());
}

void AMOSurvivorController::UpdateCraftJobExecution(float DeltaTime)
{
	APawn* ControlledPawn = GetPawn();
	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	if (!ControlledPawn)
	{
		CompleteSimpleJob(false);
		return;
	}

	// GLOBAL JOB WATCHDOG (B1): a full craft cycle takes ~35s real; any job
	// alive past 120s is wedged SOMEWHERE (a silent state is exactly how a
	// soak bricked twice on 2026-07-07). Fail loudly, NAME the state, free
	// the quota slot for reassignment.
	SimpleJobTotalSeconds += DeltaTime;
	if (SimpleJobTotalSeconds > 120.0f)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOSurvivorController] Craft job WEDGED in state %d after %.0fs (recipe %s) - failing"),
			SimpleJobState, SimpleJobTotalSeconds, *CraftJobRecipeId.ToString());
		CompleteSimpleJob(false);
		return;
	}

	auto SetProgress = [&](float Progress)
	{
		if (JobQueue)
		{
			JobQueue->UpdateJobProgress(CurrentJob.JobId, Progress);
		}
	};

	switch (SimpleJobState)
	{
	case 10: // moving to storage (withdraw leg)
		{
			const int32 Arrive = UpdateJobMoveLeg(DeltaTime, 250.0f, 600.0f);
			if (Arrive < 0) { CompleteSimpleJob(false); return; }
			if (Arrive > 0)
			{
				SimpleJobState = 11;
				SimpleJobTimer = 0.0f;
				if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Performing); }
			}
			SetProgress(0.05f);
			break;
		}

	case 11: // withdrawing ingredients (timed handling)
		{
			SimpleJobTimer += DeltaTime;
			SetProgress(0.05f + 0.15f * FMath::Clamp(SimpleJobTimer / FMath::Max(0.1f, CraftHandlingDuration), 0.0f, 1.0f));
			if (SimpleJobTimer >= CraftHandlingDuration)
			{
				if (!TransferCraftIngredients())
				{
					CompleteSimpleJob(false);
					return;
				}
				AActor* Station = CraftStationActor.Get();
				if (!Station) { CompleteSimpleJob(false); return; }
				SimpleJobState = 12;
				MoveLegBestDistance = FLT_MAX;
				MoveLegNoProgressSeconds = 0.0f;
				SimpleJobTimer = 0.0f;
				SimpleJobTargetLocation = Station->GetActorLocation();
				MoveToLocation(SimpleJobTargetLocation, 150.0f);
				if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::MovingToTarget); }
			}
			break;
		}

	case 12: // moving to station
		{
			const int32 Arrive = UpdateJobMoveLeg(DeltaTime, 250.0f, 600.0f);
			if (Arrive < 0) { CompleteSimpleJob(false); return; }
			if (Arrive > 0)
			{
				UMOCraftingQueueComponent* CraftQueue = ControlledPawn->FindComponentByClass<UMOCraftingQueueComponent>();
				const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(CraftJobRecipeId);
				if (!CraftQueue || !Recipe)
				{
					CompleteSimpleJob(false);
					return;
				}
				// We are standing at the station: pass its type (or the
				// recipe's requirement as fallback) so station-gated recipes
				// validate. The REAL crafting queue takes over from here -
				// real ingredient consume, real duration, real outputs.
				EMOCraftingStation StationType = Recipe->RequiredStation;
				if (const AMOCraftingStationActor* StationActor = Cast<AMOCraftingStationActor>(CraftStationActor.Get()))
				{
					StationType = StationActor->GetStationType();
				}
				if (!CraftQueue->EnqueueCraft(CraftJobRecipeId, 1, StationType))
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Craft enqueue failed for %s"),
						*CraftJobRecipeId.ToString());
					CompleteSimpleJob(false);
					return;
				}
				SimpleJobState = 13;
				SimpleJobTimer = 0.0f;
				if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Performing); }
			}
			SetProgress(0.25f);
			break;
		}

	case 13: // crafting - the real crafting queue runs at real duration
		{
			UMOCraftingQueueComponent* CraftQueue = ControlledPawn->FindComponentByClass<UMOCraftingQueueComponent>();
			if (!CraftQueue)
			{
				CompleteSimpleJob(false);
				return;
			}
			if (CraftQueue->IsQueueEmpty())
			{
				// Craft finished - outputs are in the pawn inventory. Head back.
				AActor* Storage = CraftStorageActor.Get();
				if (!Storage) { CompleteSimpleJob(false); return; }
				SimpleJobState = 14;
				MoveLegBestDistance = FLT_MAX;
				MoveLegNoProgressSeconds = 0.0f;
				SimpleJobTimer = 0.0f;
				SimpleJobTargetLocation = Storage->GetActorLocation();
				MoveToLocation(SimpleJobTargetLocation, 150.0f);
				if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Returning); }
				break;
			}
			if (!CraftQueue->IsCraftingActive())
			{
				// An interrupt (movement/endplay) paused the queue while we
				// stand at the station - resume, the survivor isn't going anywhere.
				CraftQueue->StartCrafting();
			}
			FMOCraftingQueueEntry Current;
			if (CraftQueue->GetCurrentCraft(Current))
			{
				SetProgress(0.3f + 0.5f * FMath::Clamp(Current.Progress, 0.0f, 1.0f));
			}
			break;
		}

	case 14: // returning to storage (deposit leg)
		{
			const int32 Arrive = UpdateJobMoveLeg(DeltaTime, 250.0f, 600.0f);
			if (Arrive < 0) { CompleteSimpleJob(false); return; }
			if (Arrive > 0)
			{
				SimpleJobState = 15;
				SimpleJobTimer = 0.0f;
				if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Performing); }
			}
			SetProgress(0.85f);
			break;
		}

	case 15: // depositing outputs (timed handling)
		{
			SimpleJobTimer += DeltaTime;
			SetProgress(0.85f + 0.15f * FMath::Clamp(SimpleJobTimer / FMath::Max(0.1f, CraftHandlingDuration), 0.0f, 1.0f));
			if (SimpleJobTimer >= CraftHandlingDuration)
			{
				DepositCraftOutputs();

				CurrentJob.CompletedCount++;
				if (CurrentJob.CompletedCount >= CurrentJob.RepeatCount)
				{
					CompleteSimpleJob(true);
				}
				else
				{
					StartSimpleJobExecution(); // next iteration: full loop again
				}
			}
			break;
		}

	default:
		CompleteSimpleJob(false);
		break;
	}
}

int32 AMOSurvivorController::UpdateJobMoveLeg(float DeltaTime, float ArriveDist, float StallDist)
{
	// Building actors have real footprints: accept arrival within interaction
	// range, and treat a stalled path within a generous radius as arrival
	// (nav edge vs mesh bounds), otherwise fail honestly — these jobs REQUIRE
	// standing at the station/storage, there is no work-from-afar fallback.
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return -1;
	}
	const float Distance = FVector::Dist2D(ControlledPawn->GetActorLocation(), SimpleJobTargetLocation);
	if (Distance <= ArriveDist)
	{
		StopMovement();
		return 1; // arrived
	}
	// NO-PROGRESS WATCHDOG: a pawn wedged in collision keeps path status
	// "Moving" at zero velocity FOREVER — the status check below never
	// fires and the job (and its quota in-flight slot) is wedged with it.
	// Fail honestly after 15s real without 50uu of progress. (B1
	// blocked-task intelligence, minimal cut; found via a bricked soak.)
	if (Distance + 50.0f < MoveLegBestDistance)
	{
		MoveLegBestDistance = Distance;
		MoveLegNoProgressSeconds = 0.0f;
	}
	else
	{
		MoveLegNoProgressSeconds += DeltaTime;
		if (MoveLegNoProgressSeconds > 15.0f)
		{
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOSurvivorController] Job move leg NO PROGRESS for 15s (%.0fuu out, likely wedged) - failing"), Distance);
			StopMovement();
			return -1;
		}
	}
	if (GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		if (Distance <= StallDist)
		{
			StopMovement();
			return 1; // close enough - nav stopped at the building's bounds
		}
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOSurvivorController] Job move leg stalled %.0fuu from target - failing"), Distance);
		return -1; // unreachable
	}
	return 0; // still moving
}

void AMOSurvivorController::StartRefuelJobExecution()
{
	CraftStationActor = CurrentJob.TargetActor;
	CraftStorageActor = CurrentJob.StorageActor;
	RefuelItemId = CurrentJob.FuelItemId;
	RefuelCarried = 0;

	AMOCraftingStationActor* Station = Cast<AMOCraftingStationActor>(CraftStationActor.Get());
	AActor* Storage = CraftStorageActor.Get();
	if (!Station || !Storage || RefuelItemId.IsNone() ||
		!Station->AcceptedFuelItems.Contains(RefuelItemId))
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOSurvivorController] Refuel job invalid (station=%s storage=%s fuel=%s)"),
			CraftStationActor.IsValid() ? *CraftStationActor->GetName() : TEXT("null"),
			Storage ? *Storage->GetName() : TEXT("null"),
			*RefuelItemId.ToString());
		CompleteSimpleJob(false);
		return;
	}

	SimpleJobState = 20; // moving to storage (fuel withdraw leg)
	SimpleJobTimer = 0.0f;
	SimpleJobTotalSeconds = 0.0f;
	MoveLegBestDistance = FLT_MAX;
	MoveLegNoProgressSeconds = 0.0f;
	SimpleJobTargetLocation = Storage->GetActorLocation();
	MoveToLocation(SimpleJobTargetLocation, 150.0f);

	if (UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue())
	{
		JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::MovingToTarget);
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Refuel job started: %dx %s to %s (storage %s)"),
		CurrentJob.FuelQuantity, *RefuelItemId.ToString(), *Station->GetName(), *Storage->GetName());
}

void AMOSurvivorController::UpdateRefuelJobExecution(float DeltaTime)
{
	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	if (!GetPawn())
	{
		CompleteSimpleJob(false);
		return;
	}

	// GLOBAL JOB WATCHDOG (B1) — same contract as the craft machine.
	SimpleJobTotalSeconds += DeltaTime;
	if (SimpleJobTotalSeconds > 120.0f)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOSurvivorController] Refuel job WEDGED in state %d after %.0fs (fuel %s) - failing"),
			SimpleJobState, SimpleJobTotalSeconds, *RefuelItemId.ToString());
		CompleteSimpleJob(false);
		return;
	}

	auto SetProgress = [&](float Progress)
	{
		if (JobQueue)
		{
			JobQueue->UpdateJobProgress(CurrentJob.JobId, Progress);
		}
	};

	switch (SimpleJobState)
	{
	case 20: // moving to storage (fuel withdraw leg)
		{
			const int32 Arrive = UpdateJobMoveLeg(DeltaTime, 250.0f, 600.0f);
			if (Arrive < 0) { CompleteSimpleJob(false); return; }
			if (Arrive > 0)
			{
				SimpleJobState = 21;
				SimpleJobTimer = 0.0f;
				if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Performing); }
			}
			SetProgress(0.15f);
			break;
		}

	case 21: // withdrawing fuel (timed handling)
		{
			SimpleJobTimer += DeltaTime;
			SetProgress(0.15f + 0.2f * FMath::Clamp(SimpleJobTimer / FMath::Max(0.1f, CraftHandlingDuration), 0.0f, 1.0f));
			if (SimpleJobTimer >= CraftHandlingDuration)
			{
				UMOInventoryComponent* StorageInv = GetActorInventory(CraftStorageActor.Get());
				UMOInventoryComponent* PawnInv = GetPawn()->FindComponentByClass<UMOInventoryComponent>();
				const int32 Available = StorageInv ? StorageInv->GetItemCountByDefinitionId(RefuelItemId) : 0;
				const int32 Take = FMath::Min(Available, FMath::Max(1, CurrentJob.FuelQuantity));
				if (Take <= 0 || !PawnInv || !StorageInv->RemoveItemByDefinitionId(RefuelItemId, Take))
				{
					UE_LOG(LogMOFramework, Warning,
						TEXT("[MOSurvivorController] Refuel job: storage has no %s - failing"),
						*RefuelItemId.ToString());
					CompleteSimpleJob(false);
					return;
				}
				// Carry on the pawn, not in the void — an aborted trip keeps
				// the fuel recoverable in the villager's pack.
				PawnInv->AddItemByGuid(FGuid::NewGuid(), RefuelItemId, Take);
				RefuelCarried = Take;

				AActor* Station = CraftStationActor.Get();
				if (!Station) { CompleteSimpleJob(false); return; }
				SimpleJobState = 22;
				MoveLegBestDistance = FLT_MAX;
				MoveLegNoProgressSeconds = 0.0f;
				SimpleJobTargetLocation = Station->GetActorLocation();
				MoveToLocation(SimpleJobTargetLocation, 150.0f);
				if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::MovingToTarget); }
			}
			break;
		}

	case 22: // moving to station
		{
			const int32 Arrive = UpdateJobMoveLeg(DeltaTime, 250.0f, 600.0f);
			if (Arrive < 0) { CompleteSimpleJob(false); return; }
			if (Arrive > 0)
			{
				SimpleJobState = 23;
				SimpleJobTimer = 0.0f;
				if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Performing); }
			}
			SetProgress(0.6f);
			break;
		}

	case 23: // loading the fuel tank + relighting (timed handling)
		{
			SimpleJobTimer += DeltaTime;
			SetProgress(0.6f + 0.4f * FMath::Clamp(SimpleJobTimer / FMath::Max(0.1f, CraftHandlingDuration), 0.0f, 1.0f));
			if (SimpleJobTimer >= CraftHandlingDuration)
			{
				AMOCraftingStationActor* Station = Cast<AMOCraftingStationActor>(CraftStationActor.Get());
				UMOInventoryComponent* PawnInv = GetPawn()->FindComponentByClass<UMOInventoryComponent>();
				if (!Station || !PawnInv) { CompleteSimpleJob(false); return; }
				// Feed only what the tank accepts; any leftover stays in the
				// pawn's pack (recoverable — no matter destroyed on a full fire).
				const int32 Accepted = Station->AddFuel(RefuelItemId, RefuelCarried);
				if (Accepted > 0)
				{
					PawnInv->RemoveItemByDefinitionId(RefuelItemId, Accepted);
				}
				// Tending the fire includes lighting it: a burned-out station
				// relights once it has fuel again.
				if (Accepted > 0 && !Station->IsStationActive())
				{
					Station->SetStationActive(true);
				}
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MOSurvivorController] Refuel job done: %d/%d item(s) into %s (%s now %s, %d kept)"),
					Accepted, RefuelCarried, *Station->GetName(),
					*RefuelItemId.ToString(), Station->IsStationActive() ? TEXT("BURNING") : TEXT("cold"),
					RefuelCarried - Accepted);
				RefuelCarried = 0;
				CompleteSimpleJob(true);
			}
			break;
		}

	default:
		CompleteSimpleJob(false);
		break;
	}
}

void AMOSurvivorController::StartExcavateJobExecution()
{
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World) { CompleteSimpleJob(false); return; }

	UMODesignationSubsystem* Desig = UMODesignationSubsystem::Get(this);
	UMOTerraformingComponent* Terra = ControlledPawn->FindComponentByClass<UMOTerraformingComponent>();
	if (!Desig || !Terra)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOSurvivorController] Excavate job: no designation subsystem or terraform component - failing"));
		CompleteSimpleJob(false);
		return;
	}

	ExcavateDigZoneId = CurrentJob.DigZoneId;
	ExcavateDumpZoneId = CurrentJob.DumpZoneId;
	ExcavateDumpMode = CurrentJob.ExcavateDumpMode;
	ExcavateSpoilItemId = CurrentJob.SpoilItemId.IsNone() ? FName("Dirt01") : CurrentJob.SpoilItemId;
	ExcavateCarried = 0;

	FMODesignationZone DigZone;
	if (!Desig->FindZone(ExcavateDigZoneId, DigZone) || DigZone.RemainingVolumeM3 <= 0.0f)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Excavate job: dig zone missing/exhausted - failing"));
		CompleteSimpleJob(false);
		return;
	}

	// Resolve where the spoil goes.
	if (ExcavateDumpMode == EMOExcavateDumpMode::FillZone)
	{
		FMODesignationZone DumpZone;
		if (!Desig->FindZone(ExcavateDumpZoneId, DumpZone))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Excavate job: fill zone missing - failing"));
			CompleteSimpleJob(false);
			return;
		}
		ExcavateDumpLocation = DumpZone.Center;
	}
	else // Container
	{
		AActor* Container = CurrentJob.StorageActor.Get();
		if (!Container)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Excavate job: dump container missing - failing"));
			CompleteSimpleJob(false);
			return;
		}
		ExcavateDumpLocation = Container->GetActorLocation();
	}

	// Bite geometry + REAL volume-based timing (the point of unit 3 — digging takes
	// time proportional to earth moved). Don't dig more than the zone has left.
	const float BiteRadius = Terra->ExcavationDigRadius;
	const float BiteStrength = Terra->ExcavationDigStrength;
	const float DepthPer = Terra->TerraformDepthPerStrengthMeters;
	ExcavateBiteVolumeM3 = FMath::Min(
		UMOTerraformingComponent::ComputeMovedVolumeCubicMeters(BiteRadius, BiteStrength, DepthPer),
		DigZone.RemainingVolumeM3);
	const float BiteDepth = BiteStrength * DepthPer;
	ExcavateBiteDuration = UMOTerraformingComponent::ComputeTerraformDurationSeconds(
		BiteRadius, BiteDepth, Terra->TerraformSecondsPerCubicMeter, Terra->TerraformDurationSeconds);
	// The wedge watchdog must clear the (possibly hours-long) dig + deposit + travel.
	ExcavateWatchdogSeconds = FMath::Max(120.0f, ExcavateBiteDuration * 2.5f + 60.0f);

	SimpleJobState = 30; // moving to the dig zone
	SimpleJobTimer = 0.0f;
	SimpleJobTotalSeconds = 0.0f;
	MoveLegBestDistance = FLT_MAX;
	MoveLegNoProgressSeconds = 0.0f;
	SimpleJobTargetLocation = DigZone.Center;
	MoveToLocation(SimpleJobTargetLocation, 150.0f);

	if (UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue())
	{
		JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::MovingToTarget);
	}

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOSurvivorController] Excavate job started: dig %s -> %s (bite %.3fm3, %.1fs/leg, watchdog %.0fs)"),
		*ExcavateDigZoneId.ToString(EGuidFormats::Short),
		ExcavateDumpMode == EMOExcavateDumpMode::FillZone ? TEXT("fill zone") : TEXT("container"),
		ExcavateBiteVolumeM3, ExcavateBiteDuration, ExcavateWatchdogSeconds);
}

void AMOSurvivorController::UpdateExcavateJobExecution(float DeltaTime)
{
	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) { CompleteSimpleJob(false); return; }

	// Global job watchdog — sized to the (long) volume-based bite in StartExcavateJobExecution.
	SimpleJobTotalSeconds += DeltaTime;
	if (SimpleJobTotalSeconds > ExcavateWatchdogSeconds)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOSurvivorController] Excavate job WEDGED in state %d after %.0fs (limit %.0f) - failing"),
			SimpleJobState, SimpleJobTotalSeconds, ExcavateWatchdogSeconds);
		CompleteSimpleJob(false);
		return;
	}

	auto SetProgress = [&](float P) { if (JobQueue) { JobQueue->UpdateJobProgress(CurrentJob.JobId, P); } };

	UMOTerraformingComponent* Terra = ControlledPawn->FindComponentByClass<UMOTerraformingComponent>();
	if (!Terra) { CompleteSimpleJob(false); return; }

	switch (SimpleJobState)
	{
	case 30: // moving to the dig zone
		{
			const int32 Arrive = UpdateJobMoveLeg(DeltaTime, 250.0f, 600.0f);
			if (Arrive < 0) { CompleteSimpleJob(false); return; }
			if (Arrive > 0)
			{
				SimpleJobState = 31;
				SimpleJobTimer = 0.0f;
				if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Performing); }
			}
			SetProgress(0.1f);
			break;
		}

	case 31: // digging one bite (timed, volume-based) -> produces spoil
		{
			SimpleJobTimer += DeltaTime;
			SetProgress(0.1f + 0.3f * FMath::Clamp(SimpleJobTimer / FMath::Max(0.1f, ExcavateBiteDuration), 0.0f, 1.0f));
			if (SimpleJobTimer >= ExcavateBiteDuration)
			{
				// Remove earth at the dig point; the moved volume becomes carryable spoil.
				const float MovedVol = Terra->TerraformAtLocationEx(SimpleJobTargetLocation,
					EMOTerraformMode::Dig, Terra->ExcavationDigRadius, Terra->ExcavationDigStrength);
				if (MovedVol <= 0.0f)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Excavate job: dig moved no earth - failing"));
					CompleteSimpleJob(false);
					return;
				}
				ExcavateBiteVolumeM3 = MovedVol;
				const int32 SpoilCount = UMOTerraformingComponent::SpoilItemsForVolume(
					MovedVol, Terra->ExcavationItemsPerCubicMeter);
				if (UMOInventoryComponent* PawnInv = ControlledPawn->FindComponentByClass<UMOInventoryComponent>())
				{
					if (SpoilCount > 0) { PawnInv->AddItemByGuid(FGuid::NewGuid(), ExcavateSpoilItemId, SpoilCount); }
				}
				ExcavateCarried = SpoilCount;

				// Debit the dig zone's earth budget (drops the zone when exhausted).
				if (UMODesignationSubsystem* Desig = UMODesignationSubsystem::Get(this))
				{
					Desig->ConsumeZoneVolume(ExcavateDigZoneId, MovedVol);
				}

				SimpleJobState = 32; // haul to the dump target
				MoveLegBestDistance = FLT_MAX;
				MoveLegNoProgressSeconds = 0.0f;
				SimpleJobTargetLocation = ExcavateDumpLocation;
				MoveToLocation(SimpleJobTargetLocation, 150.0f);
				if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::MovingToTarget); }
			}
			break;
		}

	case 32: // hauling to the dump target
		{
			const int32 Arrive = UpdateJobMoveLeg(DeltaTime, 250.0f, 600.0f);
			if (Arrive < 0) { CompleteSimpleJob(false); return; }
			if (Arrive > 0)
			{
				SimpleJobState = 33;
				SimpleJobTimer = 0.0f;
				if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Performing); }
			}
			SetProgress(0.6f);
			break;
		}

	case 33: // depositing the spoil
		{
			// Filling a zone is earth-moving (volume-based); a container deposit is quick handling.
			const float DepositDuration = (ExcavateDumpMode == EMOExcavateDumpMode::FillZone)
				? ExcavateBiteDuration : CraftHandlingDuration;
			SimpleJobTimer += DeltaTime;
			SetProgress(0.6f + 0.4f * FMath::Clamp(SimpleJobTimer / FMath::Max(0.1f, DepositDuration), 0.0f, 1.0f));
			if (SimpleJobTimer >= DepositDuration)
			{
				UMOInventoryComponent* PawnInv = ControlledPawn->FindComponentByClass<UMOInventoryComponent>();

				if (ExcavateDumpMode == EMOExcavateDumpMode::FillZone)
				{
					// Raise terrain at the fill zone, consuming the carried spoil (conservation).
					const float Raised = Terra->TerraformAtLocationEx(ExcavateDumpLocation,
						EMOTerraformMode::Raise, Terra->ExcavationDigRadius, Terra->ExcavationDigStrength);
					if (PawnInv && ExcavateCarried > 0)
					{
						PawnInv->RemoveItemByDefinitionId(ExcavateSpoilItemId, ExcavateCarried);
					}
					if (UMODesignationSubsystem* Desig = UMODesignationSubsystem::Get(this))
					{
						Desig->ConsumeZoneVolume(ExcavateDumpZoneId, Raised > 0.0f ? Raised : ExcavateBiteVolumeM3);
					}
					UE_LOG(LogMOFramework, Warning,
						TEXT("[MOSurvivorController] Excavate job done: filled %.3fm3, consumed %d spoil"),
						Raised, ExcavateCarried);
				}
				else // Container
				{
					UMOInventoryComponent* ContainerInv = GetActorInventory(CurrentJob.StorageActor.Get());
					int32 Deposited = 0;
					if (ContainerInv && PawnInv && ExcavateCarried > 0
						&& ContainerInv->CanAddItemByDefinitionId(ExcavateSpoilItemId, ExcavateCarried))
					{
						ContainerInv->AddItemByGuid(FGuid::NewGuid(), ExcavateSpoilItemId, ExcavateCarried);
						PawnInv->RemoveItemByDefinitionId(ExcavateSpoilItemId, ExcavateCarried);
						Deposited = ExcavateCarried;
					}
					UE_LOG(LogMOFramework, Warning,
						TEXT("[MOSurvivorController] Excavate job done: deposited %d/%d spoil into container (rest kept)"),
						Deposited, ExcavateCarried);
				}

				ExcavateCarried = 0;
				CompleteSimpleJob(true);
			}
			break;
		}

	default:
		CompleteSimpleJob(false);
		break;
	}
}

bool AMOSurvivorController::TransferCraftIngredients()
{
	APawn* ControlledPawn = GetPawn();
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(CraftJobRecipeId);
	UMOInventoryComponent* StorageInv = GetActorInventory(CraftStorageActor.Get());
	UMOInventoryComponent* PawnInv = ControlledPawn ? ControlledPawn->FindComponentByClass<UMOInventoryComponent>() : nullptr;
	if (!Recipe || !StorageInv || !PawnInv)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Craft withdraw failed: recipe/storage/pawn inventory missing"));
		return false;
	}

	// All-or-nothing check first so a partial withdraw never strands materials.
	for (const FMORecipeIngredient& Ing : Recipe->Ingredients)
	{
		if (!Ing.ItemDefinitionId.IsNone() && !StorageInv->HasItem(Ing.ItemDefinitionId, Ing.Quantity))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Storage lacks %dx %s for %s"),
				Ing.Quantity, *Ing.ItemDefinitionId.ToString(), *CraftJobRecipeId.ToString());
			return false;
		}
	}

	for (const FMORecipeIngredient& Ing : Recipe->Ingredients)
	{
		if (Ing.ItemDefinitionId.IsNone())
		{
			continue;
		}
		// Stackable material transfer: fresh GUID on the receiving side (same
		// as the crafting subsystem's material-gather path). Identity-tracked
		// transfer of specific instances is a non-goal for materials.
		StorageInv->RemoveItemByDefinitionId(Ing.ItemDefinitionId, Ing.Quantity);
		PawnInv->AddItemByGuid(FGuid::NewGuid(), Ing.ItemDefinitionId, Ing.Quantity);
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Withdrew ingredients for %s from %s"),
		*CraftJobRecipeId.ToString(), *CraftStorageActor->GetName());
	return true;
}

void AMOSurvivorController::DepositCraftOutputs()
{
	APawn* ControlledPawn = GetPawn();
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(CraftJobRecipeId);
	UMOInventoryComponent* StorageInv = GetActorInventory(CraftStorageActor.Get());
	UMOInventoryComponent* PawnInv = ControlledPawn ? ControlledPawn->FindComponentByClass<UMOInventoryComponent>() : nullptr;
	if (!Recipe || !StorageInv || !PawnInv)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Craft deposit skipped: recipe/storage/pawn inventory missing"));
		return;
	}

	for (const FMORecipeOutput& Out : Recipe->Outputs)
	{
		if (Out.ItemDefinitionId.IsNone())
		{
			continue;
		}
		const int32 Have = PawnInv->GetItemCountByDefinitionId(Out.ItemDefinitionId);
		const int32 Move = FMath::Min(Have, Out.Quantity);
		if (Move > 0)
		{
			PawnInv->RemoveItemByDefinitionId(Out.ItemDefinitionId, Move);
			StorageInv->AddItemByGuid(FGuid::NewGuid(), Out.ItemDefinitionId, Move);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Deposited %dx %s to %s"),
				Move, *Out.ItemDefinitionId.ToString(), *CraftStorageActor->GetName());
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Expected output %s not in pawn inventory - nothing to deposit"),
				*Out.ItemDefinitionId.ToString());
		}
	}
}

bool AMOSurvivorController::FindNearestGatherResource(EMOSurvivorJobType JobType)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Get search radius from job definition (fall back to property default)
	float SearchRadius = GatherSearchRadius;
	const FMOSurvivorJobDefinitionRow* JobDef = UMOSurvivorJobDatabaseSettings::GetJobDefinition(JobType);
	if (JobDef && JobDef->SearchRadius > 0.0f)
	{
		SearchRadius = JobDef->SearchRadius;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] FindNearestGatherResource: JobType=%d, SearchRadius=%.0f"),
		static_cast<int32>(JobType), SearchRadius);

	// For GatherWood, find trees (KeepOnHarvest) with GivesStick tag
	if (JobType == EMOSurvivorJobType::GatherWood)
	{
		FName RecipeId;
		if (FindNearestHarvestTarget(FName(TEXT("GivesStick")), SearchRadius, RecipeId))
		{
			HarvestRecipeId = RecipeId;
			bIsRecipeHarvest = true;
			UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Found tree for GatherWood at %s, recipe: %s"),
				*SimpleJobTargetLocation.ToString(), *HarvestRecipeId.ToString());
			return true;
		}
		// Fall through to try finding loose sticks on ground
	}

	// For other gather jobs (or fallback), look for ground spawns
	bIsRecipeHarvest = false;
	HarvestRecipeId = NAME_None;

	// Get the foraging subsystem to query HISM instances
	UMOForagingSubsystem* ForagingSubsystem = World->GetSubsystem<UMOForagingSubsystem>();
	if (!ForagingSubsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] No ForagingSubsystem found"));
		return false;
	}

	FVector PawnLocation = ControlledPawn->GetActorLocation();

	// Query all harvestable HISM instances in radius (excludes KeepOnHarvest)
	TArray<FMOHISMInstanceData> Instances = ForagingSubsystem->QueryHISMInstancesInRadius(PawnLocation, SearchRadius);

	if (Instances.Num() == 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] No HISM instances found in radius %.0f"), SearchRadius);
		return false;
	}

	// Filter instances by job type (check item tags)
	for (const FMOHISMInstanceData& Instance : Instances)
	{
		if (!Instance.IsValid())
		{
			continue;
		}

		// Check if this item matches the job type
		if (DoesItemMatchJobType(Instance.ItemId, JobType))
		{
			GatherTargetHISMComponent = Instance.HISMComponent;
			GatherTargetInstanceIndex = Instance.InstanceIndex;
			GatherTargetItemId = Instance.ItemId;
			SimpleJobTargetLocation = Instance.InstanceTransform.GetLocation();

			UE_LOG(LogMOFramework, Verbose, TEXT("[MOSurvivorController] Found matching ground resource: %s at %s"),
				*Instance.ItemId.ToString(), *SimpleJobTargetLocation.ToString());
			return true;
		}
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOSurvivorController] No matching resources for job type %d in %d instances"),
		static_cast<int32>(JobType), Instances.Num());
	return false;
}

bool AMOSurvivorController::FindNearestHarvestTarget(FName RequiredTag, float SearchRadius, FName& OutRecipeId)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UMOHarvestSubsystem* HarvestSubsystem = World->GetSubsystem<UMOHarvestSubsystem>();
	if (!HarvestSubsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] No HarvestSubsystem found"));
		return false;
	}

	FVector PawnLocation = ControlledPawn->GetActorLocation();
	float BestDistanceSq = SearchRadius * SearchRadius;
	bool bFound = false;

	// Iterate all actors looking for KeepOnHarvest ISM/HISM with the required tag
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Check distance first
		float DistanceSq = FVector::DistSquared(Actor->GetActorLocation(), PawnLocation);
		if (DistanceSq > BestDistanceSq)
		{
			continue;
		}

		// Check all ISM components on this actor
		TArray<UInstancedStaticMeshComponent*> ISMComponents;
		Actor->GetComponents<UInstancedStaticMeshComponent>(ISMComponents);

		for (UInstancedStaticMeshComponent* ISM : ISMComponents)
		{
			if (!IsValid(ISM) || ISM->GetInstanceCount() == 0)
			{
				continue;
			}

			// Must be KeepOnHarvest (tree, etc.)
			if (!FMOHISMHarvestHelper::ShouldKeepOnHarvest(ISM))
			{
				continue;
			}

			// Check for required tag
			TArray<FName> TargetTags = HarvestSubsystem->CollectTargetTags(ISM);
			if (!TargetTags.Contains(RequiredTag))
			{
				continue;
			}

			// Find harvest recipe for these tags
			TArray<FName> AvailableRecipes;
			// Get pawn's inventory, knowledge, and skills for recipe check
			UMOInventoryComponent* Inventory = ControlledPawn->FindComponentByClass<UMOInventoryComponent>();
			UMOKnowledgeComponent* Knowledge = ControlledPawn->FindComponentByClass<UMOKnowledgeComponent>();
			UMOSkillsComponent* Skills = ControlledPawn->FindComponentByClass<UMOSkillsComponent>();

			HarvestSubsystem->GetHarvestRecipesForTags(TargetTags, Knowledge, Skills, Inventory, AvailableRecipes);

			if (AvailableRecipes.Num() == 0)
			{
				continue;
			}

			// Found a valid target - get the nearest instance
			FTransform NearestTransform;
			int32 NearestIndex = INDEX_NONE;
			float NearestDistSq = MAX_FLT;

			for (int32 i = 0; i < ISM->GetInstanceCount(); i++)
			{
				FTransform InstanceTransform;
				if (ISM->GetInstanceTransform(i, InstanceTransform, true))
				{
					float InstDistSq = FVector::DistSquared(InstanceTransform.GetLocation(), PawnLocation);
					if (InstDistSq < NearestDistSq && InstDistSq < BestDistanceSq)
					{
						NearestDistSq = InstDistSq;
						NearestIndex = i;
						NearestTransform = InstanceTransform;
					}
				}
			}

			if (NearestIndex != INDEX_NONE)
			{
				GatherTargetHISMComponent = ISM;
				GatherTargetInstanceIndex = NearestIndex;
				GatherTargetItemId = RequiredTag;
				SimpleJobTargetLocation = NearestTransform.GetLocation();
				OutRecipeId = AvailableRecipes[0]; // Use first available recipe
				BestDistanceSq = NearestDistSq;
				bFound = true;
			}
		}
	}

	return bFound;
}

float AMOSurvivorController::ResolveGatherActionDuration() const
{
	// (H24) Non-recipe gather (loose ground pickups) and dig/forage jobs have no
	// per-action simulation time of their own — keep the configured constant.
	if (!bIsRecipeHarvest || HarvestRecipeId.IsNone())
	{
		return SimpleJobActionDuration;
	}

	const APawn* ControlledPawn = GetPawn();
	UInstancedStaticMeshComponent* ISMComp = GatherTargetHISMComponent.Get();
	const UWorld* World = GetWorld();
	if (!ControlledPawn || !IsValid(ISMComp) || !World)
	{
		return SimpleJobActionDuration;
	}

	UMOHarvestSubsystem* HarvestSubsystem = World->GetSubsystem<UMOHarvestSubsystem>();
	if (!HarvestSubsystem)
	{
		return SimpleJobActionDuration;
	}

	// Resolve the harvest action the same way BeginHarvest does: ResourceNodeId
	// from the target tags -> FindAction(HarvestRecipeId).
	const TArray<FName> TargetTags = HarvestSubsystem->CollectTargetTags(ISMComp);
	const FName ResourceNodeId = HarvestSubsystem->ExtractResourceNodeId(TargetTags);
	const FMOResourceNodeDefinitionRow* ResourceDef = HarvestSubsystem->GetResourceDefinition(ResourceNodeId);
	const FMOResourceHarvestAction* HarvestAction = ResourceDef ? ResourceDef->FindAction(HarvestRecipeId) : nullptr;
	if (!HarvestAction)
	{
		// Definition missing — fall back rather than silently treating 4s as the
		// "real" time; the constant also signals that the lookup failed.
		return SimpleJobActionDuration;
	}

	// Mirror BeginHarvest's tool penalty: optional-but-missing tool slows the action.
	float ActionTime = HarvestAction->BaseActionTime;
	if (HarvestAction->RequiresTool() && !HarvestAction->bToolRequired)
	{
		if (const UMOInventoryComponent* Inventory = ControlledPawn->FindComponentByClass<UMOInventoryComponent>())
		{
			if (!Inventory->HasToolOfType(HarvestAction->RequiredToolType))
			{
				ActionTime *= HarvestAction->MissingToolTimeMultiplier;
			}
		}
	}

	return FMath::Max(0.1f, ActionTime);
}

TArray<FName> AMOSurvivorController::GetItemTagsForJobType(EMOSurvivorJobType JobType) const
{
	TArray<FName> MatchTags;

	switch (JobType)
	{
	case EMOSurvivorJobType::GatherWood:
		MatchTags.Add(FName(TEXT("Wood")));
		MatchTags.Add(FName(TEXT("Stick")));
		MatchTags.Add(FName(TEXT("Branch")));
		MatchTags.Add(FName(TEXT("Log")));
		MatchTags.Add(FName(TEXT("Firewood")));
		break;

	case EMOSurvivorJobType::GatherStone:
		MatchTags.Add(FName(TEXT("Stone")));
		MatchTags.Add(FName(TEXT("Rock")));
		MatchTags.Add(FName(TEXT("Pebble")));
		MatchTags.Add(FName(TEXT("Flint")));
		MatchTags.Add(FName(TEXT("Cobble")));
		break;

	case EMOSurvivorJobType::GatherFiber:
		MatchTags.Add(FName(TEXT("Fiber")));
		MatchTags.Add(FName(TEXT("Plant")));
		MatchTags.Add(FName(TEXT("Grass")));
		MatchTags.Add(FName(TEXT("Vine")));
		MatchTags.Add(FName(TEXT("Reed")));
		break;

	default:
		break;
	}

	return MatchTags;
}

bool AMOSurvivorController::DoesItemMatchJobType(FName ItemId, EMOSurvivorJobType JobType) const
{
	if (ItemId.IsNone())
	{
		return false;
	}

	// Get item definition to check tags
	FMOItemDefinitionRow ItemDef;
	if (!UMOItemDatabaseSettings::GetItemDefinition(ItemId, ItemDef))
	{
		// If we can't find the definition, try matching by ItemId name containing keywords
		FString ItemIdString = ItemId.ToString().ToLower();

		switch (JobType)
		{
		case EMOSurvivorJobType::GatherWood:
			return ItemIdString.Contains(TEXT("wood")) ||
			       ItemIdString.Contains(TEXT("stick")) ||
			       ItemIdString.Contains(TEXT("branch")) ||
			       ItemIdString.Contains(TEXT("log"));

		case EMOSurvivorJobType::GatherStone:
			return ItemIdString.Contains(TEXT("stone")) ||
			       ItemIdString.Contains(TEXT("rock")) ||
			       ItemIdString.Contains(TEXT("pebble")) ||
			       ItemIdString.Contains(TEXT("flint")) ||
			       ItemIdString.Contains(TEXT("cobble"));

		case EMOSurvivorJobType::GatherFiber:
			return ItemIdString.Contains(TEXT("fiber")) ||
			       ItemIdString.Contains(TEXT("plant")) ||
			       ItemIdString.Contains(TEXT("grass")) ||
			       ItemIdString.Contains(TEXT("vine"));

		default:
			return false;
		}
	}

	// Check item tags against job type tags
	TArray<FName> JobTags = GetItemTagsForJobType(JobType);

	for (const FName& ItemTag : ItemDef.Tags)
	{
		for (const FName& JobTag : JobTags)
		{
			if (ItemTag == JobTag)
			{
				return true;
			}
		}
	}

	// Also check the ItemId itself as a fallback
	FString ItemIdString = ItemId.ToString().ToLower();
	for (const FName& JobTag : JobTags)
	{
		if (ItemIdString.Contains(JobTag.ToString().ToLower()))
		{
			return true;
		}
	}

	return false;
}
