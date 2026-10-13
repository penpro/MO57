/**
 * MOGameState.h - Replicated, world-wide match state
 *
 * Today this carries exactly one thing: the world seed (see MOWorldSeedTypes.h for why). It is
 * the engine's canonical place for state every client needs about the world itself, so future
 * world-wide replicated facts belong here rather than on a bespoke always-relevant actor.
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

	/** Client: hand the host's seed to UMOWorldSeedSubsystem, which regenerates the local terrain from it. */
	UFUNCTION()
	void OnRep_WorldSeed();
};
