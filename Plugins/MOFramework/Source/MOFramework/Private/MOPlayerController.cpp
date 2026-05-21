#include "MOPlayerController.h"
#include "MOFramework.h"
#include "MOUIDebugSubsystem.h"
#include "MOControllableInterface.h"
#include "MOSpectatorPawn.h"
#include "MOCharacter.h"
#include "GameFramework/PawnMovementComponent.h"
#include "MOUIManagerComponent.h"
#include "MOPossessionComponent.h"
#include "MONotificationComponent.h"
#include "MOBuildingComponent.h"
#include "MOBuildableActor.h"
#include "MOCarcassActor.h"
#include "MOInteractorComponent.h"
#include "MOIdentityComponent.h"
#include "MOKeyBindingManager.h"
#include "MOGameUIManagerSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"

// UI Controllers
#include "MOInventoryUIController.h"
#include "MOCraftingUIController.h"
#include "MOBuildingUIController.h"
#include "MOCharacterUIController.h"
#include "MOSystemMenuUIController.h"
#include "MOQuestUIController.h"

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

	if (PauseAction)
	{
		EnhancedInput->BindAction(PauseAction, ETriggerEvent::Started, this, &AMOPlayerController::HandlePause);
	}

	if (CloseMenuAction)
	{
		EnhancedInput->BindAction(CloseMenuAction, ETriggerEvent::Started, this, &AMOPlayerController::HandleCloseMenu);
		UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Bound CloseMenuAction (Tab key)"));
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
}

void AMOPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

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
	// Clear cached pawn components before unpossessing
	if (UIManagerComponent)
	{
		UIManagerComponent->ClearCachedPawnComponents();
	}

	CachedControllablePawn.Reset();

	// Disable spectator input when unpossessing - camera stays fixed until a pawn is possessed
	bSpectatorInputEnabled = false;
	UE_LOG(LogMOFramework, Log, TEXT("AMOPlayerController: Unpossessed - spectator input disabled"));

	Super::OnUnPossess();
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

	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
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
			// Use unified target finding (supports ISM/HISM and actors)
			FMOInteractionTarget Target;
			if (Interactor->FindInteractionTarget(Target))
			{
				// Check if the target is a recruited survivor - if so, show context menu instead
				if (Target.TargetActor.IsValid())
				{
					if (APawn* TargetPawn = Cast<APawn>(Target.TargetActor.Get()))
					{
						// Don't show context menu for self
						if (TargetPawn != ControllablePawn)
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

		// No interactable target - check for ground hit for foraging menu
		// Use camera-based trace since cursor is hidden in FPS mode
		FVector CameraLocation;
		FRotator CameraRotation;
		GetPlayerViewPoint(CameraLocation, CameraRotation);

		const FVector TraceStart = CameraLocation;
		const FVector TraceEnd = CameraLocation + (CameraRotation.Vector() * 2000.0f);

		FHitResult GroundHit;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(ControllablePawn);

		if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
		{
			if (GroundHit.bBlockingHit)
			{
				// Use viewport center for screen position
				int32 ViewportSizeX, ViewportSizeY;
				GetViewportSize(ViewportSizeX, ViewportSizeY);
				FVector2D ScreenPosition(ViewportSizeX / 2.0f, ViewportSizeY / 2.0f);

				if (InventoryUIController)
				{
					InventoryUIController->ShowGroundContextMenu(GroundHit.Location, ScreenPosition);
					return;
				}
			}
		}

		// No ground hit or no UI controller - fall back to regular secondary action (block/aim)
		IMOControllableInterface::Execute_RequestSecondaryAction(ControllablePawn);
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

void AMOPlayerController::HandlePause(const FInputActionValue& Value)
{
	MOUI_LOG(this, "PC", "HandlePause FIRED via Enhanced Input");
	if (UIManagerComponent)
	{
		UIManagerComponent->ToggleInGameMenu();
	}
}

void AMOPlayerController::HandleCloseMenu(const FInputActionValue& Value)
{
	MOUI_LOG(this, "PC", "HandleCloseMenu FIRED via Enhanced Input");
	// Tab key behavior:
	// - If any menu is open: close it (handled by widget's NativeOnPreviewKeyDown in Menu mode)
	// - If no menu is open: open in-game menu (this path fires via Enhanced Input in Game mode)
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
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("AMOPlayerController::HandlePossess - UIManagerComponent is NULL!"));
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
		}
	}
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

void AMOPlayerController::RequestShowBuildWidget_Implementation(AActor* BuildingActor)
{
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
	if (UIManagerComponent)
	{
		UIManagerComponent->OpenInventoryWithContainer(ContainerActor);
	}
}

void AMOPlayerController::RequestOpenCraftingMenu_Implementation(AActor* StationActor)
{
	if (UIManagerComponent)
	{
		UIManagerComponent->OpenCraftingMenu();
	}
}

void AMOPlayerController::RequestShowGhostContextMenu_Implementation(AActor* GhostActor, FVector WorldPosition)
{
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
	if (UIManagerComponent)
	{
		UIManagerComponent->ShowStationContextMenu(StationActor, WorldPosition);
	}
}

void AMOPlayerController::RequestStartCarcassButchering_Implementation(AActor* CarcassActor, FName PartId)
{
	if (UIManagerComponent)
	{
		if (AMOCarcassActor* Carcass = Cast<AMOCarcassActor>(CarcassActor))
		{
			UIManagerComponent->StartCarcassButchering(Carcass, PartId);
		}
	}
}
