#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOPossessionSubsystem.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOPossessionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Possession")
	float MaximumPossessDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Possession")
	bool bRequireLineOfSight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Possession")
	TEnumAsByte<ECollisionChannel> LineOfSightTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Possession")
	bool bAllowSwitchPossession = true;

	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	bool ServerPossessNearestPawn(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	APawn* FindNearestUnpossessedPawn(APlayerController* PlayerController) const;

	// Spawn any Actor BP near controller viewpoint (server only).
	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	AActor* ServerSpawnActorNearController(APlayerController* PlayerController, TSubclassOf<AActor> ActorClassToSpawn, float SpawnDistance = 300.0f, FVector SpawnOffset = FVector::ZeroVector, bool bUseViewRotation = true);

	/** Spawn a pawn and immediately possess it (server only). */
	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	APawn* ServerSpawnAndPossessPawn(APlayerController* PlayerController, TSubclassOf<APawn> PawnClassToSpawn, float SpawnDistance = 300.0f, FVector SpawnOffset = FVector::ZeroVector, bool bUseViewRotation = true);

private:
	bool ResolveViewpoint(APlayerController* PlayerController, FVector& OutViewLocation, FRotator& OutViewRotation) const;
	bool HasLineOfSight(UWorld* World, const FVector& ViewLocation, const APawn* TargetPawn) const;
};
