#include "Misc/AutomationTest.h"
#include "MOAnatomyComponent.h"
#include "MOBodyPartDefinitionRow.h"
#include "MOCharacter.h"
#include "MOCombatComponent.h"
#include "MOCombatMedicalTypes.h"
#include "MOCreature.h"
#include "MOCreatureController.h"
#include "MOMedicalDatabaseSettings.h"
#include "MOMetaHumanCharacter.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.h"

// Oct 18 2026: one bare-hand punch killed a deer (a Blueprint override of 500 on the survivor's fist), the death named no killer
// (the attacker never reached the anatomy), a spear did a fifth of a punch (ArmorPenetration scaled unarmored damage), no creature
// ever heard anything (NoTeam vs NoTeam is Friendly to the hearing sense) and a wolf chased forever.

#if WITH_DEV_AUTOMATION_TESTS
namespace MOCreatureCombatTests
{
	/** An anatomy that accepts damage: an owner with authority, body parts initialized (see MOMedicalSystemTests). */
	UMOAnatomyComponent* NewInitializedAnatomy()
	{
		AActor* OwnerActor = NewObject<AActor>();
		UMOAnatomyComponent* Anatomy = NewObject<UMOAnatomyComponent>(OwnerActor);
		Anatomy->ApplySaveDataAuthority(FMOAnatomySaveData());
		return Anatomy;
	}

