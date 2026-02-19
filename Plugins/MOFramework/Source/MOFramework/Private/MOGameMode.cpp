#include "MOGameMode.h"
#include "MOFramework.h"
#include "MOPCGInteractionSubsystem.h"
#include "MOPersistenceSubsystem.h"
#include "MOGameSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "VoxelWorld.h"
#include "VoxelStampComponent.h"
#include "VoxelExposedSeed.h"
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
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Pending new game detected, slot: %s, seed: %d"),
			*Settings->PendingNewGameSlot, Settings->PendingWorldSeed);

		// Determine effective seed
		int32 EffectiveSeed = Settings->PendingWorldSeed;
		if (EffectiveSeed == 0)
		{
			// Use time-based seed for truly random generation
			EffectiveSeed = static_cast<int32>(FDateTime::Now().GetTicks() & 0x7FFFFFFF);
			Settings->PendingWorldSeed = EffectiveSeed;  // Store for voxel stamp use
			UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Generated random seed: %d"), EffectiveSeed);
		}

		// Apply world seed to FMath::Rand() for any systems that use it
		FMath::RandInit(EffectiveSeed);
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Applied world seed to FMath: %d"), EffectiveSeed);

		// If auto-initialization is enabled, apply seed to voxel stamps and create runtime
		if (bAutoInitializeVoxelWithSeed)
		{
			InitializeVoxelWorldWithSeed();
		}

		// Clear the pending flag
		Settings->bPendingNewGame = false;
		Settings->SaveSettings();

		// Wait for voxel world to be ready before spawning
		WaitForVoxelWorldAndSpawn();
	}
	else if (!Settings->PendingNewGameSlot.IsEmpty())
	{
		// Loading existing game
		FString SlotToLoad = Settings->PendingNewGameSlot;
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Loading existing game from slot: %s"), *SlotToLoad);

		// Clear the slot name after capturing it
		Settings->PendingNewGameSlot.Empty();
		Settings->SaveSettings();

		// Load through persistence subsystem
		UGameInstance* GameInstance = GetGameInstance();
		if (GameInstance)
		{
			UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
			if (Persistence)
			{
				FMOLoadResult Result = Persistence->LoadWorldFromSlotWithResult(SlotToLoad);
				if (Result.bSuccess)
				{
					UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Save loaded successfully: %d pawns, %d items, %d buildings"),
						Result.PawnsLoaded, Result.ItemsLoaded, Result.BuildingsLoaded);
				}
				else
				{
					UE_LOG(LogMOFramework, Error, TEXT("[MOGameMode] Failed to load save: %s"), *Result.ErrorMessage);
				}
			}
			else
			{
				UE_LOG(LogMOFramework, Error, TEXT("[MOGameMode] No persistence subsystem found!"));
			}
		}
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

	// Always wait for voxel world to be ready - even if IsVoxelWorldReady() returns true,
	// the geometry might still be generating after a seed change
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Waiting for VoxelWorld render state before spawning..."));
	bPendingSpawnAfterVoxelReady = true;

	// Use a timer to poll for voxel readiness - more reliable than OnNextStateRendered
	// which may not fire again after world regeneration
	World->GetTimerManager().ClearTimer(VoxelReadyTimerHandle);
	World->GetTimerManager().SetTimer(
		VoxelReadyTimerHandle,
		FTimerDelegate::CreateUObject(this, &AMOGameMode::CheckVoxelReadyAndSpawn),
		0.5f,  // Check every 0.5 seconds
		true   // Looping until spawned
	);
}

void AMOGameMode::CheckVoxelReadyAndSpawn()
{
	if (!bPendingSpawnAfterVoxelReady)
	{
		GetWorld()->GetTimerManager().ClearTimer(VoxelReadyTimerHandle);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Find the voxel world actor
	AVoxelWorld* VoxelWorld = nullptr;
	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		VoxelWorld = *It;
		break;
	}

	// Check if voxel world is ready
	if (VoxelWorld && VoxelWorld->IsVoxelWorldReady())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] VoxelWorld ready, waiting %.1f seconds for collision generation..."),
			CollisionGenerationDelay);
		bPendingSpawnAfterVoxelReady = false;
		World->GetTimerManager().ClearTimer(VoxelReadyTimerHandle);

		// Start collision delay timer - wait for terrain collision to generate
		World->GetTimerManager().SetTimer(
			CollisionDelayTimerHandle,
			FTimerDelegate::CreateUObject(this, &AMOGameMode::OnCollisionDelayComplete),
			CollisionGenerationDelay,
			false  // Not looping - fire once
		);
	}
	else
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOGameMode] Still waiting for VoxelWorld..."));
	}
}

