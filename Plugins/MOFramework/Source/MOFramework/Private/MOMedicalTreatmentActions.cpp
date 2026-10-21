#include "MOMedicalTreatmentActions.h"
#include "MOBodyPartDefinitionRow.h"
#include "MOCharacter.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOItemActions.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOMedicalSubsystem.h"
#include "MOMentalStateComponent.h"
#include "MOPlayerController.h"
#include "MOSkillsComponent.h"
#include "MOTimedActionComponent.h"
#include "MOViewpointUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "MOMedicalTreatment"

FText FMOMedicalActionResult::Describe() const
{
	switch (Failure)
	{
	case EMOMedicalActionFailure::None: return FText::GetEmpty();
	case EMOMedicalActionFailure::NotAuthority: return LOCTEXT("Authority", "Treatment must be requested on the server.");
	case EMOMedicalActionFailure::InvalidPatient: return LOCTEXT("Patient", "That patient cannot be treated.");
	case EMOMedicalActionFailure::NoActionComponent: return LOCTEXT("Action", "This pawn cannot perform a timed treatment.");
	case EMOMedicalActionFailure::Busy: return LOCTEXT("Busy", "Finish or cancel the current action first.");
	case EMOMedicalActionFailure::CannotAct: return LOCTEXT("CannotAct", "You cannot perform treatment in your current state.");
	case EMOMedicalActionFailure::TooFar: return LOCTEXT("Reach", "Move within reach of the patient.");
	case EMOMedicalActionFailure::NoLineOfSight: return LOCTEXT("Sight", "The patient is obstructed.");
	case EMOMedicalActionFailure::PatientMoved: return LOCTEXT("Moved", "Treatment stopped because the patient moved.");
	case EMOMedicalActionFailure::UnknownSupply: return FText::Format(LOCTEXT("UnknownSupply", "Treatment requires an undefined supply: {0}."), FText::FromName(SupplyId));
	case EMOMedicalActionFailure::MissingSupplies: return FText::Format(LOCTEXT("Supplies", "Needs {0}."), UMOItemDatabaseSettings::GetItemDisplayName(SupplyId));
	case EMOMedicalActionFailure::BrokenTool: return FText::Format(LOCTEXT("Tool", "The {0} is broken."), UMOItemDatabaseSettings::GetItemDisplayName(SupplyId));
	case EMOMedicalActionFailure::InvalidSupplyState: return LOCTEXT("InvalidSupplies", "Treatment supplies have invalid inventory state.");
	case EMOMedicalActionFailure::Clinical:
		switch (ClinicalFailure)
		{
		case EMOWoundTreatmentFailure::UnknownTreatment: return LOCTEXT("Unknown", "Unknown treatment.");
		case EMOWoundTreatmentFailure::MissingWound: return LOCTEXT("Wound", "That wound no longer needs treatment.");
		case EMOWoundTreatmentFailure::WrongWoundType: return LOCTEXT("Type", "This treatment does not address that wound.");
		case EMOWoundTreatmentFailure::InsufficientSkill: return LOCTEXT("Skill", "Insufficient treatment skill.");
		case EMOWoundTreatmentFailure::UnreachableForSelf: return LOCTEXT("Self", "You cannot reach that body part to treat yourself.");
		case EMOWoundTreatmentFailure::InvalidWound: return LOCTEXT("WoundState", "The wound is not in a treatable state.");
		default: return LOCTEXT("Definition", "Treatment has an invalid clinical definition.");
		}
	}
	return LOCTEXT("Refused", "Treatment refused.");
}

