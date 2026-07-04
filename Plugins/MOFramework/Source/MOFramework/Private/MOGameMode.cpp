#include "MOGameMode.h"
#include "MOAudioSubsystem.h"
#include "MOFramework.h"
#include "MOVoxelReadinessSubsystem.h"
#include "MOCharacter.h"
#include "MORecruitmentComponent.h"
#include "MOIdentityComponent.h"
#include "MOSpawnManagerSubsystem.h" // (H51) GenerateRandomSurvivorName — shared name generator
#include "MOPCGInteractionSubsystem.h"
#include "MOPersistenceSubsystem.h"
#include "MOQuestSubsystem.h"
#include "MOGameSettings.h"
#include "MOGameInstance.h"
#include "MOHarvestDebugSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
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

	// Co-op: clients must FOLLOW the host through ServerTravel. Non-seamless
	// travel drops connected clients (mptest evidence 2026-07-03: host PCs 2->1,
	// client parked at LoadingLevel), and is unreliable in one-process PIE
	// anyway. Seamless travel keeps the connection alive through map change —
	// also the right shipping behavior (no disconnect/reconnect hitch).
	// NOTE: traveling players arrive via HandleSeamlessTravelPlayer, NOT
	// Login/PostLogin — any join-flow logic added there must handle both paths.
	// AMOMainMenuGameMode inherits this, which is what drives menu->game travel.
	bUseSeamlessTravel = true;
}

void AMOGameMode::BeginPlay()
{
	Super::BeginPlay();

	RegisterPCGTagMappings();

	// Notify audio subsystem that the world (and this GameMode) is ready.
	// At Initialize time the audio subsystem can't reliably distinguish menu
	// vs gameplay because GameMode hasn't been spawned yet — by here it has.
	// HandleWorldAudioContext is idempotent (SetMusicState early-returns
	// when state already matches), safe to call alongside PostLoadMap.
	if (UWorld* World = GetWorld())
	{
		if (UMOAudioSubsystem* Audio = UMOAudioSubsystem::Get(World))
		{
			Audio->HandleWorldAudioContext(World);
		}
	}

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

		// A previous session's loaded save must not leak into this new world:
		// reset GameInstance-scoped persistence (loaded save, destroyed-GUID
		// ledger, playtime — handing it this world's slot) and quest state
		// (ResetAllQuests re-runs the tutorial auto-starts itself).
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>())
			{
				Persistence->ResetForNewWorld(Settings->PendingNewGameSlot);
			}
			if (UMOQuestSubsystem* Quests = GameInstance->GetSubsystem<UMOQuestSubsystem>())
			{
				Quests->ResetAllQuests();
			}
		}

		// If auto-initialization is enabled, apply seed to voxel stamps and create runtime
		if (bAutoInitializeVoxelWithSeed)
		{
			InitializeVoxelWorldWithSeed();
		}

		// Clear the pending flags — the slot too, otherwise a later boot
		// without bPendingNewGame falls into the load branch with this
		// stale slot persisted in config.
		Settings->bPendingNewGame = false;
		Settings->PendingNewGameSlot.Empty();
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
				// #158: an in-game load carries a LIVE prior session; the new-game branch above
				// resets GameInstance persistence for exactly this reason, but the load branch never
				// did -- so the prior session's loaded-save ref / destroyed-GUID ledger / slot name
				// leaked into the load and it failed. Main-menu load worked only because a fresh boot
				// already had clean state. ResetForNewWorld clears in-memory session state ONLY (it
				// does NOT touch the save file), so it is safe immediately before loading this slot.
				Persistence->ResetForNewWorld(SlotToLoad);
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
					// Load failed (missing/corrupt .sav). The pre-flight lockout already
					// destroyed the voxel runtime and ResetForNewWorld wiped the live session,
					// so dismissing the loading screen here is the ONLY thing preventing a
					// permanent opaque freeze with no exit but killing the process (audit [C]).
					// Dismiss so the player can Esc -> Exit to Menu and pick another save.
					// (Fuller auto-return-to-menu recovery is a tracked follow-up.)
					UE_LOG(LogMOFramework, Error, TEXT("[MOGameMode] Failed to load save: %s -- dismissing loading screen so the player isn't stuck"), *Result.ErrorMessage);
					if (UMOGameInstance* MOGI = Cast<UMOGameInstance>(GetGameInstance()))
					{
						MOGI->DismissLoadingScreen();
					}
				}
			}
			else
			{
				UE_LOG(LogMOFramework, Error, TEXT("[MOGameMode] No persistence subsystem found! -- dismissing loading screen so the player isn't stuck"));
				if (UMOGameInstance* MOGI = Cast<UMOGameInstance>(GetGameInstance()))
				{
					MOGI->DismissLoadingScreen();
				}
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

	UMOVoxelReadinessSubsystem* Voxel = UMOVoxelReadinessSubsystem::Get(World);
	if (!Voxel)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] No VoxelReadinessSubsystem, spawning immediately"));
		SpawnInitialPawn();
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Subscribing to OnVoxelReady for initial pawn spawn"));

	// Subscribe FIRST, then BeginPolling — the subsystem doesn't replay events
	// for late subscribers, so order matters. AddUObject is safe to call
	// multiple times if we somehow re-enter; the subsystem broadcasts to all
	// listeners on the next ready transition.
	Voxel->OnVoxelReady.AddUObject(this, &AMOGameMode::HandleVoxelReadyForNewGame);
	Voxel->BeginPolling();
}

