#include "MOSurvivorController.h"
#include "MOWorldCreationTypes.h"
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
#include "MOItemDefinitionRow.h"
#include "MOInventoryComponent.h"
#include "MOTerraformingComponent.h"   // (unit 3) TerraformAtLocationEx + excavation config
#include "MODesignationSubsystem.h"    // (unit 3) dig/dump zone lookup + budget
#include "MOKnowledgeComponent.h"
#include "MOCraftingSubsystem.h"
#include "MOCraftingQueueComponent.h"
#include "MOCraftingStationActor.h"
#include "MORecipeDatabaseSettings.h"
#include "MOInventoryHolderInterface.h"
#include "MOMedicalTreatmentActions.h"
#include "MOColonyManagerSubsystem.h"
#include "MOMedicalSubsystem.h"
#include "MOTimedActionComponent.h"
#include "MOCharacter.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "EngineUtils.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

namespace MOSurvivorJobWhy
{
	// What a player reads when a survivor's job fails (N-090). The BT gather / forage tasks use the same keys.
	FText NothingToGather() { return NSLOCTEXT("MOSurvivorJobs", "NothingToGather", "found nothing to gather nearby"); }
	FText CantReach() { return NSLOCTEXT("MOSurvivorJobs", "CantReach", "couldn't get there"); }
	FText StationGone() { return NSLOCTEXT("MOSurvivorJobs", "StationGone", "the station is gone"); }
	FText StorageGone() { return NSLOCTEXT("MOSurvivorJobs", "StorageGone", "the storage is gone"); }
	FText NoIngredients() { return NSLOCTEXT("MOSurvivorJobs", "NoIngredients", "the ingredients aren't in the storage"); }
	FText CraftCancelled() { return NSLOCTEXT("MOSurvivorJobs", "CraftCancelled", "the craft was cancelled"); }
	FText NothingLeftToDig() { return NSLOCTEXT("MOSurvivorJobs", "NothingLeftToDig", "there is nothing left to dig there"); }
	FText PatientGone() { return NSLOCTEXT("MOSurvivorJobs", "PatientGone", "the patient is gone"); }
	FText WoundTended() { return NSLOCTEXT("MOSurvivorJobs", "WoundTended", "the wound no longer needs that treatment"); }
}

void AMOSurvivorController::GatherRuntimeAssets(TArray<FSoftObjectPath>& Out) const
{
	Super::GatherRuntimeAssets(Out);
	if (!DefaultSurvivorBehaviorTree.IsNull())
	{
		Out.AddUnique(DefaultSurvivorBehaviorTree.ToSoftObjectPath());
	}
}

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
	// A restored pawn may receive its AI controller after its job snapshot was installed.
	HandleJobQueueChanged();
}

void AMOSurvivorController::OnUnPossess()
{
	// Unbind from job queue
	if (UMOSurvivorJobQueueComponent* JobQueue = CachedJobQueue.Get())
	{
		JobQueue->OnQueueChanged.RemoveDynamic(this, &AMOSurvivorController::HandleJobQueueChanged);
	}

	// Execution belongs to the pawn. Controller replacement/possession must not refund its paid craft
	// or discard the haul manifest; a new executor rebinds to the saved leg and completion receipt.
	if (CurrentJob.JobType == EMOSurvivorJobType::CraftAtStation)
	{
		ReleaseCraftJob(false, false);
		SimpleJobState = 0;
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
	ReleaseCraftJob(true);
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
	ReleaseCraftJob(true);

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
		// DEBUG (MOSurvivorController, disabled Oct 18): re-enable to trace a following survivor every 2 s
		// // Log movement debug info periodically
		// static float LastLogTime = 0.f;
		// UWorld* World = GetWorld();
		// if (World && (World->GetTimeSeconds() - LastLogTime) > 2.0f)
		// {
		// 	const FVector Velocity = ControlledPawn->GetVelocity();
		// 	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Following: distance=%.1f, velocity=%.1f (%.1f, %.1f, %.1f)"),
		// 		DistanceToTarget, Velocity.Size(), Velocity.X, Velocity.Y, Velocity.Z);
		// 	LastLogTime = World->GetTimeSeconds();
		// }

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
	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	if (JobQueue && JobQueue->IsRestoringJobs())
	{
		// Loading over an already possessed pawn replaces the execution checkpoint as well as its queue.
		ReleaseCraftJob(false, false);
		bIsProcessingJob = false;
		SimpleJobState = 0;
	}
	// Skip if already processing a job
	if (bIsProcessingJob)
	{
		return;
	}

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
	case EMOSurvivorJobType::TreatWound:
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
	// Treat-wound jobs walk to the patient and dress the wound (states 40-41).
	if (CurrentJob.JobType == EMOSurvivorJobType::TreatWound)
	{
		StartTreatJobExecution();
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
			FailSimpleJob(MOSurvivorJobWhy::NothingToGather());
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
	// Item/job notifications can synchronously re-enter the controller. One interval can perform its work only once.
	if (bUpdatingSimpleJob) return;
	TGuardValue<bool> Updating(bUpdatingSimpleJob, true);
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		CompleteSimpleJob(false);
		return;
	}

	// Treat-leg (40-41), excavate-leg (30-33), refuel-leg (20-23), and craft-leg (10-15) states live
	// in their own machines. Dispatch highest band first — the >= checks below
	// would otherwise swallow the higher bands.
	if (SimpleJobState >= 40)
	{
		UpdateTreatJobExecution(DeltaTime);
		return;
	}
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
				// Item/XP callbacks may change the assignment; retain the identity of the work just performed.
				const FGuid IterationJobId = CurrentJob.JobId;
				PerformSimpleJobAction();
				CompleteSimpleJobIteration(IterationJobId);
			}
			break;
		}

	default:
		// Invalid state
		CompleteSimpleJob(false);
		break;
	}
}