FMOMedicalActionResult FMOMedicalTreatmentActions::PlanSupplies(const FMOMedicalTreatmentRow& Treatment,
	const TArray<FMOInventoryEntry>& Entries, TArray<FMOInventoryEntry>& OutPlan, MOIngredientPlans::FDefinitionLookup Definition)
{
	TMap<FName, int32> Consumables;
	TMap<FName, int32> Tools;
	for (FName Id : Treatment.RequiredItemIds)
	{
		FMOItemDefinitionRow Def;
		if (Id.IsNone() || !Definition(Id, Def)) return {EMOMedicalActionFailure::UnknownSupply, EMOWoundTreatmentFailure::None, Id};
		int32& Count = Def.bIsTool ? Tools.FindOrAdd(Id) : Consumables.FindOrAdd(Id);
		if (Count == MAX_int32) return {EMOMedicalActionFailure::UnknownSupply, EMOWoundTreatmentFailure::None, Id};
		++Count;
	}
	for (const auto& Tool : Tools)
	{
		int64 Usable = 0, Broken = 0;
		for (const auto& Entry : Entries)
		{
			if (Entry.ItemDefinitionId != Tool.Key || Entry.Quantity <= 0) continue;
			if (Entry.CurrentDurability == 0) Broken += Entry.Quantity;
			else Usable += Entry.Quantity;
		}
		if (Usable < Tool.Value) return {Broken > 0 ? EMOMedicalActionFailure::BrokenTool : EMOMedicalActionFailure::MissingSupplies,
			EMOWoundTreatmentFailure::None, Tool.Key};
	}
	TArray<FMOInventoryEntry> Plan = Entries;
	FMOIngredientReservation Used;
	const auto Result = MOIngredientPlans::Reserve(Plan, Consumables, Used, Definition);
	if (!Result.IsComplete())
	{
		const auto Failure = Result.Failure == EMOIngredientFailure::UnknownDefinition ? EMOMedicalActionFailure::UnknownSupply :
			(Result.Failure == EMOIngredientFailure::MissingIngredients ? EMOMedicalActionFailure::MissingSupplies : EMOMedicalActionFailure::InvalidSupplyState);
		return {Failure, EMOWoundTreatmentFailure::None, Result.IngredientId};
	}
	OutPlan = MoveTemp(Plan);
	return {};
}

FMOMedicalActionResult FMOMedicalTreatmentActions::CheckContext(APawn* Healer, AActor* Patient, const FGuid& WoundId,
	const FMOMedicalTreatmentRow& Treatment, int32& OutSkill, TArray<FMOInventoryEntry>& OutPlan, bool bPreview)
{
	if (!IsValid(Healer) || (!bPreview && !Healer->HasAuthority())) return {EMOMedicalActionFailure::NotAuthority};
	if (!IsValid(Patient) || (!bPreview && !Patient->HasAuthority()) || Patient->GetWorld() != Healer->GetWorld() || !Healer->GetWorld())
		return {EMOMedicalActionFailure::InvalidPatient};
	const auto* HealerCharacter = Cast<AMOCharacter>(Healer);
	const auto* PatientCharacter = Cast<AMOCharacter>(Patient);
	const auto* Mental = Healer->FindComponentByClass<UMOMentalStateComponent>();
	if ((HealerCharacter && HealerCharacter->IsDead()) || (Mental && !Mental->CanPerformActions())) return {EMOMedicalActionFailure::CannotAct};
	if (PatientCharacter && PatientCharacter->IsDead()) return {EMOMedicalActionFailure::InvalidPatient};
	auto* Anatomy = Patient->FindComponentByClass<UMOAnatomyComponent>();
	if (!Anatomy) return {EMOMedicalActionFailure::InvalidPatient};
	if (FVector::DistSquared(Healer->GetActorLocation(), Patient->GetActorLocation()) > FMath::Square(200.0)) return {EMOMedicalActionFailure::TooFar};
	if (Patient != Healer)
	{
		FVector Eye; FRotator Rotation;
		UMOViewpointUtils::FLineOfSightOptions Options;
		Options.IgnoredActor = Healer;
		Options.bAllowAttachedHits = true;
		if (!UMOViewpointUtils::ResolveViewpointForPawn(Healer, Eye, Rotation) ||
			!UMOViewpointUtils::HasLineOfSight(Healer->GetWorld(), Eye, Patient, Options)) return {EMOMedicalActionFailure::NoLineOfSight};
	}
	const auto* Skills = Healer->FindComponentByClass<UMOSkillsComponent>();
	OutSkill = Skills && !Treatment.RequiredSkillId.IsNone() ? Skills->GetSkillLevel(Treatment.RequiredSkillId) : 0;
	const auto Wounds = Anatomy->GetAllWounds();
	const FMOWound* Wound = Wounds.FindByPredicate([&](const FMOWound& Entry) { return Entry.WoundId == WoundId; });
	if (!Wound) return {EMOMedicalActionFailure::Clinical, EMOWoundTreatmentFailure::MissingWound};
	const auto Clinical = UMOAnatomyComponent::ValidateTreatmentDefinition(*Wound, Treatment, OutSkill, Patient == Healer);
	if (Clinical != EMOWoundTreatmentFailure::None) return {EMOMedicalActionFailure::Clinical, Clinical};
	auto* Inventory = Healer->FindComponentByClass<UMOInventoryComponent>();
	TArray<FMOInventoryEntry> Entries;
	if (Inventory) Inventory->GetInventoryEntries(Entries);
	return PlanSupplies(Treatment, Entries, OutPlan, UMOItemDatabaseSettings::GetItemDefinition);
}

