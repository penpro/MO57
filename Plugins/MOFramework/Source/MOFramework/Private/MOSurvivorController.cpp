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
#include "MOKnowledgeComponent.h"
#include "MOCraftingSubsystem.h"
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
