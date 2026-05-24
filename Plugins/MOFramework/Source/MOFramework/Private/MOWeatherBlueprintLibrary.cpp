#include "MOWeatherBlueprintLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"

namespace MOShelter
{
	// =========================================================================
	// OVERHEAD pattern (9 rays @ 50m)
	// 1 straight up + 8 upper-hemisphere at 35° from vertical.
	// 35° catches wind-tilted precipitation but keeps rays "overhead enough"
	// that a wall next to the actor doesn't false-positive as cover.
	//   cos(35°) ≈ 0.81915, sin(35°) ≈ 0.57358
	//   Ordinal sin component split √2: 0.57358/√2 ≈ 0.40558
	// =========================================================================
	static const TArray<FVector> OverheadDirs = {
		FVector(0.0f,      0.0f,      1.0f),       // Up
		FVector(0.0f,      0.57358f,  0.81915f),   // N
		FVector(0.57358f,  0.0f,      0.81915f),   // E
		FVector(0.0f,     -0.57358f,  0.81915f),   // S
		FVector(-0.57358f, 0.0f,      0.81915f),   // W
		FVector(0.40558f,  0.40558f,  0.81915f),   // NE
		FVector(0.40558f, -0.40558f,  0.81915f),   // SE
		FVector(-0.40558f,-0.40558f,  0.81915f),   // SW
		FVector(-0.40558f, 0.40558f,  0.81915f)    // NW
	};
	static constexpr float OverheadRange = 5000.0f;   // 50m

	// =========================================================================
	// WALL pattern (4 rays @ 3m, horizontal cardinal)
	// Detects close vertical surfaces for enclosure scoring.
	// =========================================================================
	static const TArray<FVector> WallDirs = {
		FVector(0.0f,  1.0f, 0.0f),    // N
		FVector(1.0f,  0.0f, 0.0f),    // E
		FVector(0.0f, -1.0f, 0.0f),    // S
		FVector(-1.0f, 0.0f, 0.0f)     // W
	};
	static constexpr float WallRange = 300.0f;        // 3m

	// =========================================================================
	// WIND cone (5 rays @ 15m, built dynamically toward upwind)
	// Distance-weighted: closer hit = better windbreak.
	// =========================================================================
	static constexpr float WindRange = 1500.0f;       // 15m
	static constexpr float WindConeAngleDeg = 15.0f;

	// =========================================================================
	// ENCLOSURE thresholds
	// "Close" range for an overhead hit to count as enclosure (treats a low
	// roof / ceiling as part of indoors).
	// =========================================================================
	static constexpr float IndoorCloseRange = 300.0f; // 3m

	/**
	 * Build the 5-ray upwind cone. UpwindHorizontal must be a horizontal unit
	 * vector pointing toward where the wind is coming FROM (i.e. -WindDirection
	 * with Z zeroed).
	 *
	 * Center ray plus 4 spread rays offset ±WindConeAngleDeg in horizontal
	 * (perpendicular) and vertical (world up) directions.
	 */
	static TArray<FVector> BuildWindConeRays(const FVector& UpwindHorizontal)
	{
		TArray<FVector> Out;
		Out.Reserve(5);
		Out.Add(UpwindHorizontal);

		// Perpendicular basis for spread offsets
		const FVector Right = FVector::CrossProduct(FVector::UpVector, UpwindHorizontal).GetSafeNormal();
		const FVector LocalUp = FVector::UpVector;

		const float Rad = FMath::DegreesToRadians(WindConeAngleDeg);
		const float SinA = FMath::Sin(Rad);
		const float CosA = FMath::Cos(Rad);

		// ±horizontal (sideways spread)
		Out.Add((UpwindHorizontal * CosA + Right * SinA).GetSafeNormal());
		Out.Add((UpwindHorizontal * CosA - Right * SinA).GetSafeNormal());
		// ±vertical (up/down spread to catch overhangs)
		Out.Add((UpwindHorizontal * CosA + LocalUp * SinA).GetSafeNormal());
		Out.Add((UpwindHorizontal * CosA - LocalUp * SinA).GetSafeNormal());

		return Out;
	}

	static UWorld* GetWorldFromContext(const UObject* WCO)
	{
		if (!GEngine || !WCO) return nullptr;
		return GEngine->GetWorldFromContextObject(WCO, EGetWorldErrorMode::LogAndReturnNull);
	}

	static FCollisionQueryParams MakeParams(AActor* IgnoreActor)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MOShelterTrace), /*bTraceComplex=*/false);
		Params.bReturnPhysicalMaterial = false;
		Params.bReturnFaceIndex = false;
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		return Params;
	}
}


