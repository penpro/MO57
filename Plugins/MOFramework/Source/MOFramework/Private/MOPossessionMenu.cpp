#include "MOPossessionMenu.h"
#include "MOFramework.h"
#include "MOPawnEntryWidget.h"
#include "MOCommonButton.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"

UMOPossessionMenu::UMOPossessionMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Base class (UMOMenuWidgetBase) handles:
	// - SetIsFocusable(true)
	// - bIsBackHandler = true (for Escape/Tab close)
	// - bAutoRestoreFocus = true
}

void UMOPossessionMenu::RequestClose()
{
	// Call base class which broadcasts OnRequestClose
	Super::RequestClose();

	// Also broadcast legacy delegate for backward compatibility
	OnLegacyRequestClose.Broadcast();
}

void UMOPossessionMenu::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureButtonBindings();
}

void UMOPossessionMenu::NativeDestruct()
{
	// Clean up button bindings
	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
	}
	if (CreateCharacterButton)
	{
		CreateCharacterButton->OnClicked().RemoveAll(this);
	}

	// Clean up entry widget bindings
	for (UMOPawnEntryWidget* Entry : EntryWidgets)
	{
		if (Entry)
		{
			Entry->OnPossessClicked.RemoveDynamic(this, &UMOPossessionMenu::HandlePawnEntryPossessClicked);
		}
	}

	Super::NativeDestruct();
}

void UMOPossessionMenu::EnsureButtonBindings()
{
	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
		CloseButton->OnClicked().AddUObject(this, &UMOPossessionMenu::HandleCloseClicked);
	}

	if (CreateCharacterButton)
	{
		CreateCharacterButton->OnClicked().RemoveAll(this);
		CreateCharacterButton->OnClicked().AddUObject(this, &UMOPossessionMenu::HandleCreateCharacterClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOPossessionMenu] CreateCharacterButton binding ensured"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPossessionMenu] CreateCharacterButton is NULL!"));
	}
}

void UMOPossessionMenu::PopulatePawnList(const TArray<FMOPersistedPawnRecord>& PawnRecords)
{
	TArray<FMOPossessionListEntry> Entries;
	Entries.Reserve(PawnRecords.Num());
	for (const FMOPersistedPawnRecord& Record : PawnRecords)
	{
		Entries.Add(FMOPossessionListEntry::FromRecord(Record));
	}
	PopulatePossessionList(Entries);
}

void UMOPossessionMenu::PopulatePossessionList(const TArray<FMOPossessionListEntry>& Rows)
{
	ClearPawnList();

	if (!PawnEntryWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPossessionMenu] PawnEntryWidgetClass not set"));
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	// Order: the survivor you are playing, then the free ones, then other players' (shown, not offered), the dead last; most recently played first within each.
	auto Rank = [](const FMOPossessionListEntry& E)
	{
		if (E.bIsDeceased)
		{
			return 3;
		}
		return E.Occupancy == EMOPawnOccupancy::You ? 0 : E.Occupancy == EMOPawnOccupancy::Free ? 1 : 2;
	};
	TArray<FMOPossessionListEntry> Sorted = Rows;
	Sorted.StableSort([&Rank](const FMOPossessionListEntry& A, const FMOPossessionListEntry& B)
	{
		const int32 RankA = Rank(A), RankB = Rank(B);
		return RankA != RankB ? RankA < RankB : A.LastPlayedTicks > B.LastPlayedTicks;
	});

	int32 Deceased = 0, Others = 0;
	for (const FMOPossessionListEntry& Row : Sorted)
	{
		Deceased += Row.bIsDeceased ? 1 : 0;
		Others += (!Row.bIsDeceased && Row.Occupancy == EMOPawnOccupancy::OtherPlayer) ? 1 : 0;
		UMOPawnEntryWidget* Entry = CreateWidget<UMOPawnEntryWidget>(PC, PawnEntryWidgetClass);
		if (Entry)
		{
			Entry->InitializeEntry(Row.ToDisplayRecord());
			Entry->SetOccupancy(Row.Occupancy, Row.OccupantName);
			Entry->OnPossessClicked.AddDynamic(this, &UMOPossessionMenu::HandlePawnEntryPossessClicked);
			PawnListScrollBox->AddChild(Entry);
			EntryWidgets.Add(Entry);
		}
	}
	// Survivors this player can be (their own and the free ones): other players' do not count.
	LivingPawnCount = Sorted.Num() - Deceased - Others;

	// Show/hide empty list text
	if (EmptyListText)
	{
		EmptyListText->SetVisibility(Rows.Num() == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Show "Create Character" if no living pawns
	SetCreateCharacterVisible(LivingPawnCount == 0);

	UE_LOG(LogMOFramework, Log, TEXT("[MOPossessionMenu] Populated with %d of your / free living, %d other players', %d deceased"),
		LivingPawnCount, Others, Deceased);
}

void UMOPossessionMenu::ClearPawnList()
{
	for (UMOPawnEntryWidget* Entry : EntryWidgets)
	{
		if (Entry)
		{
			Entry->RemoveFromParent();
		}
	}
	EntryWidgets.Empty();
	LivingPawnCount = 0;
}

void UMOPossessionMenu::SetCreateCharacterVisible(bool bVisible)
{
	if (CreateCharacterButton)
	{
		CreateCharacterButton->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UMOPossessionMenu::HandleCloseClicked()
{
	// Always allow close - the player can be in spectator mode or controlling any pawn
	// Use base class RequestClose() which broadcasts OnRequestClose and legacy delegate
	RequestClose();
}

void UMOPossessionMenu::HandleCreateCharacterClicked()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOPossessionMenu] HandleCreateCharacterClicked - broadcasting OnCreateCharacter (IsBound: %s)"),
		OnCreateCharacter.IsBound() ? TEXT("YES") : TEXT("NO"));
	OnCreateCharacter.Broadcast();
}

void UMOPossessionMenu::HandlePawnEntryPossessClicked(const FGuid& PawnGuid)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOPossessionMenu] Possess clicked for pawn %s"), *PawnGuid.ToString());
	OnPawnSelected.Broadcast(PawnGuid);
}
