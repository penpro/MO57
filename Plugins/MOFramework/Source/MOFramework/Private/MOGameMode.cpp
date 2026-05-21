#include "MOGameMode.h"
#include "MOFramework.h"
#include "MOCharacter.h"
#include "MORecruitmentComponent.h"
#include "MOIdentityComponent.h"
#include "MOPCGInteractionSubsystem.h"
#include "MOPersistenceSubsystem.h"
#include "MOGameSettings.h"
#include "MOGameInstance.h"
#include "MOHarvestDebugSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "VoxelWorld.h"
#include "VoxelStampComponent.h"
#include "VoxelExposedSeed.h"
#include "Graphs/VoxelHeightGraph.h"
#include "VoxelGraph.h"
#include "VoxelParameter.h"
#include "VoxelParameterOverridesOwner.h"
#include "VoxelPinValue.h"
#include "Graphs/VoxelHeightGraphStamp.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

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
		MOHARVEST_LOG(this, "Seed", "HandlePendingNewGame: Settings null, bailing");
		return;
	}

	// PACKAGED-BUILD FIX (2026-05): AVoxelWorld defaults to
	// bCreateRuntimeOnBeginPlay = true, which means its BeginPlay
	// auto-creates the runtime BEFORE we get a chance to apply the saved
	// seed. The auto-created runtime uses whatever graph parameter state is
	// in memory at that moment — in packaged that's the cooked default, not
	// our saved seed. Our subsequent CreateRuntime call then silently
	// no-ops because a runtime already exists, so the terrain ends up
	// generated from the wrong seed (and stays that way).
	//
	// Force-disable the auto-create on every AVoxelWorld in the world and
	// destroy any runtime that's already up. Our explicit CreateRuntime
	// call inside InitializeVoxelWorldWithSeed is then the only path that
	// brings the runtime up, with the correct seed already applied.
	if (UWorld* World = GetWorld())
	{
		int32 VoxelWorldsAdjusted = 0;
		int32 VoxelWorldsDestroyed = 0;
		for (TActorIterator<AVoxelWorld> It(World); It; ++It)
		{
			AVoxelWorld* VW = *It;
			if (!IsValid(VW)) continue;
			if (VW->bCreateRuntimeOnBeginPlay)
			{
				VW->bCreateRuntimeOnBeginPlay = false;
				++VoxelWorldsAdjusted;
			}
			if (VW->IsRuntimeCreated())
			{
				VW->DestroyRuntime();
				++VoxelWorldsDestroyed;
			}
		}
		MOHARVEST_LOG(this, "Seed",
			"Pre-flight AVoxelWorld lockout: bCreateRuntimeOnBeginPlay disabled on %d, runtime destroyed on %d (so our seed-aware CreateRuntime is the only one that fires)",
			VoxelWorldsAdjusted, VoxelWorldsDestroyed);
	}

	MOHARVEST_LOG(this, "Seed",
		"HandlePendingNewGame entry: bPendingNewGame=%d PendingNewGameSlot='%s' PendingWorldSeed=%d bAutoInit=%d",
		Settings->bPendingNewGame ? 1 : 0,
		*Settings->PendingNewGameSlot,
		Settings->PendingWorldSeed,
		bAutoInitializeVoxelWithSeed ? 1 : 0);

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
			MOHARVEST_LOG(this, "Seed", "NEW-GAME path: PendingWorldSeed was 0, generated random=%d", EffectiveSeed);
		}
		else
		{
			MOHARVEST_LOG(this, "Seed", "NEW-GAME path: PendingWorldSeed was %d, using as-is", EffectiveSeed);
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
		MOHARVEST_LOG(this, "Seed", "LOAD path entered, slot='%s'", *SlotToLoad);

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
					UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Save loaded successfully: %d pawns, %d items, %d buildings, seed=%d"),
						Result.PawnsLoaded, Result.ItemsLoaded, Result.BuildingsLoaded, Result.WorldSeed);
					MOHARVEST_LOG(this, "Seed",
						"LOAD result: bSuccess=1 pawns=%d items=%d buildings=%d Result.WorldSeed=%d",
						Result.PawnsLoaded, Result.ItemsLoaded, Result.BuildingsLoaded, Result.WorldSeed);

					// Apply world seed and initialize voxel world if auto-initialization is enabled
					if (bAutoInitializeVoxelWithSeed)
					{
						if (Result.WorldSeed != 0)
						{
							Settings->PendingWorldSeed = Result.WorldSeed;
							FMath::RandInit(Result.WorldSeed);
							UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Applied loaded world seed: %d"), Result.WorldSeed);
							MOHARVEST_LOG(this, "Seed", "LOAD: Applied seed %d to settings, calling InitializeVoxelWorldWithSeed", Result.WorldSeed);
							InitializeVoxelWorldWithSeed();

							// Wait for voxel terrain to generate, then re-ground all loaded pawns
							WaitForVoxelAndRegroundPawns();
						}
						else
						{
							UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] Save has no world seed (0)! Voxel terrain may not match saved positions. Re-save to fix."));
							MOHARVEST_LOG(this, "Seed", "LOAD WARNING: Result.WorldSeed=0! Save did not contain a valid seed - voxel terrain will use cooked default");
							// Still dismiss loading screen since we're not waiting for voxel
							if (UGameInstance* GI = GetGameInstance())
							{
								if (UMOGameInstance* MOGI = Cast<UMOGameInstance>(GI))
								{
									MOGI->DismissLoadingScreen();
								}
							}
						}
					}
					else
					{
						UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] bAutoInitializeVoxelWithSeed is FALSE! Voxel terrain will NOT be regenerated with saved seed. Pawns may fall through world!"));
						// Still dismiss loading screen since we're not waiting for voxel
						if (UGameInstance* GI = GetGameInstance())
						{
							if (UMOGameInstance* MOGI = Cast<UMOGameInstance>(GI))
							{
								MOGI->DismissLoadingScreen();
							}
						}
					}
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

