#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/SceneComponent.h"
#include "MOCraftingQueueComponent.h"
#include "MOCraftingStationActor.h"
#include "MOCraftingSubsystem.h"
#include "MOInventoryComponent.h"
#include "MOIdentityComponent.h"
#include "MOIdentifiableInterface.h"
#include "MOBuildProgressComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOItemDatabaseSettings.h"
#include "MOCraftingTransactionProbe.h"
#include "MOSurvivorController.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOSurvivorIterationProbe.h"
#include "MOSurvivorCraftSaveProbe.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace MOCraftingContextTests
{
	const FName RecipeId(TEXT("AuditC_ContextRecipe"));
	struct FWorld
	{
		UWorld* World;
		UDataTable* Recipes;
		APawn* Pawn;
		APlayerController* PC;
		UMOInventoryComponent* Inventory;
		UMOCraftingQueueComponent* Queue;
		UMOCraftingSubsystem* Crafting;
		FMORecipeDefinitionRow Recipe;
		FWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MOCraftingContextWorld"));
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Crafting = World->GetSubsystem<UMOCraftingSubsystem>();
			Pawn = World->SpawnActor<APawn>();
			auto* Root = NewObject<USceneComponent>(Pawn);
			Pawn->SetRootComponent(Root);
			Root->RegisterComponent();
			Inventory = NewObject<UMOInventoryComponent>(Pawn);
			Queue = NewObject<UMOCraftingQueueComponent>(Pawn);
			Pawn->AddInstanceComponent(Inventory);
			Pawn->AddInstanceComponent(Queue);
			Inventory->RegisterComponent();
			Queue->RegisterComponent();
			Pawn->DispatchBeginPlay();
			PC = World->SpawnActor<APlayerController>();
			PC->Possess(Pawn);
			Recipes = GetDefault<UMORecipeDatabaseSettings>()->GetRecipeDefinitionsDataTable();
			Recipe.RecipeId = RecipeId;
			Recipe.CraftTime = 60.0f;
			Recipe.RequiredStation = EMOCraftingStation::Forge;
			Recipe.bRequiresDiscovery = false;
			PublishRecipe();
		}
		void PublishRecipe()
		{
			if (Recipes) Recipes->AddRow(RecipeId, Recipe);
			UMORecipeDatabaseSettings::InvalidateCache();
		}
		~FWorld()
		{
			if (Recipes) Recipes->RemoveRow(RecipeId);
			UMORecipeDatabaseSettings::InvalidateCache();
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		AMOCraftingStationActor* Station(bool bComplete = true)
		{
			auto* Result = World->SpawnActor<AMOCraftingStationActor>();
			Result->DispatchBeginPlay();
			Result->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
			Result->IdentityComponent->GetOrCreateGuid();
			Result->StationType = EMOCraftingStation::Forge;
			FMOBuildProgress Progress;
			Progress.State = bComplete ? EMOBuildState::Complete : EMOBuildState::Ghost;
			Result->BuildProgressComponent->ApplySaveData(Progress);
			return Result;
		}
		void Tick(float Delta)
		{
			static_cast<UActorComponent*>(Queue)->TickComponent(Delta, LEVELTICK_All, nullptr);
		}
		void CompleteOne()
		{
			Queue->PauseCrafting();
			FMOCraftingQueueSaveData Save;
			Queue->BuildSaveData(Save);
			if (Save.QueuedCrafts.IsEmpty()) return;
			// A persisted completed interval, advanced through the real load path; no instant production bypass.
			Save.QueuedCrafts[0].Progress = 1.0f;
			Save.bWasActive = true;
			Save.PausedAt = FDateTime::UtcNow() - FTimespan::FromSeconds(1);
			Queue->ApplySaveData(Save, true);
		}
	};
	FMOCraftingQueueSaveData RoundTrip(FMOCraftingQueueSaveData Save)
	{
		TArray<uint8> Bytes;
		{
			FMemoryWriter Memory(Bytes);
			FObjectAndNameAsStringProxyArchive Archive(Memory, false);
			FMOCraftingQueueSaveData::StaticStruct()->SerializeItem(Archive, &Save, nullptr);
		}
		FMOCraftingQueueSaveData Loaded;
		FMemoryReader Memory(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Memory, true);
		FMOCraftingQueueSaveData::StaticStruct()->SerializeItem(Archive, &Loaded, nullptr);
		return Loaded;
	}
	UMOInventoryComponent* Storage(FWorld& F, int32 Slots)
	{
		auto* Actor = F.World->SpawnActor<AActor>();
		auto* Root = NewObject<USceneComponent>(Actor);
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Actor->SetActorLocation(FVector(1000, 0, 0));
		auto* Inventory = NewObject<UMOInventoryComponent>(Actor);
		Actor->AddInstanceComponent(Inventory);
		Inventory->RegisterComponent();
		Actor->DispatchBeginPlay();
		Inventory->SetSlotCountAuthority(Slots);
		return Inventory;
	}
	UMOSurvivorJobQueueComponent* AssignSurvivor(FWorld& F, AMOSurvivorController*& OutController)
	{
		F.PC->UnPossess();
		auto* Jobs = NewObject<UMOSurvivorJobQueueComponent>(F.Pawn);
		F.Pawn->AddInstanceComponent(Jobs);
		Jobs->RegisterComponent();
		OutController = F.World->SpawnActor<AMOSurvivorController>();
		OutController->Possess(F.Pawn);
		return Jobs;
	}
	template<typename T> T SerializedCopy(T Save)
	{
		TArray<uint8> Bytes;
		{
			FMemoryWriter Memory(Bytes); FObjectAndNameAsStringProxyArchive Archive(Memory, false);
			T::StaticStruct()->SerializeItem(Archive, &Save, nullptr);
		}
		T Loaded;
		FMemoryReader Memory(Bytes); FObjectAndNameAsStringProxyArchive Archive(Memory, true);
		T::StaticStruct()->SerializeItem(Archive, &Loaded, nullptr);
		return Loaded;
	}
	void RestoreCraftExecutor(FWorld& F, AMOSurvivorController*& AI, UMOSurvivorJobQueueComponent* Jobs,
		UMOInventoryComponent* Store, FMOSurvivorJobQueueSaveData JobSave, FMOCraftingQueueSaveData CraftSave,
		FMOInventorySaveData PawnSave, FMOInventorySaveData StoreSave, bool bOfflineComplete = false)
	{
		AI->UnPossess(); AI->Destroy(); AI = nullptr;
		F.Inventory->ApplySaveDataAuthority(SerializedCopy(PawnSave));
		Store->ApplySaveDataAuthority(SerializedCopy(StoreSave));
		if (bOfflineComplete && !CraftSave.QueuedCrafts.IsEmpty())
		{
			CraftSave.QueuedCrafts[0].Progress = 1;
			CraftSave.bWasActive = true;
			CraftSave.PausedAt = FDateTime::UtcNow() - FTimespan::FromSeconds(1);
		}
		// Real persistence ordering: queue/offline completion BEFORE jobs and controller binding.
		F.Queue->ApplySaveData(SerializedCopy(CraftSave), bOfflineComplete);
		Jobs->ApplySaveDataAuthority(SerializedCopy(JobSave));
		AI = F.World->SpawnActor<AMOSurvivorController>();
		AI->Possess(F.Pawn);
	}
	void ReloadCraftExecutor(FWorld& F, AMOSurvivorController*& AI, UMOSurvivorJobQueueComponent* Jobs,
		UMOInventoryComponent* Store, bool bOfflineComplete = false)
	{
		FMOSurvivorJobQueueSaveData JobSave; Jobs->BuildSaveData(JobSave);
		FMOCraftingQueueSaveData CraftSave; F.Queue->BuildSaveData(CraftSave);
		FMOInventorySaveData PawnSave, StoreSave;
		F.Inventory->BuildSaveData(PawnSave); Store->BuildSaveData(StoreSave);
		RestoreCraftExecutor(F, AI, Jobs, Store, JobSave, CraftSave, PawnSave, StoreSave, bOfflineComplete);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingContextGate, "MOFramework.Crafting.ExecutionContext", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingContextGate::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes) || !TestNotNull(TEXT("Crafting subsystem"), F.Crafting)) return false;
	TestFalse(TEXT("A forged Forge enum does not create a station"), F.Queue->EnqueueCraft(RecipeId, 1, EMOCraftingStation::Forge));
	auto* Station = F.Station(false);
	TestEqual(TEXT("Incomplete station refused"), F.Crafting->ResolveExecutionContext(RecipeId, F.Pawn, Station).Failure, EMOCraftingContextFailure::IncompleteStation);
	FMOBuildProgress Complete;
	Complete.State = EMOBuildState::Complete;
	Station->BuildProgressComponent->ApplySaveData(Complete);
	Station->StationType = EMOCraftingStation::Campfire;
	TestEqual(TEXT("Wrong real station refused"), F.Crafting->ResolveExecutionContext(RecipeId, F.Pawn, Station).Failure, EMOCraftingContextFailure::WrongStation);
	Station->StationType = EMOCraftingStation::Forge;
	Station->SetActorLocation(FVector(10000, 0, 0));
	TestEqual(TEXT("Distant station refused"), F.Crafting->ResolveExecutionContext(RecipeId, F.Pawn, Station).Failure, EMOCraftingContextFailure::OutOfRange);
	Station->SetActorLocation(FVector(100, 0, 0));
	Station->bRequiresFuel = true;
	TestEqual(TEXT("Unfueled station refused"), F.Crafting->ResolveExecutionContext(RecipeId, F.Pawn, Station).Failure, EMOCraftingContextFailure::InactiveStation);
	Station->CurrentFuel = 100;
	Station->SetStationActive(true);
	F.Recipe.MinimumProcessTemperatureCelsius = 1200;
	F.PublishRecipe();
	Station->AchievableProcessTemperatureCelsius = 900;
	TestEqual(TEXT("Insufficient process heat refused"), F.Crafting->ResolveExecutionContext(RecipeId, F.Pawn, Station).Failure, EMOCraftingContextFailure::InsufficientTemperature);
	Station->AchievableProcessTemperatureCelsius = 1250;
	TestTrue(TEXT("A complete nearby fueled capable station passes"), F.Crafting->ResolveExecutionContext(RecipeId, F.Pawn, Station).IsUsable());
	F.Recipe.bIsHarvestRecipe = true;
	F.PublishRecipe();
	TestFalse(TEXT("Harvest recipe refused even with a station"), F.Queue->EnqueueCraftAtStation(RecipeId, 1, Station));
	TestEqual(TEXT("Harvest refusal names required action"), F.Crafting->ResolveExecutionContext(RecipeId, F.Pawn, Station).Failure, EMOCraftingContextFailure::HarvestActionRequired);
	TestFalse(TEXT("Output producer also refuses harvest recipe"), F.Crafting->ProduceOutputsOnly(RecipeId, F.Inventory, nullptr, Station).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingStationBindingGate, "MOFramework.Crafting.StationBinding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingStationBindingGate::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	auto* First = F.Station();
	auto* Second = F.Station();
	Second->SetActorLocation(FVector(200, 0, 0));
	TestTrue(TEXT("Enqueue at the first real station"), F.Queue->EnqueueCraftAtStation(RecipeId, 1, First));
	F.Queue->SetActiveStation(Second);
	FMOCraftingQueueEntry Entry;
	F.Queue->GetCurrentCraft(Entry);
	TestEqual(TEXT("Menu change does not retarget existing work"), Entry.StationGuid, IMOIdentifiableInterface::Execute_GetPersistentGuid(First));
	First->SetActorLocation(FVector(10000, 0, 0));
	F.Tick(1.0f);
	TestFalse(TEXT("Work stops when its bound station leaves reach"), F.Queue->IsCraftingActive());
	TestTrue(TEXT("Work waits for its original context"), F.Queue->IsWaitingForCraftingContext());
	TestFalse(TEXT("Suspended work carries a visible reason"), F.Queue->GetCraftingBlockReason().IsEmpty());
	F.Queue->GetCurrentCraft(Entry);
	const float PausedProgress = Entry.Progress;
	F.Tick(60.0f);
	F.Queue->GetCurrentCraft(Entry);
	TestEqual(TEXT("Waiting never banks simulated work"), Entry.Progress, PausedProgress);
	TestEqual(TEXT("Queue remains, rather than completing against the second station"), F.Queue->GetQueueLength(), 1);
	FMOCraftingQueueSaveData Save;
	F.Queue->BuildSaveData(Save);
	TestTrue(TEXT("Context suspension persists"), Save.bAwaitingContext);
	TestEqual(TEXT("Bound identity persists"), Save.QueuedCrafts[0].StationGuid, Entry.StationGuid);
	TArray<uint8> Bytes;
	{
		FMemoryWriter Memory(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Memory, false);
		FMOCraftingQueueSaveData::StaticStruct()->SerializeItem(Archive, &Save, nullptr);
	}
	FMOCraftingQueueSaveData Loaded;
	{
		FMemoryReader Memory(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Memory, true);
		FMOCraftingQueueSaveData::StaticStruct()->SerializeItem(Archive, &Loaded, nullptr);
	}
	TestTrue(TEXT("Reflected serialization retains suspension"), Loaded.bAwaitingContext);
	TestEqual(TEXT("Reflected serialization retains station identity"), Loaded.QueuedCrafts[0].StationGuid, Entry.StationGuid);
	F.Queue->ApplySaveData(Loaded, false);
	First->SetActorLocation(FVector(100, 0, 0));
	F.Queue->SetActiveStation(First);
	F.Tick(1.0f);
	TestTrue(TEXT("Returning to the bound station resumes work"), F.Queue->IsCraftingActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingCancelTransportGate, "MOFramework.Crafting.CancelTransport", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingCancelTransportGate::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	F.Recipe.RequiredStation = EMOCraftingStation::None;
	F.PublishRecipe();
	TestTrue(TEXT("Hand craft enqueues"), F.Queue->EnqueueCraft(RecipeId));
	FMOCraftingQueueEntry Entry;
	F.Queue->GetCurrentCraft(Entry);
	UFunction* One = F.Queue->FindFunction(TEXT("ServerRequestCancelCraft"));
	UFunction* All = F.Queue->FindFunction(TEXT("ServerRequestCancelAllCrafts"));
	TestTrue(TEXT("Cancel-one uses a reliable server RPC"), One && One->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetReliable));
	TestTrue(TEXT("Cancel-all uses a reliable server RPC"), All && All->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetReliable));
	// A server RPC can resolve locally in a dead/no-driver world. It must return without forwarding recursively or mutating client state.
	F.Pawn->SetRole(ROLE_AutonomousProxy);
	F.Queue->CancelCraft(Entry.EntryId, false);
	F.Queue->CancelAllCrafts(false);
	TestEqual(TEXT("Dead-world client cancel does not mutate the queue"), F.Queue->GetQueueLength(), 1);
	F.Pawn->SetRole(ROLE_Authority);
	TestTrue(TEXT("Authoritative cancellation succeeds"), F.Queue->CancelCraft(Entry.EntryId));
	TestEqual(TEXT("Authoritative cancellation removes the entry"), F.Queue->GetQueueLength(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingReservedRefundGate, "MOFramework.Crafting.ReservedRefund", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingReservedRefundGate::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	F.Recipe.RequiredStation = EMOCraftingStation::None;
	FMORecipeIngredient Stick, Water;
	Stick.ItemDefinitionId = TEXT("Stick01");
	Water.ItemDefinitionId = TEXT("Water01");
	F.Recipe.Ingredients = {Stick, Water};
	F.PublishRecipe();
	F.Inventory->SetSlotCountAuthority(2);
	const FGuid JarGuid = FGuid::NewGuid(), StickGuid = FGuid::NewGuid(), BlockerGuid = FGuid::NewGuid();
	F.Inventory->AddItemByGuid(JarGuid, TEXT("CrudePottery01"), 1);
	FMOInventoryAddOptions Worn;
	Worn.State.Durability = 17;
	F.Inventory->TryAddItem(StickGuid, TEXT("Stick01"), 2, Worn);
	FMOLiquidContents Original;
	Original.Millilitres = 500;
	Original.Source = EMOWaterBodyKind::Sea;
	Original.SalinityGPerL = 35;
	Original.bBoiled = true;
	F.Inventory->SetLiquid(JarGuid, Original);
	TestTrue(TEXT("Repeated craft reserves physical items and actual water"), F.Queue->EnqueueCraft(RecipeId, 2));
	FMOCraftingQueueSaveData Save;
	F.Queue->BuildSaveData(Save);
	TestTrue(TEXT("New queue entry records material provenance"), Save.QueuedCrafts.Num() == 1 && Save.QueuedCrafts[0].bHasCapturedReservation);
	TestEqual(TEXT("Reserved water left the vessel"), F.Inventory->GetInstanceState(JarGuid).Liquid.Millilitres, 0.0f);
	F.Queue->ApplySaveData(RoundTrip(Save), false);
	// Changing recipe quantities cannot change what a previously queued entry owns/refunds.
	F.Recipe.Ingredients[0].Quantity = 99;
	F.PublishRecipe();
	F.Inventory->AddItemByGuid(BlockerGuid, TEXT("Stick01"), 1);
	F.Queue->CancelAllCrafts();
	TestTrue(TEXT("Refund restores original liquid salts/source/boiled state"), F.Inventory->GetInstanceState(JarGuid).Liquid == Original);
	TestTrue(TEXT("Full inventory retains the undelivered physical refund"), F.Queue->GetPendingMaterialLotCount() > 0);
	F.Queue->BuildSaveData(Save);
	const auto Loaded = RoundTrip(Save);
	TestEqual(TEXT("Persisted refund retains original quantity"), Loaded.PendingReturns.Lots.Num() == 1 ? Loaded.PendingReturns.Lots[0].Quantity : 0, 2);
	F.Queue->ApplySaveData(Loaded, false);
	F.Inventory->RemoveItemByGuid(BlockerGuid, 1);
	F.Queue->DeliverPendingMaterials();
	FMOInventoryEntry Returned;
	TestTrue(TEXT("Original physical GUID returned"), F.Inventory->TryGetEntryByGuid(StickGuid, Returned));
	TestEqual(TEXT("Original wear returned"), Returned.CurrentDurability, 17);
	TestEqual(TEXT("Original physical quantity returned"), Returned.Quantity, 2);
	TestEqual(TEXT("All returned material releases escrow ownership"), F.Queue->GetPendingMaterialLotCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingCompletionOwnershipGate, "MOFramework.Crafting.CompletionOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingCompletionOwnershipGate::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	F.Recipe.RequiredStation = EMOCraftingStation::None;
	FMORecipeIngredient Input;
	Input.ItemDefinitionId = TEXT("Stick01");
	F.Recipe.Ingredients = {Input};
	FMORecipeOutput Product;
	Product.ItemDefinitionId = TEXT("CrudePottery01");
	F.Recipe.Outputs = {Product};
	F.PublishRecipe();
	F.Inventory->SetSlotCountAuthority(1);
	const FGuid MaterialGuid = FGuid::NewGuid(), BlockerGuid = FGuid::NewGuid();
	F.Inventory->AddItemByGuid(MaterialGuid, Input.ItemDefinitionId, 2);
	TestTrue(TEXT("Two repeats enqueued"), F.Queue->EnqueueCraft(RecipeId, 2));
	F.Inventory->AddItemByGuid(BlockerGuid, Product.ItemDefinitionId, 1);
	auto* Probe = NewObject<UMOCraftingTransactionProbe>();
	Probe->Queue = F.Queue;
	F.Queue->OnCraftCompleted.AddDynamic(Probe, &UMOCraftingTransactionProbe::HandleCompleted);
	F.CompleteOne();
	TestEqual(TEXT("One timed completion callback"), Probe->Calls, 1);
	const auto& Seen = Probe->AtCompletion;
	TestEqual(TEXT("Observer save sees completed repeat already recorded"), Seen.QueuedCrafts.Num() == 1 ? Seen.QueuedCrafts[0].CompletedCount : -1, 1);
	TestEqual(TEXT("Observer save owns only the unconsumed repeat"), Seen.QueuedCrafts.Num() == 1 && Seen.QueuedCrafts[0].ReservedIngredients.Lots.Num() == 1 ? Seen.QueuedCrafts[0].ReservedIngredients.Lots[0].Quantity : -1, 1);
	TestEqual(TEXT("Full inventory cannot destroy a completed product"), Seen.PendingProducts.Lots.Num(), 1);
	const auto Restored = RoundTrip(Probe->AfterCancellation);
	TestEqual(TEXT("Reentrant cancel removes queued work"), Restored.QueuedCrafts.Num(), 0);
	TestEqual(TEXT("Reentrant cancel refunds only one remaining input"), Restored.PendingReturns.Lots.Num() == 1 ? Restored.PendingReturns.Lots[0].Quantity : -1, 1);
	TestEqual(TEXT("Reentrant cancellation retains completed product"), Restored.PendingProducts.Lots.Num(), 1);
	F.Queue->OnCraftCompleted.RemoveDynamic(Probe, &UMOCraftingTransactionProbe::HandleCompleted);
	F.Queue->ApplySaveData(Restored, false);
	F.Inventory->RemoveItemByGuid(BlockerGuid, 1);
	F.Queue->DeliverPendingMaterials();
	TestEqual(TEXT("Exactly one remaining input can return"), F.Inventory->GetItemCountByDefinitionId(Input.ItemDefinitionId), 1);
	F.Inventory->RemoveItemByDefinitionId(Input.ItemDefinitionId, 1);
	F.Queue->DeliverPendingMaterials();
	TestEqual(TEXT("Exactly one completed product can be delivered later"), F.Inventory->GetItemCountByDefinitionId(Product.ItemDefinitionId), 1);
	TestEqual(TEXT("No material remains stranded after delivery"), F.Queue->GetPendingMaterialLotCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingLegacyRefundGate, "MOFramework.Crafting.LegacyRefund", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingLegacyRefundGate::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	F.Recipe.RequiredStation = EMOCraftingStation::None;
	F.PublishRecipe();
	FMOCraftingQueueSaveData Old;
	FMOCraftingQueueEntry Entry;
	Entry.RecipeId = RecipeId;
	Entry.bIngredientsConsumed = true;
	Old.QueuedCrafts.Add(Entry);
	F.Queue->ApplySaveData(Old, false);
	F.Queue->CancelAllCrafts();
	TestEqual(TEXT("Legacy cancellation retains unresolved provenance record"), F.Queue->GetUnresolvedLegacyRefundCount(), 1);
	TestEqual(TEXT("Legacy cancellation never invents physical or liquid material"), F.Inventory->Inventory.Entries.Num(), 0);
	FMOCraftingQueueSaveData Saved;
	F.Queue->BuildSaveData(Saved);
	TestEqual(TEXT("Unresolved legacy refund survives reflected serialization"), RoundTrip(Saved).UnresolvedLegacyRefunds.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingReservationAtomicityGate, "MOFramework.Crafting.ReservationAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingReservationAtomicityGate::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	F.Recipe.RequiredStation = EMOCraftingStation::None;
	FMORecipeIngredient Water, Safe;
	Water.ItemDefinitionId = TEXT("Water01");
	Safe.ItemDefinitionId = TEXT("CleanWater01");
	F.Recipe.Ingredients = {Water, Safe};
	F.PublishRecipe();
	const FGuid Jar = FGuid::NewGuid();
	F.Inventory->AddItemByGuid(Jar, TEXT("CrudePottery01"), 1);
	FMOLiquidContents Liquid;
	Liquid.Millilitres = 250;
	Liquid.bBoiled = true;
	Liquid.Source = EMOWaterBodyKind::River;
	F.Inventory->SetLiquid(Jar, Liquid);
	TestFalse(TEXT("The same 250 ml cannot fund separate safe and any-water requirements"), F.Queue->EnqueueCraft(RecipeId));
	TestTrue(TEXT("Joint refusal leaves the original water intact"), F.Inventory->GetInstanceState(Jar).Liquid == Liquid);
	TestEqual(TEXT("Joint refusal creates no queue ownership"), F.Queue->GetQueueLength(), 0);
	FMORecipeIngredient Stick;
	Stick.ItemDefinitionId = TEXT("Stick01");
	Stick.Quantity = 1;
	F.Recipe.Ingredients = {Stick, Stick};
	F.PublishRecipe();
	F.Inventory->AddItemByGuid(FGuid::NewGuid(), Stick.ItemDefinitionId, 1);
	TestFalse(TEXT("Duplicate ingredient rows are combined before availability/mutation"), F.Queue->EnqueueCraft(RecipeId));
	TestEqual(TEXT("Duplicate-row refusal does not partially consume a stack"), F.Inventory->GetItemCountByDefinitionId(Stick.ItemDefinitionId), 1);
	F.Recipe.Ingredients[0].Quantity = MAX_int32;
	F.PublishRecipe();
	TestFalse(TEXT("Overflowing repeat requirements cannot enqueue"), F.Queue->EnqueueCraft(RecipeId, 1000));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingSurvivorLogisticsGate, "MOFramework.Crafting.SurvivorLogistics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingSurvivorLogisticsGate::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	F.Recipe.RequiredStation = EMOCraftingStation::None;
	F.Recipe.CraftTime = 180.0f;
	FMORecipeIngredient Water, Stick;
	Water.ItemDefinitionId = TEXT("Water01");
	Stick.ItemDefinitionId = TEXT("Stick01");
	F.Recipe.Ingredients = {Water, Stick};
	FMORecipeOutput Pot;
	Pot.ItemDefinitionId = TEXT("CrudePottery01");
	F.Recipe.Outputs = {Pot};
	F.PublishRecipe();
	F.Inventory->SetSlotCountAuthority(3);
	auto* Store = Storage(F, 2);
	const FGuid Vessel = FGuid::NewGuid();
	FMOInventoryAddOptions Worn;
	Worn.State.Durability = 13;
	Worn.State.Liquid.Millilitres = 500;
	Worn.State.Liquid.Source = EMOWaterBodyKind::River;
	Worn.State.Liquid.SalinityGPerL = 0.2f;
	Worn.State.Liquid.bBoiled = true;
	Store->TryAddItem(Vessel, Pot.ItemDefinitionId, 1, Worn);
	Store->AddItemByGuid(FGuid::NewGuid(), Stick.ItemDefinitionId, 1);
	auto* Station = F.Station();
	AMOSurvivorController* AI = nullptr;
	auto* Jobs = AssignSurvivor(F, AI);
	F.Pawn->SetActorLocation(Store->GetOwner()->GetActorLocation());
	TestTrue(TEXT("Craft job enqueues"), Jobs->EnqueueCraftJob(RecipeId, Station, Store->GetOwner(), 1).IsValid());
	static_cast<AActor*>(AI)->Tick(0.1f); // arrival at storage
	static_cast<AActor*>(AI)->Tick(2.1f); // actual handling interval
	TestEqual(TEXT("Storage ingredients physically left with the pawn"), Store->Inventory.Entries.Num(), 0);
	TestTrue(TEXT("Pawn carries the original filled vessel"), F.Inventory->GetInstanceState(Vessel).Liquid == Worn.State.Liquid);
	F.Pawn->SetActorLocation(Station->GetActorLocation());
	static_cast<AActor*>(AI)->Tick(0.1f); // station arrival / enqueue
	TestEqual(TEXT("Job owns one real timed craft"), F.Queue->GetQueueLength(), 1);
	static_cast<AActor*>(AI)->Tick(121.0f);
	TestEqual(TEXT("A valid long craft is not failed by a 120-second whole-job watchdog"), Jobs->GetJobCount(), 1);
	TestEqual(TEXT("The simulation was not skipped to satisfy the watchdog"), F.Inventory->GetItemCountByDefinitionId(Pot.ItemDefinitionId), 1);
	F.CompleteOne();
	static_cast<AActor*>(AI)->Tick(0.1f); // begin returning only after the owned completion
	F.Pawn->SetActorLocation(Store->GetOwner()->GetActorLocation());
	static_cast<AActor*>(AI)->Tick(0.1f); // storage arrival
	Store->AddItemByGuid(FGuid::NewGuid(), Pot.ItemDefinitionId, 2); // another worker filled storage
	static_cast<AActor*>(AI)->Tick(2.1f);
	TestEqual(TEXT("A full destination does not complete the job"), Jobs->GetJobCount(), 1);
	TestEqual(TEXT("Both actual output and borrowed vessel stay on the pawn"), F.Inventory->GetItemCountByDefinitionId(Pot.ItemDefinitionId), 2);
	Store->ClearInventoryAndSlots();
	static_cast<AActor*>(AI)->Tick(2.1f);
	TestEqual(TEXT("The job completes after a real handling retry can place all cargo"), Jobs->GetJobCount(), 0);
	TestEqual(TEXT("Storage receives product PLUS borrowed vessel"), Store->GetItemCountByDefinitionId(Pot.ItemDefinitionId), 2);
	TestEqual(TEXT("The borrowed vessel keeps its unconsumed water"), Store->GetInstanceState(Vessel).Liquid.Millilitres, 250.0f);
	TestEqual(TEXT("The borrowed vessel keeps its wear"), Store->GetInstanceState(Vessel).Durability, 13);
	TestEqual(TEXT("The pawn has no stranded cargo"), F.Inventory->Inventory.Entries.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingSurvivorOwnershipGate, "MOFramework.Crafting.SurvivorOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingSurvivorOwnershipGate::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	F.Recipe.RequiredStation = EMOCraftingStation::None;
	F.PublishRecipe();
	TestTrue(TEXT("Existing unrelated craft enqueues"), F.Queue->EnqueueCraft(RecipeId));
	FMOCraftingQueueEntry Existing;
	F.Queue->GetCurrentCraft(Existing);
	auto* Store = Storage(F, 2);
	AMOSurvivorController* AI = nullptr;
	auto* Jobs = AssignSurvivor(F, AI);
	Jobs->EnqueueCraftJob(RecipeId, F.Station(), Store->GetOwner(), 1);
	TestEqual(TEXT("Busy pawn cannot accept a second owner of its craft queue"), Jobs->GetJobCount(), 0);
	TestTrue(TEXT("Failed assignment never cancels the unrelated craft"), F.Queue->GetQueueEntry(Existing.EntryId, Existing));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingSurvivorRepeatSaveGate, "MOFramework.Crafting.SurvivorRepeatSave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingSurvivorRepeatSaveGate::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	F.Recipe.RequiredStation = EMOCraftingStation::None;
	F.Recipe.CraftTime = 60;
	FMORecipeIngredient Input; Input.ItemDefinitionId = TEXT("Stick01");
	FMORecipeOutput Output; Output.ItemDefinitionId = TEXT("CrudePottery01");
	F.Recipe.Ingredients = {Input}; F.Recipe.Outputs = {Output}; F.PublishRecipe();
	auto* Store = Storage(F, 4);
	Store->AddItemByGuid(FGuid::NewGuid(), Input.ItemDefinitionId, 2);
	auto* Station = F.Station();
	AMOSurvivorController* AI = nullptr;
	auto* Jobs = AssignSurvivor(F, AI);
	F.Pawn->SetActorLocation(Store->GetOwner()->GetActorLocation());
	const auto JobId = Jobs->EnqueueCraftJob(RecipeId, Station, Store->GetOwner(), 2);
	auto RunOne = [&]()
	{
		static_cast<AActor*>(AI)->Tick(0.1f); // storage arrival
		static_cast<AActor*>(AI)->Tick(2.1f); // real withdraw handling
		F.Pawn->SetActorLocation(Station->GetActorLocation());
		static_cast<AActor*>(AI)->Tick(0.1f); // station arrival / enqueue
		F.CompleteOne();
		static_cast<AActor*>(AI)->Tick(0.1f); // owned completion / return
		F.Pawn->SetActorLocation(Store->GetOwner()->GetActorLocation());
		static_cast<AActor*>(AI)->Tick(0.1f); // storage arrival
		static_cast<AActor*>(AI)->Tick(2.1f); // real deposit handling
	};
	RunOne();
	FMOSurvivorJobQueueSaveData Between; Jobs->BuildSaveData(Between);
	TestEqual(TEXT("The assignment remains queued between requested repetitions"), Jobs->GetJobCount(), 1);
	if (!TestEqual(TEXT("One persisted job between repetitions"), Between.Jobs.Num(), 1)) return false;
	TestEqual(TEXT("The completed repetition is recorded on the saved owning entry"), Between.Jobs[0].CompletedCount, 1);
	TestEqual(TEXT("The same assignment owns the next repetition"), Between.Jobs[0].JobId, JobId);
	TestEqual(TEXT("Only one output has been produced"), Store->GetItemCountByDefinitionId(Output.ItemDefinitionId), 1);
	RunOne();
	TestEqual(TEXT("Exactly the requested repetitions finish the assignment"), Jobs->GetJobCount(), 0);
	TestEqual(TEXT("Both real inputs were consumed exactly once"), Store->GetItemCountByDefinitionId(Input.ItemDefinitionId), 0);
	TestEqual(TEXT("Exactly two actual outputs were deposited"), Store->GetItemCountByDefinitionId(Output.ItemDefinitionId), 2);
	TestTrue(TEXT("The pawn has no unowned leftover craft entry"), F.Queue->IsQueueEmpty());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingSurvivorIterationCancellation, "MOFramework.Crafting.SurvivorIterationCancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingSurvivorIterationCancellation::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	F.Recipe.RequiredStation = EMOCraftingStation::None;
	F.Recipe.CraftTime = 60;
	FMORecipeIngredient Input; Input.ItemDefinitionId = TEXT("Stick01");
	FMORecipeOutput Output; Output.ItemDefinitionId = TEXT("CrudePottery01");
	F.Recipe.Ingredients = {Input}; F.Recipe.Outputs = {Output}; F.PublishRecipe();
	auto* Store = Storage(F, 4);
	Store->AddItemByGuid(FGuid::NewGuid(), Input.ItemDefinitionId, 2);
	auto* Station = F.Station();
	AMOSurvivorController* AI = nullptr;
	auto* Jobs = AssignSurvivor(F, AI);
	F.Pawn->SetActorLocation(Store->GetOwner()->GetActorLocation());
	const auto First = Jobs->EnqueueCraftJob(RecipeId, Station, Store->GetOwner(), 1);
	auto* Probe = NewObject<UMOSurvivorIterationProbe>();
	Probe->Queue = Jobs; Probe->JobId = First; Probe->bWaitForCompletedIteration = true;
	Jobs->OnQueueChanged.AddDynamic(Probe, &UMOSurvivorIterationProbe::QueueChanged);
	const auto Next = Jobs->EnqueueCraftJob(RecipeId, Station, Store->GetOwner(), 1);
	static_cast<AActor*>(AI)->Tick(0.1f);
	static_cast<AActor*>(AI)->Tick(2.1f);
	F.Pawn->SetActorLocation(Station->GetActorLocation());
	static_cast<AActor*>(AI)->Tick(0.1f);
	F.CompleteOne();
	static_cast<AActor*>(AI)->Tick(0.1f);
	F.Pawn->SetActorLocation(Store->GetOwner()->GetActorLocation());
	static_cast<AActor*>(AI)->Tick(0.1f);
	static_cast<AActor*>(AI)->Tick(2.1f);
	TestTrue(TEXT("The iteration notification exposes the owning updated count"), Probe->bFired);
	TestEqual(TEXT("An observer cancelled the original job without losing its successor"), Jobs->GetJobCount(), 1);
	TestEqual(TEXT("Cleanup cannot remove the next assignment"), Jobs->GetCurrentJob().JobId, Next);
	TestEqual(TEXT("The next assignment has performed no iterations"), Jobs->GetCurrentJob().CompletedCount, 0);
	TestEqual(TEXT("Only the first assignment's input was withdrawn"), Store->GetItemCountByDefinitionId(Input.ItemDefinitionId), 1);
	TestEqual(TEXT("The first assignment's actual output reached storage"), Store->GetItemCountByDefinitionId(Output.ItemDefinitionId), 1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingSurvivorReloadPhases, "MOFramework.Crafting.SurvivorReload.PhasesAndOfflineReceipt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingSurvivorReloadPhases::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	F.Recipe.RequiredStation = EMOCraftingStation::None;
	F.Recipe.CraftTime = 180;
	FMORecipeIngredient Water, Stick;
	Water.ItemDefinitionId = TEXT("Water01"); Stick.ItemDefinitionId = TEXT("Stick01");
	FMORecipeOutput Pot; Pot.ItemDefinitionId = TEXT("CrudePottery01");
	F.Recipe.Ingredients = {Water, Stick}; F.Recipe.Outputs = {Pot}; F.PublishRecipe();
	auto* Station = F.Station();
	auto* StoreActor = F.Station(); StoreActor->SetActorLocation(FVector(1000, 0, 0));
	auto* Store = StoreActor->StationInventory.Get(); Store->SetSlotCountAuthority(6);
	const FGuid Vessel = FGuid::NewGuid();
	FMOInventoryAddOptions Original; Original.State.Durability = 13;
	Original.State.Liquid.Millilitres = 1000; Original.State.Liquid.bBoiled = true;
	Original.State.Liquid.Source = EMOWaterBodyKind::River; Original.State.Liquid.SalinityGPerL = 0.2f;
	Store->TryAddItem(Vessel, Pot.ItemDefinitionId, 1, Original);
	Store->AddItemByGuid(FGuid::NewGuid(), Stick.ItemDefinitionId, 2);
	AMOSurvivorController* AI = nullptr; auto* Jobs = AssignSurvivor(F, AI);
	F.Pawn->SetActorLocation(StoreActor->GetActorLocation());
	const auto JobId = Jobs->EnqueueCraftJob(RecipeId, Station, StoreActor, 2);
	auto Tick = [&](float Seconds) { static_cast<AActor*>(AI)->Tick(Seconds); };
	Tick(0.1f); Tick(1.0f);
	ReloadCraftExecutor(F, AI, Jobs, Store);
	Tick(0.8f);
	TestEqual(TEXT("Reloaded handling does not skip its remaining real interval"), Store->GetItemCountByDefinitionId(Stick.ItemDefinitionId), 2);
	Tick(0.3f);
	TestEqual(TEXT("Saved withdraw leg takes one input"), Store->GetItemCountByDefinitionId(Stick.ItemDefinitionId), 1);
	ReloadCraftExecutor(F, AI, Jobs, Store);
	TestEqual(TEXT("Reload after withdrawal resumes station leg"), Jobs->GetCurrentJob().CraftExecution.Phase, uint8(12));
	TestEqual(TEXT("The original borrowed vessel is in the saved manifest"), Jobs->GetCurrentJob().CraftExecution.BorrowedContainers.Num(), 1);
	F.Pawn->SetActorLocation(Station->GetActorLocation()); Tick(0.1f);
	const FGuid Owned = Jobs->GetCurrentJob().CraftExecution.QueueEntryId;
	FMOCraftingQueueEntry Entry;
	TestTrue(TEXT("Assignment saves its exact reserved craft entry"), F.Queue->GetQueueEntry(Owned, Entry));
	ReloadCraftExecutor(F, AI, Jobs, Store);
	TestEqual(TEXT("Reload during paid work reuses the one entry"), F.Queue->GetQueueLength(), 1);
	TestEqual(TEXT("Reload during paid work retains identity"), Jobs->GetCurrentJob().CraftExecution.QueueEntryId, Owned);
	ReloadCraftExecutor(F, AI, Jobs, Store, true);
	TestTrue(TEXT("Offline work completed before executor rebind"), F.Queue->IsQueueEmpty());
	FMOCraftingCompletionReceipt Receipt;
	TestTrue(TEXT("Offline completion retains its exact result"), F.Queue->GetCompletionReceipt(Owned, Receipt));
	Tick(0.1f);
	TestEqual(TEXT("Late binding recovers result and returns, rather than failing/re-withdrawing"), Jobs->GetCurrentJob().CraftExecution.Phase, uint8(14));
	ReloadCraftExecutor(F, AI, Jobs, Store);
	F.Pawn->SetActorLocation(StoreActor->GetActorLocation()); Tick(0.1f);
	Store->SetSlotCountAuthority(1); Tick(2.1f);
	TestEqual(TEXT("Blocked deposit retains the saved job"), Jobs->GetJobCount(), 1);
	ReloadCraftExecutor(F, AI, Jobs, Store); Tick(2.1f);
	TestEqual(TEXT("Blocked reload never withdraws a second input"), Store->GetItemCountByDefinitionId(Stick.ItemDefinitionId), 1);
	TestEqual(TEXT("Blocked cargo keeps exact remaining liquid"), F.Inventory->GetInstanceState(Vessel).Liquid.Millilitres, 750.0f);
	Store->SetSlotCountAuthority(6); Tick(1.0f);
	ReloadCraftExecutor(F, AI, Jobs, Store); Tick(1.1f);
	TestEqual(TEXT("One delivered repeat is persisted"), Jobs->GetCurrentJob().CompletedCount, 1);
	TestFalse(TEXT("Delivered receipt is acknowledged"), F.Queue->GetCompletionReceipt(Owned, Receipt));
	ReloadCraftExecutor(F, AI, Jobs, Store);
	TestEqual(TEXT("Between-repeat reload keeps the stable assignment"), Jobs->GetCurrentJob().JobId, JobId);
	Tick(0.1f); Tick(2.1f);
	F.Pawn->SetActorLocation(Station->GetActorLocation()); Tick(0.1f); F.CompleteOne(); Tick(0.1f);
	F.Pawn->SetActorLocation(StoreActor->GetActorLocation()); Tick(0.1f); Tick(2.1f);
	TestEqual(TEXT("Exactly two simulated repeats finish"), Jobs->GetJobCount(), 0);
	TestEqual(TEXT("Two outputs plus the original vessel reach storage"), Store->GetItemCountByDefinitionId(Pot.ItemDefinitionId), 3);
	TestEqual(TEXT("Both physical inputs consumed once"), Store->GetItemCountByDefinitionId(Stick.ItemDefinitionId), 0);
	TestEqual(TEXT("Original vessel retains wear"), Store->GetInstanceState(Vessel).Durability, 13);
	TestEqual(TEXT("Original liquid quantity accounts for exactly two crafts"), Store->GetInstanceState(Vessel).Liquid.Millilitres, 500.0f);
	TestTrue(TEXT("Original liquid profile survives both trips"), FMath::IsNearlyEqual(Store->GetInstanceState(Vessel).Liquid.SalinityGPerL, 0.2f));
	TestEqual(TEXT("No stranded pawn cargo"), F.Inventory->Inventory.Entries.Num(), 0);
	FMOCraftingQueueSaveData Final; F.Queue->BuildSaveData(Final);
	TestEqual(TEXT("No unacknowledged receipts after delivery"), Final.CompletionReceipts.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingSurvivorReloadObservers, "MOFramework.Crafting.SurvivorReload.NotificationBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingSurvivorReloadObservers::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	F.Recipe.RequiredStation = EMOCraftingStation::None;
	FMORecipeIngredient Stick; Stick.ItemDefinitionId = TEXT("Stick01");
	FMORecipeOutput Pot; Pot.ItemDefinitionId = TEXT("CrudePottery01");
	F.Recipe.Ingredients = {Stick}; F.Recipe.Outputs = {Pot}; F.PublishRecipe();
	auto* Station = F.Station(); auto* StoreActor = F.Station(); StoreActor->SetActorLocation(FVector(1000, 0, 0));
	auto* Store = StoreActor->StationInventory.Get(); Store->SetSlotCountAuthority(6);
	Store->AddItemByGuid(FGuid::NewGuid(), Stick.ItemDefinitionId, 2);
	AMOSurvivorController* AI = nullptr; auto* Jobs = AssignSurvivor(F, AI);
	auto* Probe = NewObject<UMOSurvivorCraftSaveProbe>();
	Probe->Jobs = Jobs; Probe->Craft = F.Queue; Probe->PawnInventory = F.Inventory; Probe->Storage = Store;
	F.Inventory->OnInventoryChanged.AddDynamic(Probe, &UMOSurvivorCraftSaveProbe::Capture);
	F.Queue->OnQueueChanged.AddDynamic(Probe, &UMOSurvivorCraftSaveProbe::Capture);
	Jobs->OnQueueChanged.AddDynamic(Probe, &UMOSurvivorCraftSaveProbe::Capture);
	F.Pawn->SetActorLocation(StoreActor->GetActorLocation()); Jobs->EnqueueCraftJob(RecipeId, Station, StoreActor, 2);
	auto Tick = [&](float Seconds) { static_cast<AActor*>(AI)->Tick(Seconds); };
	auto RestoreProbe = [&]() { RestoreCraftExecutor(F, AI, Jobs, Store, Probe->JobSave, Probe->CraftSave, Probe->PawnSave, Probe->StorageSave); };
	Tick(0.1f); Tick(2.1f);
	TestTrue(TEXT("Withdraw observer captured its new phase with the new inventories"), Probe->bCaptured);
	RestoreProbe();
	Probe->ExpectedPhase = 13; Probe->bCaptured = false;
	F.Pawn->SetActorLocation(Station->GetActorLocation()); Tick(0.1f);
	TestTrue(TEXT("Enqueue observer sees owned paid entry and saved working phase together"), Probe->bCaptured);
	RestoreProbe();
	TestEqual(TEXT("Observer save resumes one reservation, never a second withdrawal"), F.Queue->GetQueueLength(), 1);
	F.CompleteOne(); Tick(0.1f); F.Pawn->SetActorLocation(StoreActor->GetActorLocation()); Tick(0.1f);
	Probe->ExpectedPhase = 16; Probe->bCaptured = false; Tick(2.1f);
	TestTrue(TEXT("Deposit observer captures delivery before acknowledgement"), Probe->bCaptured);
	RestoreProbe();
	TestEqual(TEXT("Observer save has already delivered its product"), Store->GetItemCountByDefinitionId(Pot.ItemDefinitionId), 1);
	Probe->ExpectedPhase = 0; Probe->ExpectedCount = 1; Probe->bCaptured = false;
	Tick(0.1f);
	TestTrue(TEXT("Acknowledgement observer sees count and fresh phase in the same saved transition"), Probe->bCaptured);
	RestoreProbe();
	TestEqual(TEXT("Acknowledgement reload cannot count the old delivery twice"), Jobs->GetCurrentJob().CompletedCount, 1);
	Tick(0.1f); Tick(2.1f);
	F.Pawn->SetActorLocation(Station->GetActorLocation()); Tick(0.1f); F.CompleteOne(); Tick(0.1f);
	F.Pawn->SetActorLocation(StoreActor->GetActorLocation()); Tick(0.1f); Tick(2.1f);
	TestEqual(TEXT("Observer reloads still finish exactly the requested assignment"), Jobs->GetJobCount(), 0);
	TestEqual(TEXT("No duplicate products across notification saves"), Store->GetItemCountByDefinitionId(Pot.ItemDefinitionId), 2);
	TestEqual(TEXT("No duplicated/re-withdrawn inputs"), Store->GetItemCountByDefinitionId(Stick.ItemDefinitionId), 0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingSurvivorReloadPossession, "MOFramework.Crafting.SurvivorReload.ActiveLoadAndPossession", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingSurvivorReloadPossession::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	F.Recipe.RequiredStation = EMOCraftingStation::None;
	FMORecipeIngredient Stick; Stick.ItemDefinitionId = TEXT("Stick01");
	FMORecipeOutput Pot; Pot.ItemDefinitionId = TEXT("CrudePottery01");
	F.Recipe.Ingredients = {Stick}; F.Recipe.Outputs = {Pot}; F.PublishRecipe();
	auto* Station = F.Station(); auto* StoreActor = F.Station(); StoreActor->SetActorLocation(FVector(1000, 0, 0));
	auto* Store = StoreActor->StationInventory.Get(); Store->SetSlotCountAuthority(6);
	Store->AddItemByGuid(FGuid::NewGuid(), Stick.ItemDefinitionId, 1);
	AMOSurvivorController* AI = nullptr; auto* Jobs = AssignSurvivor(F, AI);
	F.Pawn->SetActorLocation(StoreActor->GetActorLocation()); Jobs->EnqueueCraftJob(RecipeId, Station, StoreActor, 1);
	auto Tick = [&](float Seconds) { static_cast<AActor*>(AI)->Tick(Seconds); };
	Tick(0.1f); Tick(2.1f); F.Pawn->SetActorLocation(Station->GetActorLocation()); Tick(0.1f);
	FMOSurvivorJobQueueSaveData PaidJob; Jobs->BuildSaveData(PaidJob);
	FMOCraftingQueueSaveData PaidCraft; F.Queue->BuildSaveData(PaidCraft);
	FMOInventorySaveData PaidPawn, PaidStore; F.Inventory->BuildSaveData(PaidPawn); Store->BuildSaveData(PaidStore);
	const auto Owned = PaidJob.Jobs[0].CraftExecution.QueueEntryId;
	F.CompleteOne(); Tick(0.1f);
	TestEqual(TEXT("Controller has advanced beyond the old checkpoint"), Jobs->GetCurrentJob().CraftExecution.Phase, uint8(14));
	// Load over the existing controller, as the live-world restore path can do.
	F.Inventory->ApplySaveDataAuthority(SerializedCopy(PaidPawn)); Store->ApplySaveDataAuthority(SerializedCopy(PaidStore));
	F.Queue->ApplySaveData(SerializedCopy(PaidCraft), false);
	Jobs->ApplySaveDataAuthority(SerializedCopy(PaidJob));
	TestEqual(TEXT("Existing executor rebinds to the loaded phase"), Jobs->GetCurrentJob().CraftExecution.Phase, uint8(13));
	AI->UnPossess(); AI->Destroy(); F.Pawn->SetActorLocation(FVector(5000, 0, 0));
	AI = F.World->SpawnActor<AMOSurvivorController>(); AI->Possess(F.Pawn);
	TestEqual(TEXT("Possession away from paid work resumes real travel to its station"), Jobs->GetCurrentJob().CraftExecution.Phase, uint8(12));
	TestEqual(TEXT("Travelling back retains the original paid entry"), Jobs->GetCurrentJob().CraftExecution.QueueEntryId, Owned);
	F.Pawn->SetActorLocation(Station->GetActorLocation()); Tick(0.1f);
	TestEqual(TEXT("Station arrival does not enqueue a duplicate"), F.Queue->GetQueueLength(), 1);
	TestEqual(TEXT("No replacement ingredient was needed"), F.Inventory->GetItemCountByDefinitionId(Stick.ItemDefinitionId), 0);
	F.CompleteOne(); Tick(0.1f); F.Pawn->SetActorLocation(StoreActor->GetActorLocation()); Tick(0.1f); Tick(1.0f);
	AI->UnPossess(); AI->Destroy(); F.Pawn->SetActorLocation(FVector(5000, 0, 0));
	AI = F.World->SpawnActor<AMOSurvivorController>(); AI->Possess(F.Pawn);
	TestEqual(TEXT("Possession interrupts handling and requires a physical return"), Jobs->GetCurrentJob().CraftExecution.Phase, uint8(14));
	TestEqual(TEXT("No remote cargo deposit"), Store->GetItemCountByDefinitionId(Pot.ItemDefinitionId), 0);
	// This fixture has no navigation mesh. As in the other leg tests, provide arrival before ticking it;
	// ticking while far away correctly takes the production unreachable-path failure, tested below.
	F.Pawn->SetActorLocation(StoreActor->GetActorLocation()); Tick(0.1f); Tick(1.0f);
	TestEqual(TEXT("Interrupted handling restarts its complete real interval"), Store->GetItemCountByDefinitionId(Pot.ItemDefinitionId), 0);
	Tick(1.1f);
	TestEqual(TEXT("Resumed real handling delivers exactly one product"), Store->GetItemCountByDefinitionId(Pot.ItemDefinitionId), 1);
	TestEqual(TEXT("Assignment completes once"), Jobs->GetJobCount(), 0);
	// Keep the real failure policy covered too: an unreachable return must fail without teleporting cargo.
	Store->AddItemByGuid(FGuid::NewGuid(), Stick.ItemDefinitionId, 1);
	Jobs->EnqueueCraftJob(RecipeId, Station, StoreActor, 1);
	Tick(0.1f); Tick(2.1f); F.Pawn->SetActorLocation(Station->GetActorLocation()); Tick(0.1f);
	F.CompleteOne(); Tick(0.1f);
	AI->UnPossess(); AI->Destroy(); F.Pawn->SetActorLocation(FVector(5000, 0, 0));
	AI = F.World->SpawnActor<AMOSurvivorController>(); AI->Possess(F.Pawn);
	Tick(0.5f); // no navigation exists here: fail honestly, retaining the real product on its carrier
	TestEqual(TEXT("Unreachable return fails the assignment"), Jobs->GetJobCount(), 0);
	TestEqual(TEXT("Failed return retains the actual product on pawn"), F.Inventory->GetItemCountByDefinitionId(Pot.ItemDefinitionId), 1);
	TestEqual(TEXT("Failed return cannot remotely deposit another product"), Store->GetItemCountByDefinitionId(Pot.ItemDefinitionId), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingSurvivorReloadLateIdentity, "MOFramework.Crafting.SurvivorReload.LateIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingSurvivorReloadLateIdentity::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
	F.Recipe.RequiredStation = EMOCraftingStation::None; F.PublishRecipe();
	auto* Station = F.Station(); auto* StoreActor = F.Station(); StoreActor->SetActorLocation(FVector(1000, 0, 0));
	auto* Store = StoreActor->StationInventory.Get();
	AMOSurvivorController* AI = nullptr; auto* Jobs = AssignSurvivor(F, AI);
	const auto Id = Jobs->EnqueueCraftJob(RecipeId, Station, StoreActor, 1);
	FMOSurvivorJobQueueSaveData Save; Jobs->BuildSaveData(Save);
	const auto SavedStorageGuid = Save.Jobs[0].StorageActorGuid;
	StoreActor->IdentityComponent->SetGuid(FGuid::NewGuid());
	Jobs->ApplySaveDataAuthority(SerializedCopy(Save));
	static_cast<AActor*>(AI)->Tick(0.6f);
	TestEqual(TEXT("Missing hydrated identity retains the assignment"), Jobs->GetJobCount(), 1);
	TestEqual(TEXT("Missing identity does not complete/fail its assignment"), Jobs->GetCurrentJob().CompletedCount, 0);
	StoreActor->IdentityComponent->SetGuid(SavedStorageGuid);
	static_cast<AActor*>(AI)->Tick(0.6f);
	TestEqual(TEXT("Hydrated actor resumes the original saved leg"), Jobs->GetCurrentJob().CraftExecution.Phase, uint8(10));
	TestEqual(TEXT("Hydrated job retains stable identity"), Jobs->GetCurrentJob().JobId, Id);
	auto State = Jobs->GetCurrentJob().CraftExecution;
	F.Pawn->SetRole(ROLE_AutonomousProxy);
	TestFalse(TEXT("Client cannot mutate saved execution"), Jobs->SetCraftExecution(Id, State));
	F.Pawn->SetRole(ROLE_Authority);
	TestFalse(TEXT("Wrong identity cannot mutate execution"), Jobs->SetCraftExecution(FGuid::NewGuid(), State));
	State.HandlingSeconds = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite handling state is refused"), Jobs->SetCraftExecution(Id, State));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingSurvivorReloadDetachedCancel, "MOFramework.Crafting.SurvivorReload.DetachedCancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingSurvivorReloadDetachedCancel::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	for (bool bCompleted : {false, true})
	{
		FWorld F;
		if (!TestNotNull(TEXT("Recipe table"), F.Recipes)) return false;
		F.Recipe.RequiredStation = EMOCraftingStation::None;
		FMORecipeIngredient Stick; Stick.ItemDefinitionId = TEXT("Stick01");
		FMORecipeOutput Pot; Pot.ItemDefinitionId = TEXT("CrudePottery01");
		F.Recipe.Ingredients = {Stick}; F.Recipe.Outputs = {Pot}; F.PublishRecipe();
		auto* Station = F.Station(); auto* StoreActor = F.Station(); StoreActor->SetActorLocation(FVector(1000, 0, 0));
		auto* Store = StoreActor->StationInventory.Get(); Store->SetSlotCountAuthority(6);
		Store->AddItemByGuid(FGuid::NewGuid(), Stick.ItemDefinitionId, 1);
		AMOSurvivorController* AI = nullptr; auto* Jobs = AssignSurvivor(F, AI);
		F.Pawn->SetActorLocation(StoreActor->GetActorLocation()); const auto JobId = Jobs->EnqueueCraftJob(RecipeId, Station, StoreActor, 1);
		static_cast<AActor*>(AI)->Tick(0.1f); static_cast<AActor*>(AI)->Tick(2.1f);
		F.Pawn->SetActorLocation(Station->GetActorLocation()); static_cast<AActor*>(AI)->Tick(0.1f);
		const auto Owned = Jobs->GetCurrentJob().CraftExecution.QueueEntryId;
		AI->UnPossess(); AI->Destroy();
		if (bCompleted) F.CompleteOne();
		// A different craft shares this pawn's queue. Cancelling the assignment may only release its saved ID.
		F.Recipe.Ingredients.Reset(); F.PublishRecipe();
		FGuid Unrelated;
		TestTrue(TEXT("Unrelated native queue entry exists"), F.Queue->EnqueueCraftAtStationWithId(RecipeId, 1, Station, Unrelated));
		if (bCompleted) Jobs->CancelAllJobs(); else Jobs->CancelJob(JobId);
		FMOCraftingQueueEntry Entry; FMOCraftingCompletionReceipt Receipt;
		TestFalse(TEXT("Detached cancellation releases the assignment's saved craft"), F.Queue->GetQueueEntry(Owned, Entry));
		TestFalse(TEXT("Detached cancellation acknowledges only its retained result"), F.Queue->GetCompletionReceipt(Owned, Receipt));
		TestTrue(TEXT("Detached cancellation preserves unrelated work"), F.Queue->GetQueueEntry(Unrelated, Entry));
		TestEqual(TEXT("Unfinished paid inputs are refunded once; completed inputs are not"), F.Inventory->GetItemCountByDefinitionId(Stick.ItemDefinitionId), bCompleted ? 0 : 1);
		TestEqual(TEXT("Completed product remains physically on pawn after assignment cancellation"), F.Inventory->GetItemCountByDefinitionId(Pot.ItemDefinitionId), bCompleted ? 1 : 0);
		TestEqual(TEXT("Cancellation does not remotely return physical cargo"), Store->GetItemCountByDefinitionId(Stick.ItemDefinitionId), 0);
	}
	return true;
}

// BR18-28 (Wes, Oct 18): the stone maul said "Missing required tools." without naming one. The refusal names the tool and the simplest item that
// serves it, and that item really does satisfy the requirement (the control: carrying it lets the craft through).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCraftingMissingToolNamed, "MOFramework.Crafting.MissingToolNamed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOCraftingMissingToolNamed::RunTest(const FString&)
{
	using namespace MOCraftingContextTests;
	FWorld F;
	FMOToolRequirement Hammer;
	Hammer.ToolType = EMOToolType::Hammer;
	Hammer.MinQuality = 0.3f;   // the stone maul's requirement
	Hammer.bIsRequired = true;
	F.Recipe.RequiredStation = EMOCraftingStation::None;
	F.Recipe.RequiredTools = {Hammer};
	F.PublishRecipe();

	const FName Simplest = UMOItemDatabaseSettings::FindSimplestToolItem(EMOToolType::Hammer, 0.3f);
	if (!TestFalse(TEXT("Some item works as a hammer of quality 0.3"), Simplest.IsNone()))
	{
		return false;
	}
	const FMOCraftingValidation Without = F.Crafting->CanCraftRecipe(RecipeId, nullptr, nullptr, F.Inventory, EMOCraftingStation::None);
	const FString Reason = Without.FailureReason.ToString();
	TestFalse(TEXT("No hammer: the craft is refused"), Without.bCanCraft);
	TestTrue(FString::Printf(TEXT("The refusal names the tool: '%s'"), *Reason), Reason.Contains(TEXT("Hammer")));
	TestTrue(FString::Printf(TEXT("The refusal names an item that serves (%s): '%s'"), *Simplest.ToString(), *Reason),
		Reason.Contains(UMOItemDatabaseSettings::GetItemDisplayName(Simplest).ToString()));

	F.Inventory->AddItemByGuid(FGuid::NewGuid(), Simplest, 1);
	const FMOCraftingValidation With = F.Crafting->CanCraftRecipe(RecipeId, nullptr, nullptr, F.Inventory, EMOCraftingStation::None);
	TestTrue(FString::Printf(TEXT("Carrying the named item lets the craft through ('%s')"), *With.FailureReason.ToString()), With.bCanCraft);
	return true;
}
#endif
