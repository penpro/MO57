/**
 * =============================================================================
 * MOListEntryBase.h - Generic List Entry Widget Base
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Abstract base class for list entry widgets. Provides standardized selection,
 * click handling, and visual state management. Use for any scrollable list
 * where entries can be selected.
 *
 * SUBCLASSES:
 * - UMORecipeEntryWidget (existing - consider migrating)
 * - UMOBuildingEntryWidget (existing - consider migrating)
 * - UMOSkillEntryWidget (existing - consider migrating)
 * - UMOColonyCharacterEntry (planned)
 *
 * WIDGET BINDINGS (in Blueprint):
 * - EntryButton (UMOCommonButton, optional) - Clickable button
 * - BackgroundBorder (UBorder, optional) - Selection highlight
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-03] BUTTON TYPE: Uses UMOCommonButton (CommonUI). Bind click with
 *   OnClicked().AddUObject(), NOT OnClicked.AddDynamic().
 *
 * [2026-03] DATA BINDING: SetEntryId() stores the ID. Subclasses override
 *   OnDataBound() to update visuals. Always call Super.
 *
 * [2026-03] SELECTION STATE: SetSelected() updates visual and broadcasts
 *   delegate. Parent list should listen to OnEntrySelected.
 *
 * =============================================================================
 * RELATED FILES: MOScrollListBase.h, MOCommonButton.h, MOUIDelegates.h
 * LAST UPDATED: 2026-03-28
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOUIDelegates.h"
#include "MOListEntryBase.generated.h"

class UMOCommonButton;
class UBorder;

/**
 * Delegate fired when this entry is clicked/selected.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOListEntrySelectedDelegate, FName, EntryId);

/**
 * Base class for list entry widgets.
 * See file header for usage and pitfalls.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOListEntryBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOListEntryBase(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// DATA BINDING
	// ============================================================================

	/**
	 * Set the entry's identifier.
	 * Subclasses should override OnDataBound() to update visuals.
	 * @param InEntryId Unique identifier for this entry's data
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|List")
	virtual void SetEntryId(FName InEntryId);

	/** Get the entry's identifier. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|List")
	FName GetEntryId() const { return EntryId; }

	// ============================================================================
	// SELECTION
	// ============================================================================

	/** Set whether this entry is selected. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|List")
	virtual void SetSelected(bool bInSelected);

	/** Check if this entry is selected. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|List")
	bool IsSelected() const { return bIsSelected; }

	// ============================================================================
	// ENABLED STATE
	// ============================================================================

	/** Set whether this entry is enabled/interactable. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|List")
	virtual void SetEntryEnabled(bool bInEnabled);

	/** Check if this entry is enabled. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|List")
	bool IsEntryEnabled() const { return bIsEnabled; }

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when this entry is clicked/selected. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|List")
	FMOListEntrySelectedDelegate OnEntrySelected;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Color when entry is selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|List|Style")
	FLinearColor SelectedColor = FLinearColor(0.2f, 0.4f, 0.8f, 1.0f);

	/** Color when entry is not selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|List|Style")
	FLinearColor NormalColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);

	/** Color when entry is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|List|Style")
	FLinearColor DisabledColor = FLinearColor(0.1f, 0.1f, 0.1f, 0.5f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * Called when entry data is bound via SetEntryId().
	 * Override in subclasses to update visuals.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MO|UI|List")
	void OnDataBound(FName InEntryId);

	/**
	 * Update visual appearance based on selection/enabled state.
	 * Override in subclasses for custom styling.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MO|UI|List")
	void UpdateVisuals();

	/** Handle button click. Override in subclasses to add domain-specific delegates. */
	UFUNCTION()
	virtual void HandleButtonClicked();

	// ============================================================================
	// WIDGET BINDINGS
	// ============================================================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMOCommonButton> EntryButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BackgroundBorder;

private:
	FName EntryId = NAME_None;
	bool bIsSelected = false;
	bool bIsEnabled = true;
};
