/**
 * =============================================================================
 * MOViewpointUtils.h - Viewpoint Resolution & Line-of-Sight Utilities
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Centralized viewpoint resolution for the entire framework. Use this instead
 * of implementing viewpoint logic in each component. Handles the differences
 * between PlayerController camera viewpoints and AI pawn eye viewpoints.
 *
 * KEY RESPONSIBILITIES:
 * 1. Resolve view location/rotation for any controller type
 * 2. Provide line-of-sight checking with configurable options
 * 3. View cone checks for AI perception
 *
 * WHY THIS EXISTS:
 * Before this utility, these subsystems each had duplicate viewpoint code:
 * - UMOInteractionSubsystem::ResolveServerViewpoint()
 * - UMOPossessionSubsystem::ResolveViewpoint()
 * - UMOInteractorComponent::ResolveViewpoint()
 * - UMOTerraformingComponent::ResolveViewpoint()
 *
 * USE THIS UTILITY INSTEAD OF WRITING NEW VIEWPOINT CODE.
 *
 * =============================================================================
 * BEST PRACTICES
 * =============================================================================
 *
 * 1. For player interactions, use ResolveViewpointForController() - it handles
 *    both player camera and AI pawn eyes automatically.
 *
 * 2. For AI-only code, use ResolveViewpointForPawn() directly for performance.
 *
 * 3. For line-of-sight checks, use the Options struct to configure trace
 *    channel and whether to allow hits on attached actors (important for
 *    characters with equipment).
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-01] CAMERA VS EYES: Player controllers return CAMERA position, not
 *   pawn eye position. This is intentional for interactions - players click
 *   what they see on screen. AI pawns return eye socket position.
 *
 * [2024-02] ATTACHED ACTORS: When checking LOS to a character with equipment
 *   (held items, backpacks), set bAllowAttachedHits=true or the trace may
 *   hit the equipment instead of the character and return false.
 *
 * [2024-02] NULL WORLD: HasLineOfSight requires valid World pointer. If called
 *   during actor destruction, World may be null. Always null-check.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOInteractionSubsystem.h - Uses this for interaction traces
 * - MOInteractorComponent.h - Uses this for hover detection
 * - MOTerraformingComponent.h - Uses this for terraforming raycast
 * - MOPossessionSubsystem.h - Uses this for possession range checks
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

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
 * See file header for usage guide and pitfalls.
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