FMOTimeOfDay UMOWeatherBlueprintLibrary::MakeTimeOfDayFromUDS(float TimeValue, float Season, bool bIsDaytime, int32 DayOfYear)
{
	return FMOTimeOfDay::FromUDSValues(TimeValue, Season, bIsDaytime, DayOfYear);
}

FMOWeatherState UMOWeatherBlueprintLibrary::MakeWeatherStateFromUDW(
	FText DisplayName,
	float CloudCoverage,
	float Fog,
	float Rain,
	float Snow,
	float WindIntensity,
	FRotator WindDirection,
	float Thunder)
{
	FMOWeatherState State;
	State.DisplayName = DisplayName;
	State.CloudCoverage = CloudCoverage;
	State.Fog = Fog;
	State.RainIntensity = Rain;
	State.SnowIntensity = Snow;
	State.WindIntensity = WindIntensity;
	State.WindDirection = WindDirection;
	State.ThunderIntensity = Thunder;
	return State;
}

FMOWeatherExposure UMOWeatherBlueprintLibrary::MakeWeatherExposureFromUDW(
	float Rain,
	float Snow,
	float Wind,
	float Dust,
	bool bIsSheltered)
{
	FMOWeatherExposure Exposure;
	Exposure.Rain = Rain;
	Exposure.Snow = Snow;
	Exposure.Wind = Wind;
	Exposure.Dust = Dust;
	Exposure.bIsSheltered = bIsSheltered;
	return Exposure;
}

float UMOWeatherBlueprintLibrary::DateTimeToUDSTimeValue(const FDateTime& DateTime)
{
	// UDS "Time of Day" format: HHMM where minute is scaled 0-99 over the
	// in-hour. Each in-game hour adds 100; each minute adds 100/60 ≈ 1.666...
	//
	// 06:00 → 600.0
	// 13:30 → 1350.0  (30 minutes is half an hour → +50)
	// 23:59 → ~2398.33
	//
	// Documented via UDS marketplace listing + community examples — confirmed
	// 0-2400 range. Going outside that range (e.g. negative) is undefined.
	const float Hour = static_cast<float>(DateTime.GetHour());
	const float Minute = static_cast<float>(DateTime.GetMinute());
	const float Second = static_cast<float>(DateTime.GetSecond());

	// Include seconds for finer granularity. Hour * 100 + (minute + sec/60) * (100/60).
	const float FractionalMinute = Minute + (Second / 60.0f);
	return (Hour * 100.0f) + (FractionalMinute * (100.0f / 60.0f));
}

int32 UMOWeatherBlueprintLibrary::DateTimeToDayOfYear(const FDateTime& DateTime)
{
	return DateTime.GetDayOfYear();
}

FMOWeatherExposure UMOWeatherBlueprintLibrary::MakeWeatherExposureFromUDWWithShelter(
	float Rain,
	float Snow,
	float Wind,
	float Dust,
	const FMOExposureShelter& Shelter)
{
	FMOWeatherExposure Result;

	const float OverBlock = FMath::Clamp(Shelter.OverheadCover, 0.0f, 1.0f);
	const float WindBlock = FMath::Clamp(Shelter.WindShelter,   0.0f, 1.0f);

	// Rain & snow: blocked by what's above you. A roof above + open sides
	// still keeps you dry under vertical rain.
	Result.Rain = Rain * (1.0f - OverBlock);
	Result.Snow = Snow * (1.0f - OverBlock);

	// Wind & dust: blocked by what's between you and upwind. A wall to the
	// north only matters if wind blows from the north.
	Result.Wind = Wind * (1.0f - WindBlock);
	Result.Dust = Dust * (1.0f - WindBlock);

	// Composite "sheltered" — either truly indoors, or both axes give meaningful
	// protection. Used by simple consumers that only care about a single bool.
	Result.bIsSheltered = Shelter.bIsIndoors
		|| (Shelter.bIsCovered && Shelter.bIsWindSheltered);

	return Result;
}

