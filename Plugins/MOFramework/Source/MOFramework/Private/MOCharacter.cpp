#include "MOCharacter.h"
#include "MOFramework.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "MOIdentityComponent.h"
#include "MOInteractorComponent.h"
#include "MOInventoryComponent.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "MOSurvivalStatsComponent.h"
#include "UObject/SoftObjectPath.h"

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

	// NOTE: Input actions, mesh, and animation are intentionally left unset for portability.
	// Set these in child Blueprints or via Project Settings to avoid hard-coded asset paths.
	// The character will work without these - it just won't have input/visuals until configured.
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

	Super::BeginPlay();

	// Add input mapping context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (DefaultMappingContext.IsNull())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s: No DefaultMappingContext configured. Input will not work until set in child Blueprint."), *GetName());
			}
			else if (UInputMappingContext* IMC = DefaultMappingContext.LoadSynchronous())
			{
				Subsystem->AddMappingContext(IMC, 0);
			}
		}
	}
}

void AMOCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Bind Move action
		if (!MoveAction.IsNull())
		{
			if (UInputAction* Move = MoveAction.LoadSynchronous())
			{
				EnhancedInputComponent->BindAction(Move, ETriggerEvent::Triggered, this, &AMOCharacter::Move);
			}
		}

		// Bind Look action (gamepad)
		if (!LookAction.IsNull())
		{
			if (UInputAction* Look = LookAction.LoadSynchronous())
			{
				EnhancedInputComponent->BindAction(Look, ETriggerEvent::Triggered, this, &AMOCharacter::Look);
			}
		}

		// Bind MouseLook action
		if (!MouseLookAction.IsNull())
		{
			if (UInputAction* MouseLook = MouseLookAction.LoadSynchronous())
			{
				EnhancedInputComponent->BindAction(MouseLook, ETriggerEvent::Triggered, this, &AMOCharacter::Look);
			}
		}

		// Bind Jump action
		if (!JumpAction.IsNull())
		{
			if (UInputAction* Jump = JumpAction.LoadSynchronous())
			{
				EnhancedInputComponent->BindAction(Jump, ETriggerEvent::Started, this, &AMOCharacter::StartJump);
				EnhancedInputComponent->BindAction(Jump, ETriggerEvent::Completed, this, &AMOCharacter::StopJump);
			}
		}

		// Bind Interact action
		if (!InteractAction.IsNull())
		{
			if (UInputAction* InteractInput = InteractAction.LoadSynchronous())
			{
				EnhancedInputComponent->BindAction(InteractInput, ETriggerEvent::Started, this, &AMOCharacter::Interact);
			}
		}
	}
}

void AMOCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AMOCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AMOCharacter::StartJump()
{
	Jump();
}

void AMOCharacter::StopJump()
{
	StopJumping();
}

void AMOCharacter::DoMove(float Right, float Forward)
{
	if (Controller)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AMOCharacter::DoLook(float Yaw, float Pitch)
{
	if (Controller)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AMOCharacter::DoJumpStart()
{
	Jump();
}

void AMOCharacter::DoJumpEnd()
{
	StopJumping();
}

void AMOCharacter::Interact()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] Interact() called, InteractorComponent=%s"),
		InteractorComponent ? TEXT("Valid") : TEXT("NULL"));

	if (InteractorComponent)
	{
		const bool bResult = InteractorComponent->TryInteract();
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] TryInteract returned %s"), bResult ? TEXT("true") : TEXT("false"));
	}
}

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
