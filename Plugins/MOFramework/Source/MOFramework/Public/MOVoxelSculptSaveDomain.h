/**
 * =============================================================================
 * MOVoxelSculptSaveDomain.h - Voxel sculpt persistence domain (C2 fix)
 * =============================================================================
 *
 * PURPOSE:
 * Owns saving/restoring voxel sculpt modifications (height + volume actors,
 * via the MOVoxel facade). This logic previously lived inside
 * UMOPersistenceSubsystem; it has no natural owner among the gameplay
 * subsystems (it iterates world actors, not subsystem state), so it gets a
 * dedicated, single-purpose world subsystem registered as an IMOSaveDomain.
 *
 * Applies FIRST on load (priority 10): terrain must be sculpted before
 * anything placed on it restores.
 *
 * =============================================================================
 * RELATED FILES: MOSaveDomainInterface.h, MOVoxelAlias.h, MOworldSaveGame.h
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOSaveDomainInterface.h"
#include "MOVoxelSculptSaveDomain.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOVoxelSculptSaveDomain : public UWorldSubsystem, public IMOSaveDomain
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	//~ Begin IMOSaveDomain
	virtual FName GetSaveDomainName() const override { return TEXT("VoxelSculpt"); }
	virtual int32 GetSaveDomainApplyPriority() const override { return 10; }
	virtual void CaptureSaveDomain(UMOWorldSaveGame& Save) override;
	virtual void ApplySaveDomain(const UMOWorldSaveGame& Save) override;
	//~ End IMOSaveDomain
};
