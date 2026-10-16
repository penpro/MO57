#include "MOWorldActionMenu.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOPlayerController.h"
#include "MOUIUtils.h"
#include "MOWorldActionProvider.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

bool UMOWorldActionMenu::InitializeForTarget(AActor* Target)
{
	TargetActor = Target;
	const IMOWorldActionProvider* Provider = Cast<IMOWorldActionProvider>(Target);
	const APlayerController* PC = GetOwningPlayer();
	TArray<FMOItemActionOption> Options;
	if (Provider && PC)
	{
		Provider->GetWorldActions(PC->GetPawn(), Options);
	}
	if (TitleText)
	{
		TitleText->SetText(Provider ? Provider->GetWorldActionTitle() : FText::GetEmpty());
	}
	if (StyleButton)
	{
		StyleButton->SetVisibility(ESlateVisibility::Collapsed);   // only its look is used
	}
	UMOUIUtils::RebuildActionButtons(this, ActionButtons, ButtonContainer, StyleButton ? static_cast<UWidget*>(StyleButton) : nullptr, Options, StyleButton,
		[this](FName Id) { OnAction(Id); });
	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldAction] menu for %s: %d option(s)"), *GetNameSafe(Target), Options.Num());
	return Options.Num() > 0;
}

void UMOWorldActionMenu::OnAction(FName ActionId)
{
	AActor* Target = TargetActor.Get();
	APlayerController* PC = GetOwningPlayer();
	const IMOWorldActionProvider* Provider = Cast<IMOWorldActionProvider>(Target);
	if (!Provider || !PC)
	{
		RequestClose();
		return;
	}
	// Re-read: the target may have changed while the menu was open (a part cut by someone else, the carcass rotting on).
	TArray<FMOItemActionOption> Options;
	Provider->GetWorldActions(PC->GetPawn(), Options);
	const FMOItemActionOption* Option = Options.FindByPredicate([ActionId](const FMOItemActionOption& O) { return O.Id == ActionId; });
	if (!Option || !Option->bEnabled)
	{
		if (AMOPlayerController* MOPC = Cast<AMOPlayerController>(PC))
		{
			MOPC->ClientShowNotification(Option ? Option->DisabledReason : NSLOCTEXT("MOWorldAction", "Gone", "That is no longer possible."), 3.0f, EMONotificationType::Warning);
		}
		RequestClose();
		return;
	}
	FMOWorldActions::Submit(PC, Target, ActionId);
	RequestClose();
}

void UMOWorldActionMenu::NativeDestruct()
{
	for (UMOCommonButton* Button : ActionButtons)
	{
		if (Button)
		{
			Button->OnClicked().Clear();
		}
	}
	ActionButtons.Reset();
	Super::NativeDestruct();
}
