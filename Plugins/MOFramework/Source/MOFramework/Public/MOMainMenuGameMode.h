#pragma once

#include "CoreMinimal.h"
#include "MOGameMode.h"
#include "MOMainMenuGameMode.generated.h"

/**
 * Game mode for the main menu / loading screen level.
 *
 * Key differences from gameplay AMOGameMode:
 * - No default pawn (menu-only, no player character)
 * - Skips PCG tag registration (no harvesting in menu)
 * - Uses AMOMainMenuPlayerController for menu input
 */
UCLASS()
class MOFRAMEWORK_API AMOMainMenuGameMode : public AMOGameMode
{
	GENERATED_BODY()

public:
	AMOMainMenuGameMode();

protected:
	virtual void BeginPlay() override;

	/** Override to prevent spawn warnings when DefaultPawnClass is null. */
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
};
