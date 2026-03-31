/**
 * MOScrollListBase.cpp - Generic Scrollable List Widget Base Implementation
 */

#include "MOScrollListBase.h"
#include "MOListEntryBase.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/PanelWidget.h"

UMOScrollListBase::UMOScrollListBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOScrollListBase::NativeConstruct()
{
	Super::NativeConstruct();
}

// ============================================================================
// POPULATION
// ============================================================================

void UMOScrollListBase::PopulateList(const TArray<FName>& EntryIds)
{
	// Clear existing entries
	ClearList();

	// Validate entry class
	if (!EntryWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOScrollListBase::PopulateList - EntryWidgetClass is not set!"));
		return;
	}

	// Get container
	UPanelWidget* Container = GetContainer();
	if (!Container)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOScrollListBase::PopulateList - No container widget found!"));
		return;
	}

	// Store entry IDs
	CurrentEntryIds = EntryIds;

	// Create entry widgets
	for (const FName& EntryId : EntryIds)
	{
		UMOListEntryBase* Entry = CreateWidget<UMOListEntryBase>(this, EntryWidgetClass);
		if (Entry)
		{
			// Bind to selection delegate
			Entry->OnEntrySelected.AddDynamic(this, &UMOScrollListBase::HandleEntryClicked);

			// Set entry data
			Entry->SetEntryId(EntryId);

			// Allow subclasses to configure
			ConfigureEntry(Entry, EntryId);

			// Add to container
			Container->AddChild(Entry);
			EntryWidgets.Add(Entry);
		}
	}
}

void UMOScrollListBase::ClearList()
{
	UPanelWidget* Container = GetContainer();

	for (UMOListEntryBase* Entry : EntryWidgets)
	{
		if (Entry)
		{
			Entry->OnEntrySelected.RemoveAll(this);
			if (Container)
			{
				Container->RemoveChild(Entry);
			}
			Entry->RemoveFromParent();
		}
	}

	EntryWidgets.Empty();
	CurrentEntryIds.Empty();
	SelectedEntryId = NAME_None;
	CachedEntryPtrs.Empty();
}

void UMOScrollListBase::RefreshEntryStates()
{
	for (int32 i = 0; i < EntryWidgets.Num(); ++i)
	{
		UMOListEntryBase* Entry = EntryWidgets[i];
		if (Entry && CurrentEntryIds.IsValidIndex(i))
		{
			RefreshEntryState(Entry, CurrentEntryIds[i]);
		}
	}
}

void UMOScrollListBase::ConfigureEntry_Implementation(UMOListEntryBase* Entry, FName EntryId)
{
	// Base implementation does nothing - subclasses override to configure entries
}

void UMOScrollListBase::RefreshEntryState_Implementation(UMOListEntryBase* Entry, FName EntryId)
{
	// Base implementation just updates selection state
	if (Entry)
	{
		Entry->SetSelected(EntryId == SelectedEntryId);
	}
}

// ============================================================================
// SELECTION
// ============================================================================

void UMOScrollListBase::SelectEntry(FName EntryId)
{
	// Clear previous selection
	if (SelectedEntryId != NAME_None)
	{
		UMOListEntryBase* PrevEntry = GetEntryById(SelectedEntryId);
		if (PrevEntry)
		{
			PrevEntry->SetSelected(false);
		}
	}

	// Set new selection
	SelectedEntryId = EntryId;

	if (EntryId != NAME_None)
	{
		UMOListEntryBase* NewEntry = GetEntryById(EntryId);
		if (NewEntry)
		{
			NewEntry->SetSelected(true);
		}

		// Broadcast selection
		OnEntrySelected.Broadcast(EntryId);
	}
	else
	{
		// Selection cleared
		OnSelectionCleared.Broadcast();
	}
}

// ============================================================================
// ENTRY ACCESS
// ============================================================================

UMOListEntryBase* UMOScrollListBase::GetEntryById(FName EntryId) const
{
	for (int32 i = 0; i < CurrentEntryIds.Num(); ++i)
	{
		if (CurrentEntryIds[i] == EntryId && EntryWidgets.IsValidIndex(i))
		{
			return EntryWidgets[i];
		}
	}
	return nullptr;
}

const TArray<UMOListEntryBase*>& UMOScrollListBase::GetAllEntries() const
{
	CachedEntryPtrs.Reset();
	CachedEntryPtrs.Reserve(EntryWidgets.Num());
	for (const TObjectPtr<UMOListEntryBase>& Entry : EntryWidgets)
	{
		CachedEntryPtrs.Add(Entry.Get());
	}
	return CachedEntryPtrs;
}

// ============================================================================
// ENTRY CLICK HANDLING
// ============================================================================

void UMOScrollListBase::HandleEntryClicked(FName EntryId)
{
	SelectEntry(EntryId);
}

// ============================================================================
// CONTAINER ACCESS
// ============================================================================

UPanelWidget* UMOScrollListBase::GetContainer() const
{
	if (ContentScrollBox)
	{
		return ContentScrollBox;
	}
	return ContentContainer;
}
