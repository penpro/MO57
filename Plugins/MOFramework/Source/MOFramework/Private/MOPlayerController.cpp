#include "MOPlayerController.h"
#include "MOFramework.h"
#include "MODevMode.h"
#include "HAL/IConsoleManager.h"
#include "MOUIDebugSubsystem.h"
#include "MOControllableInterface.h"
#include "MOSpectatorPawn.h"
#include "MOCharacter.h"
#include "GameFramework/PawnMovementComponent.h"
#include "MOUIManagerComponent.h"
#include "MOPossessionComponent.h"
#include "MOGameMode.h"
#include "MORecruitmentComponent.h"
#include "MONotificationComponent.h"
#include "MOBiomeFinder.h"
#include "MOSpawnClearance.h"
#include "MOWorldSeedSubsystem.h"
#include "MOWorldFeatureSubsystem.h"
#include "MOWorldFeatureSettings.h"
#include "TimerManager.h"
#include "MOBuildingComponent.h"
#include "MOBuildableActor.h"
#include "MOCarcassActor.h"
#include "MOInteractorComponent.h"
#include "MOIdentityComponent.h"
#include "MOIdentityRegistrySubsystem.h"
#include "MOKeyBindingManager.h"
#include "MOGameUIManagerSubsystem.h"
#include "MOTextInputDialog.h"
#include "MOQuickPickDialog.h"
#include "MOTimeOfDayDialog.h"
#include "MOWaterActions.h"
#include "MOItemActions.h"
#include "MOFireActions.h"
#include "MOWorldActionProvider.h"
#include "MOCraftingStationActor.h"
#include "MOWaterSourceSettings.h"
#include "MOWaterSubsystem.h"
#include "MOWeatherIntegrationSubsystem.h"
#include "MOGameClockSubsystem.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeLock.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"

// UI Controllers
#include "MOInventoryUIController.h"
#include "MOForagingSubsystem.h"
#include "MOGroundContextMenu.h"
#include "MOCraftingUIController.h"
#include "MOBuildingUIController.h"
#include "MOCharacterUIController.h"
#include "MOSystemMenuUIController.h"
#include "MOQuestUIController.h"

// Quest system (for tutorial event broadcast + F1/F3 skip)
#include "MOQuestSubsystem.h"

// Survivor command system
#include "MORecruitmentComponent.h"

AMOPlayerController::AMOPlayerController()
{
	// Create UI Manager component
	UIManagerComponent = CreateDefaultSubobject<UMOUIManagerComponent>(TEXT("UIManagerComponent"));

	// Create Possession component
	PossessionComponent = CreateDefaultSubobject<UMOPossessionComponent>(TEXT("PossessionComponent"));

	// Create Notification component
	NotificationComponent = CreateDefaultSubobject<UMONotificationComponent>(TEXT("NotificationComponent"));

	// Create Building component
	BuildingComponent = CreateDefaultSubobject<UMOBuildingComponent>(TEXT("BuildingComponent"));

	// Create UI Controller components (these are sibling components that UIManager delegates to)
	InventoryUIController = CreateDefaultSubobject<UMOInventoryUIController>(TEXT("InventoryUIController"));
	CraftingUIController = CreateDefaultSubobject<UMOCraftingUIController>(TEXT("CraftingUIController"));
	BuildingUIController = CreateDefaultSubobject<UMOBuildingUIController>(TEXT("BuildingUIController"));
	CharacterUIController = CreateDefaultSubobject<UMOCharacterUIController>(TEXT("CharacterUIController"));
	SystemMenuUIController = CreateDefaultSubobject<UMOSystemMenuUIController>(TEXT("SystemMenuUIController"));
	QuestUIController = CreateDefaultSubobject<UMOQuestUIController>(TEXT("QuestUIController"));
}

void AMOPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController::BeginPlay - Setting up input context"));

	// Swap the shared IMC assets for per-controller runtime copies BEFORE we add
	// or rebind anything, so runtime key rebinding never mutates the shared asset. (#142)
	EnsureRuntimeInputContexts();

	// NOTE: Do NOT call SetInputMode() here - CommonUI manages input modes
	// via GetDesiredInputConfig() on active widgets. The MOGameplayInputStub
	// (RootContentWidgetClass on layer stacks) provides the baseline Game input mode.

	// Setup default input context
	SetupDefaultInputContext();

	// Apply any saved custom key bindings
	FMOKeyBindingManager::ApplyAllBindingsFromSettings(this);

	// Notify UI manager subsystem that this player has been added
	// This creates the primary game layout widget with layer stacks
	if (UMOGameUIManagerSubsystem* UIManagerSubsystem = UMOGameUIManagerSubsystem::Get(this))
	{
		UIManagerSubsystem->NotifyPlayerAdded(this);
	}
}

void AMOPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// Apply pending spectator view if we now have a pawn
	// Note: MOSpectatorPawn handles this in its BeginPlay, but this is a fallback for other pawn types
	if (bHasPendingSpectatorView)
	{
		if (APawn* CurrentPawn = GetPawn())
		{
			// MOSpectatorPawn handles this itself in BeginPlay, so skip if it's that type
			if (!Cast<AMOSpectatorPawn>(CurrentPawn))
			{
				CurrentPawn->SetActorLocation(PendingSpectatorLocation);
				CurrentPawn->SetActorRotation(PendingSpectatorRotation);
				SetControlRotation(PendingSpectatorRotation);

				UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController::PlayerTick - Applied pending spectator view at %s"),
					*PendingSpectatorLocation.ToString());

				if (!bSpectatorInputEnabled)
				{
					if (UPawnMovementComponent* MovementComp = CurrentPawn->GetMovementComponent())
					{
						MovementComp->Deactivate();
					}
				}

				bHasPendingSpectatorView = false;
			}
		}
	}

	// Enforce spectator input blocking - ensure movement stays disabled
	// This handles cases where the spectator pawn is spawned after we set the flag
	if (!bSpectatorInputEnabled && !CachedControllablePawn.IsValid())
	{
		if (APawn* CurrentPawn = GetPawn())
		{
			// For MOSpectatorPawn, just ensure it's locked
			if (AMOSpectatorPawn* MOSpectator = Cast<AMOSpectatorPawn>(CurrentPawn))
			{
				if (!MOSpectator->IsMovementLocked())
				{
					MOSpectator->SetMovementLocked(true);
				}
			}
			else
			{
				// Fallback: deactivate movement component
				if (UPawnMovementComponent* MovementComp = CurrentPawn->GetMovementComponent())
				{
					if (MovementComp->IsActive())
					{
						MovementComp->Deactivate();
						UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController::PlayerTick - Deactivated spectator movement"));
					}
				}
			}
		}
	}
}

void AMOPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController::SetupInputComponent called"));

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: InputComponent is not UEnhancedInputComponent!"));
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: EnhancedInputComponent is valid"));

	// ============================================================================
	// MOVEMENT BINDINGS
	// ============================================================================

	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMOPlayerController::HandleMove);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound MoveAction"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: MoveAction is NOT set!"));
	}

	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMOPlayerController::HandleLook);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound LookAction"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: LookAction is NOT set!"));
	}

	if (JumpAction)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleJumpStart);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &AMOPlayerController::HandleJumpEnd);
	}

	if (HustleAction)
	{
		EnhancedInput->BindAction(HustleAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleHustleStart);
		EnhancedInput->BindAction(HustleAction, ETriggerEvent::Triggered, this, &AMOPlayerController::HandleHustleTriggered);
		EnhancedInput->BindAction(HustleAction, ETriggerEvent::Completed, this, &AMOPlayerController::HandleHustleEnd);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound HustleAction (tap=jog, hold=sprint)"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: HustleAction is NOT set!"));
	}

	if (CrouchAction)
	{
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleCrouch);
	}

	// ============================================================================
	// ACTION BINDINGS
	// ============================================================================

	if (InteractAction)
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleInteract);
	}

	if (PrimaryAction)
	{
		EnhancedInput->BindAction(PrimaryAction, ETriggerEvent::Started, this, &AMOPlayerController::HandlePrimaryAction);
		EnhancedInput->BindAction(PrimaryAction, ETriggerEvent::Completed, this, &AMOPlayerController::HandlePrimaryActionRelease);
	}

	if (SecondaryAction)
	{
		EnhancedInput->BindAction(SecondaryAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleSecondaryAction);
		EnhancedInput->BindAction(SecondaryAction, ETriggerEvent::Completed, this, &AMOPlayerController::HandleSecondaryActionRelease);
	}

	// ============================================================================
	// UI/SYSTEM BINDINGS
	// ============================================================================

	if (InventoryAction)
	{
		EnhancedInput->BindAction(InventoryAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleInventory);
	}

	if (CraftAction)
	{
		EnhancedInput->BindAction(CraftAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleCraft);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound CraftAction"));
	}

	if (PlayerStatusAction)
	{
		EnhancedInput->BindAction(PlayerStatusAction, ETriggerEvent::Started, this, &AMOPlayerController::HandlePlayerStatus);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound PlayerStatusAction"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: PlayerStatusAction is NOT set!"));
	}

	if (SkillsAction)
	{
		EnhancedInput->BindAction(SkillsAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleSkills);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound SkillsAction"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: SkillsAction is NOT set!"));
	}

	if (JournalAction)
	{
		EnhancedInput->BindAction(JournalAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleJournal);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound JournalAction"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: JournalAction is NOT set!"));
	}

	if (BuildAction)
	{
		EnhancedInput->BindAction(BuildAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleBuild);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound BuildAction"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: BuildAction is NOT set!"));
	}

	if (BackMenuAction)
	{
		EnhancedInput->BindAction(BackMenuAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleBackMenu);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound BackMenuAction"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: BackMenuAction is NOT set — Tab/menu input will not work. Assign IA_Back_Menu on BP defaults."));
	}

	if (PossessAction)
	{
		EnhancedInput->BindAction(PossessAction, ETriggerEvent::Started, this, &AMOPlayerController::HandlePossess);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound PossessAction"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: PossessAction is NOT set!"));
	}

	// ============================================================================
	// BUILDING BINDINGS
	// ============================================================================

	if (PlaceBuildingAction)
	{
		EnhancedInput->BindAction(PlaceBuildingAction, ETriggerEvent::Started, this, &AMOPlayerController::HandlePlaceBuilding);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound PlaceBuildingAction"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: PlaceBuildingAction is NOT set! Building placement will not work."));
	}

	if (RotateBuildingCWAction)
	{
		EnhancedInput->BindAction(RotateBuildingCWAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleRotateBuildingCW);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound RotateBuildingCWAction"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: RotateBuildingCWAction is NOT set!"));
	}

	if (RotateBuildingCCWAction)
	{
		EnhancedInput->BindAction(RotateBuildingCCWAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleRotateBuildingCCW);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound RotateBuildingCCWAction"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: RotateBuildingCCWAction is NOT set!"));
	}

	if (FlipPlacementYawAction)
	{
		EnhancedInput->BindAction(FlipPlacementYawAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleFlipPlacementYaw);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound FlipPlacementYawAction"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: FlipPlacementYawAction is NOT set!"));
	}

	if (CancelPlacementAction)
	{
		EnhancedInput->BindAction(CancelPlacementAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleCancelPlacement);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound CancelPlacementAction"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: CancelPlacementAction is NOT set!"));
	}

	// ============================================================================
	// TERRAFORMING BINDINGS
	// ============================================================================

	if (TerraformToggleAction)
	{
		EnhancedInput->BindAction(TerraformToggleAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleTerraformToggle);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound TerraformToggleAction"));
	}

	if (TerraformCycleToolAction)
	{
		EnhancedInput->BindAction(TerraformCycleToolAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleTerraformCycleTool);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound TerraformCycleToolAction"));
	}

	// ============================================================================
	// CAMERA BINDINGS
	// ============================================================================

	if (ViewAction)
	{
		EnhancedInput->BindAction(ViewAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleView);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound ViewAction (camera shoulder toggle)"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: ViewAction not set - camera shoulder toggle disabled. Set it in BP_MOPlayerController."));
	}

	// ========================================================================
	// TUTORIAL CONTROLS (F1 skip current popup, F3 disable all popups)
	// ========================================================================
	if (SkipTutorialItemAction)
	{
		EnhancedInput->BindAction(SkipTutorialItemAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleSkipTutorialItem);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound SkipTutorialItemAction"));
	}
	if (SkipAllTutorialsAction)
	{
		EnhancedInput->BindAction(SkipAllTutorialsAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleSkipAllTutorials);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound SkipAllTutorialsAction"));
	}
}

void AMOPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	CachePossessedPawn(InPawn);
}

void AMOPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	// A remote client never runs OnPossess/OnUnPossess (Possess() is a server call); it learns its pawn through
	// replication, which lands here. Without this, CachedControllablePawn -- the target of every input handler -- and
	// the UI manager's pawn components stay empty on the client: it sees its pawn but cannot control it.
	if (!HasAuthority() && IsLocalController())
	{
		if (InPawn)
		{
			CachePossessedPawn(InPawn);
		}
		else
		{
			ClearPossessedPawnCache();
		}
	}
}

void AMOPlayerController::CachePossessedPawn(APawn* InPawn)
{
	// Cache the pawn if it implements the controllable interface
	if (InPawn && InPawn->Implements<UMOControllableInterface>())
	{
		CachedControllablePawn = InPawn;
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Possessed %s - Implements IMOControllableInterface: YES"),
			*InPawn->GetName());

		// Track this pawn's GUID as the last possessed (for save/load)
		if (UMOIdentityComponent* IdentityComp = InPawn->FindComponentByClass<UMOIdentityComponent>())
		{
			LastPossessedPawnGuid = IdentityComp->GetGuid();
			UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Tracking last possessed pawn GUID: %s"),
				*LastPossessedPawnGuid.ToString());
		}
	}
	else
	{
		CachedControllablePawn.Reset();
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: Possessed %s - Implements IMOControllableInterface: NO (inputs will not work!)"),
			InPawn ? *InPawn->GetName() : TEXT("None"));

		if (InPawn)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("  Pawn class: %s - Make sure it inherits from AMOCharacter or implements IMOControllableInterface"),
				*InPawn->GetClass()->GetName());
		}
	}

	// Cache pawn components in UI manager for efficient access
	if (UIManagerComponent)
	{
		UIManagerComponent->CachePawnComponents(InPawn);
	}

	// When we possess a pawn, we no longer need spectator input - the pawn handles input
	bSpectatorInputEnabled = false;
}

void AMOPlayerController::OnUnPossess()
{
	ClearPossessedPawnCache();
	Super::OnUnPossess();
}

void AMOPlayerController::ClearPossessedPawnCache()
{
	// Clear cached pawn components before unpossessing
	if (UIManagerComponent)
	{
		UIManagerComponent->ClearCachedPawnComponents();
	}

	CachedControllablePawn.Reset();

	// Disable spectator input when unpossessing - camera stays fixed until a pawn is possessed
	bSpectatorInputEnabled = false;
	UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Unpossessed - spectator input disabled"));
}

void AMOPlayerController::ServerPossessPawnByGuid_Implementation(FGuid PawnGuid)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOPlayerController] %s asked to possess pawn %s"), *GetName(), *PawnGuid.ToString());
	if (PossessionComponent)
	{
		PossessionComponent->ApplyPossessByGuid(PawnGuid);
	}
}

void AMOPlayerController::ServerRequestRespawn_Implementation()
{
	const AMOCharacter* Current = Cast<AMOCharacter>(GetPawn());
	if (Current && !Current->IsDead())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPlayerController] ServerRequestRespawn refused: %s is still alive"), *Current->GetName());
		return;
	}
	if (AMOGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOGameMode>() : nullptr)
	{
		GameMode->RespawnPlayerAfterDeath(this);
	}
}

void AMOPlayerController::ServerCreateCharacter_Implementation()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOPlayerController] %s asked to create a character"), *GetName());
	if (PossessionComponent)
	{
		PossessionComponent->ApplyCreateCharacter();
	}
}

void AMOPlayerController::ServerRequestPossessionList_Implementation()
{
	if (PossessionComponent)
	{
		TArray<FMOPossessionListEntry> Entries;
		PossessionComponent->BuildList(Entries);
		ClientReceivePossessionList(Entries);
	}
}

void AMOPlayerController::ClientReceivePossessionList_Implementation(const TArray<FMOPossessionListEntry>& Entries)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOPlayerController] received %d possession list entr(y/ies) from the server"), Entries.Num());
	for (const FMOPossessionListEntry& Entry : Entries)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOPlayerController]   possession row '%s' %s%s"), *Entry.CharacterName,
			Entry.Occupancy == EMOPawnOccupancy::You ? TEXT("you") : Entry.Occupancy == EMOPawnOccupancy::OtherPlayer ? TEXT("player=") : TEXT("free"),
			Entry.Occupancy == EMOPawnOccupancy::OtherPlayer ? *Entry.OccupantName : TEXT(""));
	}
	if (PossessionComponent)
	{
		PossessionComponent->NotifyPossessionListReceived(Entries);
	}
}

void AMOPlayerController::ClientShowNotification_Implementation(const FText& Message, float Duration, EMONotificationType Type)
{
	if (NotificationComponent)
	{
		NotificationComponent->ShowNotification(Message, Duration, Type);
	}
}

void AMOPlayerController::ServerForage_Implementation(uint8 Action, FVector_NetQuantize Location)
{
	const EMOGroundForageAction Kind = static_cast<EMOGroundForageAction>(Action);
	APawn* ForagingPawn = GetPawn();
	UWorld* World = GetWorld();
	UMOForagingSubsystem* Foraging = World ? World->GetSubsystem<UMOForagingSubsystem>() : nullptr;
	if (!ForagingPawn || !Foraging || (Kind != EMOGroundForageAction::SearchNearby && Kind != EMOGroundForageAction::DigForSupplies))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOForaging] %s: forage refused (no pawn, no foraging subsystem or unknown action %d)"), *GetName(), Action);
		return;
	}
	const float Distance = FVector::Dist(ForagingPawn->GetActorLocation(), Location);
	if (Distance > MaxForageReachCm)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOForaging] %s: forage refused, target %.0f cm from the pawn (max %.0f)"), *GetName(), Distance, MaxForageReachCm);
		ClientShowNotification(NSLOCTEXT("MO", "ForageTooFar", "Too far away to forage there"), 2.0f, EMONotificationType::Warning);
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (LastForageServerTime >= 0.0 && Now - LastForageServerTime < MinForageIntervalSeconds)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOForaging] %s: forage refused, %.2f s after the last one (min %.1f)"), *GetName(), Now - LastForageServerTime, MinForageIntervalSeconds);
		return;
	}
	LastForageServerTime = Now;

	// Skill level and radius from the server's own view of the pawn (never the client's numbers).
	const int32 Level = Foraging->GetForagingLevel(ForagingPawn);
	const int32 Count = Kind == EMOGroundForageAction::SearchNearby
		? Foraging->RevealHISMInstancesInRadius(Location, Foraging->CalculateSearchRadius(Level), ForagingPawn).Num()
		: Foraging->DigForSupplies(Location, Level, ForagingPawn).Num();
	UE_LOG(LogMOFramework, Log, TEXT("[MOForaging] %s: %s at (%.0f, %.0f, %.0f), level %d -> %d item(s)"), *GetName(),
		Kind == EMOGroundForageAction::SearchNearby ? TEXT("search") : TEXT("dig"), Location.X, Location.Y, Location.Z, Level, Count);
	ClientForageResult(Action, Count);
}

void AMOPlayerController::ClientReportQuestEvent_Implementation(uint8 Type, FName TargetId, int32 Count)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMOQuestSubsystem* Quests = GI->GetSubsystem<UMOQuestSubsystem>())
		{
			Quests->ApplyReportedEvent(static_cast<EMOObjectiveType>(Type), TargetId, Count);
		}
	}
}

void AMOPlayerController::ClientOpenCharacterCreator_Implementation()
{
	if (SystemMenuUIController)
	{
		SystemMenuUIController->ShowCharacterCreator();
	}
}

void AMOPlayerController::ClientForageResult_Implementation(uint8 Action, int32 ItemCount)
{
	if (UMOInventoryUIController* UI = FindComponentByClass<UMOInventoryUIController>())
	{
		UI->ShowForagingResult(Action, ItemCount);
	}
}

void AMOPlayerController::RequestSetPawnIdentity(FGuid PawnGuid, FName Field, const FString& Value)
{
	if (HasAuthority())
	{
		ServerSetPawnIdentity_Implementation(PawnGuid, Field, Value);
	}
	else
	{
		ServerSetPawnIdentity(PawnGuid, Field, Value);
	}
}

void AMOPlayerController::ServerSetPawnIdentity_Implementation(FGuid PawnGuid, FName Field, const FString& Value)
{
	if (!HasAuthority())
	{
		return;   // only the server changes an identity (editor Python runs a Server RPC's body locally on a client)
	}
	const UMOIdentityRegistrySubsystem* Registry = GetWorld() ? GetWorld()->GetSubsystem<UMOIdentityRegistrySubsystem>() : nullptr;
	APawn* Target = Registry ? Cast<APawn>(Registry->ResolveActorOrNull(PawnGuid)) : nullptr;
	UMOIdentityComponent* Identity = Target ? Target->FindComponentByClass<UMOIdentityComponent>() : nullptr;
	const UMORecruitmentComponent* Recruit = Target ? Target->FindComponentByClass<UMORecruitmentComponent>() : nullptr;
	// This player's own pawn, or a colonist nobody else is playing -- not a stranger, a creature, or another player's pawn by GUID guess.
	const AController* Driver = Target ? Target->GetController() : nullptr;
	const bool bOtherPlayersPawn = Driver && Driver != this && Driver->IsPlayerController();
	if (!Identity || (Target != GetPawn() && (!(Recruit && Recruit->IsPossessable()) || bOtherPlayersPawn)))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPlayerController] %s: refused identity change of %s (%s)"), *GetName(), *PawnGuid.ToString(),
			!Identity ? TEXT("no such pawn") : bOtherPlayersPawn ? TEXT("another player's pawn") : TEXT("not this player's pawn or a colonist"));
		return;
	}
	const FString Clean = Value.TrimStartAndEnd().Left(MaxIdentityTextLength);
	if (Field == TEXT("CharacterName") && !Clean.IsEmpty())
	{
		Identity->SetDisplayName(FText::FromString(Clean));
	}
	else if (Field == TEXT("Gender"))
	{
		Identity->SetGender(Clean);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPlayerController] %s: refused identity change '%s' = '%s'"), *GetName(), *Field.ToString(), *Clean);
		return;
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOPlayerController] %s set %s of %s to '%s'"), *GetName(), *Field.ToString(), *Target->GetName(), *Clean);
}

