#pragma once

#include "CoreMinimal.h"

class UWorld;

/** What a save of this world did (MOWorldSave::SaveThisWorld). A client is not a failure: the host owns the world save. */
enum class EMOWorldSaveResult : uint8
{
	Saved,
	NotHost,
	Failed
};

/**
 * THE save of the world the players are in, for every caller: the in-game Save button and every way out (UMOSystemMenuUIController), and
 * the timed autosave (UMOAutosaveSubsystem). Host / standalone only -- a client gets NotHost and writes nothing (the host owns the world
 * save). Writes this world's one slot (UMOPersistenceSubsystem::SaveToCurrentSlot; one save per world, F24). Reason goes to the log.
 */
namespace MOWorldSave
{
	MOFRAMEWORK_API EMOWorldSaveResult SaveThisWorld(UWorld* World, const TCHAR* Reason);
}
