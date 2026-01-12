#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "MOWorldSaveGame.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOWorldSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSet<FGuid> DestroyedGuids;
};