void AMOSurvivorController::CompleteSimpleJobIteration(const FGuid& ExpectedJobId)
{
	if (!bIsProcessingJob || CurrentJob.JobId != ExpectedJobId) return;
	auto* Queue = GetJobQueue();
	const auto Result = Queue ? Queue->RecordCompletedIteration(ExpectedJobId) : FMOSurvivorIterationResult{EMOSurvivorIterationFailure::WrongJob};
	// A queue observer may have cancelled or replaced the assignment while recording.
	if (!bIsProcessingJob || CurrentJob.JobId != ExpectedJobId) return;
	if (!Result.IsRecorded() || Queue->GetCurrentJob().JobId != ExpectedJobId)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Iteration acknowledgement refused for %s (failure %d)"), *ExpectedJobId.ToString(), int32(Result.Failure));
		CompleteSimpleJob(false);
		return;
	}
	CurrentJob.CompletedCount = Result.CompletedCount;
	CurrentJob.CraftExecution = {};
	if (CurrentJob.CompletedCount >= CurrentJob.RepeatCount) CompleteSimpleJob(true);
	else StartSimpleJobExecution();
}

void AMOSurvivorController::FailSimpleJob(const FText& Why)
{
	PendingFailureWhy = Why;
	CompleteSimpleJob(false);
}

