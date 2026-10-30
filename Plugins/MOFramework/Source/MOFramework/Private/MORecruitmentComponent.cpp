#include "MORecruitmentComponent.h"
#include "MOColonyManagerSubsystem.h"
#include "MOQuestSubsystem.h"
#include "MOFramework.h"
#include "MOSpawnManagerSubsystem.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MOSurvivorController.h"
#include "MOBuildableActor.h"
#include "AIController.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

UMORecruitmentComponent::UMORecruitmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.0f;  // Check every second
	SetIsReplicatedByDefault(true);

	// Wes, Oct 29 2026: a dialogue when you meet a survivor ("Omg I thought I was going to be alone forever!" and variations; "I got a fire started
	// and that's as far as I could get."), tied to the recruitment quest ("If you can do better than me I'll join your village" -> the build-a-campfire
	// quest line, or, already built, "Ohh you already built one!?").
	GreetingLines = {
		NSLOCTEXT("MORecruitment", "Greeting1", "Oh my god, I thought I was going to be alone forever!"),
		NSLOCTEXT("MORecruitment", "Greeting2", "Another person! I was starting to think I was the last one left."),
		NSLOCTEXT("MORecruitment", "Greeting3", "You're real! I haven't seen a soul in days."),
		NSLOCTEXT("MORecruitment", "Greeting4", "Thank god, someone else made it out here!"),
	};
	CampLines = {
		NSLOCTEXT("MORecruitment", "Camp1", "I got a fire started and that's as far as I could get."),
		NSLOCTEXT("MORecruitment", "Camp2", "This fire is all I've managed. I don't really know what I'm doing out here."),
		NSLOCTEXT("MORecruitment", "Camp3", "I've kept this fire going, but that's about all I know how to do."),
	};
	JoinLine = NSLOCTEXT("MORecruitment", "JoinLine", "Mind if I come with you?");
	WaryLine = NSLOCTEXT("MORecruitment", "WaryLine", "I don't know your people yet. Let me get to know your village before I join it.");

	FMORecruitmentQuest Campfire;
	Campfire.QuestType = EMORecruitmentQuestType::NeedsBuilding;
	Campfire.RequiredRecipeId = TEXT("BuildCampfire");
	Campfire.PlayerQuestId = TEXT("Tutorial_BuildCampfire");
	Campfire.QuestDialogue = NSLOCTEXT("MORecruitment", "CampfireAsk", "If you can do better than me, I'll join your village. Show me a campfire you built.");
	Campfire.AlreadyMetDialogue = NSLOCTEXT("MORecruitment", "CampfireAlreadyMet", "Ohh, you already built one!? Then I'm coming with you.");
	Campfire.ReminderDialogue = NSLOCTEXT("MORecruitment", "CampfireReminder", "Got a campfire of your own going yet? Build one and I'll join your village.");
	Campfire.SuccessDialogue = NSLOCTEXT("MORecruitment", "CampfireSuccess", "You built one! Alright, I'm coming with you.");
	PossibleQuests.Add(Campfire);
}

void UMORecruitmentComponent::BeginPlay()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UMORecruitmentComponent_BeginPlay);   // spawn-cost breakdown in traces (-statnamedevents)
	Super::BeginPlay();

	DialogueSeed = FMath::Rand();

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
	// DEBUG (Recruitment, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[Recruitment] State replicated: %d"), (int32)RecruitmentState);
}

// ============================================================================
// API
// ============================================================================

bool UMORecruitmentComponent::BeginInteraction(APawn* InteractingPawn)
{
	return BeginInteractionDetailed(InteractingPawn).Succeeded();
}

FMORecruitmentResult UMORecruitmentComponent::BeginInteractionDetailed(APawn* InteractingPawn)
{
	const FMORecruitmentResult Result = ResolveInteraction(InteractingPawn);
	if (Result.Joined())
	{
		// The recruiting player's quests: "SurvivorRecruited" is the silent gate of Tutorial_Possession -- the possession menu is
		// taught when there is a second pawn to possess, not at spawn (it was the first hint a new player saw, Oct 15 2026).
		UMOQuestSubsystem::ReportForActor(InteractingPawn, EMOObjectiveType::Event, FName(TEXT("SurvivorRecruited")), 1);
	}
	return Result;
}

