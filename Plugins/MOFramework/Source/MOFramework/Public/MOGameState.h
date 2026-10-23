/**
 * MOGameState.h - Replicated, world-wide match state
 *
 * It carries what every client needs about the world itself: the world seed (MOWorldSeedTypes.h), the host's
 * clock and weather (MOWorldSyncTypes.h), the instances removed for good (MOWorldRemovalTypes.h) and the
 * worked-ground zones (MOTerrainModificationTypes.h). It is the engine's canonical place for such state, so
 * future world-wide replicated facts belong here rather than on a bespoke always-relevant actor.
 *
 * The GameState is set as AMOGameMode's GameStateClass in its constructor (BP_MOGameMode does
 * not override it). If a Blueprint ever overrides it with a non-AMOGameState class, the host
 * logs an error when it tries to publish the seed instead of failing silently.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MOWorldSeedTypes.h"
#include "MOWorldSyncTypes.h"
#include "MOWorldRemovalTypes.h"
#include "MOTerrainModificationTypes.h"
#include "MOWeatherTypes.h"
#include "MOWorldCreationTypes.h"
#include "MOGameState.generated.h"

UCLASS()
class MOFRAMEWORK_API AMOGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AMOGameState();

	/** Authority only (UMOWorldRemovalSubsystem): one more world instance removed for good. Replicates to every client, current and future. */
	void AddRemovedInstance(const FMORemovedInstance& Entry);

	/** Authority only: replace the whole record (a loaded save). */
	void ResetRemovedInstances(const TArray<FMORemovedInstance>& All);

	const TArray<FMORemovedInstance>& GetRemovedInstances() const { return RemovedInstances.Items; }

	/**
	 * Authority only (UMOTerrainModificationSubsystem): make the replicated worked-ground zones match the host's list, index for index. Only what
	 * changed is marked dirty, so a new or grown zone sends itself and nothing else; every client, current and future, receives the list.
	 */
	void SyncTerrainZones(const TArray<FMOTerrainModifiedZone>& Zones);

	const TArray<FMOReplicatedTerrainZone>& GetTerrainZones() const { return TerrainZones.Items; }

	/** Whether the host is in developer mode (FMODevMode::IsHostInDevMode): the host refreshes it every couple of seconds (an Options toggle). */
	bool IsHostDeveloperMode() const { return bHostDeveloperMode; }

	virtual void BeginPlay() override;

	/** What the host published. bPublished is false until then (and always on a menu map). */
	const FMOWorldSeedInfo& GetWorldSeedInfo() const { return WorldSeed; }

	/** Authority only. Replicates to every connected client and to any client that joins later. */
	void SetWorldSeedInfo(const FMOWorldSeedInfo& NewInfo);

	/** The host's game clock / weather as last published (see MOWorldSyncTypes.h). */
	const FMOWorldClockInfo& GetWorldClockInfo() const { return WorldClock; }
	const FMOWorldWeatherInfo& GetWorldWeatherInfo() const { return WorldWeather; }

	/** Authority only. Replicates to every client, current and future (a mid-day joiner gets the host's time on arrival). */
	void SetWorldClockInfo(const FMOWorldClockInfo& NewInfo);
	void SetWorldWeatherInfo(const FMOWorldWeatherInfo& NewInfo);

	/** The open ground's wetness / snow cover as the host advances it (BR18-16; UMOWeatherIntegrationSubsystem). Authority sets it; it changes slowly
	 *  (minutes to hours), so only steps of GroundWeatherReplicationStep are sent. */
	const FMOGroundWeatherState& GetGroundWeather() const { return GroundWeather; }
	void SetGroundWeather(const FMOGroundWeatherState& NewState);
	static constexpr float GroundWeatherReplicationStep = 0.004f;

	/** What this world was created with (MOWorldCreationTypes.h): its pace is read on every machine through FMOWorldRules, its shape and start are shown
	 *  in bug reports. The shape that GENERATES the land travels with the seed (FMOWorldSeedInfo::Shape). */
	const FMOWorldCreationSettings& GetWorldCreation() const { return WorldCreation; }
	const FMOWorldPace& GetWorldPace() const { return WorldCreation.Pace; }

	/** Authority only: a new game's choice or a loaded save's. */
	void SetWorldCreation(const FMOWorldCreationSettings& NewSettings);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_WorldClock)
	FMOWorldClockInfo WorldClock;

	UPROPERTY(ReplicatedUsing = OnRep_WorldWeather)
	FMOWorldWeatherInfo WorldWeather;

	/** Client: hand the host's clock / weather to UMOWorldSyncSubsystem, which makes the local clock and sky follow. */
	UFUNCTION()
	void OnRep_WorldClock();

	UFUNCTION()
	void OnRep_WorldWeather();

	UPROPERTY(ReplicatedUsing = OnRep_WorldSeed)
	FMOWorldSeedInfo WorldSeed;

	/** World instances removed for good (MOWorldRemovalTypes.h). FastArray: each new entry is sent once; a late joiner receives the whole record. */
	UPROPERTY(Replicated)
	FMORemovedInstanceArray RemovedInstances;

	/** Worked ground (MOTerrainModificationTypes.h): every client sweeps the host's zones on its own PCG copy. */
	UPROPERTY(Replicated)
	FMOReplicatedTerrainZoneArray TerrainZones;

	UPROPERTY(Replicated)
	bool bHostDeveloperMode = false;

	UPROPERTY(Replicated)
	FMOGroundWeatherState GroundWeather;

	UPROPERTY(Replicated)
	FMOWorldCreationSettings WorldCreation;

	FTimerHandle DevModeRefreshHandle;

	/** Client: hand the host's seed to UMOWorldSeedSubsystem, which regenerates the local terrain from it. */
	UFUNCTION()
	void OnRep_WorldSeed();
};
