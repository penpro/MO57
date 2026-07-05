#include "MOCharacterHistoryComponent.h"
#include "MOFramework.h"
#include "MOGameClockSubsystem.h"

UMOCharacterHistoryComponent::UMOCharacterHistoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMOCharacterHistoryComponent::AddEntry(EHistoryEntryType EntryType, const FString& Description)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	FMOCharacterHistoryEntry Entry;
	Entry.EntryType = EntryType;
	Entry.Description = FText::FromString(Description);
	if (UWorld* World = GetWorld())
	{
		if (UMOGameClockSubsystem* Clock = World->GetSubsystem<UMOGameClockSubsystem>())
		{
			Entry.Timestamp = Clock->GetGameDateTime();
		}
	}

	Entries.Add(Entry);
	if (Entries.Num() > MaxEntries)
	{
		Entries.RemoveAt(0, Entries.Num() - MaxEntries);
	}

	OnEntryAdded.Broadcast(Entry);
}

TArray<FMOCharacterHistoryEntry> UMOCharacterHistoryComponent::GetRecentEntries(int32 Count) const
{
	TArray<FMOCharacterHistoryEntry> Result;
	const int32 N = FMath::Min(Count, Entries.Num());
	Result.Reserve(N);
	for (int32 i = Entries.Num() - 1; i >= Entries.Num() - N; --i)
	{
		Result.Add(Entries[i]);
	}
	return Result;
}

void UMOCharacterHistoryComponent::BuildSaveData(FMOCharacterHistorySaveData& OutSaveData) const
{
	OutSaveData.Entries = Entries;
	OutSaveData.bHasValidData = true;
}

bool UMOCharacterHistoryComponent::ApplySaveDataAuthority(const FMOCharacterHistorySaveData& InSaveData)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !InSaveData.bHasValidData)
	{
		return false;
	}
	Entries = InSaveData.Entries;
	if (Entries.Num() > MaxEntries)
	{
		Entries.RemoveAt(0, Entries.Num() - MaxEntries);
	}
	return true;
}
