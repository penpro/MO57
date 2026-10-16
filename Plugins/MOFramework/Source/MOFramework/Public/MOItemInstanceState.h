// What one particular item carries beyond its definition and count: how worn it is, what is in it. It travels with the item wherever the item goes --
// inventory to inventory, into a save, onto the ground and back -- so a half-worn axe stays half worn and a container keeps its water.

#pragma once

#include "CoreMinimal.h"
#include "MOWorldGen.h"
#include "MOItemInstanceState.generated.h"

/** The liquid in a container item (UMOItemDefinitionRow::LiquidCapacityMl > 0). Water only, for now; its source decides what drinking it does. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOLiquidContents
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Liquid")
	float Millilitres = 0.0f;

	/** Dissolved salts (g/L): boiling does not remove them (it concentrates them). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Liquid")
	float SalinityGPerL = 0.0f;

	/** Chance per litre drunk of a waterborne illness: the source's while untreated, 0 once boiled. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Liquid")
	float IllnessChancePerLitre = 0.0f;

	/** Where the water was drawn (its name in the UI). Mixed sources keep the last one's. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Liquid")
	EMOWaterBodyKind Source = EMOWaterBodyKind::None;

	/** Brought to a boil since it was last topped up with untreated water. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Liquid")
	bool bBoiled = false;

	bool IsEmpty() const { return Millilitres < 1.0f; }

	/** Fresh: below about 1 g/L of dissolved salts (rivers and lakes are well under; the sea is 35). */
	static constexpr float FreshMaxSalinityGPerL = 1.0f;

	/** Whether it can serve as a recipe's measure of water: any water, or (bNeedsSafe) only boiled, fresh water -- boiling does not take salt out. */
	bool IsSuitableMeasure(bool bNeedsSafe) const
	{
		return !IsEmpty() && (!bNeedsSafe || (bBoiled && SalinityGPerL <= FreshMaxSalinityGPerL));
	}

	/** Pour Ml of another liquid in: volumes add, salts and illness chance mix by volume; untreated water spoils "boiled". */
	void Mix(const FMOLiquidContents& Other)
	{
		const float Total = Millilitres + Other.Millilitres;
		if (Total <= 0.0f)
		{
			return;
		}
		SalinityGPerL = (SalinityGPerL * Millilitres + Other.SalinityGPerL * Other.Millilitres) / Total;
		IllnessChancePerLitre = (IllnessChancePerLitre * Millilitres + Other.IllnessChancePerLitre * Other.Millilitres) / Total;
		bBoiled = (IsEmpty() || bBoiled) && Other.bBoiled;
		Source = Other.Source;
		Millilitres = Total;
	}

	bool operator==(const FMOLiquidContents& O) const
	{
		return Millilitres == O.Millilitres && SalinityGPerL == O.SalinityGPerL && IllnessChancePerLitre == O.IllnessChancePerLitre && Source == O.Source && bBoiled == O.bBoiled;
	}
};

/** Everything per-item an item takes with it out of an inventory (a drop, a transfer) and back in (a pickup). */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOItemInstanceState
{
	GENERATED_BODY()

	/** -1 = not applicable / the definition's maximum (a fresh item). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Item")
	int32 Durability = -1;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Item")
	FMOLiquidContents Liquid;

	/** Nothing a fresh item of its definition would not have. */
	bool IsDefault() const { return Durability < 0 && Liquid.IsEmpty(); }
};
