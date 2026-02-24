#include "MOCharacter.h"
#include "MOFramework.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "MOIdentityComponent.h"
#include "MOInteractorComponent.h"
#include "MOInventoryComponent.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "MOSurvivalStatsComponent.h"
#include "MOVitalsComponent.h"
#include "MOMetabolismComponent.h"
#include "MOMentalStateComponent.h"
#include "MOAnatomyComponent.h"
#include "MOAdrenalineComponent.h"
#include "MOCraftingQueueComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOEquipmentComponent.h"
#include "MORecruitmentComponent.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOInteractableComponent.h"
#include "MONotificationComponent.h"
#include "MOCraftingTypes.h"
#include "MOTerraformingComponent.h"
#include "MOSurvivorController.h"
#include "AIController.h"
#include "MOUIManagerComponent.h"
#include "MOModeIndicatorWidget.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOGameSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "NavigationInvokerComponent.h"
#include "Engine/DataTable.h"
#include "Perception/AISense_Hearing.h"
#include "VoxelWorld.h"

AMOCharacter::AMOCharacter()
{
	// Capsule defaults (same as ThirdPerson template)
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates (let movement handle it)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Camera boom (spring arm)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	// Keep SocketOffset zeroed - shoulder offset is handled via FollowCamera relative location

	// Follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	// Default to right shoulder view - toggled via ToggleCameraShoulder()
	FollowCamera->SetRelativeLocation(FVector(0.0f, 80.0f, 0.0f));

	// Movement defaults (same as ThirdPerson template)
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// MO Components - Core Identity
	IdentityComponent = CreateDefaultSubobject<UMOIdentityComponent>(TEXT("IdentityComponent"));

	// MO Components - Inventory & Crafting
	InventoryComponent = CreateDefaultSubobject<UMOInventoryComponent>(TEXT("InventoryComponent"));
	CraftingQueueComponent = CreateDefaultSubobject<UMOCraftingQueueComponent>(TEXT("CraftingQueueComponent"));
	RecipeDiscoveryComponent = CreateDefaultSubobject<UMORecipeDiscoveryComponent>(TEXT("RecipeDiscoveryComponent"));

	// MO Components - Interaction
	InteractorComponent = CreateDefaultSubobject<UMOInteractorComponent>(TEXT("InteractorComponent"));

	// MO Components - Skills & Knowledge
	SkillsComponent = CreateDefaultSubobject<UMOSkillsComponent>(TEXT("SkillsComponent"));
	KnowledgeComponent = CreateDefaultSubobject<UMOKnowledgeComponent>(TEXT("KnowledgeComponent"));

	// MO Components - Survival Stats & Body Systems
	SurvivalStatsComponent = CreateDefaultSubobject<UMOSurvivalStatsComponent>(TEXT("SurvivalStatsComponent"));
	VitalsComponent = CreateDefaultSubobject<UMOVitalsComponent>(TEXT("VitalsComponent"));
	MetabolismComponent = CreateDefaultSubobject<UMOMetabolismComponent>(TEXT("MetabolismComponent"));
	MentalStateComponent = CreateDefaultSubobject<UMOMentalStateComponent>(TEXT("MentalStateComponent"));
	AnatomyComponent = CreateDefaultSubobject<UMOAnatomyComponent>(TEXT("AnatomyComponent"));
	AdrenalineComponent = CreateDefaultSubobject<UMOAdrenalineComponent>(TEXT("AdrenalineComponent"));

	// MO Components - Terraforming
	TerraformingComponent = CreateDefaultSubobject<UMOTerraformingComponent>(TEXT("TerraformingComponent"));

	// MO Components - Equipment
	EquipmentComponent = CreateDefaultSubobject<UMOEquipmentComponent>(TEXT("EquipmentComponent"));

	// MO Components - Recruitment
	RecruitmentComponent = CreateDefaultSubobject<UMORecruitmentComponent>(TEXT("RecruitmentComponent"));

	// MO Components - Survivor Job Queue (for task assignment when recruited)
	JobQueueComponent = CreateDefaultSubobject<UMOSurvivorJobQueueComponent>(TEXT("JobQueueComponent"));

	// MO Components - Interactable (allows other pawns to interact with this character)
	InteractableComponent = CreateDefaultSubobject<UMOInteractableComponent>(TEXT("InteractableComponent"));

	// Interaction collision sphere - blocks ECC_Visibility traces for interaction system
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(GetCapsuleComponent());
	InteractionSphere->SetSphereRadius(50.0f);
	InteractionSphere->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f)); // Centered on torso
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetGenerateOverlapEvents(false);

	// Navigation invoker - tells voxel world to generate navmesh around this character
	NavigationInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavigationInvoker"));
	NavigationInvoker->SetGenerationRadii(3000.f, 5000.f); // Generate within 30m, remove beyond 50m

	// Held item mesh components (attached to hand sockets)
	LeftHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftHandMesh"));
	LeftHandMesh->SetupAttachment(GetMesh(), LeftHandSocketName);
	LeftHandMesh->SetVisibility(false);
	LeftHandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RightHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightHandMesh"));
	RightHandMesh->SetupAttachment(GetMesh(), RightHandSocketName);
	RightHandMesh->SetVisibility(false);
	RightHandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Default mesh position (mesh itself loaded in BeginPlay if DefaultMesh is set)
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	// Default movement speeds
	WalkSpeed = 400.0f;
	SprintSpeed = 700.0f;

	// NOTE: Mesh and animation are intentionally left unset for portability.
	// Set these in child Blueprints to avoid hard-coded asset paths.
}

