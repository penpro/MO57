#include "MOPersistenceSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

#include "MOWorldSaveGame.h"
#include "MOIdentityComponent.h"
#include "MOIdentityRegistrySubsystem.h"

void UMOPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Bind to world init so this subsystem works across level loads
	PostWorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
		this,
		&UMOPersistenceSubsystem::HandlePostWorldInitialization
	);
}

void UMOPersistenceSubsystem::HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS)
{
	// IVS is intentionally unused for now
	BindToWorld(World);
}

void UMOPersistenceSubsystem::Deinitialize()
{
	UnbindFromWorld();

	if (PostWorldInitHandle.IsValid())
	{
		FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitHandle);
		PostWorldInitHandle.Reset();
	}

	Super::Deinitialize();
}

void UMOPersistenceSubsystem::BindToWorld(UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// Persistence should run on authority only (standalone, listen server, dedicated server).
	// If we bind to an NM_Client world, all destroys will be skipped (no authority).
	if (World->GetNetMode() == NM_Client)
	{
		return;
	}

	// If already bound to this world, do nothing.
	if (BoundWorld.Get() == World)
	{
		return;
	}

	UnbindFromWorld();

	BoundWorld = World;

	UMOIdentityRegistrySubsystem* RegistrySubsystem = World->GetSubsystem<UMOIdentityRegistrySubsystem>();
	if (!RegistrySubsystem)
	{
		return;
	}

	BoundRegistry = RegistrySubsystem;

	RegistrySubsystem->OnIdentityRegistered.AddDynamic(this, &UMOPersistenceSubsystem::HandleIdentityRegistered);

	// Apply already-loaded destroyed set to this authority world
	ApplyDestroyedGuidsToWorld(World);
}

void UMOPersistenceSubsystem::UnbindFromWorld()
{
	if (UMOIdentityRegistrySubsystem* RegistrySubsystem = BoundRegistry.Get())
	{
		RegistrySubsystem->OnIdentityRegistered.RemoveDynamic(this, &UMOPersistenceSubsystem::HandleIdentityRegistered);
	}

	BoundRegistry.Reset();
	BoundWorld.Reset();
}

bool UMOPersistenceSubsystem::SaveWorldToSlot(const FString& SlotName)
{
	UWorld* World = BoundWorld.Get();
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOPersist] Save ignored (no authority world bound). Slot=%s"), *SlotName);
		return false;
	}

	UMOWorldSaveGame* SaveObject = Cast<UMOWorldSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UMOWorldSaveGame::StaticClass())
	);
	if (!SaveObject)
	{
		return false;
	}

	SaveObject->DestroyedGuids = SessionDestroyedGuids;

	const bool bOk = UGameplayStatics::SaveGameToSlot(SaveObject, SlotName, 0);

	UE_LOG(LogTemp, Warning, TEXT("[MOPersist] Save slot=%s ok=%d destroyed=%d netmode=%d"),
		*SlotName, bOk ? 1 : 0, SessionDestroyedGuids.Num(), (int32)World->GetNetMode());

	return bOk;
}

bool UMOPersistenceSubsystem::LoadWorldFromSlot(const FString& SlotName)
{
	UWorld* World = BoundWorld.Get();
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOPersist] Load ignored (no authority world bound). Slot=%s"), *SlotName);
		return false;
	}

	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		SessionDestroyedGuids.Reset();
		LoadedWorldSave = nullptr;

		UE_LOG(LogTemp, Warning, TEXT("[MOPersist] Load slot=%s ok=0 (no file) netmode=%d"),
			*SlotName, (int32)World->GetNetMode());

		ApplyDestroyedGuidsToWorld(World);
		return false;
	}

	USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(SlotName, 0);
	UMOWorldSaveGame* LoadedTyped = Cast<UMOWorldSaveGame>(Loaded);
	if (!LoadedTyped)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOPersist] Load slot=%s ok=0 (wrong class) netmode=%d"),
			*SlotName, (int32)World->GetNetMode());
		return false;
	}

	LoadedWorldSave = LoadedTyped;
	SessionDestroyedGuids = LoadedTyped->DestroyedGuids;

	UE_LOG(LogTemp, Warning, TEXT("[MOPersist] Load slot=%s ok=1 destroyed=%d netmode=%d"),
		*SlotName, SessionDestroyedGuids.Num(), (int32)World->GetNetMode());

	ApplyDestroyedGuidsToWorld(World);
	return true;
}

