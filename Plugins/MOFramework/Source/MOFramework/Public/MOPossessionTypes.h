#pragma once

#include "CoreMinimal.h"
#include "MOworldSaveGame.h"
#include "MOPossessionTypes.generated.h"

/**
 * One row of the possession menu, slimmed down to what the menu displays so it can travel in an RPC.
 * (FMOPersistedPawnRecord carries every component's full save state -- far too heavy to send per row.)
 * A remote client's possession menu is filled from the SERVER's pawns, never from the client's own local saves.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOPossessionListEntry
{
	GENERATED_BODY()

	UPROPERTY() FGuid PawnGuid;
	UPROPERTY() FString CharacterName;
	UPROPERTY() FString Gender;
	UPROPERTY() int32 AgeInDays = 0;
	UPROPERTY() bool bIsDeceased = false;
	UPROPERTY() float HealthPercent = 1.0f;
	UPROPERTY() FString StatusText;
	UPROPERTY() FString LocationName;
	UPROPERTY() int64 LastPlayedTicks = 0;

	static FMOPossessionListEntry FromRecord(const FMOPersistedPawnRecord& Record)
	{
		FMOPossessionListEntry E;
		E.PawnGuid = Record.PawnGuid;
		E.CharacterName = Record.CharacterName;
		E.Gender = Record.Gender;
		E.AgeInDays = Record.AgeInDays;
		E.bIsDeceased = Record.bIsDeceased;
		E.HealthPercent = Record.HealthPercent;
		E.StatusText = Record.StatusText;
		E.LocationName = Record.LocationName;
		E.LastPlayedTicks = Record.LastPlayedTime.GetTicks();
		return E;
	}

	/** A record holding only the display fields -- all the pawn entry widgets read. */
	FMOPersistedPawnRecord ToDisplayRecord() const
	{
		FMOPersistedPawnRecord R;
		R.PawnGuid = PawnGuid;
		R.CharacterName = CharacterName;
		R.Gender = Gender;
		R.AgeInDays = AgeInDays;
		R.bIsDeceased = bIsDeceased;
		R.HealthPercent = HealthPercent;
		R.StatusText = StatusText;
		R.LocationName = LocationName;
		R.LastPlayedTime = FDateTime(LastPlayedTicks);
		R.bIsPlayerControllable = true;
		return R;
	}
};
