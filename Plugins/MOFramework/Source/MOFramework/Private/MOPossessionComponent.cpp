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
}