bool UMOPersistenceSubsystem::IsGuidDestroyed(const FGuid& Guid) const
{

	return Guid.IsValid() && SessionDestroyedGuids.Contains(Guid);
}

void UMOPersistenceSubsystem::ApplyDestroyedGuidsToWorld(UWorld* World)
{
	if (!World)
	{
		return;
	}

	const ENetMode NetMode = World->GetNetMode();
	int32 MatchesFound = 0;
	int32 DestroyIssued = 0;

	UE_LOG(LogTemp, Warning, TEXT("[MOPersist] ApplyDestroyedGuidsToWorld World=%s NetMode=%d DestroyedCount=%d"),
		*World->GetName(), (int32)NetMode, SessionDestroyedGuids.Num());

	if (SessionDestroyedGuids.Num() == 0)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
		{
			continue;
		}

		UMOIdentityComponent* Identity = Actor->FindComponentByClass<UMOIdentityComponent>();
		if (!Identity || !Identity->HasValidGuid())
		{
			continue;
		}

		const FGuid ActorGuid = Identity->GetGuid();
		if (!SessionDestroyedGuids.Contains(ActorGuid))
		{
			continue;
		}

		MatchesFound++;

		const bool bActorReplicates = Actor->GetIsReplicated();
		const bool bHasAuthority = Actor->HasAuthority();

		// Replicated actors: destroy only on server, clients will get the destruction via replication.
		if (bActorReplicates)
		{
			if (bHasAuthority)
			{
				UE_LOG(LogTemp, Warning, TEXT("[MOPersist] Destroy (replicated, authority) %s Guid=%s"),
					*Actor->GetName(), *ActorGuid.ToString());

				Actor->Destroy();
				DestroyIssued++;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[MOPersist] Skip destroy (replicated, no authority) %s Guid=%s"),
					*Actor->GetName(), *ActorGuid.ToString());
			}

			continue;
		}

		// Non-replicated actors: destroy locally (both server and client worlds need to apply).
		UE_LOG(LogTemp, Warning, TEXT("[MOPersist] Destroy (non-replicated, local) %s Guid=%s"),
			*Actor->GetName(), *ActorGuid.ToString());

		Actor->Destroy();
		DestroyIssued++;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MOPersist] Apply complete MatchesFound=%d DestroyIssued=%d"),
		MatchesFound, DestroyIssued);
}

void UMOPersistenceSubsystem::HandleIdentityRegistered(const FGuid& StableGuid, AActor* Actor)
{
	if (!StableGuid.IsValid() || !IsValid(Actor))
	{
		return;
	}

	// If this GUID was already destroyed in the session or save, kill it immediately
	if (SessionDestroyedGuids.Contains(StableGuid))
	{
		Actor->Destroy();
		return;
	}

	// Bind to identity destruction events so we can remember destroyed actors
	if (UMOIdentityComponent* IdentityComponent = Actor->FindComponentByClass<UMOIdentityComponent>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOPersist] Registered %s Guid=%s"),
		*GetNameSafe(Actor), *StableGuid.ToString());

		IdentityComponent->OnOwnerDestroyedWithGuid.AddDynamic(this, &UMOPersistenceSubsystem::HandleIdentityDestroyed);
	}
}

void UMOPersistenceSubsystem::HandleIdentityDestroyed(const FGuid& StableGuid)
{
	if (StableGuid.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOPersist] Destroyed Guid=%s (adding)"), *StableGuid.ToString());

		SessionDestroyedGuids.Add(StableGuid);
	}
}
