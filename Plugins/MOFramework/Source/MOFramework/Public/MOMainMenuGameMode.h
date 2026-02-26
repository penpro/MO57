/**
 * =============================================================================
 * MOMainMenuGameMode.h - Main Menu Level Game Mode
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Game mode for the main menu / loading screen level. Differs from gameplay
 * AMOGameMode in that it has no default pawn, skips PCG tag registration,
 * and uses AMOMainMenuPlayerController for menu-only input.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] NO PAWN: DefaultPawnClass is null intentionally. Override
 *   SpawnDefaultPawnFor to prevent spawn warnings.
 *
 * [2024-02] NO HARVESTING: Skips PCG tag registration since there's no
 *   gameplay in the menu level.
 *
 * =============================================================================
 * RELATED FILES: MOGameMode.h, MOMainMenuPlayerController.h, MOMainMenuWidget.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

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
