#pragma once

#include "CoreMinimal.h"

/**
 * Domain-neutral interaction state for an inspectable catalog row.
 *
 * Selection opens details and must remain independent from whether the
 * selected action can execute right now. Conflating these states prevents the
 * details surface from explaining missing requirements.
 */
struct FMOInspectableEntryState
{
	bool bSelectable = true;
	bool bActionAvailable = false;
};

namespace MOUIInteractionState
{
	FORCEINLINE FMOInspectableEntryState MakeInspectableEntry(bool bActionAvailable)
	{
		FMOInspectableEntryState State;
		State.bSelectable = true;
		State.bActionAvailable = bActionAvailable;
		return State;
	}
}