void AMOCharacter::BeginPlay()
{
	// Load and apply default mesh if not already set
	if (!GetMesh()->GetSkeletalMeshAsset())
	{
		if (DefaultMesh.IsNull())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s: No DefaultMesh configured. Set 'Default Mesh' in the Blueprint or leave skeletal mesh component empty."), *GetName());
		}
		else if (USkeletalMesh* LoadedMesh = DefaultMesh.LoadSynchronous())
		{
			GetMesh()->SetSkeletalMesh(LoadedMesh);
		}
	}

	// Load and apply default animation blueprint if not already set
	if (!GetMesh()->GetAnimInstance() && !GetMesh()->GetAnimClass())
	{
		if (!DefaultAnimBlueprint.IsNull())
		{
			if (UClass* AnimClass = DefaultAnimBlueprint.LoadSynchronous())
			{
				GetMesh()->SetAnimInstanceClass(AnimClass);
			}
		}
	}

	// Apply initial walk speed
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}

	// Apply FOV and sensitivity from game settings
	ApplyGameSettings();

	// Start movement physiology tracking
	StartMovementPhysiologyTracking();

	// Bind knowledge learning to recipe discovery
	if (KnowledgeComponent && RecipeDiscoveryComponent)
	{
		KnowledgeComponent->OnKnowledgeLearned.AddDynamic(this, &AMOCharacter::HandleKnowledgeLearned);
	}

	// Bind skill level up to recipe discovery
	if (SkillsComponent && RecipeDiscoveryComponent)
	{
		SkillsComponent->OnSkillLevelUp.AddDynamic(this, &AMOCharacter::HandleSkillLevelUp);
	}

	// Bind recipe discovery to notifications
	if (RecipeDiscoveryComponent)
	{
		RecipeDiscoveryComponent->OnRecipeDiscovered.AddDynamic(this, &AMOCharacter::HandleRecipeDiscovered);
	}

	// Bind equipment changes to held item visualization
	if (EquipmentComponent)
	{
		EquipmentComponent->OnEquipmentChanged.AddDynamic(this, &AMOCharacter::HandleEquipmentChanged);

		// Update held items for any already-equipped items (e.g., from save load)
		UpdateHeldItemMesh(EMOEquipmentSlot::LeftHand, EquipmentComponent->GetEquippedItem(EMOEquipmentSlot::LeftHand));
		UpdateHeldItemMesh(EMOEquipmentSlot::RightHand, EquipmentComponent->GetEquippedItem(EMOEquipmentSlot::RightHand));
	}

	// Bind interactable component to recruitment system
	if (InteractableComponent && RecruitmentComponent)
	{
		InteractableComponent->OnHandleInteract.BindUObject(this, &AMOCharacter::HandleRecruitmentInteraction);
	}

	Super::BeginPlay();
}

void AMOCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopMovementPhysiologyTracking();
	Super::EndPlay(EndPlayReason);
}

void AMOCharacter::UnPossessed()
{
	Super::UnPossessed();

	// When player unpossesses this pawn, spawn survivor AI controller if this is a recruited survivor
	if (!HasAuthority())
	{
		return;
	}

	// Don't spawn during world teardown
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	// Check if this is a recruited survivor
	if (!RecruitmentComponent || !RecruitmentComponent->IsPossessable())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: UnPossessed - not a possessable survivor, no AI controller needed"), *GetName());
		return;
	}

	// Only spawn AI controller if we don't already have one
	if (GetController() != nullptr)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: UnPossessed - already has controller %s"), *GetName(), *GetController()->GetName());
		return;
	}

	// Use the pawn's AIControllerClass if it's a survivor controller, otherwise use default
	TSubclassOf<AController> ControllerClass = AIControllerClass;
	if (!ControllerClass || !ControllerClass->IsChildOf(AMOSurvivorController::StaticClass()))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: UnPossessed - using default AMOSurvivorController (AIControllerClass was %s)"),
			*GetName(), ControllerClass ? *ControllerClass->GetName() : TEXT("None"));
		ControllerClass = AMOSurvivorController::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAIController* NewController = World->SpawnActor<AAIController>(ControllerClass, GetActorLocation(), GetActorRotation(), SpawnParams);
	if (NewController)
	{
		NewController->Possess(this);
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: UnPossessed - spawned and possessed by %s"),
			*GetName(), *NewController->GetName());
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s: UnPossessed - failed to spawn AI controller"), *GetName());
	}
}

void AMOCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Check for falling through the world
	if (bEnableFallThroughSafety)
	{
		CheckFallThroughSafety(DeltaTime);
	}

	// Generate movement noise for AI perception
	if (bGeneratesMovementNoise)
	{
		TimeSinceLastNoise += DeltaTime;

		// Check if character is moving
		const float Speed = GetVelocity().Size();
		if (Speed > 10.f && TimeSinceLastNoise >= NoiseInterval)
		{
			TimeSinceLastNoise = 0.f;

			// Determine loudness based on movement mode
			float Loudness = WalkingNoiseLoudness;
			if (bIsSprinting)
			{
				Loudness = SprintingNoiseLoudness;
			}
			else if (CurrentMovementMode == EMOMovementMode::Jogging)
			{
				Loudness = JoggingNoiseLoudness;
			}

			// Report noise event to AI perception system
			// MaxRange of 0 means use the loudness to determine range
			MakeNoise(Loudness, this, GetActorLocation());
		}
	}

	// Handle smooth camera shoulder transition
	if (bIsCameraTransitioning && FollowCamera)
	{
		CameraTransitionAlpha += DeltaTime / FMath::Max(CameraTransitionDuration, 0.01f);

		if (CameraTransitionAlpha >= 1.0f)
		{
			// Transition complete
			CameraTransitionAlpha = 1.0f;
			bIsCameraTransitioning = false;

			FVector FinalLocation = FollowCamera->GetRelativeLocation();
			FinalLocation.Y = CameraTargetY;
			FollowCamera->SetRelativeLocation(FinalLocation);
		}
		else
		{
			// Smooth interpolation using ease-out curve
			const float EasedAlpha = 1.0f - FMath::Pow(1.0f - CameraTransitionAlpha, 2.0f);
			const float NewY = FMath::Lerp(CameraStartY, CameraTargetY, EasedAlpha);

			FVector CurrentLocation = FollowCamera->GetRelativeLocation();
			CurrentLocation.Y = NewY;
			FollowCamera->SetRelativeLocation(CurrentLocation);
		}
	}
}

