#include "MORecruitmentComponent.h"
#include "MOFramework.h"
#include "MOSpawnManagerSubsystem.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MOSurvivorController.h"
#include "AIController.h"
#include "Net/UnrealNetwork.h"

UMORecruitmentComponent::UMORecruitmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.0f;  // Check every second
	SetIsReplicatedByDefault(true);
}

void UMORecruitmentComponent::BeginPlay()
{
	Super::BeginPlay();

	// Roll for quest on spawn if we have possible quests
	if (PossibleQuests.Num() > 0 && RecruitmentState == EMORecruitmentState::Wandering)
	{
		if (FMath::FRand() < QuestChance)
		{
			RollRandomQuest();
		}
	}
}

void UMORecruitmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Only tick on server/authority
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// Check quest timeout
	if (RecruitmentState == EMORecruitmentState::QuestActive)
	{
		CheckQuestTimeout();
	}
}

void UMORecruitmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMORecruitmentComponent, RecruitmentState);
	DOREPLIFETIME(UMORecruitmentComponent, ActiveQuest);
}

void UMORecruitmentComponent::OnRep_RecruitmentState()
{
	// State changed from replication
	UE_LOG(LogMOFramework, Verbose, TEXT("[Recruitment] State replicated: %d"), (int32)RecruitmentState);
}

// ============================================================================
// API
// ============================================================================

bool UMORecruitmentComponent::BeginInteraction(APawn* InteractingPawn)
{
	if (!InteractingPawn)
	{
		return false;
	}

	// Check if this pawn can be recruited at all (e.g., creatures like deer cannot)
	if (!bIsRecruitable)
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[Recruitment] %s is not recruitable"), *GetOwner()->GetName());
		return false;
	}

	if (RecruitmentState == EMORecruitmentState::Recruited)
	{
		// Already recruited
		return false;
	}

	if (RecruitmentState == EMORecruitmentState::Dead)
	{
		// Can't interact with dead
		return false;
	}

	// Mark as interacted
	if (RecruitmentState == EMORecruitmentState::Wandering)
	{
		// Check if we have a quest to give
		if (ActiveQuest.QuestType != EMORecruitmentQuestType::None)
		{
			// Start quest
			SetRecruitmentState(EMORecruitmentState::QuestActive);
			QuestStartTime = FDateTime::Now();
			NotifySpawnManagerOfInteraction();

			UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] Quest started: %s, timeout: %.1f hours"),
				*UEnum::GetValueAsString(ActiveQuest.QuestType),
				ActiveQuest.TimeoutSeconds / 3600.0f);

			return true;
		}
		else
		{
			// No quest - immediate recruitment
			ForceRecruit();
			return true;
		}
	}

	// Already interacted/questing - try to complete
	if (RecruitmentState == EMORecruitmentState::QuestActive || RecruitmentState == EMORecruitmentState::Interacted)
	{
		return TryCompleteQuest(InteractingPawn);
	}

	return false;
}

bool UMORecruitmentComponent::TryCompleteQuest(APawn* InteractingPawn)
{
	if (RecruitmentState != EMORecruitmentState::QuestActive && RecruitmentState != EMORecruitmentState::Interacted)
	{
		return false;
	}

	if (!CanCompleteQuest(InteractingPawn))
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[Recruitment] Cannot complete quest - requirements not met"));
		return false;
	}

	// Consume items if required
	if (!CheckQuestRequirements(InteractingPawn, true))
	{
		return false;
	}

	// Quest completed!
	UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] Quest completed successfully"));
	OnQuestCompleted.Broadcast();
	ForceRecruit();

	return true;
}