void AMOPlayerController::PawnLeavingGame()
{
	// Called when this controller is destroyed on the server because its player left. The engine default DESTROYS the
	// pawn. A recruited survivor is a colony member, not a disposable avatar: it stays in the world (AMOCharacter hands
	// it to an AI controller when unpossessed) so the same player -- or another one -- can take it later, instead of
	// every join spawning a fresh pawn and every leave deleting one.
	if (APawn* Leaving = GetPawn())
	{
		const UMORecruitmentComponent* Recruit = Leaving->FindComponentByClass<UMORecruitmentComponent>();
		if (Recruit && Recruit->IsPossessable())
		{
			if (UWorld* World = GetWorld())
			{
				if (AMOGameMode* GameMode = World->GetAuthGameMode<AMOGameMode>())
				{
					GameMode->NotePlayerLeavingWithPawn(this, Leaving);
				}
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPlayerController] %s left: pawn %s stays in the world as an idle colonist"),
				*GetName(), *Leaving->GetName());
			UnPossess();
			return;
		}
	}
	Super::PawnLeavingGame();
}

// ============================================================================
// PAUSE POLICY ENFORCEMENT
// ============================================================================
//
// MO57 is pure real-time. The vision (survival sim + Steam co-op à la
// Satisfactory) doesn't permit pausing — world time, AI, vitals, weather,
// hunger all keep ticking while menus are open. See Docs/PAUSE_POLICY.md.
//
// APlayerController::SetPause is the single chokepoint every pause path
// in UE funnels through (UGameplayStatics::SetGamePaused, AGameMode::SetPause,
// BP "Set Game Paused" node, console "pause" command, gamepad-disconnect
// auto-pause behavior). Returning false means the world stays unpaused
// regardless of caller — and the warning log surfaces unintended attempts
// so we can track them down rather than silently letting a future
// regression slip in.

bool AMOPlayerController::SetPause(bool bPause, FCanUnpause CanUnpauseDelegate)
{
	if (bPause)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOPlayerController] SetPause(true) refused — MO57 is real-time. "
			     "If you need this, check Docs/PAUSE_POLICY.md first. Caller stack:"));
		// PrintScriptCallstack in shipping is a no-op; in dev it surfaces the offender.
		FFrame::KismetExecutionMessage(
			TEXT("Pause attempt blocked by MO57 pause policy"),
			ELogVerbosity::Warning);
	}

	// Always report failure. Returning Super::SetPause(false, ...) on the
	// unpause side would be fine too, but consistency (always-false) is
	// simpler and ensures no caller mistakenly thinks they paused.
	return false;
}

// ============================================================================
// CONTEXT MANAGEMENT
// ============================================================================

void AMOPlayerController::SetInputContext(EMOInputContext NewContext, bool bRemoveOthers)
{
	if (bRemoveOthers)
	{
		// Remove existing non-system contexts
		if (CurrentInputContext != NewContext)
		{
			RemoveInputContext(CurrentInputContext);
		}
	}

	AddInputContext(NewContext);
	CurrentInputContext = NewContext;
}

void AMOPlayerController::AddInputContext(EMOInputContext Context)
{
	// Defensive: guarantee we operate on the per-controller runtime copies even if
	// a context switch ever happens before BeginPlay. Idempotent. (#142)
	EnsureRuntimeInputContexts();

	UInputMappingContext* MappingContext = nullptr;
	int32 Priority = 0;

	switch (Context)
	{
	case EMOInputContext::PawnControl:
		MappingContext = PawnControlContext;
		Priority = PawnControlContextPriority;
		break;

	case EMOInputContext::BaseBuilding:
		MappingContext = BaseBuildingContext;
		Priority = BaseBuildingContextPriority;
		break;

	case EMOInputContext::Menu:
		MappingContext = MenuContext;
		Priority = MenuContextPriority;
		break;

	default:
		return;
	}

	if (MappingContext)
	{
		AddMappingContext(MappingContext, Priority);
	}
}

void AMOPlayerController::RemoveInputContext(EMOInputContext Context)
{
	UInputMappingContext* MappingContext = nullptr;

	switch (Context)
	{
	case EMOInputContext::PawnControl:
		MappingContext = PawnControlContext;
		break;

	case EMOInputContext::BaseBuilding:
		MappingContext = BaseBuildingContext;
		break;

	case EMOInputContext::Menu:
		MappingContext = MenuContext;
		break;

	default:
		return;
	}

	if (MappingContext)
	{
		RemoveMappingContext(MappingContext);
	}
}

bool AMOPlayerController::IsInputContextActive(EMOInputContext Context) const
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	UInputMappingContext* MappingContext = nullptr;

	switch (Context)
	{
	case EMOInputContext::PawnControl:
		MappingContext = PawnControlContext;
		break;

	case EMOInputContext::BaseBuilding:
		MappingContext = BaseBuildingContext;
		break;

	case EMOInputContext::Menu:
		MappingContext = MenuContext;
		break;

	default:
		return false;
	}

	return MappingContext && Subsystem->HasMappingContext(MappingContext);
}

void AMOPlayerController::SetupDefaultInputContext()
{
	// Add default pawn control context
	if (PawnControlContext)
	{
		AddMappingContext(PawnControlContext, PawnControlContextPriority);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Added PawnControlContext mapping context"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: PawnControlContext is NOT set! No input mapping context active."));
	}
}

void AMOPlayerController::EnsureRuntimeInputContexts()
{
	// Runtime key rebinding (FMOKeyBindingManager::ApplyBinding) edits the mapping
	// context in place via MapKey/UnmapKey. Those edits must never touch the shared
	// IMC *asset*: it's referenced by every controller and, in-editor, is the on-disk
	// asset, so mutating it leaks rebinds across players/sessions and dirties the asset.
	// Duplicate each assigned context into a per-controller transient copy and point our
	// field at it; all downstream add/remove/rebind code then works on the copy.
	auto MakeRuntimeCopy = [this](TObjectPtr<UInputMappingContext>& Field, const TCHAR* DebugName)
	{
		if (Field && !Field->HasAnyFlags(RF_Transient))
		{
			if (UInputMappingContext* RuntimeCopy = DuplicateObject<UInputMappingContext>(Field, this))
			{
				RuntimeCopy->SetFlags(RF_Transient);
				Field = RuntimeCopy;
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOPlayerController] Failed to duplicate %s for runtime rebinding; rebinds would fall back to mutating the shared asset"), DebugName);
			}
		}
	};

	MakeRuntimeCopy(PawnControlContext, TEXT("PawnControlContext"));
	MakeRuntimeCopy(BaseBuildingContext, TEXT("BaseBuildingContext"));
	MakeRuntimeCopy(MenuContext, TEXT("MenuContext"));
}

// ============================================================================
// SPECTATOR / CAMERA CONTROL
// ============================================================================

void AMOPlayerController::SetSpectatorViewAboveLocation(FVector TargetLocation, float Height, float PitchAngle)
{
	// Calculate camera position above the target
	FVector CameraLocation = TargetLocation + FVector(0.0f, 0.0f, Height);

	// Calculate rotation looking down at target
	FRotator CameraRotation = FRotator(PitchAngle, 0.0f, 0.0f);

	// Store for pending application (in case spectator pawn doesn't exist yet)
	PendingSpectatorLocation = CameraLocation;
	PendingSpectatorRotation = CameraRotation;
	bHasPendingSpectatorView = true;

	// Set the controller's view rotation
	SetControlRotation(CameraRotation);

	// If we have a pawn, apply immediately
	if (APawn* CurrentPawn = GetPawn())
	{
		// Prefer using MOSpectatorPawn's method
		if (AMOSpectatorPawn* MOSpectator = Cast<AMOSpectatorPawn>(CurrentPawn))
		{
			MOSpectator->SetViewAboveLocation(TargetLocation, Height, PitchAngle);
			bHasPendingSpectatorView = false;
			UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: Applied spectator view via MOSpectatorPawn at %s"),
				*CameraLocation.ToString());
		}
		else
		{
			CurrentPawn->SetActorLocation(CameraLocation);
			CurrentPawn->SetActorRotation(CameraRotation);
			bHasPendingSpectatorView = false;
			UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: Applied spectator view to generic pawn at %s"),
				*CameraLocation.ToString());
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: No pawn yet, storing pending spectator view at %s"),
			*CameraLocation.ToString());
	}
}

void AMOPlayerController::SetSpectatorInputEnabled(bool bEnabled)
{
	bSpectatorInputEnabled = bEnabled;

	// Also disable/enable the spectator pawn's movement if one exists
	if (APawn* CurrentPawn = GetPawn())
	{
		// Prefer MOSpectatorPawn's lock system
		if (AMOSpectatorPawn* MOSpectator = Cast<AMOSpectatorPawn>(CurrentPawn))
		{
			MOSpectator->SetMovementLocked(!bEnabled);
		}
		else
		{
			// Fallback for non-MOSpectatorPawn
			if (UPawnMovementComponent* MovementComp = CurrentPawn->GetMovementComponent())
			{
				if (bEnabled)
				{
					MovementComp->Activate();
				}
				else
				{
					MovementComp->Deactivate();
				}
			}
		}
	}

	UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController: Spectator input %s"),
		bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
}

// ============================================================================
// INPUT HANDLERS - MOVEMENT
// ============================================================================

void AMOPlayerController::HandleMove(const FInputActionValue& Value)
{
	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		const FVector2D MovementVector = Value.Get<FVector2D>();
		IMOControllableInterface::Execute_RequestMove(ControllablePawn, MovementVector);

		// Fire on every tick the input is active. The quest subsystem discards
		// the event cheaply if no objective is listening. A one-shot flag here
		// would race the tutorial chain — if the player looks/moves BEFORE the
		// matching tutorial quest activates, the flag would latch and we'd
		// never re-fire when that quest later starts.
		if (!MovementVector.IsNearlyZero())
		{
			BroadcastTutorialEvent(TEXT("PlayerMoved"));
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController::HandleMove - No CachedControllablePawn! Current Pawn: %s"),
			GetPawn() ? *GetPawn()->GetName() : TEXT("None"));
	}
}

void AMOPlayerController::HandleLook(const FInputActionValue& Value)
{
	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		const FVector2D LookVector = Value.Get<FVector2D>();
		IMOControllableInterface::Execute_RequestLook(ControllablePawn, LookVector);

		// Same reasoning as HandleMove — fire every tick, let the quest
		// subsystem be the matcher.
		if (!LookVector.IsNearlyZero())
		{
			BroadcastTutorialEvent(TEXT("PlayerLooked"));
		}
	}
}

void AMOPlayerController::HandleJumpStart(const FInputActionValue& Value)
{
	// Don't process jump input when a menu is open
	if (UIManagerComponent && UIManagerComponent->IsAnyMenuOpen())
	{
		return;
	}

	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestJumpStart(ControllablePawn);
		BroadcastTutorialEvent(TEXT("PlayerJumped"));
	}
}