// ============================================================================
// IMOControllableInterface IMPLEMENTATION
// ============================================================================

void AMOCharacter::RequestMove_Implementation(FVector2D MovementInput)
{
	if (!CanMove_Implementation())
	{
		return;
	}

	if (Controller)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementInput.Y);
		AddMovementInput(RightDirection, MovementInput.X);
	}
}

void AMOCharacter::RequestLook_Implementation(FVector2D LookInput)
{
	if (!CanBeControlled_Implementation())
	{
		return;
	}

	if (Controller)
	{
		// Apply sensitivity
		float YawInput = LookInput.X * LookSensitivity;
		float PitchInput = LookInput.Y * LookSensitivity;

		// Check InvertY setting
		if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
		{
			if (Settings->bInvertYAxis)
			{
				PitchInput = -PitchInput;
			}
		}

		AddControllerYawInput(YawInput);
		AddControllerPitchInput(PitchInput);
	}
}

void AMOCharacter::RequestJumpStart_Implementation()
{
	if (CanJump_Implementation())
	{
		Jump();
	}
}

void AMOCharacter::RequestJumpEnd_Implementation()
{
	StopJumping();
}

void AMOCharacter::RequestSprintStart_Implementation()
{
	// Sprint is now handled via StartSprint() from hustle hold
	StartSprint();
}

void AMOCharacter::RequestSprintEnd_Implementation()
{
	// Sprint release is now handled via StopSprint()
	StopSprint();
}

void AMOCharacter::RequestToggleJog_Implementation()
{
	ToggleJog();
}

void AMOCharacter::RequestCrouchToggle_Implementation()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AMOCharacter::RequestInteract_Implementation()
{
	if (InteractorComponent)
	{
		InteractorComponent->TryInteract();
	}
}

void AMOCharacter::RequestSecondaryInteract_Implementation()
{
	if (InteractorComponent)
	{
		InteractorComponent->TrySecondaryInteract();
	}
}

void AMOCharacter::RequestPrimaryAction_Implementation()
{
	// If in terraforming mode, perform terraform action
	if (bTerraformingMode)
	{
		DoTerraform();
		return;
	}

	// Override in subclasses for weapon/tool use
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] RequestPrimaryAction - Override in subclass"));
}

void AMOCharacter::RequestPrimaryActionRelease_Implementation()
{
	// Override in subclasses
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] RequestPrimaryActionRelease - Override in subclass"));
}

void AMOCharacter::RequestSecondaryAction_Implementation()
{
	// Override in subclasses for block/aim/alt-use
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] RequestSecondaryAction - Override in subclass"));
}

void AMOCharacter::RequestSecondaryActionRelease_Implementation()
{
	// Override in subclasses
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] RequestSecondaryActionRelease - Override in subclass"));
}

void AMOCharacter::RequestTerraformToggle_Implementation()
{
	ToggleTerraformMode();
}

void AMOCharacter::RequestTerraformCycleTool_Implementation()
{
	CycleTerraformTool();
}

bool AMOCharacter::CanBeControlled_Implementation() const
{
	// Check if controllable and not pending kill
	return bCanBeControlled && !IsPendingKillPending();
}

bool AMOCharacter::CanMove_Implementation() const
{
	// Can move if controllable and has valid movement component
	return CanBeControlled_Implementation() && GetCharacterMovement() != nullptr;
}

bool AMOCharacter::CanJump_Implementation() const
{
	return CanBeControlled_Implementation() && ACharacter::CanJump();
}

bool AMOCharacter::CanSprint_Implementation() const
{
	if (!CanMove_Implementation() || bIsCrouched)
	{
		return false;
	}

	// Check stamina requirement
	if (SurvivalStatsComponent)
	{
		float CurrentStamina = SurvivalStatsComponent->GetStatCurrent(TEXT("Stamina"));
		if (CurrentStamina < MinStaminaToSprint)
		{
			return false;
		}
	}

	return true;
}

// ============================================================================
// DIRECT INPUT (for Blueprint/UI)
// ============================================================================

void AMOCharacter::DoMove(float Right, float Forward)
{
	RequestMove_Implementation(FVector2D(Right, Forward));
}

void AMOCharacter::DoLook(float Yaw, float Pitch)
{
	RequestLook_Implementation(FVector2D(Yaw, Pitch));
}

void AMOCharacter::DoJumpStart()
{
	RequestJumpStart_Implementation();
}

void AMOCharacter::DoJumpEnd()
{
	RequestJumpEnd_Implementation();
}

// ============================================================================
// GAME SETTINGS
// ============================================================================

void AMOCharacter::ApplyGameSettings()
{
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (!Settings)
	{
		return;
	}

	// Apply FOV to camera
	if (FollowCamera)
	{
		FollowCamera->SetFieldOfView(Settings->FieldOfView);
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] Applied FOV: %.1f"), Settings->FieldOfView);
	}

	// Store sensitivity for use in look input
	LookSensitivity = Settings->CameraSensitivity;
}

void AMOCharacter::ToggleCameraShoulder()
{
	if (!FollowCamera)
	{
		return;
	}

	// Get current Y position (use target if already transitioning)
	const FVector CurrentLocation = FollowCamera->GetRelativeLocation();
	CameraStartY = bIsCameraTransitioning ? FMath::Lerp(CameraStartY, CameraTargetY, CameraTransitionAlpha) : CurrentLocation.Y;

	// Calculate new target: flip the sign
	CameraTargetY = -CameraStartY;

	// If the offset was 0, use the default shoulder offset
	if (FMath::IsNearlyZero(CameraTargetY))
	{
		CameraTargetY = CameraShoulderOffset;
	}

	// Start the transition
	CameraTransitionAlpha = 0.0f;
	bIsCameraTransitioning = true;

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] Camera shoulder transitioning from %.1f to %s (Y=%.1f)"),
		CameraStartY,
		CameraTargetY > 0 ? TEXT("right") : TEXT("left"),
		CameraTargetY);
}

