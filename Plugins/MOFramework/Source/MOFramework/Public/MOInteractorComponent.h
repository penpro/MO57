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
 * -> If ISM/HISM: ServerRequestInstanceInteract(FMOInstanceRef: mesh + world location; the server resolves its OWN instance)
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
 * [2026-10-18] NO POINTERS TO PCG INSTANCES ACROSS THE WIRE.
 *   SYMPTOM: a joining player's E on a stick / stone / flint did nothing (Steam test); the host logged "Unable to resolve default guid from
 *   client: ObjectName: PCGPartitionActor_2147442263".
 *   WHY: PCG partition actors and their ISM components are spawned at runtime under names that differ per machine, and instance indices
 *   shift as instances are removed: the old ServerRequestISMInteract(owner, comp, index) arrived as null on the host.
 *   FIX: the RPC carries FMOInstanceRef (MOInstanceRef.h: static mesh + world location) and the server resolves its own copy of the instance.
 *   An instance another player already took no longer resolves, and the request is refused with a log line.
 *
 * [2026-10-22] AN INSTANCE'S PIVOT IS NOT WHERE YOU SEE IT (C8).
 *   SYMPTOM: a line of sight to a tree's or a boulder's pivot is blocked by the tree or the boulder itself, or by the ground it is sunk in.
 *   FIX: IsInstanceInSight tries the pivot, its axis at the viewer's height, the bounds' centre and top; a block by the instance's own component
 *   or near the point counts as seen. The server refuses an instance request out of sight ("rejected: out of sight", ServerCheckInstanceRequest).
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
#include "MOInstanceRef.h"
#include "MOInteractorComponent.generated.h"

class APawn;
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

	/**
	 * True when the hit is world cover with no harvest data of its own (biome grass, flowers, shrubs, debris that no resource row
	 * describes): interacting forages the ground there (AMOPlayerController::TryOpenForageMenuAtCrosshair). Every thing in the world has
	 * an action -- named harvests come from DT_Resources, everything else forages (Wes, Oct 15 2026).
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Interaction")
	bool bIsForageTarget = false;

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
		bIsForageTarget = false;
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

	/**
	 * Around the point the crosshair rests on, an interactable actor this close (and in sight) counts as aimed at. A small thing lying on the
	 * ground is never the first thing the sweep touches -- the ground in front of it is (Oct 16 2026: the joiner's E on a piece of bark opened
	 * the forage menu). 0 disables.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	float AimAssistRadius = 40.0f;
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
	 * Whether instances of this component are a harvest source or an item pickup: KeepOnHarvest / Harvestable (component or owner) /
	 * MOResource_* tags, or a PCG tag / mesh -> item mapping. The one rule E, right-click and the focus hint share (it was written out twice).
	 */
	static bool IsHarvestableInstanceComponent(const UInstancedStaticMeshComponent* ISMComp, const UWorld* World);

	/** Whether instances of this component are decorative world cover (a biome palette's grass / flower / shrub / debris layer): they forage. */
	static bool IsWorldCoverComponent(const UInstancedStaticMeshComponent* ISMComp);

	/**
	 * What interaction looks THROUGH: decorative world cover with no harvest data of its own. A stick lying in grass is picked up, not the grass
	 * foraged (Oct 16 2026: the joiner's E on an item in grass opened the forage menu); the grass still forages when nothing interactable lies
	 * behind it. Shared by the client's targeting and the server's line-of-sight check, so the two agree.
	 */
	static bool IsSeeThroughForInteraction(const UPrimitiveComponent* Component);

	/**
	 * Whether a viewer at ViewLocation sees instance Index of ISM: a line to its pivot (5 cm up), its axis at the viewer's height, the centre or the
	 * top of its bounds meets nothing solid but see-through cover, the instance's own component, or something within NearTolerance of that point
	 * (the ground a stone lies on). The pivot alone is not enough: a trunk's or a boulder's pivot sits inside it or under the ground (C8). One rule
	 * for the client's aim assist and the server's instance requests (ServerCheckInstanceRequest).
	 */
	static bool IsInstanceInSight(UWorld* World, const FVector& ViewLocation, const UInstancedStaticMeshComponent* ISM, int32 Index, const AActor* IgnoredActor,
		ECollisionChannel Channel, float NearTolerance);

	/** What the server made of a player's request on an instance (ServerCheckInstanceRequest). */
	enum class EInstanceRequestCheck : uint8 { Ok, BadIndex, TooFar, OutOfSight };

	/**
	 * The server's check of an instance a player asked to pick up or harvest (ServerRequestInstanceInteract, ServerBeginHarvest): a valid index,
	 * within reach of the pawn (twice the interact distance: an instance's pivot can sit metres from the surface aimed at) and in sight of the
	 * player's view (IsInstanceInSight, on the pawn's interaction trace channel; mo.Interact.ServerInstanceSight). OutWhy says why not, for the log.
	 */
	static EInstanceRequestCheck ServerCheckInstanceRequest(APawn* Pawn, const UInstancedStaticMeshComponent* ISM, int32 Index, FString& OutWhy);

