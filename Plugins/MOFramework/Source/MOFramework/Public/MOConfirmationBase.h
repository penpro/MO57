/**
 * =============================================================================
 * MOConfirmationBase.h - Generic Confirmation Dialog Base
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Abstract base class for confirmation dialogs. Shows title, message, and
 * confirm/cancel buttons. Use for destructive actions, confirmations, etc.
 * This is a generic version of MOConfirmationDialog for broader use.
 *
 * SUBCLASSES:
 * - UMOConfirmationDialog (existing - consider migrating)
 * - Any custom confirmation dialogs
 *
 * WIDGET BINDINGS (in Blueprint):
 * - TitleText (UTextBlock) - Dialog title
 * - MessageText (UTextBlock) - Dialog message
 * - ConfirmButton (UMOCommonButton) - Confirm action
 * - CancelButton (UMOCommonButton) - Cancel action
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-03] ESCAPE KEY: NativeOnKeyDown handles Escape as cancel.
 *
 * [2026-03] DELEGATE CLEANUP: OnConfirmed/OnCancelled bindings persist.
 *   Clear bindings before rebinding if reusing dialog.
 *
 * [2026-03] REQUIRED WIDGETS: TitleText, MessageText, ConfirmButton, and
 *   CancelButton are required (BindWidget). Blueprint must have all.
 *
 * =============================================================================
 * RELATED FILES: MOConfirmationDialog.h, MOUIDelegates.h
 * LAST UPDATED: 2026-03-28
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOConfirmationBase.generated.h"

class UTextBlock;
class UMOCommonButton;

/**
 * Delegate fired when confirmation result is received.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOConfirmationDelegate);

/**
 * Base class for confirmation dialog widgets.
 * See file header for usage and pitfalls.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOConfirmationBase : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOConfirmationBase(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/**
	 * Configure the dialog with content.
	 * @param Title Dialog title
	 * @param Message Dialog message
	 * @param ConfirmText Text for confirm button (default: "Confirm")
	 * @param CancelText Text for cancel button (default: "Cancel")
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Confirmation")
	virtual void Configure(
		const FText& Title,
		const FText& Message,
		const FText& ConfirmText = INVTEXT("Confirm"),
		const FText& CancelText = INVTEXT("Cancel")
	);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when user confirms. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Confirmation")
	FMOConfirmationDelegate OnConfirmed;

	/** Broadcast when user cancels (or presses Escape). */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Confirmation")
	FMOConfirmationDelegate OnCancelled;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/**
	 * Confirmations run in Menu input mode — they're modal and must block
	 * gameplay input until acknowledged. Subclasses can override if they need
	 * different input modes (e.g., a non-blocking notification confirm).
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	/** Handle confirm button click. */
	UFUNCTION()
	void HandleConfirmClicked();

	/** Handle cancel button click. */
	UFUNCTION()
	void HandleCancelClicked();

	/**
	 * Called when dialog is confirmed.
	 * Override for custom behavior before delegate broadcast.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MO|UI|Confirmation")
	void OnDialogConfirmed();

	/**
	 * Called when dialog is cancelled.
	 * Override for custom behavior before delegate broadcast.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MO|UI|Confirmation")
	void OnDialogCancelled();

	// ============================================================================
	// WIDGET BINDINGS
	// ============================================================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMOCommonButton> ConfirmButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMOCommonButton> CancelButton;
};
