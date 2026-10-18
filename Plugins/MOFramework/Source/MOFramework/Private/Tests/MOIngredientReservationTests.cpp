#include "Misc/AutomationTest.h"
#include "MOIngredientReservation.h"
#include "MOInventoryComponent.h"
#include "MOItemDefinitionRow.h"
#include "MOItemDatabaseSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace MOIngredientReservationTests
{
	const FName Water(TEXT("AuditWater")), Safe(TEXT("AuditSafeWater")), Jar(TEXT("AuditJar")), Stone(TEXT("AuditStone")), Tool(TEXT("AuditTool"));
	struct FDefinitions
	{
		TMap<FName, FMOItemDefinitionRow> Rows;
		FDefinitions()
		{
			Rows.FindOrAdd(Water).LiquidMeasureMl = 250;
			Rows.FindOrAdd(Safe).LiquidMeasureMl = 250;
			Rows[Safe].bMeasureNeedsSafeWater = true;
			Rows.FindOrAdd(Jar).LiquidCapacityMl = 1000;
			Rows.FindOrAdd(Stone).MaxStackSize = 10;
			Rows.FindOrAdd(Tool).bIsTool = true;
		}
		bool Lookup(FName Id, FMOItemDefinitionRow& Out) const
		{
			if (const auto* Row = Rows.Find(Id)) { Out = *Row; return true; }
			return false;
		}
	};
	FMOInventoryEntry Entry(FName Id, int32 Quantity = 1, float Ml = 0, bool bBoiled = false, float Salt = 0)
	{
		FMOInventoryEntry Result;
		Result.ItemGuid = FGuid::NewGuid();
		Result.ItemDefinitionId = Id;
		Result.Quantity = Quantity;
		Result.Liquid.Millilitres = Ml;
		Result.Liquid.bBoiled = bBoiled;
		Result.Liquid.SalinityGPerL = Salt;
		Result.Liquid.IllnessChancePerLitre = bBoiled ? 0 : 0.12f;
		Result.Liquid.Source = EMOWaterBodyKind::Pond;
		return Result;
	}
	float ReservedMl(const FMOIngredientReservation& Reservation)
	{
		float Total = 0;
		for (const auto& Lot : Reservation.Lots) if (Lot.IsLiquid()) Total += Lot.State.Liquid.Millilitres;
		return Total;
	}
	struct FWorld
	{
		UWorld* World;
		FWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MOIngredientReservationWorld"));
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		~FWorld() { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
		UMOInventoryComponent* Spawn(int32 Slots)
		{
			auto* Owner = World->SpawnActor<AActor>();
			auto* Inventory = NewObject<UMOInventoryComponent>(Owner);
			Owner->AddInstanceComponent(Inventory);
			Inventory->SetSlotCountAuthority(Slots);
			Inventory->RegisterComponent();
			Owner->DispatchBeginPlay();
			return Inventory;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOIngredientReservationAtomic, "MOFramework.Inventory.Reservation.Atomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOIngredientReservationAtomic::RunTest(const FString&)
{
	using namespace MOIngredientReservationTests;
	FDefinitions Definitions;
	auto Lookup = [&Definitions](FName Id, FMOItemDefinitionRow& Out) { return Definitions.Lookup(Id, Out); };
	TArray<FMOInventoryEntry> Stock = {Entry(Stone, 2), Entry(Jar, 1, 500)};
	FMOIngredientReservation Reservation;
	const auto Missing = MOIngredientPlans::Reserve(Stock, {{Stone, 2}, {Water, 3}}, Reservation, Lookup);
	TestEqual(TEXT("Joint reservation reports missing ingredients"), Missing.Failure, EMOIngredientFailure::MissingIngredients);
	TestEqual(TEXT("No physical material removed on failure"), Stock[0].Quantity, 2);
	TestEqual(TEXT("No water removed on failure"), Stock[1].Liquid.Millilitres, 500.0f);
	TestTrue(TEXT("Failed reservation owns nothing"), Reservation.IsEmpty());
	Stock = {Entry(Jar, 1, 250, true, 0.2f), Entry(Jar, 1, 250, false, 0.1f)};
	TestTrue(TEXT("Safe and unrestricted requirements allocate jointly"), MOIngredientPlans::Reserve(Stock, {{Water, 1}, {Safe, 1}}, Reservation, Lookup).IsComplete());
	TestEqual(TEXT("All 500 ml is reserved exactly once"), ReservedMl(Reservation), 500.0f);
	TestEqual(TEXT("Safe requirement took boiled water first"), Reservation.Lots[0].IngredientId, Safe);
	TestTrue(TEXT("Boiled profile retained"), Reservation.Lots[0].State.Liquid.bBoiled);
	TestFalse(TEXT("Untreated profile retained"), Reservation.Lots[1].State.Liquid.bBoiled);
	const auto Insufficient = MOIngredientPlans::Consume(Reservation, {{Water, 2}, {Safe, 1}});
	TestFalse(TEXT("Cannot consume the same water twice"), Insufficient.IsComplete());
	TestEqual(TEXT("Failed consumption retains every reserved ml"), ReservedMl(Reservation), 500.0f);
	TestTrue(TEXT("One repeat consumes its original quantities"), MOIngredientPlans::Consume(Reservation, {{Water, 1}, {Safe, 1}}).IsComplete());
	TestTrue(TEXT("Consumed reservation is empty"), Reservation.IsEmpty());
	Definitions.Rows[Water].LiquidMeasureMl = 0.1f;
	Stock.Empty();
	TestFalse(TEXT("A tiny modded measure cannot be created from empty stock by float tolerance"), MOIngredientPlans::Reserve(Stock, {{Water, 1}}, Reservation, Lookup).IsComplete());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOIngredientReservationRefund, "MOFramework.Inventory.Reservation.Refund", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOIngredientReservationRefund::RunTest(const FString&)
{
	using namespace MOIngredientReservationTests;
	FDefinitions Definitions;
	auto Lookup = [&Definitions](FName Id, FMOItemDefinitionRow& Out) { return Definitions.Lookup(Id, Out); };
	TArray<FMOInventoryEntry> Stock = {Entry(Jar, 1, 249.75f, true, 35.0f), Entry(Tool)};
	Stock[1].CurrentDurability = 7;
	const FGuid ToolGuid = Stock[1].ItemGuid;
	const FMOLiquidContents Original = Stock[0].Liquid;
	FMOIngredientReservation Reservation;
	TestTrue(TEXT("Reserve real volume within existing float tolerance"), MOIngredientPlans::Reserve(Stock, {{Water, 1}, {Tool, 1}}, Reservation, Lookup).IsComplete());
	TestEqual(TEXT("The reservation did not manufacture tolerance water"), ReservedMl(Reservation), 249.75f);
	TestTrue(TEXT("Refund to original vessel succeeds"), MOIngredientPlans::Return(Stock, 2, Reservation, Lookup).IsComplete());
	TestTrue(TEXT("Original volume, salts, source and boiled state restored"), Stock[0].Liquid == Original);
	TestEqual(TEXT("Physical identity restored"), Stock[1].ItemGuid, ToolGuid);
	TestEqual(TEXT("Physical wear restored"), Stock[1].CurrentDurability, 7);
	TestTrue(TEXT("Reserve again for capacity failure"), MOIngredientPlans::Reserve(Stock, {{Water, 1}, {Tool, 1}}, Reservation, Lookup).IsComplete());
	Stock = {Entry(Stone, 10)}; // original vessel has left; all target slots are occupied
	TestEqual(TEXT("Full destination refuses refund"), MOIngredientPlans::Return(Stock, 1, Reservation, Lookup).Failure, EMOIngredientFailure::Capacity);
	TestEqual(TEXT("Unreturned water remains owned"), ReservedMl(Reservation), 249.75f);
	FMOIngredientReservation Saved = Reservation;
	TArray<uint8> Bytes;
	{
		FMemoryWriter Memory(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Memory, false);
		FMOIngredientReservation::StaticStruct()->SerializeItem(Archive, &Saved, nullptr);
	}
	FMOIngredientReservation Loaded;
	{
		FMemoryReader Memory(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Memory, true);
		FMOIngredientReservation::StaticStruct()->SerializeItem(Archive, &Loaded, nullptr);
	}
	Stock = {Entry(Jar)};
	TestTrue(TEXT("Persisted remainder can be returned when room appears"), MOIngredientPlans::Return(Stock, 2, Loaded, Lookup).IsComplete());
	TestTrue(TEXT("Persisted liquid profile remains original"), Stock[0].Liquid == Original);
	TestEqual(TEXT("Persisted tool identity remains original"), Stock[1].ItemGuid, ToolGuid);
	TestTrue(TEXT("Every returned lot releases ownership"), Loaded.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOIngredientReservationTransfer, "MOFramework.Inventory.Reservation.Transfer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOIngredientReservationTransfer::RunTest(const FString&)
{
	using namespace MOIngredientReservationTests;
	const FName RealWater(TEXT("Water01")), RealJar(TEXT("CrudePottery01")), RealStick(TEXT("Stick01"));
	FMOItemDefinitionRow StickDefinition;
	if (!TestTrue(TEXT("Live stick definition is available"), UMOItemDatabaseSettings::GetItemDefinition(RealStick, StickDefinition))) return false;
	FWorld World;
	auto* Source = World.Spawn(3);
	auto* Target = World.Spawn(1);
	const FGuid SourceJar = FGuid::NewGuid(), SourceStick = FGuid::NewGuid(), TargetJar = FGuid::NewGuid();
	Source->AddItemByGuid(SourceJar, RealJar, 1);
	Source->AddItemByGuid(SourceStick, RealStick, 2);
	const FGuid UntouchedJar = FGuid::NewGuid();
	Source->AddItemByGuid(UntouchedJar, RealJar, 1);
	FMOLiquidContents Original;
	Original.Millilitres = 500;
	Original.bBoiled = true;
	Original.SalinityGPerL = 0.2f;
	Original.Source = EMOWaterBodyKind::Pond;
	Source->SetLiquid(SourceJar, Original);
	Target->AddItemByGuid(FGuid::NewGuid(), RealStick, UMOInventoryComponent::GetItemStackLimit(StickDefinition));
	const TMap<FName, int32> Requirements = {{RealWater, 1}, {RealStick, 2}};
	TestEqual(TEXT("Full target reports capacity"), Source->TransferIngredientsTo(Target, Requirements).Failure, EMOIngredientFailure::Capacity);
	TestEqual(TEXT("Refused transfer leaves source water"), Source->GetInstanceState(SourceJar).Liquid.Millilitres, 500.0f);
	TestEqual(TEXT("Refused transfer leaves source physical material"), Source->GetItemCountByDefinitionId(RealStick), 2);
	Target->ClearInventoryAndSlots();
	Target->SetSlotCountAuthority(2);
	Target->AddItemByGuid(TargetJar, RealJar, 1);
	const int32 SourceKey = Source->Inventory.Entries[0].ReplicationKey;
	const int32 TargetKey = Target->Inventory.Entries[0].ReplicationKey;
	const int32 SourceId = Source->Inventory.Entries[0].ReplicationID;
	const int32 TargetId = Target->Inventory.Entries[0].ReplicationID;
	const int32 UntouchedId = Source->Inventory.Entries[2].ReplicationID;
	const int32 UntouchedKey = Source->Inventory.Entries[2].ReplicationKey;
	TestTrue(TEXT("Combined physical/liquid transfer fits"), Source->TransferIngredientsTo(Target, Requirements).IsComplete());
	TestEqual(TEXT("Source has its remaining water"), Source->GetInstanceState(SourceJar).Liquid.Millilitres, 250.0f);
	TestEqual(TEXT("Target receives only transferred water"), Target->GetInstanceState(TargetJar).Liquid.Millilitres, 250.0f);
	TestEqual(TEXT("Water profile preserved in the target"), Target->GetInstanceState(TargetJar).Liquid.SalinityGPerL, Original.SalinityGPerL);
	TestTrue(TEXT("Boiled state preserved in the target"), Target->GetInstanceState(TargetJar).Liquid.bBoiled);
	FMOInventoryEntry Moved;
	TestTrue(TEXT("Physical stack keeps identity"), Target->TryGetEntryByGuid(SourceStick, Moved));
	TestEqual(TEXT("All physical units moved"), Moved.Quantity, 2);
	TestTrue(TEXT("Changed surviving source container will replicate"), Source->Inventory.Entries[0].ReplicationKey > SourceKey);
	TestTrue(TEXT("Changed surviving target container will replicate"), Target->Inventory.Entries[0].ReplicationKey > TargetKey);
	TestEqual(TEXT("Changed source keeps its serializer identity"), Source->Inventory.Entries[0].ReplicationID, SourceId);
	TestEqual(TEXT("Changed target keeps its serializer identity"), Target->Inventory.Entries[0].ReplicationID, TargetId);
	const auto* Unchanged = Source->Inventory.Entries.FindByPredicate([&UntouchedJar](const auto& Entry) { return Entry.ItemGuid == UntouchedJar; });
	TestTrue(TEXT("Unchanged survivor keeps both serializer identity and key"), Unchanged && Unchanged->ReplicationID == UntouchedId && Unchanged->ReplicationKey == UntouchedKey);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOIngredientContainerTransport, "MOFramework.Inventory.Reservation.ContainerTransport", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOIngredientContainerTransport::RunTest(const FString&)
{
	using namespace MOIngredientReservationTests;
	FWorld World;
	auto* Source = World.Spawn(2);
	auto* Pawn = World.Spawn(1);
	const FGuid VesselGuid = FGuid::NewGuid(), BlockerGuid = FGuid::NewGuid();
	FMOInventoryAddOptions Worn;
	Worn.State.Durability = 13;
	Worn.State.Liquid.Millilitres = 500;
	Worn.State.Liquid.Source = EMOWaterBodyKind::River;
	Worn.State.Liquid.SalinityGPerL = 0.2f;
	Worn.State.Liquid.IllnessChancePerLitre = 0.005f;
	Worn.State.Liquid.bBoiled = false;
	TestTrue(TEXT("Source has a real filled vessel"), Source->TryAddItem(VesselGuid, TEXT("CrudePottery01"), 1, Worn).IsComplete());
	Pawn->AddItemByGuid(BlockerGuid, TEXT("Stick01"), 1);
	TArray<FGuid> Carried;
	FMOIngredientTransferOptions Carry;
	Carry.bCarrySourceContainers = true;
	const TMap<FName, int32> Need = {{TEXT("Water01"), 1}};
	TestEqual(TEXT("Full pawn refuses the whole transport"), Source->TransferIngredientsTo(Pawn, Need, Carry, &Carried).Failure, EMOIngredientFailure::Capacity);
	TestTrue(TEXT("Refused transport leaves all source water intact"), Source->GetInstanceState(VesselGuid).Liquid == Worn.State.Liquid);
	TestEqual(TEXT("Refused transport reports no carried vessel"), Carried.Num(), 0);
	Pawn->RemoveItemByGuid(BlockerGuid, 1);
	TestTrue(TEXT("An empty-handed pawn can carry the source vessel"), Source->TransferIngredientsTo(Pawn, Need, Carry, &Carried).IsComplete());
	TestEqual(TEXT("Transport reports the borrowed identity"), Carried.Num() == 1 ? Carried[0] : FGuid(), VesselGuid);
	FMOInventoryEntry Vessel;
	TestFalse(TEXT("The vessel no longer exists in storage"), Source->TryGetEntryByGuid(VesselGuid, Vessel));
	TestTrue(TEXT("The vessel itself moved to the pawn"), Pawn->TryGetEntryByGuid(VesselGuid, Vessel));
	TestEqual(TEXT("The vessel retains wear"), Vessel.CurrentDurability, Worn.State.Durability);
	TestTrue(TEXT("Transport carries all original water, including the unrequested remainder"), Vessel.Liquid == Worn.State.Liquid);
	FMOIngredientReservation Reserved;
	TestTrue(TEXT("The carried vessel supplies the real crafting reservation"), Pawn->ReserveIngredients(Need, Reserved).IsComplete());
	TestEqual(TEXT("Only the requested 250 ml was reserved"), ReservedMl(Reserved), 250.0f);
	TestEqual(TEXT("The vessel still carries the other 250 ml"), Pawn->GetInstanceState(VesselGuid).Liquid.Millilitres, 250.0f);
	TestTrue(TEXT("Returning unused material preserves the carried vessel"), Pawn->ReturnReservedIngredients(Reserved).IsComplete());
	TestTrue(TEXT("The original physical vessel can return after timed handling"), Pawn->TryTransferItem(VesselGuid, Source).IsComplete());
	TestTrue(TEXT("Round trip preserves water profile and quantity"), Source->GetInstanceState(VesselGuid).Liquid == Worn.State.Liquid);
	TestEqual(TEXT("Pawn owns nothing after return"), Pawn->Inventory.Entries.Num(), 0);
	// Pottery crafting borrows a pot for water AND produces another pot. The borrowed
	// vessel must be returned in addition to the actual output, never substituted for it.
	TestTrue(TEXT("Borrow the source vessel for another craft"), Source->TransferIngredientsTo(Pawn, Need, Carry, &Carried).IsComplete());
	TestTrue(TEXT("Reserve a measure for the craft"), Pawn->ReserveIngredients(Need, Reserved).IsComplete());
	TestTrue(TEXT("Consume only that real reserved measure"), MOIngredientPlans::Consume(Reserved, Need).IsComplete());
	Pawn->SetSlotCountAuthority(2);
	const FGuid ProductGuid = FGuid::NewGuid();
	Pawn->AddItemByGuid(ProductGuid, TEXT("CrudePottery01"), 1);
	FMOIngredientTransferOptions Deposit;
	Deposit.bCarrySourceContainers = true;
	Deposit.AlsoCarryItemGuids = {VesselGuid};
	const TMap<FName, int32> Product = {{TEXT("CrudePottery01"), 1}};
	Source->SetSlotCountAuthority(1);
	TestEqual(TEXT("One slot cannot receive output plus borrowed vessel"), Pawn->TransferIngredientsTo(Source, Product, Deposit).Failure, EMOIngredientFailure::Capacity);
	TestEqual(TEXT("Refused deposit leaves both pots on the pawn"), Pawn->GetItemCountByDefinitionId(TEXT("CrudePottery01")), 2);
	TestEqual(TEXT("Refused deposit leaves storage untouched"), Source->Inventory.Entries.Num(), 0);
	Source->SetSlotCountAuthority(2);
	TestTrue(TEXT("Output and borrowed vessel deposit together"), Pawn->TransferIngredientsTo(Source, Product, Deposit).IsComplete());
	TestTrue(TEXT("Actual produced pot was delivered"), Source->TryGetEntryByGuid(ProductGuid, Vessel));
	TestTrue(TEXT("Borrowed pot also returned with its identity"), Source->TryGetEntryByGuid(VesselGuid, Vessel));
	TestEqual(TEXT("Unconsumed water returned inside the vessel"), Vessel.Liquid.Millilitres, 250.0f);
	TestEqual(TEXT("Nothing is stranded on the pawn"), Pawn->Inventory.Entries.Num(), 0);
	return true;
}
#endif
