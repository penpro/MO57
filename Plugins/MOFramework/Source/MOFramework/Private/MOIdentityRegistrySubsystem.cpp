#include "MOIdentityRegistrySubsystem.h"

#include "MOIdentityComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

void UMOIdentityRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	

	// Track new spawns
	ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UMOIdentityRegistrySubsystem::HandleActorSpawned)
	);
}

void UMOIdentityRegistrySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Only track in real gameplay worlds (filters out editor preview worlds).
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}

	// Now that actors are initialized, scan the world and register placed actors.
	RegisterExistingActors();
}

void UMOIdentityRegistrySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (ActorSpawnedHandle.IsValid())
		{
			World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
			ActorSpawnedHandle.Reset();
		}
	}

	GuidToActor.Empty();
	ActorToGuid.Empty();
	TrackedActors.Empty();

	Super::Deinitialize();
}

void UMOIdentityRegistrySubsystem::RegisterExistingActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TryTrackActor(*It);
	}
}

void UMOIdentityRegistrySubsystem::HandleActorSpawned(AActor* SpawnedActor)
{
	TryTrackActor(SpawnedActor);
}

void UMOIdentityRegistrySubsystem::TryTrackActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// Skip templates/CDOs
	if (Actor->IsTemplate() || Actor->HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	// Avoid double work
	if (TrackedActors.Contains(Actor))
	{
		return;
	}

	UMOIdentityComponent* IdentityComponent = Actor->FindComponentByClass<UMOIdentityComponent>();
	if (!IdentityComponent)
	{
		return;
	}

	TrackedActors.Add(Actor);

	// Bind to owner destruction so we can unregister
	Actor->OnDestroyed.AddDynamic(this, &UMOIdentityRegistrySubsystem::HandleActorDestroyed);

	// Bind to GUID availability (covers client replication arriving later)
	IdentityComponent->OnGuidAvailable.AddDynamic(this, &UMOIdentityRegistrySubsystem::HandleGuidAvailable);

	// If GUID is already valid now, register immediately
	if (IdentityComponent->HasValidGuid())
	{
		RegisterGuidForActor(IdentityComponent->GetGuid(), Actor);
	}
}

void UMOIdentityRegistrySubsystem::HandleGuidAvailable(const FGuid& StableGuid)
{
	// We need to find which actor fired this. For Dynamic multicast, the delegate does not carry sender.
	// So we do a cheap scan over tracked actors to find the one that matches this GUID.
	// This is acceptable because it only runs when GUIDs become available, not every frame.

	for (const TWeakObjectPtr<AActor>& WeakActor : TrackedActors)
	{
		AActor* Actor = WeakActor.Get();
		if (!Actor)
		{
			continue;
		}

		if (UMOIdentityComponent* IdentityComponent = Actor->FindComponentByClass<UMOIdentityComponent>())
		{
			if (IdentityComponent->HasValidGuid() && IdentityComponent->GetGuid() == StableGuid)
			{
				RegisterGuidForActor(StableGuid, Actor);
				return;
			}
		}
	}
}

void UMOIdentityRegistrySubsystem::HandleActorDestroyed(AActor* DestroyedActor)
{
	UnregisterActor(DestroyedActor);
}

void UMOIdentityRegistrySubsystem::RegisterGuidForActor(const FGuid& Guid, AActor* Actor)
{
	if (!Guid.IsValid() || !Actor)
	{
		return;
	}

	// If the actor already has a registered GUID, remove the old mapping first
	if (const FGuid* ExistingGuid = ActorToGuid.Find(Actor))
	{
		if (*ExistingGuid == Guid)
		{
			return; // already correct
		}

		GuidToActor.Remove(*ExistingGuid);
		ActorToGuid.Remove(Actor);
	}

	// Check current mapping BEFORE we write.
	const TWeakObjectPtr<AActor>* ExistingActorPtr = GuidToActor.Find(Guid);
	const bool bWasAlreadyRegistered = ExistingActorPtr && (ExistingActorPtr->Get() == Actor);

	// Collision policy: do not overwrite a live actor with the same GUID.
	if (ExistingActorPtr)
	{
		if (AActor* ExistingActor = ExistingActorPtr->Get())
		{
			if (ExistingActor != Actor)
			{
				return;
			}
		}
	}

	// Write mapping
	GuidToActor.Add(Guid, Actor);
	ActorToGuid.Add(Actor, Guid);

	// Broadcast only on first registration
	if (!bWasAlreadyRegistered)
	{
		OnIdentityRegistered.Broadcast(Guid, Actor);
	}
}

void UMOIdentityRegistrySubsystem::UnregisterActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	const FGuid* ExistingGuid = ActorToGuid.Find(Actor);
	if (ExistingGuid && ExistingGuid->IsValid())
	{
		const FGuid GuidToRemove = *ExistingGuid;

		GuidToActor.Remove(GuidToRemove);
		ActorToGuid.Remove(Actor);
		TrackedActors.Remove(Actor);

		OnIdentityUnregistered.Broadcast(GuidToRemove, Actor);
		return;
	}

	TrackedActors.Remove(Actor);
}

bool UMOIdentityRegistrySubsystem::TryResolveActor(const FGuid& Guid, AActor*& OutActor) const
{
	OutActor = nullptr;

	if (!Guid.IsValid())
	{
		return false;
	}

	const TWeakObjectPtr<AActor>* ActorPtr = GuidToActor.Find(Guid);
	if (!ActorPtr)
	{
		return false;
	}

	AActor* Actor = ActorPtr->Get();
	if (!Actor)
	{
		return false;
	}

	OutActor = Actor;
	return true;
}

AActor* UMOIdentityRegistrySubsystem::ResolveActorOrNull(const FGuid& Guid) const
{
	AActor* Actor = nullptr;
	TryResolveActor(Guid, Actor);
	return Actor;
}

bool UMOIdentityRegistrySubsystem::TryGetGuidFromActor(const AActor* Actor, FGuid& OutGuid) const
{
	OutGuid.Invalidate();

	if (!Actor)
	{
		return false;
	}

	const FGuid* GuidPtr = ActorToGuid.Find(Actor);
	if (!GuidPtr || !GuidPtr->IsValid())
	{
		return false;
	}

	OutGuid = *GuidPtr;
	return true;
}

int32 UMOIdentityRegistrySubsystem::GetRegisteredCount() const
{
	return GuidToActor.Num();
}


