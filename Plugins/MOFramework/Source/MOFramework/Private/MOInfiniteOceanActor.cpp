#include "MOInfiniteOceanActor.h"
#include "MOFramework.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

AMOInfiniteOceanActor::AMOInfiniteOceanActor()
{
	// Set defaults for ocean
	OceanExtent = 50000.0f;
	OceanLevel = 0.0f;
	SnapGridSize = 1000.0f;
	bFollowCamera = true;

	// Higher resolution for ocean
	MeshResolution = 128;

	// Ocean waves - wavelengths must be large enough for mesh density
	// With Extent=8000 and Resolution=128, cell size is ~125 units
	// Wavelengths should be 6-8x cell size minimum (~750+ units)
	Waves.Empty();

	FMOGerstnerWave Wave1;
	Wave1.Direction = FVector2D(1.0f, 0.2f);
	Wave1.Amplitude = 80.0f;
	Wave1.Wavelength = 3000.0f;  // Large primary swell
	Wave1.Steepness = 0.4f;
	Wave1.Speed = 0.4f;
	Waves.Add(Wave1);

	FMOGerstnerWave Wave2;
	Wave2.Direction = FVector2D(0.6f, 0.8f);
	Wave2.Amplitude = 50.0f;
	Wave2.Wavelength = 1800.0f;  // Secondary swell
	Wave2.Steepness = 0.35f;
	Wave2.Speed = 0.5f;
	Wave2.PhaseOffset = 2.0f;
	Waves.Add(Wave2);

	FMOGerstnerWave Wave3;
	Wave3.Direction = FVector2D(-0.4f, 0.9f);
	Wave3.Amplitude = 30.0f;
	Wave3.Wavelength = 1200.0f;  // Cross wave
	Wave3.Steepness = 0.3f;
	Wave3.Speed = 0.6f;
	Wave3.PhaseOffset = 4.5f;
	Waves.Add(Wave3);

	FMOGerstnerWave Wave4;
	Wave4.Direction = FVector2D(0.8f, -0.6f);
	Wave4.Amplitude = 20.0f;
	Wave4.Wavelength = 800.0f;   // Detail wave
	Wave4.Steepness = 0.25f;
	Wave4.Speed = 0.7f;
	Wave4.PhaseOffset = 1.2f;
	Waves.Add(Wave4);

	LastSnappedPosition = FVector2D::ZeroVector;
}

void AMOInfiniteOceanActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bFollowCamera)
	{
		UpdateOceanPosition();
	}
}

FVector2D AMOInfiniteOceanActor::GetFollowPosition() const
{
	// Try to get camera position
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC)
	{
		if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			FVector CameraLocation = CameraManager->GetCameraLocation();
			return FVector2D(CameraLocation.X, CameraLocation.Y);
		}

		// Fallback to pawn location
		if (APawn* Pawn = PC->GetPawn())
		{
			FVector PawnLocation = Pawn->GetActorLocation();
			return FVector2D(PawnLocation.X, PawnLocation.Y);
		}
	}

	return FVector2D::ZeroVector;
}

void AMOInfiniteOceanActor::UpdateOceanPosition()
{
	FVector2D FollowPos = GetFollowPosition();

	// Snap to grid
	FVector2D SnappedPos;
	SnappedPos.X = FMath::RoundToFloat(FollowPos.X / SnapGridSize) * SnapGridSize;
	SnappedPos.Y = FMath::RoundToFloat(FollowPos.Y / SnapGridSize) * SnapGridSize;

	// Only move if we've crossed a grid boundary
	if (!SnappedPos.Equals(LastSnappedPosition, 1.0f))
	{
		LastSnappedPosition = SnappedPos;
		SetActorLocation(FVector(SnappedPos.X, SnappedPos.Y, OceanLevel));
	}
}
