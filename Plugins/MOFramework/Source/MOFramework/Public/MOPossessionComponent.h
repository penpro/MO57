/**
 * =============================================================================
 * MOPossessionComponent.h - Pawn Spawning & Quick Possession
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Provides pawn spawning and quick possession utilities for the player controller.
 * Enables spawning actors/pawns near the controller and possessing nearby pawns.
 * This component handles CLIENT-initiated requests that are forwarded to SERVER.
 *
 * KEY RESPONSIBILITIES:
 * 1. Spawn actors near the player controller (general purpose)
 * 2. Spawn pawns and immediately possess them
 * 3. Find and possess the nearest possessable pawn
 * 4. Forward all spawn/possess operations to server via RPCs
 *
 * OWNERSHIP:
 * - Owner: AMOPlayerController (created as default subobject)
 * - Lifespan: Exists for duration of PlayerController
 *
 * RPC PATTERN:
 * All methods follow client -> server RPC pattern:
 * - TryPossessNearestPawn() -> ServerTryPossessNearestPawn()
 * - TrySpawnActorNearController() -> ServerSpawnActorNearController()
 * - TrySpawnAndPossessPawn() -> ServerSpawnAndPossessPawn()
 *
 * SPAWN LOCATION CALCULATION:
 * - SpawnDistance: Forward distance from controller location
 * - SpawnOffset: Additional offset (world space)
 * - bUseViewRotation: Use camera rotation for spawn position
 *
 * CRITICAL PATTERNS:
 * 1. Server Authority: All spawning happens on server only
 * 2. View Direction: Spawns typically use camera/view rotation
 * 3. Possession Check: TryPossessNearestPawn queries for nearest pawn
 *
 * KNOWN PITFALLS:
 * 1. SPAWN COLLISION: No collision checking - spawns may overlap geometry
 * 2. POSSESSION REQUIREMENTS: Target pawn must have UMOIdentityComponent
 * 3. NO RETURN VALUE ON RPC: Server RPCs don't return success status
 *
 * RELATED FILES:
 * - MOPlayerController.h - Owns this component
 * - MOPossessionSubsystem.h - Handles actual possession logic
 * - MOIdentityComponent.h - Required on possessable pawns
 *
 * TESTING CHECKLIST:
 * [ ] Spawn actor appears at correct distance/offset
 * [ ] Spawn pawn and possess works in single operation
 * [ ] TryPossessNearestPawn finds and possesses nearby pawn
 * [ ] All operations work correctly in multiplayer
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOPossessionComponent.generated.h"

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOPossessionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOPossessionComponent();

	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	bool TryPossessNearestPawn();

	// Call this from BP input: pass any Actor BP class to spawn.
	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	bool TrySpawnActorNearController(TSubclassOf<AActor> ActorClassToSpawn, float SpawnDistance = 300.0f, FVector SpawnOffset = FVector::ZeroVector, bool bUseViewRotation = true);

	/** Spawn a pawn and immediately possess it. */
	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	bool TrySpawnAndPossessPawn(TSubclassOf<APawn> PawnClassToSpawn, float SpawnDistance = 300.0f, FVector SpawnOffset = FVector::ZeroVector, bool bUseViewRotation = true);

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerTryPossessNearestPawn();

	UFUNCTION(Server, Reliable)
	void ServerSpawnActorNearController(TSubclassOf<AActor> ActorClassToSpawn, float SpawnDistance, FVector SpawnOffset, bool bUseViewRotation);

	UFUNCTION(Server, Reliable)
	void ServerSpawnAndPossessPawn(TSubclassOf<APawn> PawnClassToSpawn, float SpawnDistance, FVector SpawnOffset, bool bUseViewRotation);
};
