#include "MOInventoryRequest.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOItemComponent.h"
#include "MOEquipmentComponent.h"
#include "MOPlayerController.h"
#include "MOWorldItem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const TCHAR* OpName(EMOInventoryOp Op)
	{
		switch (Op)
		{
		case EMOInventoryOp::Transfer:            return TEXT("Transfer");
		case EMOInventoryOp::TransferAll:         return TEXT("TransferAll");
		case EMOInventoryOp::FillStacks:          return TEXT("FillStacks");
		case EMOInventoryOp::Swap:                return TEXT("Swap");
		case EMOInventoryOp::Drop:                return TEXT("Drop");
		case EMOInventoryOp::DropAll:             return TEXT("DropAll");
		case EMOInventoryOp::Split:               return TEXT("Split");
		case EMOInventoryOp::Organize:            return TEXT("Organize");
		case EMOInventoryOp::TakeHalfOfWorldItem: return TEXT("TakeHalfOfWorldItem");
		case EMOInventoryOp::Equip:               return TEXT("Equip");
		case EMOInventoryOp::Unequip:             return TEXT("Unequip");
		default:                                  return TEXT("?");
		}
	}

	AActor* DropOne(UMOInventoryComponent* Inventory, const FGuid& ItemGuid, const FTransform& At)
	{
		AActor* Dropped = Inventory->DropItemByGuid(ItemGuid, At.GetLocation(), At.Rotator());
		if (AMOWorldItem* WorldItem = Cast<AMOWorldItem>(Dropped))
		{
			WorldItem->EnableDropPhysics();
		}
		return Dropped;
	}
}

bool FMOInventoryRequests::Submit(APlayerController* PC, const FMOInventoryRequest& Request)
{
	if (!IsValid(PC))
	{
		return false;
	}
	if (PC->HasAuthority())
	{
		return Execute(PC, Request);   // host / single player: no round trip
	}
	if (AMOPlayerController* MOPC = Cast<AMOPlayerController>(PC))
	{
		MOPC->ServerInventoryRequest(Request);
		return true;   // the result arrives by replication (the inventory's FastArray), the UI refreshes on it
	}
	return false;
}

bool FMOInventoryRequests::CanAccess(const APawn* Pawn, const UMOInventoryComponent* Inventory)
{
	if (!IsValid(Pawn) || !IsValid(Inventory))
	{
		return false;
	}
	const AActor* Owner = Inventory->GetOwner();
	if (!IsValid(Owner))
	{
		return false;
	}
	if (Owner == Pawn)
	{
		return true;
	}
	if (const APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		if (OwnerPawn->IsPlayerControlled())
		{
			return false;   // another player's character carries their things
		}
	}
	FVector Origin = Owner->GetActorLocation();
	FVector Extent = FVector::ZeroVector;
	Owner->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent);
	const float Gap = FMath::Max(0.0f, (float)FVector::Dist(Pawn->GetActorLocation(), Origin) - (float)Extent.Size());
	return Gap <= InventoryReachCm;
}

FTransform FMOInventoryRequests::ComputeDropTransform(const APawn* Pawn)
{
	FRotator Facing = Pawn->GetActorRotation();
	Facing.Pitch = 0.0f;
	Facing.Roll = 0.0f;
	const FVector Forward = Facing.Vector();
	const FVector Right = FRotationMatrix(Facing).GetScaledAxis(EAxis::Y);
	FVector Location = Pawn->GetActorLocation() + Forward * FMath::RandRange(150.0f, 250.0f) + Right * FMath::RandRange(-50.0f, 50.0f);

	if (UWorld* World = Pawn->GetWorld())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MOInventoryDrop), /*bTraceComplex=*/false, Pawn);
		FHitResult Hit;
		const FVector Start = Location + FVector(0.0f, 0.0f, 200.0f);
		const FVector End = Location - FVector(0.0f, 0.0f, 500.0f);
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params) || World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			Location = Hit.Location + FVector(0.0f, 0.0f, 30.0f);   // high enough not to clip, low enough for the drop physics to settle it
		}
		else
		{
			Location.Z = Pawn->GetActorLocation().Z;
		}
	}
	return FTransform(FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f), Location);
}

