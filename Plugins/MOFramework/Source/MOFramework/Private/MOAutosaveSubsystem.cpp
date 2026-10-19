#include "MOAutosaveSubsystem.h"

#include "MOFramework.h"
#include "MOMainMenuGameMode.h"
#include "MOUIManagerComponent.h"
#include "MOVoxelReadinessSubsystem.h"
#include "MOWorldSave.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "MOAutosave"

static TAutoConsoleVariable<float> CVarAutosaveMinutes(
	TEXT("MO.Save.AutosaveMinutes"),
	10.0f,
	TEXT("Real minutes between timed autosaves of the world on the host (0 = off). N-157, Docs/Persistence.md section 1."),
	ECVF_Default);

namespace MOAutosave
{
	/** How often the interval is checked (the CVar can change at any time), and how long the notice shows before the save's hitch. */
	constexpr float CheckSeconds = 5.0f;
	constexpr float NoticeLeadSeconds = 0.3f;
}

bool UMOAutosaveSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && Super::ShouldCreateSubsystem(Outer);
}

void UMOAutosaveSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client || Cast<AMOMainMenuGameMode>(InWorld.GetAuthGameMode()))
	{
		return;   // the host owns the world save; the menu has no world to save
	}
	LastSaveSeconds = FPlatformTime::Seconds();
	InWorld.GetTimerManager().SetTimer(CheckTimer, this, &UMOAutosaveSubsystem::CheckAutosave, MOAutosave::CheckSeconds, true);
}

void UMOAutosaveSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
		World->GetTimerManager().ClearTimer(SaveTimer);
	}
	Super::Deinitialize();
}

double UMOAutosaveSubsystem::GetSecondsSinceLastSave() const
{
	return FPlatformTime::Seconds() - LastSaveSeconds;
}

bool UMOAutosaveSubsystem::CanAutosaveNow() const
{
	const UWorld* World = GetWorld();
	const UMOVoxelReadinessSubsystem* Readiness = World ? World->GetSubsystem<UMOVoxelReadinessSubsystem>() : nullptr;
	return Readiness && Readiness->IsReady();   // not while the world is loading or revealing
}

void UMOAutosaveSubsystem::CheckAutosave()
{
	const float Minutes = CVarAutosaveMinutes.GetValueOnGameThread();
	if (Minutes <= 0.0f || bSavePending || GetSecondsSinceLastSave() < Minutes * 60.0)
	{
		return;
	}
	if (!CanAutosaveNow())
	{
		return;   // try again at the next check
	}

	// Say it before the save's hitch: the notice needs a frame or two on screen first.
	bSavePending = true;
	UWorld* World = GetWorld();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController())
		{
			if (UMOUIManagerComponent* UI = PC->FindComponentByClass<UMOUIManagerComponent>())
			{
				UI->ShowNotification(LOCTEXT("Autosaving", "Saving the world..."), 2.0f);
			}
		}
	}
	World->GetTimerManager().SetTimer(SaveTimer, this, &UMOAutosaveSubsystem::SaveNow, MOAutosave::NoticeLeadSeconds, false);
}

void UMOAutosaveSubsystem::SaveNow()
{
	bSavePending = false;
	LastSaveSeconds = FPlatformTime::Seconds();
	if (!CanAutosaveNow())
	{
		return;
	}
	MOWorldSave::SaveThisWorld(GetWorld(), TEXT("autosave"));
}

#undef LOCTEXT_NAMESPACE
