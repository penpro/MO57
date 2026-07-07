/**
 * =============================================================================
 * MOInteractionInterface.h - Interactor Contract
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Contract for objects that can initiate interactions (typically PlayerController).
 * Provides viewpoint for traces and client-to-server interaction forwarding.
 *
 * WHO IMPLEMENTS THIS:
 * - AMOPlayerController - Primary implementer
 *
 * WHO USES THIS:
 * - UMOInteractionSubsystem - Queries viewpoint for trace origins
 * - UMOInteractorComponent - Forwards interaction requests
 *
 * =============================================================================
 * IMPLEMENTATION REQUIREMENTS
 * =============================================================================
 *
 * 1. GetInteractionViewpoint: Return camera/eye position and direction.
 *    - For players: Use GetPlayerViewPoint()
 *    - Return false if viewpoint unavailable
 *
 * 2. RequestServerInteract: Forward interaction to server.
 *    - Validate target locally before sending RPC
 *    - Return true if request was accepted for processing
 *    - Server will validate again on receipt
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-01] VIEWPOINT SOURCE: Use MOViewpointUtils for consistent behavior.
 *   Don't implement viewpoint resolution from scratch.
 *
 * [2024-02] RPC VALIDATION: RequestServerInteract returns true if the request
 *   was SENT, not if the interaction SUCCEEDED. Server-side validation may
 *   still reject the interaction.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOInteractableComponent.h - The "target" side of interactions
 * - MOInteractionSubsystem.h - Orchestrates interaction system
 * - MOPlayerController.h - Primary implementer
 * - MOViewpointUtils.h - Use for viewpoint resolution
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOInteractionInterface.generated.h"

UINTERFACE(BlueprintType)
class MOFRAMEWORKCORE_API UMOInteractionInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for interactors (typically PlayerController).
 * See file header for implementation requirements and pitfalls.
 */
class MOFRAMEWORKCORE_API IMOInteractionInterface
{
	GENERATED_BODY()

public:
	/**
	 * Provide the world-space viewpoint used for interaction traces.
	 * Return true if OutWorldLocation/OutWorldRotation are valid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Interaction")
	bool GetInteractionViewpoint(FVector& OutWorldLocation, FRotator& OutWorldRotation) const;

	/**
	 * Client-side entry point. Implementors typically forward to a Server RPC.
	 * Return true if the request was accepted for processing.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Interaction")
	bool RequestServerInteract(AActor* TargetActor);
};