void AMOGameMode::WaitForVoxelAndRegroundPawns()
{
	bPendingRegroundAfterVoxel = true;
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Waiting for voxel world before re-grounding loaded pawns..."));

	// Start polling for voxel readiness
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(
			RegroundPawnsTimerHandle,
			this,
			&AMOGameMode::CheckVoxelReadyAndReground,
			0.5f,
			true  // Looping until voxel is ready
		);
	}
}

void AMOGameMode::CheckVoxelReadyAndReground()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Find the voxel world
	AVoxelWorld* VoxelWorld = nullptr;
	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		VoxelWorld = *It;
		break;
	}

	if (VoxelWorld && VoxelWorld->IsVoxelWorldReady())
	{
		// Stop the polling timer
		World->GetTimerManager().ClearTimer(RegroundPawnsTimerHandle);

		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Voxel world ready, waiting %.1f seconds for collision generation before re-grounding..."),
			CollisionGenerationDelay);

		// Wait for collision generation, then re-ground
		World->GetTimerManager().SetTimer(
			RegroundPawnsTimerHandle,
			this,
			&AMOGameMode::RegroundAllPawns,
			CollisionGenerationDelay,
			false  // Not looping
		);
	}
}

void AMOGameMode::RegroundAllPawns()
{
	bPendingRegroundAfterVoxel = false;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Re-grounding all loaded pawns to terrain..."));

	int32 RegroundedCount = 0;

	// Find all MOCharacters and adjust their Z to be on terrain
	for (TActorIterator<AMOCharacter> It(World); It; ++It)
	{
		AMOCharacter* Character = *It;
		if (!IsValid(Character))
		{
			continue;
		}

		FVector CurrentLocation = Character->GetActorLocation();

		// Trace down from high above current position to find terrain
		FVector TraceStart = FVector(CurrentLocation.X, CurrentLocation.Y, CurrentLocation.Z + 50000.0f);
		FVector TraceEnd = FVector(CurrentLocation.X, CurrentLocation.Y, CurrentLocation.Z - 50000.0f);

		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(Character);

		if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			// Check if we hit voxel terrain
			AActor* HitActor = HitResult.GetActor();
			if (HitActor && HitActor->IsA<AVoxelWorld>())
			{
				// Position slightly above hit point
				FVector NewLocation = HitResult.ImpactPoint + FVector(0.0f, 0.0f, 100.0f);

				if (!FMath::IsNearlyEqual(CurrentLocation.Z, NewLocation.Z, 50.0f))
				{
					Character->SetActorLocation(NewLocation);
					UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Re-grounded %s from Z=%.1f to Z=%.1f"),
						*Character->GetName(), CurrentLocation.Z, NewLocation.Z);
					RegroundedCount++;
				}
			}
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Re-grounded %d pawns to terrain"), RegroundedCount);

	// Auto-possess the pawn the player was controlling at save time. Without
	// this, the player is left as a sky-cam spectator and the saved pawn
	// stands idle on the ground — feels like "respawn way off" because the
	// camera is 50m above where the pawn actually is.
	if (UGameInstance* GI = GetGameInstance())
	{
		UMOPersistenceSubsystem* Persistence = GI->GetSubsystem<UMOPersistenceSubsystem>();
		const FGuid LastGuid = Persistence ? Persistence->GetLastLoadResult().LastPossessedPawnGuid : FGuid();
		if (LastGuid.IsValid())
		{
			AMOCharacter* TargetPawn = nullptr;
			for (TActorIterator<AMOCharacter> It(World); It; ++It)
			{
				if (!IsValid(*It)) continue;
				UMOIdentityComponent* IdComp = (*It)->FindComponentByClass<UMOIdentityComponent>();
				if (IdComp && IdComp->GetGuid() == LastGuid)
				{
					TargetPawn = *It;
					break;
				}
			}
			if (TargetPawn)
			{
				if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
				{
					PC->Possess(TargetPawn);
					UE_LOG(LogMOFramework, Log,
						TEXT("[MOGameMode] Auto-possessed last-played pawn '%s' at %s after load"),
						*TargetPawn->GetName(), *TargetPawn->GetActorLocation().ToString());
				}
			}
			else
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MOGameMode] Could not find pawn with LastPossessedPawnGuid=%s — staying as spectator"),
					*LastGuid.ToString());
			}
		}
	}

	// Dismiss loading screen after re-grounding loaded pawns
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMOGameInstance* MOGI = Cast<UMOGameInstance>(GI))
		{
			MOGI->DismissLoadingScreen();
		}
	}

	// Clear the gameplay transition flag
	if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		Settings->bIsLoadingIntoGameplay = false;
	}
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

	// Assign a random name to the initial character
	if (UMOIdentityComponent* IdentityComp = NewPawn->FindComponentByClass<UMOIdentityComponent>())
	{
		if (IdentityComp->DisplayName.IsEmpty())
		{
			// Name pools for initial character
			static const TArray<FString> FirstNames = {
				TEXT("Alex"), TEXT("Sam"), TEXT("Jordan"), TEXT("Taylor"), TEXT("Morgan"),
				TEXT("Casey"), TEXT("Riley"), TEXT("Quinn"), TEXT("Avery"), TEXT("Parker"),
				TEXT("Emma"), TEXT("Liam"), TEXT("Olivia"), TEXT("Noah"), TEXT("Ava"),
				TEXT("Sophia"), TEXT("Jackson"), TEXT("Isabella"), TEXT("Lucas"), TEXT("Mia")
			};
			static const TArray<FString> LastNames = {
				TEXT("Smith"), TEXT("Johnson"), TEXT("Williams"), TEXT("Brown"), TEXT("Jones"),
				TEXT("Garcia"), TEXT("Miller"), TEXT("Davis"), TEXT("Rodriguez"), TEXT("Martinez"),
				TEXT("Anderson"), TEXT("Taylor"), TEXT("Thomas"), TEXT("Moore"), TEXT("Jackson")
			};

			const FString& FirstName = FirstNames[FMath::RandRange(0, FirstNames.Num() - 1)];
			const FString& LastName = LastNames[FMath::RandRange(0, LastNames.Num() - 1)];
			FString FullName = FString::Printf(TEXT("%s %s"), *FirstName, *LastName);

			IdentityComp->SetDisplayName(FText::FromString(FullName));
			UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Initial pawn assigned name: %s"), *FullName);
		}
	}

	// Mark the initial pawn as recruited (bypasses normal recruitment flow)
	if (UMORecruitmentComponent* RecruitComp = NewPawn->FindComponentByClass<UMORecruitmentComponent>())
	{
		RecruitComp->ForceRecruit();
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Initial pawn marked as recruited"));
	}

	// Possess the pawn with player controller
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		PC->Possess(NewPawn);
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Player controller possessed initial pawn"));

		// Start checking for pawn landing to dismiss loading screen
		PendingLandingPawn = NewPawn;
		LandingCheckTickCount = 0;
		LandingRecoveryAttempts = 0;
		GetWorld()->GetTimerManager().SetTimer(
			PawnLandingTimerHandle,
			this,
			&AMOGameMode::CheckPawnLanded,
			0.1f,  // Check every 100ms
			true   // Loop until landed
		);
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
	// Target beach spawn: between MinSpawnHeightAboveWater and MaxSpawnHeightAboveWater
	const float IdealBeachMaxZ = WaterLevelZ + MaxSpawnHeightAboveWater;

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] FindSafeSpawnLocation: WaterLevelZ=%.1f, MinSpawnZ=%.1f, BeachMaxZ=%.1f, MinSlopeNormalZ=%.2f, VoxelOnly=%d"),
		WaterLevelZ, MinSpawnZ, IdealBeachMaxZ, MinSpawnSurfaceNormalZ, bSpawnOnlyOnVoxelTerrain ? 1 : 0);

	// Track best beach candidate (lowest point within beach height range)
	FVector BestBeachLocation = FVector::ZeroVector;
	float BestBeachZ = FLT_MAX;  // We want the LOWEST point above MinSpawnZ (beach)
	bool bFoundBeach = false;

	// Track lowest valid location as fallback (prefer low elevations even outside beach range)
	FVector BestFallbackLocation = FVector::ZeroVector;
	float BestFallbackZ = FLT_MAX;  // Track LOWEST land, not highest
	bool bFoundLand = false;

	int32 TotalHits = 0;
	int32 HitsAboveWater = 0;
	int32 HitsRejectedNotVoxel = 0;
	int32 HitsRejectedTooSteep = 0;

	FCollisionQueryParams Params;
	Params.bTraceComplex = false;  // Use simple collision for voxel terrain
	Params.bReturnPhysicalMaterial = false;

	// Helper lambda to validate a hit result
	auto IsValidSpawnHit = [this, &HitsRejectedNotVoxel, &HitsRejectedTooSteep](const FHitResult& Hit) -> bool
	{
		// Check if we hit voxel terrain (AVoxelWorld)
		if (bSpawnOnlyOnVoxelTerrain)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || !HitActor->IsA<AVoxelWorld>())
			{
				++HitsRejectedNotVoxel;
				return false;
			}
		}

		// Check slope - surface normal Z component must be above threshold
		// Normal.Z of 1.0 = flat, 0.7 = ~45 degrees, 0.0 = vertical wall
		if (Hit.ImpactNormal.Z < MinSpawnSurfaceNormalZ)
		{
			++HitsRejectedTooSteep;
			return false;
		}

		return true;
	};

	// Search expanding rings until we find a BEACH (not just any land)
	// Keep searching even after finding land - we want to find a beach
	const int32 MaxRings = 20;  // Up to 20 rings expanding outward
	const float RingWidth = SpawnSearchRadius;  // Each ring is this wide
	const int32 SamplesPerRing = MaxSpawnAttempts / 2;  // Samples per ring

	for (int32 Ring = 0; Ring < MaxRings && !bFoundBeach; ++Ring)
	{
		float InnerRadius = Ring * RingWidth;
		float OuterRadius = (Ring + 1) * RingWidth;

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOGameMode] Searching ring %d: %.0f - %.0f cm from center"),
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

				// Validate this hit is on voxel terrain and not too steep
				if (!IsValidSpawnHit(Hit))
				{
					continue;
				}

				// Check if above water level - this is LAND
				if (Hit.Location.Z > MinSpawnZ)
				{
					HitsAboveWater++;
					bFoundLand = true;

					// Check if this is a beach location (within height range)
					if (Hit.Location.Z < IdealBeachMaxZ)
					{
						// Track best (lowest) beach location
						if (Hit.Location.Z < BestBeachZ)
						{
							BestBeachZ = Hit.Location.Z;
							BestBeachLocation = Hit.Location + FVector(0, 0, SpawnHeightOffset);
							bFoundBeach = true;

							// Ideal beach found - low enough to be a good beach, return immediately
							if (Hit.Location.Z < WaterLevelZ + 200.0f)
							{
								FVector SpawnLocation = Hit.Location + FVector(0, 0, SpawnHeightOffset);
								UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Found ideal beach spawn in ring %d: %s (Z=%.1f above water, slope=%.2f)"),
									Ring, *SpawnLocation.ToString(), Hit.Location.Z - WaterLevelZ, Hit.ImpactNormal.Z);
								return SpawnLocation;
							}
						}
					}

					// Track as fallback - prefer LOWER elevations (closer to beach)
					if (Hit.Location.Z < BestFallbackZ)
					{
						BestFallbackZ = Hit.Location.Z;
						BestFallbackLocation = Hit.Location + FVector(0, 0, SpawnHeightOffset);
					}
				}
			}
		}
	}

	// If we found land but no ideal beach, do a focused search around the lowest land we found
	if (bFoundLand && !bFoundBeach && BestFallbackZ < FLT_MAX)
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

				// Validate this hit is on voxel terrain and not too steep
				if (!IsValidSpawnHit(Hit))
				{
					continue;
				}

				if (Hit.Location.Z > MinSpawnZ && Hit.Location.Z < IdealBeachMaxZ && Hit.Location.Z < BestBeachZ)
				{
					BestBeachZ = Hit.Location.Z;
					BestBeachLocation = Hit.Location + FVector(0, 0, SpawnHeightOffset);
					bFoundBeach = true;

					if (Hit.Location.Z < WaterLevelZ + 200.0f)
					{
						UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Found beach near land: %s (Z=%.1f above water, slope=%.2f)"),
							*BestBeachLocation.ToString(), Hit.Location.Z - WaterLevelZ, Hit.ImpactNormal.Z);
						return BestBeachLocation;
					}
				}
			}
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Spawn search stats: TotalHits=%d, AboveWater=%d, RejectedNotVoxel=%d, RejectedSteep=%d, FoundBeach=%d, FoundLand=%d"),
		TotalHits, HitsAboveWater, HitsRejectedNotVoxel, HitsRejectedTooSteep, bFoundBeach ? 1 : 0, bFoundLand ? 1 : 0);

	// Prefer beach location if found
	if (bFoundBeach)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Using beach spawn location: %s (Z=%.1f above water)"),
			*BestBeachLocation.ToString(), BestBeachZ - WaterLevelZ);
		return BestBeachLocation;
	}

	// Use lowest valid land as fallback (even if above beach range, at least it's the lowest we found)
	if (bFoundLand && BestFallbackZ < FLT_MAX)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] No beach found! Using lowest land at Z=%.1f (%.1f above water, max beach=%.1f)"),
			BestFallbackZ, BestFallbackZ - WaterLevelZ, static_cast<double>(MaxSpawnHeightAboveWater));
		return BestFallbackLocation;
	}

	// Absolute fallback: spawn at safe default above water
	FVector FallbackLocation(SpawnSearchCenter.X, SpawnSearchCenter.Y, MinSpawnZ + 500.0f);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] No land found after searching %d rings, using fallback: %s"),
		MaxRings, *FallbackLocation.ToString());
	return FallbackLocation;
}

