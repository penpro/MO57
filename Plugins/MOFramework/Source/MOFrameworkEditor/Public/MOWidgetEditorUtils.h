/**
 * MOWidgetEditorUtils.h - Editor utilities for widget blueprint manipulation
 *
 * Exposes protected widget properties (like bIsVariable) to Python/Blueprints.
 * This is needed because UWidget::bIsVariable has UPROPERTY() with no specifiers,
 * making it inaccessible to the reflection system.
 *
 * Usage from Python:
 *   unreal.MOWidgetEditorUtils.set_widget_is_variable(widget, True)
 *   is_var = unreal.MOWidgetEditorUtils.get_widget_is_variable(widget)
 *   layout_info = unreal.MOWidgetEditorUtils.get_all_widget_layout_info(wbp)
 *   unreal.MOWidgetEditorUtils.set_widget_anchors_preset(wbp, "MyWidget", "Center")
 */

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MOWidgetEditorUtils.generated.h"

class UWidget;
class UWidgetBlueprint;
class UPanelWidget;

/**
 * Layout information for a single widget, used for auditing/analysis.
 */
USTRUCT(BlueprintType)
struct FMOWidgetLayoutInfo
{
	GENERATED_BODY()

	/** Widget name */
	UPROPERTY(BlueprintReadOnly)
	FString Name;

	/** Widget class type (e.g., "TextBlock", "Button") */
	UPROPERTY(BlueprintReadOnly)
	FString WidgetType;

	/** Slot type (e.g., "CanvasPanelSlot", "HorizontalBoxSlot") */
	UPROPERTY(BlueprintReadOnly)
	FString SlotType;

	/** Parent widget name (empty for root) */
	UPROPERTY(BlueprintReadOnly)
	FString ParentName;

	/** Depth in widget hierarchy (0 = root) */
	UPROPERTY(BlueprintReadOnly)
	int32 Depth = 0;

	/** Is marked as variable */
	UPROPERTY(BlueprintReadOnly)
	bool bIsVariable = false;

	// Canvas slot properties
	UPROPERTY(BlueprintReadOnly)
	float AnchorMinX = 0.f;
	UPROPERTY(BlueprintReadOnly)
	float AnchorMinY = 0.f;
	UPROPERTY(BlueprintReadOnly)
	float AnchorMaxX = 0.f;
	UPROPERTY(BlueprintReadOnly)
	float AnchorMaxY = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float OffsetLeft = 0.f;
	UPROPERTY(BlueprintReadOnly)
	float OffsetTop = 0.f;
	UPROPERTY(BlueprintReadOnly)
	float OffsetRight = 0.f;
	UPROPERTY(BlueprintReadOnly)
	float OffsetBottom = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float AlignmentX = 0.f;
	UPROPERTY(BlueprintReadOnly)
	float AlignmentY = 0.f;

	UPROPERTY(BlueprintReadOnly)
	int32 ZOrder = 0;

	// Box slot properties
	UPROPERTY(BlueprintReadOnly)
	FString HorizontalAlignment;
	UPROPERTY(BlueprintReadOnly)
	FString VerticalAlignment;

	UPROPERTY(BlueprintReadOnly)
	float PaddingLeft = 0.f;
	UPROPERTY(BlueprintReadOnly)
	float PaddingTop = 0.f;
	UPROPERTY(BlueprintReadOnly)
	float PaddingRight = 0.f;
	UPROPERTY(BlueprintReadOnly)
	float PaddingBottom = 0.f;

	// SizeBox overrides (if widget is a SizeBox)
	UPROPERTY(BlueprintReadOnly)
	float WidthOverride = 0.f;
	UPROPERTY(BlueprintReadOnly)
	float HeightOverride = 0.f;
	UPROPERTY(BlueprintReadOnly)
	bool bHasWidthOverride = false;
	UPROPERTY(BlueprintReadOnly)
	bool bHasHeightOverride = false;
};

/**
 * Validation issue found in a widget blueprint.
 */
USTRUCT(BlueprintType)
struct FMOWidgetValidationIssue
{
	GENERATED_BODY()

	/** Severity: "Error", "Warning", "Info" */
	UPROPERTY(BlueprintReadOnly)
	FString Severity;

	/** Widget name (empty if blueprint-level issue) */
	UPROPERTY(BlueprintReadOnly)
	FString WidgetName;

	/** Issue description */
	UPROPERTY(BlueprintReadOnly)
	FString Message;

	/** Suggested fix */
	UPROPERTY(BlueprintReadOnly)
	FString Suggestion;
};

/**
 * Style information extracted from a widget.
 */
