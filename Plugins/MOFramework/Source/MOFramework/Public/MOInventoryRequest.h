// MOInventoryRequest.h -- the ONE door through which a player's UI changes an inventory.
//
// CO-OP (Steam playtest, Oct 16 2026: "multiplayer inventory isn't working, won't allow dropping or anything", "drop / pickup arrows don't work"): the
// inventory UI called the component's mutators (TransferItem, DropItemByGuid, SwapSlots, ...) directly. They are authority-only, so on a joiner every one
// of them refused and nothing happened. Every UI-originated change is now an FMOInventoryRequest:
//   Submit(PC, Request)  -- the UI's call. On the authority (host / single player) it executes at once; on a joiner it is sent to the server
//                           (AMOPlayerController::ServerInventoryRequest), which runs the same Execute.
//   Execute(PC, Request) -- SERVER: checks the player may reach every inventory the request names (CanAccess), then calls the component.
// Drop placement is decided on the server from the pawn (ComputeDropTransform): a client never names a world location.
// A new inventory gesture = a new EMOInventoryOp + its case in Execute; never a direct mutator call from a widget.

#pragma once

#include "CoreMinimal.h"
#include "MOInventoryRequest.generated.h"

class APawn;
class APlayerController;
class UMOInventoryComponent;
class AMOWorldItem;

UENUM()
enum class EMOInventoryOp : uint8
{
	Transfer,      // ItemGuid from Source to Target
	TransferAll,   // everything from Source to Target
	FillStacks,    // top up Target's stacks from Source
	Swap,          // SlotA <-> SlotB inside Source
	Drop,          // ItemGuid from Source to the ground in front of the pawn
	DropAll,       // every item of Source to the ground
	Split,         // halve ItemGuid's stack in Source (into a new stack, or onto the ground when there is no empty slot)
	Organize,      // stack and sort Source
	TakeHalfOfWorldItem,   // half of WorldItem's stack (on the ground) into Target
	Equip,         // ItemGuid from Source onto the requester's own pawn, slot EquipSlot (MAX = the item's natural slot)
	Unequip,       // the requester's EquipSlot back into Target
};

USTRUCT()
struct MOFRAMEWORK_API FMOInventoryRequest
{
	GENERATED_BODY()

	UPROPERTY()
	EMOInventoryOp Op = EMOInventoryOp::Transfer;

	/** The inventory acted on / taken from. */
	UPROPERTY()
	TObjectPtr<UMOInventoryComponent> Source = nullptr;

	/** Transfer / fill destination. */
	UPROPERTY()
	TObjectPtr<UMOInventoryComponent> Target = nullptr;

	UPROPERTY()
	FGuid ItemGuid;

	/** TakeHalfOfWorldItem: the item lying in the world. */
	UPROPERTY()
	TObjectPtr<AMOWorldItem> WorldItem = nullptr;

	UPROPERTY()
	int32 SlotA = INDEX_NONE;

	UPROPERTY()
	int32 SlotB = INDEX_NONE;

	/** Equip / Unequip: EMOEquipmentSlot as a byte (MAX = natural slot). */
	UPROPERTY()
	uint8 EquipSlot = 0;

	static FMOInventoryRequest Make(EMOInventoryOp InOp, UMOInventoryComponent* InSource, UMOInventoryComponent* InTarget = nullptr, const FGuid& InItem = FGuid())
	{
		FMOInventoryRequest R;
		R.Op = InOp;
		R.Source = InSource;
		R.Target = InTarget;
		R.ItemGuid = InItem;
		return R;
	}
};

struct MOFRAMEWORK_API FMOInventoryRequests
{
	/** The UI's call: executes on the authority, otherwise sends the request to the server. True when it was executed / sent (not that it succeeded on the server). */
	static bool Submit(APlayerController* PC, const FMOInventoryRequest& Request);

	/** SERVER: validate and execute. True on success. */
	static bool Execute(APlayerController* PC, const FMOInventoryRequest& Request);

	/** May this player's pawn change this inventory: its own, or one whose owner it can reach (a container, a station, a carcass) that no OTHER player is driving. */
	static bool CanAccess(const APawn* Pawn, const UMOInventoryComponent* Inventory);

	/** Where an item dropped by this pawn lands: in front of it, on the ground under that spot (30 cm above it), random yaw. */
	static FTransform ComputeDropTransform(const APawn* Pawn);

	/** Reach for another actor's inventory (pawn to the owner's bounds), cm. */
	static constexpr float InventoryReachCm = 600.0f;
};
