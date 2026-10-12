// Lets anything a pawn can stand on say what its surface sounds like (a wooden floor, a stone slab, a rug). Terrain does not implement it: the footstep component asks the world generator
// for the biome under the foot instead (FMOBiomeDefinitionRow::FootstepSet). The set name is the middle of the sound-bank ids "SFX.Footsteps.<Set>1" / "<Set>2".

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOFootstepSurfaceInterface.generated.h"

UINTERFACE(BlueprintType)
class MOFRAMEWORK_API UMOFootstepSurfaceInterface : public UInterface
{
	GENERATED_BODY()
};

class MOFRAMEWORK_API IMOFootstepSurfaceInterface
{
	GENERATED_BODY()

public:
	/** The footstep sound set for a pawn standing on this actor (e.g. "BareWood", "StoneStep"). NAME_None = no opinion: the default structure set is used. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Audio")
	FName GetFootstepSet() const;
};