void UMORecruitmentComponent::ForceRecruit()
{
	// Check if this pawn can be recruited at all
	if (!bIsRecruitable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[Recruitment] ForceRecruit called on non-recruitable pawn: %s"), *GetOwner()->GetName());
		return;
	}

	SetRecruitmentState(EMORecruitmentState::Recruited);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Remove from spawn tracking entirely - recruited pawns should NEVER despawn
	UMOSpawnManagerSubsystem* SpawnManager = World->GetSubsystem<UMOSpawnManagerSubsystem>();
	if (SpawnManager)
	{
		SpawnManager->RemoveFromTracking(Cast<APawn>(GetOwner()));
	}

	// Spawn survivor AI controller and possess the pawn
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->HasAuthority())
	{
		// Get the AI controller class from the pawn (set in Blueprint or defaults)
		TSubclassOf<AController> AIControllerClass = OwnerPawn->AIControllerClass;

		// Ensure we use a survivor controller class - fallback if not set or wrong type
		if (!AIControllerClass || !AIControllerClass->IsChildOf(AMOSurvivorController::StaticClass()))
		{
			UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] Using default AMOSurvivorController (pawn AIControllerClass was %s)"),
				AIControllerClass ? *AIControllerClass->GetName() : TEXT("None"));
			AIControllerClass = AMOSurvivorController::StaticClass();
		}

		// Check if pawn already has a valid survivor controller
		AController* ExistingController = OwnerPawn->GetController();
		if (!ExistingController || !ExistingController->IsA<AMOSurvivorController>())
		{
			// Unpossess from any existing controller
			if (ExistingController)
			{
				ExistingController->UnPossess();
			}

			// Spawn the new AI controller
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AAIController* NewController = World->SpawnActor<AAIController>(AIControllerClass, OwnerPawn->GetActorLocation(), OwnerPawn->GetActorRotation(), SpawnParams);
			if (NewController)
			{
				NewController->Possess(OwnerPawn);
				UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] Spawned and possessed with AI controller: %s"), *NewController->GetName());
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[Recruitment] Failed to spawn AI controller for %s"), *OwnerPawn->GetName());
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] Pawn already has survivor controller: %s"), *ExistingController->GetName());
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] %s recruited successfully"), *GetOwner()->GetName());
}

void UMORecruitmentComponent::FailQuest()
{
	if (RecruitmentState != EMORecruitmentState::QuestActive)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] Quest failed for %s"), *GetOwner()->GetName());

	SetRecruitmentState(EMORecruitmentState::Dead);
	OnQuestFailed.Broadcast();

	// Notify spawn manager to convert to corpse
	UWorld* World = GetWorld();
	if (World)
	{
		UMOSpawnManagerSubsystem* SpawnManager = World->GetSubsystem<UMOSpawnManagerSubsystem>();
		if (SpawnManager)
		{
			SpawnManager->ConvertToCorpse(Cast<APawn>(GetOwner()));
		}
	}
}

// ============================================================================
// QUERIES
// ============================================================================

float UMORecruitmentComponent::GetQuestTimeRemaining() const
{
	if (RecruitmentState != EMORecruitmentState::QuestActive)
	{
		return 0.0f;
	}

	const FTimespan Elapsed = FDateTime::Now() - QuestStartTime;
	const float Remaining = ActiveQuest.TimeoutSeconds - Elapsed.GetTotalSeconds();
	return FMath::Max(0.0f, Remaining);
}

bool UMORecruitmentComponent::CanCompleteQuest(APawn* InteractingPawn) const
{
	if (ActiveQuest.QuestType == EMORecruitmentQuestType::None)
	{
		return true;  // No requirements
	}

	return CheckQuestRequirements(InteractingPawn, false);
}