bool FMOInventoryRequests::Execute(APlayerController* PC, const FMOInventoryRequest& R)
{
	APawn* Pawn = IsValid(PC) ? PC->GetPawn() : nullptr;
	if (!Pawn || !PC->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryReq] %s refused: no pawn or no authority"), OpName(R.Op));
		return false;
	}
	UMOInventoryComponent* Source = R.Source.Get();
	UMOInventoryComponent* Target = R.Target.Get();
	const bool bNeedsSource = R.Op != EMOInventoryOp::TakeHalfOfWorldItem && R.Op != EMOInventoryOp::Unequip;
	const bool bNeedsTarget = R.Op == EMOInventoryOp::Transfer || R.Op == EMOInventoryOp::TransferAll || R.Op == EMOInventoryOp::FillStacks || R.Op == EMOInventoryOp::TakeHalfOfWorldItem
		|| R.Op == EMOInventoryOp::Unequip;
	if ((bNeedsSource && !CanAccess(Pawn, Source)) || (bNeedsTarget && !CanAccess(Pawn, Target)))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInventoryReq] %s refused for %s: an inventory it names is out of reach or someone else's (source %s, target %s)"),
			OpName(R.Op), *PC->GetName(), *GetNameSafe(Source ? Source->GetOwner() : nullptr), *GetNameSafe(Target ? Target->GetOwner() : nullptr));
		return false;
	}

	bool bOk = false;
	int32 Count = 0;
	switch (R.Op)
	{
	case EMOInventoryOp::Transfer:
		bOk = Source->TransferItem(R.ItemGuid, Target);
		Count = bOk ? 1 : 0;
		break;
	case EMOInventoryOp::TransferAll:
		Count = Source->TransferAllTo(Target);
		bOk = Count > 0;
		break;
	case EMOInventoryOp::FillStacks:
		Count = Target->FillStacksFrom(Source);
		bOk = Count > 0;
		break;
	case EMOInventoryOp::Swap:
		bOk = Source->SwapSlots(R.SlotA, R.SlotB);
		break;
	case EMOInventoryOp::Drop:
		bOk = DropOne(Source, R.ItemGuid, ComputeDropTransform(Pawn)) != nullptr;
		Count = bOk ? 1 : 0;
		break;
	case EMOInventoryOp::DropAll:
		for (const FGuid& Guid : Source->GetAllItemGuids())   // a copy: dropping changes the inventory
		{
			Count += DropOne(Source, Guid, ComputeDropTransform(Pawn)) ? 1 : 0;
		}
		bOk = Count > 0;
		break;
	case EMOInventoryOp::Split:
	{
		FMOInventoryEntry Entry;
		if (!Source->TryGetEntryByGuid(R.ItemGuid, Entry) || Entry.Quantity <= 1)
		{
			break;   // nothing to split
		}
		const int32 SplitAmount = Entry.Quantity / 2;
		if (!Source->RemoveItemByGuid(R.ItemGuid, SplitAmount))
		{
			break;
		}
		const FGuid NewGuid = FGuid::NewGuid();
		if (Source->HasEmptySlot())
		{
			bOk = Source->AddItemByGuidNoStack(NewGuid, Entry.ItemDefinitionId, SplitAmount);   // NoStack: it must not merge straight back
		}
		else
		{
			const FTransform At = ComputeDropTransform(Pawn);   // no room: the split half goes on the ground
			bOk = Source->SpawnWorldItem(Entry.ItemDefinitionId, SplitAmount, NewGuid, At.GetLocation(), At.Rotator()) != nullptr;
		}
		Count = bOk ? SplitAmount : 0;
		break;
	}
	case EMOInventoryOp::Organize:
		Source->StackAndOrganize();
		bOk = true;
		break;
	case EMOInventoryOp::TakeHalfOfWorldItem:
	{
		AMOWorldItem* WorldItem = R.WorldItem.Get();
		UMOItemComponent* ItemComp = IsValid(WorldItem) ? WorldItem->GetItemComponent() : nullptr;
		FVector Origin = FVector::ZeroVector;
		FVector Extent = FVector::ZeroVector;
		if (IsValid(WorldItem))
		{
			WorldItem->GetActorBounds(false, Origin, Extent);
		}
		if (!ItemComp || ItemComp->Quantity <= 1 || FVector::Dist(Pawn->GetActorLocation(), Origin) - Extent.Size() > InventoryReachCm)
		{
			break;
		}
		const int32 SplitAmount = ItemComp->Quantity / 2;
		if (Target->AddItemByGuidNoStack(FGuid::NewGuid(), ItemComp->ItemDefinitionId, SplitAmount))
		{
			ItemComp->Quantity -= SplitAmount;   // replicated
			bOk = true;
			Count = SplitAmount;
		}
		break;
	}
	case EMOInventoryOp::Equip:
	case EMOInventoryOp::Unequip:
	{
		// Always the requester's OWN pawn: a player dresses their own character only.
		UMOEquipmentComponent* Equipment = Pawn->FindComponentByClass<UMOEquipmentComponent>();
		const EMOEquipmentSlot Slot = static_cast<EMOEquipmentSlot>(R.EquipSlot);
		if (!Equipment)
		{
			break;
		}
		if (R.Op == EMOInventoryOp::Equip)
		{
			bOk = Slot == EMOEquipmentSlot::MAX ? Equipment->EquipFromInventoryToNaturalSlot(Source, R.ItemGuid) : Equipment->EquipFromInventory(Source, R.ItemGuid, Slot);
		}
		else if (Slot != EMOEquipmentSlot::MAX)
		{
			bOk = Equipment->UnequipToInventory(Slot, Target);
		}
		Count = bOk ? 1 : 0;
		break;
	}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInventoryReq] %s by %s: %s (%d)"), OpName(R.Op), *PC->GetName(), bOk ? TEXT("done") : TEXT("nothing to do / refused by the inventory"), Count);
	return bOk;
}
