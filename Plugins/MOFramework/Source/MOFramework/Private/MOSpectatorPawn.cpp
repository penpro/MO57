#include "MOSpectatorPawn.h"
#include "MOFramework.h"
#include "MOPlayerController.h"
#include "GameFramework/SpectatorPawnMovement.h"

AMOSpectatorPawn::AMOSpectatorPawn()
{
	// Start locked by default - unlock when player possesses a gameplay pawn
	bMovementLocked = true;
}

void AMOSpectatorPawn::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogMOFramework, Warning, TEXT("AMOSpectatorPawn::BeginPlay - Spawned at %s"), *GetActorLocation().ToString());

	// Check if our player controller has a pending spectator view to apply
	if (AMOPlayerController* MOPC = Cast<AMOPlayerController>(GetController()))
	{
		if (MOPC->HasPendingSpectatorView())
		{
			FVector PendingLocation;
			FRotator PendingRotation;
			MOPC->GetPendingSpectatorView(PendingLocation, PendingRotation);

			SetActorLocation(PendingLocation);
			SetActorRotation(PendingRotation);
			MOPC->SetControlRotation(PendingRotation);
			MOPC->ClearPendingSpectatorView();

			UE_LOG(LogMOFramework, Warning, TEXT("AMOSpectatorPawn::BeginPlay - Applied pending view from controller: %s"),
				*PendingLocation.ToString());
		}

		// Check if spectator input should be disabled
		if (!MOPC->IsSpectatorInputEnabled())
		{
			bMovementLocked = true;
			UE_LOG(LogMOFramework, Warning, TEXT("AMOSpectatorPawn::BeginPlay - Movement locked (spectator input disabled)"));
		}
	}
}

void AMOSpectatorPawn::SetMovementLocked(bool bLocked)
{
	bMovementLocked = bLocked;
	UE_LOG(LogMOFramework, Warning, TEXT("AMOSpectatorPawn: Movement %s"), bLocked ? TEXT("LOCKED") : TEXT("UNLOCKED"));
}

void AMOSpectatorPawn::SetViewAboveLocation(FVector TargetLocation, float Height, float PitchAngle)
{
	FVector CameraLocation = TargetLocation + FVector(0.0f, 0.0f, Height);
	FRotator CameraRotation = FRotator(PitchAngle, 0.0f, 0.0f);

	SetActorLocation(CameraLocation);
	SetActorRotation(CameraRotation);

	// Also set the controller rotation if we have one
	if (AController* PC = GetController())
	{
		PC->SetControlRotation(CameraRotation);
	}

	UE_LOG(LogMOFramework, Warning, TEXT("AMOSpectatorPawn: Set view at %s looking down"), *CameraLocation.ToString());
}

void AMOSpectatorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Call parent to set up default spectator input bindings
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AMOSpectatorPawn::MoveForward(float Val)
{
	if (bMovementLocked)
	{
		return;
	}
	Super::MoveForward(Val);
}

void AMOSpectatorPawn::MoveRight(float Val)
{
	if (bMovementLocked)
	{
		return;
	}
	Super::MoveRight(Val);
}

void AMOSpectatorPawn::MoveUp_World(float Val)
{
	if (bMovementLocked)
	{
		return;
	}
	Super::MoveUp_World(Val);
}

void AMOSpectatorPawn::TurnAtRate(float Rate)
{
	if (bMovementLocked)
	{
		return;
	}
	Super::TurnAtRate(Rate);
}

void AMOSpectatorPawn::LookUpAtRate(float Rate)
{
	if (bMovementLocked)
	{
		return;
	}
	Super::LookUpAtRate(Rate);
}
