#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "MOInteractionSubsystem.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOInteractionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UMOInteractionSubsystem();

	// Server validation: max distance between viewpoint and target.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	float MaximumInteractDistance = 500.0f;

	// Rate limit per controller.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	float MinimumSecondsBetweenInteract = 0.15f;

	// If true, validate line of sight on the server.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	TEnumAsByte<ECollisionChannel> ValidationTraceChannel = ECC_Visibility;

	// Server-only execution entry point.
	UFUNCTION(BlueprintCallable, Category="MO|Interaction")
	bool ServerExecuteInteract(AController* InteractorController, AActor* TargetActor);

	// Server authoritative targeting trace settings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	float ServerTraceRadius = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	float ServerTraceForwardOffset = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	TEnumAsByte<ECollisionChannel> InteractTraceChannel = ECC_Visibility; // set to your "Interactable" channel in defaults

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	bool bServerTraceComplex = false;

	// Optional: require that the target is roughly in front of the viewpoint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	float MaxInteractAngleDegrees = 75.0f;

	// Optional: clamp suspicious viewpoint offsets (useful for 3P camera spoof scenarios).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	float MaxViewpointDistanceFromPawn = 250.0f;

	// Helper utilities (non-UFUNCTION, server-side use).
	bool FindServerInteractTarget(AController* InteractorController, AActor*& OutTargetActor, FHitResult& OutHit) const;
	bool PassesViewCone(const FVector& ViewLocation, const FRotator& ViewRotation, const AActor* TargetActor) const;
	FVector ComputeAimPoint(const AActor* TargetActor) const;

private:
	mutable TMap<FObjectKey, double> LastInteractTimeSeconds;

	bool ResolveServerViewpoint(AController* InteractorController, FVector& OutViewLocation, FRotator& OutViewRotation) const;
	bool HasServerLineOfSight(const FVector& ViewLocation, const AActor* InteractorPawnActor, const AActor* TargetActor) const;
};