void AMOGameMode::HandleVoxelReadyForNewGame()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] OnVoxelReady received — spawning initial pawn"));

	// Debug: Verify stamp seeds after voxel world is ready
	DebugLogVoxelStampSeeds();

	SpawnInitialPawn();
}

void AMOGameMode::WaitForVoxelAndRegroundPawns()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Subscribing to OnVoxelReady for pawn regrounding..."));

	// Suppress fall-through safety on every MOCharacter for the duration of
	// the voxel-load + regrounding window. Without this, the per-character
	// CheckFallThroughSafety tick sees the pawn falling (no voxel collision
	// yet) and teleports it ~20m off its saved spot via FindSafeTerrainNearLocation.
	int32 SuppressedCount = 0;
	for (TActorIterator<AMOCharacter> It(World); It; ++It)
	{
		if (AMOCharacter* C = *It)
		{
			C->bSuppressFallThroughDuringLoad = true;
			++SuppressedCount;
		}
	}
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOGameMode] Suppressed fall-through safety on %d characters during load"),
		SuppressedCount);

	UMOVoxelReadinessSubsystem* Voxel = UMOVoxelReadinessSubsystem::Get(World);
	if (!Voxel)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOGameMode] No VoxelReadinessSubsystem on load — regrounding immediately"));
		RegroundAllPawns();
		bWorldReadyForJoins = true;
		FlushPendingJoinControllers();
		return;
	}

	Voxel->OnVoxelReady.AddUObject(this, &AMOGameMode::HandleVoxelReadyForLoad);
	Voxel->BeginPolling();
}

void AMOGameMode::HandleVoxelReadyForLoad()
{
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOGameMode] OnVoxelReady received — regrounding loaded pawns"));
	RegroundAllPawns();

	// Loaded world is now spawnable for remote joins (co-op join-spawn, S0).
	bWorldReadyForJoins = true;
	FlushPendingJoinControllers();
}

