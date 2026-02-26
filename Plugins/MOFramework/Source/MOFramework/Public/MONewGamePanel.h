/**
 * =============================================================================
 * MONewGamePanel.h - New Game Configuration Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Panel for configuring a new game - world name and seed. Part of the main
 * menu flow before starting a fresh world.
 *
 * FEATURES:
 * - World name input (used for save folder naming)
 * - Seed input (numeric or string, hashed if non-numeric)
 * - Random seed generation
 * - Start Game triggers level transition
 *
 * INTEGRATION:
 * - Appears in MainMenuWidget's focus window (index 1)
 * - Stores settings in UMOGameSettings pending values
 * - Voxel graphs can read seed via UMOGameSettings::GetWorldSeed()
 *
 * BLUEPRINT SETUP:
 * 1. Create WBP_NewGamePanel with parent UMONewGamePanel
 * 2. Add WorldNameInputBox (EditableTextBox)
 * 3. Add SeedInputBox (EditableTextBox)
 * 4. Add RandomSeedButton, StartGameButton (UMOCommonButton)
 * 5. Optional: BackButton, SeedPreviewText
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] SEED HASHING: GetCurrentSeed() parses text as int32. If not purely
 *   numeric, uses GetTypeHash(FString) for deterministic hash. Same text
 *   always produces same seed (e.g., "MyWorld" -> consistent int32).
 *
 * [2024-02] WORLD NAME VALIDATION: GetWorldName() returns raw input. Caller
 *   should sanitize for filesystem before creating save folder. Invalid chars:
 *   \ / : * ? " < > |
 *
 * [2024-02] FOCUS WINDOW INDEX: Panel must be index 1 in MainMenuWidget's
 *   FocusWindowSwitcher. Index 0=empty, 1=NewGame, 2=Load, 3=Options.
 *
 * [2024-02] ENTER KEY: NativeOnKeyDown() handles Enter to trigger Start Game.
 *   Equivalent to clicking StartGameButton. Escape triggers Back.
 *
 * [2024-02] DEFAULT WORLD NAME: GenerateDefaultWorldName() counts existing
 *   save folders via UMOGameSettings::GetSaveSlotCount(). Names are
 *   "World 1", "World 2", etc. based on count + 1.
 *
 * [2024-02] SETTINGS STORAGE: HandleStartGameClicked() stores seed and world
 *   name in UMOGameSettings before broadcasting OnStartGameRequested.
 *   Voxel graphs read via UMOGameSettings::GetWorldSeed().
 *
 * [2024-02] DELEGATE BINDING: NativeConstruct() binds button handlers with
 *   RemoveAll(this) pattern. Also calls GenerateRandomSeed() and
 *   GenerateDefaultWorldName() for initial values.
 *
 * [2024-02] REQUIRED WIDGETS: WorldNameInputBox, SeedInputBox, RandomSeedButton,
 *   StartGameButton are required (BindWidget). BackButton and SeedPreviewText
 *   are optional.
 *
 * =============================================================================
 * RELATED FILES: MOMainMenuWidget.h, MOGameSettings.h, MOGameInstance.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MONewGamePanel.generated.h"

class UMOCommonButton;
class UEditableTextBox;
class UTextBlock;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMONewGameStartSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMONewGameCloseSignature);

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMONewGamePanel : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UMONewGamePanel(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Called when Start Game button is clicked and seed is configured. */
	UPROPERTY(BlueprintAssignable, Category="MO|NewGame")
	FMONewGameStartSignature OnStartGameRequested;

	/** Called when panel requests to close (back/cancel). */
	UPROPERTY(BlueprintAssignable, Category="MO|NewGame")
	FMONewGameCloseSignature OnRequestClose;

	// ============================================================================
	// METHODS
	// ============================================================================

	/** Generate a new random seed and update UI. */
	UFUNCTION(BlueprintCallable, Category="MO|NewGame")
	void GenerateRandomSeed();

	/** Get the current seed value from the input field. */
	UFUNCTION(BlueprintPure, Category="MO|NewGame")
	int32 GetCurrentSeed() const;

	/** Set the seed input field. */
	UFUNCTION(BlueprintCallable, Category="MO|NewGame")
	void SetSeed(int32 Seed);

	/** Get the current world name from the input field. */
	UFUNCTION(BlueprintPure, Category="MO|NewGame")
	FString GetWorldName() const;

	/** Set the world name input field. */
	UFUNCTION(BlueprintCallable, Category="MO|NewGame")
	void SetWorldName(const FString& Name);

	/** Generate a default world name (e.g., "World 1", "World 2"). */
	UFUNCTION(BlueprintCallable, Category="MO|NewGame")
	void GenerateDefaultWorldName();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// ============================================================================
	// HANDLERS
	// ============================================================================

	UFUNCTION() void HandleRandomSeedClicked();
	UFUNCTION() void HandleStartGameClicked();
	UFUNCTION() void HandleBackClicked();
	UFUNCTION() void HandleSeedTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	// ============================================================================
	// BIND WIDGETS
	// ============================================================================

	/** Text input for world name. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableTextBox> WorldNameInputBox;

	/** Text input for seed value. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableTextBox> SeedInputBox;

	/** Random seed button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> RandomSeedButton;

	/** Start Game button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> StartGameButton;

	/** Back/Cancel button. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> BackButton;

	/** Optional: Display current seed as integer. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SeedPreviewText;
};
