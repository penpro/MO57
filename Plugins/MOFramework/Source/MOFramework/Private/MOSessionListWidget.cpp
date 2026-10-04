#include "MOSessionListWidget.h"
#include "MOSessionListEntry.h"

void UMOSessionListWidget::SetSessionResults(const TArray<FMOFoundSessionInfo>& Results)
{
	SessionsById.Reset();

	TArray<FName> Ids;
	Ids.Reserve(Results.Num());
	for (const FMOFoundSessionInfo& Info : Results)
	{
		const FName Id(*FString::FromInt(Info.ResultIndex));
		SessionsById.Add(Id, Info);
		Ids.Add(Id);
	}

	PopulateList(Ids);
}

bool UMOSessionListWidget::GetSelectedSessionInfo(FMOFoundSessionInfo& OutInfo) const
{
	const FName SelectedId = GetSelectedEntryId();
	if (SelectedId.IsNone())
	{
		return false;
	}

	if (const FMOFoundSessionInfo* Found = SessionsById.Find(SelectedId))
	{
		OutInfo = *Found;
		return true;
	}

	return false;
}

void UMOSessionListWidget::ConfigureEntry_Implementation(UMOListEntryBase* Entry, FName EntryId)
{
	Super::ConfigureEntry_Implementation(Entry, EntryId);

	if (UMOSessionListEntry* SessionEntry = Cast<UMOSessionListEntry>(Entry))
	{
		if (const FMOFoundSessionInfo* Info = SessionsById.Find(EntryId))
		{
			SessionEntry->SetSessionInfo(*Info);
		}
	}
}