void AMOGameMode::RegroundAllPawns()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] === RegroundAllPawns: ENTRY (after voxel ready + %.1fs collision delay) ==="),
		CollisionGenerationDelay);

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
		// [DIAG] log every character's location at regrounding-entry time so
		// we can compare with the spawn-time location in persistence logs.
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOGameMode]   pre-reground: %s at %s"),
			*Character->GetName(), *CurrentLocation.ToString());

		// Trace narrow window above/below the saved position to find local
		// terrain. Narrow trace prevents accidentally hitting distant terrain
		// (cliff above, void below) and snapping the pawn to it.
		FVector TraceStart = FVector(CurrentLocation.X, CurrentLocation.Y, CurrentLocation.Z + RegroundTraceHalfHeight);
		FVector TraceEnd = FVector(CurrentLocation.X, CurrentLocation.Y, CurrentLocation.Z - RegroundTraceHalfHeight);

		// (H52) Use a MULTI-hit trace and pick the voxel surface NEAREST the
		// saved Z, not the topmost. A single-hit downward trace returns the
		// first (highest) surface, so a pawn saved inside a dug tunnel/cave
		// would be re-snapped to the hill above it. Caves/mining are a project
		// pillar — the floor the pawn was actually standing on is the one
		// closest to its saved Z within the search window.
		TArray<FHitResult> HitResults;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(Character);

		if (World->LineTraceMultiByChannel(HitResults, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			const FHitResult* BestVoxelHit = nullptr;
			float BestZDelta = TNumericLimits<float>::Max();
			for (const FHitResult& Candidate : HitResults)
			{
				if (!Candidate.bBlockingHit)
				{
					continue;
				}
				AActor* CandidateActor = Candidate.GetActor();
				if (!CandidateActor || !CandidateActor->IsA<AVoxelWorld>())
				{
					continue;
				}
				const float ZDelta = FMath::Abs(Candidate.ImpactPoint.Z - CurrentLocation.Z);
				if (ZDelta < BestZDelta)
				{
					BestZDelta = ZDelta;
					BestVoxelHit = &Candidate;
				}
			}

			if (BestVoxelHit)
			{
				// Land snug on terrain — small lift just clears collision.
				FVector NewLocation = BestVoxelHit->ImpactPoint + FVector(0.0f, 0.0f, RegroundLiftAboveTerrain);

				// Only relocate if the mismatch is meaningful. Saved position is
				// authoritative — wiggles smaller than RegroundTriggerThreshold
				// stay exactly where the player saved.
				const float ZMismatch = FMath::Abs(CurrentLocation.Z - NewLocation.Z);
				if (ZMismatch > RegroundTriggerThreshold)
				{
					Character->SetActorLocation(NewLocation);
					UE_LOG(LogMOFramework, Warning,
						TEXT("[MOGameMode] Re-grounded %s: Z drift was %.1fcm (threshold %.0f) — moved from Z=%.1f to Z=%.1f"),
						*Character->GetName(), ZMismatch, RegroundTriggerThreshold,
						CurrentLocation.Z, NewLocation.Z);
					RegroundedCount++;
				}
				else
				{
					UE_LOG(LogMOFramework, Verbose,
						TEXT("[MOGameMode] Preserved saved position for %s — Z drift only %.1fcm"),
						*Character->GetName(), ZMismatch);
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
					// [DIAG] dump location IMMEDIATELY before Possess.
					const FVector PrePossess = TargetPawn->GetActorLocation();
					PC->Possess(TargetPawn);
					const FVector PostPossess = TargetPawn->GetActorLocation();

					UE_LOG(LogMOFramework, Warning,
						TEXT("[MOGameMode] Auto-possessed last-played pawn '%s'  PrePossess=%s  PostPossess=%s  (possess delta: %.1fcm)"),
						*TargetPawn->GetName(),
						*PrePossess.ToString(),
						*PostPossess.ToString(),
						FVector::Dist(PrePossess, PostPossess));

					// Schedule a 1-second "settling" log so we can see if the pawn
					// keeps moving (physics drift, character movement, etc.) AFTER
					// possession completes.
					TWeakObjectPtr<APawn> WeakTarget = TargetPawn;
					FTimerHandle SettlingHandle;
					World->GetTimerManager().SetTimer(SettlingHandle,
						FTimerDelegate::CreateLambda([WeakTarget, PostPossess]()
						{
							if (APawn* P = WeakTarget.Get())
							{
								UE_LOG(LogMOFramework, Warning,
									TEXT("[MOGameMode]   pawn '%s' settled at %s (drift from post-possess: %.1fcm)"),
									*P->GetName(),
									*P->GetActorLocation().ToString(),
									FVector::Dist(P->GetActorLocation(), PostPossess));
							}
						}),
						1.0f, false);
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

	// Find the player pawn — we'll wait on its grounded state before dropping
	// the loading screen. Without this, players see ~2-5s of all pawns falling
	// before voxel collision settles.
	APawn* PlayerPawn = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMOPersistenceSubsystem* Persistence = GI->GetSubsystem<UMOPersistenceSubsystem>())
		{
			const FGuid LastGuid = Persistence->GetLastLoadResult().LastPossessedPawnGuid;
			if (LastGuid.IsValid())
			{
				for (TActorIterator<AMOCharacter> It(World); It; ++It)
				{
					if (!IsValid(*It)) continue;
					UMOIdentityComponent* IdComp = (*It)->FindComponentByClass<UMOIdentityComponent>();
					if (IdComp && IdComp->GetGuid() == LastGuid)
					{
						PlayerPawn = *It;
						break;
					}
				}
			}
		}
	}

	if (IsValid(PlayerPawn))
	{
		// Keep loading screen + fall-through suppression up until pawn lands.
		BeginPlayerLandingPoll(PlayerPawn);
	}
	else
	{
		// No player pawn to wait on — fall through to immediate handoff.
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOGameMode] No player pawn found for landing wait — finishing load handoff immediately"));
		FinishLoadHandoff();
	}
}

