#pragma once

#include "CoreMinimal.h"

/** Describes the observable result of a catalog selection mutation. */
enum class EMOListSelectionChange : uint8
{
	None,
	Selected,
	Cleared
};

/** Result returned by FMOListSelectionModel mutations. */
struct MOFRAMEWORK_API FMOListSelectionTransition
{
	EMOListSelectionChange Change = EMOListSelectionChange::None;
	FName PreviousId = NAME_None;
	FName CurrentId = NAME_None;
};

/**
 * Pure authoritative state for an ID-backed UI catalog.
 *
 * Entry IDs are de-duplicated in source order. Replacing the catalog preserves
 * a still-valid selection and clears a selection that is no longer present.
 * Selecting the current ID is idempotent and selecting an unknown ID is
 * treated as an explicit clear.
 */
class MOFRAMEWORK_API FMOListSelectionModel
{
public:
	FMOListSelectionTransition ReplaceEntries(const TArray<FName>& InEntryIds)
	{
		TArray<FName> UniqueIds;
		UniqueIds.Reserve(InEntryIds.Num());
		for (const FName EntryId : InEntryIds)
		{
			if (!EntryId.IsNone())
			{
				UniqueIds.AddUnique(EntryId);
			}
		}

		EntryIds = MoveTemp(UniqueIds);
		if (!SelectedId.IsNone() && !EntryIds.Contains(SelectedId))
		{
			return ApplySelection(NAME_None);
		}

		return MakeUnchangedTransition();
	}

	FMOListSelectionTransition ClearEntries()
	{
		EntryIds.Reset();
		return ApplySelection(NAME_None);
	}

	FMOListSelectionTransition Select(FName RequestedId)
	{
		const FName ResolvedId = !RequestedId.IsNone() && EntryIds.Contains(RequestedId)
			? RequestedId
			: NAME_None;
		return ApplySelection(ResolvedId);
	}

	const TArray<FName>& GetEntryIds() const { return EntryIds; }
	FName GetSelectedId() const { return SelectedId; }
	bool HasSelection() const { return !SelectedId.IsNone(); }

private:
	FMOListSelectionTransition ApplySelection(FName NewSelectedId)
	{
		FMOListSelectionTransition Transition;
		Transition.PreviousId = SelectedId;
		Transition.CurrentId = SelectedId;

		if (SelectedId == NewSelectedId)
		{
			return Transition;
		}

		SelectedId = NewSelectedId;
		Transition.CurrentId = SelectedId;
		Transition.Change = SelectedId.IsNone()
			? EMOListSelectionChange::Cleared
			: EMOListSelectionChange::Selected;
		return Transition;
	}

	FMOListSelectionTransition MakeUnchangedTransition() const
	{
		FMOListSelectionTransition Transition;
		Transition.PreviousId = SelectedId;
		Transition.CurrentId = SelectedId;
		return Transition;
	}

	TArray<FName> EntryIds;
	FName SelectedId = NAME_None;
};
