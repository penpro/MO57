#pragma once

#include "CoreMinimal.h"
#include "CommonGameViewportClient.h"
#include "MOGameViewportClient.generated.h"

/**
 * Custom game viewport client for MO57.
 * Inherits from UCommonGameViewportClient to enable proper CommonUI input routing.
 */
UCLASS()
class MOFRAMEWORK_API UMOGameViewportClient : public UCommonGameViewportClient
{
	GENERATED_BODY()

public:
	UMOGameViewportClient();
};
