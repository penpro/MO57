#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MOGameInstance.generated.h"

/**
 * Game instance for MO57.
 * Handles one-time initialization when the game first launches.
 */
UCLASS()
class MOFRAMEWORK_API UMOGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/** Called when the game instance is created. */
	virtual void Init() override;
};
