// The carcasses lying in a world, in its save (Save.CarcassData). A carcass is its own actor with no pawn record, so until Nov 2026 a kill left lying, and its butchering, were gone at a load.
// A subsystem of its own, as with MOVoxelSculptSaveDomain: it iterates world actors and has no other state. Applies after the rest of the world (priority 95).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOSaveDomainInterface.h"
#include "MOCarcassSaveDomain.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOCarcassSaveDomain : public UWorldSubsystem, public IMOSaveDomain
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	//~ Begin IMOSaveDomain
	virtual FName GetSaveDomainName() const override { return TEXT("Carcasses"); }
	virtual int32 GetSaveDomainApplyPriority() const override { return 95; }
	virtual void CaptureSaveDomain(UMOWorldSaveGame& Save) override;
	virtual void ApplySaveDomain(const UMOWorldSaveGame& Save) override;
	//~ End IMOSaveDomain
};
