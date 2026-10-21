#pragma once

#include "CoreMinimal.h"
#include "MOAnatomyComponent.h"
#include "MOIngredientReservation.h"

class APawn;
struct FMOInventoryEntry;
struct FMOMedicalTreatmentRow;
struct FMOItemActionOption;

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

	/** What Begin would answer, starting nothing: the treatment menu's verdict on any machine that sees the patient's wounds (the server, or a
	 *  player's own pawn on its client -- wounds replicate owner-only). Begin's checks, the authority ones aside, plus "busy". */
	static FMOMedicalActionResult Preview(APawn* Healer, AActor* Patient, const FGuid& WoundId, FName TreatmentId);

	/** The menu's buttons for one wound (N-061): every treatment row for its wound type, "<name> (<seconds> s)", greyed with Preview's reason
	 *  when it cannot be done now. A click sends AMOPlayerController::ServerTreatWound; the server checks it all again. */
	static void GetOptions(APawn* Healer, AActor* Patient, const FMOWound& Wound, TArray<FMOItemActionOption>& Out);

	/** The colony medic's choice (N-061): the patient's worst bleeding wound at or above MinBleedMlPerSecond, and of the treatments for it that
	 *  slow bleeding, the one Healer has the skill and supplies for with the most total benefit (bleed + infection + healing reductions), so a
	 *  dressing beats a tourniquet, which stays the fallback when it is all the medic carries. ExtraSupplies counts as carried too (a settlement
	 *  store the medic would fetch from first). Healer == Patient is self-treatment, with its penalty and its unreachable body parts. Reach,
	 *  sight and a busy healer are ignored (the job walks there first; Begin checks them all at the bedside). False when nothing needs, or
	 *  nothing can get, treatment. */
	static bool PickForMedic(APawn* Healer, AActor* Patient, float MinBleedMlPerSecond, const TArray<FMOInventoryEntry>& ExtraSupplies,
		FGuid& OutWoundId, FName& OutTreatmentId);

	/** RequiredItemIds means one unit per occurrence. Defined tools are reusable presence requirements;
	 * broken tools fail. Consumables and liquid measures use the same joint planner as crafting.
	 * Pure and atomic: refusal leaves OutPlan untouched. Unknown requirements fail closed. */
	static FMOMedicalActionResult PlanSupplies(const FMOMedicalTreatmentRow& Treatment, const TArray<FMOInventoryEntry>& Entries,
		TArray<FMOInventoryEntry>& OutPlan, MOIngredientPlans::FDefinitionLookup Definition);
private:
	static FMOMedicalActionResult CheckContext(APawn* Healer, AActor* Patient, const FGuid& WoundId,
		const FMOMedicalTreatmentRow& Treatment, int32& OutSkill, TArray<FMOInventoryEntry>& OutPlan, bool bPreview = false);
};
