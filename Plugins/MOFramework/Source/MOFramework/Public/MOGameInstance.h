#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MOGameInstance.generated.h"

class UMOLoadingOverlay;

/**
 * Game instance for MO57.
 * Handles one-time initialization and loading screens.
 */
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
