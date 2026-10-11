/**
 * MOWeatherDirector.cpp - Weather that changes by itself (see the header)
 */

#include "MOWeatherDirector.h"
#include "MOFramework.h"
#include "MOGameClockSubsystem.h"
#include "MOGameMode.h"
#include "MOWeatherIntegrationSubsystem.h"
#include "MOWorldSeedSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarWeatherDirector(TEXT("MO.Weather.Director"), 1, TEXT("1 = the weather changes by itself (fronts come and go); 0 = it stays as it is."), ECVF_Default);

	// The eight base states, in the column order of the table below. Snow states are the cold twins of the wet ones.
	enum EBase : int32 { Clear, Partly, Cloudy, Overcast, Foggy, RainLight, Rain, Storm, NumBase };

	struct FPresetInfo
	{
		const TCHAR* Name;
		EBase Base;
		bool bSnow;
		float MinHours, MaxHours;
	};

	const FPresetInfo Presets[] = {
		{ TEXT("Clear_Skies"), Clear, false, 4.0f, 10.0f },
		{ TEXT("Partly_Cloudy"), Partly, false, 2.0f, 6.0f },
		{ TEXT("Cloudy"), Cloudy, false, 2.0f, 5.0f },
		{ TEXT("Overcast"), Overcast, false, 2.0f, 6.0f },
		{ TEXT("Foggy"), Foggy, false, 1.0f, 3.5f },
		{ TEXT("Rain_Light"), RainLight, false, 1.5f, 4.0f },
		{ TEXT("Rain"), Rain, false, 1.0f, 4.0f },
		{ TEXT("Rain_Thunderstorm"), Storm, false, 0.7f, 2.0f },
		{ TEXT("Snow_Light"), RainLight, true, 2.0f, 5.0f },
		{ TEXT("Snow"), Rain, true, 1.5f, 4.0f },
		{ TEXT("Snow_Blizzard"), Storm, true, 1.0f, 3.0f },
	};

	/** Row = the weather now, column = the next weather (percent). Snow states use the row of their wet twin. The wet columns are the "wet front" and become snow when it is cold. */
	constexpr int32 Chain[NumBase][NumBase] = {
		//        Clear Partly Cloudy Overcast Foggy RainL Rain Storm
		/* Clear    */ { 28, 40, 16, 5, 8, 3, 0, 0 },
		/* Partly   */ { 22, 28, 28, 10, 2, 8, 2, 0 },
		/* Cloudy   */ { 8, 22, 28, 22, 3, 12, 5, 0 },
		/* Overcast */ { 3, 8, 22, 30, 4, 18, 12, 3 },
		/* Foggy    */ { 30, 25, 15, 10, 15, 5, 0, 0 },
		/* RainL    */ { 6, 16, 25, 17, 4, 18, 12, 2 },
		/* Rain     */ { 2, 6, 14, 28, 2, 28, 12, 8 },
		/* Storm    */ { 2, 5, 12, 25, 2, 14, 40, 0 },
	};

	const FPresetInfo* Find(FName Name)
	{
		for (const FPresetInfo& P : Presets)
		{
			if (Name == FName(P.Name))
			{
				return &P;
			}
		}
		return nullptr;
	}

	const FPresetInfo& Pick(EBase Base, bool bSnow)
	{
		for (const FPresetInfo& P : Presets)
		{
			if (P.Base == Base && P.bSnow == bSnow)
			{
				return P;
			}
		}
		return Presets[0];
	}
}

bool UMOWeatherDirectorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	// Played worlds that own their weather: not the editor preview, not a client (it follows the replicated sky).
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client && Super::ShouldCreateSubsystem(Outer);
}

TStatId UMOWeatherDirectorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMOWeatherDirectorSubsystem, STATGROUP_Tickables);
}

bool UMOWeatherDirectorSubsystem::IsWet(FName Preset)
{
	const FPresetInfo* P = Find(Preset);
	return P && P->Base >= RainLight;
}

FName UMOWeatherDirectorSubsystem::ChooseNextPreset(FName Current, float TemperatureCelsius, FRandomStream& Rng)
{
	const FPresetInfo* From = Find(Current);
	const EBase Row = From ? From->Base : Clear;
	const bool bCurrentSnow = From && From->bSnow;

	// Cold enough for snow / warm enough for rain, with a band between where a wet front keeps the family it is in.
	const bool bSnow = TemperatureCelsius <= 0.5f || (TemperatureCelsius < 3.0f && bCurrentSnow);

	int32 Total = 0;
	int32 Weights[NumBase];
	for (int32 C = 0; C < NumBase; ++C)
	{
		Weights[C] = Chain[Row][C];
		Total += Weights[C];
	}
	int32 Roll = Rng.RandRange(0, Total - 1);
	for (int32 C = 0; C < NumBase; ++C)
	{
		if (Roll < Weights[C])
		{
			return FName(Pick((EBase)C, bSnow && C >= RainLight).Name);
		}
		Roll -= Weights[C];
	}
	return FName(Presets[0].Name);
}

