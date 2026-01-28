#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOConfirmationDialog.generated.h"

class UMOCommonButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOConfirmationResultSignature);

/**
 * Reusable confirmation dialog for destructive actions.
 *
 * Use cases:
 * - Overwrite save
 * - Load game (lose unsaved progress)
 * - Exit to main menu
 * - Exit game
 */
UCLASS()
class MOFRAMEWORK_API UMOConfirmationDialog : public UCommonActivatableWidget
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
