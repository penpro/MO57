/**
 * =============================================================================
 * MOWindDirectionWidget.h - HUD wind direction indicator
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * Small HUD widget showing wind direction relative to the player's facing.
 * Paleolithic-appropriate: real hunters watched the wind for scent, sound,
 * and smoke. Players see the same information without a compass — only the
 * RELATIVE direction (wind in your face / wind at your back / from your left)
 * — not absolute N/S/E/W (no clocks, no compass in this era).
 *
 * BEHAVIOR:
 * - Reads UMOWeatherIntegrationSubsystem::GetCurrentWeatherState() for
 *   WindDirection (rotator).
 * - Reads player controller control rotation for "what direction am I facing."
 * - Sets WindArrow's render transform rotation to (Wind.Yaw - Player.Yaw).
 *   Arrow points in the direction the wind is BLOWING (so "wind in your face"
 *   = arrow pointing down at the player).
 * - Hidden when WindIntensity < HideBelowIntensity (default 0.1) — no useful
 *   info to show on calm days.
 *
 * USAGE (Blueprint):
 *   1. Create WBP_WindDirection based on this class.
 *   2. Add UImage named "WindArrow" (required BindWidget) — an arrow texture
 *      pointing UP at zero rotation, anchored to its center.
 *   3. Optional UTextBlock "WindLabel" — descriptive text like "Wind".
 *   4. Drop WBP_WindDirection into WBP_HUDRoot, name it "WindIndicator",
 *      tick "Is Variable".
 *
 * =============================================================================
 * RELATED FILES:
 *   - MOWeatherIntegrationSubsystem.h
 *   - MOHUDRootWidget.h
 * LAST UPDATED: 2026-05-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOWindDirectionWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS()
class MOFRAMEWORK_API UMOWindDirectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Force a refresh right now (cheats, debug, etc.). Normally invoked by tick timer. */
	UFUNCTION(BlueprintCallable, Category="MO|HUD|Wind")
	void RefreshFromWeather();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Required arrow image — designer authors an up-pointing arrow texture. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> WindArrow;

	/** Optional descriptive label. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> WindLabel;

	/**
	 * Hide the widget below this wind intensity (0–1). Default 0.1 = still air
	 * doesn't display arrows. Above this the widget renders normally.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|HUD|Wind",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float HideBelowIntensity = 0.10f;

	/**
	 * Refresh cadence (seconds). Weather doesn't change rapidly so 0.25s
	 * (~4Hz) is plenty; lower if the arrow visibly stutters when the player
	 * spins, higher to save tick budget.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|HUD|Wind",
		meta=(ClampMin="0.05", ClampMax="2.0"))
	float RefreshIntervalSeconds = 0.25f;

private:
	FTimerHandle RefreshTimerHandle;
};
