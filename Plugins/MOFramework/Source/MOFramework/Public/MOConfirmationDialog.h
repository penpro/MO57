/**
 * =============================================================================
 * MOConfirmationDialog.h - Reusable Confirmation Popup
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Reusable confirmation dialog for destructive/irreversible actions. Shows
 * title, message, and Yes/No buttons. Broadcasts result via delegate.
 *
 * USE CASES:
 * - Overwrite existing save file
 * - Load game (lose unsaved progress)
 * - Exit to main menu
 * - Exit game
 * - Delete save
 *
 * USAGE:
 *   UMOConfirmationDialog* Dialog = CreateWidget<UMOConfirmationDialog>(...);
 *   Dialog->Configure(Title, Message, YesText, NoText);
 *   Dialog->OnConfirmed.AddUObject(this, &Handler);
 *   Dialog->OnCancelled.AddUObject(this, &Handler);
 *   Dialog->ActivateWidget();
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] DELEGATE CLEANUP: OnConfirmed/OnCancelled bindings persist after
 *   dialog closes. Either: (a) create new dialog each time, or (b) call
 *   OnConfirmed.Clear() and OnCancelled.Clear() before rebinding.
 *
 * [2024-02] ESCAPE KEY: NativeOnKeyDown handles Escape to close. Broadcasts
 *   OnCancelled, same as clicking Cancel button. No special handling needed.
 *
 * [2024-02] BUTTON BINDING: NativeConstruct() binds button clicks. Uses
 *   RemoveAll(this) pattern to prevent duplicate bindings on reuse.
 *
 * [2024-02] FOCUS TARGET: NativeGetDesiredFocusTarget() returns CancelButton
 *   by default (safe choice). Override in Blueprint if Confirm should be default.
 *
 * [2024-02] ACTIVATION LIFECYCLE: Call ActivateWidget() after Setup() and
 *   delegate binding. Dialog auto-deactivates after Confirm/Cancel.
 *
 * [2024-02] REQUIRED WIDGETS: All BindWidget properties are required (not
 *   BindWidgetOptional). Blueprint must have TitleText, MessageText,
 *   ConfirmButton, CancelButton or compile will fail.
 *   SEVERITY: Compile-time error - won't package without all widgets.
 *
 * [2024-02] CONCURRENT DIALOGS: Opening multiple dialogs simultaneously is
 *   NOT recommended. Common UI focus system handles only one modal at a time.
 *   SYMPTOM: Second dialog steals focus, first becomes unresponsive.
 *   SOLUTION: Wait for OnConfirmed/OnCancelled before showing next dialog.
 *   Chain dialogs via delegate handlers, not parallel activation.
 *
 * [2024-02] DESTRUCTION DURING CALLBACK: If dialog is destroyed inside
 *   OnConfirmed/OnCancelled handler (e.g., RemoveFromParent() in handler):
 *   SAFE - delegate broadcast completes before destruction executes.
 *   UE delegate system handles this gracefully. No special handling needed.
 *
 * [2024-02] CONFIGURE VS SETUP: Configure() sets text and is idempotent.
 *   Can call Configure() multiple times to reuse same dialog instance.
 *   After Configure(), rebind delegates if handlers changed, then Activate.
 *
 * [SEVERITY GUIDE]
 *   REQUIRED WIDGETS: Compile-time failure
 *   DELEGATE CLEANUP: Logic bug (stale handlers fire)
 *   CONCURRENT DIALOGS: UX bug (focus issues)
 *   ESCAPE KEY: Feature documentation, not a pitfall
 *
 * =============================================================================
 * RELATED FILES: MOInGameMenu.h, MOSavePanel.h, MOLoadPanel.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOConfirmationDialog.generated.h"

class UMOCommonButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOConfirmationResultSignature);

/**
 * Reusable confirmation dialog.
 * See file header for usage and pitfalls.
 */
UCLASS()
class MOFRAMEWORK_API UMOConfirmationDialog : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	/**
	 * Configure and show the dialog.
	 * @param Title The title text (e.g., "Confirm Exit")
	 * @param Message The message text (e.g., "Are you sure you want to exit? Unsaved progress will be lost.")
	 * @param ConfirmText Text for confirm button (default: "Confirm")
	 * @param CancelText Text for cancel button (default: "Cancel")
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Confirmation")
	void Setup(const FText& Title, const FText& Message, const FText& ConfirmText = INVTEXT("Confirm"), const FText& CancelText = INVTEXT("Cancel"));

	/** Called when user confirms the action. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Confirmation")
	FMOConfirmationResultSignature OnConfirmed;

	/** Called when user cancels (or presses escape). */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Confirmation")
	FMOConfirmationResultSignature OnCancelled;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION() void HandleConfirmClicked();
	UFUNCTION() void HandleCancelClicked();

private:
	// ============================================================
	// BIND WIDGETS - Create these in your WBP_ConfirmationDialog
	// ============================================================

	/** Title text. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	/** Message/body text. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	/** Confirm button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> ConfirmButton;

	/** Cancel button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> CancelButton;
};
