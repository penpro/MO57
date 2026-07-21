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

	const FMOListSelectionTransition Transition = SelectionModel.ReplaceEntries(EntryIds);
	ClearEntryWidgets();

	// Create entry widgets
	for (const FName& EntryId : SelectionModel.GetEntryIds())
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
			Entry->SetSelected(EntryId == SelectionModel.GetSelectedId());

			// Add to container
			Container->AddChild(Entry);
			EntryWidgets.Add(Entry);
		}
	}

	ApplySelectionTransition(Transition);
}

void UMOScrollListBase::ClearList()
{
	const FMOListSelectionTransition Transition = SelectionModel.ClearEntries();
	ClearEntryWidgets();
	ApplySelectionTransition(Transition);
}

void UMOScrollListBase::ClearEntryWidgets()
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
	CachedEntryPtrs.Empty();
}

void UMOScrollListBase::RefreshEntryStates()
{
	for (int32 i = 0; i < EntryWidgets.Num(); ++i)
	{
		UMOListEntryBase* Entry = EntryWidgets[i];
		if (Entry && SelectionModel.GetEntryIds().IsValidIndex(i))
		{
			RefreshEntryState(Entry, SelectionModel.GetEntryIds()[i]);
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
		Entry->SetSelected(EntryId == SelectionModel.GetSelectedId());
	}
}

// ============================================================================
// SELECTION
// ============================================================================

void UMOScrollListBase::SelectEntry(FName EntryId)
{
	ApplySelectionTransition(SelectionModel.Select(EntryId));
}

void UMOScrollListBase::ApplySelectionTransition(const FMOListSelectionTransition& Transition)
{
	if (Transition.Change == EMOListSelectionChange::None)
	{
		return;
	}

	if (UMOListEntryBase* PreviousEntry = GetEntryById(Transition.PreviousId))
	{
		PreviousEntry->SetSelected(false);
	}

	if (Transition.Change == EMOListSelectionChange::Selected)
	{
		if (UMOListEntryBase* CurrentEntry = GetEntryById(Transition.CurrentId))
		{
			CurrentEntry->SetSelected(true);
		}
		OnEntrySelected.Broadcast(Transition.CurrentId);
	}
	else
	{
		OnSelectionCleared.Broadcast();
	}
}

// ============================================================================
// ENTRY ACCESS
// ============================================================================

UMOListEntryBase* UMOScrollListBase::GetEntryById(FName EntryId) const
{
	const TArray<FName>& EntryIds = SelectionModel.GetEntryIds();
	for (int32 i = 0; i < EntryIds.Num(); ++i)
	{
		if (EntryIds[i] == EntryId && EntryWidgets.IsValidIndex(i))
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
