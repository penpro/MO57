#include "MOPossessionSubsystem.h"
#include "MOFramework.h"
#include "MOViewpointUtils.h"
#include "MOCharacter.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "MOIdentityComponent.h"
#include "MOInventoryComponent.h"
#include "MORecruitmentComponent.h"
#include "MOIdentityRegistrySubsystem.h"
#include "MOPersistenceSubsystem.h"
#include "MOGameMode.h"
#include "Engine/GameInstance.h"

bool UMOPossessionSubsystem::ResolveViewpoint(APlayerController* PlayerController, FVector& OutViewLocation, FRotator& OutViewRotation) const
{
	// Delegate to shared utility
	return UMOViewpointUtils::ResolveViewpointForPlayerController(PlayerController, OutViewLocation, OutViewRotation);
}

bool UMOPossessionSubsystem::HasLineOfSight(UWorld* World, const FVector& ViewLocation, const APawn* TargetPawn) const
{
	if (!bRequireLineOfSight)
	{
		return true;
	}

	if (!World || !IsValid(TargetPawn))
	{
		return false;
	}

	// Use simple line of sight check (no attached actor consideration for possession)
	// We ignore the target pawn in the trace
	return UMOViewpointUtils::HasLineOfSightSimple(
		World,
		ViewLocation,
		TargetPawn->GetActorLocation(),
		LineOfSightTraceChannel.GetValue(),
		const_cast<APawn*>(TargetPawn)  // Safe to const_cast - only used for ignore list
	);
}

bool UMOPossessionSubsystem::IsPawnAvailableTo(const APawn* Pawn, const AController* Requester) const
{
	if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed())
	{
		return false;
	}
	if (!Pawn->FindComponentByClass<UMOIdentityComponent>())
	{
		return false;
	}

	// Another HUMAN is driving it. (An AI controller -- an idle colonist -- does not count.)
	if (const AController* Current = Pawn->GetController())
	{
		if (Current != Requester && Current->IsPlayerController())
		{
			return false;
		}
	}

	// Must be a recruited colony member.
	const UMORecruitmentComponent* Recruit = Pawn->FindComponentByClass<UMORecruitmentComponent>();
	if (!Recruit || !Recruit->IsPossessable())
	{
		return false;
	}

	if (const AMOCharacter* MOChar = Cast<AMOCharacter>(Pawn))
	{
		if (MOChar->IsDead())
		{
			return false;
		}
	}
	return true;
}

APawn* UMOPossessionSubsystem::FindAvailablePawnForJoiner(const AController* Joiner, const FGuid& PreferredPawnGuid, const FVector& AnchorLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APawn* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		if (!IsPawnAvailableTo(Candidate, Joiner))
		{
			continue;
		}

		if (PreferredPawnGuid.IsValid())
		{
			const UMOIdentityComponent* Identity = Candidate->FindComponentByClass<UMOIdentityComponent>();
			if (Identity && Identity->GetGuid() == PreferredPawnGuid)
			{
				return Candidate;
			}
		}

		const float DistSq = FVector::DistSquared(AnchorLocation, Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}
	return Best;
}