FMOExposureShelter UMOWeatherBlueprintLibrary::TestExposureShelter(
	const UObject* WorldContextObject,
	FVector Location,
	FVector WindDirection,
	AActor* IgnoreActor)
{
	FMOExposureShelter Result;
	Result.OverheadRayCount = MOShelter::OverheadDirs.Num();
	Result.WallRayCount     = MOShelter::WallDirs.Num();

	UWorld* World = MOShelter::GetWorldFromContext(WorldContextObject);
	if (!World)
	{
		return Result;
	}

	const FCollisionQueryParams Params = MOShelter::MakeParams(IgnoreActor);

	// --------------------------------------------------------------------
	// 1) Overhead — 9 long rays. Count all hits AND count hits within
	//    "close" range (those contribute to enclosure as a low roof / ceiling).
	// --------------------------------------------------------------------
	int32 OverheadCloseHits = 0;
	for (const FVector& Dir : MOShelter::OverheadDirs)
	{
		const FVector End = Location + Dir * MOShelter::OverheadRange;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Location, End, ECC_Visibility, Params))
		{
			++Result.OverheadHits;
			if (Hit.Distance <= MOShelter::IndoorCloseRange)
			{
				++OverheadCloseHits;
			}
		}
	}
	Result.OverheadCover = Result.OverheadRayCount > 0
		? static_cast<float>(Result.OverheadHits) / static_cast<float>(Result.OverheadRayCount)
		: 0.0f;

	// --------------------------------------------------------------------
	// 2) Walls — 4 short horizontal rays. Hits = close vertical surfaces.
	// --------------------------------------------------------------------
	for (const FVector& Dir : MOShelter::WallDirs)
	{
		const FVector End = Location + Dir * MOShelter::WallRange;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Location, End, ECC_Visibility, Params))
		{
			++Result.WallHits;
		}
	}

	// --------------------------------------------------------------------
	// 3) Wind cone — only if wind direction is meaningful (non-zero horizontal).
	//    Distance-weighted: a wall 1m upwind is a much better windbreak than
	//    a wall 14m upwind. Score per hit = 1 - (HitDistance / WindRange).
	// --------------------------------------------------------------------
	FVector Upwind = -WindDirection;
	Upwind.Z = 0.0f;
	if (Upwind.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		Upwind = Upwind.GetSafeNormal();
		const TArray<FVector> WindRays = MOShelter::BuildWindConeRays(Upwind);

		float WindScoreSum = 0.0f;
		for (const FVector& Dir : WindRays)
		{
			const FVector End = Location + Dir * MOShelter::WindRange;
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Location, End, ECC_Visibility, Params))
			{
				const float DistFrac = FMath::Clamp(Hit.Distance / MOShelter::WindRange, 0.0f, 1.0f);
				WindScoreSum += (1.0f - DistFrac);
			}
		}
		Result.WindShelter = WindRays.Num() > 0
			? WindScoreSum / static_cast<float>(WindRays.Num())
			: 0.0f;
	}

	// --------------------------------------------------------------------
	// Enclosure — combine close-overhead hits with wall hits, weighted toward
	// overhead slightly (roof matters more than walls for "indoors" feel).
	// --------------------------------------------------------------------
	const float OverheadCloseFrac = Result.OverheadRayCount > 0
		? static_cast<float>(OverheadCloseHits) / static_cast<float>(Result.OverheadRayCount)
		: 0.0f;
	const float WallFrac = Result.WallRayCount > 0
		? static_cast<float>(Result.WallHits) / static_cast<float>(Result.WallRayCount)
		: 0.0f;
	Result.Enclosure = (OverheadCloseFrac * 0.6f) + (WallFrac * 0.4f);

	// Convenience bools
	Result.bIsCovered        = Result.OverheadCover >= 0.5f;
	Result.bIsWindSheltered  = Result.WindShelter   >= 0.5f;
	// "Indoors" requires meaningful coverage on BOTH close-overhead and walls,
	// not just a high blended score (a wide open field with a low ceiling
	// shouldn't read as indoors).
	Result.bIsIndoors = (Result.Enclosure >= 0.85f)
		&& (OverheadCloseFrac >= 0.7f)
		&& (WallFrac >= 0.7f);

	return Result;
}

FMOExposureShelter UMOWeatherBlueprintLibrary::TestOverheadCover(
	const UObject* WorldContextObject,
	FVector Location,
	AActor* IgnoreActor)
{
	FMOExposureShelter Result;
	Result.OverheadRayCount = MOShelter::OverheadDirs.Num();

	UWorld* World = MOShelter::GetWorldFromContext(WorldContextObject);
	if (!World)
	{
		return Result;
	}

	const FCollisionQueryParams Params = MOShelter::MakeParams(IgnoreActor);

	for (const FVector& Dir : MOShelter::OverheadDirs)
	{
		const FVector End = Location + Dir * MOShelter::OverheadRange;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Location, End, ECC_Visibility, Params))
		{
			++Result.OverheadHits;
		}
	}

	Result.OverheadCover = Result.OverheadRayCount > 0
		? static_cast<float>(Result.OverheadHits) / static_cast<float>(Result.OverheadRayCount)
		: 0.0f;
	Result.bIsCovered = Result.OverheadCover >= 0.5f;

	return Result;
}
