/**
 * =============================================================================
 * MOSaveDomainInterface.h - Save-domain registration (C2 god-class fix)
 * =============================================================================
 *
 * PURPOSE:
 * Extension-without-modification for persistence. A subsystem that owns save
 * data implements IMOSaveDomain and registers with UMOPersistenceSubsystem;
 * the persistence subsystem iterates registered domains and never learns
 * their names. Adding a new save domain touches TWO files: the new domain
 * itself and the UMOWorldSaveGame field it writes — NEVER the persistence
 * subsystem. (Same pattern as IMOMovementInterruptibleInterface: one shared
 * mechanism, register N times.)
 *
 * HOW TO ADD A DOMAIN:
 * 1. Add your FMOYourSaveData UPROPERTY to UMOWorldSaveGame (the schema).
 * 2. Implement IMOSaveDomain on the subsystem that OWNS the data:
 *      CaptureSaveDomain writes Save.YourData; ApplySaveDomain reads it.
 * 3. Register: world subsystems in OnWorldBeginPlay (never Initialize — the
 *    cook-commandlet trap, see MOWeatherIntegrationSubsystem), GameInstance
 *    subsystems in Initialize after Collection.InitializeDependency on the
 *    persistence subsystem. Unregister in Deinitialize.
 *
 * APPLY ORDER: ApplySaveDomain runs on load AFTER pawns/world-items/buildings
 * are respawned, sorted by GetSaveDomainApplyPriority (ascending). Existing
 * domains: VoxelSculpt=10, Quest=20, Weather=30, TerrainMod=40, Clock=50,
 * Colony=60, ResourceDepletion=70. Capture order is unspecified.
 *
 * =============================================================================
 * RELATED FILES: MOPersistenceSubsystem.h, MOworldSaveGame.h
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOSaveDomainInterface.generated.h"

class UMOWorldSaveGame;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UMOSaveDomain : public UInterface
{
	GENERATED_BODY()
};

class MOFRAMEWORK_API IMOSaveDomain
{
	GENERATED_BODY()

public:
	/** Stable id for logs and diagnostics. */
	virtual FName GetSaveDomainName() const = 0;

	/** Lower applies earlier on load. Capture order is unspecified. */
	virtual int32 GetSaveDomainApplyPriority() const { return 100; }

	/** Write this domain's state into its UMOWorldSaveGame field(s). */
	virtual void CaptureSaveDomain(UMOWorldSaveGame& Save) = 0;

	/** Restore this domain's state from the loaded save (authority only —
	 *  the persistence subsystem only calls this on the server). */
	virtual void ApplySaveDomain(const UMOWorldSaveGame& Save) = 0;
};
