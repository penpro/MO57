/**
 * =============================================================================
 * MOBuoyancyComponent.h - Physics Buoyancy for Water Bodies
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Component that applies buoyancy forces to physics actors based on water
 * surface queries from MOWaterActorBase. Supports single-point or multi-point
 * buoyancy for realistic boat/object physics.
 *
 * FEATURES:
 * - Single-point buoyancy (actor center)
 * - Multi-point buoyancy (configurable sample points for boats)
 * - Depth-based force calculation
 * - Water drag (linear and angular)
 * - Submersion percentage tracking
 * - Enter/exit water events
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] PHYSICS COMPONENT: PhysicsComponent must be set or auto-found.
 *   If null, no forces are applied. Check GetPhysicsComponent() return.
 *
 * [2024-02] WATER ACTOR: WaterActor is weak pointer. If water actor is destroyed,
 *   buoyancy stops. Use bAutoFindWater=true to automatically find new water.
 *
 * [2024-02] TICK FREQUENCY: Ticks every frame for smooth physics. Consider
 *   substepping or reducing tick rate for many buoyant objects.
 *
 * [2024-02] FORCE SCALE: BuoyancyForce is in Newtons. Default 2000 works for
 *   ~100kg objects. Scale proportionally for mass.
 *
 * =============================================================================
 * RELATED FILES: MOWaterActorBase.h, MOWaterTypes.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOBuoyancyComponent.generated.h"

class AMOWaterActorBase;
class UPrimitiveComponent;

/**
 * Buoyancy point for multi-point buoyancy calculation.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBuoyancyPoint
{
	GENERATED_BODY()

	/** Local offset from actor origin for this buoyancy point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Buoyancy")
	FVector LocalOffset = FVector::ZeroVector;

	/** Buoyancy force multiplier for this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Buoyancy", meta=(ClampMin="0.0"))
	float ForceMultiplier = 1.0f;

	/** Drag multiplier when this point is underwater. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Buoyancy", meta=(ClampMin="0.0"))
	float DragMultiplier = 1.0f;

	// Runtime state
	UPROPERTY(BlueprintReadOnly, Category="Buoyancy")
	bool bIsUnderwater = false;

	UPROPERTY(BlueprintReadOnly, Category="Buoyancy")
	float CurrentDepth = 0.0f;
};

/**
 * Component that applies buoyancy forces based on water actor surface queries.
 * Supports single-point or multi-point buoyancy for realistic boat physics.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOBuoyancyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOBuoyancyComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================================================
	// WATER REFERENCE
	// ============================================================================

	/** The water actor to query for surface height. If null, will try to find one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Buoyancy")
	TWeakObjectPtr<AMOWaterActorBase> WaterActor;

	/** Whether to automatically find a water actor if none is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Buoyancy")
	bool bAutoFindWater = true;

	/** Tag to use when searching for water actors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Buoyancy", meta=(EditCondition="bAutoFindWater"))
	FName WaterActorTag = NAME_None;

	// ============================================================================
	// BUOYANCY CONFIGURATION
	// ============================================================================

	/** Base buoyancy force (upward acceleration when fully submerged). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Buoyancy|Forces", meta=(ClampMin="0.0"))
	float BuoyancyForce = 2000.0f;

	/** How quickly buoyancy force increases with depth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Buoyancy|Forces", meta=(ClampMin="0.0"))
	float BuoyancyDamping = 0.5f;

	/** Linear drag when underwater (slows movement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Buoyancy|Forces", meta=(ClampMin="0.0"))
	float WaterLinearDrag = 3.0f;

	/** Angular drag when underwater (slows rotation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Buoyancy|Forces", meta=(ClampMin="0.0"))
	float WaterAngularDrag = 1.0f;

	/** Depth at which buoyancy force is at maximum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Buoyancy|Forces", meta=(ClampMin="1.0"))
	float MaxBuoyancyDepth = 100.0f;

	// ============================================================================
	// BUOYANCY POINTS
	// ============================================================================

	/** Whether to use multiple buoyancy points. If false, uses actor center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Buoyancy|Points")
	bool bUseMultiplePoints = false;

	/** Buoyancy sample points for multi-point buoyancy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Buoyancy|Points", meta=(EditCondition="bUseMultiplePoints"))
	TArray<FMOBuoyancyPoint> BuoyancyPoints;

	// ============================================================================
	// PHYSICS TARGET
	// ============================================================================

	/** Component to apply physics forces to. If null, uses first primitive with physics enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Buoyancy|Physics")
	TWeakObjectPtr<UPrimitiveComponent> PhysicsComponent;

	// ============================================================================
	// STATE
	// ============================================================================

	/** Whether the actor is currently in water. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Buoyancy|State")
	bool bIsInWater = false;

	/** Current submersion percentage (0-1). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Buoyancy|State")
	float SubmersionPercent = 0.0f;

	/** Current water surface height at actor location. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Buoyancy|State")
	float CurrentWaterHeight = 0.0f;

	/** Current depth below water (negative if above). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Buoyancy|State")
	float CurrentDepth = 0.0f;

	// ============================================================================
	// EVENTS
	// ============================================================================

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOEnteredWaterSignature);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOExitedWaterSignature);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOSubmersionChangedSignature, float, SubmersionPercent);

	/** Called when the actor enters water. */
	UPROPERTY(BlueprintAssignable, Category="MO|Buoyancy|Events")
	FMOEnteredWaterSignature OnEnteredWater;

	/** Called when the actor exits water. */
	UPROPERTY(BlueprintAssignable, Category="MO|Buoyancy|Events")
	FMOExitedWaterSignature OnExitedWater;

	/** Called when submersion percentage changes significantly. */
	UPROPERTY(BlueprintAssignable, Category="MO|Buoyancy|Events")
	FMOSubmersionChangedSignature OnSubmersionChanged;

	// ============================================================================
	// FUNCTIONS
	// ============================================================================

	/** Find and set the water actor reference. */
	UFUNCTION(BlueprintCallable, Category="MO|Buoyancy")
	void FindWaterActor();

	/** Get the current water surface normal at the actor location. */
	UFUNCTION(BlueprintPure, Category="MO|Buoyancy")
	FVector GetWaterSurfaceNormal() const;

	/** Get water velocity at the actor location (from wave motion). */
	UFUNCTION(BlueprintPure, Category="MO|Buoyancy")
	FVector GetWaterVelocity() const;

protected:
	/** Apply buoyancy forces for this frame. */
	void ApplyBuoyancyForces(float DeltaTime);

	/** Apply forces for a single buoyancy point. */
	void ApplyPointBuoyancy(const FVector& WorldPosition, float ForceMultiplier, float DragMultiplier, float DeltaTime);

	/** Update state variables. */
	void UpdateState();

private:
	/** Cached physics component. */
	TWeakObjectPtr<UPrimitiveComponent> CachedPhysicsComponent;

	/** Last submersion percent for change detection. */
	float LastSubmersionPercent = 0.0f;

	/** Get the physics component to apply forces to. */
	UPrimitiveComponent* GetPhysicsComponent();
};
