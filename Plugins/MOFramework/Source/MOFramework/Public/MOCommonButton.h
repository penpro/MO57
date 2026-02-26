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

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	/**
	 * Called when the button text needs to be updated.
	 * Override this in Blueprint to set your TextBlock's text.
	 *
	 * Example Blueprint implementation:
	 * - Get your TextBlock reference
	 * - Call SetText with the NewText parameter
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|Button")
	void UpdateButtonText(const FText& NewText);

protected:
	/** The text label displayed on the button. Set this per-instance in the designer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Button", meta=(ExposeOnSpawn=true))
	FText ButtonLabel;
};