void UMOPossessionSubsystem::BuildPossessionEntries(const APlayerController* Requester, TArray<FMOPossessionListEntry>& OutEntries) const
{
	OutEntries.Reset();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Saved records (includes pawns not currently in the world, and deceased ones).
	TArray<FMOPersistedPawnRecord> Records;
	TSet<FGuid> KnownGuids;
	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		if (UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>())
		{
			Records = Persistence->GetAllPawnRecords();
			for (const FMOPersistedPawnRecord& Record : Records)
			{
				KnownGuids.Add(Record.PawnGuid);
			}
		}
	}

	TSet<FGuid> DrivenByOtherHumans;
	int32 Scanned = 0, NoIdentity = 0, TakenByOther = 0, NotAvailable = 0, AlreadyKnown = 0, AddedFromWorld = 0;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed())
		{
			continue;
		}
		++Scanned;
		UMOIdentityComponent* Identity = Pawn->FindComponentByClass<UMOIdentityComponent>();
		if (!Identity)
		{
			++NoIdentity;
			continue;
		}
		// Create the GUID if the pawn has none yet (join/spawned pawns get theirs lazily): a pawn without one could
		// never be listed, and so never be chosen.
		const FGuid Guid = Identity->GetOrCreateGuid();
		if (!Guid.IsValid())
		{
			++NoIdentity;
			continue;
		}

		if (const AController* Current = Pawn->GetController())
		{
			if (Current != Requester && Current->IsPlayerController())
			{
				DrivenByOtherHumans.Add(Guid);
				++TakenByOther;
				continue;
			}
		}

		// Live recruited pawns that have no save record yet (e.g. just recruited).
		if (KnownGuids.Contains(Guid))
		{
			++AlreadyKnown;
			continue;
		}
		if (!IsPawnAvailableTo(Pawn, Requester))
		{
			++NotAvailable;
			continue;
		}
		++AddedFromWorld;

		FMOPersistedPawnRecord WorldPawnRecord;
		WorldPawnRecord.PawnGuid = Guid;
		WorldPawnRecord.Transform = Pawn->GetActorTransform();
		WorldPawnRecord.PawnClassPath = FSoftClassPath(Pawn->GetClass());
		WorldPawnRecord.bIsPlayerControllable = true;
		WorldPawnRecord.bIsDeceased = false;
		WorldPawnRecord.CharacterName = Identity->DisplayName.IsEmpty()
			? FString::Printf(TEXT("Survivor_%s"), *Guid.ToString().Right(4))
			: Identity->DisplayName.ToString();
		WorldPawnRecord.StatusText = TEXT("Recruited");
		WorldPawnRecord.LastPlayedTime = FDateTime::Now();
		Records.Add(WorldPawnRecord);
		KnownGuids.Add(Guid);
	}

	for (const FMOPersistedPawnRecord& Record : Records)
	{
		// Creatures/NPCs are not player-controllable; a pawn another human is driving is not on offer.
		if (Record.bIsPlayerControllable && !DrivenByOtherHumans.Contains(Record.PawnGuid))
		{
			OutEntries.Add(FMOPossessionListEntry::FromRecord(Record));
		}
	}

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOPossession] list for %s: %d entr(y/ies) -- scanned %d pawns (no identity %d, driven by another player %d, "
			"already in saves %d, not available %d, added from world %d); %d save record(s)"),
		Requester ? *Requester->GetName() : TEXT("?"), OutEntries.Num(), Scanned, NoIdentity, TakenByOther,
		AlreadyKnown, NotAvailable, AddedFromWorld, Records.Num() - AddedFromWorld);
}

APawn* UMOPossessionSubsystem::ServerPossessPawnByGuid(APlayerController* PlayerController, const FGuid& PawnGuid)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(PlayerController) || !PawnGuid.IsValid() || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	APawn* Pawn = nullptr;
	if (UMOIdentityRegistrySubsystem* Registry = World->GetSubsystem<UMOIdentityRegistrySubsystem>())
	{
		Pawn = Cast<APawn>(Registry->ResolveActorOrNull(PawnGuid));
	}
	if (!Pawn)
	{
		// Not in the world: bring it back from its save record (host persistence).
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>())
			{
				Pawn = Persistence->SpawnPawnFromRecord(PawnGuid);
			}
		}
	}
	if (!Pawn)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPossession] %s asked for pawn %s: not in the world and no save record"),
			*PlayerController->GetName(), *PawnGuid.ToString());
		return nullptr;
	}

	if (!IsPawnAvailableTo(Pawn, PlayerController))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPossession] %s may not take %s (dead, not recruited, or another player is driving it)"),
			*PlayerController->GetName(), *Pawn->GetName());
		return nullptr;
	}

	if (PlayerController->GetPawn() != Pawn)
	{
		PlayerController->Possess(Pawn);
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOPossession] %s possessed %s"), *PlayerController->GetName(), *Pawn->GetName());
	return Pawn;
}

APawn* UMOPossessionSubsystem::ServerCreateCharacter(APlayerController* PlayerController)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(PlayerController) || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	// The menu only offers this to a player with no character; the server enforces it (a client could call the RPC anyway).
	if (const AMOCharacter* Current = Cast<AMOCharacter>(PlayerController->GetPawn()))
	{
		if (!Current->IsDead())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPossession] %s already has a living character -- create refused"), *PlayerController->GetName());
			return nullptr;
		}
	}

	AMOGameMode* GameMode = World->GetAuthGameMode<AMOGameMode>();
	return GameMode ? GameMode->CreateCharacterForPlayer(PlayerController) : nullptr;
}

APawn* UMOPossessionSubsystem::FindNearestUnpossessedPawn(APlayerController* PlayerController) const
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(PlayerController))
	{
		return nullptr;
	}

	if (World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!ResolveViewpoint(PlayerController, ViewLocation, ViewRotation))
	{
		return nullptr;
	}

	const float MaxDistSq = FMath::Square(FMath::Max(0.0f, MaximumPossessDistance));
	APawn* BestPawn = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* CandidatePawn = *It;
		if (!IsValid(CandidatePawn) || CandidatePawn->IsActorBeingDestroyed())
		{
			continue;
		}

		if (IsValid(CandidatePawn->GetController()))
		{
			continue;
		}

		if (!CandidatePawn->FindComponentByClass<UMOIdentityComponent>())
		{
			continue;
		}
		if (!CandidatePawn->FindComponentByClass<UMOInventoryComponent>())
		{
			continue;
		}

		// Check recruitment state - must be recruited (or not have recruitment component)
		if (UMORecruitmentComponent* RecruitComp = CandidatePawn->FindComponentByClass<UMORecruitmentComponent>())
		{
			if (!RecruitComp->IsPossessable())
			{
				continue;
			}
		}

		// Cannot possess dead pawns
		if (AMOCharacter* MOChar = Cast<AMOCharacter>(CandidatePawn))
		{
			if (MOChar->IsDead())
			{
				continue;
			}
		}

		const float DistSq = FVector::DistSquared(ViewLocation, CandidatePawn->GetActorLocation());
		if (DistSq > MaxDistSq)
		{
			continue;
		}

		if (bRequireLineOfSight && !HasLineOfSight(World, ViewLocation, CandidatePawn))
		{
			continue;
		}

		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestPawn = CandidatePawn;
		}
	}

	return BestPawn;
}