USTRUCT(BlueprintType)
struct FMOWidgetStyleInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString WidgetName;

	UPROPERTY(BlueprintReadOnly)
	FString WidgetType;

	// Text styles (for TextBlock)
	UPROPERTY(BlueprintReadOnly)
	FString FontFamily;
	UPROPERTY(BlueprintReadOnly)
	float FontSize = 0.f;
	UPROPERTY(BlueprintReadOnly)
	FLinearColor TextColor = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly)
	bool bHasTextStyle = false;

	// Border/Background styles
	UPROPERTY(BlueprintReadOnly)
	FLinearColor BackgroundColor = FLinearColor::Transparent;
	UPROPERTY(BlueprintReadOnly)
	FLinearColor BorderColor = FLinearColor::Transparent;
	UPROPERTY(BlueprintReadOnly)
	bool bHasBorderStyle = false;

	// Image tint
	UPROPERTY(BlueprintReadOnly)
	FLinearColor ImageTint = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly)
	bool bHasImageStyle = false;
};

/**
 * Editor utility functions for widget blueprint manipulation.
 * Exposes functionality that is otherwise inaccessible to Python/Blueprints.
 */
UCLASS()
class MOFRAMEWORKEDITOR_API UMOWidgetEditorUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Set the bIsVariable flag on a widget.
	 * This is equivalent to checking "Is Variable" in the Designer hierarchy.
	 *
	 * @param Widget The widget to modify
	 * @param bIsVariable Whether the widget should be exposed as a variable
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static bool SetWidgetIsVariable(UWidget* Widget, bool bIsVariable);

	/**
	 * Get the bIsVariable flag from a widget.
	 *
	 * @param Widget The widget to query
	 * @return True if the widget is marked as a variable
	 */
	UFUNCTION(BlueprintPure, Category = "MO|Editor|Widget")
	static bool GetWidgetIsVariable(UWidget* Widget);

	/**
	 * Mark a widget as variable and notify the blueprint system.
	 * This handles all the necessary editor notifications.
	 *
	 * @param WidgetBlueprint The widget blueprint containing the widget
	 * @param WidgetName Name of the widget to mark as variable
	 * @param bIsVariable Whether the widget should be exposed as a variable
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static bool SetWidgetIsVariableByName(UWidgetBlueprint* WidgetBlueprint, FName WidgetName, bool bIsVariable);

	/**
	 * Batch set multiple widgets as variables in a widget blueprint.
	 *
	 * @param WidgetBlueprint The widget blueprint to modify
	 * @param WidgetNames Array of widget names to mark as variables
	 * @param bIsVariable Whether the widgets should be exposed as variables
	 * @return Number of widgets successfully modified
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static int32 BatchSetWidgetsAsVariables(UWidgetBlueprint* WidgetBlueprint, const TArray<FName>& WidgetNames, bool bIsVariable = true);

	/**
	 * Get layout information for all widgets in a widget blueprint.
	 * This traverses the entire widget tree and extracts position, size, anchor, and slot info.
	 *
	 * @param WidgetBlueprint The widget blueprint to analyze
	 * @return Array of layout info for each widget in the tree
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static TArray<FMOWidgetLayoutInfo> GetAllWidgetLayoutInfo(UWidgetBlueprint* WidgetBlueprint);

	/**
	 * Get the root widget name of a widget blueprint.
	 *
	 * @param WidgetBlueprint The widget blueprint to query
	 * @return Name of the root widget, or empty string if none
	 */
	UFUNCTION(BlueprintPure, Category = "MO|Editor|Widget")
	static FString GetRootWidgetName(UWidgetBlueprint* WidgetBlueprint);

	/**
	 * Get the root widget type of a widget blueprint.
	 *
	 * @param WidgetBlueprint The widget blueprint to query
	 * @return Class name of the root widget, or empty string if none
	 */
	UFUNCTION(BlueprintPure, Category = "MO|Editor|Widget")
	static FString GetRootWidgetType(UWidgetBlueprint* WidgetBlueprint);

	// ========== Widget Modification ==========

	/**
	 * Set anchor preset for a widget in a canvas panel.
	 * Presets: "TopLeft", "TopCenter", "TopRight", "CenterLeft", "Center", "CenterRight",
	 *          "BottomLeft", "BottomCenter", "BottomRight", "TopStretch", "BottomStretch",
	 *          "LeftStretch", "RightStretch", "FullScreen"
	 *
	 * @param WidgetBlueprint The widget blueprint
	 * @param WidgetName Name of the widget to modify
	 * @param PresetName Anchor preset name
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static bool SetWidgetAnchorsPreset(UWidgetBlueprint* WidgetBlueprint, FName WidgetName, const FString& PresetName);

	/**
	 * Set size for a widget in a canvas panel (point anchor required).
	 *
	 * @param WidgetBlueprint The widget blueprint
	 * @param WidgetName Name of the widget to modify
	 * @param Width Desired width
	 * @param Height Desired height
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static bool SetWidgetSize(UWidgetBlueprint* WidgetBlueprint, FName WidgetName, float Width, float Height);

	/**
	 * Set position offset for a widget in a canvas panel.
	 *
	 * @param WidgetBlueprint The widget blueprint
	 * @param WidgetName Name of the widget to modify
	 * @param OffsetX X offset from anchor
	 * @param OffsetY Y offset from anchor
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static bool SetWidgetPosition(UWidgetBlueprint* WidgetBlueprint, FName WidgetName, float OffsetX, float OffsetY);

	/**
	 * Set alignment for a widget in a canvas panel.
	 *
	 * @param WidgetBlueprint The widget blueprint
	 * @param WidgetName Name of the widget to modify
	 * @param AlignmentX X alignment (0=left, 0.5=center, 1=right)
	 * @param AlignmentY Y alignment (0=top, 0.5=center, 1=bottom)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static bool SetWidgetAlignment(UWidgetBlueprint* WidgetBlueprint, FName WidgetName, float AlignmentX, float AlignmentY);

	/**
	 * Set Z-order for a widget in a canvas panel.
	 *
	 * @param WidgetBlueprint The widget blueprint
	 * @param WidgetName Name of the widget to modify
	 * @param ZOrder Z-order value (higher = on top)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static bool SetWidgetZOrder(UWidgetBlueprint* WidgetBlueprint, FName WidgetName, int32 ZOrder);

	/**
	 * Rename a widget in a blueprint.
	 *
	 * @param WidgetBlueprint The widget blueprint
	 * @param OldName Current widget name
	 * @param NewName Desired new name
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static bool RenameWidget(UWidgetBlueprint* WidgetBlueprint, FName OldName, FName NewName);

	// ========== Query Functions ==========

	/**
	 * Get all widgets of a specific type in a blueprint.
	 *
	 * @param WidgetBlueprint The widget blueprint
	 * @param WidgetTypeName Class name to search for (e.g., "TextBlock", "Button")
	 * @return Array of widget names matching the type
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static TArray<FString> GetWidgetsByType(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetTypeName);

	/**
	 * Get all widget names in a blueprint.
	 *
	 * @param WidgetBlueprint The widget blueprint
	 * @return Array of all widget names
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static TArray<FString> GetAllWidgetNames(UWidgetBlueprint* WidgetBlueprint);

	/**
	 * Check if a widget exists in a blueprint.
	 *
	 * @param WidgetBlueprint The widget blueprint
	 * @param WidgetName Name to check
	 * @return True if widget exists
	 */
	UFUNCTION(BlueprintPure, Category = "MO|Editor|Widget")
	static bool DoesWidgetExist(UWidgetBlueprint* WidgetBlueprint, FName WidgetName);

	// ========== Validation ==========

	/**
	 * Validate a widget blueprint for common issues.
	 * Checks: naming conventions, missing variables for BindWidget candidates,
	 * inconsistent anchoring, overlapping widgets, etc.
	 *
	 * @param WidgetBlueprint The widget blueprint to validate
	 * @return Array of validation issues found
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static TArray<FMOWidgetValidationIssue> ValidateWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint);

	// ========== Style Extraction ==========

	/**
	 * Extract style information from all widgets in a blueprint.
	 * Useful for identifying common styles that should be consolidated.
	 *
	 * @param WidgetBlueprint The widget blueprint
	 * @return Array of style info for each styled widget
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static TArray<FMOWidgetStyleInfo> ExtractWidgetStyles(UWidgetBlueprint* WidgetBlueprint);

	// ========== Batch Operations ==========

	/**
	 * Batch rename widgets matching a pattern.
	 *
	 * @param WidgetBlueprint The widget blueprint
	 * @param OldPrefix Prefix to replace (e.g., "txt")
	 * @param NewPrefix New prefix (e.g., "Text")
	 * @return Number of widgets renamed
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static int32 BatchRenameByPrefix(UWidgetBlueprint* WidgetBlueprint, const FString& OldPrefix, const FString& NewPrefix);

	/**
	 * Mark all widgets matching a naming pattern as variables.
	 *
	 * @param WidgetBlueprint The widget blueprint
	 * @param NamePattern Substring to match (e.g., "Button", "Text")
	 * @param bIsVariable Whether to mark as variable
	 * @return Number of widgets modified
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|Editor|Widget")
	static int32 BatchSetVariablesByPattern(UWidgetBlueprint* WidgetBlueprint, const FString& NamePattern, bool bIsVariable = true);
};
