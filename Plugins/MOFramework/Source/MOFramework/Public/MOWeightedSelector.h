/**
 * =============================================================================
 * MOWeightedSelector.h - Weighted Random Selection Utility
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Utility class for weighted random selection. Consolidates duplicate weighted
 * selection logic from PCG nodes and subsystems.
 *
 * USAGE:
 *   TArray<FMyWeightedEntry> Items;
 *   float TotalWeight = FMOWeightedSelector::CalculateTotalWeight(Items);
 *   FRandomStream RandomStream(Seed);
 *   const FMyWeightedEntry* Selected = FMOWeightedSelector::SelectWeighted(Items, TotalWeight, RandomStream);
 *
 * REQUIRES: Entry types must have GetWeight() method returning float.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] NEGATIVE WEIGHTS: Negative weights are clamped to 0 in calculations.
 *   Use positive weights only.
 *
 * [2024-02] FLOATING POINT FALLBACK: SelectWeighted returns last entry on
 *   floating point edge cases where accumulated weight doesn't reach target.
 *
 * [2024-02] TEMPLATE REQUIREMENT: Entry type must implement GetWeight() const
 *   returning float. See FMOWeightedEntry for base implementation.
 *
 * =============================================================================
 * RELATED FILES: MOPCGItemSpawnerSettings.h, MOForagingSubsystem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOWeightedSelector.generated.h"

/**
 * Generic weighted selection entry interface.
 * Types using FMOWeightedSelector must implement GetWeight().
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWeightedEntry
{
	GENERATED_BODY()

	/** Weight for selection probability. Higher = more likely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0"))
	float Weight = 1.0f;

	float GetWeight() const { return Weight; }
};

/**
 * Utility class for weighted random selection.
 * Consolidates duplicate weighted selection logic from PCG nodes and subsystems.
 *
 * Usage:
 *   TArray<FMyWeightedEntry> Items;
 *   float TotalWeight = FMOWeightedSelector::CalculateTotalWeight(Items);
 *   FRandomStream RandomStream(Seed);
 *   const FMyWeightedEntry* Selected = FMOWeightedSelector::SelectWeighted(Items, TotalWeight, RandomStream);
 */
class MOFRAMEWORK_API FMOWeightedSelector
{
public:
	/**
	 * Calculate total weight of all entries.
	 * @param Entries Array of weighted entries (must have GetWeight() method)
	 * @return Total weight sum (negative weights clamped to 0)
	 */
	template<typename T>
	static float CalculateTotalWeight(const TArray<T>& Entries)
	{
		float TotalWeight = 0.0f;
		for (const T& Entry : Entries)
		{
			TotalWeight += FMath::Max(0.0f, Entry.GetWeight());
		}
		return TotalWeight;
	}

	/**
	 * Calculate total weight with validation predicate.
	 * Only includes entries that pass validation.
	 * @param Entries Array of weighted entries
	 * @param IsValid Predicate to check if entry should be counted
	 * @return Total weight sum of valid entries
	 */
	template<typename T, typename Predicate>
	static float CalculateTotalWeightIf(const TArray<T>& Entries, Predicate IsValid)
	{
		float TotalWeight = 0.0f;
		for (const T& Entry : Entries)
		{
			if (IsValid(Entry))
			{
				TotalWeight += FMath::Max(0.0f, Entry.GetWeight());
			}
		}
		return TotalWeight;
	}

	/**
	 * Select a random entry based on weights.
	 * @param Entries Array of weighted entries
	 * @param TotalWeight Pre-calculated total weight (for efficiency in loops)
	 * @param RandomStream Random stream for deterministic selection
	 * @return Pointer to selected entry, or nullptr if array is empty
	 */
	template<typename T>
	static const T* SelectWeighted(const TArray<T>& Entries, float TotalWeight, FRandomStream& RandomStream)
	{
		if (Entries.Num() == 0 || TotalWeight <= 0.0f)
		{
			return nullptr;
		}

		const float RandomValue = RandomStream.FRand() * TotalWeight;
		float AccumulatedWeight = 0.0f;

		for (const T& Entry : Entries)
		{
			AccumulatedWeight += FMath::Max(0.0f, Entry.GetWeight());
			if (RandomValue <= AccumulatedWeight)
			{
				return &Entry;
			}
		}

		// Fallback to last item (handles floating point edge cases)
		return &Entries.Last();
	}

	/**
	 * Select a random entry with validation predicate.
	 * @param Entries Array of weighted entries
	 * @param TotalWeight Pre-calculated total weight of valid entries
	 * @param RandomStream Random stream for deterministic selection
	 * @param IsValid Predicate to check if entry is valid for selection
	 * @return Pointer to selected entry, or nullptr if no valid entries
	 */
	template<typename T, typename Predicate>
	static const T* SelectWeightedIf(const TArray<T>& Entries, float TotalWeight, FRandomStream& RandomStream, Predicate IsValid)
	{
		if (Entries.Num() == 0 || TotalWeight <= 0.0f)
		{
			return nullptr;
		}

		const float RandomValue = RandomStream.FRand() * TotalWeight;
		float AccumulatedWeight = 0.0f;
		const T* LastValid = nullptr;

		for (const T& Entry : Entries)
		{
			if (!IsValid(Entry))
			{
				continue;
			}

			LastValid = &Entry;
			AccumulatedWeight += FMath::Max(0.0f, Entry.GetWeight());
			if (RandomValue <= AccumulatedWeight)
			{
				return &Entry;
			}
		}

		// Fallback to last valid item
		return LastValid;
	}

	/**
	 * Select using FMath::FRand() instead of a random stream.
	 * Use when deterministic selection is not required.
	 */
	template<typename T>
	static const T* SelectWeightedSimple(const TArray<T>& Entries)
	{
		const float TotalWeight = CalculateTotalWeight(Entries);
		if (Entries.Num() == 0 || TotalWeight <= 0.0f)
		{
			return nullptr;
		}

		const float RandomValue = FMath::FRand() * TotalWeight;
		float AccumulatedWeight = 0.0f;

		for (const T& Entry : Entries)
		{
			AccumulatedWeight += FMath::Max(0.0f, Entry.GetWeight());
			if (RandomValue <= AccumulatedWeight)
			{
				return &Entry;
			}
		}

		return &Entries.Last();
	}
};