void AMOGameMode::BeginPlayerLandingPoll(APawn* PlayerPawn)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(PlayerPawn))
	{
		FinishLoadHandoff();
		return;
	}

	WaitingForLandingPawn = PlayerPawn;
	PlayerLandingPollStartedAtSeconds = World->GetTimeSeconds();
	LastTerrainProbeZ = -FLT_MAX;
	ConsecutiveStableTerrainTicks = 0;

	// === Spawn-and-wait, anchored to saved location ===
	// 1. Cache saved X/Y/Z so the probe trace doesn't drift if the pawn
	//    moves for any reason during the wait.
	// 2. Place pawn at saved Z + 25cm clearance.
	// 3. Disable character movement entirely (MOVE_None) — no gravity, no
	//    floor sweep, no falling. Pawn hovers in place and acts as the
	//    voxel-world INVOKER — voxel terrain streams + generates collision
	//    around it because that's where the player is.
	// 4. Poll-trace through a wide vertical window around saved Z for terrain.
	// 5. When terrain found within range: snap pawn to terrain + clearance,
	//    restore MOVE_Walking, finish.
	// 6. ABSOLUTE LAST RESORT after timeout: FindSafeSpawnLocation.
	//
	// Saved position is authoritative — no falling, no lateral relocation,
	// no physics surprises. AMOCharacter::CheckFallThroughSafety stays
	// suppressed for the entire wait (true in-game fall-through only).
	const FVector OriginalLocation = PlayerPawn->GetActorLocation();
	SavedAnchorXY = FVector(OriginalLocation.X, OriginalLocation.Y, 0.0f);
	SavedAnchorZ = OriginalLocation.Z;
	const FVector SpawnPos = OriginalLocation + FVector(0.0f, 0.0f, PlayerLandingSpawnOffset);

	// Disable physics/movement entirely. Pawn becomes inert.
	if (ACharacter* Character = Cast<ACharacter>(PlayerPawn))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->Velocity = FVector::ZeroVector;
			Movement->SetMovementMode(MOVE_None);
		}
	}

	PlayerPawn->SetActorLocation(SpawnPos, /*bSweep=*/false, nullptr,
		ETeleportType::TeleportPhysics);

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOGameMode] Spawn-and-wait: '%s' placed at %s (+%.0fcm over save). Physics OFF — waiting for terrain (max %.1fs)."),
		*PlayerPawn->GetName(), *SpawnPos.ToString(),
		PlayerLandingSpawnOffset, PlayerLandingMaxWaitSeconds);

	// Poll every 250ms — fast enough that the wait feels short once terrain
	// loads, cheap enough to not matter.
	World->GetTimerManager().SetTimer(
		PlayerLandingPollHandle,
		this,
		&AMOGameMode::PollPlayerLanding,
		0.25f,
		true);
}

