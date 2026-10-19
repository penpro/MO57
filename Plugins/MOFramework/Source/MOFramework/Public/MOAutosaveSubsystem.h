#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOAutosaveSubsystem.generated.h"

/**
 * The timed autosave (N-157, designed in Docs/Persistence.md §1). A crash or a killed process used to lose everything since the last exit,
 * and killing the process was the one save-scum route left.
 *
 * Host / standalone gameplay worlds only (a client never saves: the host owns the world save). Every MO.Save.AutosaveMinutes real minutes
 * (default 10; 0 turns it off) it shows the host's players a short "Saving the world..." notice -- no hidden work -- and then saves through
 * MOWorldSave::SaveThisWorld, the step the Save button and every exit use. It skips the main menu and any moment the world is not READY
 * (loading / revealing). Nothing pauses (Docs/PAUSE_POLICY.md); the save itself is a short game-thread step (0.01 s packaged, 4 pawns).
 */
UCLASS()
class MOFRAMEWORK_API UMOAutosaveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Real seconds since the last autosave (or since the world began play). */
	double GetSecondsSinceLastSave() const;

private:
	void CheckAutosave();
	void SaveNow();
	bool CanAutosaveNow() const;

	FTimerHandle CheckTimer;
	FTimerHandle SaveTimer;
	double LastSaveSeconds = 0.0;
	bool bSavePending = false;
};
