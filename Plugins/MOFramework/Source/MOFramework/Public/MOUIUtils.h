/**
 * =============================================================================
 * MOUIUtils.h - UI Formatting & Widget Creation Utilities
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Centralized formatting and widget creation for UI. Use these utilities
 * instead of duplicating formatting logic in individual widgets. Ensures
 * consistent display of quantities, durations, and skill requirements.
 *
 * KEY RESPONSIBILITIES:
 * 1. Time/duration formatting (seconds -> "5s", "2m 30s", etc.)
 * 2. Quantity display formatting ("Stone (5/10)", "Wood x3")
 * 3. Skill requirement display
 * 4. Simple text widget creation with color coding
 * 5. Icon loading with fallbacks
 *
 * WHEN TO USE:
 * - FormatDurationAsText(): Crafting time, build time display
 * - FormatQuantityDisplay(): Ingredient lists (have/need)
 * - FormatOutputDisplay(): Recipe output preview
 * - CreateSimpleTextWidget(): Dynamic text for panels
 * - LoadRecipeIcon(): Recipe list icons with fallback
 *
 * =============================================================================
 * BEST PRACTICES
 * =============================================================================
 *
 * 1. ALWAYS use these formatters instead of FString::Printf for UI text.
 *    This ensures consistent formatting across all UI elements.
 *
 * 2. Use ColorAvailable/ColorInsufficient/ColorDisabled constants for
 *    consistent color coding across the UI.
 *
 * 3. LoadRecipeIcon() handles fallback logic - don't duplicate it.
 *    It checks recipe icon first, then first output item's icon.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] WIDGET OWNERSHIP: CreateSimpleTextWidget() requires valid Outer.
 *   Pass the parent widget or owning UObject. Passing nullptr causes GC issues.
 *
 * [2024-02] ICON SOFT REFERENCES: Icon paths in DataTables are TSoftObjectPtr.
 *   LoadRecipeIcon/LoadItemIconSmall handle synchronous loading internally.
 *   For async loading, use different approach.
 *
 * [2024-02] CHANCE DISPLAY: FormatOutputDisplay only shows chance if < 1.0.
 *   100% chance items don't show "(100%)" - this is intentional.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MORecipeDetailPanel.h - Uses formatters for recipe display
 * - MOCraftingQueueEntryWidget.h - Uses duration formatting
 * - MOInventoryMenu.h - Uses quantity display
 * - MOSkillsPanel.h - Uses skill requirement formatting
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

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
 * See file header for usage guide and pitfalls.
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