void AMOGameMode::PollPlayerLanding()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		FinishLoadHandoff();
		return;
	}

	APawn* Pawn = WaitingForLandingPawn.Get();
	if (!IsValid(Pawn))
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOGameMode] Landing-wait pawn became invalid — finishing load handoff"));
		FinishLoadHandoff();
		return;
	}

	const float Elapsed = World->GetTimeSeconds() - PlayerLandingPollStartedAtSeconds;

	// Use a CAPSULE SWEEP matching the pawn's capsule, on ECC_Pawn channel —
	// this is exactly how UCharacterMovementComponent probes for floor when
	// other characters (deer, wolves) land on terrain. If they land, this
	// sweep will find the same surface they're landing on.
	//
	// CRITICAL: bTraceComplex = false. Voxel Plugin uses SIMPLE collision
	// for terrain (per existing code comment in FindSafeSpawnLocation).
	// Complex traces miss the voxel surface entirely.
	float CapsuleRadius = 34.0f;
	float CapsuleHalfHeight = 88.0f;
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			CapsuleRadius = Capsule->GetScaledCapsuleRadius();
			CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	// (H52) Honor the configured search distance instead of a hard 20000cm
	// floor. The FMath::Max(..., 20000.0f) that used to live here made the
	// documented ±PlayerLandingTerrainSearchDistance config dead — the probe
	// always reached ±200m regardless of the setting, which is exactly the
	// behavior the property exists to constrain (it warns against picking up
	// "wrong" terrain in caves/underground voids).
	const float SearchHalfRange = PlayerLandingTerrainSearchDistance;
	const FVector SweepStart(SavedAnchorXY.X, SavedAnchorXY.Y, SavedAnchorZ + SearchHalfRange);
	const FVector SweepEnd  (SavedAnchorXY.X, SavedAnchorXY.Y, SavedAnchorZ - SearchHalfRange);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Pawn);
	QueryParams.bTraceComplex = false; // voxel uses SIMPLE collision

	const FCollisionShape CapsuleShape =
		FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);

	// (H52) Sweep for ALL blocking surfaces in the window and keep the one
	// NEAREST the saved anchor Z, not the topmost. A single downward sweep
	// returns the first (highest) surface, so a save made inside a dug
	// tunnel/cave re-snapped the pawn to the hill surface above it every poll
	// (~250ms). Caves/mining are a project pillar — the surface the player
	// actually saved on is the one closest to SavedAnchorZ.
	TArray<FHitResult> Hits;
	const bool bAnyHit = World->SweepMultiByChannel(
		Hits, SweepStart, SweepEnd, FQuat::Identity,
		ECC_Pawn, CapsuleShape, QueryParams);

	FHitResult Hit;
	bool bHit = false;
	if (bAnyHit)
	{
		float BestZDelta = TNumericLimits<float>::Max();
		for (const FHitResult& Candidate : Hits)
		{
			if (!Candidate.bBlockingHit)
			{
				continue;
			}
			const float ZDelta = FMath::Abs(Candidate.ImpactPoint.Z - SavedAnchorZ);
			if (ZDelta < BestZDelta)
			{
				BestZDelta = ZDelta;
				Hit = Candidate;
				bHit = true;
			}
		}
	}

	// Re-snap pawn to detected terrain each tick. As voxel LODs refine, the
	// surface Z may shift slightly; following it keeps the pawn from being
	// stuck inside terrain when LOD finishes.
	if (bHit && Hit.bBlockingHit)
	{
		const FVector LandedPos = Hit.ImpactPoint + FVector(0.0f, 0.0f, PlayerLandingSpawnOffset);
		Pawn->SetActorLocation(LandedPos, /*bSweep=*/false, nullptr,
			ETeleportType::TeleportPhysics);

		// Track stability — terrain Z within threshold of previous reading.
		const bool bStableTick = (LastTerrainProbeZ != -FLT_MAX) &&
			(FMath::Abs(Hit.ImpactPoint.Z - LastTerrainProbeZ) <= LandingTerrainStableThreshold);
		LastTerrainProbeZ = Hit.ImpactPoint.Z;

		if (bStableTick)
		{
			++ConsecutiveStableTerrainTicks;
		}
		else
		{
			ConsecutiveStableTerrainTicks = 0;
		}

		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOGameMode] terrain-probe @ %.2fs / %.1fs  hit Z=%.1f (Δ from prev: %.1f)  stable=%d/%d  hitActor=%s"),
			Elapsed, PlayerLandingMaxWaitSeconds,
			Hit.ImpactPoint.Z,
			bStableTick ? FMath::Abs(Hit.ImpactPoint.Z - LastTerrainProbeZ) : 0.0f,
			ConsecutiveStableTerrainTicks,
			LandingTerrainStableTicks,
			Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("(noActor)"));

		if (ConsecutiveStableTerrainTicks >= LandingTerrainStableTicks)
		{
			// LOD has settled — restore movement.
			if (ACharacter* Character = Cast<ACharacter>(Pawn))
			{
				if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
				{
					Movement->SetMovementMode(MOVE_Walking);
				}
			}

			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOGameMode] === Terrain STABLE at Z=%.1f after %.2fs — placed pawn at %s, restored Walking ==="),
				Hit.ImpactPoint.Z, Elapsed, *LandedPos.ToString());
			FinishLoadHandoff();
			return;
		}
	}
	else
	{
		// No hit this tick — reset stability counter, keep waiting.
		ConsecutiveStableTerrainTicks = 0;

		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOGameMode] terrain-probe @ %.2fs / %.1fs  pawnZ=%.1f anchorZ=%.1f probe=±%.0fcm  result=(no visibility hit)"),
			Elapsed, PlayerLandingMaxWaitSeconds,
			Pawn->GetActorLocation().Z, SavedAnchorZ,
			PlayerLandingTerrainSearchDistance);
	}

	if (Elapsed >= PlayerLandingMaxWaitSeconds)
	{
		// LAST RESORT — after PlayerLandingMaxWaitSeconds (default 30s) of no
		// terrain found anywhere in a 100m vertical window around the saved
		// Z, the save data points at coordinates with no voxel terrain at
		// all (corrupted save / outside world / etc.). Recover to a safe
		// new-game-style spawn location so the player isn't stuck.
		const FVector SafeFallback = FindSafeSpawnLocation();
		Pawn->SetActorLocation(SafeFallback, /*bSweep=*/false, nullptr,
			ETeleportType::TeleportPhysics);

		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->SetMovementMode(MOVE_Walking);
			}
		}

		UE_LOG(LogMOFramework, Error,
			TEXT("[MOGameMode] === LAST-RESORT TIMEOUT after %.2fs — no terrain anywhere in ±%.0fcm of saved Z=%.1f at saved X/Y=%.0f,%.0f. Recovered to safe spawn %s ==="),
			Elapsed, PlayerLandingTerrainSearchDistance, SavedAnchorZ,
			SavedAnchorXY.X, SavedAnchorXY.Y, *SafeFallback.ToString());
		FinishLoadHandoff();
	}
}

