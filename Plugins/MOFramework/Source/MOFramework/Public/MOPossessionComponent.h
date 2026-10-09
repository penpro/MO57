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
#include "MOPossessionTypes.h"
#include "MOPossessionComponent.generated.h"

class UMOPossessionSubsystem;

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

	// ------------------------------------------------------------------------
	// POSSESSION MENU (works for EVERY player)
	// APlayerController::Possess() is authority-only: on a remote client it silently does nothing. The menu therefore
	// never possesses directly -- it asks through here, which resolves locally on the host and via an RPC on a client.
	// ------------------------------------------------------------------------

	/** Take over the pawn with this GUID. Returns false if the request could not even be made. */
	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	bool RequestPossessPawn(const FGuid& PawnGuid);

	/** Create a new character for this player (only valid while they have none). */
	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	bool RequestCreateCharacter();

	/** Ask for the rows of the possession menu. The answer arrives on OnPossessionListReady (at once on the host,
	 *  after a round trip on a client -- built from the SERVER's pawns, not the client's own saves). */
	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	void RequestPossessionList();

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOPossessionListReadySignature, const TArray<FMOPossessionListEntry>&, Entries);
	UPROPERTY(BlueprintAssignable, Category="MO|Possession")
	FMOPossessionListReadySignature OnPossessionListReady;

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerTryPossessNearestPawn();

	UFUNCTION(Server, Reliable)
	void ServerSpawnActorNearController(TSubclassOf<AActor> ActorClassToSpawn, float SpawnDistance, FVector SpawnOffset, bool bUseViewRotation);

	UFUNCTION(Server, Reliable)
	void ServerSpawnAndPossessPawn(TSubclassOf<APawn> PawnClassToSpawn, float SpawnDistance, FVector SpawnOffset, bool bUseViewRotation);

public:
	// ------------------------------------------------------------------------
	// The RPCs for the menu requests live on AMOPlayerController, the connection-owning actor, and forward into these
	// authority-side bodies; this component is the API the menu calls. (TESTING: never fire these from editor-Python.
	// Under the editor script guard AActor::GetFunctionCallspace maps every RPC to LOCAL, so the "server" request just
	// runs on the client. Use MO.Possess.List / MO.Possess.Take, which defer a tick, or the real menu.)
	// ------------------------------------------------------------------------

	/** Authority only: the server-side bodies behind the controller's RPCs and the host's direct path. */
	void ApplyPossessByGuid(const FGuid& PawnGuid);
	void ApplyCreateCharacter();
	void BuildList(TArray<FMOPossessionListEntry>& Out) const;

	/** The controller received the server's list: hand it to whoever is listening (the possession menu). */
	void NotifyPossessionListReceived(const TArray<FMOPossessionListEntry>& Entries);

private:
	UMOPossessionSubsystem* GetAuthoritySubsystem(APlayerController*& OutController) const;
};