void AMOSurvivorController::CompleteSimpleJob(bool bSuccess)
{
	// The reason belongs to this completion only (a success, or a failure nobody explained, reports none).
	const FText FailureWhy = bSuccess ? FText::GetEmpty() : PendingFailureWhy;
	PendingFailureWhy = FText::GetEmpty();
	const FGuid CompletedJobId = CurrentJob.JobId;
	ReleaseCraftJob(!bSuccess);
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
	if (JobQueue && CurrentJob.IsValid() && JobQueue->GetCurrentJob().JobId == CompletedJobId)
	{
		JobQueue->CompleteCurrentJobWithReason(bSuccess, FailureWhy);
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
	ReleaseCraftJob(false, false);
	auto* Queue = GetPawn() ? GetPawn()->FindComponentByClass<UMOCraftingQueueComponent>() : nullptr;
	const auto Saved = CurrentJob.CraftExecution;
	if (!Queue || (Saved.Phase == 0 && (!Queue->IsQueueEmpty() || Queue->GetPendingMaterialLotCount() > 0)))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Craft job requires an available pawn crafting queue"));
		CompleteSimpleJob(false);
		return;
	}
	CraftJobQueue = Queue;
	Queue->OnCraftCompleted.RemoveDynamic(this, &AMOSurvivorController::HandleJobCraftCompleted);
	Queue->OnCraftCompleted.AddDynamic(this, &AMOSurvivorController::HandleJobCraftCompleted);
	// Resolve job data. TargetActor carries the station, StorageActor the
	// container (see EnqueueCraftJob).
	CraftStationActor = CurrentJob.TargetActor;
	CraftStorageActor = CurrentJob.StorageActor;
	CraftJobRecipeId = CurrentJob.CraftRecipeId;
	CraftQueueEntryId = Saved.QueueEntryId;
	CraftBorrowedContainers = Saved.BorrowedContainers;
	CraftCompletedProducts = Saved.CompletedProducts;
	bJobCraftCompleted = Saved.bCraftCompleted;

	AActor* Station = CraftStationActor.Get();
	AActor* Storage = CraftStorageActor.Get();
	if ((!Station && CurrentJob.TargetActorGuid.IsValid()) || (!Storage && CurrentJob.StorageActorGuid.IsValid()))
	{
		// Actor records can hydrate later than the pawn record. Retain ownership and retry GUID resolution.
		SimpleJobState = 17;
		SimpleJobTimer = 0;
		return;
	}
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

	if (CurrentJob.CompletedCount >= CurrentJob.RepeatCount) { CompleteSimpleJob(true); return; }
	if (Saved.Phase != 0 && (Saved.Phase < 10 || Saved.Phase > 16 ||
		!FMath::IsFinite(Saved.HandlingSeconds) || Saved.HandlingSeconds < 0))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Invalid saved craft checkpoint; cargo is retained on pawn"));
		CompleteSimpleJob(false);
		return;
	}
	SimpleJobState = Saved.Phase == 0 ? 10 : Saved.Phase;
	SimpleJobTimer = Saved.HandlingSeconds;
	FMOCraftingCompletionReceipt Receipt;
	if (Queue->GetCompletionReceipt(CraftQueueEntryId, Receipt))
	{
		bJobCraftCompleted = true;
		CraftCompletedProducts = Receipt.ProducedItems;
	}
	// Handling/work can resume after player possession moved the pawn. It must physically return first.
	if ((SimpleJobState == 11 || SimpleJobState == 15) &&
		FVector::DistSquared(GetPawn()->GetActorLocation(), Storage->GetActorLocation()) > FMath::Square(250.0f))
	{
		SimpleJobState = SimpleJobState == 11 ? 10 : 14;
		SimpleJobTimer = 0;
	}
	if (SimpleJobState == 13 && !bJobCraftCompleted &&
		FVector::DistSquared(GetPawn()->GetActorLocation(), Station->GetActorLocation()) > FMath::Square(250.0f))
	{
		SimpleJobState = 12;
		SimpleJobTimer = 0;
	}
	SimpleJobTotalSeconds = 0.0f;
	MoveLegBestDistance = FLT_MAX;
	MoveLegNoProgressSeconds = 0.0f;
	SimpleJobTargetLocation = SimpleJobState == 12 ? Station->GetActorLocation() : Storage->GetActorLocation();
	if (SimpleJobState == 10 || SimpleJobState == 12 || SimpleJobState == 14)
		MoveToLocation(SimpleJobTargetLocation, 150.0f);
	PersistCraftExecution();

	if (UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue())
	{
		JobQueue->SetJobState(CurrentJob.JobId, SimpleJobState == 14 ? EMOSurvivorJobState::Returning :
			(SimpleJobState == 10 || SimpleJobState == 12 ? EMOSurvivorJobState::MovingToTarget : EMOSurvivorJobState::Performing));
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

	// Recipe work may take hours. Each movement leg already detects lack of
	// progress; a fixed whole-job timeout would fail valid simulated work.
	SimpleJobTotalSeconds += DeltaTime;
	if (!JobQueue || JobQueue->GetCurrentJob().JobId != CurrentJob.JobId)
	{
		// The job was cancelled/replaced externally. Do not complete the next job in its place.
		ReleaseCraftJob(true);
		bIsProcessingJob = false;
		SimpleJobState = 0;
		HandleJobQueueChanged();
		return;
	}

	auto SetProgress = [&](float Progress)
	{
		PersistCraftExecution();
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
			if (Arrive < 0) { FailSimpleJob(MOSurvivorJobWhy::CantReach()); return; }
			if (Arrive > 0)
			{
				SimpleJobState = 11;
				SimpleJobTimer = 0.0f;
				PersistCraftExecution();
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
					FailSimpleJob(MOSurvivorJobWhy::NoIngredients());
					return;
				}
				AActor* Station = CraftStationActor.Get();
				if (!Station) { FailSimpleJob(MOSurvivorJobWhy::StationGone()); return; }
				SimpleJobState = 12;
				MoveLegBestDistance = FLT_MAX;
				MoveLegNoProgressSeconds = 0.0f;
				SimpleJobTimer = 0.0f;
				SimpleJobTargetLocation = Station->GetActorLocation();
				MoveToLocation(SimpleJobTargetLocation, 150.0f);
				PersistCraftExecution();
				if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::MovingToTarget); }
			}
			break;
		}

	case 12: // moving to station
		{
			const int32 Arrive = UpdateJobMoveLeg(DeltaTime, 250.0f, 600.0f);
			if (Arrive < 0) { FailSimpleJob(MOSurvivorJobWhy::CantReach()); return; }
			if (Arrive > 0)
			{
				UMOCraftingQueueComponent* CraftQueue = ControlledPawn->FindComponentByClass<UMOCraftingQueueComponent>();
				const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(CraftJobRecipeId);
				if (!CraftQueue || !Recipe)
				{
					CompleteSimpleJob(false);
					return;
				}
				// The queue derives requirements from the real station, never from the recipe's enum as a fallback.
				FMOCraftingQueueEntry Existing;
				FMOCraftingCompletionReceipt Completed;
				const bool bEnqueued = CraftQueueEntryId.IsValid()
					? (CraftQueue->GetQueueEntry(CraftQueueEntryId, Existing) || CraftQueue->GetCompletionReceipt(CraftQueueEntryId, Completed))
					: CraftQueue->EnqueueCraftAtStationWithId(CraftJobRecipeId, 1, CraftStationActor.Get(), CraftQueueEntryId,
					[this](const FGuid& EntryId)
					{
						CraftQueueEntryId = EntryId;
						SimpleJobState = 13;
						SimpleJobTimer = 0;
						PersistCraftExecution();
					});
				if (!bEnqueued)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Craft enqueue failed for %s"),
						*CraftJobRecipeId.ToString());
					FText Why;
					if (const UMOCraftingSubsystem* Crafting = GetWorld() ? GetWorld()->GetSubsystem<UMOCraftingSubsystem>() : nullptr)
					{
						Why = Crafting->CanCraftRecipeAtStation(CraftJobRecipeId, ControlledPawn->FindComponentByClass<UMOKnowledgeComponent>(),
							ControlledPawn->FindComponentByClass<UMOSkillsComponent>(), ControlledPawn->FindComponentByClass<UMOInventoryComponent>(),
							CraftStationActor.Get()).FailureReason;
					}
					FailSimpleJob(Why);
					return;
				}
				SimpleJobState = 13;
				SimpleJobTimer = 0.0f;
				PersistCraftExecution();
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
			FMOCraftingQueueEntry OwnedEntry;
			FMOCraftingCompletionReceipt Receipt;
			if (CraftQueue->GetCompletionReceipt(CraftQueueEntryId, Receipt))
			{
				CraftCompletedProducts = Receipt.ProducedItems;
				bJobCraftCompleted = true;
				PersistCraftExecution();
			}
			if (!bJobCraftCompleted && !CraftQueue->GetQueueEntry(CraftQueueEntryId, OwnedEntry))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Owned craft was cancelled before completion"));
				FailSimpleJob(MOSurvivorJobWhy::CraftCancelled());
				return;
			}
			if (bJobCraftCompleted)
			{
				CraftQueue->DeliverPendingMaterials();
				if (CraftQueue->GetPendingMaterialLotCount() > 0) break;
				// Craft finished - outputs are in the pawn inventory. Head back.
				AActor* Storage = CraftStorageActor.Get();
				if (!Storage) { FailSimpleJob(MOSurvivorJobWhy::StorageGone()); return; }
				SimpleJobState = 14;
				MoveLegBestDistance = FLT_MAX;
				MoveLegNoProgressSeconds = 0.0f;
				SimpleJobTimer = 0.0f;
				SimpleJobTargetLocation = Storage->GetActorLocation();
				MoveToLocation(SimpleJobTargetLocation, 150.0f);
				PersistCraftExecution();
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
			if (Arrive < 0) { FailSimpleJob(MOSurvivorJobWhy::CantReach()); return; }
			if (Arrive > 0)
			{
				SimpleJobState = 15;
				SimpleJobTimer = 0.0f;
				PersistCraftExecution();
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
				const FGuid IterationJobId = CurrentJob.JobId;
				if (!DepositCraftOutputs())
				{
					// Keep the job/cargo and retry after another real handling interval.
					SimpleJobTimer = 0.0f;
					PersistCraftExecution();
					break;
				}

				CompleteSimpleJobIteration(IterationJobId);
			}
			break;
		}

	case 16: // Cargo was delivered; a save inside its inventory notification precedes iteration acknowledgement.
		CompleteSimpleJobIteration(CurrentJob.JobId);
		break;
	case 17: // Waiting for saved actor identities to hydrate. Never withdraw again while holding prior cargo.
		SimpleJobTimer += DeltaTime;
		if (SimpleJobTimer >= 0.5f)
		{
			CurrentJob = JobQueue->GetCurrentJob();
			StartCraftJobExecution();
		}
		break;
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
		if (!Station) { FailSimpleJob(MOSurvivorJobWhy::StationGone()); }
		else if (!Storage) { FailSimpleJob(MOSurvivorJobWhy::StorageGone()); }
		else { CompleteSimpleJob(false); }
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
			if (Arrive < 0) { FailSimpleJob(MOSurvivorJobWhy::CantReach()); return; }
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
				if (!Station) { FailSimpleJob(MOSurvivorJobWhy::StationGone()); return; }
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
			if (Arrive < 0) { FailSimpleJob(MOSurvivorJobWhy::CantReach()); return; }
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

