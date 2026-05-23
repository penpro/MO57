#include "MOConfirmationDialog.h"

void UMOConfirmationDialog::Setup(const FText& Title, const FText& Message,
	const FText& ConfirmText, const FText& CancelText)
{
	// Legacy alias for the base class's Configure(). All actual setup —
	// applying button labels, wiring delegates, etc. — lives on
	// UMOConfirmationBase. This shim just preserves the older method name
	// for existing call sites.
	Configure(Title, Message, ConfirmText, CancelText);
}
