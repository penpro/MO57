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
