/**
 * =============================================================================
 * MODetailPanelBase.h - Generic Detail Panel Widget Base
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Abstract base class for detail panels that show information about a
 * selected item. Provides title, description, icon, and action button
 * handling. Use for recipe details, building details, item info, etc.
 *
 * SUBCLASSES:
 * - UMORecipeDetailPanel (existing - consider migrating)
 * - UMOBuildingDetailPanel (existing - consider migrating)
 * - UMOItemInfoPanel (existing - consider migrating)
 * - UMOCharacterDetailPanel (planned)
 *
 * WIDGET BINDINGS (in Blueprint):
 * - TitleText (UTextBlock, optional) - Title display
 * - DescriptionText (UTextBlock, optional) - Description display
 * - IconImage (UImage, optional) - Icon display
 * - ActionButton (UMOCommonButton, optional) - Primary action
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-03] ASYNC ICONS: If using TSoftObjectPtr for icons, load async or
 *   use LoadSynchronous() in SetIcon(). Current impl uses sync loading.
 *
 * [2026-03] ACTION DELEGATE: OnActionRequested broadcasts when action button
 *   is clicked. Parent should handle the action (craft, build, etc).
 *
 * =============================================================================
 * RELATED FILES: MORecipeDetailPanel.h, MOScrollListBase.h, MOUIDelegates.h
 * LAST UPDATED: 2026-03-28
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOUIDelegates.h"
#include "MODetailPanelBase.generated.h"

class UTextBlock;
class UImage;
class UMOCommonButton;
class UTexture2D;

/**
 * Delegate fired when the action button is clicked.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMODetailPanelActionDelegate, FName, ItemId);

/**
 * Base class for detail panel widgets.
 * See file header for usage and pitfalls.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMODetailPanelBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UMODetailPanelBase(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// DATA BINDING
	// ============================================================================

	/**
	 * Set the item ID this panel is showing details for.
	 * Override OnItemBound() to load and display data.
	 * @param InItemId Unique identifier for the item
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Detail")
	virtual void SetItemId(FName InItemId);

	/** Get the current item ID. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Detail")
	FName GetItemId() const { return ItemId; }

	/**
	 * Clear the panel (no item selected).
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Detail")
	virtual void ClearPanel();

	// ============================================================================
	// CONTENT SETTERS
	// ============================================================================

	/** Set the title text. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Detail")
	virtual void SetTitle(const FText& Title);

	/** Set the description text. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Detail")
	virtual void SetDescription(const FText& Description);

	/** Set the icon texture. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Detail")
	virtual void SetIcon(UTexture2D* Icon);

	/** Set the icon from a soft reference (loads synchronously). */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Detail")
	virtual void SetIconFromSoftPtr(TSoftObjectPtr<UTexture2D> IconPtr);

	// ============================================================================
	// ACTION BUTTON
	// ============================================================================

	/** Set the action button text. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Detail")
	virtual void SetActionText(const FText& ActionText);

	/** Set whether the action button is enabled. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Detail")
	virtual void SetActionEnabled(bool bEnabled);

	/** Set the action button visibility. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Detail")
	virtual void SetActionVisible(bool bVisible);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when the action button is clicked. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Detail")
	FMODetailPanelActionDelegate OnActionRequested;

	/** Broadcast when panel requests to close. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Detail")
	FMOUIRequestClose OnCloseRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * Called when item ID is set via SetItemId().
	 * Override in subclasses to load and display item data.
	 * @param InItemId The item identifier
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MO|UI|Detail")
	void OnItemBound(FName InItemId);

	/** Handle action button click. */
	UFUNCTION()
	void HandleActionClicked();

	// ============================================================================
	// WIDGET BINDINGS
	// ============================================================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMOCommonButton> ActionButton;

private:
	FName ItemId = NAME_None;
};