private:
	/** One hit as an interaction target: a harvest / HISM instance, decorative cover (forage) or an actor with an interactable component. */
	bool EvaluateInteractionHit(const FHitResult& HitResult, FMOInteractionTarget& OutTarget) const;

	/** The nearest actor with an interactable component within AimAssistRadius of AimPoint that the viewer can see (the server's own LOS rule). */
	bool FindInteractableNearAimPoint(const FVector& ViewLocation, const FVector& AimPoint, FMOInteractionTarget& OutTarget) const;

public:

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

	/**
	 * Request the server to interact with a SPECIFIC, already-resolved actor (e.g. a
	 * UI-driven pickup of a known world item, where no re-trace is wanted). Locally
	 * controlled clients route through the authoritative ServerRequestInteract.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Interaction")
	void RequestInteractWithActor(AActor* TargetActor);

	/**
	 * Ask the server to interact with instance InstanceIndex of ISM (a stick, a stone on the ground): what E does on an instance, for a caller that
	 * already chose it (the right-click menu's Pick Up). Sent as an FMOInstanceRef. False when the pawn is not locally controlled or the instance is
	 * not valid here.
	 */
	bool RequestInteractWithInstance(const UInstancedStaticMeshComponent* ISM, int32 InstanceIndex);

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

	/**
	 * Server RPC for a UI-driven pickup of a SPECIFIC, already-resolved world item
	 * (nearby-items menu, loot-all, drag-to-slot). Unlike ServerRequestInteract this
	 * does NOT re-trace from the crosshair -- the UI already chose the item -- it just
	 * distance-validates and routes to the canonical GiveToInteractorInventory (#159).
	 */
	UFUNCTION(Server, Reliable)
	void ServerPickUpWorldItem(AActor* TargetActor);

	UFUNCTION(Server, Reliable)
	void ServerRequestSecondaryInteract(AActor* TargetActor);

	/**
	 * Server RPC for an ISM / HISM instance (a stick, a stone, a flint on the ground). It names the instance by mesh + world location, never by
	 * component pointer or index: neither means the same thing on another machine (see KNOWN PITFALLS, 2026-10-18).
	 */
	UFUNCTION(Server, Reliable)
	void ServerRequestInstanceInteract(const FMOInstanceRef& Instance);

private:
	bool ResolveViewpoint(FVector& OutViewLocation, FRotator& OutViewRotation) const;
	void BuildTrace(const FVector& ViewLocation, const FRotator& ViewRotation, FVector& OutTraceStart, FVector& OutTraceEnd) const;
	bool TraceForHit(const FVector& TraceStart, const FVector& TraceEnd, FHitResult& OutHitResult) const;

	/**
	 * Server-side validation for the instance-harvest RPCs: ServerCheckInstanceRequest (index, reach, line of sight) and minimum spacing
	 * between accepted requests. The client names the instance, so without this a client could harvest any instance in the world, through
	 * walls, at unlimited rate (the actor path validates in UMOInteractionSubsystem; the instance path must match it).
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
