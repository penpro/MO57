#include "MOWorldSave.h"

#include "MOFramework.h"
#include "MOPersistenceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

EMOWorldSaveResult MOWorldSave::SaveThisWorld(UWorld* World, const TCHAR* Reason)
{
	if (World && World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldSave] Saving this world (%s): client -- the host owns the world save, nothing written"), Reason);
		return EMOWorldSaveResult::NotHost;
	}

	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UMOPersistenceSubsystem* Persistence = GameInstance ? GameInstance->GetSubsystem<UMOPersistenceSubsystem>() : nullptr;
	if (!Persistence)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOWorldSave] Saving this world (%s): no persistence subsystem -- NOT saved"), Reason);
		return EMOWorldSaveResult::Failed;
	}

	const double Start = FPlatformTime::Seconds();
	const bool bSaved = Persistence->SaveToCurrentSlot();
	UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldSave] Saving this world (%s): slot '%s' ok=%d in %.0f ms"),
		Reason, *Persistence->GetCurrentSlotName(), bSaved ? 1 : 0, (FPlatformTime::Seconds() - Start) * 1000.0);
	return bSaved ? EMOWorldSaveResult::Saved : EMOWorldSaveResult::Failed;
}