// ============================================================================
// PAWN LANDING DETECTION
// ============================================================================

void AMOGameMode::CheckPawnLanded()
{
	APawn* Pawn = PendingLandingPawn.Get();
	if (!Pawn)
	{
		GetWorld()->GetTimerManager().ClearTimer(PawnLandingTimerHandle);
		return;
	}

	// Check if character is on ground (not falling)
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		if (Movement && !Movement->IsFalling() && Movement->IsMovingOnGround())
		{
			GetWorld()->GetTimerManager().ClearTimer(PawnLandingTimerHandle);
			OnPawnLandedSafely();
			return;
		}
	}
	else
	{
		// Not a character - just dismiss immediately
		GetWorld()->GetTimerManager().ClearTimer(PawnLandingTimerHandle);
		OnPawnLandedSafely();
		return;
	}

	// Pawn isn't landed yet — bump the tick counter and check for stuck recovery.
	// Timer fires every 0.1s, so MaxLandingWaitSeconds maps to ticks at 10/sec.
	++LandingCheckTickCount;
	const int32 TicksBeforeRecovery = FMath::Max(1, FMath::RoundToInt(MaxLandingWaitSeconds * 10.0f));
	if (LandingCheckTickCount >= TicksBeforeRecovery)
	{
		LandingCheckTickCount = 0;
		RecoverStuckSpawn();
	}
}