// ============================================================================
// APPEARANCE
// ============================================================================

void AMOCharacter::SetCharacterMesh(USkeletalMesh* NewMesh)
{
	if (NewMesh && GetMesh())
	{
		GetMesh()->SetSkeletalMesh(NewMesh);
	}
}

void AMOCharacter::SetAnimationBlueprint(TSubclassOf<UAnimInstance> NewAnimClass)
{
	if (NewAnimClass && GetMesh())
	{
		GetMesh()->SetAnimInstanceClass(NewAnimClass);
	}
}

// ============================================================================
// MOVEMENT MODE SYSTEM
// ============================================================================

void AMOCharacter::SetMovementMode(EMOMovementMode NewMode)
{
	if (CurrentMovementMode == NewMode)
	{
		return;
	}

	CurrentMovementMode = NewMode;
	bIsSprinting = (NewMode == EMOMovementMode::Sprinting);
	UpdateMovementSpeed();

	// Get actual speed for logging
	float ActualSpeed = 0.0f;
	if (GetCharacterMovement())
	{
		ActualSpeed = GetCharacterMovement()->MaxWalkSpeed;
	}

	const FString ModeName = NewMode == EMOMovementMode::Walking ? TEXT("Walking") :
							 NewMode == EMOMovementMode::Jogging ? TEXT("Jogging") : TEXT("Sprinting");

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: Movement mode -> %s (Speed: %.0f)"),
		*GetName(), *ModeName, ActualSpeed);

	// On-screen debug message
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		const FColor ModeColor = NewMode == EMOMovementMode::Walking ? FColor::White :
								 NewMode == EMOMovementMode::Jogging ? FColor::Yellow : FColor::Orange;
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, ModeColor,
			FString::Printf(TEXT("%s: %.0f"), *ModeName, ActualSpeed));
	}
#endif
}

void AMOCharacter::ToggleJog()
{
	if (!CanMove_Implementation())
	{
		return;
	}

	// If currently sprinting, don't toggle jog
	if (bIsSprinting)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: Cannot toggle jog while sprinting"), *GetName());
		return;
	}

	bJogToggled = !bJogToggled;

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: Jog toggled %s"), *GetName(), bJogToggled ? TEXT("ON") : TEXT("OFF"));

	if (bJogToggled)
	{
		SetMovementMode(EMOMovementMode::Jogging);
	}
	else
	{
		SetMovementMode(EMOMovementMode::Walking);
	}
}

void AMOCharacter::StartSprint()
{
	if (!CanSprint_Implementation())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: Cannot sprint (CanSprint returned false)"), *GetName());
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: Sprint START (hold detected)"), *GetName());
	bIsSprinting = true;
	SetMovementMode(EMOMovementMode::Sprinting);
}

void AMOCharacter::StopSprint()
{
	if (!bIsSprinting)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: Sprint STOP (returning to %s)"),
		*GetName(), bJogToggled ? TEXT("Jogging") : TEXT("Walking"));

	bIsSprinting = false;

	// Return to previous mode (jog if toggled, walk otherwise)
	if (bJogToggled)
	{
		SetMovementMode(EMOMovementMode::Jogging);
	}
	else
	{
		SetMovementMode(EMOMovementMode::Walking);
	}
}

void AMOCharacter::UpdateMovementSpeed()
{
	if (!GetCharacterMovement())
	{
		return;
	}

	switch (CurrentMovementMode)
	{
	case EMOMovementMode::Walking:
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		break;
	case EMOMovementMode::Jogging:
		GetCharacterMovement()->MaxWalkSpeed = JogSpeed;
		break;
	case EMOMovementMode::Sprinting:
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
		break;
	}
}

float AMOCharacter::GetCurrentMET() const
{
	switch (CurrentMovementMode)
	{
	case EMOMovementMode::Jogging:
		return JoggingMET;
	case EMOMovementMode::Sprinting:
		return SprintingMET;
	case EMOMovementMode::Walking:
	default:
		return WalkingMET;
	}
}

float AMOCharacter::GetCurrentExertionLevel() const
{
	switch (CurrentMovementMode)
	{
	case EMOMovementMode::Jogging:
		return JoggingExertion;
	case EMOMovementMode::Sprinting:
		return SprintingExertion;
	case EMOMovementMode::Walking:
	default:
		return WalkingExertion;
	}
}

float AMOCharacter::GetCurrentTempRiseRate() const
{
	switch (CurrentMovementMode)
	{
	case EMOMovementMode::Jogging:
		return JoggingTempRisePerSec;
	case EMOMovementMode::Sprinting:
		return SprintingTempRisePerSec;
	case EMOMovementMode::Walking:
	default:
		return WalkingTempRisePerSec;
	}
}

void AMOCharacter::StartMovementPhysiologyTracking()
{
	// Set up a timer to apply physiological effects every 0.5 seconds
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			MovementPhysiologyTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				ApplyMovementPhysiologyEffects(0.5f);
			}),
			0.5f,
			true
		);
	}
}

void AMOCharacter::StopMovementPhysiologyTracking()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MovementPhysiologyTimerHandle);
	}
}

