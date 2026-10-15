/**
 * The inventory slot invariant (UMOInventoryComponent::ReconcileSlots): every entry sits in a slot while one is free, and a slot
 * holds only an entry's GUID, once. Run: python Tools/ue.py auto --filter "MOFramework.Inventory.Slot"
 *
 * Found Oct 15 2026 by a first-ten-minutes pass: the pawn's kit row "Stick01 x20" (stack 10) became two entries, and only the
 * first was put in a slot -- 10 sticks in the inventory that the backpack never showed.
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOworldSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MOInventorySlotTest
{
	/** Entries with no slot, and slots holding a GUID that is not an entry. */
	void Audit(const UMOInventoryComponent& Inv, int32& OutUnslotted, int32& OutDangling)
	{
		OutUnslotted = 0;
		OutDangling = 0;
		TArray<FGuid> Slotted;
		for (int32 Slot = 0; Slot < Inv.GetSlotCount(); ++Slot)
		{
			FGuid Guid;
			if (Inv.TryGetSlotGuid(Slot, Guid) && Guid.IsValid())
			{
				FMOInventoryEntry Entry;
				if (!Inv.TryGetEntryByGuid(Guid, Entry))
				{
					++OutDangling;
				}
				Slotted.Add(Guid);
			}
		}
		for (const FGuid& Guid : Inv.GetAllItemGuids())
		{
			if (!Slotted.Contains(Guid))
			{
				++OutUnslotted;
			}
		}
	}

	struct FWorld
	{
		UWorld* World = nullptr;
		FWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("MOInventorySlotTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		~FWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		/** An actor whose inventory runs BeginPlay (and so applies its starting items) the way a spawned pawn's does. */
		UMOInventoryComponent* Spawn(const TArray<FMOStartingInventoryItem>& Kit, int32 Slots)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UMOInventoryComponent* Inv = NewObject<UMOInventoryComponent>(Actor);
			Inv->StartingItems = Kit;
			Inv->SetSlotCountAuthority(Slots);
			Actor->AddInstanceComponent(Inv);
			Inv->RegisterComponent();
			Actor->DispatchBeginPlay();
			return Inv;
		}
	};

	FMOStartingInventoryItem Row(FName Id, int32 Quantity, int32 SlotIndex = -1)
	{
		FMOStartingInventoryItem Item;
		Item.ItemDefinitionId = Id;
		Item.Quantity = Quantity;
		Item.SlotIndex = SlotIndex;
		return Item;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOInventory_SlotInvariant,
	"MOFramework.Inventory.SlotInvariant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOInventory_SlotInvariant::RunTest(const FString& Parameters)
{
	using namespace MOInventorySlotTest;

	const FName Stick(TEXT("Stick01"));
	FMOItemDefinitionRow StickDef;
	if (!TestTrue(TEXT("Stick01 is in the item table"), UMOItemDatabaseSettings::GetItemDefinition(Stick, StickDef))
		|| !TestTrue(TEXT("Stick01 stacks (the test needs a stack limit above 1)"), StickDef.MaxStackSize > 1 && !StickDef.bIsTool))
	{
		return false;
	}
	const int32 Stack = StickDef.MaxStackSize;

	FWorld W;
	int32 Unslotted = 0, Dangling = 0;

	// 1. A kit row over the stack limit splits into several entries -- all of them are shown.
	{
		UMOInventoryComponent* Inv = W.Spawn({ Row(Stick, 2 * Stack) }, 10);
		TestEqual(TEXT("a row of two stacks' worth makes two entries"), Inv->GetEntryCount(), 2);
		Audit(*Inv, Unslotted, Dangling);
		TestEqual(TEXT("... and both are in a slot"), Unslotted, 0);
		TestEqual(TEXT("... and no slot points at nothing"), Dangling, 0);
	}

	// 2. A row that only tops up an existing stack makes no new entry -- and must not put a GUID that is not an entry in a slot.
	{
		UMOInventoryComponent* Inv = W.Spawn({ Row(Stick, 1), Row(Stick, 1) }, 10);
		TestEqual(TEXT("two small rows of one item merge into one entry"), Inv->GetEntryCount(), 1);
		Audit(*Inv, Unslotted, Dangling);
		TestEqual(TEXT("... which is in a slot"), Unslotted, 0);
		TestEqual(TEXT("... and no slot holds the merged row's unused GUID"), Dangling, 0);
	}

	// 3. An explicit SlotIndex is honoured, and the overflow of that row fills the free slots.
	{
		UMOInventoryComponent* Inv = W.Spawn({ Row(Stick, Stack + 1, 3) }, 10);
		FGuid InSlot3;
		FMOInventoryEntry Entry;
		TestTrue(TEXT("the requested slot 3 holds the row's first stack"),
			Inv->TryGetSlotGuid(3, InSlot3) && Inv->TryGetEntryByGuid(InSlot3, Entry) && Entry.Quantity == Stack);
		Audit(*Inv, Unslotted, Dangling);
		TestEqual(TEXT("... and the overflow stack is in a slot too"), Unslotted, 0);
	}

	// 4. A save written with the bug (an entry in no slot, a slot holding a GUID that is not an entry) is repaired on load.
	{
		UMOInventoryComponent* Inv = W.Spawn({}, 10);
		FMOInventorySaveData Save;
		Save.SlotCount = 10;
		const FGuid Shown = FGuid::NewGuid(), Hidden = FGuid::NewGuid(), Ghost = FGuid::NewGuid();
		for (const FGuid& Guid : { Shown, Hidden })
		{
			FMOInventoryItemSaveEntry Item;
			Item.ItemGuid = Guid;
			Item.ItemDefinitionId = Stick;
			Item.Quantity = Stack;
			Save.Items.Add(Item);
		}
		Save.SlotItemGuids = { Shown, Ghost };
		TestTrue(TEXT("the save applies"), Inv->ApplySaveDataAuthority(Save));
		Audit(*Inv, Unslotted, Dangling);
		TestEqual(TEXT("the entry the old save left in no slot is shown after load"), Unslotted, 0);
		TestEqual(TEXT("the slot that pointed at nothing is cleared"), Dangling, 0);
		FGuid InSlot0;
		TestTrue(TEXT("an entry that had a slot keeps it"), Inv->TryGetSlotGuid(0, InSlot0) && InSlot0 == Shown);
	}

	return true;
}

#endif
