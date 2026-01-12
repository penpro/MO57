#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h" // for UWorld::InitializationValues
#include "Subsystems/GameInstanceSubsystem.h"
#include "MOPersistenceSubsystem.generated.h"


class UMOIdentityComponent;
class UMOIdentityRegistrySubsystem;
class UMOWorldSaveGame;

UCLASS()
class MOFRAMEWORK_API UMOPersistenceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="MO|Persistence")
	bool SaveWorldToSlot(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category="MO|Persistence")
	bool LoadWorldFromSlot(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category="MO|Persistence")
	bool IsGuidDestroyed(const FGuid& Guid) const;

private:
	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);
	void BindToWorld(UWorld* World);
	void UnbindFromWorld();

	void ApplyDestroyedGuidsToWorld(UWorld* World);

	UFUNCTION()
	void HandleIdentityRegistered(const FGuid& StableGuid, AActor* Actor);

	UFUNCTION()
	void HandleIdentityDestroyed(const FGuid& StableGuid);

private:
	UPROPERTY()
	TSet<FGuid> SessionDestroyedGuids;

	UPROPERTY()
	TObjectPtr<UMOWorldSaveGame> LoadedWorldSave;

	FDelegateHandle PostWorldInitHandle;

	UPROPERTY()
	TWeakObjectPtr<UWorld> BoundWorld;

	UPROPERTY()
	TWeakObjectPtr<UMOIdentityRegistrySubsystem> BoundRegistry;
};
