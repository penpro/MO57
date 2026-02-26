/**
 * =============================================================================
 * MOInteractionSubsystem.h - Server-Authoritative Interaction System
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * WorldSubsystem providing server-side validation and execution of player
 * interactions. Handles primary (left-click) and secondary (right-click)
 * interactions with validation for distance, line-of-sight, and rate limiting.
 *
 * KEY RESPONSIBILITIES:
 * 1. Validate interaction requests (distance, LOS, angle, rate limit)
 * 2. Execute primary interactions (ServerExecuteInteract)
 * 3. Execute secondary interactions (ServerExecuteSecondaryInteract)
 * 4. Find valid interaction targets via server trace
 * 5. Prevent interaction spam via per-controller rate limiting
 *
 * ARCHITECTURE NOTES:
 * - WorldSubsystem: One per UWorld
 * - Server-only execution (marked with "Server" prefix)
 * - Uses IMOInteractionInterface on target actors
 * - Rate limiting via LastInteractTimeSeconds map (mutable for const methods)
 *
 * CRITICAL PATTERNS:
 * 1. Server Validation Flow:
 *    Client sends target actor -> Server validates distance, LOS, angle
 *    -> Server executes via IMOInteractionInterface if valid
 *
 * 2. Server Trace (FindServerInteractTarget):
 *    - Resolves controller viewpoint (spectator or pawn)
 *    - Sphere trace from eye position forward
 *    - Validates result against view cone and distance
 *
 * 3. View Cone Check:
 *    - MaxInteractAngleDegrees (default 75°) from view forward
 *    - Prevents interacting with objects behind player
 *    - Uses dot product for efficient angle check
 *
 * KNOWN PITFALLS:
 * 1. VIEWPOINT SPOOFING: Client could send fake viewpoint data.
 *    MaxViewpointDistanceFromPawn (250u) limits how far eye can be from pawn.
 *
 * 2. TRACE CHANNEL: InteractTraceChannel defaults to Visibility.
 *    Create custom "Interactable" channel for proper filtering.
 *
 * 3. RATE LIMIT BYPASS: LastInteractTimeSeconds uses FObjectKey.
 *    If controller is recreated, rate limit resets. Consider using
 *    player ID instead for persistent limiting.
 *
 * 4. LOS THROUGH GLASS: HasServerLineOfSight uses ValidationTraceChannel.
 *    Glass/transparent objects may need special handling.
 *
 * 5. MULTIPLAYER LATENCY: Client prediction of valid targets may
 *    differ from server validation. Consider feedback for failed interactions.
 *
 * RELATED FILES:
 * - MOInteractorComponent.h - Client-side interaction detection
 * - IMOInteractionInterface.h - Interface implemented by interactable actors
 * - MOPlayerController.h - Sends interaction requests to server
 * - MOHarvestSubsystem.h - Specialized harvest interaction handling
 *
 * TESTING CHECKLIST:
 * [ ] Interaction respects MaximumInteractDistance
 * [ ] LOS check blocks through walls (if bRequireLineOfSight)
 * [ ] View cone check prevents behind-back interaction
 * [ ] Rate limiting prevents spam clicks
 * [ ] Secondary interaction (RMB) works separately
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

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

	// Server-only secondary interaction entry point (right-click).
	UFUNCTION(BlueprintCallable, Category="MO|Interaction")
	bool ServerExecuteSecondaryInteract(AController* InteractorController, AActor* TargetActor);

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
