#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"

UMOCommonButton::UMOCommonButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOCommonButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	// Call Blueprint event to update text during design time
	UpdateButtonText(ButtonLabel);
}

void UMOCommonButton::NativeConstruct()
{
	Super::NativeConstruct();
	// Call Blueprint event to update text at runtime
	UpdateButtonText(ButtonLabel);
}

void UMOCommonButton::SetButtonText(const FText& InText)
{
	ButtonLabel = InText;
	UpdateButtonText(ButtonLabel);
}

bool UMOCommonButton::SimulateClick()
{
	// Route through the protected UCommonButtonBase handler so the click is
	// FAITHFUL: HandleButtonClicked() re-checks IsInteractionEnabled() and the
	// hold requirement before NativeOnClicked() broadcasts OnClicked(). We
	// pre-check interactability only to give the caller an honest return value
	// (the base handler returns void).
	if (!IsInteractionEnabled())
	{
		return false;
	}
	HandleButtonClicked();
	return true;
}

void UMOCommonButton::UpdateButtonText_Implementation(const FText& NewText)
{
	// Default: walk the button's widget tree and set the first TextBlock /
	// CommonTextBlock we find. This way the WBP doesn't need to override
	// the BlueprintNativeEvent — any button BP containing a text widget
	// will update correctly. Previously this was BlueprintImplementableEvent
	// (no C++ fallback) and WBPs that didn't implement the event left their
	// design-time defaults on screen — causing the swapped Overwrite/Confirm
	// labels on the save-overwrite vs exit-to-menu confirmation dialogs.
	if (!WidgetTree)
	{
		return;
	}

	TArray<UWidget*> AllWidgets;
	WidgetTree->GetAllWidgets(AllWidgets);
	for (UWidget* Widget : AllWidgets)
	{
		if (UCommonTextBlock* CTB = Cast<UCommonTextBlock>(Widget))
		{
			CTB->SetText(NewText);
			return;
		}
		if (UTextBlock* TB = Cast<UTextBlock>(Widget))
		{
			TB->SetText(NewText);
			return;
		}
	}
}