FText UMORecruitmentComponent::GetQuestProgressText() const
{
	if (RecruitmentState != EMORecruitmentState::QuestActive)
	{
		return FText::GetEmpty();
	}

	const float TimeRemaining = GetQuestTimeRemaining();
	const int32 HoursRemaining = FMath::FloorToInt(TimeRemaining / 3600.0f);
	const int32 MinutesRemaining = FMath::FloorToInt(FMath::Fmod(TimeRemaining, 3600.0f) / 60.0f);

	FText QuestTypeName;
	switch (ActiveQuest.QuestType)
	{
	case EMORecruitmentQuestType::NeedsFood:
		QuestTypeName = NSLOCTEXT("MORecruitment", "NeedsFood", "Needs Food");
		break;
	case EMORecruitmentQuestType::NeedsMedical:
		QuestTypeName = NSLOCTEXT("MORecruitment", "NeedsMedical", "Needs Medical Aid");
		break;
	case EMORecruitmentQuestType::NeedsWarmth:
		QuestTypeName = NSLOCTEXT("MORecruitment", "NeedsWarmth", "Needs Warmth");
		break;
	case EMORecruitmentQuestType::NeedsProtection:
		QuestTypeName = NSLOCTEXT("MORecruitment", "NeedsProtection", "Needs Protection");
		break;
	default:
		QuestTypeName = NSLOCTEXT("MORecruitment", "UnknownQuest", "Unknown Quest");
		break;
	}

	return FText::Format(
		NSLOCTEXT("MORecruitment", "QuestProgress", "{0} - {1}h {2}m remaining"),
		QuestTypeName,
		FText::AsNumber(HoursRemaining),
		FText::AsNumber(MinutesRemaining));
}

// ============================================================================
// INTERNAL
// ============================================================================

void UMORecruitmentComponent::SetRecruitmentState(EMORecruitmentState NewState)
{
	if (RecruitmentState == NewState)
	{
		return;
	}

	const EMORecruitmentState OldState = RecruitmentState;
	RecruitmentState = NewState;

	OnRecruitmentStateChanged.Broadcast(OldState, NewState);

	UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] %s state changed: %d -> %d"),
		*GetOwner()->GetName(), (int32)OldState, (int32)NewState);
}

void UMORecruitmentComponent::RollRandomQuest()
{
	if (PossibleQuests.Num() == 0)
	{
		return;
	}

	const int32 Index = FMath::RandRange(0, PossibleQuests.Num() - 1);
	ActiveQuest = PossibleQuests[Index];

	UE_LOG(LogMOFramework, Verbose, TEXT("[Recruitment] Rolled quest type: %s"),
		*UEnum::GetValueAsString(ActiveQuest.QuestType));
}

void UMORecruitmentComponent::CheckQuestTimeout()
{
	const float TimeRemaining = GetQuestTimeRemaining();
	if (TimeRemaining <= 0.0f)
	{
		FailQuest();
	}
}

void UMORecruitmentComponent::NotifySpawnManagerOfInteraction()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMOSpawnManagerSubsystem* SpawnManager = World->GetSubsystem<UMOSpawnManagerSubsystem>();
	if (SpawnManager)
	{
		// Make this survivor persistent
		const float PersistenceHours = ActiveQuest.TimeoutSeconds / 3600.0f;
		SpawnManager->MarkEntityPersistent(Cast<APawn>(GetOwner()), PersistenceHours);
	}
}

bool UMORecruitmentComponent::CheckQuestRequirements(APawn* InteractingPawn, bool bConsumeItems) const
{
	if (!InteractingPawn)
	{
		return false;
	}

	UMOInventoryComponent* Inventory = InteractingPawn->FindComponentByClass<UMOInventoryComponent>();
	UMOSkillsComponent* Skills = InteractingPawn->FindComponentByClass<UMOSkillsComponent>();

	// Check item requirement
	if (!ActiveQuest.RequiredItemId.IsNone())
	{
		if (!Inventory)
		{
			return false;
		}

		if (!HasRequiredItem(Inventory))
		{
			return false;
		}

		if (bConsumeItems)
		{
			// Remove the item from inventory
			Inventory->RemoveItemByDefinitionId(ActiveQuest.RequiredItemId, ActiveQuest.RequiredItemQuantity);
		}
	}

	// Check skill requirement
	if (!ActiveQuest.RequiredSkillId.IsNone())
	{
		if (!HasRequiredSkill(Skills))
		{
			return false;
		}
	}

	return true;
}

bool UMORecruitmentComponent::HasRequiredItem(UMOInventoryComponent* Inventory) const
{
	if (!Inventory)
	{
		return false;
	}

	return Inventory->HasItem(ActiveQuest.RequiredItemId, ActiveQuest.RequiredItemQuantity);
}

