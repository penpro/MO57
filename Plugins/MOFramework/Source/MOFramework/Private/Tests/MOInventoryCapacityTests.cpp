// Audit batch B: capacity is enforced where entries are created, with conservation and replication regressions.
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace MOInventoryCapacityTests
{
	struct FWorld
	{
		UWorld* World;
		FWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MOInventoryCapacityWorld"));
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		~FWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		UMOInventoryComponent* Spawn(int32 Slots)
		{
			APawn* Pawn = World->SpawnActor<APawn>();
			auto* Inventory = NewObject<UMOInventoryComponent>(Pawn);
			Pawn->AddInstanceComponent(Inventory);
			Inventory->SetSlotCountAuthority(Slots);
			Inventory->RegisterComponent();
			Pawn->DispatchBeginPlay();
			return Inventory;
		}
	};

	FName Stackable(int32& OutMax)
	{
		UDataTable* Table = GetDefault<UMOItemDatabaseSettings>()->GetItemDefinitionsDataTable();
		if (!Table) return NAME_None;
		TArray<FName> Names = Table->GetRowNames();
		Names.Sort([](FName A, FName B) { return A.LexicalLess(B); });
		for (FName Name : Names)
		{
			const auto* Def = Table->FindRow<FMOItemDefinitionRow>(Name, TEXT("MOInventoryCapacity"));
			if (Def && !Def->bIsTool && Def->LiquidCapacityMl <= 0 && Def->MaxStackSize >= 4)
			{
				OutMax = Def->MaxStackSize;
				return Name;
			}
		}
		return NAME_None;
	}

	bool AllSlotted(UMOInventoryComponent* Inventory)
	{
		TSet<FGuid> Slotted;
		for (const FGuid& Guid : Inventory->SlotItemGuids) if (Guid.IsValid()) Slotted.Add(Guid);
		if (Slotted.Num() != Inventory->Inventory.Entries.Num()) return false;
		for (const FMOInventoryEntry& Entry : Inventory->Inventory.Entries) if (!Slotted.Contains(Entry.ItemGuid)) return false;
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOInventoryCapacityGate, "MOFramework.Inventory.Capacity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOInventoryCapacityGate::RunTest(const FString&)
{
	using namespace MOInventoryCapacityTests;
	int32 Max = 0;
	const FName Item = Stackable(Max);
	if (!TestFalse(TEXT("Real stackable item is available"), Item.IsNone())) return false;
	FWorld World;
	auto* Inventory = World.Spawn(1);
	const FGuid Guid = FGuid::NewGuid();
	TestTrue(TEXT("Fill one slot"), Inventory->AddItemByGuid(Guid, Item, Max - 1));
	const FMOInventoryAddResult Refused = Inventory->TryAddItem(FGuid::NewGuid(), Item, 2);
	TestEqual(TEXT("Atomic refusal accepts nothing"), Refused.Accepted, 0);
	TestEqual(TEXT("Failure identifies capacity"), Refused.Failure, EMOInventoryAddFailure::Capacity);
	TestEqual(TEXT("Refusal leaves partial stack untouched"), Inventory->GetItemCountByDefinitionId(Item), Max - 1);
	FMOInventoryAddOptions Partial;
	Partial.bAllowPartial = true;
	const auto Placed = Inventory->TryAddItem(FGuid::NewGuid(), Item, 2, Partial);
	TestEqual(TEXT("Explicit partial add reports exact accepted quantity"), Placed.Accepted, 1);
	TestEqual(TEXT("Partial result reports remainder capacity failure"), Placed.Failure, EMOInventoryAddFailure::Capacity);
	TestFalse(TEXT("Full original GUID cannot bypass limit"), Inventory->AddItemByGuid(Guid, Item, 1));
	TestFalse(TEXT("Durability variant cannot bypass limit"), Inventory->AddItemByGuidWithDurability(FGuid::NewGuid(), Item, 1, 3));
	TestFalse(TEXT("NoStack cannot bypass limit"), Inventory->AddItemByGuidNoStack(FGuid::NewGuid(), Item, 1));
	TestFalse(TEXT("Unassigned add cannot bypass limit"), Inventory->AddItemByGuidWithoutSlotAutoAssign(FGuid::NewGuid(), Item, 1));
	TestTrue(TEXT("No invisible overflow"), AllSlotted(Inventory));
	TestEqual(TEXT("Exactly the stack limit was accepted"), Inventory->GetItemCountByDefinitionId(Item), Max);
	auto* Split = World.Spawn(1);
	TestFalse(TEXT("Oversized NoStack request refuses atomically"), Split->AddItemByGuidNoStack(FGuid::NewGuid(), Item, Max + 1));
	TestEqual(TEXT("NoStack did not truncate and claim success"), Split->GetEntryCount(), 0);
	Split->SetSlotCountAuthority(2);
	TestTrue(TEXT("With two slots NoStack splits oversized quantity"), Split->AddItemByGuidNoStack(FGuid::NewGuid(), Item, Max + 1));
	TestEqual(TEXT("Split conserves every unit"), Split->GetItemCountByDefinitionId(Item), Max + 1);
	TestTrue(TEXT("Split entries both have slots"), AllSlotted(Split));
	auto* Kit = World.Spawn(1);
	TestTrue(TEXT("Unassigned batch reserves its slot"), Kit->AddItemByGuidWithoutSlotAutoAssign(FGuid::NewGuid(), Item, Max));
	TestFalse(TEXT("Second unassigned row cannot reuse reserved slot"), Kit->AddItemByGuidWithoutSlotAutoAssign(FGuid::NewGuid(), Item, 1));
	TestFalse(TEXT("Capacity preview agrees with mutation"), Kit->CanAddItemByDefinitionId(Item, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOInventoryStackReplicationGate, "MOFramework.Inventory.StackReplication", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOInventoryStackReplicationGate::RunTest(const FString&)
{
	using namespace MOInventoryCapacityTests;
	int32 Max = 0;
	const FName Item = Stackable(Max);
	if (!TestFalse(TEXT("Real stackable item is available"), Item.IsNone())) return false;
	FWorld World;
	auto* Target = World.Spawn(2);
	auto* Source = World.Spawn(1);
	const FGuid TargetGuid = FGuid::NewGuid(), SourceGuid = FGuid::NewGuid();
	Target->AddItemByGuid(TargetGuid, Item, Max - 1);
	Source->AddItemByGuid(SourceGuid, Item, Max - 1);
	const int32 TargetKey = Target->Inventory.Entries[0].ReplicationKey;
	const int32 SourceKey = Source->Inventory.Entries[0].ReplicationKey;
	TestEqual(TEXT("Fill moves one unit"), Target->FillStacksFrom(Source), 1);
	TestEqual(TEXT("Both stacks survive"), Source->Inventory.Entries[0].Quantity, Max - 2);
	TestTrue(TEXT("Changed target entry will replicate"), Target->Inventory.Entries[0].ReplicationKey > TargetKey);
	TestTrue(TEXT("Changed surviving source entry will replicate"), Source->Inventory.Entries[0].ReplicationKey > SourceKey);
	TestEqual(TEXT("Filling from self is refused"), Target->FillStacksFrom(Target), 0);
	Target->ClearInventoryAndSlots();
	Target->AddItemByGuidNoStack(TargetGuid, Item, 1);
	Target->AddItemByGuidNoStack(SourceGuid, Item, 1);
	const int32 BeforeOrganize = Target->Inventory.Entries[0].ReplicationKey;
	Target->StackAndOrganize();
	TestEqual(TEXT("Organize merged two stacks"), Target->GetEntryCount(), 1);
	TestEqual(TEXT("Organize conserved quantity"), Target->Inventory.Entries[0].Quantity, 2);
	TestTrue(TEXT("Surviving organized entry will replicate"), Target->Inventory.Entries[0].ReplicationKey > BeforeOrganize);
	TestTrue(TEXT("Organize reconciled slots"), AllSlotted(Target));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOInventoryTransferGate, "MOFramework.Inventory.TransferConservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOInventoryTransferGate::RunTest(const FString&)
{
	using namespace MOInventoryCapacityTests;
	int32 Max = 0;
	const FName Item = Stackable(Max);
	if (!TestFalse(TEXT("Real stackable item is available"), Item.IsNone())) return false;
	FWorld World;
	auto* Source = World.Spawn(1);
	auto* Target = World.Spawn(1);
	const FGuid Guid = FGuid::NewGuid();
	Source->AddItemByGuid(Guid, Item, 2);
	Target->AddItemByGuid(FGuid::NewGuid(), Item, Max);
	const auto Refused = Source->TryTransferItem(Guid, Target);
	TestEqual(TEXT("Full transfer reports capacity"), Refused.Failure, EMOInventoryAddFailure::Capacity);
	TestEqual(TEXT("Refused transfer leaves source intact"), Source->GetItemCountByDefinitionId(Item), 2);
	TestEqual(TEXT("Refused transfer leaves target intact"), Target->GetItemCountByDefinitionId(Item), Max);
	TestTrue(TEXT("No target overflow"), AllSlotted(Target));
	TestFalse(TEXT("Self transfer refuses"), Source->TransferItem(Guid, Source));
	Target->ClearInventoryAndSlots();
	FMOItemInstanceState State;
	State.Durability = 7;
	State.Liquid.Millilitres = 240;
	State.Liquid.SalinityGPerL = 0.3f;
	State.Liquid.Source = EMOWaterBodyKind::Pond;
	State.Liquid.bBoiled = true;
	Source->ApplyInstanceState(Guid, State);
	TestTrue(TEXT("Transfer fits after clearing target"), Source->TryTransferItem(Guid, Target).IsComplete());
	FMOInventoryEntry Moved;
	TestTrue(TEXT("Transferred stack keeps identity"), Target->TryGetEntryByGuid(Guid, Moved));
	TestEqual(TEXT("Transferred quantity conserved"), Moved.Quantity, 2);
	TestEqual(TEXT("Transferred durability conserved"), Moved.CurrentDurability, State.Durability);
	TestTrue(TEXT("Transferred contents conserved"), Moved.Liquid == State.Liquid);
	TestEqual(TEXT("Source no longer owns transferred item"), Source->GetEntryCount(), 0);
	// Distinct carried state must not be destroyed by later ordinary adds or organization.
	Target->SetSlotCountAuthority(2);
	Target->AddItemByGuid(FGuid::NewGuid(), Item, 1);
	Target->StackAndOrganize();
	TestEqual(TEXT("Different instance states remain separate"), Target->GetEntryCount(), 2);
	TestTrue(TEXT("Original contents remain intact"), Target->GetInstanceState(Guid).Liquid == State.Liquid);
	return true;
}
#endif