void AMOPlayerController::HandleJumpEnd(const FInputActionValue& Value)
{
	// Don't process jump input when a menu is open
	if (UIManagerComponent && UIManagerComponent->IsAnyMenuOpen())
	{
		return;
	}

	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestJumpEnd(ControllablePawn);
	}
}

void AMOPlayerController::HandleHustleStart(const FInputActionValue& Value)
{
	// Don't process movement input when a menu is open
	if (UIManagerComponent && UIManagerComponent->IsAnyMenuOpen())
	{
		return;
	}

	// Record when hustle was pressed for tap/hold detection
	HustlePressTime = GetWorld()->GetTimeSeconds();
	bHustleHoldTriggered = false;
}

void AMOPlayerController::HandleHustleTriggered(const FInputActionValue& Value)
{
	// Don't process movement input when a menu is open
	if (UIManagerComponent && UIManagerComponent->IsAnyMenuOpen())
	{
		return;
	}

	// Check if we've held long enough to trigger sprint
	if (!bHustleHoldTriggered)
	{
		const float HeldTime = GetWorld()->GetTimeSeconds() - HustlePressTime;
		if (HeldTime >= HustleHoldThreshold)
		{
			// Start sprinting
			bHustleHoldTriggered = true;
			if (APawn* ControllablePawn = CachedControllablePawn.Get())
			{
				IMOControllableInterface::Execute_RequestSprintStart(ControllablePawn);
				BroadcastTutorialEvent(TEXT("PlayerSprinted"));
			}
		}
	}
}

void AMOPlayerController::HandleHustleEnd(const FInputActionValue& Value)
{
	// Don't process movement input when a menu is open
	if (UIManagerComponent && UIManagerComponent->IsAnyMenuOpen())
	{
		bHustleHoldTriggered = false;
		return;
	}

	if (bHustleHoldTriggered)
	{
		// Was a hold - stop sprinting
		if (APawn* ControllablePawn = CachedControllablePawn.Get())
		{
			IMOControllableInterface::Execute_RequestSprintEnd(ControllablePawn);
		}
	}
	else
	{
		// Was a tap - toggle jog
		if (APawn* ControllablePawn = CachedControllablePawn.Get())
		{
			IMOControllableInterface::Execute_RequestToggleJog(ControllablePawn);
			BroadcastTutorialEvent(TEXT("PlayerJogged"));
		}
	}

	bHustleHoldTriggered = false;
}

void AMOPlayerController::HandleCrouch(const FInputActionValue& Value)
{
	// Don't process movement input when a menu is open
	if (UIManagerComponent && UIManagerComponent->IsAnyMenuOpen())
	{
		return;
	}

	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestCrouchToggle(ControllablePawn);
		BroadcastTutorialEvent(TEXT("PlayerCrouched"));
	}
}

// ============================================================================
// INPUT HANDLERS - ACTIONS
// ============================================================================

void AMOPlayerController::HandleInteract(const FInputActionValue& Value)
{
	// Don't process gameplay input when a menu is open
	if (UIManagerComponent && UIManagerComponent->IsAnyMenuOpen())
	{
		return;
	}

	// While placing a building, E rotates the ghost (RotateBuildingCWAction shares the key): it must not also interact with whatever is
	// behind the ghost.
	if (BuildingComponent && BuildingComponent->IsInPlacementMode())
	{
		return;
	}

	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		// Nothing with an interaction of its own under the crosshair (bare ground, grass, world cover): forage there, as right-click does.
		UMOInteractorComponent* Interactor = ControllablePawn->FindComponentByClass<UMOInteractorComponent>();
		FMOInteractionTarget Target;
		// Water before forage: a lake bed's grass is a forage target, but what the crosshair meets first is the water.
		if (Interactor && (!Interactor->FindInteractionTarget(Target) || Target.bIsForageTarget) && (TryDrinkAtCrosshair() || TryOpenForageMenuAtCrosshair()))
		{
			return;
		}
		IMOControllableInterface::Execute_RequestInteract(ControllablePawn);
	}
}

void AMOPlayerController::HandlePrimaryAction(const FInputActionValue& Value)
{
	// Don't process gameplay input when a menu is open
	if (UIManagerComponent && UIManagerComponent->IsAnyMenuOpen())
	{
		return;
	}

	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestPrimaryAction(ControllablePawn);
	}
}

void AMOPlayerController::HandlePrimaryActionRelease(const FInputActionValue& Value)
{
	// Don't process gameplay input when a menu is open
	if (UIManagerComponent && UIManagerComponent->IsAnyMenuOpen())
	{
		return;
	}

	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestPrimaryActionRelease(ControllablePawn);
	}
}

void AMOPlayerController::HandleSecondaryAction(const FInputActionValue& Value)
{
	// Don't process gameplay input when a menu is open
	if (UIManagerComponent && UIManagerComponent->IsAnyMenuOpen())
	{
		return;
	}

	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		// Check if there's an interactable target - if so, trigger secondary interact (right-click menu)
		// Otherwise, fall back to secondary action (block/aim)
		UMOInteractorComponent* Interactor = ControllablePawn->FindComponentByClass<UMOInteractorComponent>();
		if (Interactor)
		{
			// Use unified target finding (supports ISM/HISM and actors); world cover falls through to the forage menu below
			FMOInteractionTarget Target;
			if (Interactor->FindInteractionTarget(Target) && !Target.bIsForageTarget)
			{
				// Check if the target is a recruited survivor - if so, show context menu instead
				if (Target.TargetActor.IsValid())
				{
					if (APawn* TargetPawn = Cast<APawn>(Target.TargetActor.Get()))
					{
						// Not for self, and not for another PLAYER's character (co-op): it is theirs, not a survivor to command -- building the menu on the
						// host gave it a survivor AI and threw the player out of it (Steam playtest Oct 16 2026).
						if (TargetPawn != ControllablePawn && !TargetPawn->IsPlayerControlled())
						{
							if (UMORecruitmentComponent* RecruitComp = TargetPawn->FindComponentByClass<UMORecruitmentComponent>())
							{
								if (RecruitComp->IsPossessable())
								{
									// This is a recruited survivor - show context menu instead of secondary interact
									int32 ViewportSizeX, ViewportSizeY;
									GetViewportSize(ViewportSizeX, ViewportSizeY);
									FVector2D ScreenPosition(ViewportSizeX / 2.0f, ViewportSizeY / 2.0f);

									if (SystemMenuUIController)
									{
										UE_LOG(LogMOFramework, Log, TEXT("[MOPlayerController] Opening survivor context menu for %s at screen center"), *TargetPawn->GetName());
										SystemMenuUIController->ShowSurvivorContextMenu(TargetPawn, ScreenPosition);
										return;
									}
									else
									{
										UE_LOG(LogMOFramework, Warning, TEXT("[MOPlayerController] SystemMenuUIController is null, cannot show survivor context menu"));
									}
								}
							}
						}
					}
				}

				// Not a recruited survivor - use normal secondary interact
				IMOControllableInterface::Execute_RequestSecondaryInteract(ControllablePawn);
				return;
			}
		}

		// No interactable target (or world cover) - the ground forage menu where the crosshair meets the world
		if (TryOpenForageMenuAtCrosshair())
		{
			return;
		}

		// No ground hit or no UI controller - fall back to regular secondary action (block/aim)
		IMOControllableInterface::Execute_RequestSecondaryAction(ControllablePawn);
	}
}

bool AMOPlayerController::TryOpenForageMenuAtCrosshair()
{
	APawn* ControllablePawn = CachedControllablePawn.Get();
	UWorld* World = GetWorld();
	if (!ControllablePawn || !World || !InventoryUIController)
	{
		return false;
	}

	// Camera-based trace: the cursor is hidden in gameplay
	FVector CameraLocation;
	FRotator CameraRotation;
	GetPlayerViewPoint(CameraLocation, CameraRotation);

	FHitResult GroundHit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(ControllablePawn);
	if (!World->LineTraceSingleByChannel(GroundHit, CameraLocation, CameraLocation + CameraRotation.Vector() * 2000.0f, ECC_WorldStatic, QueryParams)
		|| !GroundHit.bBlockingHit)
	{
		return false;
	}

	int32 ViewportSizeX, ViewportSizeY;
	GetViewportSize(ViewportSizeX, ViewportSizeY);
	InventoryUIController->ShowGroundContextMenu(GroundHit.Location, FVector2D(ViewportSizeX / 2.0f, ViewportSizeY / 2.0f));
	return true;
}

bool AMOPlayerController::FindWaterAtCrosshair(FVector& OutPoint, EMOWaterBodyKind& OutKind, bool& bOutWithinReach) const
{
	const APawn* ControllablePawn = CachedControllablePawn.Get();
	const UWorld* World = GetWorld();
	const UMOWaterSubsystem* Water = World ? UMOWaterSubsystem::Get(World) : nullptr;
	if (!ControllablePawn || !Water)
	{
		return false;
	}
	FVector CameraLocation;
	FRotator CameraRotation;
	GetPlayerViewPoint(CameraLocation, CameraRotation);
	// The shoulder camera sits a few metres behind the pawn: look far enough to see past it to water at the pawn's feet.
	constexpr float ViewCm = 1200.0f;
	const FVector Dir = CameraRotation.Vector();
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MOWaterView), false, ControllablePawn);
	const float BlockedAt = World->LineTraceSingleByChannel(Hit, CameraLocation, CameraLocation + Dir * ViewCm, ECC_WorldStatic, QueryParams) ? Hit.Distance : ViewCm;
	if (!Water->FindWaterAlongRay(CameraLocation, Dir, ViewCm, BlockedAt, OutPoint, OutKind))
	{
		return false;
	}
	bOutWithinReach = FVector::Dist(ControllablePawn->GetActorLocation(), OutPoint) <= GetDefault<UMOWaterSourceSettings>()->ReachCm;
	return true;
}

bool AMOPlayerController::TryDrinkAtCrosshair()
{
	FVector Point;
	EMOWaterBodyKind Kind = EMOWaterBodyKind::None;
	bool bWithinReach = false;
	if (!FindWaterAtCrosshair(Point, Kind, bWithinReach))
	{
		return false;
	}
	if (!bWithinReach)
	{
		if (NotificationComponent)
		{
			NotificationComponent->ShowInfoNotification(NSLOCTEXT("MOWater", "TooFarToDrink", "Get closer to the water to drink."), 2.5f);
		}
		return true;
	}
	ServerDrinkWater(Point);
	return true;
}

void AMOPlayerController::ServerGatherCover_Implementation(FVector_NetQuantize Location, int32 EntryIndex)
{
	UMOForagingSubsystem* Foraging = GetWorld() ? GetWorld()->GetSubsystem<UMOForagingSubsystem>() : nullptr;
	FText Reason = NSLOCTEXT("MO", "NothingToGather", "Nothing to gather there.");
	if (!Foraging || !Foraging->BeginGatherCover(GetPawn(), Location, EntryIndex, &Reason))
	{
		ClientShowNotification(Reason, 2.0f, EMONotificationType::Warning);
	}
}

void AMOPlayerController::ServerStationAction_Implementation(AMOCraftingStationActor* Station, FName ActionId)
{
	if (!FMOFireActions::Perform(GetPawn(), Station, ActionId))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOFire] %s: %s on %s refused"), *GetName(), *ActionId.ToString(), *GetNameSafe(Station));
	}
}

