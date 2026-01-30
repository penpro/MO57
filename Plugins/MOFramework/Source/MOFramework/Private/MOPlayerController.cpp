#include "MOPlayerController.h"
#include "MOControllableInterface.h"
#include "MOUIManagerComponent.h"
#include "MOPossessionComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"

AMOPlayerController::AMOPlayerController()
{
	// Create UI Manager component
	UIManagerComponent = CreateDefaultSubobject<UMOUIManagerComponent>(TEXT("UIManagerComponent"));

	// Create Possession component
	PossessionComponent = CreateDefaultSubobject<UMOPossessionComponent>(TEXT("PossessionComponent"));
}

void AMOPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("AMOPlayerController::BeginPlay - Setting up input context"));

	// Setup default input context
	SetupDefaultInputContext();

	// Setup debug bindings (uses legacy input, always available)
	SetupDebugInputBindings();
}

void AMOPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UE_LOG(LogTemp, Log, TEXT("AMOPlayerController::SetupInputComponent called"));

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Warning, TEXT("AMOPlayerController: InputComponent is not UEnhancedInputComponent!"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AMOPlayerController: EnhancedInputComponent is valid"));

	// ============================================================================
	// MOVEMENT BINDINGS
	// ============================================================================

	if (UInputAction* Move = MoveAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Move, ETriggerEvent::Triggered, this, &AMOPlayerController::HandleMove);
		UE_LOG(LogTemp, Log, TEXT("AMOPlayerController: Bound MoveAction"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AMOPlayerController: MoveAction is NOT set!"));
	}

	if (UInputAction* Look = LookAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Look, ETriggerEvent::Triggered, this, &AMOPlayerController::HandleLook);
		UE_LOG(LogTemp, Log, TEXT("AMOPlayerController: Bound LookAction"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AMOPlayerController: LookAction is NOT set!"));
	}

	if (UInputAction* Jump = JumpAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Jump, ETriggerEvent::Started, this, &AMOPlayerController::HandleJumpStart);
		EnhancedInput->BindAction(Jump, ETriggerEvent::Completed, this, &AMOPlayerController::HandleJumpEnd);
	}

	if (UInputAction* Hustle = HustleAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Hustle, ETriggerEvent::Started, this, &AMOPlayerController::HandleHustleStart);
		EnhancedInput->BindAction(Hustle, ETriggerEvent::Triggered, this, &AMOPlayerController::HandleHustleTriggered);
		EnhancedInput->BindAction(Hustle, ETriggerEvent::Completed, this, &AMOPlayerController::HandleHustleEnd);
		UE_LOG(LogTemp, Log, TEXT("AMOPlayerController: Bound HustleAction (tap=jog, hold=sprint)"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AMOPlayerController: HustleAction is NOT set!"));
	}

	if (UInputAction* Crouch = CrouchAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Crouch, ETriggerEvent::Started, this, &AMOPlayerController::HandleCrouch);
	}

	// ============================================================================
	// ACTION BINDINGS
	// ============================================================================

	if (UInputAction* Interact = InteractAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Interact, ETriggerEvent::Started, this, &AMOPlayerController::HandleInteract);
	}

	if (UInputAction* Primary = PrimaryAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Primary, ETriggerEvent::Started, this, &AMOPlayerController::HandlePrimaryAction);
		EnhancedInput->BindAction(Primary, ETriggerEvent::Completed, this, &AMOPlayerController::HandlePrimaryActionRelease);
	}

	if (UInputAction* Secondary = SecondaryAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Secondary, ETriggerEvent::Started, this, &AMOPlayerController::HandleSecondaryAction);
		EnhancedInput->BindAction(Secondary, ETriggerEvent::Completed, this, &AMOPlayerController::HandleSecondaryActionRelease);
	}

	// ============================================================================
	// UI/SYSTEM BINDINGS
	// ============================================================================

	if (UInputAction* Inventory = InventoryAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Inventory, ETriggerEvent::Started, this, &AMOPlayerController::HandleInventory);
	}

	if (UInputAction* PlayerStatus = PlayerStatusAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(PlayerStatus, ETriggerEvent::Started, this, &AMOPlayerController::HandlePlayerStatus);
	}

	if (UInputAction* Pause = PauseAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Pause, ETriggerEvent::Started, this, &AMOPlayerController::HandlePause);
	}

	if (UInputAction* Possess = PossessAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Possess, ETriggerEvent::Started, this, &AMOPlayerController::HandlePossess);
	}
}

void AMOPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Cache the pawn if it implements the controllable interface
	if (InPawn && InPawn->Implements<UMOControllableInterface>())
	{
		CachedControllablePawn = InPawn;
		UE_LOG(LogTemp, Log, TEXT("AMOPlayerController: Possessed %s - Implements IMOControllableInterface: YES"),
			*InPawn->GetName());
	}
	else
	{
		CachedControllablePawn.Reset();
		UE_LOG(LogTemp, Warning, TEXT("AMOPlayerController: Possessed %s - Implements IMOControllableInterface: NO (inputs will not work!)"),
			InPawn ? *InPawn->GetName() : TEXT("None"));

		if (InPawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Pawn class: %s - Make sure it inherits from AMOCharacter or implements IMOControllableInterface"),
				*InPawn->GetClass()->GetName());
		}
	}
}

