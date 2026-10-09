#include "MOPossessionComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "MOPossessionSubsystem.h"
#include "MOPlayerController.h"

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

UMOPossessionSubsystem* UMOPossessionComponent::GetAuthoritySubsystem(APlayerController*& OutController) const
{
	OutController = Cast<APlayerController>(GetOwner());
	if (!IsValid(OutController))
	{
		return nullptr;
	}
	UWorld* World = OutController->GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	return World->GetSubsystem<UMOPossessionSubsystem>();
}

void UMOPossessionComponent::ApplyPossessByGuid(const FGuid& PawnGuid)
{
	APlayerController* PC = nullptr;
	if (UMOPossessionSubsystem* Subsystem = GetAuthoritySubsystem(PC))
	{
		Subsystem->ServerPossessPawnByGuid(PC, PawnGuid);
	}
}

void UMOPossessionComponent::ApplyCreateCharacter()
{
	APlayerController* PC = nullptr;
	if (UMOPossessionSubsystem* Subsystem = GetAuthoritySubsystem(PC))
	{
		Subsystem->ServerCreateCharacter(PC);
	}
}

void UMOPossessionComponent::BuildList(TArray<FMOPossessionListEntry>& Out) const
{
	APlayerController* PC = nullptr;
	if (UMOPossessionSubsystem* Subsystem = GetAuthoritySubsystem(PC))
	{
		Subsystem->BuildPossessionEntries(PC, Out);
	}
}

bool UMOPossessionComponent::RequestPossessPawn(const FGuid& PawnGuid)
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!IsValid(PC) || !PC->IsLocalController() || !PawnGuid.IsValid())
	{
		return false;
	}
	if (PC->HasAuthority())
	{
		ApplyPossessByGuid(PawnGuid);
	}
	else if (AMOPlayerController* MOPC = Cast<AMOPlayerController>(GetOwner()))
	{
		MOPC->ServerPossessPawnByGuid(PawnGuid);
	}
	else
	{
		return false;
	}
	return true;
}

bool UMOPossessionComponent::RequestCreateCharacter()
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!IsValid(PC) || !PC->IsLocalController())
	{
		return false;
	}
	if (PC->HasAuthority())
	{
		ApplyCreateCharacter();
	}
	else if (AMOPlayerController* MOPC = Cast<AMOPlayerController>(GetOwner()))
	{
		MOPC->ServerCreateCharacter();
	}
	else
	{
		return false;
	}
	return true;
}

void UMOPossessionComponent::RequestPossessionList()
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!IsValid(PC) || !PC->IsLocalController())
	{
		return;
	}
	if (PC->HasAuthority())
	{
		TArray<FMOPossessionListEntry> Entries;
		BuildList(Entries);
		OnPossessionListReady.Broadcast(Entries);
	}
	else if (AMOPlayerController* MOPC = Cast<AMOPlayerController>(GetOwner()))
	{
		MOPC->ServerRequestPossessionList();
	}
}

void UMOPossessionComponent::NotifyPossessionListReceived(const TArray<FMOPossessionListEntry>& Entries)
{
	OnPossessionListReady.Broadcast(Entries);
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
