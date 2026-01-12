#include "MOPersistenceSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

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

	UnbindFromWorld();

	BoundWorld = World;

	UMOIdentityRegistrySubsystem* RegistrySubsystem = World->GetSubsystem<UMOIdentityRegistrySubsystem>();
	if (!RegistrySubsystem)
	{
		return;
	}

	BoundRegistry = RegistrySubsystem;

	RegistrySubsystem->OnIdentityRegistered.AddDynamic(this, &UMOPersistenceSubsystem::HandleIdentityRegistered);

	// Apply already-loaded destroyed set to this world
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
	UMOWorldSaveGame* SaveObject = Cast<UMOWorldSaveGame>(UGameplayStatics::CreateSaveGameObject(UMOWorldSaveGame::StaticClass()));
	if (!SaveObject)
	{
		return false;
	}

	SaveObject->DestroyedGuids = SessionDestroyedGuids;
	return UGameplayStatics::SaveGameToSlot(SaveObject, SlotName, 0);
}

bool UMOPersistenceSubsystem::LoadWorldFromSlot(const FString& SlotName)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		SessionDestroyedGuids.Reset();
		LoadedWorldSave = nullptr;

		// Still apply empty state (no-op)
		if (UWorld* World = BoundWorld.Get())
		{
			ApplyDestroyedGuidsToWorld(World);
		}
		return false;
	}

	USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(SlotName, 0);
	UMOWorldSaveGame* LoadedTyped = Cast<UMOWorldSaveGame>(Loaded);
	if (!LoadedTyped)
	{
		return false;
	}

	LoadedWorldSave = LoadedTyped;
	SessionDestroyedGuids = LoadedTyped->DestroyedGuids;

	if (UWorld* World = BoundWorld.Get())
	{
		ApplyDestroyedGuidsToWorld(World);
	}
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

	UMOIdentityRegistrySubsystem* RegistrySubsystem = World->GetSubsystem<UMOIdentityRegistrySubsystem>();
	if (!RegistrySubsystem)
	{
		return;
	}

	// Simple approach: iterate all registered GUIDs by scanning registry map via TryResolve
	// You can add a registry iterator later if you want.
	for (const FGuid& DestroyedGuid : SessionDestroyedGuids)
	{
		AActor* ActorToDestroy = RegistrySubsystem->ResolveActorOrNull(DestroyedGuid);
		if (IsValid(ActorToDestroy))
		{
			ActorToDestroy->Destroy();
		}
	}
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
		IdentityComponent->OnOwnerDestroyedWithGuid.AddDynamic(this, &UMOPersistenceSubsystem::HandleIdentityDestroyed);
	}
}

void UMOPersistenceSubsystem::HandleIdentityDestroyed(const FGuid& StableGuid)
{
	if (StableGuid.IsValid())
	{
		SessionDestroyedGuids.Add(StableGuid);
	}
}