FMORecruitmentResult UMORecruitmentComponent::ResolveInteraction(APawn* InteractingPawn)
{
	FMORecruitmentResult Result;   // NotRecruitable until something says otherwise
	if (!InteractingPawn)
	{
		return Result;
	}

	// Check if this pawn can be recruited at all (e.g., creatures like deer cannot)
	if (!bIsRecruitable)
	{
		// DEBUG (Recruitment, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[Recruitment] %s is not recruitable"), *GetOwner()->GetName());
		return Result;
	}

	if (RecruitmentState == EMORecruitmentState::Recruited)
	{
		Result.Outcome = EMORecruitmentOutcome::AlreadyRecruited;
		return Result;
	}

	if (RecruitmentState == EMORecruitmentState::Dead)
	{
		Result.Outcome = EMORecruitmentOutcome::Unavailable;
		return Result;
	}

	// STANDING GATE (V2.3): once a colony exists, strangers only agree to join
	// after real shared time near settlers has built trust. Quests still start
	// (doing someone a favor is HOW you build standing), but the immediate
	// no-quest recruit path refuses below the threshold. ForceRecruit (dev
	// verb) bypasses by design.
	// The campfire ask wins the first villagers (Wes, Oct 29 2026); once the gate is active (RecruitGateMinRoster settlers) a stranger no longer
	// asks for a fire the village has long had -- "already built one" would let every survivor met join on the spot, and one is always near
	// (UMOSpawnManagerSubsystem::EnsureSurvivorNearEachPlayer) -- so a NeedsBuilding ask is dropped there and the standing gate decides.
	if (RecruitmentState == EMORecruitmentState::Wandering)
	{
		if (const UWorld* World = GetWorld())
		{
			if (UMOColonyManagerSubsystem* Colony = World->GetSubsystem<UMOColonyManagerSubsystem>())
			{
				APawn* OwnerPawn = Cast<APawn>(GetOwner());
				const UMOColonyManagerSubsystem::FStandingGate Gate = OwnerPawn ? Colony->EvaluateRecruitGate(OwnerPawn) : UMOColonyManagerSubsystem::FStandingGate();
				if (OwnerPawn && Gate.bGateActive && ActiveQuest.QuestType == EMORecruitmentQuestType::NeedsBuilding)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[Recruitment] %s: the colony is past its first settlers: no building ask, the standing gate decides"), *GetOwner()->GetName());
					ActiveQuest = FMORecruitmentQuest();
				}
				if (OwnerPawn && ActiveQuest.QuestType == EMORecruitmentQuestType::None)
				{
					if (Gate.bGateActive && !Gate.bPasses)
					{
						UE_LOG(LogMOFramework, Warning,
							TEXT("[Recruitment] %s declines to join: colony standing %.2f below %.2f (about %.1f more shared game hours)"),
							*GetOwner()->GetName(), Gate.Standing, Gate.Needed, Gate.HoursToPass);
						Result.Outcome = EMORecruitmentOutcome::DeclinedStanding;
						Result.Standing = Gate.Standing;
						Result.StandingNeeded = Gate.Needed;
						Result.HoursToPass = Gate.HoursToPass;
						return Result;
					}
				}
			}
		}
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

			// Met before it was asked (a campfire already stands): she joins on the spot (Wes: "if it's already done have the quest complete right
			// away with like 'Ohh you already built one!?'").
			if (TryCompleteQuest(InteractingPawn))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[Recruitment] %s: the ask (%s) was already met -> joined"), *GetOwner()->GetName(),
					*UEnum::GetValueAsString(ActiveQuest.QuestType));
				Result.Outcome = EMORecruitmentOutcome::QuestAlreadyMet;
				return Result;
			}

			UE_LOG(LogMOFramework, Warning, TEXT("[Recruitment] %s: quest started: %s (recipe %s), timeout: %.1f hours"), *GetOwner()->GetName(),
				*UEnum::GetValueAsString(ActiveQuest.QuestType), *ActiveQuest.RequiredRecipeId.ToString(),
				ActiveQuest.TimeoutSeconds / 3600.0f);

			Result.Outcome = EMORecruitmentOutcome::QuestStarted;
			return Result;
		}
		else
		{
			// No quest - immediate recruitment
			ForceRecruit();
			Result.Outcome = RecruitmentState == EMORecruitmentState::Recruited ? EMORecruitmentOutcome::Recruited : EMORecruitmentOutcome::NotRecruitable;
			return Result;
		}
	}

	// Already interacted/questing - try to complete
	if (RecruitmentState == EMORecruitmentState::QuestActive || RecruitmentState == EMORecruitmentState::Interacted)
	{
		Result.Outcome = TryCompleteQuest(InteractingPawn) ? EMORecruitmentOutcome::Recruited : EMORecruitmentOutcome::QuestPending;
		return Result;
	}

	return Result;
}

