/**
 * =============================================================================
 * MOConfirmationDialog.h - Concrete Confirmation Dialog (legacy shim)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Backward-compatibility shim over UMOConfirmationBase. Originally a parallel
 * implementation of the same yes/no dialog pattern. Now reduced to a thin
 * subclass so existing call sites that use Setup(...) and BPs that derive
 * from this class continue working.
 *
 * NEW CODE SHOULD: derive its WBP directly from UMOConfirmationBase and call
 * Configure(...) instead. This shim exists only to keep legacy paths working.
 *
 * USAGE (legacy, still supported):
 *   UMOConfirmationDialog* Dialog = CreateWidget<UMOConfirmationDialog>(...);
 *   Dialog->Setup(Title, Message, ConfirmText, CancelText);
 *   Dialog->OnConfirmed.AddDynamic(this, &Handler);   // inherited from Base
 *   Dialog->OnCancelled.AddDynamic(this, &Handler);   // inherited from Base
 *   Dialog->ActivateWidget();
 *
 * USAGE (preferred, going forward):
 *   Derive WBP_YourDialog directly from UMOConfirmationBase, call
 *   Configure(...) — Setup is just an alias for Configure.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-05] MIGRATION NOTE: All bindings (OnConfirmed, OnCancelled, TitleText,
 *   MessageText, ConfirmButton, CancelButton) are inherited from
 *   UMOConfirmationBase. WBPs parented to this class with those BindWidget
 *   names continue to work without changes.
 *
 * [2026-05] SHIM ONLY: This class adds NO new behavior. Setup() is a wrapper
 *   around the base's Configure(). All input mode / key handling / focus
 *   logic lives in the base.
 *
 * =============================================================================
 * RELATED FILES: MOConfirmationBase.h (canonical), MOInGameMenu.h, MOSavePanel.h
 * LAST UPDATED: 2026-05-23
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOConfirmationBase.h"
#include "MOConfirmationDialog.generated.h"

/**
 * Backward-compatibility shim. New code should derive directly from
 * UMOConfirmationBase. See file header.
 */
UCLASS()
class MOFRAMEWORK_API UMOConfirmationDialog : public UMOConfirmationBase
{
	GENERATED_BODY()

public:
	/**
	 * Legacy entry point — alias for UMOConfirmationBase::Configure().
	 * Kept so existing call sites (e.g., save-overwrite, exit-to-menu flows)
	 * don't need to be renamed.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Confirmation")
	void Setup(const FText& Title, const FText& Message,
		const FText& ConfirmText = INVTEXT("Confirm"),
		const FText& CancelText = INVTEXT("Cancel"));
};