FMOMedicalActionResult FMOMedicalTreatmentActions::Preview(APawn* Healer, AActor* Patient, const FGuid& WoundId, FName TreatmentId)
{
	const UGameInstance* GI = Healer && Healer->GetWorld() ? Healer->GetWorld()->GetGameInstance() : nullptr;
	const auto* Medical = GI ? GI->GetSubsystem<UMOMedicalSubsystem>() : nullptr;
	FMOMedicalTreatmentRow Treatment;
	if (!Medical || !Medical->GetTreatmentDefinition(TreatmentId, Treatment)) return {EMOMedicalActionFailure::Clinical, EMOWoundTreatmentFailure::UnknownTreatment};
	int32 Skill = 0;
	TArray<FMOInventoryEntry> Plan;
	const auto Result = CheckContext(Healer, Patient, WoundId, Treatment, Skill, Plan, /*bPreview=*/true);
	if (!Result.IsAccepted()) return Result;
	const auto* Timed = Healer->FindComponentByClass<UMOTimedActionComponent>();
	if (!Timed) return {EMOMedicalActionFailure::NoActionComponent};
	if (Timed->IsActive()) return {EMOMedicalActionFailure::Busy};
	return Result;
}

void FMOMedicalTreatmentActions::GetOptions(APawn* Healer, AActor* Patient, const FMOWound& Wound, TArray<FMOItemActionOption>& Out)
{
	const UGameInstance* GI = Healer && Healer->GetWorld() ? Healer->GetWorld()->GetGameInstance() : nullptr;
	const auto* Medical = GI ? GI->GetSubsystem<UMOMedicalSubsystem>() : nullptr;
	if (!Medical)
	{
		return;
	}
	for (const FMOMedicalTreatmentRow& Row : Medical->GetTreatmentsForWoundType(Wound.WoundType))
	{
		const FMOMedicalActionResult Result = Preview(Healer, Patient, Wound.WoundId, Row.TreatmentId);
		if (Result.Failure == EMOMedicalActionFailure::Clinical && (Result.ClinicalFailure == EMOWoundTreatmentFailure::WrongWoundType ||
			Result.ClinicalFailure == EMOWoundTreatmentFailure::MissingWound || Result.ClinicalFailure == EMOWoundTreatmentFailure::UnknownTreatment))
		{
			continue;   // not a treatment for this wound at all
		}
		FMOItemActionOption& Option = Out.AddDefaulted_GetRef();
		Option.Id = Row.TreatmentId;
		Option.Label = FText::Format(LOCTEXT("TreatOption", "{0} ({1} s)"), Row.DisplayName.IsEmpty() ? FText::FromName(Row.TreatmentId) : Row.DisplayName,
			FText::AsNumber(FMath::RoundToInt(Row.TreatmentDuration)));
		Option.bEnabled = Result.IsAccepted();
		Option.DisabledReason = Result.Describe();
	}
}