void AMOGameMode::FinishLoadHandoff()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Stop any in-flight landing poll. Safe to call even if no timer set.
	World->GetTimerManager().ClearTimer(PlayerLandingPollHandle);
	WaitingForLandingPawn.Reset();

	// Lift the fall-through safety suppression — load is complete, the world
	// has settled, characters can now legitimately fall through and need the
	// rescue. (Match the iteration in WaitForVoxelAndRegroundPawns.)
	int32 UnsuppressedCount = 0;
	for (TActorIterator<AMOCharacter> It(World); It; ++It)
	{
		if (AMOCharacter* C = *It)
		{
			C->bSuppressFallThroughDuringLoad = false;
			++UnsuppressedCount;
		}
	}
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOGameMode] Lifted fall-through suppression on %d characters (load complete)"),
		UnsuppressedCount);

	// Dismiss loading screen
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

	// Assign the initial character's name.
	if (UMOIdentityComponent* IdentityComp = NewPawn->FindComponentByClass<UMOIdentityComponent>())
	{
		if (IdentityComp->DisplayName.IsEmpty())
		{
			// (H51) Consume PendingSurvivorName (written by the new-game panel,
			// which derived the save slot from it — e.g. slot "Alex_Smith-01").
			// Previously this rolled a fresh name from a DUPLICATE local pool,
			// so the slot said one name but the pawn got another, and the stale
			// pending name was later stolen by the first wandering survivor.
			// Mirror the consume-then-fallback shape in MOSpawnManagerSubsystem's
			// AssignSurvivorNameIfNeeded, sharing the canonical name generator.
			FString NameToAssign;
			if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
			{
				if (!Settings->PendingSurvivorName.IsEmpty())
				{
					NameToAssign = Settings->PendingSurvivorName;
					Settings->PendingSurvivorName.Empty();
					UE_LOG(LogMOFramework, Log,
						TEXT("[MOGameMode] Initial pawn consumed pending survivor name '%s' from new-game flow"),
						*NameToAssign);
				}
			}

			if (NameToAssign.IsEmpty())
			{
				NameToAssign = UMOSpawnManagerSubsystem::GenerateRandomSurvivorName();
			}

			IdentityComp->SetDisplayName(FText::FromString(NameToAssign));
			UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Initial pawn assigned name: %s"), *NameToAssign);
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

		// World is now spawnable — release any remote players who joined while
		// the voxel world was still generating (co-op join-spawn, S0).
		bWorldReadyForJoins = true;
		FlushPendingJoinControllers();

		// Event-driven landing detection.
		//
		// Replaces the old 10Hz polling loop. The pawn spawns SpawnHeightOffset
		// (default 200cm) above detected terrain, so it falls a moment and
		// triggers ACharacter::LandedDelegate on contact. We bind that here and
		// arm a single MaxLandingWaitSeconds timeout as the safety net — if no
		// land event arrives in time, RecoverStuckSpawn runs and re-arms.
		//
		// Edge case: if the spawn collision handler nudged the pawn to a
		// settled position (no falling at all), LandedDelegate would never
		// fire — so we check IsMovingOnGround immediately and short-circuit
		// to OnPawnLandedSafely if the pawn is already grounded at t=0.
		PendingLandingPawn = NewPawn;
		LandingRecoveryAttempts = 0;

		if (ACharacter* Character = Cast<ACharacter>(NewPawn))
		{
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			if (Movement && !Movement->IsFalling() && Movement->IsMovingOnGround())
			{
				UE_LOG(LogMOFramework, Log,
					TEXT("[MOGameMode] Initial pawn spawned already grounded — skipping landing wait"));
				OnPawnLandedSafely();
				return;
			}

			Character->LandedDelegate.RemoveDynamic(this, &AMOGameMode::HandlePawnLanded);
			Character->LandedDelegate.AddDynamic(this, &AMOGameMode::HandlePawnLanded);
			ArmLandingTimeout();
		}
		else
		{
			// Non-character pawn — no landing concept, succeed immediately.
			UE_LOG(LogMOFramework, Log,
				TEXT("[MOGameMode] Initial pawn is not a Character — dismissing loading screen immediately"));
			OnPawnLandedSafely();
		}
	}
}

