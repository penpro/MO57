#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MOGameInstance.generated.h"

class UTexture2D;

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
	// LOADING SCREEN
	// ============================================================================

	/** Background image for loading screen (optional). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|LoadingScreen")
	TSoftObjectPtr<UTexture2D> LoadingScreenBackground;

	/** Loading tip messages to display randomly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|LoadingScreen")
	TArray<FText> LoadingTips;

	/** Minimum time to show loading screen (seconds). Prevents flickering on fast loads. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|LoadingScreen")
	float MinLoadingScreenTime = 1.0f;

protected:
	/** Called before a map starts loading. */
	UFUNCTION()
	virtual void BeginLoadingScreen(const FString& MapName);

	/** Called after a map finishes loading. */
	UFUNCTION()
	virtual void EndLoadingScreen(UWorld* InLoadedWorld);

private:
	/** Create the Slate widget for loading screen. */
	TSharedRef<class SWidget> CreateLoadingScreenWidget();

	/** Track loading screen state. */
	bool bLoadingScreenShowing = false;
};