void AMOSurvivorController::StartTreatJobExecution()
{
	AActor* Patient = CurrentJob.TargetActor.Get();
	if (!IsValid(Patient) || !GetPawn())
	{
		FailSimpleJob(MOSurvivorJobWhy::PatientGone());
		return;
	}
	// Supplies in a settlement store: fetch them first (42-43); otherwise straight to the patient (40).
	AActor* Store = CurrentJob.StorageActor.Get();
	SimpleJobState = IsValid(Store) ? 42 : 40;
	bTreatDirectApproach = false;
	SimpleJobTimer = 0.0f;
	SimpleJobTotalSeconds = 0.0f;
	MoveLegBestDistance = FLT_MAX;
	MoveLegNoProgressSeconds = 0.0f;
	SimpleJobTargetLocation = IsValid(Store) ? Store->GetActorLocation() : Patient->GetActorLocation();
	MoveToLocation(SimpleJobTargetLocation, IsValid(Store) ? 150.0f : 80.0f);
	if (UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue())
	{
		JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::MovingToTarget);
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Treat job started: %s on %s (supplies from %s)"),
		*CurrentJob.TreatmentId.ToString(), *Patient->GetName(), IsValid(Store) ? *Store->GetName() : TEXT("own pack"));
}

bool AMOSurvivorController::TakeTreatmentSupplies()
{
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UMOMedicalSubsystem* Medical = GI ? GI->GetSubsystem<UMOMedicalSubsystem>() : nullptr;
	UMOInventoryComponent* StorageInv = GetActorInventory(CurrentJob.StorageActor.Get());
	UMOInventoryComponent* PawnInv = GetPawn() ? GetPawn()->FindComponentByClass<UMOInventoryComponent>() : nullptr;
	FMOMedicalTreatmentRow Treatment;
	if (!Medical || !StorageInv || !PawnInv || !Medical->GetTreatmentDefinition(CurrentJob.TreatmentId, Treatment))
	{
		return false;
	}
	TArray<FMOInventoryEntry> StoreEntries;
	StorageInv->GetInventoryEntries(StoreEntries);
	TArray<FMOInventoryEntry> PackEntries;
	PawnInv->GetInventoryEntries(PackEntries);
	TMap<FName, int32> Consumables;
	TArray<FGuid> ToolsToCarry;
	for (FName Id : Treatment.RequiredItemIds)
	{
		FMOItemDefinitionRow Def;
		if (!UMOItemDatabaseSettings::GetItemDefinition(Id, Def)) return false;
		if (!Def.bIsTool)
		{
			Consumables.FindOrAdd(Id)++;
			continue;
		}
		// A tool is carried whole, durability and all: a usable one in the pack is enough, else the store's usable one comes along.
		const auto Usable = [Id](const FMOInventoryEntry& Entry) { return Entry.ItemDefinitionId == Id && Entry.Quantity > 0 && Entry.CurrentDurability != 0; };
		if (PackEntries.ContainsByPredicate(Usable)) continue;
		const FMOInventoryEntry* Tool = StoreEntries.FindByPredicate(Usable);
		if (!Tool) return false;
		ToolsToCarry.AddUnique(Tool->ItemGuid);
	}
	// Only the shortfall: what the pack already holds is used first.
	for (auto It = Consumables.CreateIterator(); It; ++It)
	{
		It.Value() -= FMath::Min(It.Value(), PawnInv->CountIngredient(It.Key()));
		if (It.Value() <= 0) It.RemoveCurrent();
	}
	FMOInventoryMutationScope StorageMutation(StorageInv);
	FMOInventoryMutationScope PawnMutation(PawnInv);
	if (Consumables.Num() > 0)
	{
		const auto Result = StorageInv->TransferIngredientsTo(PawnInv, Consumables);
		if (!Result.IsComplete())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Treat job: the store lacks %s (%d short)"),
				*Result.IngredientId.ToString(), Result.MissingUnits);
			return false;
		}
	}
	for (const FGuid& ToolGuid : ToolsToCarry)
	{
		if (!StorageInv->TryTransferItem(ToolGuid, PawnInv).IsComplete()) return false;
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Treat job: took the supplies for %s from %s"),
		*CurrentJob.TreatmentId.ToString(), *CurrentJob.StorageActor->GetName());
	return true;
}

