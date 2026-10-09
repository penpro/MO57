#include "MOLoadPanel.h"
#include "MOFramework.h"
#include "Components/TextBlock.h"

void UMOLoadPanel::SetFilterToCurrentWorld(bool bFilter)
{
	if (bFilterToCurrentWorld == bFilter)
	{
		return;
	}
	bFilterToCurrentWorld = bFilter;
	RefreshSaveList();
}

void UMOLoadPanel::LoadFromSlot(const FString& SlotName)
{
	OnLoadRequested.Broadcast(SlotName);
}

void UMOLoadPanel::SetHostStatus(const FText& Text)
{
	if (StatusText)
	{
		StatusText->SetText(Text);
		StatusText->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UMOLoadPanel::HandleSlotHostRequested(const FString& SlotName)
{
	// Hosting resumes a save as a co-op server. Like load, the caller owns the transition.
	OnHostRequested.Broadcast(SlotName);
}

void UMOLoadPanel::HandleSlotSelected(const FString& SlotName)
{
	// In the Load panel, picking a slot means "load this". Caller is
	// responsible for confirming progress loss via UMOConfirmationBase.
	LoadFromSlot(SlotName);
}
