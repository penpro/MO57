#pragma once

#include "CoreMinimal.h"
#include "MOAnatomyComponent.h"
#include "MOIngredientReservation.h"

class APawn;
struct FMOInventoryEntry;
struct FMOMedicalTreatmentRow;

enum class EMOMedicalActionFailure : uint8
{
	None, NotAuthority, InvalidPatient, NoActionComponent, Busy, CannotAct, TooFar, NoLineOfSight,
	PatientMoved, Clinical, UnknownSupply, MissingSupplies, BrokenTool, InvalidSupplyState
};

struct MOFRAMEWORK_API FMOMedicalActionResult
{
	EMOMedicalActionFailure Failure = EMOMedicalActionFailure::None;
	EMOWoundTreatmentFailure ClinicalFailure = EMOWoundTreatmentFailure::None;
	FName SupplyId;
	bool IsAccepted() const { return Failure == EMOMedicalActionFailure::None; }
	FText Describe() const;
};

/** Shared player/AI wound action. The server derives skill, self-treatment, time, reach and supplies.
 * Supplies stay in the healer's inventory throughout work; cancellation/save/reload cannot strand an escrow.
 * Completion revalidates and installs one joint physical/liquid plan with the clinical effect before observers run.
 * The authored row is captured at start, so content edits cannot shorten or change an ongoing treatment. */
struct MOFRAMEWORK_API FMOMedicalTreatmentActions
{
	static FMOMedicalActionResult Begin(APawn* Healer, AActor* Patient, const FGuid& WoundId, FName TreatmentId);

	/** RequiredItemIds means one unit per occurrence. Defined tools are reusable presence requirements;
	 * broken tools fail. Consumables and liquid measures use the same joint planner as crafting.
	 * Pure and atomic: refusal leaves OutPlan untouched. Unknown requirements fail closed. */
	static FMOMedicalActionResult PlanSupplies(const FMOMedicalTreatmentRow& Treatment, const TArray<FMOInventoryEntry>& Entries,
		TArray<FMOInventoryEntry>& OutPlan, MOIngredientPlans::FDefinitionLookup Definition);
private:
	static FMOMedicalActionResult CheckContext(APawn* Healer, AActor* Patient, const FGuid& WoundId,
		const FMOMedicalTreatmentRow& Treatment, int32& OutSkill, TArray<FMOInventoryEntry>& OutPlan);
};