void AMOSurvivorController::UpdateTreatJobExecution(float DeltaTime)
{
	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	APawn* Medic = GetPawn();
	AActor* Patient = CurrentJob.TargetActor.Get();
	// A failed attempt is told to the colony, which leaves this patient a while before sending anyone again.
	const auto NoteFailed = [this, Patient]()
	{
		if (UMOColonyManagerSubsystem* Colony = GetWorld() ? GetWorld()->GetSubsystem<UMOColonyManagerSubsystem>() : nullptr)
		{
			Colony->NoteMedicCouldNotTreat(Patient);
		}
	};
	const AMOCharacter* PatientCharacter = Cast<AMOCharacter>(Patient);
	if (!Medic || !IsValid(Patient) || (PatientCharacter && PatientCharacter->IsDead()))
	{
		FailSimpleJob(MOSurvivorJobWhy::PatientGone());
		return;
	}

	// GLOBAL JOB WATCHDOG (B1): the longest authored treatment (a suture) is 60 s; a walk across the settlement fits in the rest.
	SimpleJobTotalSeconds += DeltaTime;
	if (SimpleJobTotalSeconds > 180.0f)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Treat job WEDGED in state %d after %.0fs - failing"),
			SimpleJobState, SimpleJobTotalSeconds);
		NoteFailed();
		CompleteSimpleJob(false);
		return;
	}

	const UMOAnatomyComponent* Anatomy = Patient->FindComponentByClass<UMOAnatomyComponent>();
	const FMOWound* Wound = nullptr;
	TArray<FMOWound> Wounds;
	if (Anatomy)
	{
		Wounds = Anatomy->GetAllWounds();
		Wound = Wounds.FindByPredicate([this](const FMOWound& Entry) { return Entry.WoundId == CurrentJob.WoundId; });
	}

	switch (SimpleJobState)
	{
	case 42: // walking to the store
		{
			if (!CurrentJob.StorageActor.IsValid()) { FailSimpleJob(MOSurvivorJobWhy::StorageGone()); return; }
			const int32 Arrive = UpdateJobMoveLeg(DeltaTime, 250.0f, 600.0f);
			if (Arrive < 0)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Treat job FAILED: can't reach the store %s"), *GetNameSafe(CurrentJob.StorageActor.Get()));
				NoteFailed();   // the colony would otherwise send it to the same store every tick
				FailSimpleJob(MOSurvivorJobWhy::CantReach());
				return;
			}
			if (Arrive > 0)
			{
				SimpleJobState = 43;
				SimpleJobTimer = 0.0f;
				if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Performing); }
			}
			if (JobQueue) { JobQueue->UpdateJobProgress(CurrentJob.JobId, 0.05f); }
			break;
		}

	case 43: // taking the supplies (one combined handling gesture)
		{
			SimpleJobTimer += DeltaTime;
			if (SimpleJobTimer < CraftHandlingDuration) break;
			if (!TakeTreatmentSupplies())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Treat job: no %s supplies in the store - failing"),
					*CurrentJob.TreatmentId.ToString());
				CompleteSimpleJob(false);
				return;
			}
			SimpleJobState = 40;
			MoveLegBestDistance = FLT_MAX;
			MoveLegNoProgressSeconds = 0.0f;
			SimpleJobTargetLocation = Patient->GetActorLocation();
			MoveToLocation(SimpleJobTargetLocation, 80.0f);
			if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::MovingToTarget); }
			break;
		}

	case 40: // walking to the patient
		{
			if (!Wound)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Treat job FAILED: %s's wound no longer needs it"), *Patient->GetName());
				FailSimpleJob(MOSurvivorJobWhy::WoundTended());
				return;
			}
			// A patient on its feet moves: follow it rather than kneeling where it stood.
			if (FVector::Dist2D(Patient->GetActorLocation(), SimpleJobTargetLocation) > 100.0f)
			{
				SimpleJobTargetLocation = Patient->GetActorLocation();
				MoveToLocation(SimpleJobTargetLocation, 60.0f, true, /*bUsePathfinding=*/!bTreatDirectApproach);
			}
			const int32 Arrive = UpdateJobMoveLeg(DeltaTime, 150.0f, 150.0f);
			if (Arrive < 0)
			{
				// A standing body carves the nav mesh around itself: the path ends a couple of metres short of the patient (Oct 20: stalled at
				// 202 cm, again every upkeep tick). The last stretch is open ground: walk straight in, once.
				const float Short = FVector::Dist2D(Medic->GetActorLocation(), Patient->GetActorLocation());
				if (!bTreatDirectApproach && Short < 400.0f)
				{
					bTreatDirectApproach = true;
					MoveLegBestDistance = FLT_MAX;
					MoveLegNoProgressSeconds = 0.0f;
					SimpleJobTargetLocation = Patient->GetActorLocation();
					MoveToLocation(SimpleJobTargetLocation, 60.0f, true, /*bUsePathfinding=*/false);
					break;
				}
				UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Treat job FAILED: can't reach %s (%.0f cm off)"), *Patient->GetName(), Short);
				NoteFailed();
				FailSimpleJob(MOSurvivorJobWhy::CantReach());
				return;
			}
			if (Arrive == 0)
			{
				if (JobQueue) { JobQueue->UpdateJobProgress(CurrentJob.JobId, 0.1f); }
				break;
			}
			// At the bedside: Begin checks reach, sight, skill and supplies on the server like a player's own treatment.
			const FMOMedicalActionResult Result = FMOMedicalTreatmentActions::Begin(Medic, Patient, CurrentJob.WoundId, CurrentJob.TreatmentId);
			if (!Result.IsAccepted())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Treat job refused at %s: %s"),
					*Patient->GetName(), *Result.Describe().ToString());
				NoteFailed();
				FailSimpleJob(Result.Describe());
				return;
			}
			TreatStartBleedRate = Wound->BleedRate;
			SimpleJobState = 41;
			SimpleJobTimer = 0.0f;
			if (JobQueue) { JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::Performing); }
			break;
		}

	case 41: // treating (this pawn's timed action)
		{
			SimpleJobTimer += DeltaTime;
			const UMOTimedActionComponent* Timed = Medic->FindComponentByClass<UMOTimedActionComponent>();
			if (Timed && Timed->IsActive())
			{
				if (JobQueue) { JobQueue->UpdateJobProgress(CurrentJob.JobId, 0.5f); }
				break;
			}
			// The action ended: it worked if the wound now bleeds less (or has closed).
			const bool bWorked = !Wound || Wound->BleedRate < TreatStartBleedRate;
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Treat job %s: %s on %s, bleed %.2f -> %.2f mL/s after %.0fs"),
				bWorked ? TEXT("done") : TEXT("FAILED"), *CurrentJob.TreatmentId.ToString(), *Patient->GetName(),
				TreatStartBleedRate, Wound ? Wound->BleedRate : 0.0f, SimpleJobTimer);
			if (!bWorked)
			{
				NoteFailed();   // interrupted (the patient moved) or no better
			}
			CompleteSimpleJob(bWorked);
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
	ExcavateCarriedYields.Reset();

	FMODesignationZone DigZone;
	if (!Desig->FindZone(ExcavateDigZoneId, DigZone) || DigZone.RemainingVolumeM3 <= 0.0f)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Excavate job: dig zone missing/exhausted - failing"));
		FailSimpleJob(MOSurvivorJobWhy::NothingLeftToDig());
		return;
	}

	// Where the bite comes from: the highest live ground of the zone (a flatten zone: only above its target). With no terrain collision
	// under the zone yet (no samples) the centre, as before stage 5; a flatten zone with nothing left above its target is done.
	FVector DigPoint = DigZone.Center;
	int32 DigSamples = 0;
	if (DigZone.Kind == EMODesignationKind::Flatten)
	{
		FVector FlattenFill;
		if (!Desig->FindFlattenWork(DigZone, DigPoint, FlattenFill))
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Excavate job: flatten zone %s is level (or needs outside earth) - done"), *ExcavateDigZoneId.ToString(EGuidFormats::Short));
			CompleteSimpleJob(false);
			return;
		}
	}
	else if (!Desig->FindBitePoint(DigZone, /*bDig=*/true, DigPoint, DigSamples))
	{
		DigPoint = DigZone.Center;   // no terrain collision sampled under the zone yet: the centre, as before stage 5
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
		// The lowest live ground below the target (a flatten zone: ANY amount below); a zone sampled at its target already takes no more
		// earth (fail BEFORE digging).
		int32 FillSamples = 0;
		const float FillTolerance = DumpZone.Kind == EMODesignationKind::Flatten ? 0.0f : -1.0f;
		if (!Desig->FindBitePoint(DumpZone, /*bDig=*/false, ExcavateDumpLocation, FillSamples, FillTolerance))
		{
			if (FillSamples > 0)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] Excavate job: fill zone %s is at its target height - nothing to dig for"), *ExcavateDumpZoneId.ToString(EGuidFormats::Short));
				CompleteSimpleJob(false);
				return;
			}
			ExcavateDumpLocation = DumpZone.Center;
		}
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
	const float RealBiteM3 = UMOTerraformingComponent::ComputeMovedVolumeCubicMeters(BiteRadius, BiteStrength);   // the sculpt's real earth
	ExcavateBiteVolumeM3 = FMath::Min(RealBiteM3, DigZone.RemainingVolumeM3);
	// What the bite cuts decides how long it takes, as for the player's dig (FMOGeologyYield::DigTimeMultiplier: earth 1, limestone ~5, granite ~40).
	const float BiteHardness = FMOGeologyYield::DigTimeMultiplierInWorld(this, DigPoint, BiteRadius, RealBiteM3, /*bSlab=*/true);
	ExcavateBiteDuration = UMOTerraformingComponent::ComputeDurationForVolumeSeconds(
		RealBiteM3, Terra->TerraformSecondsPerCubicMeter * BiteHardness, Terra->TerraformDurationSeconds);
	// The wedge watchdog must clear the (possibly hours-long) dig + deposit + travel.
	ExcavateWatchdogSeconds = FMath::Max(120.0f, ExcavateBiteDuration * 2.5f + 60.0f);

	SimpleJobState = 30; // moving to the dig zone
	SimpleJobTimer = 0.0f;
	SimpleJobTotalSeconds = 0.0f;
	MoveLegBestDistance = FLT_MAX;
	MoveLegNoProgressSeconds = 0.0f;
	SimpleJobTargetLocation = DigPoint;
	MoveToLocation(SimpleJobTargetLocation, 150.0f);

	if (UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue())
	{
		JobQueue->SetJobState(CurrentJob.JobId, EMOSurvivorJobState::MovingToTarget);
	}

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOSurvivorController] Excavate job started: dig %s at z=%.0f -> %s (bite %.3fm3, %.1fs/leg, watchdog %.0fs)"),
		*ExcavateDigZoneId.ToString(EGuidFormats::Short), DigPoint.Z,
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
			if (Arrive < 0) { FailSimpleJob(MOSurvivorJobWhy::CantReach()); return; }
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
				// What the bite cut through, from the world's geology (Wes, Oct 17 2026): the soil (the job's spoil item), the bedrock's stone, any ore body.
				ExcavateCarriedYields.Reset();
				if (!FMOGeologyYield::ForDigInWorld(this, SimpleJobTargetLocation, Terra->ExcavationDigRadius, MovedVol, Terra->ExcavationItemsPerCubicMeter,
					/*bSlab=*/true, ExcavateSpoilItemId, ExcavateCarriedYields))
				{
					ExcavateCarriedYields.Add({ ExcavateSpoilItemId, UMOTerraformingComponent::SpoilItemsForVolume(MovedVol, Terra->ExcavationItemsPerCubicMeter), true });   // no world seed (tests): plain earth
				}
				ExcavateCarried = 0;
				for (const FMOItemYield& Y : ExcavateCarriedYields)
				{
					ExcavateCarried += Y.Count;
				}
				if (UMOInventoryComponent* PawnInv = ControlledPawn->FindComponentByClass<UMOInventoryComponent>())
				{
					const int32 Dropped = FMOGeologyYield::GiveOrDrop(PawnInv, ExcavateCarriedYields, SimpleJobTargetLocation);
					UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Excavate bite %.3fm3 yielded %s%s"), MovedVol, *FMOGeologyYield::Describe(ExcavateCarriedYields),
						Dropped > 0 ? *FString::Printf(TEXT(" (%d dropped: no room)"), Dropped) : TEXT(""));
				}

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
			if (Arrive < 0) { FailSimpleJob(MOSurvivorJobWhy::CantReach()); return; }
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
					// Raise terrain at the fill zone's lowest ground NOW (others may have filled since this trip began), consuming the carried
					// spoil (conservation).
					UMODesignationSubsystem* Desig = UMODesignationSubsystem::Get(this);
					FMODesignationZone DumpZone;
					FVector FillPoint;
					int32 FillSamples = 0;
					if (Desig && Desig->FindZone(ExcavateDumpZoneId, DumpZone)
						&& Desig->FindBitePoint(DumpZone, /*bDig=*/false, FillPoint, FillSamples, DumpZone.Kind == EMODesignationKind::Flatten ? 0.0f : -1.0f))
					{
						ExcavateDumpLocation = FillPoint;
					}
					const float Raised = Terra->TerraformAtLocationEx(ExcavateDumpLocation,
						EMOTerraformMode::Raise, Terra->ExcavationDigRadius, Terra->ExcavationDigStrength);
					// The earth and stone become the fill (conservation); the ore stays in the pawn's pack.
					int32 Filled = 0;
					for (const FMOItemYield& Y : ExcavateCarriedYields)
					{
						if (PawnInv && Y.bFill && Y.Count > 0)
						{
							const int32 Have = FMath::Min(Y.Count, PawnInv->GetItemCountByDefinitionId(Y.ItemId));
							if (Have > 0)
							{
								PawnInv->RemoveItemByDefinitionId(Y.ItemId, Have);
								Filled += Have;
							}
						}
					}
					// A flatten zone is both ends of the trip: its budget caps the earth DUG, so the fill does not count it twice.
					if (Desig && ExcavateDumpZoneId != ExcavateDigZoneId)
					{
						Desig->ConsumeZoneVolume(ExcavateDumpZoneId, Raised > 0.0f ? Raised : ExcavateBiteVolumeM3);
					}
					UE_LOG(LogMOFramework, Warning,
						TEXT("[MOSurvivorController] Excavate job done: filled %.3fm3, consumed %d spoil of %d carried (ore kept)"),
						Raised, Filled, ExcavateCarried);
				}
				else // Container
				{
					UMOInventoryComponent* ContainerInv = GetActorInventory(CurrentJob.StorageActor.Get());
					int32 Deposited = 0;
					for (const FMOItemYield& Y : ExcavateCarriedYields)
					{
						const int32 Have = PawnInv ? FMath::Min(Y.Count, PawnInv->GetItemCountByDefinitionId(Y.ItemId)) : 0;
						if (ContainerInv && Have > 0 && ContainerInv->CanAddItemByDefinitionId(Y.ItemId, Have))
						{
							ContainerInv->AddItemByGuid(FGuid::NewGuid(), Y.ItemId, Have);
							PawnInv->RemoveItemByDefinitionId(Y.ItemId, Have);
							Deposited += Have;
						}
					}
					UE_LOG(LogMOFramework, Warning,
						TEXT("[MOSurvivorController] Excavate job done: deposited %d/%d spoil into container (rest kept)"),
						Deposited, ExcavateCarried);
				}

				ExcavateCarried = 0;
				ExcavateCarriedYields.Reset();
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

	TMap<FName, int32> Requirements;
	for (const FMORecipeIngredient& Ing : Recipe->Ingredients)
	{
		const int64 Total = int64(Requirements.FindRef(Ing.ItemDefinitionId)) + Ing.Quantity;
		if (Ing.ItemDefinitionId.IsNone() || Ing.Quantity <= 0 || Total > MAX_int32) return false;
		Requirements.Add(Ing.ItemDefinitionId, int32(Total));
	}
	FMOIngredientTransferOptions Carry;
	Carry.bCarrySourceContainers = true;
	FMOInventoryMutationScope StorageMutation(StorageInv);
	FMOInventoryMutationScope PawnMutation(PawnInv);
	const auto Result = StorageInv->TransferIngredientsTo(PawnInv, Requirements, Carry, &CraftBorrowedContainers);
	if (!Result.IsComplete())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Ingredient haul refused: failure=%d ingredient=%s missing=%d"),
			int32(Result.Failure), *Result.IngredientId.ToString(), Result.MissingUnits);
		return false;
	}

	SimpleJobState = 12;
	SimpleJobTimer = 0;
	PersistCraftExecution();
	UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Withdrew ingredients for %s from %s"),
		*CraftJobRecipeId.ToString(), *CraftStorageActor->GetName());
	return true;
}

