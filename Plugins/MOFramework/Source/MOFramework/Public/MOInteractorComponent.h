#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOInteractorComponent.generated.h"

class UMOHISMInteractableComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;

/**
 * Result of an interaction target query.
 * Can be either a regular actor with UMOInteractableComponent,
 * or an ISM/HISM instance (PCG-spawned meshes).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOInteractionTarget
{
	GENERATED_BODY()

	/** The actor that was hit (always valid if target found). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Interaction")
	TWeakObjectPtr<AActor> TargetActor;

	/** For ISM/HISM hits: the ISM component that was hit. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Interaction")
	TWeakObjectPtr<UInstancedStaticMeshComponent> ISMComponent;

	/** For HISM hits: the HISM component that was hit (same as ISMComponent but typed). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Interaction")
	TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> HISMComponent;

	/** For ISM/HISM hits: the instance index that was hit. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Interaction")
	int32 InstanceIndex = INDEX_NONE;

	/** For HISM hits: the HISM interactable component (if using component-based approach). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Interaction")
	TWeakObjectPtr<UMOHISMInteractableComponent> HISMInteractable;

	/** True if this is an instanced mesh target (ISM or HISM). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Interaction")
	bool bIsInstancedMeshTarget = false;

	/** True if the instanced mesh is specifically HISM (hierarchical). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Interaction")
	bool bIsHISM = false;

	/** The hit result from the trace. */
	FHitResult HitResult;

	bool IsValid() const
	{
		if (bIsInstancedMeshTarget)
		{
			return ISMComponent.IsValid() && InstanceIndex != INDEX_NONE;
		}
		return TargetActor.IsValid();
	}

	void Reset()
	{
		TargetActor.Reset();
		ISMComponent.Reset();
		HISMComponent.Reset();
		InstanceIndex = INDEX_NONE;
		HISMInteractable.Reset();
		bIsInstancedMeshTarget = false;
		bIsHISM = false;
		HitResult = FHitResult();
	}
};

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOInteractionTraceConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	float TraceDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	float TraceRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	bool bTraceComplex = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	float ViewStartForwardOffset = 15.0f;
};

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOInteractorComponent();

	/** Find an interaction target (actor or HISM instance). */
	UFUNCTION(BlueprintCallable, Category="MO|Interaction")
	bool FindInteractionTarget(FMOInteractionTarget& OutTarget) const;

	/** Legacy method - finds actor-based targets only. */
	UFUNCTION(BlueprintCallable, Category="MO|Interaction")
	bool FindInteractTarget(AActor*& OutTargetActor, FHitResult& OutHitResult) const;

	/** Try to interact with whatever we're looking at. */
	UFUNCTION(BlueprintCallable, Category="MO|Interaction")
	bool TryInteract();

	/** Try secondary interaction with whatever we're looking at. */
	UFUNCTION(BlueprintCallable, Category="MO|Interaction")
	bool TrySecondaryInteract();

	/** Get the current interaction target (updated by FindInteractionTarget). */
	UFUNCTION(BlueprintPure, Category="MO|Interaction")
	const FMOInteractionTarget& GetCurrentTarget() const { return CurrentTarget; }

	/** Get interaction prompt text for current target. */
	UFUNCTION(BlueprintPure, Category="MO|Interaction")
	FText GetCurrentInteractionPrompt() const;

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerRequestInteract(AActor* TargetActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestSecondaryInteract(AActor* TargetActor);

	/** Server RPC for HISM interaction. */
	UFUNCTION(Server, Reliable)
	void ServerRequestHISMInteract(AActor* OwnerActor, UHierarchicalInstancedStaticMeshComponent* HISMComponent, int32 InstanceIndex);

	/** Server RPC for ISM interaction. */
	UFUNCTION(Server, Reliable)
	void ServerRequestISMInteract(AActor* OwnerActor, UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex);

private:
	bool ResolveViewpoint(FVector& OutViewLocation, FRotator& OutViewRotation) const;
	void BuildTrace(const FVector& ViewLocation, const FRotator& ViewRotation, FVector& OutTraceStart, FVector& OutTraceEnd) const;
	bool TraceForHit(const FVector& TraceStart, const FVector& TraceEnd, FHitResult& OutHitResult) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Interaction", meta=(AllowPrivateAccess="true"))
	FMOInteractionTraceConfig TraceConfig;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastTracedActor;

	/** Current interaction target (updated each frame or on demand). */
	UPROPERTY(Transient)
	FMOInteractionTarget CurrentTarget;
};
