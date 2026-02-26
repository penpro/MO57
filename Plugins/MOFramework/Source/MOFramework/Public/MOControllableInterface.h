/**
 * =============================================================================
 * MOControllableInterface.h - Controller-to-Pawn Input Delegation
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Interface for pawns that can receive input from AMOPlayerController.
 * Provides standardized delegation allowing the controller to work with
 * any pawn type (characters, vehicles, turrets) without hard coupling.
 *
 * KEY RESPONSIBILITIES:
 * 1. Define standardized input methods for movement
 * 2. Define standardized input methods for actions
 * 3. Define standardized input methods for terraforming
 * 4. Provide capability queries (CanMove, CanSprint, etc.)
 *
 * ARCHITECTURE NOTES:
 * - All methods are BlueprintNativeEvent (C++ overridable + BP callable)
 * - Controller calls interface methods, pawn implements behavior
 * - Pawn capabilities checked before applying input
 * - Supports both player and AI control patterns
 *
 * INPUT DELEGATION FLOW:
 * Player input -> AMOPlayerController::HandleMove()
 * -> Get possessed pawn -> Cast to IMOControllableInterface
 * -> Call RequestMove(MovementVector)
 * -> Pawn::RequestMove_Implementation() applies movement
 *
 * METHOD CATEGORIES:
 * - Movement: RequestMove, RequestLook, RequestJump*, RequestSprint*, etc.
 * - Actions: RequestInteract, RequestPrimaryAction, RequestSecondaryAction
 * - Terraforming: RequestTerraformToggle, RequestTerraformCycleTool
 * - Queries: CanBeControlled, CanMove, CanJump, CanSprint
 *
 * CRITICAL PATTERNS:
 * 1. Continuous Input (Move, Look):
 *    Called every frame while input active
 *    Values are normalized -1 to 1
 *
 * 2. Press/Release Input (Jump, Sprint, Actions):
 *    Separate Start/End methods for press and release
 *    Pawn tracks state internally
 *
 * 3. Toggle Input (Jog, Crouch, Terraform):
 *    Single method that toggles state
 *    Pawn maintains toggle state
 *
 * KNOWN PITFALLS:
 * 1. QUERY BEFORE ACTION: Always check CanMove() before RequestMove().
 *    Controller is responsible for capability checking.
 *
 * 2. IMPLEMENTATION REQUIRED: _Implementation suffix required on
 *    implementing class methods (BlueprintNativeEvent pattern).
 *
 * 3. INTERFACE CAST: Use TScriptInterface<IMOControllableInterface> or
 *    Cast<IMOControllableInterface>(Pawn) for safe access.
 *
 * RELATED FILES:
 * - MOPlayerController.h - Calls interface methods for input
 * - MOCharacter.h - Primary implementer of this interface
 * - MOVehicle.h - Vehicle implementation (if applicable)
 *
 * TESTING CHECKLIST:
 * [ ] All movement inputs delegate to pawn correctly
 * [ ] All action inputs delegate to pawn correctly
 * [ ] Capability queries return correct values
 * [ ] Non-controllable pawn rejects input gracefully
 * [ ] AI controller can also call interface methods
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOControllableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UMOControllableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for pawns that can be controlled by AMOPlayerController.
 * Provides a standardized way for the controller to delegate input to any pawn type.
 */
class MOFRAMEWORK_API IMOControllableInterface
{
	GENERATED_BODY()

public:
	// ============================================================================
	// MOVEMENT
	// ============================================================================

	/**
	 * Request movement input. Called every frame while movement input is active.
	 * @param MovementInput - 2D movement vector (X = Right, Y = Forward) normalized -1 to 1
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Movement")
	void RequestMove(FVector2D MovementInput);

	/**
	 * Request look/aim input. Called every frame while look input is active.
	 * @param LookInput - 2D look vector (X = Yaw, Y = Pitch)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Movement")
	void RequestLook(FVector2D LookInput);

	/**
	 * Request jump start.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Movement")
	void RequestJumpStart();

	/**
	 * Request jump end (for variable height jumps).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Movement")
	void RequestJumpEnd();

	/**
	 * Request sprint start.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Movement")
	void RequestSprintStart();

	/**
	 * Request sprint end.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Movement")
	void RequestSprintEnd();

	/**
	 * Request toggle jog mode (tap hustle).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Movement")
	void RequestToggleJog();

	/**
	 * Request crouch toggle.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Movement")
	void RequestCrouchToggle();

	// ============================================================================
	// ACTIONS
	// ============================================================================

	/**
	 * Request interaction with nearby interactable.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Actions")
	void RequestInteract();

	/**
	 * Request secondary interaction with nearby interactable (right-click context menu, etc.).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Actions")
	void RequestSecondaryInteract();

	/**
	 * Request primary action (attack, use tool, etc.).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Actions")
	void RequestPrimaryAction();

	/**
	 * Request primary action release.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Actions")
	void RequestPrimaryActionRelease();

	/**
	 * Request secondary action (block, aim, alt-use, etc.).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Actions")
	void RequestSecondaryAction();

	/**
	 * Request secondary action release.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Actions")
	void RequestSecondaryActionRelease();

	// ============================================================================
	// TERRAFORMING
	// ============================================================================

	/**
	 * Request toggle terraforming mode.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Terraforming")
	void RequestTerraformToggle();

	/**
	 * Request cycle terraforming tool (Dig, Raise, Flatten, Smooth).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Terraforming")
	void RequestTerraformCycleTool();

	// ============================================================================
	// QUERIES
	// ============================================================================

	/**
	 * Check if this pawn can currently be controlled.
	 * @return True if the pawn accepts control input
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Query")
	bool CanBeControlled() const;

	/**
	 * Check if this pawn can currently move.
	 * @return True if the pawn can move
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Query")
	bool CanMove() const;

	/**
	 * Check if this pawn can currently jump.
	 * @return True if the pawn can jump
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Query")
	bool CanJump() const;

	/**
	 * Check if this pawn can currently sprint.
	 * @return True if the pawn can sprint
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Controllable|Query")
	bool CanSprint() const;
};