void AMOPlayerController::ServerWorldAction_Implementation(AActor* Target, FName ActionId)
{
	FMOWorldActions::Execute(this, Target, ActionId);
}

void AMOPlayerController::ServerInventoryRequest_Implementation(const FMOInventoryRequest& Request)
{
	FMOInventoryRequests::Execute(this, Request);
}

void AMOPlayerController::ServerItemAction_Implementation(FGuid ItemGuid, FName ActionId)
{
	if (!FMOItemActions::Perform(GetPawn(), ItemGuid, ActionId))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOItemActions] %s: %s on %s refused"), *GetName(), *ActionId.ToString(), *ItemGuid.ToString(EGuidFormats::Short));
	}
}

void AMOPlayerController::ServerDrinkWater_Implementation(FVector_NetQuantize Point)
{
	switch (FMOWaterActions::BeginDrink(GetPawn(), Point))
	{
	case EMODrinkRefusal::None:
		break;
	case EMODrinkRefusal::TooFar:
		ClientShowNotification(NSLOCTEXT("MOWater", "TooFar", "Get closer to the water to drink."), 2.5f, EMONotificationType::Info);
		break;
	case EMODrinkRefusal::NoWater:
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWater] %s: drink refused, no water at (%.0f, %.0f, %.0f) on the server"), *GetName(), Point.X, Point.Y, Point.Z);
		break;
	default:
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWater] %s: drink refused (no pawn or no timed-action component)"), *GetName());
		break;
	}
}

void AMOPlayerController::HandleSecondaryActionRelease(const FInputActionValue& Value)
{
	// Don't process gameplay input when a menu is open
	if (UIManagerComponent && UIManagerComponent->IsAnyMenuOpen())
	{
		return;
	}

	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestSecondaryActionRelease(ControllablePawn);
	}
}

// ============================================================================
// INPUT HANDLERS - UI/SYSTEM
// ============================================================================

void AMOPlayerController::HandleInventory(const FInputActionValue& Value)
{
	MOUI_LOG(this, "PC", "HandleInventory FIRED via Enhanced Input");
	if (UIManagerComponent)
	{
		UIManagerComponent->ToggleInventoryMenu();
	}
	else
	{
		MOUI_LOG(this, "PC", "  UIManagerComponent is NULL");
	}
}

void AMOPlayerController::HandleCraft(const FInputActionValue& Value)
{
	MOUI_LOG(this, "PC", "HandleCraft FIRED via Enhanced Input");
	if (UIManagerComponent)
	{
		UIManagerComponent->ToggleCraftingMenu();
	}
}

void AMOPlayerController::HandlePlayerStatus(const FInputActionValue& Value)
{
	MOUI_LOG(this, "PC", "HandlePlayerStatus FIRED via Enhanced Input");
	if (UIManagerComponent)
	{
		UIManagerComponent->TogglePlayerStatus();
	}
}

void AMOPlayerController::HandleSkills(const FInputActionValue& Value)
{
	MOUI_LOG(this, "PC", "HandleSkills FIRED via Enhanced Input");
	if (UIManagerComponent)
	{
		UIManagerComponent->ToggleSkillsPanel();
	}
}

void AMOPlayerController::HandleJournal(const FInputActionValue& Value)
{
	MOUI_LOG(this, "PC", "HandleJournal FIRED via Enhanced Input");
	if (UIManagerComponent)
	{
		UIManagerComponent->ToggleQuestLog();
	}
}

void AMOPlayerController::HandleBuild(const FInputActionValue& Value)
{
	MOUI_LOG(this, "PC", "HandleBuild FIRED via Enhanced Input");
	// If in placement mode, cancel it
	if (BuildingComponent && BuildingComponent->IsInPlacementMode())
	{
		BuildingComponent->ExitPlacementMode(true);
		return;
	}

	// Otherwise toggle the building menu
	if (UIManagerComponent)
	{
		UIManagerComponent->ToggleBuildingMenu();
	}
}

bool AMOPlayerController::TryCloseMenuForToggleKey(const FKey& Key)
{
	UEnhancedInputLocalPlayerSubsystem* Input = GetEnhancedInputSubsystem();
	if (!Input || !UIManagerComponent)
	{
		return false;
	}
	UMOUIManagerComponent* UI = UIManagerComponent;
	struct FToggle { const UInputAction* Action; bool bOpen; TFunction<void()> Close; };
	const FToggle Toggles[] = {
		{ InventoryAction,    UI->IsInventoryMenuOpen(), [UI]() { UI->ToggleInventoryMenu(); } },
		{ CraftAction,        UI->IsCraftingMenuOpen(),  [UI]() { UI->ToggleCraftingMenu(); } },
		{ SkillsAction,       UI->IsSkillsPanelOpen(),   [UI]() { UI->ToggleSkillsPanel(); } },
		{ JournalAction,      UI->IsQuestLogOpen(),      [UI]() { UI->ToggleQuestLog(); } },
		{ BuildAction,        UI->IsBuildingMenuOpen(),  [UI]() { UI->ToggleBuildingMenu(); } },
		{ PossessAction,      UI->IsPossessionMenuOpen(),[UI]() { UI->TogglePossessionMenu(); } },
		{ PlayerStatusAction, UI->IsPlayerStatusVisible(), [UI]() { UI->TogglePlayerStatus(); } },
	};
	for (const FToggle& T : Toggles)
	{
		if (T.Action && T.bOpen && Input->QueryKeysMappedToAction(T.Action).Contains(Key))
		{
			MOUI_LOG(this, "PC", "%s closes the menu it toggles", *Key.ToString());
			T.Close();
			return true;
		}
	}
	return false;
}

void AMOPlayerController::HandleBackMenu(const FInputActionValue& Value)
{
	MOUI_LOG(this, "PC", "HandleBackMenu FIRED via Enhanced Input");
	// "Back / Menu" semantic:
	//   - Any menu open → close the topmost one (inventory, crafting,
	//     in-game menu, etc.). Per-widget Tab handling in
	//     UMOActivatableWidget::NativeOnPreviewKeyDown also handles this
	//     when a menu has Slate focus; this path is the fallback for when
	//     focus has drifted to the viewport.
	//   - Nothing open → open the in-game system menu.
	//
	// Does NOT pause. MO57 is real-time — see Docs/PAUSE_POLICY.md.
	if (UIManagerComponent)
	{
		if (UIManagerComponent->IsAnyMenuOpen())
		{
			UIManagerComponent->CloseActiveMenu();
		}
		else
		{
			UIManagerComponent->ToggleInGameMenu();
		}
	}
}

void AMOPlayerController::HandlePossess(const FInputActionValue& Value)
{
	MOUI_LOG(this, "PC", "HandlePossess FIRED via Enhanced Input");
	UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController::HandlePossess - Input received"));

	if (UIManagerComponent)
	{
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController::HandlePossess - Calling TogglePossessionMenu"));
		UIManagerComponent->TogglePossessionMenu();
		// Tutorial objective "press P to open the possession menu" (Tutorial_Possession).
		BroadcastTutorialEvent(TEXT("PlayerOpenedPossessionMenu"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController::HandlePossess - UIManagerComponent is NULL!"));
	}
}

// ============================================================================
// DEV CONSOLE POPUP
// ============================================================================

namespace
{
	/** Collects what the game logs while one console command runs, so the popup can show a result (most MO commands only log). */
	class FMOConsoleLogCapture : public FOutputDevice
	{
	public:
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			// Only the game's own categories, plus anything at Display or worse: engine chatter is not a command result.
			if (Verbosity > ELogVerbosity::Display && !Category.ToString().StartsWith(TEXT("LogMO")))
			{
				return;
			}
			FScopeLock Lock(&Mutex);
			if (Lines.Num() < 12)
			{
				Lines.Add(FString(Message));
			}
		}
		TArray<FString> TakeLines()
		{
			FScopeLock Lock(&Mutex);
			return MoveTemp(Lines);
		}

	private:
		FCriticalSection Mutex;
		TArray<FString> Lines;
	};
}

bool AMOPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// Dev console popup on the Tilde key. Hard-coded (requested) and deliberately NOT an Input Action: UEnhancedInputComponent deletes
	// the legacy BindKey, and a mapping context would only add assets to forget. InputKey sees every key before action bindings.
	if (Params.Key == EKeys::Tilde && Params.Event == IE_Pressed && FMODevMode::IsEnabled())
	{
		HandleOpenDevConsole();
		return true;
	}
	// The HUD mode key, unmodified (Shift+F1 is the editor's "release the mouse" in PIE).
	if (Params.Key == HUDModeKey && UIManagerComponent && !(PlayerInput && (PlayerInput->IsShiftPressed() || PlayerInput->IsCtrlPressed() || PlayerInput->IsAltPressed())))
	{
		if (Params.Event == IE_Pressed)
		{
			UIManagerComponent->CycleHUDMode();
		}
		return true;   // the release too: nothing else may act on this key (dev builds bind F1 to wireframe)
	}
	return Super::InputKey(Params);
}

void AMOPlayerController::HandleOpenDevConsole(const FString& InitialText)
{
	if (!FMODevMode::IsEnabled())
	{
		return;
	}
	if (DevConsoleDialog.IsValid())
	{
		return; // already open: do not stack a second popup
	}

	UClass* DialogClass = DevConsoleDialogClass.LoadSynchronous();
	if (!DialogClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] Cannot open the console popup: %s did not load"), *DevConsoleDialogClass.ToString());
		return;
	}

	UCommonActivatableWidget* Pushed = UMOGameUIManagerSubsystem::PushModalWidget(this, DialogClass);
	UMOTextInputDialog* Dialog = Cast<UMOTextInputDialog>(Pushed);
	if (!Dialog)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] Cannot open the console popup: modal push returned %s"), *GetNameSafe(Pushed));
		return;
	}

	FString Recent;
	for (const FString& Cmd : DevConsoleHistory)
	{
		Recent += (Recent.IsEmpty() ? FString() : FString(TEXT("   |   "))) + Cmd;
	}
	Dialog->ConfigureWithText(
		NSLOCTEXT("MOConsole", "Title", "Console"),
		Recent.IsEmpty() ? NSLOCTEXT("MOConsole", "Message", "Type a command. Run it empty: command menu.")
		                 : FText::Format(NSLOCTEXT("MOConsole", "MessageRecent", "Recent: {0}\nRun it empty: command menu."), FText::FromString(Recent)),
		FText::FromString(InitialText),
		/*MaxLength=*/256,
		NSLOCTEXT("MOConsole", "Hint", "command"),
		NSLOCTEXT("MOConsole", "Run", "Run"),
		NSLOCTEXT("MOConsole", "Close", "Close"));
	Dialog->OnTextConfirmed.AddDynamic(this, &AMOPlayerController::HandleDevConsoleConfirmed);
	DevConsoleDialog = Dialog;
	UE_LOG(LogMOFramework, Log, TEXT("[MOConsole] popup opened"));
}

void AMOPlayerController::HandleDevConsoleConfirmed(const FText& EnteredText)
{
	DevConsoleDialog.Reset();
	RunDevConsoleCommand(EnteredText.ToString());
}

FString AMOPlayerController::SanitizeDevConsoleInput(const FString& Raw)
{
	FString Command = Raw;
	Command.TrimStartAndEndInline();
	while (Command.StartsWith(TEXT("`")) || Command.StartsWith(TEXT("~")))
	{
		Command.RightChopInline(1);
		Command.TrimStartInline();
	}
	return Command;
}

