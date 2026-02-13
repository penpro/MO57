#include "MOGameMode.h"
#include "MOFramework.h"
#include "MOPCGInteractionSubsystem.h"
#include "MOGameSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "VoxelWorld.h"
#include "EngineUtils.h"

AMOGameMode::AMOGameMode()
{
	// Add default tag mappings - can be overridden in Blueprint
	PCGTagItemMappings.Add({ TEXT("GivesStick"), TEXT("Stick01") });
}

void AMOGameMode::BeginPlay()
{
	Super::BeginPlay();

	RegisterPCGTagMappings();

	// Check for pending new game from main menu
	HandlePendingNewGame();
}

void AMOGameMode::RegisterPCGTagMappings()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMOPCGInteractionSubsystem* PCGSubsystem = World->GetSubsystem<UMOPCGInteractionSubsystem>();
	if (!PCGSubsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] No PCG Interaction Subsystem found"));
		return;
	}

	for (const FMOTagItemMapping& Mapping : PCGTagItemMappings)
	{
		if (!Mapping.Tag.IsNone() && !Mapping.ItemId.IsNone())
		{
			PCGSubsystem->RegisterTagItemMapping(Mapping.Tag, Mapping.ItemId);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Registered %d PCG tag-to-item mappings"), PCGTagItemMappings.Num());
}

void AMOGameMode::HandlePendingNewGame()
{
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (!Settings)
	{
		return;
	}

	if (Settings->bPendingNewGame)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Pending new game detected, slot: %s"), *Settings->PendingNewGameSlot);

		// Clear the pending flag
		Settings->bPendingNewGame = false;
		Settings->SaveSettings();

		// Wait for voxel world to be ready before spawning
		WaitForVoxelWorldAndSpawn();
	}
	else if (!Settings->PendingNewGameSlot.IsEmpty())
	{
		// Loading existing game - the persistence system will handle this
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Loading existing game from slot: %s"), *Settings->PendingNewGameSlot);

		// Clear the slot name after processing
		Settings->PendingNewGameSlot.Empty();
		Settings->SaveSettings();

		// TODO: Trigger load through persistence subsystem
	}
}

void AMOGameMode::WaitForVoxelWorldAndSpawn()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] No world available for voxel check"));
		SpawnInitialPawn();
		return;
	}

	// Find the voxel world actor
	AVoxelWorld* VoxelWorld = nullptr;
	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		VoxelWorld = *It;
		break;
	}

	if (!VoxelWorld)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] No VoxelWorld found in level, spawning immediately"));
		SpawnInitialPawn();
		return;
	}

	// Check if voxel world is already ready
	if (VoxelWorld->IsVoxelWorldReady())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] VoxelWorld already ready, spawning pawn"));
		SpawnInitialPawn();
		return;
	}

	// Wait for voxel world to be ready
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Waiting for VoxelWorld to be ready before spawning pawn..."));
	bPendingSpawnAfterVoxelReady = true;

	VoxelWorld->OnNextStateRendered(FSimpleDelegate::CreateWeakLambda(this, [this]()
	{
		if (bPendingSpawnAfterVoxelReady)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] VoxelWorld ready, spawning pawn now"));
			bPendingSpawnAfterVoxelReady = false;
			SpawnInitialPawn();
		}
	}));
}

void AMOGameMode::SpawnInitialPawn()
{
	if (!DefaultNewGamePawnClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] DefaultNewGamePawnClass not set, cannot spawn initial pawn"));
		return;
	}

	// Find safe spawn location
	FVector SpawnLocation = FindSafeSpawnLocation();

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Spawning initial pawn at: %s"), *SpawnLocation.ToString());

	// Spawn the pawn
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APawn* NewPawn = World->SpawnActor<APawn>(DefaultNewGamePawnClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (!NewPawn)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOGameMode] Failed to spawn initial pawn"));
		return;
	}

	// Possess the pawn with player controller
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		PC->Possess(NewPawn);
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Player controller possessed initial pawn"));
	}
}

FVector AMOGameMode::FindSafeSpawnLocation() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return FVector(0, 0, WaterLevelZ + MinSpawnHeightAboveWater + SpawnHeightOffset);
	}

	const float RaycastStartZ = 50000.0f;  // Start high above terrain
	const float MinSpawnZ = WaterLevelZ + MinSpawnHeightAboveWater;

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] FindSafeSpawnLocation: WaterLevelZ=%.1f, MinSpawnZ=%.1f, SearchCenter=%s, SearchRadius=%.1f, MaxAttempts=%d"),
		WaterLevelZ, MinSpawnZ, *SpawnSearchCenter.ToString(), SpawnSearchRadius, MaxSpawnAttempts);

	// Track best candidate location (highest Z above water)
	FVector BestLocation = FVector::ZeroVector;
	float BestZ = -FLT_MAX;
	int32 HitsAboveWater = 0;
	int32 TotalHits = 0;

	for (int32 Attempt = 0; Attempt < MaxSpawnAttempts; ++Attempt)
	{
		// Random XY within search radius from spawn search center
		FVector2D RandomOffset = FMath::RandPointInCircle(SpawnSearchRadius);
		FVector Start(SpawnSearchCenter.X + RandomOffset.X, SpawnSearchCenter.Y + RandomOffset.Y, RaycastStartZ);
		FVector End = Start - FVector(0, 0, RaycastStartZ * 2);

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.bTraceComplex = false;  // Use simple collision for voxel terrain
		Params.bReturnPhysicalMaterial = false;

		// Use WorldStatic channel - this is what voxel terrain uses
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			TotalHits++;

			// Log first few hits for debugging
			if (TotalHits <= 5)
			{
				FString ActorName = Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT("None");
				FString CompName = Hit.GetComponent() ? Hit.GetComponent()->GetName() : TEXT("None");
				UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Hit %d: Z=%.1f, Actor=%s, Component=%s"),
					TotalHits, Hit.Location.Z, *ActorName, *CompName);
			}

			// Check if above water level
			if (Hit.Location.Z > MinSpawnZ)
			{
				HitsAboveWater++;

				// Track the highest valid point
				if (Hit.Location.Z > BestZ)
				{
					BestZ = Hit.Location.Z;
					BestLocation = Hit.Location + FVector(0, 0, SpawnHeightOffset);
				}

				// Accept first hit that's sufficiently above water (not just barely above)
				if (Hit.Location.Z > WaterLevelZ + 200.0f)
				{
					FVector SpawnLocation = Hit.Location + FVector(0, 0, SpawnHeightOffset);
					UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Found good spawn location at attempt %d: %s (Z=%.1f above water)"),
						Attempt + 1, *SpawnLocation.ToString(), Hit.Location.Z - WaterLevelZ);
					return SpawnLocation;
				}
			}
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Spawn search stats: TotalHits=%d, HitsAboveWater=%d"), TotalHits, HitsAboveWater);

	// Use best candidate if we found any valid locations
	if (BestZ > MinSpawnZ)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Using best candidate spawn location: %s"), *BestLocation.ToString());
		return BestLocation;
	}

	// Fallback: spawn at safe default above water
	FVector FallbackLocation(0, 0, MinSpawnZ + 500.0f);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] No valid spawn found after %d attempts (0 hits above water), using fallback: %s"),
		MaxSpawnAttempts, *FallbackLocation.ToString());
	return FallbackLocation;
}
