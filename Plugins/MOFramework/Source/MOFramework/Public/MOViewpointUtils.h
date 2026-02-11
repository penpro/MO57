#pragma once

#include "CoreMinimal.h"
#include "MOViewpointUtils.generated.h"

class AController;
class APlayerController;
class APawn;
class AActor;
class UWorld;

/**
 * Static utility functions for resolving viewpoints and performing line-of-sight checks.
 * Consolidates duplicate implementations from:
 * - UMOInteractionSubsystem::ResolveServerViewpoint()
 * - UMOPossessionSubsystem::ResolveViewpoint()
 * - UMOInteractorComponent::ResolveViewpoint()
 * - UMOTerraformingComponent::ResolveViewpoint()
 */
UCLASS()
class MOFRAMEWORK_API UMOViewpointUtils : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================================
	// VIEWPOINT RESOLUTION
	// ============================================================================

	/**
	 * Resolve the viewpoint for a controller.
	 * Tries PlayerController::GetPlayerViewPoint first, falls back to pawn eyes.
	 *
	 * @param Controller - The controller to resolve viewpoint for
	 * @param OutViewLocation - Receives the view location
	 * @param OutViewRotation - Receives the view rotation
	 * @return True if viewpoint was successfully resolved
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Utils|Viewpoint")
	static bool ResolveViewpointForController(AController* Controller, FVector& OutViewLocation, FRotator& OutViewRotation);

	/**
	 * Resolve the viewpoint for a player controller specifically.
	 * Uses PlayerController::GetPlayerViewPoint.
	 *
	 * @param PlayerController - The player controller
	 * @param OutViewLocation - Receives the view location
	 * @param OutViewRotation - Receives the view rotation
	 * @return True if viewpoint was successfully resolved
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Utils|Viewpoint")
	static bool ResolveViewpointForPlayerController(APlayerController* PlayerController, FVector& OutViewLocation, FRotator& OutViewRotation);

	/**
	 * Resolve the viewpoint for a pawn directly.
	 * Uses Pawn::GetActorEyesViewPoint.
	 *
	 * @param Pawn - The pawn to get viewpoint from
	 * @param OutViewLocation - Receives the view location
	 * @param OutViewRotation - Receives the view rotation
	 * @return True if viewpoint was successfully resolved
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Utils|Viewpoint")
	static bool ResolveViewpointForPawn(APawn* Pawn, FVector& OutViewLocation, FRotator& OutViewRotation);

	// ============================================================================
	// LINE OF SIGHT
	// ============================================================================

	/**
	 * Options for line of sight checks.
	 */
	struct FLineOfSightOptions
	{
		/** Trace channel to use for the check. */
		ECollisionChannel TraceChannel = ECC_Visibility;

		/** Whether to check for attached actors (allow hits on things attached to target). */
		bool bAllowAttachedHits = true;

		/** Actor to ignore in the trace (typically the interactor pawn). */
		const AActor* IgnoredActor = nullptr;

		FLineOfSightOptions() = default;
	};

	/**
	 * Check if there is line of sight between a view location and a target actor.
	 *
	 * @param World - The world to trace in
	 * @param ViewLocation - The origin of the line of sight check
	 * @param TargetActor - The target to check visibility of
	 * @param Options - Configuration for the check
	 * @return True if the target is visible from the view location
	 */
	static bool HasLineOfSight(UWorld* World, const FVector& ViewLocation, const AActor* TargetActor, const FLineOfSightOptions& Options = FLineOfSightOptions());

	/**
	 * Simple line of sight check - returns true if nothing blocks the path.
	 * Does not consider attached actors or special cases.
	 *
	 * @param World - The world to trace in
	 * @param ViewLocation - The origin of the line of sight check
	 * @param TargetLocation - The target location
	 * @param TraceChannel - The collision channel to use
	 * @param IgnoredActor - Optional actor to ignore
	 * @return True if the line of sight is clear
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Utils|Viewpoint")
	static bool HasLineOfSightSimple(UWorld* World, const FVector& ViewLocation, const FVector& TargetLocation, ECollisionChannel TraceChannel = ECC_Visibility, AActor* IgnoredActor = nullptr);

	// ============================================================================
	// VIEW CONE
	// ============================================================================

	/**
	 * Check if a target is within the view cone of the viewer.
	 *
	 * @param ViewLocation - The viewer's eye location
	 * @param ViewRotation - The viewer's look direction
	 * @param TargetLocation - The target's location
	 * @param MaxAngleDegrees - Maximum angle in degrees (0-180, 0 disables check)
	 * @return True if the target is within the view cone
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Utils|Viewpoint")
	static bool IsInViewCone(const FVector& ViewLocation, const FRotator& ViewRotation, const FVector& TargetLocation, float MaxAngleDegrees);
};
