// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MO57GameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AMO57GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AMO57GameMode();
};



