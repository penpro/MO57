#include "Misc/AutomationTest.h"
#include "MOMedicalTreatmentActions.h"
#include "MOItemActions.h"
#include "MOMedicalActionTestClock.h"
#include "MOBodyPartDefinitionRow.h"
#include "MOInventoryComponent.h"
#include "MOItemDefinitionRow.h"
#include "MOItemDatabaseSettings.h"
#include "MOMedicalSubsystem.h"
#include "MOMedicalDatabaseSettings.h"
#include "MOSkillsComponent.h"
#include "MOTimedActionComponent.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace MOMedicalActionTests
{
	FMOMedicalTreatmentRow Definition()
	{
		FMOMedicalTreatmentRow Row;
		Row.TreatmentId = TEXT("AuditWoundTreatment");
		Row.TreatsWoundTypes = {EMOWoundType::Laceration};
		Row.RequiredSkillId = TEXT("Medicine");
		Row.MinimumSkillLevel = 0;
		Row.TreatmentDuration = 20;
		Row.RequiredItemIds = {TEXT("Stick01"), TEXT("CleanWater01")};
		return Row;
	}
	FMOWound Wound()
	{
		FMOWound Result;
		Result.WoundId = FGuid::NewGuid();
		Result.WoundType = EMOWoundType::Laceration;
		Result.BodyPart = EMOBodyPartType::ForearmLeft;
		Result.BleedRate = 10;
		Result.InfectionRisk = 0.5f;
		return Result;
	}
	struct FWorld
	{
		UGameInstance* GI;
		UWorld* World;
		UDataTable* Table;
		APawn* Healer;
		APawn* Patient;
		UMOAnatomyComponent* Anatomy;
		UMOInventoryComponent* Inventory;
		UMOTimedActionComponent* Timed;
		AMOMedicalActionTestClock* Clock;
		FMOWound Injury = Wound();
		FGuid Jar = FGuid::NewGuid();
		FWorld()
		{
			Table = NewObject<UDataTable>();
			Table->AddToRoot();
			Table->RowStruct = FMOMedicalTreatmentRow::StaticStruct();
			auto Good = Definition();
			Table->AddRow(Good.TreatmentId, Good);
			auto Skilled = Good; Skilled.TreatmentId = TEXT("AuditSkilledTreatment"); Skilled.MinimumSkillLevel = 3;
			Table->AddRow(Skilled.TreatmentId, Skilled);
			auto SelfBlocked = Good; SelfBlocked.TreatmentId = TEXT("AuditSelfBlocked"); SelfBlocked.UnreachableForSelf = {Injury.BodyPart};
			Table->AddRow(SelfBlocked.TreatmentId, SelfBlocked);
			auto Wrong = Good; Wrong.TreatmentId = TEXT("AuditWrongWound"); Wrong.TreatsWoundTypes.Empty();
			Table->AddRow(Wrong.TreatmentId, Wrong);
			auto Strong = Good; Strong.TreatmentId = TEXT("AuditStrongTreatment"); Strong.BleedReduction = 1; Strong.InfectionReduction = 1;
			Table->AddRow(Strong.TreatmentId, Strong);
			// Medical caches may be populated by another subsystem during initialization.
			// Supply the fixture before Init rather than replacing a table under an already built cache.
			auto* Settings = GetMutableDefault<UMOMedicalDatabaseSettings>();
			const auto OriginalTreatments = Settings->MedicalTreatmentsTable;
			Settings->MedicalTreatmentsTable = Table;
			GI = NewObject<UGameInstance>(GEngine);
			GI->InitializeStandalone();
			Settings->MedicalTreatmentsTable = OriginalTreatments;
			World = GI->GetWorld();
			World->InitializeActorsForPlay(FURL());
			Healer = SpawnPawn();
			Patient = SpawnPawn();
			Patient->SetActorLocation(FVector(100, 0, 0));
			Inventory = Add<UMOInventoryComponent>(Healer);
			Timed = Add<UMOTimedActionComponent>(Healer);
			Add<UMOSkillsComponent>(Healer);
			Anatomy = Add<UMOAnatomyComponent>(Patient);
			Healer->DispatchBeginPlay(); Patient->DispatchBeginPlay();
			Anatomy->InflictWound(Injury);
			Inventory->AddItemByGuid(FGuid::NewGuid(), TEXT("Stick01"), 2);
			Inventory->AddItemByGuid(Jar, TEXT("CrudePottery01"), 1);
			FMOLiquidContents Water;
			Water.Millilitres = 500; Water.bBoiled = true; Water.Source = EMOWaterBodyKind::Pond; Water.SalinityGPerL = 0.2f;
			Inventory->SetLiquid(Jar, Water);
			Clock = World->SpawnActor<AMOMedicalActionTestClock>();
			World->SetGameState(Clock);
		}
		~FWorld()
		{
			Timed->Cancel(TEXT("test ended"));
			GI->Shutdown();
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			Table->RemoveFromRoot();
		}
		APawn* SpawnPawn()
		{
			auto* Pawn = World->SpawnActor<APawn>();
			auto* Root = NewObject<USceneComponent>(Pawn); Pawn->SetRootComponent(Root); Root->RegisterComponent();
			return Pawn;
		}
		template<class T> T* Add(AActor* Owner)
		{
			auto* Component = NewObject<T>(Owner); Owner->AddInstanceComponent(Component); Component->RegisterComponent(); return Component;
		}
		void Advance(double Seconds)
		{
			Clock->Now += Seconds;
			Timed->TickComponent(float(Seconds), LEVELTICK_All, nullptr);
		}
		FMOMedicalActionResult Begin(FName Id = TEXT("AuditWoundTreatment")) { return FMOMedicalTreatmentActions::Begin(Healer, Patient, Injury.WoundId, Id); }
		FMOWound Current() const { return Anatomy->GetAllWounds()[0]; }
		AActor* BlockSight()
		{
			auto* Wall = World->SpawnActor<AActor>();
			auto* Box = NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(10, 30, 100));
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			Box->RegisterComponent(); Wall->SetActorLocation(FVector(50, 0, 30));
			return Wall;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedicalClinicalGate, "MOFramework.Medical.Treatment.ClinicalValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOMedicalClinicalGate::RunTest(const FString&)
{
	using namespace MOMedicalActionTests;
	auto Injury = Wound(); auto Row = Definition();
	auto Validate = [&](int32 Skill = 0, bool Self = false) { return UMOAnatomyComponent::ValidateTreatmentDefinition(Injury, Row, Skill, Self); };
	TestEqual(TEXT("A matching wound is treatable"), Validate(), EMOWoundTreatmentFailure::None);
	Row.TreatsWoundTypes.Empty();
	TestEqual(TEXT("A condition-only or wrong-wound row is refused"), Validate(), EMOWoundTreatmentFailure::WrongWoundType);
	Row = Definition(); Row.MinimumSkillLevel = 3;
	TestEqual(TEXT("Actual insufficient skill is refused"), Validate(2), EMOWoundTreatmentFailure::InsufficientSkill);
	Row.UnreachableForSelf.Add(Injury.BodyPart);
	TestEqual(TEXT("Self-treatment reach is clinical policy"), Validate(3, true), EMOWoundTreatmentFailure::UnreachableForSelf);
	TestEqual(TEXT("A second medic can reach that part"), Validate(3), EMOWoundTreatmentFailure::None);
	Row = Definition(); Row.TreatmentDuration = 0;
	TestEqual(TEXT("Zero duration cannot create an instant treatment"), Validate(), EMOWoundTreatmentFailure::InvalidDefinition);
	Row = Definition(); Row.BleedReduction = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("Corrupt effects refuse before mutation"), Validate(), EMOWoundTreatmentFailure::InvalidDefinition);
	Row = Definition(); Injury.HealingProgress = 100;
	TestEqual(TEXT("A healed wound cannot consume treatment supplies"), Validate(), EMOWoundTreatmentFailure::InvalidWound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedicalSupplyGate, "MOFramework.Medical.Treatment.SupplyPlan", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOMedicalSupplyGate::RunTest(const FString&)
{
	FMOMedicalTreatmentRow Row;
	Row.RequiredItemIds = {TEXT("Bandage"), TEXT("Tool"), TEXT("SafeWater")};
	TMap<FName, FMOItemDefinitionRow> Defs;
	Defs.FindOrAdd(TEXT("Bandage")).MaxStackSize = 10;
	Defs.FindOrAdd(TEXT("Tool")).bIsTool = true;
	Defs.FindOrAdd(TEXT("SafeWater")).LiquidMeasureMl = 250;
	Defs[TEXT("SafeWater")].bMeasureNeedsSafeWater = true;
	Defs.FindOrAdd(TEXT("Jar")).LiquidCapacityMl = 1000;
	auto Lookup = [&](FName Id, FMOItemDefinitionRow& Out) { if (const auto* Found = Defs.Find(Id)) { Out = *Found; return true; } return false; };
	TArray<FMOInventoryEntry> Entries;
	for (FName Id : {FName(TEXT("Bandage")), FName(TEXT("Tool")), FName(TEXT("Jar"))})
	{
		FMOInventoryEntry Entry; Entry.ItemGuid = FGuid::NewGuid(); Entry.ItemDefinitionId = Id; Entry.Quantity = 1; Entries.Add(Entry);
	}
	Entries[1].CurrentDurability = 7;
	Entries[2].Liquid.Millilitres = 500; Entries[2].Liquid.bBoiled = true; Entries[2].Liquid.SalinityGPerL = 0.2f;
	TArray<FMOInventoryEntry> Plan;
	TestTrue(TEXT("Physical supplies and liquid are planned jointly"), FMOMedicalTreatmentActions::PlanSupplies(Row, Entries, Plan, Lookup).IsAccepted());
	TestEqual(TEXT("The bandage is spent; tool and vessel survive"), Plan.Num(), 2);
	TestEqual(TEXT("Tool identity survives"), Plan[0].ItemGuid, Entries[1].ItemGuid);
	TestEqual(TEXT("Required tool is reusable, without invented wear policy"), Plan[0].CurrentDurability, 7);
	TestEqual(TEXT("Only the required liquid measure is consumed"), Plan[1].Liquid.Millilitres, 250.0f);
	TestEqual(TEXT("The unspent liquid retains salts"), Plan[1].Liquid.SalinityGPerL, 0.2f);
	Entries[2].Liquid.bBoiled = false;
	const auto PreviousGuid = Plan[0].ItemGuid;
	TestEqual(TEXT("Unsafe water cannot satisfy clean water"), FMOMedicalTreatmentActions::PlanSupplies(Row, Entries, Plan, Lookup).Failure, EMOMedicalActionFailure::MissingSupplies);
	TestEqual(TEXT("Refusal does not partially overwrite a plan"), Plan[0].ItemGuid, PreviousGuid);
	TestEqual(TEXT("Planning never consumes live physical stock"), Entries[0].Quantity, 1);
	Entries[1].CurrentDurability = 0;
	TestEqual(TEXT("Broken tools are distinct from missing materials"), FMOMedicalTreatmentActions::PlanSupplies(Row, Entries, Plan, Lookup).Failure, EMOMedicalActionFailure::BrokenTool);
	Row.RequiredItemIds = {TEXT("Unknown")};
	TestEqual(TEXT("Unknown supply IDs fail closed"), FMOMedicalTreatmentActions::PlanSupplies(Row, Entries, Plan, Lookup).Failure, EMOMedicalActionFailure::UnknownSupply);
	Row.RequiredItemIds.Empty(); Entries[1].ItemGuid = Entries[0].ItemGuid;
	TestEqual(TEXT("Corrupt identities are distinct from missing supplies"), FMOMedicalTreatmentActions::PlanSupplies(Row, Entries, Plan, Lookup).Failure, EMOMedicalActionFailure::InvalidSupplyState);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedicalTimedCompletion, "MOFramework.Medical.Treatment.TimedCompletion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOMedicalTimedCompletion::RunTest(const FString&)
{
	using namespace MOMedicalActionTests;
	FWorld F;
	TestTrue(TEXT("The authoritative supplied action begins"), F.Begin().IsAccepted());
	TestTrue(TEXT("Treatment has visible replicated progress state"), F.Timed->GetState().bActive);
	const float Duration = F.Timed->GetState().RealSeconds;
	TestTrue(TEXT("Treatment duration remains nonzero"), Duration > 0);
	F.Advance(Duration * 0.5);
	TestEqual(TEXT("Waiting does not grant early clinical effects"), F.Current().BleedRate, 10.0f);
	TestEqual(TEXT("Waiting does not remove supplies"), F.Inventory->GetItemCountByDefinitionId(TEXT("Stick01")), 2);
	TestEqual(TEXT("Waiting retains every liquid millilitre"), F.Inventory->GetInstanceState(F.Jar).Liquid.Millilitres, 500.0f);
	FMOSkillProgress Before;
	const auto* Skills = F.Healer->FindComponentByClass<UMOSkillsComponent>();
	const bool bHadSkill = Skills->GetSkillProgress(TEXT("Medicine"), Before);
	TestTrue(TEXT("An incomplete interval does not award skill progression"), !bHadSkill || (Before.Level == 0 && Before.CurrentXP == 0));
	bool bObserverSawCompleteTransaction = false;
	F.Timed->OnStateChanged.AddLambda([&](const FMOTimedActionState& State)
	{
		if (!State.bActive) bObserverSawCompleteTransaction = F.Current().BleedRate < 10 && F.Inventory->GetItemCountByDefinitionId(TEXT("Stick01")) == 1;
	});
	F.Advance(Duration * 0.5 + 0.01);
	TestFalse(TEXT("The action ends only after the complete interval"), F.Timed->IsActive());
	TestTrue(TEXT("Completion observers see supplies and clinical effect installed together"), bObserverSawCompleteTransaction);
	TestTrue(TEXT("Clinical effect applies on completion"), F.Current().BleedRate < 10);
	TestEqual(TEXT("Exactly one physical supply is spent"), F.Inventory->GetItemCountByDefinitionId(TEXT("Stick01")), 1);
	TestEqual(TEXT("Exactly one clean liquid measure is spent"), F.Inventory->GetInstanceState(F.Jar).Liquid.Millilitres, 250.0f);
	FMOSkillProgress After;
	TestTrue(TEXT("Only completed work earns the authored treatment XP"), Skills->GetSkillProgress(TEXT("Medicine"), After) && (After.Level > 0 || After.CurrentXP > 0));
	F.Advance(Duration);
	TestEqual(TEXT("A completed callback cannot spend twice"), F.Inventory->GetItemCountByDefinitionId(TEXT("Stick01")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedicalInterruption, "MOFramework.Medical.Treatment.Interruption", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOMedicalInterruption::RunTest(const FString&)
{
	using namespace MOMedicalActionTests;
	FWorld F;
	TestTrue(TEXT("Treatment begins"), F.Begin().IsAccepted());
	TestEqual(TEXT("A duplicate request cannot reset or replace existing work"), F.Begin().Failure, EMOMedicalActionFailure::Busy);
	F.Patient->SetActorLocation(FVector(180, 0, 0)); F.Advance(0.1);
	TestFalse(TEXT("Patient motion interrupts while still inside overall reach"), F.Timed->IsActive());
	TestEqual(TEXT("Interrupted treatment has no effect"), F.Current().BleedRate, 10.0f);
	TestEqual(TEXT("Interrupted action retains physical supplies"), F.Inventory->GetItemCountByDefinitionId(TEXT("Stick01")), 2);
	TestEqual(TEXT("Interrupted action retains original liquid"), F.Inventory->GetInstanceState(F.Jar).Liquid.Millilitres, 500.0f);
	TestTrue(TEXT("An interrupted course can be restarted"), F.Begin().IsAccepted());
	F.Healer->SetActorLocation(FVector(80, 0, 0)); F.Advance(0.1);
	TestFalse(TEXT("Healer motion uses the existing timed-action drift cancellation"), F.Timed->IsActive());
	TestTrue(TEXT("Treatment can begin again after stopping"), F.Begin().IsAccepted());
	F.Inventory->ConsumeIngredient(TEXT("Stick01"), 2); F.Advance(0.1);
	TestFalse(TEXT("Losing supplies cancels rather than treating for free"), F.Timed->IsActive());
	TestEqual(TEXT("A failed joint plan leaves the water untouched"), F.Inventory->GetInstanceState(F.Jar).Liquid.Millilitres, 500.0f);
	F.Inventory->AddItemByGuid(FGuid::NewGuid(), TEXT("Stick01"), 2);
	TestTrue(TEXT("Restocked treatment starts normally"), F.Begin().IsAccepted());
	FMOInterruptContext Damage; Damage.Reason = EMOInterruptReason::Damage;
	F.Timed->NotifyInterrupt_Implementation(Damage);
	TestFalse(TEXT("The shared damage interrupt cancels treatment"), F.Timed->IsActive());
	TestEqual(TEXT("Damage interruption consumes no supplies"), F.Inventory->GetItemCountByDefinitionId(TEXT("Stick01")), 2);
	TestEqual(TEXT("Damage interruption grants no treatment effect"), F.Current().BleedRate, 10.0f);
	FMOSkillProgress Progress;
	TestFalse(TEXT("Repeated interrupted attempts do not award free skill XP"), F.Healer->FindComponentByClass<UMOSkillsComponent>()->GetSkillProgress(TEXT("Medicine"), Progress));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedicalAuthority, "MOFramework.Medical.Treatment.AuthorityAndClinical", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOMedicalAuthority::RunTest(const FString&)
{
	using namespace MOMedicalActionTests;
	FWorld F;
	TestEqual(TEXT("Unknown treatment IDs no longer produce default effects"), F.Begin(TEXT("Unknown")).ClinicalFailure, EMOWoundTreatmentFailure::UnknownTreatment);
	TestFalse(TEXT("The original clinical primitive also refuses unknown IDs"), F.Anatomy->ApplyTreatment(F.Injury.WoundId, TEXT("Unknown"), 100));
	TestEqual(TEXT("The actual healer skill is used"), F.Begin(TEXT("AuditSkilledTreatment")).ClinicalFailure, EMOWoundTreatmentFailure::InsufficientSkill);
	TestEqual(TEXT("A condition-only row cannot treat a wound"), F.Begin(TEXT("AuditWrongWound")).ClinicalFailure, EMOWoundTreatmentFailure::WrongWoundType);
	auto* SelfAnatomy = F.Add<UMOAnatomyComponent>(F.Healer);
	SelfAnatomy->ApplySaveDataAuthority(FMOAnatomySaveData()); SelfAnatomy->InflictWound(F.Injury);
	TestEqual(TEXT("Self-treatment is derived from actor identity"), FMOMedicalTreatmentActions::Begin(F.Healer, F.Healer, F.Injury.WoundId, TEXT("AuditSelfBlocked")).ClinicalFailure, EMOWoundTreatmentFailure::UnreachableForSelf);
	F.Patient->SetActorLocation(FVector(300, 0, 0));
	TestEqual(TEXT("Distant patients are refused"), F.Begin().Failure, EMOMedicalActionFailure::TooFar);
	F.Healer->SetRole(ROLE_AutonomousProxy);
	TestEqual(TEXT("A client cannot execute the native service"), F.Begin().Failure, EMOMedicalActionFailure::NotAuthority);
	F.Healer->SetRole(ROLE_Authority);
	TestEqual(TEXT("All refused courses leave clinical state intact"), F.Current().BleedRate, 10.0f);
	TestEqual(TEXT("All refused courses retain supplies"), F.Inventory->GetItemCountByDefinitionId(TEXT("Stick01")), 2);
	F.Patient->SetActorLocation(FVector(100, 0, 0));
	F.Healer->FindComponentByClass<UMOSkillsComponent>()->SetSkillLevel(TEXT("Medicine"), 100);
	TestTrue(TEXT("High actual skill permits a strong treatment"), F.Begin(TEXT("AuditStrongTreatment")).IsAccepted());
	F.Advance(F.Timed->GetState().RealSeconds + 0.01);
	TestEqual(TEXT("High-quality bleed reduction cannot create negative bleeding"), F.Current().BleedRate, 0.0f);
	TestEqual(TEXT("High-quality infection reduction cannot create negative risk"), F.Current().InfectionRisk, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOTimedActionTransitions, "MOFramework.Medical.Treatment.SharedTimedActionTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOTimedActionTransitions::RunTest(const FString&)
{
	using namespace MOMedicalActionTests;
	FWorld F;
	bool bNestedAccepted = true; int32 Completed = 0, Cancelled = 0;
	F.Timed->Begin(TEXT("Old"), FText::GetEmpty(), 10, [](){}, [&]()
	{
		++Cancelled;
		bNestedAccepted = F.Timed->Begin(TEXT("Nested"), FText::GetEmpty(), 10, [](){});
	});
	F.Timed->Begin(TEXT("Replacement"), FText::GetEmpty(), 10, [&]() { ++Completed; });
	TestFalse(TEXT("Reentrant cancellation cannot create an orphan overwritten action"), bNestedAccepted);
	TestEqual(TEXT("Old action cancelled exactly once"), Cancelled, 1);
	const double SynchronizedNow = F.Clock->Now;
	F.Clock->Now -= 3;
	TestEqual(TEXT("A newly joined clock behind the server start reports zero progress"), F.Timed->GetProgress01(), 0.0f);
	TestEqual(TEXT("Clock skew cannot display more remaining time than the full course"), F.Timed->GetRemainingRealSeconds(), F.Timed->GetState().RealSeconds);
	F.Clock->Now = SynchronizedNow;
	F.Advance(F.Timed->GetState().RealSeconds + 0.01);
	TestEqual(TEXT("Only the accepted replacement completes"), Completed, 1);
	F.Timed->Begin(TEXT("FirstPart"), FText::GetEmpty(), 10, [&]()
	{
		F.Timed->Begin(TEXT("NextPart"), FText::GetEmpty(), 10, [&]() { ++Completed; });
	});
	F.Advance(F.Timed->GetState().RealSeconds + 0.01);
	TestTrue(TEXT("Completion may intentionally chain the next action"), F.Timed->IsActive());
	TestEqual(TEXT("The next action retains its identity"), F.Timed->GetState().ActionId, FName(TEXT("NextPart")));
	F.Advance(F.Timed->GetState().RealSeconds + 0.01);
	TestEqual(TEXT("The chained action completes exactly once"), Completed, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedicalObstruction, "MOFramework.Medical.Treatment.ObstructionAndInterrupt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOMedicalObstruction::RunTest(const FString&)
{
	using namespace MOMedicalActionTests;
	FWorld F;
	auto* Wall = F.BlockSight();
	TestEqual(TEXT("Treatment cannot begin through a wall"), F.Begin().Failure, EMOMedicalActionFailure::NoLineOfSight);
	Wall->Destroy();
	TestTrue(TEXT("Removing the obstruction permits work"), F.Begin().IsAccepted());
	F.BlockSight(); F.Advance(0.1);
	TestFalse(TEXT("An obstruction arising during work cancels it"), F.Timed->IsActive());
	TestEqual(TEXT("An obstructed course consumes no physical supplies"), F.Inventory->GetItemCountByDefinitionId(TEXT("Stick01")), 2);
	TestEqual(TEXT("An obstructed course consumes no water"), F.Inventory->GetInstanceState(F.Jar).Liquid.Millilitres, 500.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedicalSaveCancellation, "MOFramework.Medical.Treatment.SaveAndCancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOMedicalSaveCancellation::RunTest(const FString&)
{
	using namespace MOMedicalActionTests;
	FWorld F;
	TestTrue(TEXT("A course starts before saving"), F.Begin().IsAccepted());
	F.Advance(F.Timed->GetState().RealSeconds * 0.5);
	FMOInventorySaveData InventoryData; FMOAnatomySaveData AnatomyData;
	F.Inventory->BuildSaveData(InventoryData); F.Anatomy->BuildSaveData(AnatomyData);
	TArray<uint8> Bytes;
	{
		FMemoryWriter Memory(Bytes); FObjectAndNameAsStringProxyArchive Archive(Memory, false);
		FMOInventorySaveData::StaticStruct()->SerializeItem(Archive, &InventoryData, nullptr);
		FMOAnatomySaveData::StaticStruct()->SerializeItem(Archive, &AnatomyData, nullptr);
	}
	FMOInventorySaveData LoadedInventory; FMOAnatomySaveData LoadedAnatomy;
	{
		FMemoryReader Memory(Bytes); FObjectAndNameAsStringProxyArchive Archive(Memory, true);
		FMOInventorySaveData::StaticStruct()->SerializeItem(Archive, &LoadedInventory, nullptr);
		FMOAnatomySaveData::StaticStruct()->SerializeItem(Archive, &LoadedAnatomy, nullptr);
	}
	F.Timed->ServerCancel();
	TestFalse(TEXT("The owning cancel path stops treatment"), F.Timed->IsActive());
	TestEqual(TEXT("Cancellation leaves the original wound unchanged"), F.Current().BleedRate, 10.0f);
	auto* Reloaded = F.SpawnPawn();
	auto* Inventory = F.Add<UMOInventoryComponent>(Reloaded);
	auto* Anatomy = F.Add<UMOAnatomyComponent>(Reloaded);
	auto* Timed = F.Add<UMOTimedActionComponent>(Reloaded);
	Reloaded->DispatchBeginPlay();
	TestTrue(TEXT("Serialized in-progress supplies restore through the real inventory path"), Inventory->ApplySaveDataAuthority(LoadedInventory));
	Anatomy->ApplySaveDataAuthority(LoadedAnatomy);
	TestFalse(TEXT("An interrupted course is not silently completed on load"), Timed->IsActive());
	TestEqual(TEXT("No physical supply is stranded in a transient escrow"), Inventory->GetItemCountByDefinitionId(TEXT("Stick01")), 2);
	TestEqual(TEXT("The original liquid volume survives saving during treatment"), Inventory->GetInstanceState(F.Jar).Liquid.Millilitres, 500.0f);
	TestEqual(TEXT("The original liquid profile survives saving during treatment"), Inventory->GetInstanceState(F.Jar).Liquid.SalinityGPerL, 0.2f);
	TestEqual(TEXT("No unearned treatment effect appears on load"), Anatomy->GetAllWounds()[0].BleedRate, 10.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedicalAuthoredRequirements, "MOFramework.Medical.Treatment.AuthoredRequirements", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOMedicalAuthoredRequirements::RunTest(const FString&)
{
	// Check the configured live assets, independently of the synthetic action fixtures and stale JSON/CSV exports.
	const auto* Settings = UMOMedicalDatabaseSettings::Get();
	UDataTable* Table = Settings ? Settings->GetMedicalTreatmentsTable() : nullptr;
	if (!TestNotNull(TEXT("Configured treatment table loads"), Table)) return false;
	TArray<FMOMedicalTreatmentRow*> Rows;
	Table->GetAllRows(TEXT("TreatmentContract"), Rows);
	TSet<FName> Seen;
	int32 WoundRows = 0;
	for (const auto* Row : Rows)
	{
		if (!Row || Row->TreatsWoundTypes.IsEmpty()) continue;
		++WoundRows;
		TestFalse(*FString::Printf(TEXT("Treatment ID %s is unique"), *Row->TreatmentId.ToString()), Seen.Contains(Row->TreatmentId));
		Seen.Add(Row->TreatmentId);
		auto Injury = MOMedicalActionTests::Wound();
		Injury.WoundType = Row->TreatsWoundTypes[0];
		TestEqual(*FString::Printf(TEXT("%s has a valid timed wound definition"), *Row->TreatmentId.ToString()),
			UMOAnatomyComponent::ValidateTreatmentDefinition(Injury, *Row, FMath::Max(0, Row->MinimumSkillLevel), false), EMOWoundTreatmentFailure::None);
		AddInfo(FString::Printf(TEXT("Treatment %s: %.2f game seconds, skill %s >= %d"), *Row->TreatmentId.ToString(), Row->TreatmentDuration,
			*Row->RequiredSkillId.ToString(), Row->MinimumSkillLevel));
		for (FName Id : Row->RequiredItemIds)
		{
			FMOItemDefinitionRow Definition;
			const bool bDefined = !Id.IsNone() && UMOItemDatabaseSettings::GetItemDefinition(Id, Definition);
			TestTrue(*FString::Printf(TEXT("%s requirement %s is a current item definition"), *Row->TreatmentId.ToString(), *Id.ToString()), bDefined);
			if (bDefined)
				AddInfo(FString::Printf(TEXT("  %s: reusableTool=%d maxDurability=%d liquidMeasureMl=%.2f safeWater=%d"), *Id.ToString(),
					Definition.bIsTool, Definition.MaxDurability, Definition.LiquidMeasureMl, Definition.bMeasureNeedsSafeWater));
		}
	}
	TestTrue(TEXT("Configured table contains wound treatment definitions"), WoundRows > 0);
	return true;
}
// The treatment menu (N-061, Oct 20 2026): the Status panel's Wounds tab shows, under each wound, a button per treatment for it -- the
// server's own verdict (Preview = Begin's checks), greyed with the reason. Before, no UI could treat a wound at all.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedicalMenuOptions, "MOFramework.Medical.Treatment.MenuOptions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOMedicalMenuOptions::RunTest(const FString&)
{
	using namespace MOMedicalActionTests;
	FWorld F;
	TArray<FMOItemActionOption> Options;
	const auto Find = [&Options](const TCHAR* Id) { return Options.FindByPredicate([Id](const FMOItemActionOption& O) { return O.Id == FName(Id); }); };
	FMOMedicalTreatmentActions::GetOptions(F.Healer, F.Patient, F.Current(), Options);
	const FMOItemActionOption* Good = Find(TEXT("AuditWoundTreatment"));
	TestTrue(TEXT("the wound's treatment is offered, ready"), Good && Good->bEnabled);
	TestTrue(TEXT("its label says how long it takes"), Good && Good->Label.ToString().Contains(TEXT("(20 s)")));
	TestNull(TEXT("a treatment for other wounds is not offered"), Find(TEXT("AuditWrongWound")));
	const FMOItemActionOption* Skilled = Find(TEXT("AuditSkilledTreatment"));
	TestTrue(TEXT("too little skill: offered, greyed, and says why"), Skilled && !Skilled->bEnabled && Skilled->DisabledReason.ToString().Contains(TEXT("skill")));
	TestTrue(TEXT("Preview agrees with Begin on a doable treatment"), FMOMedicalTreatmentActions::Preview(F.Healer, F.Patient, F.Injury.WoundId, TEXT("AuditWoundTreatment")).IsAccepted());

	F.Inventory->RemoveItemByDefinitionId(TEXT("Stick01"), 2);
	Options.Reset();
	FMOMedicalTreatmentActions::GetOptions(F.Healer, F.Patient, F.Current(), Options);
	Good = Find(TEXT("AuditWoundTreatment"));
	TestTrue(TEXT("without its supplies: greyed, naming what is missing"), Good && !Good->bEnabled && Good->DisabledReason.ToString().StartsWith(TEXT("Needs ")));
	TestEqual(TEXT("... and Begin refuses for the same reason"), F.Begin().Failure, EMOMedicalActionFailure::MissingSupplies);

	F.Inventory->AddItemByGuid(FGuid::NewGuid(), TEXT("Stick01"), 2);
	TestTrue(TEXT("supplies back: Begin starts it"), F.Begin().IsAccepted());
	TestEqual(TEXT("while it runs, the menu says the pawn is busy"), FMOMedicalTreatmentActions::Preview(F.Healer, F.Patient, F.Injury.WoundId, TEXT("AuditStrongTreatment")).Failure,
		EMOMedicalActionFailure::Busy);
	return true;
}

// A treatment's authored healing and pain effects reach the wound (N-061, Oct 20 2026): HealingSpeedBonus / PainReduction were fields nothing read.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMedicalHealingAndPain, "MOFramework.Medical.Treatment.HealingAndPainEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOMedicalHealingAndPain::RunTest(const FString&)
{
	using namespace MOMedicalActionTests;
	FWorld F;
	FMOWound Painful = Wound();   // the fixture's own wound has no severity, so no pain: a second one that hurts
	Painful.Severity = 40.0f;
	F.Anatomy->InflictWound(Painful);
	const auto Find = [&F](const FGuid& Id) { const auto All = F.Anatomy->GetAllWounds(); const FMOWound* W = All.FindByPredicate([&Id](const FMOWound& E) { return E.WoundId == Id; }); return W ? *W : FMOWound(); };
	const float PainBefore = F.Anatomy->GetTotalPainLevel();
	TestEqual(TEXT("CONTROL: an untreated wound has no treatment effects"), Find(Painful.WoundId).TreatmentHealingBonus, 0.0f);
	TestTrue(TEXT("CONTROL: the wound hurts"), PainBefore > 0.0f);
	TestTrue(TEXT("the treatment starts"), FMOMedicalTreatmentActions::Begin(F.Healer, F.Patient, Painful.WoundId, TEXT("AuditWoundTreatment")).IsAccepted());
	F.Advance(F.Timed->GetState().RealSeconds + 0.01);
	const FMOWound Treated = Find(Painful.WoundId);
	TestTrue(TEXT("its healing bonus is on the wound (heals 1 + bonus times as fast)"), Treated.TreatmentHealingBonus > 0.0f);
	TestTrue(TEXT("its pain relief is on the wound"), Treated.TreatmentPainReduction > 0.0f && Treated.TreatmentPainReduction <= 1.0f);
	TestTrue(TEXT("the wound hurts less"), F.Anatomy->GetTotalPainLevel() < PainBefore);
	FMOAnatomySaveData Saved;
	F.Anatomy->BuildSaveData(Saved);
	const FMOWoundSaveEntry* SavedWound = Saved.Wounds.FindByPredicate([&Treated](const FMOWoundSaveEntry& E) { return E.WoundId == Treated.WoundId; });
	TestTrue(TEXT("both are saved with the wound"), SavedWound && SavedWound->TreatmentHealingBonus == Treated.TreatmentHealingBonus
		&& SavedWound->TreatmentPainReduction == Treated.TreatmentPainReduction);
	return true;
}

#endif