void AMOPlayerController::OnUnPossess()
{
	CachedControllablePawn.Reset();
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
		MappingContext = PawnControlContext.LoadSynchronous();
		Priority = PawnControlContextPriority;
		break;

	case EMOInputContext::BaseBuilding:
		MappingContext = BaseBuildingContext.LoadSynchronous();
		Priority = BaseBuildingContextPriority;
		break;

	case EMOInputContext::Menu:
		MappingContext = MenuContext.LoadSynchronous();
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
		MappingContext = PawnControlContext.LoadSynchronous();
		break;

	case EMOInputContext::BaseBuilding:
		MappingContext = BaseBuildingContext.LoadSynchronous();
		break;

	case EMOInputContext::Menu:
		MappingContext = MenuContext.LoadSynchronous();
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
		MappingContext = PawnControlContext.Get();
		break;

	case EMOInputContext::BaseBuilding:
		MappingContext = BaseBuildingContext.Get();
		break;

	case EMOInputContext::Menu:
		MappingContext = MenuContext.Get();
		break;

	default:
		return false;
	}

	return MappingContext && Subsystem->HasMappingContext(MappingContext);
}

void AMOPlayerController::SetupDefaultInputContext()
{
	// Add default pawn control context
	if (UInputMappingContext* Context = PawnControlContext.LoadSynchronous())
	{
		AddMappingContext(Context, PawnControlContextPriority);
		UE_LOG(LogTemp, Log, TEXT("AMOPlayerController: Added PawnControlContext mapping context"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AMOPlayerController: PawnControlContext is NOT set! No input mapping context active."));
	}
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
		UE_LOG(LogTemp, Warning, TEXT("AMOPlayerController::HandleMove - No CachedControllablePawn! Current Pawn: %s"),
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
	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestJumpStart(ControllablePawn);
	}
}

void AMOPlayerController::HandleJumpEnd(const FInputActionValue& Value)
{
	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestJumpEnd(ControllablePawn);
	}
}

void AMOPlayerController::HandleHustleStart(const FInputActionValue& Value)
{
	// Record when hustle was pressed for tap/hold detection
	HustlePressTime = GetWorld()->GetTimeSeconds();
	bHustleHoldTriggered = false;
}

void AMOPlayerController::HandleHustleTriggered(const FInputActionValue& Value)
{
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
	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestInteract(ControllablePawn);
	}
}

void AMOPlayerController::HandlePrimaryAction(const FInputActionValue& Value)
{
	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestPrimaryAction(ControllablePawn);
	}
}

void AMOPlayerController::HandlePrimaryActionRelease(const FInputActionValue& Value)
{
	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestPrimaryActionRelease(ControllablePawn);
	}
}

void AMOPlayerController::HandleSecondaryAction(const FInputActionValue& Value)
{
	if (APawn* ControllablePawn = CachedControllablePawn.Get())
	{
		IMOControllableInterface::Execute_RequestSecondaryAction(ControllablePawn);
	}
}

void AMOPlayerController::HandleSecondaryActionRelease(const FInputActionValue& Value)
{
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
	if (UIManagerComponent)
	{
		UIManagerComponent->ToggleInventoryMenu();
	}
}

void AMOPlayerController::HandlePlayerStatus(const FInputActionValue& Value)
{
	if (UIManagerComponent)
	{
		UIManagerComponent->TogglePlayerStatus();
	}
}

void AMOPlayerController::HandlePause(const FInputActionValue& Value)
{
	if (UIManagerComponent)
	{
		UIManagerComponent->ToggleInGameMenu();
	}
}

void AMOPlayerController::HandlePossess(const FInputActionValue& Value)
{
	if (PossessionComponent)
	{
		PossessionComponent->TryPossessNearestPawn();
	}
}

// ============================================================================
// DEBUG INPUT HANDLERS
// ============================================================================

void AMOPlayerController::SetupDebugInputBindings()
{
	if (!InputComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("AMOPlayerController: Cannot setup debug bindings - no InputComponent"));
		return;
	}

	// Bind debug keys using legacy input (always works, no input actions needed)
	InputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &AMOPlayerController::HandleDebugSpawnPawn);
	InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AMOPlayerController::HandleDebugToggle);

	UE_LOG(LogTemp, Log, TEXT("AMOPlayerController: Debug bindings set up (0=SpawnPawn, F1=ToggleDebug)"));
}

void AMOPlayerController::HandleDebugSpawnPawn()
{
	if (!bDebugModeEnabled)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AMOPlayerController: Debug spawn pawn triggered"));

	if (!DebugSpawnPawnClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("AMOPlayerController: DebugSpawnPawnClass is not set! Set it in the Blueprint."));

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
				TEXT("Debug spawn failed: DebugSpawnPawnClass not set in BP_MOPlayerController"));
		}
		return;
	}

	if (PossessionComponent)
	{
		// Spawn the pawn and possess it
		bool bSuccess = PossessionComponent->TrySpawnAndPossessPawn(
			DebugSpawnPawnClass,
			DebugSpawnDistance,
			FVector::ZeroVector,
			true
		);

		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("AMOPlayerController: Spawning and possessing debug pawn of class %s"), *DebugSpawnPawnClass->GetName());

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
					FString::Printf(TEXT("Spawned: %s"), *DebugSpawnPawnClass->GetName()));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AMOPlayerController: Failed to spawn debug pawn"));

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
					TEXT("Failed to spawn debug pawn"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AMOPlayerController: No PossessionComponent available"));
	}
}

void AMOPlayerController::HandleDebugToggle()
{
	bDebugModeEnabled = !bDebugModeEnabled;
	UE_LOG(LogTemp, Log, TEXT("AMOPlayerController: Debug mode %s"), bDebugModeEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));

	// Could add on-screen message here
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, bDebugModeEnabled ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("Debug Mode: %s"), bDebugModeEnabled ? TEXT("ON") : TEXT("OFF")));
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
