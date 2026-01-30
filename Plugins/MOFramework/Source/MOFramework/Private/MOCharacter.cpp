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

	// Follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Movement defaults (same as ThirdPerson template)
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// MO Components
	IdentityComponent = CreateDefaultSubobject<UMOIdentityComponent>(TEXT("IdentityComponent"));
	InventoryComponent = CreateDefaultSubobject<UMOInventoryComponent>(TEXT("InventoryComponent"));
	InteractorComponent = CreateDefaultSubobject<UMOInteractorComponent>(TEXT("InteractorComponent"));
	SurvivalStatsComponent = CreateDefaultSubobject<UMOSurvivalStatsComponent>(TEXT("SurvivalStatsComponent"));
	SkillsComponent = CreateDefaultSubobject<UMOSkillsComponent>(TEXT("SkillsComponent"));
	KnowledgeComponent = CreateDefaultSubobject<UMOKnowledgeComponent>(TEXT("KnowledgeComponent"));

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

	// Start movement physiology tracking
	StartMovementPhysiologyTracking();

	Super::BeginPlay();
}

void AMOCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopMovementPhysiologyTracking();
	Super::EndPlay(EndPlayReason);
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
		AddControllerYawInput(LookInput.X * LookSensitivity);
		AddControllerPitchInput(LookInput.Y * LookSensitivity);
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

void AMOCharacter::RequestPrimaryAction_Implementation()
{
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
	return CanMove_Implementation() && !bIsCrouched;
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
	if (GEngine)
	{
		const FColor ModeColor = NewMode == EMOMovementMode::Walking ? FColor::White :
								 NewMode == EMOMovementMode::Jogging ? FColor::Yellow : FColor::Orange;
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, ModeColor,
			FString::Printf(TEXT("%s: %.0f"), *ModeName, ActualSpeed));
	}
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
		if (UMOVitalsComponent* Vitals = FindComponentByClass<UMOVitalsComponent>())
		{
			// Gradually return to resting exertion (handled by vitals component)
			Vitals->SetExertionLevel(0.0f);
		}
		return;
	}

	// Get body weight for calorie calculation
	float BodyWeightKg = 75.0f; // Default
	if (UMOMetabolismComponent* Metabolism = FindComponentByClass<UMOMetabolismComponent>())
	{
		BodyWeightKg = Metabolism->BodyComposition.TotalWeight;
	}

	// Calculate calories burned: MET × weight(kg) × 0.0175 × time(minutes)
	const float CurrentMET = GetCurrentMET();
	const float CaloriesPerMinute = CurrentMET * BodyWeightKg * 0.0175f;
	const float CaloriesBurned = CaloriesPerMinute * (DeltaTime / 60.0f);

	// Apply to metabolism
	if (UMOMetabolismComponent* Metabolism = FindComponentByClass<UMOMetabolismComponent>())
	{
		Metabolism->ApplyCalorieBurn(CaloriesBurned);

		// Also count as cardio training if jogging or sprinting
		if (CurrentMovementMode != EMOMovementMode::Walking)
		{
			float Intensity = (CurrentMovementMode == EMOMovementMode::Sprinting) ? 0.9f : 0.5f;
			Metabolism->ApplyCardioTraining(Intensity, DeltaTime);
		}
	}

	// Apply to vitals (exertion, temperature)
	if (UMOVitalsComponent* Vitals = FindComponentByClass<UMOVitalsComponent>())
	{
		// Set exertion level
		Vitals->SetExertionLevel(GetCurrentExertionLevel());

		// Apply temperature increase
		// Note: The vitals component handles temperature regulation,
		// we just add heat generation from exercise
		float TempRise = GetCurrentTempRiseRate() * DeltaTime;
		Vitals->Vitals.BodyTemperature = FMath::Min(Vitals->Vitals.BodyTemperature + TempRise, 40.0f);
	}
}