bool UMORecruitmentComponent::HasRequiredSkill(UMOSkillsComponent* Skills) const
{
	if (!Skills)
	{
		return ActiveQuest.RequiredSkillLevel <= 0;
	}

	const int32 CurrentLevel = Skills->GetSkillLevel(ActiveQuest.RequiredSkillId);
	return CurrentLevel >= ActiveQuest.RequiredSkillLevel;
}

// ============================================================================
// SAVE/LOAD
// ============================================================================

void UMORecruitmentComponent::BuildSaveData(FMORecruitmentSaveData& OutSaveData) const
{
	OutSaveData.RecruitmentState = RecruitmentState;
	OutSaveData.ActiveQuest = ActiveQuest;
	OutSaveData.QuestStartTimeTicks = QuestStartTime.GetTicks();
	OutSaveData.bHasValidData = true;

	UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] BuildSaveData: State=%d, QuestType=%d, StartTime=%lld"),
		(int32)RecruitmentState, (int32)ActiveQuest.QuestType, OutSaveData.QuestStartTimeTicks);
}

bool UMORecruitmentComponent::ApplySaveDataAuthority(const FMORecruitmentSaveData& InSaveData)
{
	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[Recruitment] ApplySaveDataAuthority called without authority"));
		return false;
	}

	if (!InSaveData.bHasValidData)
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[Recruitment] ApplySaveDataAuthority: No valid data to apply"));
		return false;
	}

	const EMORecruitmentState OldState = RecruitmentState;
	RecruitmentState = InSaveData.RecruitmentState;
	ActiveQuest = InSaveData.ActiveQuest;
	QuestStartTime = FDateTime(InSaveData.QuestStartTimeTicks);

	UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] ApplySaveDataAuthority: State=%d, QuestType=%d, TimeRemaining=%.1f sec"),
		(int32)RecruitmentState, (int32)ActiveQuest.QuestType, GetQuestTimeRemaining());

	// If we loaded a Recruited state, ensure the AI controller is spawned
	// This replicates the behavior from ForceRecruit() but without changing state
	if (RecruitmentState == EMORecruitmentState::Recruited && OldState != EMORecruitmentState::Recruited)
	{
		EnsureSurvivorAIController();
	}

	return true;
}

void UMORecruitmentComponent::EnsureSurvivorAIController()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->HasAuthority())
	{
		return;
	}

	// Check if pawn already has a valid survivor controller
	AController* ExistingController = OwnerPawn->GetController();
	if (ExistingController && ExistingController->IsA<AMOSurvivorController>())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] Pawn already has survivor controller: %s"), *ExistingController->GetName());
		return;
	}

	// Get the AI controller class from the pawn (set in Blueprint or defaults)
	TSubclassOf<AController> AIControllerClass = OwnerPawn->AIControllerClass;

	// Ensure we use a survivor controller class - fallback if not set or wrong type
	if (!AIControllerClass || !AIControllerClass->IsChildOf(AMOSurvivorController::StaticClass()))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] Using default AMOSurvivorController (pawn AIControllerClass was %s)"),
			AIControllerClass ? *AIControllerClass->GetName() : TEXT("None"));
		AIControllerClass = AMOSurvivorController::StaticClass();
	}

	// Unpossess from any existing controller
	if (ExistingController)
	{
		ExistingController->UnPossess();
	}

	// Spawn the new AI controller
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAIController* NewController = World->SpawnActor<AAIController>(AIControllerClass, OwnerPawn->GetActorLocation(), OwnerPawn->GetActorRotation(), SpawnParams);
	if (NewController)
	{
		NewController->Possess(OwnerPawn);
		UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] Loaded recruited pawn - spawned AI controller: %s"), *NewController->GetName());
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[Recruitment] Failed to spawn AI controller for loaded recruited pawn %s"), *OwnerPawn->GetName());
	}
}
