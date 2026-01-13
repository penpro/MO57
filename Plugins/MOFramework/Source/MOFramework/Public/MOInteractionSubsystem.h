#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOInteractionSubsystem.generated.h"

class UMOInteractableComponent;

USTRUCT(BlueprintType)
struct FMOInteractionHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> HitActor = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UMOInteractableComponent> InteractableComponent = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FHitResult HitResult;
};

UCLASS()
class MOFRAMEWORK_API UMOInteractionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Interaction")
	float TraceDistance = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UFUNCTION(BlueprintCallable, Category="MO|Interaction")
	bool QueryInteractable(APlayerController* PlayerController, FMOInteractionHit& OutHit) const;

	/** Client helper: find target and route request to server (via interface). */
	UFUNCTION(BlueprintCallable, Category="MO|Interaction")
	bool TryInteract(APlayerController* PlayerController);

	/** Server-side execution. Call this from your Server RPC. */
	UFUNCTION(BlueprintCallable, Category="MO|Interaction")
	bool ServerExecuteInteract(AController* InteractorController, AActor* TargetActor);

private:
	bool ComputeViewTrace(APlayerController* PlayerController, FVector& OutTraceStart, FVector& OutTraceEnd) const;
};