bool FMOMedicalTreatmentActions::PickForMedic(APawn* Healer, AActor* Patient, float MinBleedMlPerSecond, const TArray<FMOInventoryEntry>& ExtraSupplies,
	FGuid& OutWoundId, FName& OutTreatmentId)
{
	const UGameInstance* GI = IsValid(Healer) && Healer->GetWorld() ? Healer->GetWorld()->GetGameInstance() : nullptr;
	const auto* Medical = GI ? GI->GetSubsystem<UMOMedicalSubsystem>() : nullptr;
	const auto* Anatomy = IsValid(Patient) ? Patient->FindComponentByClass<UMOAnatomyComponent>() : nullptr;
	if (!Medical || !Anatomy) return false;
	if (const auto* PatientCharacter = Cast<AMOCharacter>(Patient); PatientCharacter && PatientCharacter->IsDead()) return false;
	if (const auto* HealerCharacter = Cast<AMOCharacter>(Healer); HealerCharacter && HealerCharacter->IsDead()) return false;
	TArray<FMOWound> Wounds = Anatomy->GetAllWounds();
	Wounds.Sort([](const FMOWound& A, const FMOWound& B) { return A.BleedRate > B.BleedRate; });
	const auto* Skills = Healer->FindComponentByClass<UMOSkillsComponent>();
	TArray<FMOInventoryEntry> Entries;
	if (const auto* Inventory = Healer->FindComponentByClass<UMOInventoryComponent>()) Inventory->GetInventoryEntries(Entries);
	Entries.Append(ExtraSupplies);
	for (const FMOWound& Wound : Wounds)
	{
		if (!(Wound.BleedRate >= MinBleedMlPerSecond)) break;   // sorted: nothing after this one bleeds enough either
		const TArray<FMOMedicalTreatmentRow> Rows = Medical->GetTreatmentsForWoundType(Wound.WoundType);
		const FMOMedicalTreatmentRow* Best = nullptr;
		float BestBenefit = 0.0f;
		for (const FMOMedicalTreatmentRow& Row : Rows)
		{
			if (Row.BleedReduction <= 0.0f) continue;
			const int32 Skill = Skills && !Row.RequiredSkillId.IsNone() ? Skills->GetSkillLevel(Row.RequiredSkillId) : 0;
			if (UMOAnatomyComponent::ValidateTreatmentDefinition(Wound, Row, Skill, Patient == Healer) != EMOWoundTreatmentFailure::None) continue;
			TArray<FMOInventoryEntry> Plan;
			if (!PlanSupplies(Row, Entries, Plan, UMOItemDatabaseSettings::GetItemDefinition).IsAccepted()) continue;
			const float Benefit = Row.BleedReduction + Row.InfectionReduction + Row.HealingSpeedBonus;
			if (!Best || Benefit > BestBenefit) { Best = &Row; BestBenefit = Benefit; }
		}
		if (Best)
		{
			OutWoundId = Wound.WoundId;
			OutTreatmentId = Best->TreatmentId;
			return true;
		}
	}
	return false;
}

