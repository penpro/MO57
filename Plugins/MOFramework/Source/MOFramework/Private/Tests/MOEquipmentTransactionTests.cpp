// Audit batch A: untrusted slot bytes and conservation across delayed hand swaps/save-load.
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "MOEquipmentComponent.h"
#include "MOInventoryComponent.h"
#include "MOInventoryRequest.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOworldSaveGame.h"
#include "MOEquipmentWearProbe.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace MOEquipmentTransactionTests
{
	struct FPawn
	{
		APawn* Pawn = nullptr;
		APlayerController* PC = nullptr;
		UMOInventoryComponent* Inventory = nullptr;
		UMOEquipmentComponent* Equipment = nullptr;
	};

	struct FWorld
	{
		UWorld* World = nullptr;
		FWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MOEquipmentTransactionWorld"));
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		~FWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		FPawn Spawn()
		{
			FPawn Result;
			Result.Pawn = World->SpawnActor<APawn>();
			Result.PC = World->SpawnActor<APlayerController>();
			Result.Inventory = NewObject<UMOInventoryComponent>(Result.Pawn);
			Result.Equipment = NewObject<UMOEquipmentComponent>(Result.Pawn);
			Result.Inventory->SetSlotCountAuthority(8);
			Result.Equipment->LightSwapTime = Result.Equipment->MediumSwapTime = Result.Equipment->HeavySwapTime = 1.0f;
			Result.Pawn->AddInstanceComponent(Result.Inventory);
			Result.Pawn->AddInstanceComponent(Result.Equipment);
			Result.Inventory->RegisterComponent();
			Result.Equipment->RegisterComponent();
			Result.Pawn->DispatchBeginPlay();
			Result.PC->Possess(Result.Pawn);
			return Result;
		}
	};

	FName HeldItem()
	{
		UDataTable* Table = GetDefault<UMOItemDatabaseSettings>()->GetItemDefinitionsDataTable();
		if (!Table) return NAME_None;
		TArray<FName> Names = Table->GetRowNames();
		Names.Sort([](FName A, FName B) { return A.LexicalLess(B); });
		for (FName Name : Names)
		{
			const FMOItemDefinitionRow* Def = Table->FindRow<FMOItemDefinitionRow>(Name, TEXT("MOEquipmentTransactions"));
			if (Def && Def->bEquippable && Def->EquipmentSlotType == EMOEquipmentSlotType::Held && Def->MaxStackSize == 1)
			{
				return Name;
			}
		}
		return NAME_None;
	}

	void Advance(UMOEquipmentComponent* Equipment, float Seconds)
	{
		// Dispatch the real override through the public UActorComponent interface.
		static_cast<UActorComponent*>(Equipment)->TickComponent(Seconds, LEVELTICK_All, nullptr);
	}

	int32 OwnersOf(const FPawn& Pawn, const FGuid& Guid)
	{
		FMOInventoryEntry Entry;
		int32 Owners = Pawn.Inventory->TryGetEntryByGuid(Guid, Entry) ? Entry.Quantity : 0;
		FMOEquipmentSaveData State;
		Pawn.Equipment->BuildSaveData(State);
		for (const FMOEquipmentSlotSaveData& Slot : State.Slots) Owners += Slot.EquippedItem.ItemGuid == Guid ? 1 : 0;
		for (const FMOEquipmentPendingSwap& Pending : State.PendingSwaps) Owners += Pending.Item.ItemGuid == Guid ? 1 : 0;
		return Owners;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOEquipmentInvalidSlots, "MOFramework.Equipment.InvalidSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOEquipmentInvalidSlots::RunTest(const FString& Parameters)
{
	using namespace MOEquipmentTransactionTests;
	FWorld World;
	const FPawn Pawn = World.Spawn();
	for (int32 Byte = 0; Byte <= 255; ++Byte)
	{
		const EMOEquipmentSlot Slot = static_cast<EMOEquipmentSlot>(Byte);
		TestEqual(TEXT("slot validity is bounded by the actual slot count"), UMOEquipmentComponent::IsValidEquipmentSlot(Slot), Byte < 8);
		if (Byte < 8) continue;
		if (Byte == 8 || Byte == 9 || Byte == 255)
		{
			FMOInventoryRequest Request = FMOInventoryRequest::Make(EMOInventoryOp::Unequip, nullptr, Pawn.Inventory);
			Request.EquipSlot = static_cast<uint8>(Byte);
			TestFalse(TEXT("an invalid unequip request is refused safely"), FMOInventoryRequests::Execute(Pawn.PC, Request));
		}
		TestFalse(TEXT("the component also refuses an invalid slot"), Pawn.Equipment->UnequipToInventory(Slot, Pawn.Inventory));
		TestFalse(TEXT("an invalid slot reads empty"), Pawn.Equipment->GetEquippedItem(Slot).IsValid());
		Pawn.Equipment->SwapSlots(EMOEquipmentSlot::RightHand, Slot);
	}
	FMOEquipmentSaveData InvalidSave;
	FMOEquipmentSlotSaveData& InvalidSlot = InvalidSave.Slots.AddDefaulted_GetRef();
	InvalidSlot.Slot = static_cast<EMOEquipmentSlot>(255);
	InvalidSlot.EquippedItem.ItemGuid = FGuid::NewGuid();
	InvalidSlot.EquippedItem.ItemDefinitionId = TEXT("InvalidSlotTest");
	Pawn.Equipment->ApplySaveData(InvalidSave);
	FMOEquipmentSaveData After;
	Pawn.Equipment->BuildSaveData(After);
	TestEqual(TEXT("a malformed save does not index or install an invalid slot"), After.Slots.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOEquipmentSwapConservation, "MOFramework.Equipment.SwapConservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOEquipmentSwapConservation::RunTest(const FString& Parameters)
{
	using namespace MOEquipmentTransactionTests;
	const FName Item = HeldItem();
	if (!TestFalse(TEXT("the live table supplies a non-stackable held item"), Item.IsNone())) return false;
	FWorld World;
	const FPawn Pawn = World.Spawn();
	const FGuid Old = FGuid::NewGuid(), Incoming = FGuid::NewGuid();
	Pawn.Inventory->AddItemByGuidWithDurability(Old, Item, 1, 5);
	Pawn.Inventory->AddItemByGuidWithDurability(Incoming, Item, 1, 9);
	FMOInventoryRequest Equip = FMOInventoryRequest::Make(EMOInventoryOp::Equip, Pawn.Inventory, nullptr, Old);
	Equip.EquipSlot = static_cast<uint8>(EMOEquipmentSlot::MAX);
	TestTrue(TEXT("the natural-slot sentinel remains valid for equip"), FMOInventoryRequests::Execute(Pawn.PC, Equip));
	Advance(Pawn.Equipment, 1.1f);
	TestEqual(TEXT("equipping preserves the original item's identity"), Pawn.Equipment->GetEquippedItem(EMOEquipmentSlot::RightHand).ItemGuid, Old);
	TestTrue(TEXT("a replacement starts"), Pawn.Equipment->EquipFromInventory(Pawn.Inventory, Incoming, EMOEquipmentSlot::RightHand));
	TestFalse(TEXT("the returned old item no longer occupies the hand"), Pawn.Equipment->HasItemInSlot(EMOEquipmentSlot::RightHand));
	TestFalse(TEXT("unequip during the delay cannot return the old item again"), Pawn.Equipment->UnequipToInventory(EMOEquipmentSlot::RightHand, Pawn.Inventory));
	Pawn.Equipment->SwapSlots(EMOEquipmentSlot::RightHand, EMOEquipmentSlot::LeftHand);
	TestTrue(TEXT("a conflicting slot swap did not move the pending transaction"), Pawn.Equipment->IsSlotSwapping(EMOEquipmentSlot::RightHand));
	TestEqual(TEXT("the old item has exactly one owner during the swap"), OwnersOf(Pawn, Old), 1);
	TestEqual(TEXT("the incoming item has exactly one owner during the swap"), OwnersOf(Pawn, Incoming), 1);
	Advance(Pawn.Equipment, 0.3f);
	TestFalse(TEXT("the hand cannot skip the remaining work"), Pawn.Equipment->HasItemInSlot(EMOEquipmentSlot::RightHand));
	Advance(Pawn.Equipment, 0.8f);
	TestEqual(TEXT("the correct replacement completes"), Pawn.Equipment->GetEquippedItem(EMOEquipmentSlot::RightHand).ItemGuid, Incoming);
	TestTrue(TEXT("a finished item can be unequipped"), Pawn.Equipment->UnequipToInventory(EMOEquipmentSlot::RightHand, Pawn.Inventory));
	FMOInventoryEntry Returned;
	TestTrue(TEXT("the original incoming GUID returns to inventory"), Pawn.Inventory->TryGetEntryByGuid(Incoming, Returned));
	TestEqual(TEXT("its wear survives the round trip"), Returned.CurrentDurability, 9);
	TestEqual(TEXT("the old item still exists once"), OwnersOf(Pawn, Old), 1);
	TestEqual(TEXT("the incoming item still exists once"), OwnersOf(Pawn, Incoming), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOEquipmentPendingSave, "MOFramework.Equipment.PendingSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOEquipmentPendingSave::RunTest(const FString& Parameters)
{
	using namespace MOEquipmentTransactionTests;
	const FName Item = HeldItem();
	if (!TestFalse(TEXT("the live table supplies a held item"), Item.IsNone())) return false;
	FWorld World;
	const FPawn Original = World.Spawn();
	const FGuid Guid = FGuid::NewGuid(), LeftGuid = FGuid::NewGuid();
	Original.Inventory->AddItemByGuidWithDurability(Guid, Item, 1, 7);
	Original.Inventory->AddItemByGuidWithDurability(LeftGuid, Item, 1, 4);
	FMOLiquidContents Contents;
	Contents.Millilitres = 300.0f;
	Contents.SalinityGPerL = 2.0f;
	Contents.bBoiled = true;
	Original.Inventory->SetLiquid(Guid, Contents);
	TestTrue(TEXT("the timed equip starts"), Original.Equipment->EquipFromInventory(Original.Inventory, Guid, EMOEquipmentSlot::RightHand));
	TestTrue(TEXT("the other hand starts independently"), Original.Equipment->EquipFromInventory(Original.Inventory, LeftGuid, EMOEquipmentSlot::LeftHand));
	Advance(Original.Equipment, 0.25f);
	FMOEquipmentSaveData Save;
	Original.Equipment->BuildSaveData(Save);
	TestEqual(TEXT("both pending items are in the save"), Save.PendingSwaps.Num(), 2);
	FMOInventorySaveData InventorySave;
	Original.Inventory->BuildSaveData(InventorySave);

	// Exercise reflected serialization, not just a C++ struct copy.
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive SaveArchive(Writer, false);
	FMOEquipmentSaveData::StaticStruct()->SerializeItem(SaveArchive, &Save, nullptr);
	FMOEquipmentSaveData Loaded;
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
	FMOEquipmentSaveData::StaticStruct()->SerializeItem(LoadArchive, &Loaded, nullptr);
	const FPawn Restored = World.Spawn();
	Restored.Inventory->ApplySaveDataAuthority(InventorySave);
	Restored.Equipment->ApplySaveData(Loaded);
	TestEqual(TEXT("reloaded escrow owns the item once"), OwnersOf(Restored, Guid), 1);
	TestEqual(TEXT("the other loaded escrow owns its item once"), OwnersOf(Restored, LeftGuid), 1);
	TestTrue(TEXT("the loaded hand is still swapping"), Restored.Equipment->IsSlotSwapping(EMOEquipmentSlot::RightHand));
	Advance(Restored.Equipment, 0.5f);
	TestFalse(TEXT("load did not complete the equip early"), Restored.Equipment->HasItemInSlot(EMOEquipmentSlot::RightHand));
	Advance(Restored.Equipment, 0.3f);
	TestEqual(TEXT("the pending item completes after the saved remaining time"), Restored.Equipment->GetEquippedItem(EMOEquipmentSlot::RightHand).ItemGuid, Guid);
	TestEqual(TEXT("loaded wear is preserved"), Restored.Equipment->GetEquippedItem(EMOEquipmentSlot::RightHand).CurrentDurability, 7);
	TestEqual(TEXT("the other hand completes with its own identity"), Restored.Equipment->GetEquippedItem(EMOEquipmentSlot::LeftHand).ItemGuid, LeftGuid);
	TestTrue(TEXT("per-instance contents survive the save and equip"), Restored.Equipment->GetEquippedItem(EMOEquipmentSlot::RightHand).Liquid == Contents);
	TestEqual(TEXT("completion still owns the item once"), OwnersOf(Restored, Guid), 1);

	// Older saves contain only Slots and no pending state.
	FMOEquipmentSaveData Legacy;
	FMOEquipmentSlotSaveData& Slot = Legacy.Slots.AddDefaulted_GetRef();
	Slot.Slot = EMOEquipmentSlot::RightHand;
	Slot.EquippedItem = Restored.Equipment->GetEquippedItem(EMOEquipmentSlot::RightHand);
	Restored.Equipment->ApplySaveData(Legacy);
	TestFalse(TEXT("a legacy snapshot clears previous pending work"), Restored.Equipment->IsSlotSwapping(EMOEquipmentSlot::RightHand));
	TestEqual(TEXT("a legacy equipped item is preserved"), OwnersOf(Restored, Guid), 1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOEquipmentDurabilityGate, "MOFramework.Equipment.Durability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOEquipmentDurabilityGate::RunTest(const FString&)
{
	using namespace MOEquipmentTransactionTests;
	FWorld World;
	const FPawn Pawn = World.Spawn();
	const FName Definition = HeldItem();
	if (!TestFalse(TEXT("A held-item fixture exists"), Definition.IsNone())) return false;
	const FGuid Guid = FGuid::NewGuid();
	FMOInventoryAddOptions Options;
	Options.State.Durability = 5;
	Pawn.Inventory->TryAddItem(Guid, Definition, 1, Options);
	TestTrue(TEXT("Begin a real equip delay"), Pawn.Equipment->EquipFromInventory(Pawn.Inventory, Guid, EMOEquipmentSlot::RightHand));
	bool Destroyed = true;
	TestFalse(TEXT("An incoming swap item cannot wear before it is equipped"), Pawn.Equipment->ReduceEquippedDurability(EMOEquipmentSlot::RightHand, 1, Destroyed));
	TestFalse(TEXT("Refused wear does not report destruction"), Destroyed);
	Advance(Pawn.Equipment, 1.1f);
	auto* Probe = NewObject<UMOEquipmentWearProbe>();
	Pawn.Equipment->OnEquipmentChanged.AddDynamic(Probe, &UMOEquipmentWearProbe::EquipmentChanged);
	Pawn.Equipment->OnEquipmentDurabilityChanged.AddDynamic(Probe, &UMOEquipmentWearProbe::DurabilityChanged);
	TArray<FMOEquippedItem> Before;
	for (int32 I = 0; I < int32(EMOEquipmentSlot::MAX); ++I) Before.Add(Pawn.Equipment->GetEquippedItem(static_cast<EMOEquipmentSlot>(I)));
	TestTrue(TEXT("Authority can wear the equipped item"), Pawn.Equipment->ReduceEquippedDurability(EMOEquipmentSlot::RightHand, 2, Destroyed));
	TestEqual(TEXT("Wear preserves identity and reduces only durability"), Pawn.Equipment->GetEquippedItem(EMOEquipmentSlot::RightHand).CurrentDurability, 3);
	TestEqual(TEXT("Ordinary wear does not rebuild equipment visuals"), Probe->EquipmentEvents, 0);
	TestEqual(TEXT("Ordinary wear has a distinct notification"), Probe->WearEvents, 1);
	Probe->EquipmentEvents = Probe->WearEvents = 0;
	UFunction* RepNotify = Pawn.Equipment->FindFunction(TEXT("OnRep_EquippedSlots"));
	if (!TestNotNull(TEXT("Equipment replication has a delta-aware notification"), RepNotify)) return false;
	struct FRepParams { TArray<FMOEquippedItem> PreviousSlots; } Params{Before};
	Pawn.Equipment->ProcessEvent(RepNotify, &Params);
	TestEqual(TEXT("Replicated wear does not rebuild held meshes"), Probe->EquipmentEvents, 0);
	TestEqual(TEXT("Replicated wear updates wear consumers"), Probe->WearEvents, 1);
	for (int32 Raw = int32(EMOEquipmentSlot::MAX); Raw < 256; ++Raw)
		TestFalse(TEXT("Untrusted invalid slot bytes refuse wear"), Pawn.Equipment->ReduceEquippedDurability(static_cast<EMOEquipmentSlot>(Raw), 1, Destroyed));
	TestFalse(TEXT("Zero wear is not an operation"), Pawn.Equipment->ReduceEquippedDurability(EMOEquipmentSlot::RightHand, 0, Destroyed));
	Pawn.Pawn->SetRole(ROLE_AutonomousProxy);
	TestFalse(TEXT("A client cannot wear equipment authoritatively"), Pawn.Equipment->ReduceEquippedDurability(EMOEquipmentSlot::RightHand, 1, Destroyed));
	Pawn.Pawn->SetRole(ROLE_Authority);
	FMOEquipmentSaveData Saved;
	Pawn.Equipment->BuildSaveData(Saved);
	const FPawn Restored = World.Spawn();
	Restored.Equipment->ApplySaveData(Saved);
	TestEqual(TEXT("Worn durability survives the equipment snapshot"), Restored.Equipment->GetEquippedItem(EMOEquipmentSlot::RightHand).CurrentDurability, 3);
	Saved.Slots[0].EquippedItem.CurrentDurability = -1;
	Restored.Equipment->ApplySaveData(Saved);
	TestFalse(TEXT("Infinite durability is unchanged"), Restored.Equipment->ReduceEquippedDurability(EMOEquipmentSlot::RightHand, 1, Destroyed));
	Probe->EquipmentEvents = Probe->WearEvents = 0;
	TestTrue(TEXT("Finite durability can be exhausted"), Pawn.Equipment->ReduceEquippedDurability(EMOEquipmentSlot::RightHand, MAX_int32, Destroyed));
	TestTrue(TEXT("Depletion reports destruction"), Destroyed);
	TestFalse(TEXT("Depleted equipment releases ownership"), Pawn.Equipment->HasItemInSlot(EMOEquipmentSlot::RightHand));
	TestEqual(TEXT("Depletion changes equipment visuals exactly once"), Probe->EquipmentEvents, 1);
	TestEqual(TEXT("The consumed item has no remaining owner"), OwnersOf(Pawn, Guid), 0);
	Pawn.Equipment->OnEquipmentChanged.RemoveDynamic(Probe, &UMOEquipmentWearProbe::EquipmentChanged);
	Pawn.Equipment->OnEquipmentDurabilityChanged.RemoveDynamic(Probe, &UMOEquipmentWearProbe::DurabilityChanged);
	return true;
}
#endif