bool UMOPossessionSubsystem::ServerPossessNearestPawn(APlayerController* PlayerController)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(PlayerController))
	{
		return false;
	}

	if (World->GetNetMode() == NM_Client)
	{
		return false;
	}

	APawn* CurrentPawn = PlayerController->GetPawn();
	if (IsValid(CurrentPawn) && !bAllowSwitchPossession)
	{
		return false;
	}

	APawn* TargetPawn = FindNearestUnpossessedPawn(PlayerController);
	if (!IsValid(TargetPawn))
	{
		return false;
	}

	if (IsValid(CurrentPawn))
	{
		PlayerController->UnPossess();
	}

	PlayerController->Possess(TargetPawn);
	return PlayerController->GetPawn() == TargetPawn;
}

AActor* UMOPossessionSubsystem::ServerSpawnActorNearController(APlayerController* PlayerController, TSubclassOf<AActor> ActorClassToSpawn, float SpawnDistance, FVector SpawnOffset, bool bUseViewRotation)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(PlayerController))
	{
		return nullptr;
	}

	if (World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	if (!ActorClassToSpawn)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPossess] SpawnActorNearController failed: no class set"));
		return nullptr;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!ResolveViewpoint(PlayerController, ViewLocation, ViewRotation))
	{
		return nullptr;
	}

	const FRotator SpawnRotation = bUseViewRotation ? ViewRotation : FRotator::ZeroRotator;
	const FVector Forward = SpawnRotation.Vector();
	const FVector RotatedOffset = bUseViewRotation ? SpawnRotation.RotateVector(SpawnOffset) : SpawnOffset;
	const FVector SpawnLocation = ViewLocation + (Forward * SpawnDistance) + RotatedOffset;

	FTransform SpawnTransform(SpawnRotation, SpawnLocation);

	AActor* DeferredActor = World->SpawnActorDeferred<AActor>(
		ActorClassToSpawn,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
	);

	if (!IsValid(DeferredActor))
	{
		return nullptr;
	}

	// If it has identity, assign a guid before BeginPlay.
	if (UMOIdentityComponent* IdentityComponent = DeferredActor->FindComponentByClass<UMOIdentityComponent>())
	{
		if (!IdentityComponent->HasValidGuid())
		{
			IdentityComponent->SetGuid(FGuid::NewGuid());
		}
	}

	UGameplayStatics::FinishSpawningActor(DeferredActor, SpawnTransform);

	UE_LOG(LogMOFramework, Log, TEXT("[MOPossess] Spawned Actor=%s Class=%s"),
		*GetNameSafe(DeferredActor),
		*GetNameSafe(ActorClassToSpawn.Get()));

	return DeferredActor;
}

APawn* UMOPossessionSubsystem::ServerSpawnAndPossessPawn(APlayerController* PlayerController, TSubclassOf<APawn> PawnClassToSpawn, float SpawnDistance, FVector SpawnOffset, bool bUseViewRotation)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(PlayerController))
	{
		return nullptr;
	}

	if (World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	if (!PawnClassToSpawn)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPossess] SpawnAndPossessPawn failed: no pawn class set"));
		return nullptr;
	}

	// Spawn the pawn using the existing spawn logic
	AActor* SpawnedActor = ServerSpawnActorNearController(PlayerController, PawnClassToSpawn, SpawnDistance, SpawnOffset, bUseViewRotation);
	APawn* SpawnedPawn = Cast<APawn>(SpawnedActor);

	if (!IsValid(SpawnedPawn))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPossess] SpawnAndPossessPawn failed: spawned actor is not a pawn"));
		return nullptr;
	}

	// Unpossess current pawn if any
	APawn* CurrentPawn = PlayerController->GetPawn();
	if (IsValid(CurrentPawn))
	{
		PlayerController->UnPossess();
	}

	// Possess the new pawn
	PlayerController->Possess(SpawnedPawn);

	UE_LOG(LogMOFramework, Log, TEXT("[MOPossess] Spawned and possessed Pawn=%s Class=%s"),
		*GetNameSafe(SpawnedPawn),
		*GetNameSafe(PawnClassToSpawn.Get()));

	return SpawnedPawn;
}
