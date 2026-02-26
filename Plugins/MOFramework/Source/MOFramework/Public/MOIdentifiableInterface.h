/**
 * =============================================================================
 * MOIdentifiableInterface.h - Persistent Identity Contract
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Contract for actors with GUID-based persistent identity. The persistence
 * system uses this to save/load actors across sessions without hard class
 * dependencies or FindComponentByClass calls.
 *
 * WHO IMPLEMENTS THIS:
 * - AMOCharacter - Players and NPCs
 * - AMOBuildableActor - Constructed buildings
 * - AMOWorldItem - Dropped items
 * - AMOContainerActor - Storage containers
 *
 * WHO USES THIS:
 * - UMOPersistenceSubsystem - Save/load actor state
 * - UMOIdentityRegistrySubsystem - Track GUID→Actor mapping
 * - UMOInventoryComponent - Reference items by GUID
 *
 * GUID LIFECYCLE:
 * 1. Created: BeginPlay (new actor) or ApplySaveData (loaded actor)
 * 2. Registered: UMOIdentityRegistrySubsystem receives OnIdentityInitialized
 * 3. Persisted: UMOPersistenceSubsystem captures on save
 * 4. Destroyed: UMOIdentityRegistrySubsystem tracks for respawn prevention
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-01] GUID TIMING: GetPersistentGuid() may return invalid GUID if called
 *   before BeginPlay. Check HasValidIdentity() first.
 *
 * [2024-02] DESTROYED TRACKING: When actor is destroyed, its GUID is tracked
 *   in MOPersistenceSubsystem to prevent respawning. This is intentional for
 *   items but NOT for pawns (which should respawn from records).
 *
 * [2024-02] DUPLICATE GUIDS: Never manually assign GUIDs. Let the identity
 *   component generate them or load them from save data. Duplicate GUIDs
 *   cause registry conflicts.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOIdentityComponent.h - The actual GUID storage component
 * - MOIdentityRegistrySubsystem.h - GUID→Actor lookup
 * - MOPersistenceSubsystem.h - Uses this for save/load
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOIdentifiableInterface.generated.h"

class UMOIdentityComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class UMOIdentifiableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for actors with persistent GUID identity.
 * See file header for implementation requirements and pitfalls.
 */
class MOFRAMEWORK_API IMOIdentifiableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get the identity component for this actor.
	 * @return The identity component, or nullptr if not available
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Identity")
	UMOIdentityComponent* GetIdentityComponent() const;

	/**
	 * Get the persistent GUID for this actor.
	 * @return Valid GUID if identifiable, invalid GUID otherwise
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Identity")
	FGuid GetPersistentGuid() const;

	/**
	 * Check if this actor has a valid persistent identity.
	 * @return True if the actor has a valid GUID
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Identity")
	bool HasValidIdentity() const;
};