void AMOGameMode::RecoverStuckSpawn()
{
	APawn* Pawn = PendingLandingPawn.Get();
	if (!Pawn)
	{
		GetWorld()->GetTimerManager().ClearTimer(PawnLandingTimerHandle);
		return;
	}

	++LandingRecoveryAttempts;
	const FVector CurrentLocation = Pawn->GetActorLocation();

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOGameMode] Pawn stuck at %s after %.1fs (attempt %d/%d) — recovering"),
		*CurrentLocation.ToString(), MaxLandingWaitSeconds,
		LandingRecoveryAttempts, MaxLandingRecoveryAttempts);

	// Reset velocity so the pawn doesn't keep its bad momentum after teleport.
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->Velocity = FVector::ZeroVector;
		}
	}

	FVector RecoveryLocation;

	if (LandingRecoveryAttempts == 1)
	{
		// Attempt 1: lift the pawn up. Maybe it spawned inside terrain or just
		// below the surface — bumping up by a known offset usually unsticks it.
		RecoveryLocation = CurrentLocation + FVector(0, 0, RecoveryLiftOffset);
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOGameMode]   Recovery 1: lifting pawn by %.0f to %s"),
			RecoveryLiftOffset, *RecoveryLocation.ToString());
	}
	else if (LandingRecoveryAttempts == 2)
	{
		// Attempt 2: re-run the spawn search. Different random samples may find
		// a different (better) location, even with the same parameters.
		RecoveryLocation = FindSafeSpawnLocation();
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOGameMode]   Recovery 2: re-searched, new spawn at %s"),
			*RecoveryLocation.ToString());
	}
	else if (LandingRecoveryAttempts <= MaxLandingRecoveryAttempts)
	{
		// Attempt 3+: shift the search center by a random horizontal offset and
		// re-search. Useful when the entire search area is bad (in water, in a
		// chunk that hasn't streamed yet, etc).
		const float ShiftDistance = SpawnSearchRadius * 2.0f;
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const FVector OriginalCenter = SpawnSearchCenter;
		SpawnSearchCenter += FVector(
			FMath::Cos(Angle) * ShiftDistance,
			FMath::Sin(Angle) * ShiftDistance,
			0.0f);
		RecoveryLocation = FindSafeSpawnLocation();
		SpawnSearchCenter = OriginalCenter;  // Restore for next time
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOGameMode]   Recovery %d: shifted search center by %.0f, new spawn at %s"),
			LandingRecoveryAttempts, ShiftDistance, *RecoveryLocation.ToString());
	}
	else
	{
		// All recovery strategies exhausted — drop the pawn at an unambiguously
		// safe Z above the water at the configured search center. The pawn may
		// fall a long way but it won't get stuck.
		RecoveryLocation = FVector(
			SpawnSearchCenter.X,
			SpawnSearchCenter.Y,
			WaterLevelZ + MinSpawnHeightAboveWater + 10000.0f);
		UE_LOG(LogMOFramework, Error,
			TEXT("[MOGameMode]   Hard fallback after %d recovery attempts — placing pawn at %s"),
			MaxLandingRecoveryAttempts, *RecoveryLocation.ToString());

		// Give up the timer loop after this final placement; if the pawn still
		// can't land here, the world is broken and polling won't help.
		Pawn->SetActorLocation(RecoveryLocation, /*bSweep=*/false, nullptr,
			ETeleportType::TeleportPhysics);
		GetWorld()->GetTimerManager().ClearTimer(PawnLandingTimerHandle);
		OnPawnLandedSafely();
		return;
	}

	Pawn->SetActorLocation(RecoveryLocation, /*bSweep=*/false, nullptr,
		ETeleportType::TeleportPhysics);
}

