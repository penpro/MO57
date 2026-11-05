// MO.Test.SaveState (Nov 5 2026; Tools/check_save_roundtrip.py drives it): one line for each person and carcass in the world, of what a save would write for them, so a check can save,
// load in a real world and compare. It reads each component's own BuildSaveData: the line is the save's view, not a second set of getters that could disagree with it. A carcass line adds
// what it DRAWS (the mesh and pose on its poseable component), which the record alone cannot show. Development builds only.
//
//   [MOTEST] SAVESTATE pawn name=... guid=... player=1 dead=0 items=Stick01:2 learned=0 traits=-0.40,0.70,-0.90 clock=LungRight:152.0 wet=0.80 o2=0.52 asphyx=0.0 circ=12.0 resp=0.0 heat=0.00 fatal=None
//                      history=3 left=none lit=1 skills=4 clocks=2 melee=0:2.0 defense=0:0.0 digesting=1 vitaminc=0.40
//   [MOTEST] SAVESTATE carcass name=... row=Wolf at=x,y,z decay=Fresh since=12.0 cut=Bones+Hide mesh=SK_Wolf bones=64 saved=64 pose=1234.5 root=x,y,z
//   [MOTEST] SAVESTATE end seed=670711536 people=2 carcasses=1

#include "MOAnatomyComponent.h"
#include "MOCarcassActor.h"
#include "MOCarriedLightComponent.h"
#include "MOCharacter.h"
#include "MOCharacterHistoryComponent.h"
#include "MOCreature.h"
#include "MOFramework.h"
#include "MOGameState.h"
#include "MOIdentityComponent.h"
#include "MOKnowledgeComponent.h"
#include "MOMetabolismComponent.h"
#include "MOPersistenceSubsystem.h"
#include "MOPersonalityComponent.h"
#include "MORespirationComponent.h"
#include "MOSkillsComponent.h"
#include "MOVitalsComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

namespace MOSaveStateTest
{
	/** The enum's value without its "EType::" prefix: no spaces, so a check can split the line on them. */
	template <typename TEnum>
	static FString EnumName(TEnum Value)
	{
		FString Name = UEnum::GetValueAsString(Value);
		int32 Colon = INDEX_NONE;
		return Name.FindLastChar(TEXT(':'), Colon) ? Name.Mid(Colon + 1) : Name;
	}

	static FString Vector(const FVector& V)
	{
		return FString::Printf(TEXT("%.1f,%.1f,%.1f"), V.X, V.Y, V.Z);
	}