void AMOPlayerController::RunDevConsoleCommand(const FString& RawCommandLine)
{
	if (!FMODevMode::IsEnabled())
	{
		return;
	}
	const FString Command = SanitizeDevConsoleInput(RawCommandLine);
	if (Command.IsEmpty())
	{
		OpenDevCommandMenu();   // Run with nothing typed: browse the commands instead
		return;
	}

	DevConsoleHistory.Remove(Command);
	DevConsoleHistory.Add(Command);
	while (DevConsoleHistory.Num() > 4)
	{
		DevConsoleHistory.RemoveAt(0);
	}

	FMOConsoleLogCapture Capture;
	GLog->AddOutputDevice(&Capture);
	const FString Printed = ConsoleCommand(Command, /*bWriteToLog=*/true);
	// The log redirector buffers lines for its own thread; without this flush the command's lines reach the capture device only
	// AFTER it is removed, and the popup reports an empty result for a command that did print.
	GLog->FlushThreadedLogs();
	GLog->RemoveOutputDevice(&Capture);

	// Result = what the command wrote to the output device, else what the game logged while it ran.
	FString Result = Printed.TrimStartAndEnd();
	if (Result.IsEmpty())
	{
		Result = FString::Join(Capture.TakeLines(), TEXT("\n"));
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] > %s%s%s"), *Command, Result.IsEmpty() ? TEXT("") : TEXT("  =>  "),
		*Result.Replace(TEXT("\n"), TEXT(" | ")));

	if (NotificationComponent)
	{
		const FText Shown = Result.IsEmpty()
			? FText::Format(NSLOCTEXT("MOConsole", "Ran", "> {0}"), FText::FromString(Command))
			: FText::Format(NSLOCTEXT("MOConsole", "RanResult", "> {0}\n{1}"), FText::FromString(Command), FText::FromString(Result));
		NotificationComponent->ShowInfoNotification(Shown, 6.0f);
	}
}

// ============================================================================
// DEV COMMAND MENU: every MO.* console command as a button, grouped by namespace
// ============================================================================

namespace
{
	struct FMODevMenuCommand
	{
		FString Name;
		FString Help;
	};

	/** Every registered MO.* console COMMAND (variables are not listed), by namespace: MO.Player.GiveItem is in "Player"; a namespace of one goes to "Other". */
	TMap<FString, TArray<FMODevMenuCommand>> GatherDevMenuCommands()
	{
		TMap<FString, TArray<FMODevMenuCommand>> Groups;
		IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateLambda([&Groups](const TCHAR* Name, IConsoleObject* Object)
		{
			if (!Object || !Object->AsCommand())
			{
				return;
			}
			TArray<FString> Parts;
			FString(Name).ParseIntoArray(Parts, TEXT("."));
			Groups.FindOrAdd(Parts.Num() >= 3 ? Parts[1] : FString(TEXT("Other"))).Add({ FString(Name), FString(Object->GetHelp()) });
		}), TEXT("MO."));

		TArray<FString> Singles;
		for (const TPair<FString, TArray<FMODevMenuCommand>>& Group : Groups)
		{
			if (Group.Value.Num() == 1 && Group.Key != TEXT("Other"))
			{
				Singles.Add(Group.Key);
			}
		}
		for (const FString& Key : Singles)
		{
			Groups.FindOrAdd(TEXT("Other")).Append(Groups.FindAndRemoveChecked(Key));
		}
		for (TPair<FString, TArray<FMODevMenuCommand>>& Group : Groups)
		{
			Group.Value.Sort([](const FMODevMenuCommand& A, const FMODevMenuCommand& B) { return A.Name < B.Name; });
		}
		return Groups;
	}

	/** A command whose usage names a required argument (<...>) opens the console with its name typed in; any other runs on the click. */
	bool DevMenuCommandTakesArguments(const FString& Help)
	{
		return Help.Contains(TEXT("<"));
	}
}

void AMOPlayerController::OpenDevCommandMenu(const FString& Group)
{
	if (!FMODevMode::IsEnabled())
	{
		return;
	}
	const TMap<FString, TArray<FMODevMenuCommand>> Groups = GatherDevMenuCommands();
	// The menus chain into each other (and into the console popup); each next one opens on the next tick, after the clicked one has closed.
	auto Then = [this](TFunction<void()> Next)
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, MoveTemp(Next)));
	};

	TArray<FMOQuickPickOption> Options;
	if (Group.IsEmpty() || !Groups.Contains(Group))
	{
		Options.Add({ FName(TEXT("@type")), NSLOCTEXT("MOConsole", "MenuType", "Type a command..."), NSLOCTEXT("MOConsole", "MenuTypeTip", "The console popup (~).") });
		Options.Add({ FName(TEXT("@biome")), NSLOCTEXT("MOConsole", "MenuBiome", "Go to a biome..."), NSLOCTEXT("MOConsole", "MenuBiomeTip", "biome: teleports you deep inside the biome you pick.") });
		Options.Add({ FName(TEXT("@poi")), NSLOCTEXT("MOConsole", "MenuPoi", "Go to a POI..."), NSLOCTEXT("MOConsole", "MenuPoiTip", "poi: takes you to the nearest world feature you pick.") });
		Options.Add({ FName(TEXT("@weather")), NSLOCTEXT("MOConsole", "MenuWeather", "Weather..."), NSLOCTEXT("MOConsole", "MenuWeatherTip", "weather: sets the server's weather.") });
		Options.Add({ FName(TEXT("@time")), NSLOCTEXT("MOConsole", "MenuTime", "Time of day..."), NSLOCTEXT("MOConsole", "MenuTimeTip", "time: moves the server's clock forward.") });
		Options.Add({ FName(TEXT("@starter")), NSLOCTEXT("MOConsole", "MenuStarter", "Starter kit"), NSLOCTEXT("MOConsole", "MenuStarterTip", "starter: 50 sticks and 20 stones.") });
		TArray<FString> Names;
		Groups.GetKeys(Names);
		Names.Sort([](const FString& A, const FString& B) { return A == TEXT("Other") ? false : (B == TEXT("Other") ? true : A < B); });
		for (const FString& Name : Names)
		{
			Options.Add({ FName(*Name), FText::Format(NSLOCTEXT("MOConsole", "MenuGroup", "{0} ({1})"), FText::FromString(Name), FText::AsNumber(Groups[Name].Num())) });
		}
		OpenDevQuickPick(NSLOCTEXT("MOConsole", "MenuTitle", "Developer menu"),
			NSLOCTEXT("MOConsole", "MenuMessage", "Every MO console command, by group. Hover a button for its help."), Options, /*Columns=*/4,
			[this, Then](FName Id)
			{
				const FString Pick = Id.ToString();
				Then([this, Pick]()
				{
					if (Pick == TEXT("@type"))
					{
						HandleOpenDevConsole();
					}
					else if (Pick.StartsWith(TEXT("@")))
					{
						RunDevConsoleCommand(Pick.RightChop(1));   // biome / poi / weather / time / starter
					}
					else
					{
						OpenDevCommandMenu(Pick);
					}
				});
			});
		return;
	}

	const FString Prefix = FString::Printf(TEXT("MO.%s."), *Group);
	Options.Add({ FName(TEXT("@back")), NSLOCTEXT("MOConsole", "MenuBack", "< Back") });
	for (const FMODevMenuCommand& Command : Groups[Group])
	{
		FString Label = Command.Name.StartsWith(Prefix) ? Command.Name.RightChop(Prefix.Len()) : Command.Name.RightChop(3);
		if (DevMenuCommandTakesArguments(Command.Help))
		{
			Label += TEXT("...");
		}
		Options.Add({ FName(*Command.Name), FText::FromString(Label), FText::FromString(Command.Help) });
	}
	OpenDevQuickPick(FText::Format(NSLOCTEXT("MOConsole", "MenuGroupTitle", "{0} commands"), FText::FromString(Group)),
		NSLOCTEXT("MOConsole", "MenuGroupMessage", "A click runs the command; one ending in ... opens the console with it typed in, for its arguments."), Options, /*Columns=*/3,
		[this, Then](FName Id)
		{
			const FString Pick = Id.ToString();
			Then([this, Pick]()
			{
				if (Pick == TEXT("@back"))
				{
					OpenDevCommandMenu();
					return;
				}
				const IConsoleObject* Object = IConsoleManager::Get().FindConsoleObject(*Pick);
				if (Object && DevMenuCommandTakesArguments(Object->GetHelp()))
				{
					HandleOpenDevConsole(Pick + TEXT(" "));
				}
				else
				{
					RunDevConsoleCommand(Pick);
				}
			});
		});
}

bool AMOPlayerController::ServerAllowsDevCheats(const TCHAR* What)
{
	// The server's own developer mode decides (a client's Options toggle cannot turn cheats on in the host's world).
	if (FMODevMode::IsEnabled())
	{
		return true;
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] %s refused: developer mode is off on the server"), What);
	if (NotificationComponent)
	{
		NotificationComponent->ShowInfoNotification(NSLOCTEXT("MOConsole", "ServerDevOff", "The host's developer mode is off."), 4.0f);
	}
	return false;
}

// ============================================================================
// DEV PICKERS (`weather`, `time`)
// ============================================================================

void AMOPlayerController::OpenDevQuickPick(const FText& Title, const FText& Message, const TArray<FMOQuickPickOption>& Options, int32 Columns, TFunction<void(FName)> OnChosen)
{
	if (!FMODevMode::IsEnabled())
	{
		return;
	}
	if (DevQuickPickDialog.IsValid())
	{
		return; // already open
	}
	UClass* DialogClass = DevQuickPickDialogClass.LoadSynchronous();
	if (!DialogClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] Cannot open the picker: %s did not load (build it: python Tools/ue.py ui build Content/Python/ui_specs/quick_pick.py)"), *DevQuickPickDialogClass.ToString());
		return;
	}
	UMOQuickPickDialog* Dialog = Cast<UMOQuickPickDialog>(UMOGameUIManagerSubsystem::PushModalWidget(this, DialogClass));
	if (!Dialog)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] Cannot open the picker: the modal push returned nothing"));
		return;
	}
	Dialog->Configure(Title, Message, Options, Columns);
	UE_LOG(LogMOFramework, Log, TEXT("[MOConsole] picker opened: %s (%d options)"), *Title.ToString(), Options.Num());
	Dialog->OnChosen.AddWeakLambda(this, [this, OnChosen = MoveTemp(OnChosen)](FName Id) { DevQuickPickDialog.Reset(); OnChosen(Id); });
	Dialog->OnCancelled.AddWeakLambda(this, [this]() { DevQuickPickDialog.Reset(); });
	DevQuickPickDialog = Dialog;
}

void AMOPlayerController::OpenDevWeatherPicker(const FString& PresetName)
{
	if (!FMODevMode::IsEnabled())
	{
		return;
	}
	if (!PresetName.IsEmpty())
	{
		ServerDevSetWeather(FName(*PresetName));
		return;
	}
	TArray<FMOQuickPickOption> Options;
	for (const FMOWeatherPresetChoice& Choice : UMOWeatherIntegrationSubsystem::GetPresetChoices())
	{
		if (Choice.bBasic)
		{
			Options.Add({ Choice.Id, Choice.Label });
		}
	}
	OpenDevQuickPick(NSLOCTEXT("MOConsole", "WeatherTitle", "Weather"), NSLOCTEXT("MOConsole", "WeatherMessage", "Sets the server's weather."), Options, /*Columns=*/3,
		[this](FName Id) { ServerDevSetWeather(Id); });
}

