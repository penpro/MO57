/**
 * MOGeologyYield.h - what digging yields, from the world's geology
 *
 * Wes, Oct 17 2026: "mining yields from the geology ... they should be able to collect pretty much any ore". A dug volume is split by what the generator says
 * is there (FMOWorldGen::SampleDig): the soil over the bedrock (earth), the bedrock's own stone, and any ore body the dig cuts (a coal seam, a hematite
 * fissure, a copper or tin vein, a chert bed). ONE map from geology to items (ItemFor); every dig path -- the player's dig tool, the survivors' excavation
 * job -- yields through ForDig, so the same ground gives the same things to whoever digs it.
 *
 * Fill: earth and rubble can raise ground again (the excavation design's conservation: dig here, fill there); ore is kept.
 *
 * Time: what is dug also decides how long it takes (Wes, Oct 17 2026: "yes, different times for different rocks/minerals"). HardnessFor gives each material
 * its multiple of the earth rate; DigTimeMultiplier averages it over the same points ForDig shares its yield over, and both dig paths scale their duration
 * by it. Evidence and the table: Docs/Technology_History.md, "Digging time by what is dug".
 */
#pragma once

#include "CoreMinimal.h"
#include "MOWorldGen.h"

class UMOInventoryComponent;

/** Items one dig yields of one kind. */
struct FMOItemYield
{
	FName ItemId;
	int32 Count = 0;
	/** Earth or rubble: usable as fill (raises ground at a fill zone). Ore is not. */
	bool bFill = false;
};

struct MOFRAMEWORK_API FMOGeologyYield
{
	/** The item a dug point becomes (SoilItemId for soil: the excavation job's configured spoil, Dirt01 by default). Also returns whether it is fill. */
	static FName ItemFor(const FMODigSample& Sample, FName SoilItemId, bool& bOutFill);

	/**
	 * Splits VolumeM3 dug around Center into items: floor(VolumeM3 x ItemsPerCubicMeter) in all, shared by what deterministic points of the dug region are
	 * (SampleDig). bSlab: a height dig -- a slab under a disc of RadiusCm, as deep as the volume over the disc's area; otherwise a volume dig -- the half of a
	 * sphere of RadiusCm behind Center (a pick into a wall).
	 */
	static void ForDig(const FMOWorldGenParams& Params, int32 Seed, const FVector& Center, float RadiusCm, float VolumeM3, float ItemsPerCubicMeter, bool bSlab, FName SoilItemId,
		TArray<FMOItemYield>& Out);

	/** ForDig with the world's generator params and terrain seed. False (nothing out) when the world has no active seed. */
	static bool ForDigInWorld(const UObject* WorldContext, const FVector& Center, float RadiusCm, float VolumeM3, float ItemsPerCubicMeter, bool bSlab, FName SoilItemId,
		TArray<FMOItemYield>& Out);

	/** Puts the yields into Inventory as far as it has room; the rest is dropped at DropLocation (never lost, never a hidden entry). Returns the number dropped.
	 *  Inventory null: everything is dropped (WorldContext gives the world then). */
	static int32 GiveOrDrop(UMOInventoryComponent* Inventory, const TArray<FMOItemYield>& Yields, const FVector& DropLocation, const UObject* WorldContext = nullptr);

	/** One line for a log: "12 Dirt01, 3 Limestone01, 1 IronOre01". */
	static FString Describe(const TArray<FMOItemYield>& Yields);

	/**
	 * How many times longer than earth a cubic metre of this takes to dig with hand tools (soil 1): soft coal 1.5, the shales and sandstones of the coal
	 * measures 3, limestone 5 (Neolithic chalk mining ran ~6x earth, hauling included), granite and basalt 40 (before iron they were worked by fire-setting,
	 * ~40-60x). An ore vein by its own mineral: galena 3, hematite 8, chert 10, copper 15, cassiterite 40.
	 */
	static float HardnessFor(const FMODigSample& Sample);

	/** The dug region's dig-time multiplier: the mean HardnessFor over the solid points ForDig would share its yield over (1 when every point is in the air --
	 *  ground the player raised or already dug out is loose earth). */
	static float DigTimeMultiplier(const FMOWorldGenParams& Params, int32 Seed, const FVector& Center, float RadiusCm, float VolumeM3, bool bSlab);

	/** DigTimeMultiplier with the world's params and terrain seed; 1 when the world has no active seed. */
	static float DigTimeMultiplierInWorld(const UObject* WorldContext, const FVector& Center, float RadiusCm, float VolumeM3, bool bSlab);

private:
	/** What the dug region holds: SampleDig at its deterministic points that are solid (not above the unworked ground, not in cave air). */
	static void SampleDugRegion(const FMOWorldGenParams& Params, int32 Seed, const FVector& Center, float RadiusCm, float VolumeM3, bool bSlab, TArray<FMODigSample>& OutSolid);
};