bool AMOSurvivorController::DepositCraftOutputs()
{
	APawn* ControlledPawn = GetPawn();
	UMOInventoryComponent* StorageInv = GetActorInventory(CraftStorageActor.Get());
	UMOInventoryComponent* PawnInv = ControlledPawn ? ControlledPawn->FindComponentByClass<UMOInventoryComponent>() : nullptr;
	if (!StorageInv || !PawnInv || !bJobCraftCompleted)
	{
		return false;
	}
	FMOIngredientTransferOptions Carry;
	Carry.bCarrySourceContainers = true;
	for (const auto& Guid : CraftBorrowedContainers)
	{
		FMOInventoryEntry Entry;
		// A vessel explicitly consumed as a recipe ingredient no longer exists to return.
		if (PawnInv->TryGetEntryByGuid(Guid, Entry)) Carry.AlsoCarryItemGuids.Add(Guid);
	}
	FMOInventoryMutationScope PawnMutation(PawnInv);
	FMOInventoryMutationScope StorageMutation(StorageInv);
	const auto Result = PawnInv->TransferIngredientsTo(StorageInv, CraftCompletedProducts, Carry);
	if (!Result.IsComplete())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorController] Deposit waiting: failure=%d ingredient=%s; cargo stays on pawn"),
			int32(Result.Failure), *Result.IngredientId.ToString());
		return false;
	}
	// Update cargo ownership before deferred inventory observers can save/cancel the job.
	CraftCompletedProducts.Reset();
	CraftBorrowedContainers.Reset();
	SimpleJobState = 16;
	SimpleJobTimer = 0;
	PersistCraftExecution();
	if (auto* Queue = CraftJobQueue.Get()) Queue->AcknowledgeCompletionReceipt(CraftQueueEntryId);
	return true;
}

