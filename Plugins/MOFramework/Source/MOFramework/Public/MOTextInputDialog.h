/**
 * =============================================================================
 * MOTextInputDialog.h - Generic Single-Field Text Input Modal
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Reusable modal for collecting a single line of text from the player.
 * Subclasses UMOConfirmationBase so it inherits title / message / confirm /
 * cancel / Escape behavior, then adds an editable text field on top.
 *
 * USE CASES:
 *   - Rename a save slot (player enters new display name)
 *   - Name a new pawn / settlement / waypoint
 *   - Console command entry (later)
 *   - Search text input (later)
 *
 * USAGE (C++):
 *   UMOTextInputDialog* Dialog = CreateWidget<UMOTextInputDialog>(...);
 *   Dialog->ConfigureWithText(Title, Message, InitialText, MaxLen, Placeholder);
 *   Dialog->OnTextConfirmed.AddDynamic(this, &Handler);  // fires with entered text
 *   Dialog->OnCancelled.AddDynamic(this, &CancelHandler);
 *   Dialog->ActivateWidget();
 *
 * BLUEPRINT (WBP_TextInputDialog):
 *   Parent class: MOTextInputDialog
 *   Required BindWidgets (inherited from UMOConfirmationBase):
 *     - TitleText (UTextBlock)
 *     - MessageText (UTextBlock)
 *     - ConfirmButton (UMOCommonButton)
 *     - CancelButton (UMOCommonButton)
 *   New required BindWidget on this class:
 *     - TextInput (UEditableTextBox) — the single-line input
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-05] EMPTY INPUT: Confirm fires even if the input is empty. Consumers
 *   should either pre-fill InitialText (so the player has to actively delete
 *   it to send empty) or check the returned FText in their OnTextConfirmed
 *   handler and re-open the dialog if invalid. The dialog itself does NOT
 *   validate — that's policy and lives with the caller.
 *
 * [2026-05] CONFIRM BROADCAST ORDER: OnTextConfirmed fires BEFORE OnConfirmed
 *   (the inherited parameterless delegate). Consumers can listen to either;
 *   prefer OnTextConfirmed since you almost always want the entered text.
 *
 * [2026-05] FOCUS: NativeGetDesiredFocusTarget returns the TextInput so the
 *   player can start typing immediately. Override in subclass / BP if a
 *   different default focus makes sense.
 *
 * [2026-05] ENTER-TO-CONFIRM: Pressing Enter on the text input commits the
 *   dialog (calls confirm). The text widget's OnTextCommitted with
 *   ETextCommit::OnEnter triggers HandleConfirmClicked.
 *
 * =============================================================================
 * RELATED FILES: MOConfirmationBase.h, MOConfirmationDialog.h, MOSavePanel.h
 * LAST UPDATED: 2026-05-23
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOConfirmationBase.h"
#include "MOTextInputDialog.generated.h"

class UEditableTextBox;

/** Confirmation delegate that carries the entered text. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOTextInputConfirmedDelegate, const FText&, EnteredText);

/**
 * Reusable text-input modal. See file header for usage and pitfalls.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOTextInputDialog : public UMOConfirmationBase
{
	GENERATED_BODY()

public:
	UMOTextInputDialog(const FObjectInitializer& ObjectInitializer);

	/**
	 * Configure the dialog with text-input specifics on top of the base
	 * Title / Message / button labels.
	 *
	 * @param Title         Modal title shown via the inherited TitleText.
	 * @param Message       Message body shown via the inherited MessageText.
	 * @param InitialText   Pre-fills the input. Useful for rename flows where
	 *                      the player should see the current name first.
	 * @param MaxLength     Hard cap on input length (0 = unlimited). Enforced
	 *                      via FOnTextCommitted clamp; UI clamp depends on BP.
	 * @param HintText      Placeholder shown when the input is empty.
	 * @param ConfirmText   Label for the confirm button.
	 * @param CancelText    Label for the cancel button.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|TextInput")
	void ConfigureWithText(
		const FText& Title,
		const FText& Message,
		const FText& InitialText,
		int32 MaxLength = 0,
		const FText& HintText = FText::GetEmpty(),
		const FText& ConfirmText = INVTEXT("Confirm"),
		const FText& CancelText = INVTEXT("Cancel"));

	/** Read the currently entered text from the input field. */
	UFUNCTION(BlueprintPure, Category="MO|UI|TextInput")
	FText GetEnteredText() const;

	/**
	 * Fires on confirm with the entered text. Prefer this over the inherited
	 * OnConfirmed (parameterless) for consumers that need the text — almost
	 * always the case.
	 */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|TextInput")
	FMOTextInputConfirmedDelegate OnTextConfirmed;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/**
	 * Override the base confirmed hook so we can broadcast OnTextConfirmed
	 * (with the entered text) before the inherited OnConfirmed.
	 */
	virtual void OnDialogConfirmed_Implementation() override;

	UFUNCTION()
	void HandleTextCommitted(const FText& InText, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleTextChanged(const FText& InText);

	/** Single-line text input. Required BindWidget on the BP. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UEditableTextBox> TextInput;

	/** Cached max length (0 = unlimited) set by ConfigureWithText. */
	UPROPERTY(BlueprintReadOnly, Category="MO|UI|TextInput")
	int32 MaxLength = 0;
};
