/**
 * MOUIStyleConstants.h - Centralized UI style definitions
 *
 * Use these constants throughout UI code for consistent styling.
 * All Widget Blueprints should reference these instead of hardcoded values.
 */

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MOUIStyleConstants.generated.h"

/**
 * Font size presets for consistent typography.
 */
UENUM(BlueprintType)
enum class EMOFontSize : uint8
{
	Default = 0		UMETA(DisplayName = "Default (12pt)"),
	Small = 1		UMETA(DisplayName = "Small (10pt)"),
	Body = 2		UMETA(DisplayName = "Body (12pt)"),
	BodyLarge = 3	UMETA(DisplayName = "Body Large (14pt)"),
	Header = 4		UMETA(DisplayName = "Header (16pt)"),
	HeaderLarge = 5 UMETA(DisplayName = "Header Large (20pt)"),
	Title = 6		UMETA(DisplayName = "Title (24pt)"),
	TitleLarge = 7	UMETA(DisplayName = "Title Large (32pt)")
};

/**
 * Color palette for consistent UI theming.
 */
UENUM(BlueprintType)
enum class EMOUIColor : uint8
{
	// Text colors
	TextPrimary,		// White - main text
	TextSecondary,		// Light gray - secondary info
	TextMuted,			// Dark gray - disabled/hints
	TextAccent,			// Highlight color
	TextError,			// Red - errors
	TextSuccess,		// Green - success
	TextWarning,		// Yellow - warnings

	// Background colors
	BackgroundDark,		// Darkest background
	BackgroundMedium,	// Standard panel background
	BackgroundLight,	// Lighter panels
	BackgroundOverlay,	// Semi-transparent overlay

	// Interactive colors
	ButtonNormal,
	ButtonHovered,
	ButtonPressed,
	ButtonDisabled,

	// Border colors
	BorderDefault,
	BorderFocused,
	BorderError,

	// Progress/Status colors
	ProgressFill,
	ProgressBackground,
	HealthBar,
	StaminaBar,
	HungerBar
};

/**
 * Standard widget dimensions for consistency.
 */
UENUM(BlueprintType)
enum class EMOWidgetSize : uint8
{
	// Buttons
	ButtonSmall,		// 100x30
	ButtonMedium,		// 150x40
	ButtonLarge,		// 200x50

	// List entries
	EntryCompact,		// Height: 30
	EntryNormal,		// Height: 40
	EntryLarge,			// Height: 60

	// Icons
	IconSmall,			// 24x24
	IconMedium,			// 48x48
	IconLarge,			// 80x80
	IconXLarge,			// 100x100

	// Panels
	PanelSmall,			// 300x200
	PanelMedium,		// 500x400
	PanelLarge,			// 800x600
	PanelFullscreen		// Fill screen
};

/**
 * Blueprint function library for accessing UI style constants.
 */
UCLASS()
class MOFRAMEWORK_API UMOUIStyleConstants : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ========== Colors ==========

	/** Get a color from the MO UI palette */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Style")
	static FLinearColor GetColor(EMOUIColor ColorType);

	/** Get color as SlateColor for widget styling */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Style")
	static FSlateColor GetSlateColor(EMOUIColor ColorType);

	// ========== Font Sizes ==========

	/** Get font size value */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Style")
	static int32 GetFontSize(EMOFontSize SizePreset);

	// ========== Dimensions ==========

	/** Get standard widget dimensions */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Style")
	static FVector2D GetWidgetSize(EMOWidgetSize SizePreset);

	/** Get standard button size */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Style")
	static FVector2D GetButtonSize(bool bLarge = false);

	/** Get standard entry/list item height */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Style")
	static float GetEntryHeight(bool bCompact = false);

	/** Get standard icon size */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Style")
	static float GetIconSize(bool bLarge = false);

	// ========== Spacing ==========

	/** Get standard padding value */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Style")
	static float GetPadding(bool bLarge = false);

	/** Get standard margin value */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Style")
	static FMargin GetMargin(bool bLarge = false);

	/** Get standard spacing between elements */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Style")
	static float GetSpacing(bool bLarge = false);
};