void AMOCharacter::ApplyMovementPhysiologyEffects(float DeltaTime)
{
	// Only apply effects if we're actually moving
	if (!GetCharacterMovement() || GetVelocity().Size2D() < 10.0f)
	{
		// Not moving - set exertion to resting
		if (VitalsComponent)
		{
			// Gradually return to resting exertion (handled by vitals component)
			VitalsComponent->SetExertionLevel(0.0f);
		}
		return;
	}

	// Get body weight for calorie calculation
	float BodyWeightKg = 75.0f; // Default
	if (MetabolismComponent)
	{
		BodyWeightKg = MetabolismComponent->BodyComposition.TotalWeight;
	}

	// Calculate calories burned: MET × weight(kg) × 0.0175 × time(minutes)
	const float CurrentMET = GetCurrentMET();
	const float CaloriesPerMinute = CurrentMET * BodyWeightKg * 0.0175f;
	const float CaloriesBurned = CaloriesPerMinute * (DeltaTime / 60.0f);

	// Apply to metabolism
	if (MetabolismComponent)
	{
		MetabolismComponent->ApplyCalorieBurn(CaloriesBurned);

		// Also count as cardio training if jogging or sprinting
		if (CurrentMovementMode != EMOMovementMode::Walking)
		{
			float Intensity = (CurrentMovementMode == EMOMovementMode::Sprinting) ? 0.9f : 0.5f;
			MetabolismComponent->ApplyCardioTraining(Intensity, DeltaTime);
		}
	}

	// Apply to vitals (exertion, temperature)
	if (VitalsComponent)
	{
		// Set exertion level
		VitalsComponent->SetExertionLevel(GetCurrentExertionLevel());

		// Apply temperature increase
		// Note: The vitals component handles temperature regulation,
		// we just add heat generation from exercise
		float TempRise = GetCurrentTempRiseRate() * DeltaTime;
		VitalsComponent->Vitals.BodyTemperature = FMath::Min(VitalsComponent->Vitals.BodyTemperature + TempRise, 40.0f);
	}

	// ============================================================================
	// STAMINA CONSUMPTION
	// ============================================================================
	if (SurvivalStatsComponent)
	{
		float StaminaCost = 0.0f;
		switch (CurrentMovementMode)
		{
		case EMOMovementMode::Jogging:
			StaminaCost = JoggingStaminaCostPerSecond * DeltaTime;
			break;
		case EMOMovementMode::Sprinting:
			StaminaCost = SprintingStaminaCostPerSecond * DeltaTime;
			break;
		default:
			break;
		}

		if (StaminaCost > 0.0f)
		{
			SurvivalStatsComponent->ModifyStat(TEXT("Stamina"), -StaminaCost);

			// Auto-stop sprinting if stamina depleted
			if (bIsSprinting && SurvivalStatsComponent->GetStatCurrent(TEXT("Stamina")) <= 0.0f)
			{
				StopSprint();
			}
		}
	}

	// ============================================================================
	// ATHLETICS SKILL XP
	// ============================================================================
	if (SkillsComponent && !AthleticsSkillId.IsNone())
	{
		float XPGained = 0.0f;
		switch (CurrentMovementMode)
		{
		case EMOMovementMode::Walking:
			XPGained = WalkingXPPerSecond * DeltaTime;
			break;
		case EMOMovementMode::Jogging:
			XPGained = JoggingXPPerSecond * DeltaTime;
			break;
		case EMOMovementMode::Sprinting:
			XPGained = SprintingXPPerSecond * DeltaTime;
			break;
		}

		if (XPGained > 0.0f)
		{
			SkillsComponent->AddExperience(AthleticsSkillId, XPGained);
		}
	}
}

// ============================================================================
// COMPONENT EVENT HANDLERS
// ============================================================================

void AMOCharacter::HandleKnowledgeLearned(FName KnowledgeId, FName FromItemId)
{
	if (RecipeDiscoveryComponent)
	{
		RecipeDiscoveryComponent->CheckDiscoveryFromKnowledge(KnowledgeId);
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] Knowledge '%s' learned - checking for recipe discoveries"), *KnowledgeId.ToString());
	}

	// Show notification via player controller's notification component
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UMONotificationComponent* NotificationComp = PC->FindComponentByClass<UMONotificationComponent>())
		{
			NotificationComp->ShowKnowledgeLearnedNotification(KnowledgeId);
		}
	}
}

void AMOCharacter::HandleSkillLevelUp(FName SkillId, int32 OldLevel, int32 NewLevel)
{
	if (RecipeDiscoveryComponent)
	{
		RecipeDiscoveryComponent->CheckDiscoveryFromSkillLevel(SkillId, NewLevel);
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] Skill '%s' leveled up to %d - checking for recipe discoveries"), *SkillId.ToString(), NewLevel);
	}
}

void AMOCharacter::HandleRecipeDiscovered(FName RecipeId, EMODiscoveryMethod Method)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] Recipe '%s' discovered via method %d"), *RecipeId.ToString(), (int32)Method);

	// Show notification via player controller's notification component
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UMONotificationComponent* NotificationComp = PC->FindComponentByClass<UMONotificationComponent>())
		{
			NotificationComp->ShowRecipeUnlockedNotification(RecipeId);
		}
	}
}

// ============================================================================
// IMOInventoryHolderInterface IMPLEMENTATION
// ============================================================================

UMOInventoryComponent* AMOCharacter::GetInventory_Implementation() const
{
	return InventoryComponent;
}

bool AMOCharacter::HasInventoryItem_Implementation(FName ItemDefinitionId, int32 Quantity) const
{
	return InventoryComponent ? InventoryComponent->HasItem(ItemDefinitionId, Quantity) : false;
}

int32 AMOCharacter::GetInventoryItemCount_Implementation(FName ItemDefinitionId) const
{
	return InventoryComponent ? InventoryComponent->GetItemCountByDefinitionId(ItemDefinitionId) : 0;
}

// ============================================================================
// IMOMaterialSourceInterface IMPLEMENTATION
// ============================================================================

