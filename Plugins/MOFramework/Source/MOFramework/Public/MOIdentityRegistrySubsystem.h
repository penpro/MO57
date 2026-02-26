/**
 * =============================================================================
 * MOIdentityRegistrySubsystem.h - GUID-to-Actor Mapping System
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * WorldSubsystem that maintains a bidirectional mapping between persistent
 * GUIDs and their corresponding Actors. Essential for save/load, inventory
 * references, and any cross-session actor identification.
 *
 * KEY RESPONSIBILITIES:
 * 1. Register actors with UMOIdentityComponent on spawn/BeginPlay
 * 2. Maintain GuidToActor and ActorToGuid maps
 * 3. Unregister actors on destruction
 * 4. Broadcast registration/unregistration events
 *
 * ARCHITECTURE NOTES:
 * - WorldSubsystem: One per UWorld, recreated on level load
 * - Uses TWeakObjectPtr for actor references (avoids dangling pointers)
 * - Tracks actors in TrackedActors set to prevent double-binding
 * - OnIdentityRegistered fires when GUID becomes available (may be deferred)
 *
 * CRITICAL PATTERNS:
 * 1. Actor Registration:
 *    ActorSpawned -> TryTrackActor -> Bind to OnGuidAvailable
 *    -> HandleGuidAvailable -> RegisterGuidForActor -> Broadcast
 *
 * 2. GUID Resolution:
 *    TryResolveActor(Guid) -> Check GuidToActor map -> Return Actor or null
 *    TryGetGuidFromActor(Actor) -> Check ActorToGuid map -> Return Guid
 *
 * 3. Cleanup:
 *    Actor destroyed -> HandleActorDestroyed -> UnregisterActor
 *    -> Remove from maps -> Broadcast OnIdentityUnregistered
 *
 * KNOWN PITFALLS:
 * 1. DEFERRED GUID: UMOIdentityComponent may not have GUID immediately
 *    (e.g., loaded from save). Always check IsGuidValid() before registering.
 *
 * 2. LEVEL TRANSITIONS: WorldSubsystem destroyed on level change. All
 *    registrations lost. New level re-registers all actors on BeginPlay.
 *
 * 3. RACE CONDITIONS: Actor may be destroyed before GUID registration
 *    completes. Always check Actor validity in HandleGuidAvailable.
 *
 * 4. DELEGATE BINDING: Uses AddUObject for delegates - will auto-unbind
 *    on object destruction, but manual cleanup in Deinitialize recommended.
 *
 * RELATED FILES:
 * - MOIdentityComponent.h - Per-actor component that owns the GUID
 * - MOPersistenceSubsystem.h - Uses this for GUID-based save/load
 * - MOInventoryComponent.h - References items by GUID
 *
 * TESTING CHECKLIST:
 * [ ] New actors get registered automatically
 * [ ] Destroyed actors removed from maps
 * [ ] Level transition re-registers existing actors
 * [ ] GetRegisteredCount() matches expected actor count
 * [ ] No duplicate registrations (check TrackedActors)
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOIdentityRegistrySubsystem.generated.h"

class UMOIdentityComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOIdentityRegisteredSignature, const FGuid&, StableGuid, AActor*, Actor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOIdentityUnregisteredSignature, const FGuid&, StableGuid, AActor*, Actor);

UCLASS()
class MOFRAMEWORK_API UMOIdentityRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

public:
	// UWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Queries
	UFUNCTION(BlueprintCallable, Category="MO|Identity")
	bool TryResolveActor(const FGuid& Guid, AActor*& OutActor) const;

	UFUNCTION(BlueprintCallable, Category="MO|Identity")
	AActor* ResolveActorOrNull(const FGuid& Guid) const;

	UFUNCTION(BlueprintCallable, Category="MO|Identity")
	bool TryGetGuidFromActor(const AActor* Actor, FGuid& OutGuid) const;

	UFUNCTION(BlueprintCallable, Category="MO|Identity")
	int32 GetRegisteredCount() const;

	UPROPERTY(BlueprintAssignable, Category="MO|Identity")
	FMOIdentityRegisteredSignature OnIdentityRegistered;

	UPROPERTY(BlueprintAssignable, Category="MO|Identity")
	FMOIdentityUnregisteredSignature OnIdentityUnregistered;

private:
	// Registration lifecycle
	void RegisterExistingActors();
	void TryTrackActor(AActor* Actor);

	UFUNCTION()
	void HandleActorSpawned(AActor* SpawnedActor);

	UFUNCTION()
	void HandleGuidAvailable(const FGuid& StableGuid);

	UFUNCTION()
	void HandleActorDestroyed(AActor* DestroyedActor);

	void RegisterGuidForActor(const FGuid& Guid, AActor* Actor);
	void UnregisterActor(AActor* Actor);

private:
	// Guid -> Actor
	TMap<FGuid, TWeakObjectPtr<AActor>> GuidToActor;

	// Actor -> Guid (for fast removal and reverse queries)
	TMap<TWeakObjectPtr<AActor>, FGuid> ActorToGuid;

	// Track which actors we have bound to to avoid double-binding
	TSet<TWeakObjectPtr<AActor>> TrackedActors;

	// Spawn delegate handle
	FDelegateHandle ActorSpawnedHandle;
};
