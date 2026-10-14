/**
 * =============================================================================
 * MOModeIndicatorWidget.h - HUD Gameplay Mode Display
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Simple HUD widget showing current gameplay mode in bottom-right corner.
 * Displays mode name with color coding: Explore, Build, Terraform, Combat.
 *
 * BLUEPRINT SETUP:
 * 1. Create WBP inheriting from this class
 * 2. Add TextBlock named "ModeText" (BindWidgetOptional)
 * 3. Style colors per mode using the Color properties
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] BLUEPRINT EVENT: UpdateDisplay is BlueprintNativeEvent.
 *   Override in Blueprint to add animations/sound effects on mode change.
 *
 * [2024-02] MODE TEXT OPTIONAL: ModeText uses BindWidgetOptional.
 *   Widget still functions without it (for pure Blueprint display).
 *
 * =============================================================================
 * RELATED FILES: MOPlayerController.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOModeIndicatorWidget.generated.h"

/**
 * Gameplay mode types for the mode indicator.
 */
UENUM(BlueprintType)
enum class EMOGameplayMode : uint8
{
	Explore UMETA(DisplayName="Explore"),
	Build UMETA(DisplayName="Build"),
	Terraform UMETA(DisplayName="Terraform"),
	Combat UMETA(DisplayName="Combat")
};

class UTextBlock;

/**
 * Simple HUD widget showing current gameplay mode.
 * Displays in bottom-right corner: "Explore", "Build", "Terraform", "Combat". In Explore it names the biome underfoot -- "EXPLORING MEADOW" -- refreshed once a
 * second (playtest 2026-10-14 #16: "give me a 'exploring plains' and have it change with what biome we're in"): the biome whose GROUND the player stands on
 * (the dominant surface weight of the world generator), by its DT_Biomes display name.
 *
 * Setup in Blueprint:
 * - Bind a TextBlock named "ModeText" for the mode display
 * - Optionally style with colors per mode
 */
UCLASS()
class MOFRAMEWORK_API UMOModeIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ============================================================================
	// MODE MANAGEMENT
	// ============================================================================

	/** Set the current gameplay mode. Updates display. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Mode")
	void SetMode(EMOGameplayMode NewMode);

	/** Get the current gameplay mode. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Mode")
	EMOGameplayMode GetCurrentMode() const { return CurrentMode; }

	/** Get display name for a mode. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Mode")
	static FText GetModeDisplayName(EMOGameplayMode Mode);

	/** Get display color for a mode. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Mode")
	FLinearColor GetModeColor(EMOGameplayMode Mode) const;

	// ============================================================================
	// EVENTS
	// ============================================================================

	/** Called when mode changes. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOModeChangedSignature, EMOGameplayMode, OldMode, EMOGameplayMode, NewMode);
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Mode")
	FMOModeChangedSignature OnModeChanged;

	/** The biome the local player stands in (its display name), empty without a generated world. */
	FText GetBiomeUnderfoot() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Update the visual display. Called when mode changes. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|Mode")
	void UpdateDisplay();

	// ============================================================================
	// BINDINGS
	// ============================================================================

	/** Text block showing mode name. Bind in Blueprint. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ModeText;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	// Mode colours are design tokens (Explore = primary text, Build = accent, Terraform = notice, Combat = critical), resolved in GetModeColor.

private:
	void RefreshBiome();

	EMOGameplayMode CurrentMode = EMOGameplayMode::Explore;
	FText BiomeName;
	FTimerHandle BiomeTimer;
};
