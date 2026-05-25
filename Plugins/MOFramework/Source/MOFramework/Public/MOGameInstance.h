/**
 * =============================================================================
 * MOGameInstance.h - Game Instance (Session-Persistent State)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Core game instance handling session-wide state and loading screens.
 * Persists across level transitions. Owns GameInstance subsystems like
 * MOPersistenceSubsystem and MOMedicalSubsystem.
 *
 * FEATURES:
 * - ShowLoadingOverlay(): Shows loading screen for transitions
 * - DismissLoadingScreen(): Fades out after gameplay ready
 * - LoadingTips: Random tips displayed during loading
 *
 * LOADING SCREEN FLOW:
 * 1. Call ShowLoadingOverlay() before OpenLevel
 * 2. BeginLoadingScreen() triggered automatically
 * 3. EndLoadingScreen() called when map loads
 * 4. Call DismissLoadingScreen() when pawn is ready
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] LOADING OVERLAY CLASS: Must set LoadingOverlayClass in
 *   BP_MOGameInstance defaults. Null class = no loading screen.
 *
 * [2024-02] MANUAL DISMISS: Gameplay transitions use bWaitingForManualDismiss.
 *   Must call DismissLoadingScreen() or screen stays up forever.
 *
 * [2024-02] MAP FILTERING: ShouldShowLoadingScreen() checks map name.
 *   Some transitions (e.g., to main menu) may skip loading screen.
 *
 * =============================================================================
 * RELATED FILES: MOLoadingOverlay.h, MOPersistenceSubsystem.h, MOGameSettings.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MOGameInstance.generated.h"

class UMOLoadingOverlay;
UCLASS()
class MOFRAMEWORK_API UMOGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/** Called when the game instance is created. */
	virtual void Init() override;

	/** Called when the game instance is shut down. */
	virtual void Shutdown() override;

	// ============================================================================
	// LOADING OVERLAY
	// ============================================================================

	/** Widget class to use for the loading overlay. Set in BP_MOGameInstance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|LoadingScreen")
	TSubclassOf<UMOLoadingOverlay> LoadingOverlayClass;

	/** Loading tip messages to display randomly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|LoadingScreen")
	TArray<FText> LoadingTips;

	/**
	 * Fade-out duration for the loading screen after the player pawn lands.
	 * Set explicitly in code so the value doesn't depend on whatever the
	 * WBP_LoadingOverlay default ends up being. 1.0s = deliberate hand-off.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|LoadingScreen",
		meta=(ClampMin="0.0", ClampMax="5.0"))
	float LoadingScreenFadeOutSeconds = 1.0f;

	/**
	 * Debug toggle — when true, the black loading overlay is suppressed
	 * entirely. The pawn-spawn + voxel-wait sequence runs as normal but the
	 * player sees the world the whole time. Useful for debugging "why is
	 * the pawn not landing" scenarios visually.
	 *
	 * Toggle via console: `MO.Loading.Skip 1` (or 0 to re-enable).
	 * Not persisted — defaults off each session.
	 */
	UPROPERTY(BlueprintReadWrite, Category="MO|LoadingScreen|Debug")
	bool bSkipLoadingOverlay = false;

	/**
	 * Show the loading overlay immediately.
	 * Call this before starting a level transition.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|LoadingScreen")
	void ShowLoadingOverlay();

	/**
	 * Dismiss the loading overlay with a fade out.
	 * Call this when pawn has landed safely after a gameplay transition.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|LoadingScreen")
	void DismissLoadingScreen();

	/** Check if loading overlay is currently visible. */
	UFUNCTION(BlueprintPure, Category="MO|LoadingScreen")
	bool IsLoadingOverlayVisible() const;

protected:
	/** Called before a map starts loading. */
	UFUNCTION()
	virtual void BeginLoadingScreen(const FString& MapName);

	/** Called after a map finishes loading. */
	UFUNCTION()
	virtual void EndLoadingScreen(UWorld* InLoadedWorld);

private:
	/** Check if loading screen should be shown for this transition. */
	bool ShouldShowLoadingScreen(const FString& MapName) const;

	/** The active loading overlay widget. */
	UPROPERTY()
	TObjectPtr<UMOLoadingOverlay> LoadingOverlayWidget;

	/** Whether we're waiting for manual dismissal (gameplay transitions). */
	bool bWaitingForManualDismiss = false;
};