void AMOGameMode::OnCollisionDelayComplete()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Collision delay complete, spawning pawn now"));

	// Debug: Verify stamp seeds after voxel world is ready
	DebugLogVoxelStampSeeds();

	SpawnInitialPawn();
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
	// Target beach spawn: between MinSpawnHeightAboveWater and 500cm above water
	const float IdealBeachMaxZ = WaterLevelZ + 500.0f;

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] FindSafeSpawnLocation: WaterLevelZ=%.1f, MinSpawnZ=%.1f, BeachMaxZ=%.1f"),
		WaterLevelZ, MinSpawnZ, IdealBeachMaxZ);

	// Track best beach candidate (closest to water but safely above it)
	FVector BestBeachLocation = FVector::ZeroVector;
	float BestBeachZ = FLT_MAX;  // We want the LOWEST point above MinSpawnZ (beach)
	bool bFoundBeach = false;

	// Track any valid location as fallback (any land above water)
	FVector BestFallbackLocation = FVector::ZeroVector;
	float BestFallbackZ = -FLT_MAX;
	bool bFoundLand = false;

	int32 TotalHits = 0;
	int32 HitsAboveWater = 0;

	FCollisionQueryParams Params;
	Params.bTraceComplex = false;  // Use simple collision for voxel terrain
	Params.bReturnPhysicalMaterial = false;

	// Multi-pass search: expand outward in rings until we find land
	// Pass 1: Search expanding rings to find ANY land
	// Pass 2: Once land found, focus search there for beach
	const int32 MaxRings = 20;  // Up to 20 rings expanding outward
	const float RingWidth = SpawnSearchRadius;  // Each ring is this wide
	const int32 SamplesPerRing = MaxSpawnAttempts / 2;  // Samples per ring

	for (int32 Ring = 0; Ring < MaxRings && !bFoundLand; ++Ring)
	{
		float InnerRadius = Ring * RingWidth;
		float OuterRadius = (Ring + 1) * RingWidth;

		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Searching ring %d: %.0f - %.0f cm from center"),
			Ring, InnerRadius, OuterRadius);

		for (int32 Sample = 0; Sample < SamplesPerRing; ++Sample)
		{
			// Random point in ring (between inner and outer radius)
			float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
			float Radius = FMath::FRandRange(InnerRadius, OuterRadius);
			FVector Start(
				SpawnSearchCenter.X + FMath::Cos(Angle) * Radius,
				SpawnSearchCenter.Y + FMath::Sin(Angle) * Radius,
				RaycastStartZ
			);
			FVector End = Start - FVector(0, 0, RaycastStartZ * 2);

			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
			{
				TotalHits++;

				// Log first few hits of each ring
				if (Sample < 3)
				{
					UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Ring %d Hit: Z=%.1f at XY=(%.0f, %.0f)"),
						Ring, Hit.Location.Z, Hit.Location.X, Hit.Location.Y);
				}

				// Check if above water level - this is LAND
				if (Hit.Location.Z > MinSpawnZ)
				{
					HitsAboveWater++;
					bFoundLand = true;

					// Check if this is a beach location (low elevation, close to water)
					if (Hit.Location.Z < IdealBeachMaxZ && Hit.Location.Z < BestBeachZ)
					{
						BestBeachZ = Hit.Location.Z;
						BestBeachLocation = Hit.Location + FVector(0, 0, SpawnHeightOffset);
						bFoundBeach = true;

						// Ideal beach found - low enough to be a good beach
						if (Hit.Location.Z < WaterLevelZ + 200.0f)
						{
							FVector SpawnLocation = Hit.Location + FVector(0, 0, SpawnHeightOffset);
							UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Found ideal beach spawn in ring %d: %s (Z=%.1f above water)"),
								Ring, *SpawnLocation.ToString(), Hit.Location.Z - WaterLevelZ);
							return SpawnLocation;
						}
					}

					// Track as fallback (any valid land)
					if (Hit.Location.Z > BestFallbackZ)
					{
						BestFallbackZ = Hit.Location.Z;
						BestFallbackLocation = Hit.Location + FVector(0, 0, SpawnHeightOffset);
					}
				}
			}
		}
	}

	// If we found land but no ideal beach, do a focused search around the best land we found
	if (bFoundLand && !bFoundBeach && BestFallbackZ > MinSpawnZ)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Found land at Z=%.1f, searching for nearby beach..."),
			BestFallbackZ);

		FVector LandCenter = BestFallbackLocation;
		const float BeachSearchRadius = 10000.0f;  // Search 100m around the land

		for (int32 BeachSample = 0; BeachSample < SamplesPerRing; ++BeachSample)
		{
			FVector2D RandomOffset = FMath::RandPointInCircle(BeachSearchRadius);
			FVector Start(LandCenter.X + RandomOffset.X, LandCenter.Y + RandomOffset.Y, RaycastStartZ);
			FVector End = Start - FVector(0, 0, RaycastStartZ * 2);

			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
			{
				TotalHits++;

				if (Hit.Location.Z > MinSpawnZ && Hit.Location.Z < IdealBeachMaxZ && Hit.Location.Z < BestBeachZ)
				{
					BestBeachZ = Hit.Location.Z;
					BestBeachLocation = Hit.Location + FVector(0, 0, SpawnHeightOffset);
					bFoundBeach = true;

					if (Hit.Location.Z < WaterLevelZ + 200.0f)
					{
						UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Found beach near land: %s (Z=%.1f above water)"),
							*BestBeachLocation.ToString(), Hit.Location.Z - WaterLevelZ);
						return BestBeachLocation;
					}
				}
			}
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Spawn search stats: TotalHits=%d, HitsAboveWater=%d, FoundBeach=%d, FoundLand=%d"),
		TotalHits, HitsAboveWater, bFoundBeach ? 1 : 0, bFoundLand ? 1 : 0);

	// Prefer beach location if found
	if (bFoundBeach)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Using beach spawn location: %s (Z=%.1f above water)"),
			*BestBeachLocation.ToString(), BestBeachZ - WaterLevelZ);
		return BestBeachLocation;
	}

	// Use any valid land as fallback
	if (bFoundLand && BestFallbackZ > MinSpawnZ)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Using fallback land spawn: %s (Z=%.1f)"),
			*BestFallbackLocation.ToString(), BestFallbackZ);
		return BestFallbackLocation;
	}

	// Absolute fallback: spawn at safe default above water
	FVector FallbackLocation(SpawnSearchCenter.X, SpawnSearchCenter.Y, MinSpawnZ + 500.0f);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] No land found after searching %d rings, using fallback: %s"),
		MaxRings, *FallbackLocation.ToString());
	return FallbackLocation;
}

