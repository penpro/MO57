/**
 * =============================================================================
 * MOInteractorComponent.h - Player Interaction Initiation Component
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Pawn component that initiates interactions. Traces from player viewpoint,
 * finds interactable targets (actors or ISM/HISM instances), and sends
 * server requests. The "active" half of the interaction system.
 *
 * KEY RESPONSIBILITIES:
 * 1. Trace from player viewpoint to find interaction targets
 * 2. Support both actor-based and ISM/HISM instance targets
 * 3. Send server RPCs for interaction requests
 * 4. Provide current target info for UI prompts
 * 5. Handle interaction config (distance, radius, channel)
 *
 * ARCHITECTURE NOTES:
 * - Traces from camera/viewpoint, not pawn location
 * - Uses FMOInteractionTarget to unify actor and ISM/HISM results
 * - Server RPCs ensure authoritative interaction handling
 * - TraceConfig controls trace parameters
 *
 * INTERACTION TYPES:
 * - Actor: Normal actors with UMOInteractableComponent
 * - ISM: Instanced Static Mesh instances (PCG foliage)
 * - HISM: Hierarchical ISM instances (harvestable trees)
 *
 * INTERACTION FLOW:
 * Player input -> TryInteract() -> FindInteractionTarget()
 * -> If actor: ServerRequestInteract()
 * -> If HISM: ServerRequestHISMInteract(owner, comp, index)
 * -> If ISM: ServerRequestISMInteract(owner, comp, index)
 *
 * CRITICAL PATTERNS:
 * 1. Finding Targets:
 *    FindInteractionTarget(OutTarget) fills FMOInteractionTarget
 *    Check OutTarget.bIsInstancedMeshTarget for ISM/HISM
 *
 * 2. UI Prompts:
 *    GetCurrentInteractionPrompt() returns appropriate text
 *    CurrentTarget updated by FindInteractionTarget()
 *
 * 3. Viewpoint Resolution:
 *    ResolveViewpoint() handles both player and AI controllers
 *    Uses MOViewpointUtils internally
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] VIEWPOINT TIMING: Camera may not be ready in early BeginPlay.
 *   SYMPTOM: FindInteractionTarget() returns false, no interaction possible.
 *   WHY: Camera component initializes asynchronously in PIE.
 *   FIX: Don't call TryInteract() in BeginPlay. Use player input callbacks
 *   which fire after camera is ready. Check log for "Failed to resolve viewpoint".
 *
 * [2024-02] ISM INDEX VALIDITY: Instance index can become stale.
 *   SYMPTOM: ServerRequestISMInteract fails silently, no interaction occurs.
 *   WHY: Another player harvested the ISM instance between trace and RPC.
 *   FIX: Server validates index against current ISM instance count. If invalid,
 *   interaction gracefully fails. Client should re-trace on next input.
 *
 * [2024-02] TRACE CHANNEL: Default ECC_Visibility may miss custom collision.
 *   SYMPTOM: Can't interact with certain objects (doors, vehicles).
 *   WHY: Objects using custom collision channels aren't hit by visibility trace.
 *   FIX: Configure TraceConfig.TraceChannel to match your collision setup.
 *   DEBUG: Enable "Show Collision" in viewport to visualize trace hits.
 *
 * [2024-02] DEPRECATED API: FindInteractTarget() -> FindInteractionTarget().
 *   WHY: Old API was actor-only, new API supports ISM/HISM instances.
 *   SYMPTOM: Deprecation warning in compile output.
 *   FIX: Replace FindInteractTarget(Actor) with FindInteractionTarget(Target).
 *
 * [2024-02] TARGET STALING: CurrentTarget cached between frames.
 *   SYMPTOM: GetCurrentInteractionPrompt() shows stale target info.
 *   WHY: CurrentTarget updated only when TryInteract/TrySecondaryInteract called.
 *   FIX: For real-time prompts, call FindInteractionTarget() each frame in Tick,
 *   or use a timer to refresh every 0.1s. Don't rely on cached CurrentTarget.
 *
 * [2024-02] ISM OWNER MISMATCH: Trace must hit ISM's owning actor.
 *   SYMPTOM: ServerRequestISMInteract fails even with valid instance index.
 *   WHY: RPC passes OwnerActor from trace. If trace hits Actor A but ISM belongs
 *   to Actor B, server validation fails.
 *   FIX: Ensure ISM is attached to same actor that has collision. Check
 *   HitResult.GetActor() == ISMComponent->GetOwner().
 *
 * [2024-02] VIEW OFFSET EDGE CASE: ViewStartForwardOffset (15u default).
 *   SYMPTOM: Can't interact while standing inside large interactable.
 *   WHY: Offset moves trace start forward to avoid self-intersection.
 *   If inside object, trace starts past it.
 *   FIX: Reduce ViewStartForwardOffset or use sphere trace instead of line.
 *
 * [SEVERITY GUIDE]
 *   VIEWPOINT TIMING: Runtime bug (interaction fails at game start)
 *   ISM INDEX VALIDITY: Expected edge case (handled gracefully)
 *   TRACE CHANNEL: Configuration issue (fixable in editor)
 *   TARGET STALING: UX bug (stale prompts)
 *
 * RELATED FILES:
 * - MOInteractableComponent.h - Receives interaction requests
 * - MOHISMInteractableComponent.h - HISM-specific interaction handling
 * - MOInteractionSubsystem.h - Server-side validation
 * - MOPCGInteractionSubsystem.h - PCG/HISM interaction processing
 *
 * TESTING CHECKLIST:
 * [ ] TryInteract triggers interaction on actor target
 * [ ] TryInteract triggers interaction on HISM target
 * [ ] TrySecondaryInteract works for right-click
 * [ ] FindInteractionTarget returns correct target type
 * [ ] GetCurrentInteractionPrompt shows correct text
 * [ ] TraceConfig.TraceDistance limits interaction range
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

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

	/**
	 * Legacy method - finds actor-based targets only.
	 * @deprecated Use FindInteractionTarget() instead, which supports both actors and ISM/HISM instances.
	 */
	UE_DEPRECATED(5.7, "Use FindInteractionTarget() instead, which supports both actors and ISM/HISM instances.")
	UFUNCTION(BlueprintCallable, Category="MO|Interaction", meta=(DeprecatedFunction, DeprecationMessage="Use FindInteractionTarget instead"))
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

	/**
	 * Server-side validation for the instance-harvest RPCs: index in range,
	 * instance within interact reach of the pawn, and minimum spacing
	 * between accepted requests. The client supplies the component and
	 * index, so without this a client could harvest any instance in the
	 * world at unlimited rate (the actor path validates in
	 * UMOInteractionSubsystem; the instance path must match it).
	 */
	bool ValidateInstanceHarvestRequest(UInstancedStaticMeshComponent* Component, int32 InstanceIndex, const TCHAR* RPCName);

	/** Server time of the last accepted instance-harvest RPC (rate limiting). */
	double LastInstanceHarvestServerTime = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Interaction", meta=(AllowPrivateAccess="true"))
	FMOInteractionTraceConfig TraceConfig;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastTracedActor;

	/** Current interaction target (updated each frame or on demand). */
	UPROPERTY(Transient)
	FMOInteractionTarget CurrentTarget;
};
