#include "MOPossessionComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "MOPossessionSubsystem.h"

UMOPossessionComponent::UMOPossessionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOPossessionComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UMOPossessionComponent::TryPossessNearestPawn()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return false;
	}

	ServerTryPossessNearestPawn();
	return true;
}

bool UMOPossessionComponent::TrySpawnActorNearController(TSubclassOf<AActor> ActorClassToSpawn, float SpawnDistance, FVector SpawnOffset, bool bUseViewRotation)
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return false;
	}

	if (!ActorClassToSpawn)
	{
		return false;
	}

	ServerSpawnActorNearController(ActorClassToSpawn, SpawnDistance, SpawnOffset, bUseViewRotation);
	return true;
}

void UMOPossessionComponent::ServerTryPossessNearestPawn_Implementation()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!IsValid(PlayerController))
	{
		return;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	UMOPossessionSubsystem* PossessionSubsystem = World->GetSubsystem<UMOPossessionSubsystem>();
	if (!PossessionSubsystem)
	{
		return;
	}

	PossessionSubsystem->ServerPossessNearestPawn(PlayerController);
}

void UMOPossessionComponent::ServerSpawnActorNearController_Implementation(TSubclassOf<AActor> ActorClassToSpawn, float SpawnDistance, FVector SpawnOffset, bool bUseViewRotation)
{
	// Client-controlled spawn surface: a client can request ANY class at ANY
	// offset through this RPC. No production flow uses it (the possession
	// menu calls the subsystem server-side); the only referencer is the
	// template BP_ThirdPersonPlayerController. Dev/prototyping tool only —
	// reject outright in shipping, clamp the spatial params elsewhere.
#if UE_BUILD_SHIPPING
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOPossession] ServerSpawnActorNearController rejected: dev-only RPC (shipping build)"));
	return;
#else
	if (!ActorClassToSpawn)
	{
		return;
	}
	SpawnDistance = FMath::Clamp(SpawnDistance, 0.0f, 2000.0f);
	SpawnOffset = SpawnOffset.GetClampedToMaxSize(2000.0f);

	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!IsValid(PlayerController))
	{
		return;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	UMOPossessionSubsystem* PossessionSubsystem = World->GetSubsystem<UMOPossessionSubsystem>();
	if (!PossessionSubsystem)
	{
		return;
	}

	PossessionSubsystem->ServerSpawnActorNearController(PlayerController, ActorClassToSpawn, SpawnDistance, SpawnOffset, bUseViewRotation);
#endif
}

bool UMOPossessionComponent::TrySpawnAndPossessPawn(TSubclassOf<APawn> PawnClassToSpawn, float SpawnDistance, FVector SpawnOffset, bool bUseViewRotation)
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return false;
	}

	if (!PawnClassToSpawn)
	{
		return false;
	}

	ServerSpawnAndPossessPawn(PawnClassToSpawn, SpawnDistance, SpawnOffset, bUseViewRotation);
	return true;
}

void UMOPossessionComponent::ServerSpawnAndPossessPawn_Implementation(TSubclassOf<APawn> PawnClassToSpawn, float SpawnDistance, FVector SpawnOffset, bool bUseViewRotation)
{
	// Same client-controlled spawn surface as ServerSpawnActorNearController
	// above — dev-only, rejected in shipping, clamped elsewhere.
#if UE_BUILD_SHIPPING
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOPossession] ServerSpawnAndPossessPawn rejected: dev-only RPC (shipping build)"));
	return;
#else
	if (!PawnClassToSpawn)
	{
		return;
	}
	SpawnDistance = FMath::Clamp(SpawnDistance, 0.0f, 2000.0f);
	SpawnOffset = SpawnOffset.GetClampedToMaxSize(2000.0f);

	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!IsValid(PlayerController))
	{
		return;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	UMOPossessionSubsystem* PossessionSubsystem = World->GetSubsystem<UMOPossessionSubsystem>();
	if (!PossessionSubsystem)
	{
		return;
	}

	PossessionSubsystem->ServerSpawnAndPossessPawn(PlayerController, PawnClassToSpawn, SpawnDistance, SpawnOffset, bUseViewRotation);
#endif
}
