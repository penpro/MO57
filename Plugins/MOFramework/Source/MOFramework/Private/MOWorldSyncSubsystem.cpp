#include "MOWorldSyncSubsystem.h"
#include "MOFramework.h"
#include "MOGameState.h"
#include "MOGameClockSubsystem.h"
#include "MOWeatherIntegrationSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	/** TEST ONLY: the host stops publishing its clock and weather, reproducing the old "every client runs a private day and sky".
	 *  `ue.py nettest game --withhold-sync` sets it to prove the harness's agreement checks can actually fail. */
	TAutoConsoleVariable<bool> CVarWithholdWorldSync(
		TEXT("MO.WorldSync.Withhold"), false,
		TEXT("TEST ONLY: the host does not publish its game clock / weather to clients."), ECVF_Cheat);
}

UMOWorldSyncSubsystem* UMOWorldSyncSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMOWorldSyncSubsystem>() : nullptr;
}

TStatId UMOWorldSyncSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMOWorldSyncSubsystem, STATGROUP_Tickables);
}

double UMOWorldSyncSubsystem::SnapThresholdGameSeconds(float TimeScale)
{
	// 1 game-second at normal speed; scales with TimeScale so the ordinary jitter between two machines (tens of ms of real time)
	// is never mistaken for drift when the clock is fast-forwarded.
	return FMath::Max(1.0, 0.25 * static_cast<double>(TimeScale));
}

// ============================================================================
// HOST: publish
// ============================================================================

void UMOWorldSyncSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	if (World->GetNetMode() == NM_Client)
	{
		if (bWeatherDirty)
		{
			TryApplyDesiredWeather(); // the weather provider (a Blueprint) can register after the first replication
		}
		return;
	}
	if (World->GetNetMode() == NM_Standalone)
	{
		return; // single player: nobody to tell
	}

	AMOGameState* State = World->GetGameState<AMOGameState>();
	if (!State || !State->HasAuthority() || CVarWithholdWorldSync.GetValueOnGameThread())
	{
		return;
	}

	ClockTimer += DeltaTime;
	if (ClockTimer >= ClockPublishIntervalSeconds)
	{
		ClockTimer = 0.f;
		PublishClock(*State);
	}
	WeatherTimer += DeltaTime;
	if (WeatherTimer >= WeatherPollIntervalSeconds)
	{
		WeatherTimer = 0.f;
		PublishWeather(*State);
	}
}

void UMOWorldSyncSubsystem::PublishClock(AMOGameState& State)
{
	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	if (!Clock)
	{
		return;
	}
	FMOWorldClockInfo Info;
	Info.bPublished = true;
	Info.GameDateTimeTicks = Clock->GetGameDateTime().GetTicks();
	Info.TimeScale = Clock->GetTimeScale();
	Info.ServerTimeSeconds = State.GetServerWorldTimeSeconds();
	State.SetWorldClockInfo(Info);
}

void UMOWorldSyncSubsystem::PublishWeather(AMOGameState& State)
{
	UMOWeatherIntegrationSubsystem* Weather = GetWorld()->GetSubsystem<UMOWeatherIntegrationSubsystem>();
	if (!Weather || !Weather->HasWeatherProvider())
	{
		return;
	}

	// The provider (the UDS bridge) reports what the sky is showing; its preset path is what a client needs to show the same.
	const FSoftObjectPath Path = Weather->GetCurrentWeatherPresetPath();
	if (!Path.IsValid() || Path == LastPublishedWeather)
	{
		return;
	}

	LastPublishedWeather = Path;
	FMOWorldWeatherInfo Info;
	Info.bPublished = true;
	Info.PresetPath = Path;
	State.SetWorldWeatherInfo(Info);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldSync] host published weather preset %s"), *Path.ToString());
}

// ============================================================================
// CLIENT: apply
// ============================================================================

void UMOWorldSyncSubsystem::HandleReplicatedClock(const FMOWorldClockInfo& Info)
{
	UWorld* World = GetWorld();
	if (!Info.bPublished || !World || World->GetNetMode() != NM_Client)
	{
		return;
	}
	UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	if (!Clock)
	{
		return;
	}

	if (!FMath::IsNearlyEqual(Clock->GetTimeScale(), Info.TimeScale))
	{
		Clock->SetTimeScale(Info.TimeScale);
	}

	// Where the host's clock is NOW: its sample plus the (server) time that has passed since, at its time scale.
	const AGameStateBase* State = World->GetGameState();
	const double AgeSeconds = State ? FMath::Max(0.0, State->GetServerWorldTimeSeconds() - Info.ServerTimeSeconds) : 0.0;
	const FDateTime Expected(Info.GameDateTimeTicks + FTimespan::FromSeconds(AgeSeconds * Info.TimeScale).GetTicks());

	const double DriftSeconds = (Clock->GetGameDateTime() - Expected).GetTotalSeconds();
	if (FMath::Abs(DriftSeconds) > SnapThresholdGameSeconds(Info.TimeScale))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldSync] client clock was %.1f game-s off the host's; snapping to %s"),
			DriftSeconds, *Expected.ToString());
		Clock->SetGameDateTime(Expected);
	}
}

void UMOWorldSyncSubsystem::HandleReplicatedWeather(const FMOWorldWeatherInfo& Info)
{
	UWorld* World = GetWorld();
	if (!Info.bPublished || !Info.PresetPath.IsValid() || !World || World->GetNetMode() != NM_Client)
	{
		return;
	}
	DesiredWeather = Info.PresetPath;
	bWeatherDirty = true;
	TryApplyDesiredWeather();
}

void UMOWorldSyncSubsystem::TryApplyDesiredWeather()
{
	UMOWeatherIntegrationSubsystem* Weather = GetWorld() ? GetWorld()->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr;
	if (!bWeatherDirty || !Weather || !Weather->HasWeatherProvider())
	{
		return; // retried from Tick until the provider registers
	}
	bWeatherDirty = false;

	UObject* Preset = DesiredWeather.TryLoad();
	if (!Preset)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldSync] cannot load the host's weather preset %s"), *DesiredWeather.ToString());
		return;
	}
	Weather->SetWeatherPreset(Preset);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldSync] client following the host's weather preset %s"), *DesiredWeather.ToString());
}
