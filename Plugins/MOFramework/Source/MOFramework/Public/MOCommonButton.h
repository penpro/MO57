/**
 * =============================================================================
 * MOCommonButton.h - Standard UI Button Class
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Standard button class for ALL MOFramework UI. Extends UCommonButtonBase
 * with text label support. Use this instead of UButton for consistent
 * styling and Common UI integration.
 *
 * BLUEPRINT SETUP (WBP_MOCommonButton):
 * 1. Create WBP_MOCommonButton with parent class UMOCommonButton
 * 2. Add a TextBlock for the label
 * 3. Override "UpdateButtonText" event to set the TextBlock's text
 * 4. Configure button styles in Common UI settings
 *
 * C++ USAGE:
 *   // Binding clicks (NOT OnClicked.AddDynamic - use delegate directly)
 *   MyButton->OnClicked().AddUObject(this, &UMyWidget::HandleClick);
 *
 *   // Setting text
 *   MyButton->SetButtonText(FText::FromString("Click Me"));
 *
 *   // Enable/disable
 *   MyButton->SetIsEnabled(bCanClick);
 *
 * =============================================================================
 * BEST PRACTICES
 * =============================================================================
 *
 * 1. ALWAYS use OnClicked().AddUObject(), NOT OnClicked.AddDynamic().
 *    CommonButtonBase uses different delegate pattern than UButton.
 *
 * 2. ALWAYS call OnClicked().RemoveAll(this) before binding in NativeConstruct
 *    to prevent duplicate bindings during widget reconstruction.
 *
 * 3. Text is NOT BindWidget. Use UpdateButtonText Blueprint event or
 *    SetButtonText() to update the label.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [CRITICAL] DELEGATE BINDING: This is UCommonButtonBase, NOT UButton.
 *   - WRONG: Button->OnClicked.AddDynamic(this, &Handler)
 *   - RIGHT: Button->OnClicked().AddUObject(this, &Handler)
 *
 * [2024-01] DUPLICATE BINDINGS: NativeConstruct can be called multiple times.
 *   Always RemoveAll(this) before AddUObject to prevent duplicate handlers.
 *
 * [2024-02] BLUEPRINT TEXT: The text widget is managed in Blueprint via
 *   UpdateButtonText event, not via BindWidget. This is because CommonUI's
 *   internal structure makes BindWidget unreliable for text.
 *
 * [2024-02] FOCUS HANDLING: CommonUI buttons handle focus automatically.
 *   Don't manually call SetFocus unless you have a specific reason.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOUIDelegates.h - Standard delegate patterns
 * - MOUIControllerBase.h - Input mode management for menus
 * - All menu widgets - Use this as their button class
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "MOCommonButton.generated.h"

/**
 * Standard button for MOFramework UI.
 * See file header for usage guide and pitfalls.
 */
UCLASS()
class MOFRAMEWORK_API UMOCommonButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UMOCommonButton(const FObjectInitializer& ObjectInitializer);

	/** Set the button's text label. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Button")
	void SetButtonText(const FText& InText);

	/** Get the button's current text label. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Button")
	FText GetButtonText() const { return ButtonLabel; }

	/**
	 * TEST/AUTOMATION: run the full guarded click path exactly as a user click
	 * would — UCommonButtonBase::HandleButtonClicked() checks
	 * IsInteractionEnabled() + hold requirements, then NativeOnClicked()
	 * broadcasts OnClicked()/BP_OnClicked. Used by MO.Test.ClickWidget so the
	 * autonomous harness can drive real UI flows (a locked/disabled button
	 * correctly refuses the click). Returns false without clicking when the
	 * button is not interactable.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Button|Test")
	bool SimulateClick();

	/**
	 * Select or deselect from code, whichever way round. CommonUI's SetIsSelected(false) is a NO-OP on a button that is not toggleable (it only honours a deselect for toggles; a button group clears
	 * its members through ClearSelection()), so a tab row that called SetIsSelected(i == Selected) on selectable, non-toggleable tabs left every earlier tab selected (underlined) -- found in the
	 * status panel and in UMOTabBarWidget. Use this for any selectable button whose selection your code owns.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Button")
	void SetSelectedState(bool bInSelected);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	/** Push the style's per-state text style (NormalTextStyle / Hovered / Selected / Disabled) to the label: CommonUI only does this for you in Blueprint (OnCurrentTextStyleChanged). */
	virtual void NativeOnCurrentTextStyleChanged() override;

	/** Keyboard / gamepad focus (not mouse hover) shows the kit's focus ring: 2 px tallow ring, 2 px gap, drawn outside the button, so hover, focus and selected never look alike. */
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	/**
	 * Optional: an Image (kit `focus_ring` 9-slice, tint color.focus.ring) in the button's own overlay so a scroll box cannot clip it. Its slot padding is owned by FitFocusRingToButton(): the button's
	 * content (this widget) sits INSIDE the style's button padding, so a fixed -4 drew the ring around the content area, narrower than the button by the padding on each side.
	 */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> FocusRing;

	/** Grow the FocusRing slot outward by the style's button padding plus the 4 px (2 px ring + 2 px gap) the kit asks for, so the ring hugs the button's drawn bounds for every style (Lg, icon, tab, row). */
	void FitFocusRingToButton();

	void ApplyCurrentTextStyleToLabels();

	/**
	 * Called when the button text needs to be updated.
	 *
	 * C++ default behavior: walks the button's widget tree to find the first
	 * UTextBlock or UCommonTextBlock descendant and updates its text. This
	 * means SetButtonText() Just Works for any button BP that contains a
	 * text widget — no Blueprint event override required.
	 *
	 * Override in Blueprint if you need custom logic (e.g. multiple text
	 * widgets, formatted display, conditional swap-out).
	 */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|Button")
	void UpdateButtonText(const FText& NewText);
	virtual void UpdateButtonText_Implementation(const FText& NewText);

public:
	/** The action a runtime button stands for (UMOUIUtils::RebuildActionButtons): how a refresh knows the same actions are on screen and updates them in place. */
	UPROPERTY(Transient)
	FName ActionId;

protected:
	/** The text label displayed on the button. Set this per-instance in the designer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Button", meta=(ExposeOnSpawn=true))
	FText ButtonLabel;
};