void AMOPlayerController::OpenDevTimePicker(int32 Hour)
{
	if (!FMODevMode::IsEnabled())
	{
		return;
	}
	if (Hour >= 0)
	{
		ServerDevAdvanceToTime(FMath::Min(Hour, 23), 0);
		return;
	}
	if (DevTimeDialog.IsValid() && DevTimeDialog->IsActivated())
	{
		return;   // already open
	}
	UClass* DialogClass = DevTimeDialogClass.LoadSynchronous();
	UMOTimeOfDayDialog* Dialog = DialogClass ? Cast<UMOTimeOfDayDialog>(UMOGameUIManagerSubsystem::PushModalWidget(this, DialogClass)) : nullptr;
	if (!Dialog)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] Cannot open the time dialog: %s did not load or push (build it: python Tools/ue.py ui build Content/Python/ui_specs/kit_time_dialog.py)"),
			*DevTimeDialogClass.ToString());
		return;
	}
	Dialog->Configure();
	Dialog->OnChosen.AddWeakLambda(this, [this](int32 Minutes) { DevTimeDialog.Reset(); ServerDevAdvanceToTime(Minutes / 60, Minutes % 60); });
	Dialog->OnCancelled.AddWeakLambda(this, [this]() { DevTimeDialog.Reset(); });
	DevTimeDialog = Dialog;
	UE_LOG(LogMOFramework, Log, TEXT("[MOConsole] time dialog opened"));
}

void AMOPlayerController::DevAdvanceToGoldenHour(bool bEvening)
{
	if (!FMODevMode::IsEnabled())
	{
		return;
	}
	const UMOWeatherIntegrationSubsystem* Weather = GetWorld() ? GetWorld()->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr;
	const int32 Minutes = Weather ? Weather->GetGoldenHourMinutesOfDay(bEvening) : (bEvening ? 17 * 60 + 30 : 6 * 60 + 30);
	ServerDevAdvanceToTime(Minutes / 60, Minutes % 60);
}

void AMOPlayerController::ServerDevSetWeather_Implementation(FName PresetName)
{
	if (!ServerAllowsDevCheats(TEXT("weather")))
	{
		return;
	}
	UMOWeatherIntegrationSubsystem* Weather = GetWorld() ? GetWorld()->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr;
	const bool bOk = Weather && Weather->SetWeatherPresetByName(PresetName.ToString());
	UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] weather -> %s: %s"), *PresetName.ToString(), bOk ? TEXT("applied") : TEXT("FAILED (unknown preset or no weather provider)"));
	if (NotificationComponent)
	{
		NotificationComponent->ShowInfoNotification(FText::Format(NSLOCTEXT("MOConsole", "WeatherSet", "Weather: {0}{1}"), FText::FromName(PresetName),
			bOk ? FText::GetEmpty() : NSLOCTEXT("MOConsole", "WeatherFailed", " (failed: see the log)")), 4.0f);
	}
}

void AMOPlayerController::ServerDevAdvanceToTime_Implementation(int32 Hour, int32 Minute)
{
	if (!ServerAllowsDevCheats(TEXT("time")))
	{
		return;
	}
	UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(GetWorld());
	if (!Clock)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] time -> %02d:%02d: FAILED (no game clock)"), Hour, Minute);
		return;
	}
	const FDateTime Now = Clock->AdvanceToTimeOfDay(Hour, Minute);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] time -> %02d:%02d: clock now %s"), Hour, Minute, *Now.ToString());
	if (NotificationComponent)
	{
		NotificationComponent->ShowInfoNotification(FText::Format(NSLOCTEXT("MOConsole", "TimeSet", "Time: {0}"), FText::FromString(Now.ToString(TEXT("%Y-%m-%d %H:%M")))), 4.0f);
	}
}

void AMOPlayerController::OpenDevBiomePicker(const FString& BiomeId)
{
	if (!FMODevMode::IsEnabled())
	{
		return;
	}
	if (!BiomeId.IsEmpty())
	{
		ServerDevTeleportToBiome(FName(*BiomeId));
		return;
	}
	if (!FMOWorldGenParamsProvider::HasPublished())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] biome: no world generator in this world"));
		return;
	}
	TArray<FMOQuickPickOption> Options;
	for (const FMOWorldGenBiome& Biome : FMOWorldGenParamsProvider::Get()->Biomes)
	{
		Options.Add({ Biome.Id, FText::FromString(FName::NameToDisplayString(Biome.Id.ToString(), false)) });
	}
	Options.Sort([](const FMOQuickPickOption& A, const FMOQuickPickOption& B) { return A.Label.CompareTo(B.Label) < 0; });
	OpenDevQuickPick(NSLOCTEXT("MOConsole", "BiomeTitle", "Biome"), NSLOCTEXT("MOConsole", "BiomeMessage", "Teleports you to the nearest place deep inside the biome."), Options, /*Columns=*/3,
		[this](FName Id) { ServerDevTeleportToBiome(Id); });
}

void AMOPlayerController::ServerDevTeleportToBiome_Implementation(FName BiomeId)
{
	if (!ServerAllowsDevCheats(TEXT("biome")))
	{
		return;
	}
	UWorld* World = GetWorld();
	AMOCharacter* Traveller = Cast<AMOCharacter>(GetPawn());
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(World);
	auto Say = [this](const FText& Text)
	{
		if (NotificationComponent)
		{
			NotificationComponent->ShowInfoNotification(Text, 5.0f);
		}
	};
	if (!World || !Traveller || !Seeds || !Seeds->HasActiveSeed() || !FMOWorldGenParamsProvider::HasPublished())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] biome %s: needs a character and a generated world"), *BiomeId.ToString());
		return;
	}
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	const int32 Index = Params->FindBiomeIndex(BiomeId);
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] biome %s: no such biome"), *BiomeId.ToString());
		Say(FText::Format(NSLOCTEXT("MOConsole", "BiomeUnknown", "No biome called {0}"), FText::FromName(BiomeId)));
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	const FVector From = Traveller->GetActorLocation();
	const FMOBiomeFinder Finder(Params, Seeds->GetActiveTerrainSeed());
	FVector Spot;
	int32 Samples = 0;
	const double WorldCm = Params->Tuning.WorldHalfSizeCm * 2.0;
	const bool bFound = Finder.FindNearest(FVector2D(From), Index, WorldCm, /*bRough=*/false, Spot, &Samples)
		|| Finder.FindNearest(FVector2D(From), Index, WorldCm, /*bRough=*/true, Spot, &Samples);   // a biome that is never flat (scree, cliffs): any spot inside it
	const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
	const FString Label = FName::NameToDisplayString(BiomeId.ToString(), false);
	if (!bFound)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] biome %s: none in this world (%d samples, %.0f ms)"), *BiomeId.ToString(), Samples, Ms);
		Say(FText::Format(NSLOCTEXT("MOConsole", "BiomeNone", "{0}: not in this world"), FText::FromString(Label)));
		return;
	}
	const float DistKm = FVector2D::Distance(FVector2D(From), FVector2D(Spot)) / 100000.0f;
	UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] biome %s: x=%.0f y=%.0f ground=%.0f, %.1f km away (%d samples, %.0f ms) -- holding the pawn until the ground is there"),
		*BiomeId.ToString(), Spot.X, Spot.Y, Spot.Z, DistKm, Samples, Ms);
	Say(FText::Format(NSLOCTEXT("MOConsole", "BiomeGo", "{0}: {1} km away"), FText::FromString(Label), FText::AsNumber(FMath::RoundToInt(DistKm * 10.0f) / 10.0f)));

	BeginDevTeleportLanding(Traveller, Spot, FRotator(-6.0f, FMath::FRand() * 360.0f, 0.0f));
}

void AMOPlayerController::BeginDevTeleportLanding(AMOCharacter* Traveller, const FVector& Spot, const FRotator& Facing)
{
	UWorld* World = GetWorld();
	if (!World || !Traveller)
	{
		return;
	}
	// Stand on the generator's ground and hold there (no gravity, no fall-through rescue) until the voxel collision under the pawn exists.
	const MOSpawnClearance::FCapsule Capsule = MOSpawnClearance::CapsuleOf(Traveller);
	Traveller->SetLoadHold(true);
	Traveller->SetActorLocation(FVector(Spot.X, Spot.Y, Spot.Z + Capsule.HalfHeight + 10.0f), false, nullptr, ETeleportType::TeleportPhysics);
	SetControlRotation(Facing);
	ClientSetRotation(Facing);
	DevTeleportPawn = Traveller;
	DevTeleportTarget = Spot;
	DevTeleportStartSeconds = FPlatformTime::Seconds();
	World->GetTimerManager().SetTimer(DevTeleportTimer, this, &AMOPlayerController::PollDevTeleportLanding, 0.25f, /*bLoop=*/true);
}

void AMOPlayerController::OpenDevFeaturePicker(const FString& FeatureId)
{
	if (!FMODevMode::IsEnabled())
	{
		return;
	}
	if (!FeatureId.IsEmpty())
	{
		ServerDevTeleportToFeature(FName(*FeatureId));
		return;
	}
	const UMOWorldFeatureSettings* Settings = GetDefault<UMOWorldFeatureSettings>();
	const UDataTable* Table = Settings ? Settings->FeatureTable.LoadSynchronous() : nullptr;
	if (!Table)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] poi: no DT_WorldFeatures"));
		return;
	}
	TArray<FMOQuickPickOption> Options;
	Table->ForeachRow<FMOWorldFeatureDefinitionRow>(TEXT("poi"), [&Options](const FName& Name, const FMOWorldFeatureDefinitionRow& Row)
	{
		if (Row.bEnabled)
		{
			Options.Add({ Name, Row.DisplayName.IsEmpty() ? FText::FromString(FName::NameToDisplayString(Name.ToString(), false)) : Row.DisplayName });
		}
	});
	OpenDevQuickPick(NSLOCTEXT("MOConsole", "PoiTitle", "Point of interest"), NSLOCTEXT("MOConsole", "PoiMessage", "Takes you to the nearest one, facing it."), Options, /*Columns=*/2,
		[this](FName Id) { ServerDevTeleportToFeature(Id); });
}

