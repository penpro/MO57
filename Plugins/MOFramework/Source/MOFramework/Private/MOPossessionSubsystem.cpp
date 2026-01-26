#include "MOPossessionSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "MOIdentityComponent.h"
#include "MOInventoryComponent.h"

bool UMOPossessionSubsystem::ResolveViewpoint(APlayerController* PlayerController, FVector& OutViewLocation, FRotator& OutViewRotation) const
{
	if (!IsValid(PlayerController))
	{
		return false;
	}

	PlayerController->GetPlayerViewPoint(OutViewLocation, OutViewRotation);
	return true;
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

	const FVector TargetLocation = TargetPawn->GetActorLocation();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MOPossessionLOS), true);
	QueryParams.bReturnPhysicalMaterial = false;
	QueryParams.AddIgnoredActor(TargetPawn);

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, ViewLocation, TargetLocation, LineOfSightTraceChannel.GetValue(), QueryParams);
	return !bHit;
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
		UE_LOG(LogTemp, Warning, TEXT("[MOPossess] SpawnActorNearController failed: no class set"));
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

	UE_LOG(LogTemp, Warning, TEXT("[MOPossess] Spawned Actor=%s Class=%s"),
		*GetNameSafe(DeferredActor),
		*GetNameSafe(ActorClassToSpawn.Get()));

	return DeferredActor;
}