bool UMORecruitmentComponent::TryCompleteQuest(APawn* InteractingPawn)
{
	if (RecruitmentState != EMORecruitmentState::QuestActive && RecruitmentState != EMORecruitmentState::Interacted)
	{
		return false;
	}

	if (!CanCompleteQuest(InteractingPawn))
	{
		// DEBUG (Recruitment, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[Recruitment] Cannot complete quest - requirements not met"));
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

	// Its survivor AI -- unless a player is driving it (then it comes when they leave).
	EnsureSurvivorAIController();

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
	case EMORecruitmentQuestType::NeedsBuilding:
		QuestTypeName = NSLOCTEXT("MORecruitment", "NeedsBuilding", "Wants to see what you can build");
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

	// DEBUG (Recruitment, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[Recruitment] Rolled quest type: %s"),
	// 	*UEnum::GetValueAsString(ActiveQuest.QuestType));
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

	// Check building requirement (first: nothing is consumed if it fails)
	if (!ActiveQuest.RequiredRecipeId.IsNone() && !IsBuildingStanding(GetWorld(), ActiveQuest.RequiredRecipeId))
	{
		return false;
	}

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

bool UMORecruitmentComponent::IsBuildingStanding(UWorld* World, FName RecipeId)
{
	if (!World || RecipeId.IsNone())
	{
		return false;
	}
	for (TActorIterator<AMOBuildableActor> It(World); It; ++It)
	{
		if (It->GetRecipeId() == RecipeId && It->IsComplete() && !It->ActorHasTag(UMOSpawnManagerSubsystem::SurvivorBeaconTag))
		{
			return true;
		}
	}
	return false;
}

FText UMORecruitmentComponent::GetDialogueFor(EMORecruitmentOutcome Outcome) const
{
	auto Pick = [this](const TArray<FText>& Lines, int32 Salt) -> FText
	{
		return Lines.Num() > 0 ? Lines[static_cast<uint32>(DialogueSeed + Salt) % static_cast<uint32>(Lines.Num())] : FText::GetEmpty();
	};
	TArray<FText> Said;
	auto Say = [&Said](const FText& Line)
	{
		if (!Line.IsEmpty())
		{
			Said.Add(Line);
		}
	};

	switch (Outcome)
	{
	case EMORecruitmentOutcome::QuestStarted:
	case EMORecruitmentOutcome::QuestAlreadyMet:
		Say(Pick(GreetingLines, 0));
		Say(Pick(CampLines, 7));
		Say(ActiveQuest.QuestDialogue);
		if (Outcome == EMORecruitmentOutcome::QuestAlreadyMet)
		{
			Say(ActiveQuest.AlreadyMetDialogue);
		}
		break;
	case EMORecruitmentOutcome::QuestPending:
		Say(ActiveQuest.ReminderDialogue.IsEmpty() ? ActiveQuest.QuestDialogue : ActiveQuest.ReminderDialogue);
		break;
	case EMORecruitmentOutcome::Recruited:
		if (ActiveQuest.QuestType == EMORecruitmentQuestType::None)
		{
			Say(Pick(GreetingLines, 0));   // no ask: the first meeting is the joining
			Say(JoinLine);
		}
		else
		{
			Say(ActiveQuest.SuccessDialogue);
		}
		break;
	case EMORecruitmentOutcome::DeclinedStanding:
		Say(Pick(GreetingLines, 0));   // still glad to see someone; the notification gives the numbers
		Say(WaryLine);
		break;
	default:
		break;   // the rest is the notification's to explain
	}
	return FText::Join(FText::FromString(TEXT("\n")), Said);
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
		// DEBUG (Recruitment, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[Recruitment] ApplySaveDataAuthority: No valid data to apply"));
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

AMOSurvivorController* UMORecruitmentComponent::EnsureSurvivorAIController()
{
	UWorld* World = GetWorld();
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!World || !OwnerPawn || !OwnerPawn->HasAuthority())
	{
		return nullptr;
	}

	AController* ExistingController = OwnerPawn->GetController();
	if (AMOSurvivorController* Existing = Cast<AMOSurvivorController>(ExistingController))
	{
		return Existing;
	}
	if (ExistingController && ExistingController->IsPlayerController())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] %s is driven by player %s: no survivor AI while they play it"), *OwnerPawn->GetName(), *ExistingController->GetName());
		return nullptr;
	}

	// The pawn's own AI controller class when it is a survivor controller, else the default one.
	TSubclassOf<AController> AIControllerClass = OwnerPawn->AIControllerClass;
	if (!AIControllerClass || !AIControllerClass->IsChildOf(AMOSurvivorController::StaticClass()))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] Using default AMOSurvivorController (pawn AIControllerClass was %s)"),
			AIControllerClass ? *AIControllerClass->GetName() : TEXT("None"));
		AIControllerClass = AMOSurvivorController::StaticClass();
	}
	if (ExistingController)
	{
		ExistingController->UnPossess();   // an AI that is not a survivor controller (a creature's, a stale one)
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAIController* NewController = World->SpawnActor<AAIController>(AIControllerClass, OwnerPawn->GetActorLocation(), OwnerPawn->GetActorRotation(), SpawnParams);
	if (!NewController)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[Recruitment] Failed to spawn a survivor AI controller for %s"), *OwnerPawn->GetName());
		return nullptr;
	}
	NewController->Possess(OwnerPawn);
	UE_LOG(LogMOFramework, Log, TEXT("[Recruitment] %s: spawned and possessed by survivor AI %s"), *OwnerPawn->GetName(), *NewController->GetName());
	return Cast<AMOSurvivorController>(NewController);
}