FMOMedicalActionResult FMOMedicalTreatmentActions::Begin(APawn* Healer, AActor* Patient, const FGuid& WoundId, FName TreatmentId)
{
	if (!IsValid(Healer) || !Healer->HasAuthority()) return {EMOMedicalActionFailure::NotAuthority};
	const UGameInstance* GI = Healer->GetWorld() ? Healer->GetWorld()->GetGameInstance() : nullptr;
	const auto* Medical = GI ? GI->GetSubsystem<UMOMedicalSubsystem>() : nullptr;
	FMOMedicalTreatmentRow Treatment;
	if (!Medical || !Medical->GetTreatmentDefinition(TreatmentId, Treatment)) return {EMOMedicalActionFailure::Clinical, EMOWoundTreatmentFailure::UnknownTreatment};
	int32 Skill = 0;
	TArray<FMOInventoryEntry> Plan;
	auto Result = CheckContext(Healer, Patient, WoundId, Treatment, Skill, Plan);
	if (!Result.IsAccepted()) return Result;
	auto* Timed = Healer->FindComponentByClass<UMOTimedActionComponent>();
	if (!Timed) return {EMOMedicalActionFailure::NoActionComponent};
	if (Timed->IsActive()) return {EMOMedicalActionFailure::Busy};
	const TWeakObjectPtr<APawn> WeakHealer(Healer);
	const TWeakObjectPtr<AActor> WeakPatient(Patient);
	const FVector PatientStart = Patient->GetActorLocation();
	// Share the same captured server-authored course across continuation and completion.
	const auto Course = MakeShared<FMOMedicalTreatmentRow>(MoveTemp(Treatment));
	const auto Check = [WeakHealer, WeakPatient, WoundId, PatientStart, Course](int32& CurrentSkill, TArray<FMOInventoryEntry>& CurrentPlan)
	{
		if (WeakPatient.IsValid() && FVector::DistSquared(WeakPatient->GetActorLocation(), PatientStart) > FMath::Square(75.0))
			return FMOMedicalActionResult{EMOMedicalActionFailure::PatientMoved};
		return CheckContext(WeakHealer.Get(), WeakPatient.Get(), WoundId, *Course, CurrentSkill, CurrentPlan);
	};
	const auto Notify = [WeakHealer](const FMOMedicalActionResult& Refusal)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTreatment] %s: %s"), *GetNameSafe(WeakHealer.Get()), *Refusal.Describe().ToString());
		if (WeakHealer.IsValid())
			if (auto* PC = Cast<AMOPlayerController>(WeakHealer->GetController())) PC->ClientShowNotification(Refusal.Describe(), 3, EMONotificationType::Warning);
	};
	const auto LastRefusal = MakeShared<FMOMedicalActionResult>();
	const FText Label = FText::Format(LOCTEXT("Treating", "Treating: {0}"), Course->DisplayName.IsEmpty() ? FText::FromName(Course->TreatmentId) : Course->DisplayName);
	const bool bStarted = Timed->Begin(TEXT("MedicalTreatment"), Label, Course->TreatmentDuration,
		[WeakHealer, WeakPatient, WoundId, Course, Check, Notify]()
		{
			int32 CurrentSkill = 0; TArray<FMOInventoryEntry> CurrentPlan;
			const auto Completion = Check(CurrentSkill, CurrentPlan);
			if (!Completion.IsAccepted()) { Notify(Completion); return; }
			APawn* Executor = WeakHealer.Get(); AActor* Target = WeakPatient.Get();
			auto* Inventory = Executor->FindComponentByClass<UMOInventoryComponent>();
			auto* Anatomy = Target->FindComponentByClass<UMOAnatomyComponent>();
			const TWeakObjectPtr<UMOSkillsComponent> Skills(Executor->FindComponentByClass<UMOSkillsComponent>());
			{
				// Neither the clinical primitive nor planning calls observers. Install both halves before inventory notifications.
				FMOInventoryMutationScope Mutation(Inventory);
				Anatomy->ApplyResolvedTreatment(WoundId, *Course, CurrentSkill, Executor == Target);
				if (Inventory) Inventory->InstallIngredientPlan(MoveTemp(CurrentPlan));
			}
			if (Skills.IsValid() && !Course->RequiredSkillId.IsNone() && Course->SkillXPGrant > 0)
				Skills->AddExperience(Course->RequiredSkillId, Course->SkillXPGrant);
		}, [LastRefusal, Notify]() { if (!LastRefusal->IsAccepted()) Notify(*LastRefusal); }, 75.0f,
		[Check, LastRefusal]()
		{
			int32 CurrentSkill = 0; TArray<FMOInventoryEntry> CurrentPlan;
			*LastRefusal = Check(CurrentSkill, CurrentPlan);
			return LastRefusal->IsAccepted();
		});
	return bStarted ? FMOMedicalActionResult{} : FMOMedicalActionResult{EMOMedicalActionFailure::Busy};
}

#undef LOCTEXT_NAMESPACE
