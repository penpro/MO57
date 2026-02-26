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
 * Displays in bottom-right corner: "Explore", "Build", "Terraform", "Combat"
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

protected:
	virtual void NativeConstruct() override;

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

	/** Color for Explore mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Mode|Colors")
	FLinearColor ExploreColor = FLinearColor(0.7f, 0.9f, 1.0f, 1.0f); // Light blue

	/** Color for Build mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Mode|Colors")
	FLinearColor BuildColor = FLinearColor(1.0f, 0.8f, 0.2f, 1.0f); // Gold

	/** Color for Terraform mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Mode|Colors")
	FLinearColor TerraformColor = FLinearColor(0.6f, 0.4f, 0.2f, 1.0f); // Brown/Earth

	/** Color for Combat mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Mode|Colors")
	FLinearColor CombatColor = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f); // Red

private:
	EMOGameplayMode CurrentMode = EMOGameplayMode::Explore;
};