// ============================================================================
// CO-OP JOIN-SPAWN (pipeline S0)
// ============================================================================

void AMOGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// Direct/late joins into a running world. (Seamless-travel players never
	// hit this path — they arrive via HandleSeamlessTravelPlayer below.)
	HandleRemotePlayerJoin(NewPlayer);
}

void AMOGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	// Super may SWAP the controller object (it recreates the PC for the new
	// level when classes differ) — use C only after it returns.
	Super::HandleSeamlessTravelPlayer(C);
	HandleRemotePlayerJoin(Cast<APlayerController>(C));
}

void AMOGameMode::HandleRemotePlayerJoin(APlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	if (PC->IsLocalController())
	{
		// The host — served by the initial-pawn / load flows above.
		return;
	}
	if (PC->GetPawn())
	{
		// Already has a pawn (future: session-restored possession).
		return;
	}

	if (!bWorldReadyForJoins)
	{
		PendingJoinControllers.AddUnique(PC);
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOGameMode] Remote player %s joined before world ready — queued (%d pending)"),
			*PC->GetName(), PendingJoinControllers.Num());
		return;
	}

	SpawnJoinPawnForController(PC);
}

void AMOGameMode::FlushPendingJoinControllers()
{
	if (PendingJoinControllers.Num() == 0)
	{
		return;
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] Flushing %d pending remote join(s)"),
		PendingJoinControllers.Num());
	for (const TWeakObjectPtr<APlayerController>& WeakPC : PendingJoinControllers)
	{
		APlayerController* PC = WeakPC.Get();
		if (PC && !PC->GetPawn())
		{
			SpawnJoinPawnForController(PC);
		}
	}
	PendingJoinControllers.Reset();
}