bool AMOCharacter::CanProvideMaterial_Implementation(FName MaterialId, int32 Quantity) const
{
	return InventoryComponent ? InventoryComponent->HasItem(MaterialId, Quantity) : false;
}

int32 AMOCharacter::GatherMaterial_Implementation(FName MaterialId, int32 Quantity)
{
	if (!InventoryComponent)
	{
		return 0;
	}

	int32 Gathered = 0;
	for (int32 i = 0; i < Quantity; ++i)
	{
		if (InventoryComponent->RemoveItemByDefinitionId(MaterialId, 1))
		{
			++Gathered;
		}
		else
		{
			break;
		}
	}
	return Gathered;
}

int32 AMOCharacter::GetMaterialSourcePriority_Implementation() const
{
	// Player inventory has highest priority (100)
	return 100;
}

// ============================================================================
// IMOIdentifiableInterface IMPLEMENTATION
// ============================================================================

UMOIdentityComponent* AMOCharacter::GetIdentityComponent_Implementation() const
{
	return IdentityComponent;
}

FGuid AMOCharacter::GetPersistentGuid_Implementation() const
{
	return IdentityComponent ? IdentityComponent->GetGuid() : FGuid();
}

bool AMOCharacter::HasValidIdentity_Implementation() const
{
	return IdentityComponent && IdentityComponent->GetGuid().IsValid();
}

// ============================================================================
// IMOCraftingCapableInterface IMPLEMENTATION
// ============================================================================

UMOCraftingQueueComponent* AMOCharacter::GetCraftingQueue_Implementation() const
{
	return CraftingQueueComponent;
}

UMORecipeDiscoveryComponent* AMOCharacter::GetRecipeDiscovery_Implementation() const
{
	return RecipeDiscoveryComponent;
}

void AMOCharacter::SetActiveCraftingStation_Implementation(AActor* StationActor)
{
	if (CraftingQueueComponent)
	{
		CraftingQueueComponent->SetActiveStation(StationActor);
	}
}

AActor* AMOCharacter::GetActiveCraftingStation_Implementation() const
{
	if (CraftingQueueComponent)
	{
		return CraftingQueueComponent->GetActiveStation();
	}
	return nullptr;
}

// ============================================================================
// IMOMedicalProviderInterface IMPLEMENTATION
// ============================================================================

UMOVitalsComponent* AMOCharacter::GetVitals_Implementation() const
{
	return VitalsComponent;
}

UMOAnatomyComponent* AMOCharacter::GetAnatomy_Implementation() const
{
	return AnatomyComponent;
}

UMOMentalStateComponent* AMOCharacter::GetMentalState_Implementation() const
{
	return MentalStateComponent;
}

UMOAdrenalineComponent* AMOCharacter::GetAdrenaline_Implementation() const
{
	return AdrenalineComponent;
}

UMOMetabolismComponent* AMOCharacter::GetMetabolism_Implementation() const
{
	return MetabolismComponent;
}

// ============================================================================
// TERRAFORMING
// ============================================================================

void AMOCharacter::ToggleTerraformMode()
{
	bTerraformingMode = !bTerraformingMode;

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: Terraform mode %s"),
		*GetName(),
		bTerraformingMode ? TEXT("ENABLED") : TEXT("DISABLED"));

	// Update UI via player controller
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UMOUIManagerComponent* UIManager = PC->FindComponentByClass<UMOUIManagerComponent>())
		{
			if (bTerraformingMode)
			{
				// Set gameplay mode to Terraform
				UIManager->SetGameplayMode(EMOGameplayMode::Terraform);

				// Show current tool hint
				if (TerraformingComponent)
				{
					UIManager->ShowToolHint(TerraformingComponent->GetModeDisplayName());
				}
			}
			else
			{
				// Return to Explore mode
				UIManager->SetGameplayMode(EMOGameplayMode::Explore);
				UIManager->HideToolHint();
			}
		}
	}

	if (bTerraformingMode && TerraformingComponent)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] Current terraform tool: %s"),
			*TerraformingComponent->GetModeDisplayName().ToString());
	}
}

void AMOCharacter::CycleTerraformTool()
{
	if (TerraformingComponent)
	{
		TerraformingComponent->CycleMode();

		// Show tool hint
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (UMOUIManagerComponent* UIManager = PC->FindComponentByClass<UMOUIManagerComponent>())
			{
				UIManager->ShowToolHint(TerraformingComponent->GetModeDisplayName());
			}
		}
	}
}

bool AMOCharacter::DoTerraform()
{
	if (!TerraformingComponent)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] DoTerraform: No TerraformingComponent"));
		return false;
	}

	if (!bTerraformingMode)
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] DoTerraform: Not in terraform mode"));
		return false;
	}

	return TerraformingComponent->TryTerraform();
}

// ============================================================================
// HELD ITEM VISUALS
// ============================================================================

void AMOCharacter::HandleEquipmentChanged(EMOEquipmentSlot EquipSlot, const FMOEquippedItem& EquippedItem)
{
	// Only handle hand slots for held item visuals
	if (EquipSlot == EMOEquipmentSlot::LeftHand || EquipSlot == EMOEquipmentSlot::RightHand)
	{
		UpdateHeldItemMesh(EquipSlot, EquippedItem);
	}
}

bool AMOCharacter::HandleRecruitmentInteraction(AController* InteractorController)
{
	if (!RecruitmentComponent)
	{
		return false;
	}

	// Already recruited - no special interaction needed
	if (RecruitmentComponent->IsPossessable())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s is already recruited"), *GetName());
		return false;
	}

	// Get the interacting pawn
	APawn* InteractingPawn = InteractorController ? InteractorController->GetPawn() : nullptr;
	if (!InteractingPawn)
	{
		return false;
	}

	// Attempt recruitment interaction
	const bool bResult = RecruitmentComponent->BeginInteraction(InteractingPawn);

	if (bResult)
	{
		if (RecruitmentComponent->IsPossessable())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s has been recruited!"), *GetName());
			// TODO: Show UI notification that survivor has been recruited
		}
		else if (RecruitmentComponent->HasActiveQuest())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s has a quest: %s"),
				*GetName(), *RecruitmentComponent->GetQuestProgressText().ToString());
			// TODO: Show UI with quest requirements
		}
	}

	return bResult;
}