float UMOWeatherDirectorSubsystem::ChooseSpellHours(FName Preset, FRandomStream& Rng)
{
	const FPresetInfo* P = Find(Preset);
	return P ? Rng.FRandRange(P->MinHours, P->MaxHours) : Rng.FRandRange(2.0f, 5.0f);
}

void UMOWeatherDirectorSubsystem::SetDirectorEnabled(bool bEnable)
{
	bEnabled = bEnable;
	bScheduled = false;   // re-adopt whatever the sky shows when switched back on
}

void UMOWeatherDirectorSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TimeSinceEvaluate += DeltaTime;
	if (TimeSinceEvaluate >= 5.0f)
	{
		TimeSinceEvaluate = 0.0f;
		Evaluate();
	}
}

void UMOWeatherDirectorSubsystem::Evaluate()
{
	UWorld* World = GetWorld();
	UMOWeatherIntegrationSubsystem* Weather = World ? World->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr;
	UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(World);
	// Only a real game with a real sky: the main menu world has a clock and a bridge too, but its GameMode is not the game's.
	if (!bEnabled || CVarWeatherDirector.GetValueOnGameThread() == 0 || !Weather || !Clock || !Weather->HasWeatherProvider() || !Cast<AMOGameMode>(World->GetAuthGameMode()))
	{
		return;
	}

	const FDateTime Now = Clock->GetGameDateTime();
	FRandomStream Rng;
	{
		const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(World);
		Rng.Initialize((Seeds && Seeds->HasActiveSeed() ? Seeds->GetActiveSeed() : 7) ^ (Changes * 7919 + 13));
	}

	const FString SkyNow = Weather->GetCurrentWeatherPresetPath().GetAssetName();
	if (!bScheduled)
	{
		// Adopt whatever the sky shows (clear skies at a new game, the saved weather after a load) and give it a first spell.
		bScheduled = true;
		LastAppliedAssetName = SkyNow;
		CurrentPreset = SkyNow.IsEmpty() ? FName(TEXT("Clear_Skies")) : FName(*SkyNow);
		NextChange = Now + FTimespan::FromHours(ChooseSpellHours(CurrentPreset, Rng));
		UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherDirector] holding %s until %s"), *CurrentPreset.ToString(), *NextChange.ToString());
		return;
	}

	if (!SkyNow.IsEmpty() && SkyNow != LastAppliedAssetName)
	{
		// Someone else changed the weather: respect it for a while.
		LastAppliedAssetName = SkyNow;
		CurrentPreset = FName(*SkyNow);
		NextChange = Now + FTimespan::FromHours(ManualHoldHours);
		UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherDirector] the weather was set to %s from outside: holding it until %s"), *SkyNow, *NextChange.ToString());
		return;
	}

	if (Now >= NextChange)
	{
		ApplyNext(Now);
	}
}

void UMOWeatherDirectorSubsystem::AdvanceNow()
{
	if (UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(GetWorld()))
	{
		ApplyNext(Clock->GetGameDateTime());
	}
}

void UMOWeatherDirectorSubsystem::ApplyNext(const FDateTime& Now)
{
	UWorld* World = GetWorld();
	UMOWeatherIntegrationSubsystem* Weather = World ? World->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr;
	if (!Weather)
	{
		return;
	}
	FRandomStream Rng;
	{
		const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(World);
		Rng.Initialize((Seeds && Seeds->HasActiveSeed() ? Seeds->GetActiveSeed() : 7) ^ ((Changes + 1) * 104729 + 31));
	}
	const FName Next = ChooseNextPreset(CurrentPreset, Weather->GetGlobalTemperature(EMOTemperatureUnit::Celsius), Rng);
	++Changes;
	const float Hours = ChooseSpellHours(Next, Rng);
	if (Weather->SetWeatherPresetByName(Next.ToString()))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOWeatherDirector] %s -> %s for %.1f game hours"), *CurrentPreset.ToString(), *Next.ToString(), Hours);
		CurrentPreset = Next;
		LastAppliedAssetName = Next.ToString();
	}
	NextChange = Now + FTimespan::FromHours(Hours);
	bScheduled = true;
}
