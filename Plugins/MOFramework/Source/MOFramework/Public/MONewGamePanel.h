#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MONewGamePanel.generated.h"

class UMOCommonButton;
class UEditableTextBox;
class UTextBlock;

/**
 * New Game panel for configuring world name and seed before starting a new game.
 *
 * Features:
 * - World name input (used for save file naming)
 * - Seed text input (numeric or string, hashed if non-numeric)
 * - Random seed button
 * - Start Game button
 *
 * Integration:
 * - Appears in MainMenuWidget's focus window when New Game is clicked
 * - Stores world name in UMOGameSettings::PendingWorldName
 * - Stores seed in UMOGameSettings::PendingWorldSeed
 * - Broadcasts OnStartGameRequested when Start Game is clicked
 */
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