void AMOCharacter::UpdateHeldItemMesh(EMOEquipmentSlot EquipSlot, const FMOEquippedItem& EquippedItem)
{
	// Determine which mesh component to use
	UStaticMeshComponent* TargetMesh = nullptr;
	FName SocketName = NAME_None;

	if (EquipSlot == EMOEquipmentSlot::LeftHand)
	{
		TargetMesh = LeftHandMesh;
		SocketName = LeftHandSocketName;
	}
	else if (EquipSlot == EMOEquipmentSlot::RightHand)
	{
		TargetMesh = RightHandMesh;
		SocketName = RightHandSocketName;
	}
	else
	{
		return; // Not a hand slot
	}

	if (!TargetMesh)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] UpdateHeldItemMesh: No mesh component for slot"));
		return;
	}

	// If slot is empty, hide the mesh
	if (!EquippedItem.IsValid())
	{
		ClearHeldItemMesh(EquipSlot);
		return;
	}

	// Look up item definition to get the mesh
	const UMOItemDatabaseSettings* Settings = GetDefault<UMOItemDatabaseSettings>();
	if (!Settings)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] UpdateHeldItemMesh: No item database settings"));
		return;
	}

	UDataTable* ItemTable = Settings->ItemDefinitionsDataTable.LoadSynchronous();
	if (!ItemTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] UpdateHeldItemMesh: Failed to load item DataTable"));
		return;
	}

	const FMOItemDefinitionRow* ItemDef = ItemTable->FindRow<FMOItemDefinitionRow>(EquippedItem.ItemDefinitionId, TEXT("UpdateHeldItemMesh"));
	if (!ItemDef)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] UpdateHeldItemMesh: Item '%s' not found in DataTable"),
			*EquippedItem.ItemDefinitionId.ToString());
		ClearHeldItemMesh(EquipSlot);
		return;
	}

	// Determine which mesh to use: HeldVisual.StaticMesh first, fall back to WorldVisual.StaticMesh
	TSoftObjectPtr<UStaticMesh> MeshToLoad = ItemDef->HeldVisual.StaticMesh;
	if (MeshToLoad.IsNull())
	{
		MeshToLoad = ItemDef->WorldVisual.StaticMesh;
	}

	if (MeshToLoad.IsNull())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] UpdateHeldItemMesh: Item '%s' has no StaticMesh defined (checked HeldVisual and WorldVisual)"),
			*EquippedItem.ItemDefinitionId.ToString());
		ClearHeldItemMesh(EquipSlot);
		return;
	}

	UStaticMesh* LoadedMesh = MeshToLoad.LoadSynchronous();
	if (!LoadedMesh)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] UpdateHeldItemMesh: Failed to load mesh for '%s'"),
			*EquippedItem.ItemDefinitionId.ToString());
		ClearHeldItemMesh(EquipSlot);
		return;
	}

	// Set the mesh and make it visible
	TargetMesh->SetStaticMesh(LoadedMesh);

	// Apply held transform for the appropriate hand
	const bool bIsLeftHand = (EquipSlot == EMOEquipmentSlot::LeftHand);
	TargetMesh->SetRelativeTransform(ItemDef->HeldVisual.GetTransformForHand(bIsLeftHand));

	// Apply material override: HeldVisual first, fall back to WorldVisual
	TSoftObjectPtr<UMaterialInterface> MaterialToLoad = ItemDef->HeldVisual.MaterialOverride;
	if (MaterialToLoad.IsNull())
	{
		MaterialToLoad = ItemDef->WorldVisual.MaterialOverride;
	}

	if (!MaterialToLoad.IsNull())
	{
		if (UMaterialInterface* Material = MaterialToLoad.LoadSynchronous())
		{
			TargetMesh->SetMaterial(0, Material);
		}
	}

	// Ensure it's attached to the correct socket
	TargetMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);

	TargetMesh->SetVisibility(true);

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: Equipped '%s' to %s hand - mesh visible"),
		*GetName(),
		*EquippedItem.ItemDefinitionId.ToString(),
		EquipSlot == EMOEquipmentSlot::LeftHand ? TEXT("left") : TEXT("right"));
}

void AMOCharacter::ClearHeldItemMesh(EMOEquipmentSlot EquipSlot)
{
	UStaticMeshComponent* TargetMesh = nullptr;

	if (EquipSlot == EMOEquipmentSlot::LeftHand)
	{
		TargetMesh = LeftHandMesh;
	}
	else if (EquipSlot == EMOEquipmentSlot::RightHand)
	{
		TargetMesh = RightHandMesh;
	}

	if (TargetMesh)
	{
		TargetMesh->SetVisibility(false);
		TargetMesh->SetStaticMesh(nullptr);

		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: Cleared %s hand mesh"),
			*GetName(),
			EquipSlot == EMOEquipmentSlot::LeftHand ? TEXT("left") : TEXT("right"));
	}
}

// ============================================================================
// FALL-THROUGH SAFETY
// ============================================================================

