#include "MOLoadPanel.h"
#include "MOFramework.h"

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

void UMOLoadPanel::HandleSlotSelected(const FString& SlotName)
{
	// In the Load panel, picking a slot means "load this". Caller is
	// responsible for confirming progress loss via UMOConfirmationBase.
	LoadFromSlot(SlotName);
}
