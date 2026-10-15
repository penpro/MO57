// What a pawn can do with one item it carries beyond the fixed menu (Drop, Inspect ...): drink from a container, fill it at water, pour it out, boil it.
// ONE registry: the item context menu lists GetActions as buttons, AMOPlayerController::ServerItemAction runs Perform on the server, and a survivor pawn's
// job can call Perform directly. Adding an action = one entry in each function, no menu or controller change. Docs/Water_And_Illness.md (boiling).

#pragma once

#include "CoreMinimal.h"

class APawn;
class UMOInventoryComponent;

/** One action offered for an item: what the button says, and why it is greyed out if it is. */
struct FMOItemActionOption
{
	FName Id;
	FText Label;
	bool bEnabled = true;
	/** Shown as the disabled button's tooltip: what is missing (a fire, stones, water in reach). */
	FText DisabledReason;
};

struct MOFRAMEWORK_API FMOItemActions
{
	static const FName Drink;
	static const FName Fill;
	static const FName PourOut;
	static const FName BoilOverFire;
	static const FName BoilWithStones;
	/** The menu's long-standing "Use" (consume a consumable): routed through the server here too. */
	static const FName Use;

	/** The extra actions this pawn has for the item (any machine: what the menu shows). */
	static void GetActions(const APawn* Pawn, const UMOInventoryComponent* Inventory, const FGuid& ItemGuid, TArray<FMOItemActionOption>& Out);

	/** Do it (server): re-checks everything GetActions checked, then starts the pawn's timed action or applies it at once. False if refused. */
	static bool Perform(APawn* Pawn, const FGuid& ItemGuid, FName ActionId);

	/** Water the pawn can reach from where it stands (within the drinking reach), and where: what Fill draws from. */
	static bool FindReachableWater(const APawn* Pawn, FVector& OutPoint, uint8& OutKind);

	/** A fire burning within reach of the pawn (any registered local heat source that is lit). */
	static bool IsFireWithinReach(const APawn* Pawn);

	/** Stones usable for stone boiling the pawn carries (Stone, River Cobble ...). */
	static int32 CountBoilingStones(const UMOInventoryComponent* Inventory);
};