bool AMOCharacter::FindSafeTerrainNearLocation(const FVector& NearLocation, FVector& OutSafeLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	// Helper lambda to check if a hit is valid voxel terrain with acceptable slope
	auto IsValidTerrainHit = [this](const FHitResult& Hit) -> bool
	{
		// Check if voxel terrain only
		if (bSafetyTeleportOnlyVoxelTerrain)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || !HitActor->IsA<AVoxelWorld>())
			{
				return false;
			}
		}

		// Check slope - normal Z must be above threshold
		if (Hit.ImpactNormal.Z < SafetyMinSlopeNormalZ)
		{
			return false;
		}

		return true;
	};

	const float RaycastStartZ = 50000.0f;

	// First, try the immediate area directly above
	{
		FHitResult UpHit;
		const FVector UpTraceStart = NearLocation;
		const FVector UpTraceEnd = NearLocation + FVector(0.f, 0.f, SafetyTerrainSearchDistance);

		if (World->LineTraceSingleByChannel(UpHit, UpTraceStart, UpTraceEnd, ECC_WorldStatic, QueryParams))
		{
			if (IsValidTerrainHit(UpHit))
			{
				// Calculate teleport height - higher for steeper slopes
				// At normal.Z = 1.0 (flat), use SafetyTeleportHeight
				// At normal.Z = 0.6 (steep), use 2x SafetyTeleportHeight
				const float SlopeMultiplier = 1.0f + (1.0f - UpHit.ImpactNormal.Z) * 2.5f;
				OutSafeLocation = UpHit.Location + FVector(0.f, 0.f, SafetyTeleportHeight * SlopeMultiplier);
				return true;
			}
		}
	}

	// Search laterally for flat voxel terrain
	FVector BestLocation = FVector::ZeroVector;
	float BestSlopeZ = -1.0f;  // Track flattest surface found (highest normal.Z)
	bool bFoundValid = false;

	for (int32 Sample = 0; Sample < SafetyLateralSearchSamples; ++Sample)
	{
		const float Angle = (static_cast<float>(Sample) / SafetyLateralSearchSamples) * 2.0f * PI;
		const float Radius = SafetyLateralSearchRadius;

		const FVector SampleXY(
			NearLocation.X + FMath::Cos(Angle) * Radius,
			NearLocation.Y + FMath::Sin(Angle) * Radius,
			RaycastStartZ
		);
		const FVector TraceEnd = SampleXY - FVector(0.f, 0.f, RaycastStartZ * 2.f);

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, SampleXY, TraceEnd, ECC_WorldStatic, QueryParams))
		{
			if (IsValidTerrainHit(Hit))
			{
				// Prefer flatter terrain (higher normal.Z)
				if (Hit.ImpactNormal.Z > BestSlopeZ)
				{
					BestSlopeZ = Hit.ImpactNormal.Z;
					const float SlopeMultiplier = 1.0f + (1.0f - Hit.ImpactNormal.Z) * 2.5f;
					BestLocation = Hit.Location + FVector(0.f, 0.f, SafetyTeleportHeight * SlopeMultiplier);
					bFoundValid = true;
				}
			}
		}
	}

	if (bFoundValid)
	{
		OutSafeLocation = BestLocation;
		return true;
	}

	return false;
}

void AMOCharacter::CheckFallThroughSafety(float DeltaTime)
{
	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	if (!MovementComp)
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const float CurrentZ = CurrentLocation.Z;
	const float VerticalVelocity = GetVelocity().Z;

	// Check if we're falling (in air with significant downward velocity)
	const bool bIsFalling = MovementComp->IsFalling() && VerticalVelocity < FallThroughVelocityThreshold;

	if (bIsFalling)
	{
		ContinuousFallTime += DeltaTime;

		// Only check for fall-through after exceeding time threshold
		if (ContinuousFallTime >= FallThroughTimeThreshold)
		{
			// Helper lambda to check if a hit is valid voxel terrain
			auto IsValidTerrainHit = [this](const FHitResult& Hit) -> bool
			{
				if (bSafetyTeleportOnlyVoxelTerrain)
				{
					AActor* HitActor = Hit.GetActor();
					if (!HitActor || !HitActor->IsA<AVoxelWorld>())
					{
						return false;
					}
				}
				return true;
			};

			// Raycast downward to check if there's valid terrain below
			FHitResult DownHit;
			const FVector TraceStart = CurrentLocation;
			const FVector TraceEnd = CurrentLocation - FVector(0.f, 0.f, SafetyTerrainSearchDistance);

			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);

			bool bFoundValidTerrainBelow = false;
			if (GetWorld()->LineTraceSingleByChannel(DownHit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
			{
				bFoundValidTerrainBelow = IsValidTerrainHit(DownHit);
			}

			if (!bFoundValidTerrainBelow)
			{
				// No valid terrain below - search for safe location
				FVector SafeLocation;
				if (FindSafeTerrainNearLocation(CurrentLocation, SafeLocation))
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s: Fall-through detected! Found safe terrain, teleporting to %s"),
						*GetName(), *SafeLocation.ToString());
				}
				else
				{
					// No valid terrain found anywhere - teleport to a default safe height
					SafeLocation = FVector(CurrentLocation.X, CurrentLocation.Y, SafetyTeleportHeight);
					UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s: Fall-through detected! No valid terrain found, teleporting to default height Z=%.0f"),
						*GetName(), SafeLocation.Z);
				}

				// Teleport to safety
				SetActorLocation(SafeLocation, false, nullptr, ETeleportType::TeleportPhysics);

				// Reset velocity to prevent continued falling
				MovementComp->StopMovementImmediately();

				// Reset fall tracking
				ContinuousFallTime = 0.f;

#if !UE_BUILD_SHIPPING
				// Log warning for debugging
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
						FString::Printf(TEXT("Fall-through safety activated! Teleported to %s"), *SafeLocation.ToString()));
				}
#endif
			}
			else
			{
				// There's valid terrain below - we're just falling normally
				// Reset timer only if we're getting close to it (within 1000 units)
				if (DownHit.Distance < 1000.f)
				{
					ContinuousFallTime = 0.f;
				}
			}
		}
	}
	else
	{
		// Not falling (or not falling fast enough) - reset timer
		ContinuousFallTime = 0.f;
	}

	LastZPosition = CurrentZ;
}
