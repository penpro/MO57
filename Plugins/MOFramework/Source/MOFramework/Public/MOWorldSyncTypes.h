/**
 * MOWorldSyncTypes.h - Host-owned world state that every co-op client must follow: the game clock and the weather preset
 *
 * WHY THIS EXISTS:
 * The game clock (UMOGameClockSubsystem) and the weather (UMOWeatherIntegrationSubsystem -> the UDS bridge) are world subsystems:
 * one per MACHINE, advancing on their own. A joining client therefore started its own 08:00 and ran its own sky, so a co-op
 * client could be in daylight while the host's world was at night, with different weather. The host's values live on AMOGameState
 * (replicated) and UMOWorldSyncSubsystem applies them on clients.
 */

#pragma once

#include "CoreMinimal.h"
#include "MOWorldSyncTypes.generated.h"

/** The host's clock at one instant. A client advances locally between updates and only snaps if it has drifted. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWorldClockInfo
{
	GENERATED_BODY()

	/** False until the host has published (a client must not touch its own clock before then). */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Clock")
	bool bPublished = false;

	/** The host's FDateTime::GetTicks() for the in-game date/time at ServerTimeSeconds. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Clock")
	int64 GameDateTimeTicks = 0;

	/** In-game seconds per real second on the host (a skip is a jump in DateTime, not a TimeScale change). */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Clock")
	float TimeScale = 1.0f;

	/** AGameStateBase::GetServerWorldTimeSeconds() when sampled: lets a client account for how old this sample is. */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Clock")
	double ServerTimeSeconds = 0.0;
};

/** The weather preset the host's sky is showing (a UDS_Weather_Settings data asset). Each machine runs its own transition. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWorldWeatherInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MO|Weather")
	bool bPublished = false;

	UPROPERTY(BlueprintReadOnly, Category = "MO|Weather")
	FSoftObjectPath PresetPath;

	bool operator==(const FMOWorldWeatherInfo& Other) const
	{
		return bPublished == Other.bPublished && PresetPath == Other.PresetPath;
	}
	bool operator!=(const FMOWorldWeatherInfo& Other) const { return !(*this == Other); }
};