APawn* AMOGameMode::SpawnJoinPawnForController(APlayerController* PC)
{
	if (!PC || !DefaultNewGamePawnClass)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOGameMode] SpawnJoinPawnForController: %s"),
			!PC ? TEXT("null PC") : TEXT("DefaultNewGamePawnClass not set"));
		return nullptr;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Same safe-spawn rules as the host; independent roll so co-op players
	// land near the same biome band but not inside each other.
	const FVector SpawnLocation = FindSafeSpawnLocation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* NewPawn = World->SpawnActor<APawn>(DefaultNewGamePawnClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (!NewPawn)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOGameMode] Failed to spawn join pawn for %s"), *PC->GetName());
		return nullptr;
	}

	// Name from the canonical generator (the session layer will provide the
	// player's real name later — Move 3). Same empty-check discipline as the
	// initial pawn so a restored identity is never stomped.
	if (UMOIdentityComponent* IdentityComp = NewPawn->FindComponentByClass<UMOIdentityComponent>())
	{
		if (IdentityComp->DisplayName.IsEmpty())
		{
			const FString JoinName = UMOSpawnManagerSubsystem::GenerateRandomSurvivorName();
			IdentityComp->SetDisplayName(FText::FromString(JoinName));
			UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Join pawn assigned name: %s"), *JoinName);
		}
	}

	// Recruited like the host's pawn — captured by the next save like any
	// other recruited survivor (GUID identity handles multiple player pawns).
	if (UMORecruitmentComponent* RecruitComp = NewPawn->FindComponentByClass<UMORecruitmentComponent>())
	{
		RecruitComp->ForceRecruit();
	}

	PC->Possess(NewPawn);
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOGameMode] Remote player %s possessed join pawn %s at %s"),
		*PC->GetName(), *NewPawn->GetName(), *SpawnLocation.ToCompactString());
	// No landing/loading-screen machinery here: PendingLandingPawn is host-only
	// single-slot state; the join pawn simply falls SpawnHeightOffset to ground.
	return NewPawn;
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

	// Spawn-search parameters (Log-level diagnostics; #150 verified fixed in the 5.8 packaged build).
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

	// Spawn-search outcome stats (Log-level diagnostics; #150 verified fixed in the 5.8 packaged build).
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
//
// Event-driven: SpawnInitialPawn binds ACharacter::LandedDelegate to
// HandlePawnLanded and arms a single MaxLandingWaitSeconds timeout. The
// delegate fires the moment the pawn lands (cancels timeout, success path).
// If the timeout fires first, RecoverStuckSpawn runs and re-arms a fresh
// timeout for its teleport recovery to land. No more 10Hz polling.

void AMOGameMode::HandlePawnLanded(const FHitResult& Hit)
{
	UE_LOG(LogMOFramework, Log,
		TEXT("[MOGameMode] HandlePawnLanded — landed at %s, normal=%s"),
		*Hit.ImpactPoint.ToString(), *Hit.ImpactNormal.ToString());
	OnPawnLandedSafely();
}

void AMOGameMode::ArmLandingTimeout()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// SetTimer resets an existing handle, so multiple calls are safe — each
	// arms a fresh MaxLandingWaitSeconds window from "now."
	World->GetTimerManager().SetTimer(
		PawnLandingTimerHandle,
		this,
		&AMOGameMode::RecoverStuckSpawn,
		MaxLandingWaitSeconds,
		/*bLoop=*/false);
}

void AMOGameMode::RecoverStuckSpawn()
{
	APawn* Pawn = PendingLandingPawn.Get();
	if (!Pawn)
	{
		// The pawn we were waiting on was destroyed mid-spawn. Don't just clear the
		// timer and bail — that strands the player on the loading screen forever.
		// Dismiss it and clear the transition flag, same as the success path. (H32)
		GetWorld()->GetTimerManager().ClearTimer(PawnLandingTimerHandle);
		PendingLandingPawn.Reset();
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UMOGameInstance* MOGI = Cast<UMOGameInstance>(GI))
			{
				MOGI->DismissLoadingScreen();
			}
		}
		if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
		{
			Settings->bIsLoadingIntoGameplay = false;
		}
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

	// Re-arm the timeout. If this teleport doesn't land the pawn either,
	// we'll fall through to the next recovery strategy after another
	// MaxLandingWaitSeconds. If it DOES land, LandedDelegate fires first
	// and OnPawnLandedSafely cancels this timer.
	ArmLandingTimeout();
}

void AMOGameMode::OnPawnLandedSafely()
{
	APawn* Pawn = PendingLandingPawn.Get();
	PendingLandingPawn.Reset();

	// Stop the recovery timeout — whichever path got us here, we're done waiting.
	// Safe to call even if the timer isn't running.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PawnLandingTimerHandle);
	}

	// Unbind LandedDelegate so the pawn's later normal jumps/falls during
	// gameplay don't keep firing this handler.
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		Character->LandedDelegate.RemoveDynamic(this, &AMOGameMode::HandlePawnLanded);
	}

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