// ============================================================================
// VOXEL SEED INTEGRATION
// ============================================================================

FString AMOGameMode::IntSeedToVoxelSeedString(int32 Seed)
{
	// Replicate the algorithm from FVoxelExposedSeed::Randomize()
	// Generates an 8-character uppercase string (A-Z) from the seed
	const FRandomStream Stream(Seed);

	FString Result;
	Result.Reserve(8);
	for (int32 Index = 0; Index < 8; Index++)
	{
		Result += TCHAR(Stream.RandRange(TEXT('A'), TEXT('Z')));
	}

	return Result;
}

int32 AMOGameMode::ApplySeedToVoxelStamps(int32 WorldSeed)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] ApplySeedToVoxelStamps: No world available"));
		return 0;
	}

	// Convert integer seed to voxel seed string format
	const FString VoxelSeedString = IntSeedToVoxelSeedString(WorldSeed);

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Applying voxel seed: %d (%s)"), WorldSeed, *VoxelSeedString);

	int32 StampsUpdated = 0;
	int32 TotalStampsFound = 0;

	// Find all VoxelStampComponent instances in the world
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor)
		{
			continue;
		}

		// Get all stamp components on this actor
		TArray<UVoxelStampComponent*> StampComponents;
		Actor->GetComponents<UVoxelStampComponent>(StampComponents);

		for (UVoxelStampComponent* StampComp : StampComponents)
		{
			if (!StampComp)
			{
				continue;
			}

			TotalStampsFound++;

			// Get the current stamp, modify its seed
			FVoxelStampRef StampRef = StampComp->GetStamp();
			if (StampRef.IsValid())
			{
				// Log the old seed before changing
				const FString OldSeed = StampRef->StampSeed.Seed;

				// Set the seed string via operator->
				StampRef->StampSeed.Seed = VoxelSeedString;
				StampsUpdated++;

				UE_LOG(LogMOFramework, Verbose, TEXT("[MOGameMode] Stamp in '%s': seed '%s' -> '%s'"),
					*Actor->GetName(), *OldSeed, *VoxelSeedString);
			}
			else
			{
				UE_LOG(LogMOFramework, Verbose, TEXT("[MOGameMode] Stamp in '%s': no stamp data"),
					*Actor->GetName());
			}
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Voxel seed applied to %d/%d stamps"), StampsUpdated, TotalStampsFound);

	return StampsUpdated;
}

