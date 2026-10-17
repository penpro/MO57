/**
 * MOGeologyYield.h - what digging yields, from the world's geology
 *
 * Wes, Oct 17 2026: "mining yields from the geology ... they should be able to collect pretty much any ore". A dug volume is split by what the generator says
 * is there (FMOWorldGen::SampleDig): the soil over the bedrock (earth), the bedrock's own stone, and any ore body the dig cuts (a coal seam, a hematite
 * fissure, a copper or tin vein, a chert bed). ONE map from geology to items (ItemFor); every dig path -- the player's dig tool, the survivors' excavation
 * job -- yields through ForDig, so the same ground gives the same things to whoever digs it.
 *
 * Fill: earth and rubble can raise ground again (the excavation design's conservation: dig here, fill there); ore is kept.
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
};
