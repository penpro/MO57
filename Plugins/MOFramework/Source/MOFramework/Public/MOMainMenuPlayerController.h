#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MOMainMenuPlayerController.generated.h"

class UMOMainMenuWidget;
class UMOIntroWidget;
class UMediaSource;
class UMediaPlayer;
class UMediaTexture;
class UMediaSoundComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * Player controller for the main menu level.
 *
 * Responsibilities:
 * - Show intro video on first launch (if bPlayIntro = true)
 * - Display and manage main menu widget
 * - Handle menu input (Escape to skip intro, etc.)
 * - Trigger level transitions for New Game / Load Game
 *
 * Does NOT create gameplay components (inventory, possession, etc.)
 * since there's no pawn or gameplay in the menu level.
 */
UCLASS()
class MOFRAMEWORK_API AMOMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMOMainMenuPlayerController();

	// ============================================================================
	// WIDGET CLASSES
	// ============================================================================

	/** Main menu widget class to spawn. */
	UPROPERTY(EditDefaultsOnly, Category="MO|MainMenu")
	TSubclassOf<UMOMainMenuWidget> MainMenuWidgetClass;

	/** Intro video widget class to spawn. */
	UPROPERTY(EditDefaultsOnly, Category="MO|MainMenu")
	TSubclassOf<UMOIntroWidget> IntroWidgetClass;

	/** Media source for intro video (optional - if not set, uses IntroVideoFileName). */
	UPROPERTY(EditDefaultsOnly, Category="MO|MainMenu")
	TObjectPtr<UMediaSource> IntroVideoSource;

	/**
	 * Video filename relative to Content/Penumbra/Movies/ (e.g., "introfinal.mp4").
	 * Used if IntroVideoSource is not set. This creates the media source at runtime
	 * with the correct path for both editor and packaged builds.
	 */
	UPROPERTY(EditDefaultsOnly, Category="MO|MainMenu")
	FString IntroVideoFileName = TEXT("introfinal.mp4");

	/** Material to use for video display (must have a TextureParameter named "MediaTexture"). */
	UPROPERTY(EditDefaultsOnly, Category="MO|MainMenu")
	TObjectPtr<UMaterialInterface> VideoMaterial;

	// ============================================================================
	// LEVEL PATHS
	// ============================================================================

	/** Path to the gameplay level. */
	UPROPERTY(EditDefaultsOnly, Category="MO|MainMenu")
	FString GameplayLevelPath = TEXT("/Game/VoxelExamples/PCGScattering/MOPCGScattering");

	// ============================================================================
	// MAIN MENU ACTIONS
	// ============================================================================

	/** Show the main menu widget. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void ShowMainMenu();

	/** Hide the main menu widget. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void HideMainMenu();

	/** Play the intro video. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void PlayIntroVideo();

	/** Skip the intro video immediately. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void SkipIntroVideo();

	/** Start a new game - creates save slot and loads gameplay level. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void StartNewGame();

	/** Load an existing game - loads the specified save and gameplay level. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void LoadGame(const FString& SlotName);

	/** Exit the game. */
	UFUNCTION(BlueprintCallable, Category="MO|MainMenu")
	void ExitGame();

	/** Check if intro video is currently playing. */
	UFUNCTION(BlueprintPure, Category="MO|MainMenu")
	bool IsIntroPlaying() const { return bIntroPlaying; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// ============================================================================
	// INPUT HANDLERS
	// ============================================================================

	/** Handle any key to skip intro. */
	void HandleAnyKeyPressed();

	// ============================================================================
	// CALLBACKS
	// ============================================================================

	/** Called when intro video completes (or is skipped). */
	UFUNCTION()
	void HandleIntroComplete();

	/** Called when New Game is requested from menu. */
	UFUNCTION()
	void HandleNewGameRequested();

	/** Called when Load Game is requested from menu. */
	UFUNCTION()
	void HandleLoadGameRequested(const FString& SlotName);

	/** Called when Exit Game is requested from menu. */
	UFUNCTION()
	void HandleExitGameRequested();

	// ============================================================================
	// MEDIA CALLBACKS
	// ============================================================================

	/** Called when media playback ends. */
	UFUNCTION()
	void HandleMediaEndReached();

	/** Called when media opens successfully. */
	UFUNCTION()
	void HandleMediaOpened(FString OpenedUrl);

	/** Called when media fails to open. */
	UFUNCTION()
	void HandleMediaOpenFailed(FString FailedUrl);

	/** Setup media player and related components. */
	void SetupMediaPlayer();

	/** Cleanup media player and related components. */
	void CleanupMediaPlayer();

	/** Get or create the media source for the intro video. */
	UMediaSource* GetOrCreateIntroMediaSource();

private:
	// ============================================================================
	// WIDGETS
	// ============================================================================

	UPROPERTY()
	TObjectPtr<UMOMainMenuWidget> MainMenuWidget;

	UPROPERTY()
	TObjectPtr<UMOIntroWidget> IntroWidget;

	// ============================================================================
	// MEDIA COMPONENTS
	// ============================================================================

	UPROPERTY()
	TObjectPtr<UMediaPlayer> MediaPlayer;

	UPROPERTY()
	TObjectPtr<UMediaTexture> MediaTexture;

	UPROPERTY()
	TObjectPtr<UMediaSoundComponent> MediaSoundComponent;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> VideoMaterialInstance;

	/** Runtime-created media source (when using IntroVideoFileName). */
	UPROPERTY()
	TObjectPtr<UMediaSource> RuntimeMediaSource;

	// ============================================================================
	// STATE
	// ============================================================================

	/** Whether intro video is currently playing. */
	bool bIntroPlaying = false;

	/** Timer handle for video fallback timeout. */
	FTimerHandle VideoFallbackTimerHandle;

	/** Timer handle for delayed level load (lets loading screen render first). */
	FTimerHandle DelayedLevelLoadTimerHandle;

	/** Level path pending load (used with delayed load). */
	FString PendingLevelPath;

	/** Execute the delayed level load. */
	void ExecuteDelayedLevelLoad();
};