void AMOGameMode::DebugLogVoxelStampSeeds()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[VOXEL SEED VERIFY] No world available"));
		return;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("========================================"));
	UE_LOG(LogMOFramework, Warning, TEXT("[VOXEL SEED VERIFY] Current stamp seeds:"));
	UE_LOG(LogMOFramework, Warning, TEXT("========================================"));

	int32 TotalStamps = 0;

	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor)
		{
			continue;
		}

		TArray<UVoxelStampComponent*> StampComponents;
		Actor->GetComponents<UVoxelStampComponent>(StampComponents);

		for (UVoxelStampComponent* StampComp : StampComponents)
		{
			if (!StampComp)
			{
				continue;
			}

			TotalStamps++;
			FVoxelStampRef StampRef = StampComp->GetStamp();
			if (StampRef.IsValid())
			{
				const FString CurrentSeed = StampRef->StampSeed.Seed;
				const int32 SeedAsInt = StampRef->StampSeed.GetSeed();
				UE_LOG(LogMOFramework, Warning, TEXT("[VOXEL SEED VERIFY] '%s': Seed='%s' (int32=%d)"),
					*Actor->GetName(), *CurrentSeed, SeedAsInt);
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[VOXEL SEED VERIFY] '%s': INVALID STAMP"),
					*Actor->GetName());
			}
		}
	}

	// Also check the expected seed from settings
	if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		const FString ExpectedSeedString = IntSeedToVoxelSeedString(Settings->PendingWorldSeed);
		UE_LOG(LogMOFramework, Warning, TEXT("========================================"));
		UE_LOG(LogMOFramework, Warning, TEXT("[VOXEL SEED VERIFY] Expected seed: %d ('%s')"),
			Settings->PendingWorldSeed, *ExpectedSeedString);
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[VOXEL SEED VERIFY] Total stamps checked: %d"), TotalStamps);
	UE_LOG(LogMOFramework, Warning, TEXT("========================================"));
}

void AMOGameMode::InitializeVoxelWorldWithSeed()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] InitializeVoxelWorldWithSeed: No world available"));
		return;
	}

	// Get the seed from game settings
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	const int32 WorldSeed = Settings ? Settings->PendingWorldSeed : 0;

	if (WorldSeed == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] InitializeVoxelWorldWithSeed: No seed set in game settings"));
	}

	// Apply seed to all stamp components
	const int32 StampsUpdated = ApplySeedToVoxelStamps(WorldSeed);

	// Find and initialize the voxel world
	AVoxelWorld* VoxelWorld = nullptr;
	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		VoxelWorld = *It;
		break;
	}

	if (!VoxelWorld)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] InitializeVoxelWorldWithSeed: No VoxelWorld found in level"));
		return;
	}

	// Check if runtime is already created
	if (VoxelWorld->IsRuntimeCreated())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] VoxelWorld runtime already created. "
			"Set bCreateRuntimeOnBeginPlay = false on VoxelWorld for seed to apply before generation."));
		return;
	}

	// Create the runtime to start generation with the new seed
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Creating VoxelWorld runtime with seed %d (%d stamps updated)"),
		WorldSeed, StampsUpdated);
	VoxelWorld->CreateRuntime();
}
