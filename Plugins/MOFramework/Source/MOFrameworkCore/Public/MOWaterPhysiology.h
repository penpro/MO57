// What a draught of water does to a body, and the shape of a self-limiting illness: the pure numbers behind drinking (Docs/Water_And_Illness.md).
// Headless and deterministic, so the tests pin them; the components that apply them live above (survival stats, anatomy).

#pragma once

#include "CoreMinimal.h"

struct FMOWaterPhysiology   // header-only (every function inline): no export, each module compiles its own copy
{
	/** Sea water's salinity (g of salts per litre). */
	static constexpr float SeaSalinityGPerL = 35.0f;

	/**
	 * Litres of urine the kidneys spend clearing the salts of one litre of sea water. Sea water (~1,100 mOsm, ~470 mmol Na/L) is saltier than the kidney can
	 * concentrate urine, so it pays in body water: ~1.5 L per litre drunk -- a net loss of about half a litre.
	 */
	static constexpr float UrineLitresPerSeaLitre = 1.5f;

	/** Sodium in sea water: ~10.8 g per litre at 35 g/L. */
	static constexpr float SodiumGramsPerSeaLitre = 10.8f;

	/** An adult's daily sodium need (g): the metabolism's Sodium 100 %. */
	static constexpr float DailySodiumGrams = 1.5f;

	/** Body water a draught leaves behind once the kidneys have cleared its salt (ml; negative for anything as salty as the sea). */
	static float NetBodyWaterMl(float Millilitres, float SalinityGPerL)
	{
		return Millilitres * (1.0f - UrineLitresPerSeaLitre * FMath::Max(0.0f, SalinityGPerL) / SeaSalinityGPerL);
	}

	/** The sodium a draught carries, as % of a day's need (the metabolism's unit). */
	static float SodiumPercentOfDailyNeed(float Millilitres, float SalinityGPerL)
	{
		return Millilitres / 1000.0f * SodiumGramsPerSeaLitre * FMath::Max(0.0f, SalinityGPerL) / SeaSalinityGPerL / DailySodiumGrams * 100.0f;
	}

	/** The chance a draught of this volume makes you ill, from the source's chance per litre (independent litres: 1 - (1 - p)^litres). */
	static float IllnessChance(float Millilitres, float ChancePerLitre)
	{
		const float P = FMath::Clamp(ChancePerLitre, 0.0f, 1.0f);
		return Millilitres <= 0.0f || P <= 0.0f ? 0.0f : 1.0f - FMath::Pow(1.0f - P, Millilitres / 1000.0f);
	}

	/**
	 * Severity of a self-limiting illness at Elapsed seconds into a course of CourseSeconds: rises to Peak over the first third, then falls back to zero at the end (the
	 * usual shape of an acute gastroenteritis: a day or two of the worst, then a slow mend). 0 before the course and after it.
	 */
	static float CourseSeverity(float ElapsedSeconds, float CourseSeconds, float Peak)
	{
		if (CourseSeconds <= 0.0f || ElapsedSeconds <= 0.0f || ElapsedSeconds >= CourseSeconds)
		{
			return 0.0f;
		}
		const float T = ElapsedSeconds / CourseSeconds;
		const float Shape = T < 1.0f / 3.0f ? T * 3.0f : (1.0f - T) * 1.5f;
		return Peak * FMath::Clamp(Shape, 0.0f, 1.0f);
	}
};