void AMOPlayerController::ServerDevTeleportToFeature_Implementation(FName FeatureId)
{
	if (!ServerAllowsDevCheats(TEXT("poi")))
	{
		return;
	}
	AMOCharacter* Traveller = Cast<AMOCharacter>(GetPawn());
	UMOWorldFeatureSubsystem* Features = UMOWorldFeatureSubsystem::Get(this);
	FMOWorldFeatureSite Site;
	if (!Traveller || !Features || !Features->FindNearestSite(FeatureId, FVector2D(Traveller->GetActorLocation()), 3000000.0, Site))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] poi %s: none within 30 km (or no pawn / catalog)"), *FeatureId.ToString());
		if (NotificationComponent)
		{
			NotificationComponent->ShowInfoNotification(FText::Format(NSLOCTEXT("MOConsole", "PoiNone", "{0}: none nearby"), FText::FromName(FeatureId)), 5.0f);
		}
		return;
	}
	// Stand outside the footprint, looking at the composition.
	FVector2D Away = (FVector2D(Traveller->GetActorLocation()) - FVector2D(Site.Location)).GetSafeNormal();
	if (Away.IsNearlyZero())
	{
		Away = FVector2D(1.0, 0.0);   // standing on the site already: approach from +X
	}
	const FVector2D StandXY = FVector2D(Site.Location) + Away * (Site.FootprintRadiusCm + 1500.0);
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(this);
	const float Ground = FMOWorldGen::SampleHeightCm(*Params, StandXY.X, StandXY.Y, Seeds ? Seeds->GetActiveTerrainSeed() : 0, Params->Tuning.VoxelSizeCm);
	const FRotator Facing(-8.0f, FMath::RadiansToDegrees(FMath::Atan2(-Away.Y, -Away.X)), 0.0f);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] poi %s: site at %s, %.1f km away"), *FeatureId.ToString(), *Site.Location.ToCompactString(),
		FVector2D::Distance(FVector2D(Traveller->GetActorLocation()), FVector2D(Site.Location)) / 100000.0);
	BeginDevTeleportLanding(Traveller, FVector(StandXY.X, StandXY.Y, Ground), Facing);
}

void AMOPlayerController::PollDevTeleportLanding()
{
	AMOCharacter* Traveller = DevTeleportPawn.Get();
	UWorld* World = GetWorld();
	const double Waited = FPlatformTime::Seconds() - DevTeleportStartSeconds;
	FHitResult Hit;
	const bool bGround = Traveller && World && MOSpawnClearance::TraceVoxelGround(World, DevTeleportTarget.X, DevTeleportTarget.Y, DevTeleportTarget.Z + 3000.0f, DevTeleportTarget.Z - 3000.0f, Traveller, Hit)
		&& FMath::Abs(Hit.ImpactPoint.Z - DevTeleportTarget.Z) <= 150.0f;   // the refined surface, not a coarse far-LOD one
	if (!Traveller || bGround || Waited > 20.0)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(DevTeleportTimer);
		}
		if (Traveller)
		{
			const float GroundZ = bGround ? Hit.ImpactPoint.Z : DevTeleportTarget.Z;
			Traveller->SetActorLocation(FVector(DevTeleportTarget.X, DevTeleportTarget.Y, GroundZ + MOSpawnClearance::CapsuleOf(Traveller).HalfHeight + 6.0f), false, nullptr, ETeleportType::TeleportPhysics);
			Traveller->SetLoadHold(false);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOConsole] biome teleport landed after %.1f s on %s ground z=%.0f"), Waited, bGround ? TEXT("the voxel") : TEXT("the generator's (no collision yet)"), GroundZ);
		}
		DevTeleportPawn.Reset();
	}
}

// ============================================================================
// INPUT HANDLERS - BUILDING
// ============================================================================

void AMOPlayerController::HandlePlaceBuilding(const FInputActionValue& Value)
{
	if (BuildingComponent && BuildingComponent->IsInPlacementMode())
	{
		BuildingComponent->TryPlaceGhost();
	}
}

void AMOPlayerController::HandleRotateBuildingCW(const FInputActionValue& Value)
{
	if (BuildingComponent && BuildingComponent->IsInPlacementMode())
	{
		BuildingComponent->RotateGhostZ(BuildingRotationIncrement);
	}
}

void AMOPlayerController::HandleRotateBuildingCCW(const FInputActionValue& Value)
{
	if (BuildingComponent && BuildingComponent->IsInPlacementMode())
	{
		BuildingComponent->RotateGhostZ(-BuildingRotationIncrement);
	}
}

void AMOPlayerController::HandleFlipPlacementYaw(const FInputActionValue& Value)
{
	if (BuildingComponent && BuildingComponent->IsInPlacementMode())
	{
		BuildingComponent->ToggleGhostFlipYaw();
	}
}

void AMOPlayerController::HandleCancelPlacement(const FInputActionValue& Value)
{
	if (BuildingComponent && BuildingComponent->IsInPlacementMode())
	{
		BuildingComponent->ExitPlacementMode(true); // true = cancel
	}
}

// ============================================================================
// INPUT HANDLERS - TERRAFORMING
// ============================================================================

void AMOPlayerController::HandleTerraformToggle(const FInputActionValue& Value)
{
	// Don't process gameplay input when a menu is open
	if (UIManagerComponent && UIManagerComponent->IsAnyMenuOpen())
	{
		return;
	}

	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestTerraformToggle(ControllablePawn);
	}
}

void AMOPlayerController::HandleTerraformCycleTool(const FInputActionValue& Value)
{
	// Don't process gameplay input when a menu is open
	if (UIManagerComponent && UIManagerComponent->IsAnyMenuOpen())
	{
		return;
	}

	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestTerraformCycleTool(ControllablePawn);
	}
}

void AMOPlayerController::HandleView(const FInputActionValue& Value)
{
	// Toggle camera shoulder - works even with menus open
	if (APawn* ControlledPawn = GetPawn())
	{
		if (AMOCharacter* MOChar = Cast<AMOCharacter>(ControlledPawn))
		{
			MOChar->ToggleCameraShoulder();
			BroadcastTutorialEvent(TEXT("ToggledView"));
		}
	}
}

// ============================================================================
// INPUT HANDLERS - TUTORIAL
// ============================================================================

void AMOPlayerController::HandleSkipTutorialItem(const FInputActionValue& Value)
{
	UMOQuestSubsystem* Quest = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMOQuestSubsystem>() : nullptr;
	if (Quest)
	{
		Quest->SkipCurrentTutorialObjective();
	}
}

void AMOPlayerController::HandleSkipAllTutorials(const FInputActionValue& Value)
{
	UMOQuestSubsystem* Quest = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMOQuestSubsystem>() : nullptr;
	if (Quest)
	{
		// F3 disables popups for the rest of the session. We don't toggle —
		// once dismissed, stays dismissed. Re-enable via console or a settings
		// menu later if needed.
		Quest->SetTutorialPopupsEnabled(false);
	}
}

void AMOPlayerController::BroadcastTutorialEvent(FName EventName)
{
	UMOQuestSubsystem* Quest = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMOQuestSubsystem>() : nullptr;
	if (!Quest)
	{
		return;
	}

	// Gate on the listener cache so high-frequency callers (HandleMove /
	// HandleLook fire every tick) cost nothing once no quest cares. Once
	// the movement tutorials complete, this returns false and FireGameEvent
	// is never called — no map iteration, no logging spam.
	if (!Quest->IsAnyQuestListeningForEvent(EventName))
	{
		return;
	}

	Quest->FireGameEvent(EventName);
}

// ============================================================================
// INTERNAL
// ============================================================================

UEnhancedInputLocalPlayerSubsystem* AMOPlayerController::GetEnhancedInputSubsystem() const
{
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		return LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	}
	return nullptr;
}

void AMOPlayerController::AddMappingContext(UInputMappingContext* Context, int32 Priority)
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem())
	{
		if (!Subsystem->HasMappingContext(Context))
		{
			Subsystem->AddMappingContext(Context, Priority);
		}
	}
}

void AMOPlayerController::RemoveMappingContext(UInputMappingContext* Context)
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem())
	{
		Subsystem->RemoveMappingContext(Context);
	}
}

// ============================================================================
// IMOUIContractInterface IMPLEMENTATION
// ============================================================================

bool AMOPlayerController::ForwardUIContractRequest(EMOUIContractRequest Request, AActor* Target, const FVector& WorldPosition)
{
	if (IsLocalController())
	{
		return false;
	}
	ClientUIContractRequest(Request, Target, WorldPosition);
	return true;
}

void AMOPlayerController::ClientUIContractRequest_Implementation(EMOUIContractRequest Request, AActor* Target, FVector_NetQuantize WorldPosition)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOUIContract] the server asked this client for UI request %d on %s"), static_cast<int32>(Request), *GetNameSafe(Target));
	switch (Request)
	{
	case EMOUIContractRequest::ShowBuildWidget:        RequestShowBuildWidget_Implementation(Target); break;
	case EMOUIContractRequest::OpenContainerInventory: RequestOpenContainerInventory_Implementation(Target); break;
	case EMOUIContractRequest::OpenCraftingMenu:       RequestOpenCraftingMenu_Implementation(Target); break;
	case EMOUIContractRequest::ShowGhostContextMenu:   RequestShowGhostContextMenu_Implementation(Target, WorldPosition); break;
	case EMOUIContractRequest::ShowStationContextMenu: RequestShowStationContextMenu_Implementation(Target, WorldPosition); break;
	case EMOUIContractRequest::ShowWorldActionMenu:    RequestShowWorldActionMenu_Implementation(Target, WorldPosition); break;
	}
}

void AMOPlayerController::RequestShowBuildWidget_Implementation(AActor* BuildingActor)
{
	if (ForwardUIContractRequest(EMOUIContractRequest::ShowBuildWidget, BuildingActor))
	{
		return;
	}
	if (UIManagerComponent)
	{
		if (AMOBuildableActor* Buildable = Cast<AMOBuildableActor>(BuildingActor))
		{
			UIManagerComponent->ShowBuildWidget(Buildable);
		}
	}
}

void AMOPlayerController::RequestOpenContainerInventory_Implementation(AActor* ContainerActor)
{
	if (ForwardUIContractRequest(EMOUIContractRequest::OpenContainerInventory, ContainerActor))
	{
		return;
	}
	if (UIManagerComponent)
	{
		UIManagerComponent->OpenInventoryWithContainer(ContainerActor);
	}
}

void AMOPlayerController::RequestOpenCraftingMenu_Implementation(AActor* StationActor)
{
	if (ForwardUIContractRequest(EMOUIContractRequest::OpenCraftingMenu, StationActor))
	{
		return;
	}
	if (UIManagerComponent)
	{
		UIManagerComponent->OpenCraftingMenu();
	}
}

void AMOPlayerController::RequestShowGhostContextMenu_Implementation(AActor* GhostActor, FVector WorldPosition)
{
	if (ForwardUIContractRequest(EMOUIContractRequest::ShowGhostContextMenu, GhostActor, WorldPosition))
	{
		return;
	}
	// Ghost context menu is handled by ShowBuildWidget - it shows the context menu for ghost buildings
	if (UIManagerComponent)
	{
		if (AMOBuildableActor* Buildable = Cast<AMOBuildableActor>(GhostActor))
		{
			UIManagerComponent->ShowBuildWidget(Buildable);
		}
	}
}

void AMOPlayerController::RequestShowStationContextMenu_Implementation(AActor* StationActor, FVector WorldPosition)
{
	if (ForwardUIContractRequest(EMOUIContractRequest::ShowStationContextMenu, StationActor, WorldPosition))
	{
		return;
	}
	if (UIManagerComponent)
	{
		UIManagerComponent->ShowStationContextMenu(StationActor, WorldPosition);
	}
}

void AMOPlayerController::RequestShowWorldActionMenu_Implementation(AActor* TargetActor, FVector WorldPosition)
{
	if (ForwardUIContractRequest(EMOUIContractRequest::ShowWorldActionMenu, TargetActor, WorldPosition))
	{
		return;
	}
	if (UIManagerComponent)
	{
		UIManagerComponent->ShowWorldActionMenu(TargetActor, WorldPosition);
	}
}