void AMOGameMode::OnPawnLandedSafely()
{
	APawn* Pawn = PendingLandingPawn.Get();
	PendingLandingPawn.Reset();

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Pawn landed safely at %s"),
		Pawn ? *Pawn->GetActorLocation().ToString() : TEXT("NULL"));

	// 1. Dismiss loading screen
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMOGameInstance* MOGI = Cast<UMOGameInstance>(GI))
		{
			MOGI->DismissLoadingScreen();
		}
	}

	// 2. Clear the gameplay transition flag
	if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		Settings->bIsLoadingIntoGameplay = false;
	}
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

				// 1) Set the StampSeed (placement-RNG field).
				StampRef->StampSeed.Seed = VoxelSeedString;
				StampsUpdated++;

				UE_LOG(LogMOFramework, Verbose, TEXT("[MOGameMode] Stamp in '%s': StampSeed '%s' -> '%s'"),
					*Actor->GetName(), *OldSeed, *VoxelSeedString);

				// 2) CRITICAL FIX (2026-05): If this stamp is a HeightGraphStamp
				// (the level's world-gen stamp using VHG_Flat), it has its OWN
				// "Seed" parameter override that drives terrain generation —
				// SEPARATE from the StampSeed above. The graph asset's own
				// Seed override is IGNORED in favor of the stamp's override.
				// Per the user's editor screenshot, the world-gen stamp has
				// a "Seed" parameter set inside the stamp itself (not on the
				// underlying graph asset). Need to call SetParameter on the
				// stamp via its IVoxelParameterOverridesOwner interface.
				if (StampRef.IsA<FVoxelHeightGraphStamp>())
				{
					FVoxelHeightGraphStamp* HGStamp = StampRef.As<FVoxelHeightGraphStamp>();
					if (HGStamp)
					{
						IVoxelParameterOverridesOwner* StampOwner = static_cast<IVoxelParameterOverridesOwner*>(HGStamp);
						if (StampOwner->HasParameter(VoxelSeedParameterName))
						{
							FVoxelExposedSeed StampSeedValue;
							StampSeedValue.Seed = VoxelSeedString;
							FString StampError;
							if (StampOwner->SetParameter(VoxelSeedParameterName, FVoxelPinValue::Make(StampSeedValue), &StampError))
							{
								MOHARVEST_LOG(this, "Seed",
									"  HGStamp '%s': Set 'Seed' param='%s' (stamp's overrides now=%d) — THIS is the world-gen seed",
									*Actor->GetName(), *VoxelSeedString,
									StampOwner->GetParameterOverrides().GuidToValueOverride.Num());
							}
							else
							{
								MOHARVEST_LOG(this, "Seed",
									"  HGStamp '%s': FAILED to set Seed: %s",
									*Actor->GetName(), *StampError);
							}
						}
						else
						{
							MOHARVEST_LOG(this, "Seed",
								"  HGStamp '%s': no 'Seed' parameter on this stamp",
								*Actor->GetName());
						}

						// Tell the runtime the stamp changed so it re-runs.
						StampRef.Update();
					}
				}
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

	// Apply seed to all stamp components (for runtime stamps)
	const int32 StampsUpdated = ApplySeedToVoxelStamps(WorldSeed);

	// Apply seed to height graph parameters (for base terrain generation)
	const bool bGraphParameterSet = ApplySeedToHeightGraphParameter(WorldSeed);

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

	// Check if runtime is already created (mid-game load scenario)
	if (VoxelWorld->IsRuntimeCreated())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] VoxelWorld runtime already exists - destroying and recreating with seed %d"), WorldSeed);
		VoxelWorld->DestroyRuntime();

		// Re-apply seed parameters after destroying runtime (they may have been cleared)
		ApplySeedToVoxelStamps(WorldSeed);
		ApplySeedToHeightGraphParameter(WorldSeed);
	}

	// Create the runtime to start generation with the new seed
	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Creating VoxelWorld runtime with seed %d (stamps=%d, graphParam=%s)"),
		WorldSeed, StampsUpdated, bGraphParameterSet ? TEXT("SET") : TEXT("NOT SET"));
	MOHARVEST_LOG(this, "Seed",
		"CreateRuntime: seed=%d stamps=%d graphParamSet=%d",
		WorldSeed, StampsUpdated, bGraphParameterSet ? 1 : 0);
	VoxelWorld->CreateRuntime();
}

