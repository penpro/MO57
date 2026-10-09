/**
 * =============================================================================
 * MOPossessionSubsystem.h - Pawn Possession Management
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * WorldSubsystem managing pawn possession mechanics. Handles finding nearby
 * unpossessed pawns, spawning pawns near player, and possession validation
 * (distance, line-of-sight).
 *
 * KEY RESPONSIBILITIES:
 * 1. Find nearest unpossessed pawn within MaximumPossessDistance
 * 2. Validate possession requirements (distance, LOS)
 * 3. Spawn new actors/pawns near player controller
 * 4. Immediately possess newly spawned pawns
 *
 * ARCHITECTURE NOTES:
 * - WorldSubsystem: One per UWorld
 * - Server-only operations (marked with "Server" prefix)
 * - Uses MOViewpointUtils for consistent viewpoint resolution
 * - Works with AMOPlayerController's possession menu
 *
 * CRITICAL PATTERNS:
 * 1. FindNearestUnpossessedPawn:
 *    - Iterates all APawn in world
 *    - Filters by distance, possession state, line-of-sight
 *    - Returns closest valid pawn or nullptr
 *
 * 2. ServerSpawnAndPossessPawn:
 *    - Calculate spawn location from controller viewpoint
 *    - SpawnActorDeferred -> Set transforms -> FinishSpawning
 *    - Controller->Possess(NewPawn)
 *
 * 3. Viewpoint Resolution:
 *    - Spectator: Camera location/rotation
 *    - Possessed: Pawn's GetActorEyesViewPoint
 *    - Uses MOViewpointUtils for consistent handling
 *
 * KNOWN PITFALLS:
 * 1. COMPONENT REQUIREMENTS: This subsystem requires possessed pawns to
 *    have UMOIdentityComponent and UMOInventoryComponent. Pawns without
 *    these won't integrate with save/possession systems.
 *
 * 2. LOS CHECK CHANNEL: LineOfSightTraceChannel defaults to ECC_Visibility.
 *    May need adjustment if custom collision channels block visibility.
 *
 * 3. SPAWN LOCATION: SpawnDistance from eye location. Check for valid
 *    navmesh/ground at spawn point to avoid spawning in air/walls.
 *
 * 4. AUTHORITY: All Server* functions are server-only. Client calls will
 *    have no effect in multiplayer.
 *
 * RELATED FILES:
 * - MOPlayerController.h - UI-facing possession controls
 * - MOPossessionComponent.h - Per-controller possession state
 * - MOPersistenceSubsystem.h - SpawnPawnFromRecord uses this
 * - MOViewpointUtils.h - Viewpoint resolution helpers
 *
 * TESTING CHECKLIST:
 * [ ] Find nearest pawn respects MaximumPossessDistance
 * [ ] LOS check blocks possession through walls
 * [ ] SpawnAndPossess creates pawn at correct location
 * [ ] SpawnAndPossess immediately possesses new pawn
 * [ ] Works from both spectator and possessed states
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOPossessionTypes.h"
#include "MOPossessionSubsystem.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOPossessionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Possession")
	float MaximumPossessDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Possession")
	bool bRequireLineOfSight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Possession")
	TEnumAsByte<ECollisionChannel> LineOfSightTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Possession")
	bool bAllowSwitchPossession = true;

	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	bool ServerPossessNearestPawn(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	APawn* FindNearestUnpossessedPawn(APlayerController* PlayerController) const;

	// Spawn any Actor BP near controller viewpoint (server only).
	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	AActor* ServerSpawnActorNearController(APlayerController* PlayerController, TSubclassOf<AActor> ActorClassToSpawn, float SpawnDistance = 300.0f, FVector SpawnOffset = FVector::ZeroVector, bool bUseViewRotation = true);

	/** Spawn a pawn and immediately possess it (server only). */
	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	APawn* ServerSpawnAndPossessPawn(APlayerController* PlayerController, TSubclassOf<APawn> PawnClassToSpawn, float SpawnDistance = 300.0f, FVector SpawnOffset = FVector::ZeroVector, bool bUseViewRotation = true);

	// ------------------------------------------------------------------------
	// WHO MAY TAKE WHICH PAWN (authority only)
	// One definition, used by the possession menu list, possess-by-GUID and join-time pawn reuse, so the three can
	// never disagree about what is "available".
	// ------------------------------------------------------------------------

	/** A pawn `Requester` may take over: alive, identified, recruited (possessable), and not currently driven by a
	 *  DIFFERENT human. An idle colonist run by AI, an uncontrolled pawn, or the requester's own pawn all qualify. */
	bool IsPawnAvailableTo(const APawn* Pawn, const AController* Requester) const;

	/** Pick an existing pawn for a player who just joined: their previous pawn if it is still available, else the
	 *  available pawn nearest `AnchorLocation`. nullptr when every colonist is taken (the caller then spawns one). */
	APawn* FindAvailablePawnForJoiner(const AController* Joiner, const FGuid& PreferredPawnGuid, const FVector& AnchorLocation) const;

	/** Possession-menu rows for `Requester`: saved records + live world pawns, never a pawn another human is driving. */
	void BuildPossessionEntries(const APlayerController* Requester, TArray<FMOPossessionListEntry>& OutEntries) const;

	/** Possess the pawn with this GUID for `PlayerController`. Resolves it in the world, else respawns it from its
	 *  save record; refuses pawns the player may not take. Returns the pawn, or nullptr. */
	APawn* ServerPossessPawnByGuid(APlayerController* PlayerController, const FGuid& PawnGuid);

	/** Create a new character for a player that has none (possession menu "Create Character"). Returns it, or nullptr. */
	APawn* ServerCreateCharacter(APlayerController* PlayerController);

private:
	bool ResolveViewpoint(APlayerController* PlayerController, FVector& OutViewLocation, FRotator& OutViewRotation) const;
	bool HasLineOfSight(UWorld* World, const FVector& ViewLocation, const APawn* TargetPawn) const;
};
