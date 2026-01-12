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
