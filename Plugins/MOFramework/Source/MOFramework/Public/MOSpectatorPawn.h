/**
 * =============================================================================
 * MOSpectatorPawn.h - Lockable Spectator Camera Pawn
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Custom spectator pawn used when player is between possessions (possession
 * menu, death, etc.). Can be locked in place or given free movement.
 *
 * FEATURES:
 * - SetMovementLocked(): Lock/unlock camera movement
 * - SetViewAboveLocation(): Position camera looking down at a point
 * - Overrides MoveForward/Right/Up to respect lock state
 *
 * USAGE:
 * - Assign in GameMode's SpectatorClass property
 * - Controller possesses this when unpossessing gameplay pawn
 * - Lock movement during possession menu
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] DEFAULT LOCKED: Spawns with bMovementLocked=true. Remember to
 *   unlock if free-fly spectator mode is desired.
 *
 * [2024-02] GAMEMODE SETUP: Must be set as SpectatorClass in GameMode BP.
 *   Default SpectatorPawn has free movement.
 *
 * =============================================================================
 * RELATED FILES: MOPlayerController.h, MOPossessionSubsystem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "MOSpectatorPawn.generated.h"
UCLASS()
class MOFRAMEWORK_API AMOSpectatorPawn : public ASpectatorPawn
{
	GENERATED_BODY()

public:
	AMOSpectatorPawn();

	// Lock/unlock spectator movement
	UFUNCTION(BlueprintCallable, Category="MO|Spectator")
	void SetMovementLocked(bool bLocked);

	UFUNCTION(BlueprintPure, Category="MO|Spectator")
	bool IsMovementLocked() const { return bMovementLocked; }

	// Teleport to a location looking down
	UFUNCTION(BlueprintCallable, Category="MO|Spectator")
	void SetViewAboveLocation(FVector TargetLocation, float Height = 2000.0f, float PitchAngle = -60.0f);

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void MoveForward(float Val) override;
	virtual void MoveRight(float Val) override;
	virtual void MoveUp_World(float Val) override;
	virtual void TurnAtRate(float Rate) override;
	virtual void LookUpAtRate(float Rate) override;

private:
	/** Whether spectator movement is locked (camera stays fixed). */
	UPROPERTY()
	bool bMovementLocked = true;
};