	static void LogPerson(APawn& Pawn)
	{
		UMOIdentityComponent* Identity = Pawn.FindComponentByClass<UMOIdentityComponent>();
		const AMOCharacter* Character = Cast<AMOCharacter>(&Pawn);
		FString Line = FString::Printf(TEXT("pawn name=%s guid=%s player=%d dead=%d"), *Pawn.GetName(), Identity ? *Identity->GetOrCreateGuid().ToString(EGuidFormats::Digits) : TEXT("none"),
			Pawn.IsPlayerControlled() ? 1 : 0, Character && Character->IsDead() ? 1 : 0);

		if (const UMOKnowledgeComponent* Knowledge = Pawn.FindComponentByClass<UMOKnowledgeComponent>())
		{
			FMOKnowledgeSaveData Data;
			Knowledge->BuildSaveData(Data);
			TArray<FString> Items;
			for (const FMOItemKnowledgeProgress& Item : Data.ItemKnowledge)
			{
				Items.Add(FString::Printf(TEXT("%s:%d"), *Item.ItemDefinitionId.ToString(), Item.InspectionCount));
			}
			Line += FString::Printf(TEXT(" items=%s learned=%d"), Items.Num() ? *FString::Join(Items, TEXT(",")) : TEXT("none"), Data.AllLearnedKnowledge.Num());
		}
		if (const UMOPersonalityComponent* Personality = Pawn.FindComponentByClass<UMOPersonalityComponent>())
		{
			FMOPersonalityTraits Traits;
			Personality->BuildSaveData(Traits);
			Line += FString::Printf(TEXT(" traits=%.2f,%.2f,%.2f"), Traits.Conscientiousness, Traits.Sociability, Traits.Stability);
		}
		if (const UMOAnatomyComponent* Anatomy = Pawn.FindComponentByClass<UMOAnatomyComponent>())
		{
			FMOAnatomySaveData Data;
			Anatomy->BuildSaveData(Data);
			Line += FString::Printf(TEXT(" clock=%s:%.1f"), *EnumName(Data.DeathClockPart), Data.DeathClockRemaining);
		}
		if (const UMOVitalsComponent* Vitals = Pawn.FindComponentByClass<UMOVitalsComponent>())
		{
			FMOVitalsSaveData Data;
			Vitals->BuildSaveData(Data);
			Line += FString::Printf(TEXT(" wet=%.3f circ=%.1f resp=%.1f heat=%.2f fatal=%s"), Data.WetnessLevel, Data.CirculatoryFailureSeconds, Data.RespiratoryFailureSeconds,
				Data.HeatStrokeDoseMinutes, *EnumName(Data.FatalSystemicFailure));
		}
		if (const UMORespirationComponent* Respiration = Pawn.FindComponentByClass<UMORespirationComponent>())
		{
			FMORespirationSaveData Data;
			Respiration->BuildSaveData(Data);
			Line += FString::Printf(TEXT(" o2=%.3f asphyx=%.1f"), Data.OxygenReserve, Data.SecondsAsphyxiating);
		}
		if (const UMOCharacterHistoryComponent* History = Pawn.FindComponentByClass<UMOCharacterHistoryComponent>())
		{
			FMOCharacterHistorySaveData Data;
			History->BuildSaveData(Data);
			Line += FString::Printf(TEXT(" history=%d left=%s"), Data.Entries.Num(), Data.PlayerLeftAt == FDateTime::MinValue() ? TEXT("none") : *Data.PlayerLeftAt.ToIso8601());
		}
		if (const UMOCarriedLightComponent* Light = Pawn.FindComponentByClass<UMOCarriedLightComponent>())
		{
			FMOCarriedLightSaveData Data;
			Light->BuildSaveData(Data);
			Line += FString::Printf(TEXT(" lit=%d"), Data.bLit ? 1 : 0);
		}
		if (const UMOSkillsComponent* Skills = Pawn.FindComponentByClass<UMOSkillsComponent>())
		{
			FMOSkillsSaveData Data;
			Skills->BuildSaveData(Data);
			const auto Progress = [&Data](const TCHAR* SkillId)
			{
				const FName Id(SkillId);
				const FMOSkillProgress* Found = Data.Skills.FindByPredicate([&Id](const FMOSkillProgress& Skill) { return Skill.SkillId == Id; });
				return Found ? FString::Printf(TEXT("%d:%.1f"), Found->Level, Found->CurrentXP) : FString(TEXT("0:0.0"));
			};
			Line += FString::Printf(TEXT(" skills=%d clocks=%d melee=%s defense=%s"), Data.Skills.Num(), Data.LastUsedGameSeconds.Num(), *Progress(TEXT("Melee")), *Progress(TEXT("Defense")));
		}
		if (const UMOMetabolismComponent* Metabolism = Pawn.FindComponentByClass<UMOMetabolismComponent>())
		{
			FMOMetabolismSaveData Data;
			Metabolism->BuildSaveData(Data);
			float VitaminC = 0.0f;
			for (const auto& Food : Data.DigestingFood)
			{
				VitaminC += Food.RemainingVitaminC;
			}
			Line += FString::Printf(TEXT(" digesting=%d vitaminc=%.2f"), Data.DigestingFood.Num(), VitaminC);
		}
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] SAVESTATE %s"), *Line);
	}

	static void LogCarcass(const AMOCarcassActor& Carcass)
	{
		FMOCarcassSaveRecord Record;
		Carcass.BuildSaveRecord(Record);
		TArray<FString> Cut;
		for (const FName& Part : Record.HarvestedParts)
		{
			Cut.Add(Part.ToString());
		}
		FString Mesh = TEXT("none");
		int32 Bones = 0;
		double Pose = 0.0;
		FVector Root = FVector::ZeroVector;
		if (const UPoseableMeshComponent* Poseable = Carcass.FindComponentByClass<UPoseableMeshComponent>(); Poseable && Poseable->GetSkinnedAsset())
		{
			Mesh = Poseable->GetSkinnedAsset()->GetName();
			Bones = Poseable->GetNumBones();
			for (int32 Bone = 0; Bone < Bones; ++Bone)
			{
				// Each bone's reach from the component: the same pose gives the same sum wherever the actor lies, and a body back in its reference pose does not.
				Pose += const_cast<UPoseableMeshComponent*>(Poseable)->GetBoneTransformByName(Poseable->GetBoneName(Bone), EBoneSpaces::ComponentSpace).GetLocation().Size();
			}
			if (Bones > 0)
			{
				Root = const_cast<UPoseableMeshComponent*>(Poseable)->GetBoneTransformByName(Poseable->GetBoneName(0), EBoneSpaces::WorldSpace).GetLocation();
			}
		}
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] SAVESTATE carcass name=%s row=%s at=%s decay=%s since=%.1f cut=%s mesh=%s bones=%d saved=%d pose=%.1f root=%s"), *Carcass.GetName(),
			*Record.RowName.ToString(), *Vector(Record.Transform.GetLocation()), *EnumName(Record.DecayStage), Record.TimeSinceDeath, Cut.Num() ? *FString::Join(Cut, TEXT("+")) : TEXT("none"), *Mesh,
			Bones, Record.Body.BoneTransforms.Num(), Pose, *Vector(Root));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GMOTestSaveState(
	TEXT("MO.Test.SaveState"),
	TEXT("Server: log one line for each person and carcass in the world, of what a save would write for them (Tools/check_save_roundtrip.py). Usage: MO.Test.SaveState"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		using namespace MOSaveStateTest;
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] SAVESTATE end FAILED: no world, or not the server"));
			return;
		}
		int32 People = 0;
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			if (!Cast<AMOCreature>(*It) && It->FindComponentByClass<UMOSkillsComponent>())
			{
				LogPerson(**It);
				++People;
			}
		}
		int32 Carcasses = 0;
		for (TActorIterator<AMOCarcassActor> It(World); It; ++It)
		{
			LogCarcass(**It);
			++Carcasses;
		}
		const AMOGameState* State = World->GetGameState<AMOGameState>();
		const UGameInstance* Game = World->GetGameInstance();
		const UMOPersistenceSubsystem* Persistence = Game ? Game->GetSubsystem<UMOPersistenceSubsystem>() : nullptr;
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] SAVESTATE end seed=%d people=%d carcasses=%d world=\"%s\""), State ? State->GetWorldSeedInfo().Seed : 0, People, Carcasses,
			Persistence ? *Persistence->GetWorldDisplayName() : TEXT(""));
	}));

#endif
