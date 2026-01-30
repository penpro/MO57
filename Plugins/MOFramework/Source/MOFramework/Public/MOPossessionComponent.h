#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOPossessionComponent.generated.h"

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOPossessionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOPossessionComponent();

	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	bool TryPossessNearestPawn();

	// Call this from BP input: pass any Actor BP class to spawn.
	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	bool TrySpawnActorNearController(TSubclassOf<AActor> ActorClassToSpawn, float SpawnDistance = 300.0f, FVector SpawnOffset = FVector::ZeroVector, bool bUseViewRotation = true);

	/** Spawn a pawn and immediately possess it. */
	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	bool TrySpawnAndPossessPawn(TSubclassOf<APawn> PawnClassToSpawn, float SpawnDistance = 300.0f, FVector SpawnOffset = FVector::ZeroVector, bool bUseViewRotation = true);

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerTryPossessNearestPawn();

	UFUNCTION(Server, Reliable)
	void ServerSpawnActorNearController(TSubclassOf<AActor> ActorClassToSpawn, float SpawnDistance, FVector SpawnOffset, bool bUseViewRotation);

	UFUNCTION(Server, Reliable)
	void ServerSpawnAndPossessPawn(TSubclassOf<APawn> PawnClassToSpawn, float SpawnDistance, FVector SpawnOffset, bool bUseViewRotation);
};