	/** The combat component a Blueprint pawn is built with (its construction script's template, searched up the Blueprint chain). */
	const UMOCombatComponent* FindCombatTemplate(UClass* PawnClass)
	{
		for (UClass* Class = PawnClass; Class; Class = Class->GetSuperClass())
		{
			const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Class);
			if (!Generated || !Generated->SimpleConstructionScript)
			{
				continue;
			}
			for (const USCS_Node* Node : Generated->SimpleConstructionScript->GetAllNodes())
			{
				if (const UMOCombatComponent* Combat = Node ? Cast<UMOCombatComponent>(Node->ComponentTemplate) : nullptr)
				{
					return Combat;
				}
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCombatArmorPenetrationNeedsArmor, "MOFramework.Combat.Damage.ArmorPenetrationNeedsArmor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCombatArmorPenetrationNeedsArmor::RunTest(const FString& Parameters)
{
	// A wooden spear's light thrust (DT_Weapons: 10 damage, penetration 0.1) into an unarmored body lands all 10.
	FMOCombatHitInfo Hit;
	Hit.BaseDamage = 10.0f;
	Hit.ArmorPenetration = 0.1f;
	TestEqual(TEXT("unarmored: penetration does not scale the blow"), Hit.GetFinalDamage(), 10.0f);
	Hit.CriticalMultiplier = 2.0f;
	TestEqual(TEXT("a critical hit still doubles it"), Hit.GetFinalDamage(), 20.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCombatHitCarriesInstigator, "MOFramework.Combat.Damage.HitCarriesInstigator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCombatHitCarriesInstigator::RunTest(const FString& Parameters)
{
	UMOAnatomyComponent* Anatomy = MOCreatureCombatTests::NewInitializedAnatomy();
	AActor* Attacker = NewObject<AActor>();
	FMOCombatHitInfo Hit;
	Hit.TargetBodyPart = EMOBodyPartType::Torso;
	Hit.BaseDamage = 5.0f;
	Hit.Instigator = Attacker;
	TestTrue(TEXT("the blow is applied"), UMOCombatMedicalHelpers::ApplyCombatDamage(Hit, Anatomy));
	TestTrue(TEXT("the anatomy knows who struck it (the creature's killer / what it flees)"), Anatomy->GetLastDamageInstigator() == Attacker);

	// The direct path a wolf's bite takes (BTTask_CreatureAttack).
	AActor* Wolf = NewObject<AActor>();
	TestTrue(TEXT("a bite is applied"), Anatomy->InflictDamage(EMOBodyPartType::ThighLeft, 5.0f, EMOWoundType::Laceration, Wolf));
	TestTrue(TEXT("the biter is the latest instigator"), Anatomy->GetLastDamageInstigator() == Wolf);

	// Damage from nobody (a fall) is nobody's doing.
	Anatomy->InflictDamage(EMOBodyPartType::CalfLeft, 5.0f, EMOWoundType::Blunt);
	TestNull(TEXT("environment damage names nobody"), Anatomy->GetLastDamageInstigator());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCombatBareHandCannotDestroyVitalPart, "MOFramework.Combat.Damage.BareHandCannotDestroyVitalPart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCombatBareHandCannotDestroyVitalPart::RunTest(const FString& Parameters)
{
	// Realism (Principle 11): a fist does not destroy a vital body part -- a deer's torso, a skull -- in one blow. Every survivor pawn's
	// own fist (a Blueprint can override the class default; BP_MOMetaHuman_Female did, with 500) at point blank with no distance falloff.
	const UMOMedicalDatabaseSettings* Medical = GetDefault<UMOMedicalDatabaseSettings>();
	UDataTable* BodyParts = Medical ? Medical->GetBodyPartDefinitionsTable() : nullptr;
	UDataTable* Wounds = Medical ? Medical->GetWoundTypeDefinitionsTable() : nullptr;
	if (!TestNotNull(TEXT("DT_BodyParts"), BodyParts) || !TestNotNull(TEXT("DT_Wounds"), Wounds))
	{
		return false;
	}

	struct FFist { FString Name; const UMOCombatComponent* Combat; };
	TArray<FFist> Fists = {{TEXT("UMOCombatComponent default"), GetDefault<UMOCombatComponent>()}};
	for (const TCHAR* Path : {TEXT("/MOFramework/Characters/BP_MOMetaHuman_Female.BP_MOMetaHuman_Female_C"),
		TEXT("/MOFramework/Characters/BP_MOCharacter.BP_MOCharacter_C")})
	{
		UClass* PawnClass = LoadObject<UClass>(nullptr, Path);
		const UMOCombatComponent* Combat = PawnClass ? MOCreatureCombatTests::FindCombatTemplate(PawnClass) : nullptr;
		if (TestNotNull(FString::Printf(TEXT("%s has a combat component"), Path), Combat))
		{
			Fists.Add({FString(Path), Combat});
		}
	}

	int32 VitalParts = 0;
	for (const FFist& Fist : Fists)
	{
		const FMOAttackDamageProfile& Profile = Fist.Combat->GetUnarmedAttackProfile();
		FMOCombatHitInfo Hit;
		Hit.BaseDamage = Profile.BaseDamage;
		Hit.DamageCategory = Profile.PrimaryDamageType;
		Hit.ArmorPenetration = Profile.ArmorPenetration;
		float WoundMultiplier = 1.0f;
		for (const TPair<FName, uint8*>& Row : Wounds->GetRowMap())
		{
			const FMOWoundTypeDefinitionRow* Wound = reinterpret_cast<const FMOWoundTypeDefinitionRow*>(Row.Value);
			if (Wound && Wound->WoundType == Hit.GetWoundType())
			{
				WoundMultiplier = Wound->BaseDamageMultiplier;
			}
		}
		const float Blow = Hit.GetFinalDamage() * WoundMultiplier;   // what InflictDamage takes off the part's HP
		VitalParts = 0;
		for (const TPair<FName, uint8*>& Row : BodyParts->GetRowMap())
		{
			const FMOBodyPartDefinitionRow* Part = reinterpret_cast<const FMOBodyPartDefinitionRow*>(Row.Value);
			if (Part && AMOCreature::IsVitalPart(Part->PartType))
			{
				++VitalParts;
				TestTrue(FString::Printf(TEXT("%s: one bare-hand blow (%.1f HP) leaves %s (%.0f HP) standing"), *Fist.Name, Blow,
					*UEnum::GetValueAsString(Part->PartType), Part->BaseHP), Blow < Part->BaseHP);
			}
		}
	}
	TestTrue(TEXT("the vital parts are in DT_BodyParts"), VitalParts >= 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCreatureHearsUnteamedPawns, "MOFramework.Creature.Perception.HearsUnteamedPawns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCreatureHearsUnteamedPawns::RunTest(const FString& Parameters)
{
	// The hearing sense filters by raw team ids; every MO57 pawn and controller is NoTeam, which the engine's solver calls Friendly.
	const uint8 Flags = AMOCreatureController::SenseAllAffiliations().GetAsFlags();
	TestTrue(TEXT("a NoTeam footstep reaches a NoTeam listener"),
		FAISenseAffiliationFilter::ShouldSenseTeam(FGenericTeamId::NoTeam, FGenericTeamId::NoTeam, Flags));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCreatureChaseGivesUp, "MOFramework.Creature.Pursuit.GivesUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCreatureChaseGivesUp::RunTest(const FString& Parameters)
{
	const float Reach = 225.0f;     // a wolf's 150 cm attack range x 1.5
	const float Leash = 20000.0f;   // 200 m from the den
	const float NoGain = 20.0f;

	// Equal speeds (wolf and deer both run 750 cm/s): the gap holds at ~15 m; it gives up 20 s in, not before.
	float Mark = 1500.0f;
	double LastGain = 0.0;
	const TCHAR* Verdict = nullptr;
	double GaveUpAt = -1.0;
	for (double Now = 0.0; Now <= 30.0 && !Verdict; Now += 0.2)
	{
		Verdict = AMOCreatureController::JudgeChase(1500.0f + 50.0f * FMath::Sin(Now), 5000.0f, Now, Reach, Leash, NoGain, Mark, LastGain);
		GaveUpAt = Verdict ? Now : GaveUpAt;
	}
	TestTrue(TEXT("a chase it is not winning is given up"), Verdict != nullptr && FCString::Strcmp(Verdict, TEXT("not gaining on it")) == 0);
	TestTrue(FString::Printf(TEXT("after ~20 s without closing in (gave up at %.1f s)"), GaveUpAt), GaveUpAt > 19.9 && GaveUpAt < 21.0);

	// Closing in by a metre a second: it keeps chasing.
	Mark = 3000.0f;
	LastGain = 0.0;
	Verdict = nullptr;
	for (double Now = 0.0; Now <= 25.0 && !Verdict; Now += 0.2)
	{
		Verdict = AMOCreatureController::JudgeChase(3000.0f - 100.0f * static_cast<float>(Now), 5000.0f, Now, Reach, Leash, NoGain, Mark, LastGain);
	}
	TestNull(TEXT("a chase that is closing in goes on"), Verdict);

	// Biting (within reach) is not a stalled chase.
	Mark = 200.0f;
	LastGain = 0.0;
	TestNull(TEXT("within reach after 60 s still fights"), AMOCreatureController::JudgeChase(200.0f, 5000.0f, 60.0, Reach, Leash, NoGain, Mark, LastGain));

	// Past the leash from home: given up at once, even while gaining.
	Mark = 3000.0f;
	LastGain = 0.0;
	Verdict = AMOCreatureController::JudgeChase(500.0f, 20100.0f, 1.0, Reach, Leash, NoGain, Mark, LastGain);
	TestTrue(TEXT("201 m from the den it turns back"), Verdict != nullptr && FCString::Strcmp(Verdict, TEXT("too far from home")) == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCharacterFootfallLoudness, "MOFramework.Creature.Perception.FootfallLoudness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCharacterFootfallLoudness::RunTest(const FString& Parameters)
{
	// The heard loudness follows the actual speed through the pawn's gaits (the survivor's walk 200 / jog 500 / sprint 700: GASP's, Oct 28): a player's
	// gaits keep their old values, an AI pawn running is loud (its player-only gait state used to leave it at the walk's 0.3).
	const AMOCharacter* Pawn = GetDefault<AMOMetaHumanCharacter>();   // the player's survivor
	TestEqual(TEXT("standing still"), Pawn->GetMovementNoiseLoudness(0.0f), 0.0f);
	TestEqual(TEXT("half a walk is half as loud"), Pawn->GetMovementNoiseLoudness(100.0f), 0.15f, 0.001f);
	TestEqual(TEXT("walk"), Pawn->GetMovementNoiseLoudness(200.0f), 0.3f, 0.001f);
	TestEqual(TEXT("jog"), Pawn->GetMovementNoiseLoudness(500.0f), 0.6f, 0.001f);
	TestEqual(TEXT("sprint"), Pawn->GetMovementNoiseLoudness(700.0f), 1.0f, 0.001f);
	TestEqual(TEXT("faster than a sprint is no louder"), Pawn->GetMovementNoiseLoudness(900.0f), 1.0f, 0.001f);
	return true;
}

#endif