void AMOSurvivorController::HandleJobCraftCompleted(const FGuid& EntryId, const FMOCraftResult& Result)
{
	if (EntryId != CraftQueueEntryId || !Result.bSuccess) return;
	CraftCompletedProducts = Result.ProducedItems;
	bJobCraftCompleted = true;
	PersistCraftExecution();
}

bool AMOSurvivorController::PersistCraftExecution()
{
	if (SimpleJobState < 10 || SimpleJobState > 16) return false;
	FMOSurvivorCraftExecution State;
	State.Phase = SimpleJobState;
	State.HandlingSeconds = SimpleJobTimer;
	State.QueueEntryId = CraftQueueEntryId;
	State.BorrowedContainers = CraftBorrowedContainers;
	State.CompletedProducts = CraftCompletedProducts;
	State.bCraftCompleted = bJobCraftCompleted;
	auto* Jobs = GetJobQueue();
	return Jobs && Jobs->SetCraftExecution(CurrentJob.JobId, State);
}

void AMOSurvivorController::ReleaseCraftJob(bool bCancelOwnedCraft, bool bForgetReceipt)
{
	const FGuid OwnedId = CraftQueueEntryId;
	CraftQueueEntryId.Invalidate();
	if (auto* Queue = CraftJobQueue.Get())
	{
		Queue->OnCraftCompleted.RemoveDynamic(this, &AMOSurvivorController::HandleJobCraftCompleted);
		if (bCancelOwnedCraft && OwnedId.IsValid()) Queue->CancelCraft(OwnedId, true);
		if (bForgetReceipt) Queue->AcknowledgeCompletionReceipt(OwnedId);
	}
	CraftJobQueue.Reset();
	CraftBorrowedContainers.Reset();
	CraftCompletedProducts.Reset();
	bJobCraftCompleted = false;
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

	// DEBUG (MOSurvivorController, disabled Oct 18): re-enable to trace resource searches
	// UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] FindNearestGatherResource: JobType=%d, SearchRadius=%.0f"),
	// 	static_cast<int32>(JobType), SearchRadius);

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

			// DEBUG (MOSurvivorController, disabled Oct 18): re-enable to trace verbose detail
			// UE_LOG(LogMOFramework, Verbose, TEXT("[MOSurvivorController] Found matching ground resource: %s at %s"),
			// 	*Instance.ItemId.ToString(), *SimpleJobTargetLocation.ToString());
			return true;
		}
	}

	// DEBUG (MOSurvivorController, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOSurvivorController] No matching resources for job type %d in %d instances"),
	// 	static_cast<int32>(JobType), Instances.Num());
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

	// Iterate all actors looking for KeepOnHarvest ISM/HISM with the required tag (every actor of the world: the cost is logged, perf sweep Oct 22)
	const double SearchStart = FPlatformTime::Seconds();
	int32 ActorsWalked = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		++ActorsWalked;
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorController] harvest target search ('%s'): %d actor(s) walked, %s, %.2f ms"), *RequiredTag.ToString(), ActorsWalked,
		bFound ? TEXT("found") : TEXT("none"), (FPlatformTime::Seconds() - SearchStart) * 1000.0);
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

	return FMath::Max(0.1f, FMOWorldRules::ScaleSeconds(this, EMOWorldPace::Gathering, ActionTime));   // the world's gathering pace, as a player's harvest
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