bool AMOGameMode::ApplySeedToHeightGraphParameter(int32 WorldSeed)
{
	// Create the seed value in Voxel's expected format
	FVoxelExposedSeed SeedValue;
	SeedValue.Seed = IntSeedToVoxelSeedString(WorldSeed);

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Attempting to set seed parameter '%s' = '%s' on height graphs"),
		*VoxelSeedParameterName.ToString(), *SeedValue.Seed);

	// PACKAGED-BUILD FIX (2026-05): In packaged builds, UVoxelHeightGraph
	// assets are loaded LAZILY — the height graph referenced by the level's
	// VoxelWorld actor may not be in memory yet when this runs (the level
	// has spawned the actor but the actor's CreateRuntime() hasn't pulled
	// in the graph reference yet). TObjectIterator below only sees
	// in-memory objects, so without an explicit pre-load it returns 0
	// graphs in packaged → seed silently doesn't apply → terrain
	// regenerates with the default seed baked into the cooked graph →
	// saved voxel sculpt data lands at world positions that no longer match
	// the heightmap (looks like a pit).
	//
	// Force-load every cooked UVoxelHeightGraph asset via Asset Registry
	// before iterating. The user's project has only one or two graphs, so
	// the cost is negligible and the iteration below is now guaranteed to
	// see them.
	int32 PreloadCount = 0;
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> GraphAssets;
		AssetRegistry.GetAssetsByClass(UVoxelHeightGraph::StaticClass()->GetClassPathName(), GraphAssets, /*bSearchSubClasses=*/true);
		MOHARVEST_LOG(this, "Seed", "ApplySeedToHeightGraphParameter: AssetRegistry returned %d UVoxelHeightGraph assets", GraphAssets.Num());
		for (const FAssetData& AD : GraphAssets)
		{
			// .GetAsset() forces synchronous load if not already in memory
			UObject* Loaded = AD.GetAsset();
			UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Pre-loaded height graph for seed application: %s (%s)"),
				*AD.AssetName.ToString(), Loaded ? TEXT("ok") : TEXT("FAILED"));
			MOHARVEST_LOG(this, "Seed", "Pre-load attempt: %s -> %s", *AD.AssetName.ToString(), Loaded ? TEXT("OK") : TEXT("NULL"));
			if (Loaded) ++PreloadCount;
		}
	}

	int32 GraphsUpdated = 0;
	int32 GraphsChecked = 0;

	// Iterate through all loaded UVoxelHeightGraph assets and set the seed parameter
	// UVoxelGraph (parent of UVoxelHeightGraph) implements IVoxelParameterOverridesObjectOwner
	// which provides the SetParameter method
	for (TObjectIterator<UVoxelHeightGraph> It; It; ++It)
	{
		UVoxelHeightGraph* Graph = *It;
		if (!Graph)
		{
			continue;
		}

		// Skip transient/template objects
		if (Graph->HasAnyFlags(RF_Transient | RF_ClassDefaultObject))
		{
			continue;
		}

		GraphsChecked++;

		// Check if this graph has a parameter with the expected name
		if (!Graph->HasParameter(VoxelSeedParameterName))
		{
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOGameMode] Graph '%s' has no parameter named '%s'"),
				*Graph->GetName(), *VoxelSeedParameterName.ToString());
			continue;
		}

		// Set the parameter value
		// UVoxelGraph implements IVoxelParameterOverridesObjectOwner which provides SetParameter
		FString Error;
		const FVoxelPinValue PinValue = FVoxelPinValue::Make(SeedValue);

		if (Graph->SetParameter(VoxelSeedParameterName, PinValue, &Error))
		{
			GraphsUpdated++;
			UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Set seed='%s' on HeightGraph '%s'"),
				*SeedValue.Seed, *Graph->GetName());
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] Failed to set seed on '%s': %s"),
				*Graph->GetName(), *Error);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Seed parameter set on %d/%d height graphs"),
		GraphsUpdated, GraphsChecked);
	MOHARVEST_LOG(this, "Seed",
		"ApplySeedToHeightGraphParameter result: seedString='%s' preloaded=%d graphsChecked=%d graphsUpdated=%d",
		*SeedValue.Seed, PreloadCount, GraphsChecked, GraphsUpdated);

	// DIAGNOSTIC: dump the FULL parameter list AND the current override map
	// on every UVoxelGraph. Same seed string applied in both new-game and
	// load produced different terrain. Now logging the OVERRIDE MAP (which
	// is what the runtime actually reads) — if it differs between sessions
	// despite identical SetParameter calls, that proves the divergence is
	// in another override owner (a stamp, a scatter actor, etc) that we're
	// not seeing.
	for (TObjectIterator<UVoxelGraph> GraphIt; GraphIt; ++GraphIt)
	{
		UVoxelGraph* G = *GraphIt;
		if (!G) continue;
		if (G->HasAnyFlags(RF_Transient | RF_ClassDefaultObject)) continue;
		const int32 ParamCount = G->NumParameters();
		const FVoxelParameterOverrides& Overrides = G->GetParameterOverrides();
		MOHARVEST_LOG(this, "Seed", "ParamDump graph '%s' (class=%s, params=%d, overrides=%d):",
			*G->GetName(), *G->GetClass()->GetName(), ParamCount, Overrides.GuidToValueOverride.Num());
		G->ForeachParameter([this, G](const FGuid& Guid, const FVoxelParameter& Param)
		{
			MOHARVEST_LOG(this, "Seed", "  '%s' name='%s' type='%s'",
				*G->GetName(), *Param.Name.ToString(),
				*Param.Type.ToString());
		});
		for (const auto& OPair : Overrides.GuidToValueOverride)
		{
			MOHARVEST_LOG(this, "Seed",
				"  override guid=%s enable=%d valueType='%s'",
				*OPair.Key.ToString(), OPair.Value.bEnable ? 1 : 0,
				*OPair.Value.Value.GetType().ToString());
		}
	}

	// Also enumerate all UObjects implementing IVoxelParameterOverridesObjectOwner
	// — these are stamp components, scatter actors, etc that have their OWN
	// override maps that take precedence over the graph asset's defaults.
	// If the terrain bug is from one of these, we'll see it here.
	int32 OwnerCount = 0;
	for (TObjectIterator<UObject> ObjIt; ObjIt; ++ObjIt)
	{
		UObject* Obj = *ObjIt;
		if (!Obj) continue;
		if (Obj->HasAnyFlags(RF_Transient | RF_ClassDefaultObject)) continue;
		if (!Obj->Implements<UVoxelParameterOverridesObjectOwner>()) continue;

		IVoxelParameterOverridesObjectOwner* OwnerObj = Cast<IVoxelParameterOverridesObjectOwner>(Obj);
		if (!OwnerObj) continue;
		IVoxelParameterOverridesOwner* ParamOwner = static_cast<IVoxelParameterOverridesOwner*>(OwnerObj);
		++OwnerCount;
		const UVoxelGraph* OwnerGraph = ParamOwner->GetGraph();
		const FVoxelParameterOverrides& OwnerOverrides = ParamOwner->GetParameterOverrides();
		MOHARVEST_LOG(this, "Seed",
			"ParamOwner #%d: obj='%s' class='%s' graph='%s' overrides=%d",
			OwnerCount, *Obj->GetName(), *Obj->GetClass()->GetName(),
			OwnerGraph ? *OwnerGraph->GetName() : TEXT("<null>"),
			OwnerOverrides.GuidToValueOverride.Num());

		// If this owner has a "Seed" parameter, apply our seed to it too —
		// the runtime may use this owner's override chain instead of the
		// graph asset's own defaults.
		if (ParamOwner->HasParameter(VoxelSeedParameterName))
		{
			FString OwnerError;
			if (ParamOwner->SetParameter(VoxelSeedParameterName, FVoxelPinValue::Make(SeedValue), &OwnerError))
			{
				MOHARVEST_LOG(this, "Seed",
					"  -> applied Seed='%s' to owner '%s' (its overrides now=%d)",
					*SeedValue.Seed, *Obj->GetName(),
					ParamOwner->GetParameterOverrides().GuidToValueOverride.Num());
			}
			else
			{
				MOHARVEST_LOG(this, "Seed",
					"  -> FAILED to apply Seed on owner '%s': %s",
					*Obj->GetName(), *OwnerError);
			}
		}
	}
	MOHARVEST_LOG(this, "Seed", "Total IVoxelParameterOverridesObjectOwner instances found: %d", OwnerCount);

	return GraphsUpdated > 0;
}
