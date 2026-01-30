#include "MOPossessionMenu.h"
#include "MOFramework.h"
#include "MOPawnEntryWidget.h"
#include "MOCommonButton.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"

void UMOPossessionMenu::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
		CloseButton->OnClicked().AddUObject(this, &UMOPossessionMenu::HandleCloseClicked);
	}

	if (CreateCharacterButton)
	{
		CreateCharacterButton->OnClicked().RemoveAll(this);
		CreateCharacterButton->OnClicked().AddUObject(this, &UMOPossessionMenu::HandleCreateCharacterClicked);
	}
}

FReply UMOPossessionMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();

	// Escape closes the menu (only if there are living pawns to return to)
	if (PressedKey == EKeys::Escape && LivingPawnCount > 0)
	{
		OnRequestClose.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMOPossessionMenu::PopulatePawnList(const TArray<FMOPersistedPawnRecord>& PawnRecords)
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

	// Separate living and deceased pawns
	TArray<FMOPersistedPawnRecord> LivingPawns;
	TArray<FMOPersistedPawnRecord> DeceasedPawns;

	for (const FMOPersistedPawnRecord& Record : PawnRecords)
	{
		if (Record.bIsDeceased)
		{
			DeceasedPawns.Add(Record);
		}
		else
		{
			LivingPawns.Add(Record);
		}
	}

	LivingPawnCount = LivingPawns.Num();

	// Sort living by last played (most recent first)
	LivingPawns.Sort([](const FMOPersistedPawnRecord& A, const FMOPersistedPawnRecord& B)
	{
		return A.LastPlayedTime > B.LastPlayedTime;
	});

	// Sort deceased by last played as well
	DeceasedPawns.Sort([](const FMOPersistedPawnRecord& A, const FMOPersistedPawnRecord& B)
	{
		return A.LastPlayedTime > B.LastPlayedTime;
	});

	// Add living pawns first
	for (const FMOPersistedPawnRecord& Record : LivingPawns)
	{
		UMOPawnEntryWidget* Entry = CreateWidget<UMOPawnEntryWidget>(PC, PawnEntryWidgetClass);
		if (Entry)
		{
			Entry->InitializeEntry(Record);
			Entry->OnPossessClicked.AddDynamic(this, &UMOPossessionMenu::HandlePawnEntryPossessClicked);
			PawnListScrollBox->AddChild(Entry);
			EntryWidgets.Add(Entry);
		}
	}

	// Add deceased pawns at the end
	for (const FMOPersistedPawnRecord& Record : DeceasedPawns)
	{
		UMOPawnEntryWidget* Entry = CreateWidget<UMOPawnEntryWidget>(PC, PawnEntryWidgetClass);
		if (Entry)
		{
			Entry->InitializeEntry(Record);
			Entry->OnPossessClicked.AddDynamic(this, &UMOPossessionMenu::HandlePawnEntryPossessClicked);
			PawnListScrollBox->AddChild(Entry);
			EntryWidgets.Add(Entry);
		}
	}

	// Show/hide empty list text
	if (EmptyListText)
	{
		EmptyListText->SetVisibility(PawnRecords.Num() == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Show "Create Character" if no living pawns
	SetCreateCharacterVisible(LivingPawnCount == 0);

	UE_LOG(LogMOFramework, Log, TEXT("[MOPossessionMenu] Populated with %d living, %d deceased pawns"),
		LivingPawns.Num(), DeceasedPawns.Num());
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
	// Only allow close if there are living pawns
	if (LivingPawnCount > 0)
	{
		OnRequestClose.Broadcast();
	}
}

void UMOPossessionMenu::HandleCreateCharacterClicked()
{
	OnCreateCharacter.Broadcast();
}

void UMOPossessionMenu::HandlePawnEntryPossessClicked(const FGuid& PawnGuid)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOPossessionMenu] Possess clicked for pawn %s"), *PawnGuid.ToString());
	OnPawnSelected.Broadcast(PawnGuid);
}
