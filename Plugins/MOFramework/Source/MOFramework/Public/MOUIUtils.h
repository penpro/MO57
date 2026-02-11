#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MOUIUtils.generated.h"

class UTextBlock;
class UTexture2D;
struct FMORecipeDefinitionRow;
struct FMOItemDefinitionRow;

/**
 * Utility functions for MO UI widgets.
 * Centralizes common formatting and widget creation logic.
 */
UCLASS()
class MOFRAMEWORK_API UMOUIUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================================
	// TIME FORMATTING
	// ============================================================================

	/**
	 * Format a duration in seconds to a human-readable text.
	 * Examples: "5s", "2m", "1.5h"
	 * @param Seconds Duration in seconds
	 * @return Formatted text
	 */
	UFUNCTION(BlueprintPure, Category="MO|UI|Utils")
	static FText FormatDurationAsText(float Seconds);

	/**
	 * Format a duration as MM:SS or H:MM:SS.
	 * Examples: "5:32", "1:05:32"
	 * @param Seconds Duration in seconds
	 * @return Formatted text
	 */
	UFUNCTION(BlueprintPure, Category="MO|UI|Utils")
	static FText FormatDurationAsTimeCode(float Seconds);

	// ============================================================================
	// QUANTITY FORMATTING
	// ============================================================================

	/**
	 * Format a have/need quantity display.
	 * Examples: "Stone (5/10)", "Wood (10/10)"
	 * @param DisplayName Item display name
	 * @param Available Amount available
	 * @param Required Amount required
	 * @return Formatted text
	 */
	UFUNCTION(BlueprintPure, Category="MO|UI|Utils")
	static FText FormatQuantityDisplay(const FText& DisplayName, int32 Available, int32 Required);

	/**
	 * Format an output quantity display.
	 * Examples: "Stone x5", "Wood x1 (50%)"
	 * @param DisplayName Item display name
	 * @param Quantity Output quantity
	 * @param Chance Drop chance (1.0 = 100%)
	 * @return Formatted text
	 */
	UFUNCTION(BlueprintPure, Category="MO|UI|Utils")
	static FText FormatOutputDisplay(const FText& DisplayName, int32 Quantity, float Chance = 1.0f);

	/**
	 * Format an action display (for building parts).
	 * Examples: "Dig x1", "Hammer x5"
	 * @param ActionName Action display name
	 * @param Quantity Required quantity
	 * @return Formatted text
	 */
	UFUNCTION(BlueprintPure, Category="MO|UI|Utils")
	static FText FormatActionDisplay(const FText& ActionName, int32 Quantity);

	// ============================================================================
	// SKILL FORMATTING
	// ============================================================================

	/**
	 * Format a skill requirement display.
	 * Examples: "Woodworking: 3/5"
	 * @param SkillId Skill identifier
	 * @param CurrentLevel Player's current level
	 * @param RequiredLevel Required level
	 * @return Formatted text
	 */
	UFUNCTION(BlueprintPure, Category="MO|UI|Utils")
	static FText FormatSkillRequirement(FName SkillId, int32 CurrentLevel, int32 RequiredLevel);

	// ============================================================================
	// WIDGET CREATION
	// ============================================================================

	/**
	 * Create a simple text widget for ingredient/output display.
	 * @param Outer Object to own the widget
	 * @param Text Text to display
	 * @param bHasEnough If false, text is colored red
	 * @param FontSize Font size in points (default 12)
	 * @return Created text widget, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Utils")
	static UTextBlock* CreateSimpleTextWidget(UObject* Outer, const FText& Text, bool bHasEnough = true, int32 FontSize = 12);

	// ============================================================================
	// ICON LOADING
	// ============================================================================

	/**
	 * Load a recipe icon, falling back to the first output item's icon if not set.
	 * @param Recipe Recipe to get icon for
	 * @return Loaded texture, or nullptr if none available
	 */
	static UTexture2D* LoadRecipeIcon(const FMORecipeDefinitionRow* Recipe);

	/**
	 * Load an item icon (small variant).
	 * @param ItemDefId Item definition ID
	 * @return Loaded texture, or nullptr if not found
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Utils")
	static UTexture2D* LoadItemIconSmall(FName ItemDefId);

	// ============================================================================
	// COLOR CONSTANTS
	// ============================================================================

	/** Color for available/sufficient quantities (white). */
	static const FLinearColor ColorAvailable;

	/** Color for insufficient quantities (red-ish). */
	static const FLinearColor ColorInsufficient;

	/** Color for disabled/unavailable items (grey). */
	static const FLinearColor ColorDisabled;
};
