/**
 * The survivor bases and the body shapes (Wes, Oct 27 2026: "i made a few bases with the new metahumans stuff, see if you can get them all
 * customizable and out into the game"): UMOSurvivorBaseSettings' bases (Config/DefaultGame.ini), the look that carries a base and its build,
 * and the starting body a base gives the metabolism.
 * Run: python Tools/ue.py auto --filter "MOFramework.Appearance.SurvivorBases" (and MOFramework.Metabolism.StartingBodyHolds)
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "GroomAsset.h"
#include "GroomComponent.h"
#include "Materials/MaterialInterface.h"
#include "MOAppearanceSubsystem.h"
#include "MOCharacterAppearance.h"
#include "MOMetabolismTypes.h"
#include "MOSurvivorBase.h"
#include "Misc/PackageName.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MOSurvivorBaseTestsPrivate
{
	/** A Blueprint's groom component as the Blueprint makes it (its construction script's template), nearest class first. */
	const UGroomComponent* FindBlueprintGroomTemplate(UClass* Class, FName Name)
	{
		for (const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Class); Generated; Generated = Cast<UBlueprintGeneratedClass>(Generated->GetSuperClass()))
		{
			for (const USCS_Node* Node : Generated->SimpleConstructionScript ? Generated->SimpleConstructionScript->GetAllNodes() : TArray<USCS_Node*>())
			{
				if (Node && Node->GetVariableName() == Name)
				{
					return Cast<UGroomComponent>(Node->ComponentTemplate);
				}
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAppearance_SurvivorBases,
	"MOFramework.Appearance.SurvivorBases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAppearance_SurvivorBases::RunTest(const FString& Parameters)
{
	const TArray<FMOSurvivorBase>& Bases = UMOSurvivorBaseSettings::GetBases();
	TestTrue(TEXT("the femme survivor and at least one Creator base are listed"), Bases.Num() >= 2);
	TSet<FName> Ids;
	int32 Shaped = 0;
	for (const FMOSurvivorBase& Base : Bases)
	{
		TestFalse(FString::Printf(TEXT("%s: an id"), *Base.Id.ToString()), Base.Id.IsNone());
		TestFalse(FString::Printf(TEXT("%s: one entry per id"), *Base.Id.ToString()), Ids.Contains(Base.Id));
		Ids.Add(Base.Id);
		TestFalse(FString::Printf(TEXT("%s: a name for the picker"), *Base.Id.ToString()), Base.DisplayName.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s: a body fat the metabolism allows (3..50 %%)"), *Base.Id.ToString()), Base.BodyFatPercent >= 3.0f && Base.BodyFatPercent <= 50.0f);
		TestTrue(FString::Printf(TEXT("%s: a muscle mass the metabolism allows (15..50 kg)"), *Base.Id.ToString()), Base.MuscleMassKg >= 15.0f && Base.MuscleMassKg <= 50.0f);
		const USkeletalMesh* Body = Cast<USkeletalMesh>(Base.Body.TryLoad());
		const USkeletalMesh* Face = Cast<USkeletalMesh>(Base.Face.TryLoad());
		TestNotNull(FString::Printf(TEXT("%s: its body mesh loads (%s)"), *Base.Id.ToString(), *Base.Body.ToString()), Body);
		TestNotNull(FString::Printf(TEXT("%s: its face mesh loads (%s)"), *Base.Id.ToString(), *Base.Face.ToString()), Face);
		for (const FString& Groom : { Base.Hair, Base.Eyebrows, Base.Eyelashes })
		{
			if (!Groom.IsEmpty() && Groom != FMOCharacterAppearance::NoGroom)
			{
				TestTrue(FString::Printf(TEXT("%s: groom %s exists"), *Base.Id.ToString(), *Groom), FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Groom)));
			}
		}
		// A Creator base carries the shapes on its body and its face (the neck seam moves with the body); the femme survivor's own meshes do not.
		if (Body && Face && Body->FindMorphTarget(TEXT("MO_Body_Heavy")))
		{
			++Shaped;
			TestNotNull(FString::Printf(TEXT("%s: the face's neck follows the heavy body"), *Base.Id.ToString()), Face->FindMorphTarget(TEXT("MO_Body_Heavy")));
			TestNotNull(FString::Printf(TEXT("%s: a thin body"), *Base.Id.ToString()), Body->FindMorphTarget(TEXT("MO_Body_Thin")));
			TestNotNull(FString::Printf(TEXT("%s: a muscular body"), *Base.Id.ToString()), Body->FindMorphTarget(TEXT("MO_Body_Muscular")));
		}
	}
	TestTrue(TEXT("at least one base carries the body shapes"), Shaped >= 1);
	TestNull(TEXT("None is no base (the pawn's own meshes)"), UMOSurvivorBaseSettings::Find(NAME_None));
	TestNull(TEXT("an id no longer listed is no base"), UMOSurvivorBaseSettings::Find(TEXT("NoSuchBase")));

	// The look carries the base and the build; a base puts its id and grooms on a look.
	FMOCharacterAppearance Chosen;
	Chosen.BaseId = TEXT("SomeBase");
	Chosen.SetBuildValue(TEXT("Bust"), 0.5f);
	Chosen.SetBuildValue(TEXT("Bust"), -0.25f);
	Chosen.SetBuildValue(TEXT("Hips"), 1.0f);
	FMOCharacterAppearance Pawn;
	Pawn.CopyLookFrom(Chosen);
	TestEqual(TEXT("the base is part of the look"), Pawn.BaseId, FName(TEXT("SomeBase")));
	TestEqual(TEXT("a build slider set twice keeps the last value"), Pawn.GetBuildValue(TEXT("Bust")), -0.25f);
	TestEqual(TEXT("each slider its own"), Pawn.GetBuildValue(TEXT("Hips")), 1.0f);
	TestEqual(TEXT("an unset slider is the base as made"), Pawn.GetBuildValue(TEXT("Waist")), 0.0f);
	TestEqual(TEXT("one entry per slider"), Pawn.BodyBuild.Num(), 2);
	if (Bases.Num() > 0)
	{
		FMOCharacterAppearance Look;
		Look.HairAssetPath = TEXT("/Game/Some/Other.Hair");
		Bases.Last().ApplyToLook(Look);
		TestEqual(TEXT("a base names itself on the look"), Look.BaseId, Bases.Last().Id);
		TestEqual(TEXT("and puts its own hair on"), Look.HairAssetPath, Bases.Last().Hair);
	}
	// The body's sex (Wes, Oct 28: "add it"): every base says it, and the words follow it (they when it is not known).
	for (const FMOSurvivorBase& Base : Bases)
	{
		TestTrue(FString::Printf(TEXT("%s: the base names its body's sex"), *Base.Id.ToString()), Base.Sex != EMOBodySex::Unspecified);
	}
	TestEqual(TEXT("she"), FMOPronouns::Subject(EMOBodySex::Female).ToString(), FString(TEXT("she")));
	TestEqual(TEXT("his"), FMOPronouns::Possessive(EMOBodySex::Male).ToString(), FString(TEXT("his")));
	TestEqual(TEXT("them when unknown"), FMOPronouns::Object(EMOBodySex::Unspecified).ToString(), FString(TEXT("them")));
	TestEqual(TEXT("no sex name when unknown"), FMOPronouns::SexName(EMOBodySex::Unspecified).ToString(), FString());

	// Facial hair: a base names its own, and one without takes the last base's off (a bearded base, then a woman: clean-shaven).
	{
		FMOSurvivorBase Bearded;
		Bearded.Id = TEXT("Bearded");
		Bearded.Beard = TEXT("/Game/Some/Beard.Beard");
		FMOSurvivorBase Shaven;
		Shaven.Id = TEXT("Shaven");
		FMOCharacterAppearance Look;
		Bearded.ApplyToLook(Look);
		TestEqual(TEXT("a bearded base puts its beard on"), Look.BeardAssetPath, Bearded.Beard);
		TestEqual(TEXT("and no mustache it does not have"), Look.MustacheAssetPath, FMOCharacterAppearance::NoGroom);
		Shaven.ApplyToLook(Look);
		TestEqual(TEXT("a base without a beard takes it off"), Look.BeardAssetPath, FMOCharacterAppearance::NoGroom);
	}

	// The survivor Blueprint keeps its hair on the strands at every distance (LOD bias -7 on each group); a base's own groom draws nothing on
	// its cards LODs, so a swap carries that onto every group of the new groom, whatever its group count (bald in third person otherwise, Oct 27).
	const UGroomComponent* HairTemplate = MOSurvivorBaseTestsPrivate::FindBlueprintGroomTemplate(
		LoadClass<AActor>(nullptr, TEXT("/MOFramework/Characters/BP_MOMetaHuman_Female.BP_MOMetaHuman_Female_C")), TEXT("Hair"));
	TestTrue(TEXT("the survivor Blueprint's hair is on the strands at every distance (LOD bias -7 on each group)"), HairTemplate
		&& HairTemplate->GroomGroupsDesc.Num() > 0 && !HairTemplate->GroomGroupsDesc.ContainsByPredicate([](const FHairGroupDesc& D) { return D.LODBias > -7.0f; }));
	// Its own material instances go back on too (the Blueprint's overrides, which the swap clears); a library groom has none and keeps the
	// shared ones.
	for (const FMOSurvivorBase& Base : Bases)
	{
		UGroomAsset* Own = Base.Hair.IsEmpty() ? nullptr : Cast<UGroomAsset>(FSoftObjectPath(Base.Hair).TryLoad());
		if (!Own)
		{
			continue;
		}
		UGroomComponent* Component = NewObject<UGroomComponent>(GetTransientPackage());
		if (HairTemplate && HairTemplate->GroomGroupsDesc.Num() > 0)
		{
			TestTrue(FString::Printf(TEXT("%s: a swap keeps the Blueprint's group settings"), *Base.Id.ToString()),
				UMOAppearanceSubsystem::KeepGroupSettings(Component, HairTemplate, Own));
			TestEqual(FString::Printf(TEXT("%s: one per group of its own hair"), *Base.Id.ToString()), Component->GroomGroupsDesc.Num(), Own->GetNumHairGroups());
			TestFalse(FString::Printf(TEXT("%s: each on the strands at every distance"), *Base.Id.ToString()),
				Component->GroomGroupsDesc.ContainsByPredicate([&](const FHairGroupDesc& D) { return D.LODBias != HairTemplate->GroomGroupsDesc[0].LODBias; }));
		}
		const int32 Taken = UMOAppearanceSubsystem::ApplyOwnGroomMaterials(Component, Own);
		// One per slot it has its own instance for: strands, and cards / helmet where the groom has them (Zuri's coil buzz cut is strands only).
		TestTrue(FString::Printf(TEXT("%s: its own hair takes its own material instances (%d)"), *Base.Id.ToString(), Taken), Taken >= 1);
		const int32 Cards = Own->GetHairGroupsMaterials().IndexOfByPredicate([](const FHairGroupsMaterial& M) { return M.SlotName == TEXT("MI_Hair_Cards"); });
		if (Cards != INDEX_NONE)
		{
			const UMaterialInterface* Material = Component->GetMaterial(Cards);
			TestTrue(FString::Printf(TEXT("%s: the cards slot wears MI_WI_%s_Hair_Cards (%s)"), *Base.Id.ToString(), *Own->GetName(), *GetNameSafe(Material)),
				Material && Material->GetName() == FString::Printf(TEXT("MI_WI_%s_Hair_Cards"), *Own->GetName()));
		}
	}
	if (UGroomAsset* Library = Cast<UGroomAsset>(FSoftObjectPath(TEXT("/Game/MetaHumans/Common/Optional/Grooms/GroomAssets/Hair/Hair_S_Updo/Hair_S_Updo.Hair_S_Updo")).TryLoad()))
	{
		TestEqual(TEXT("a library groom keeps the shared materials"), UMOAppearanceSubsystem::ApplyOwnGroomMaterials(NewObject<UGroomComponent>(GetTransientPackage()), Library), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOMetabolism_StartingBodyHolds,
	"MOFramework.Metabolism.StartingBodyHolds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOMetabolism_StartingBodyHolds::RunTest(const FString& Parameters)
{
	// UMOMetabolismComponent::UpdateBodyWeight: weight = fat mass (weight x fat %) + muscle + bone + the rest; fat % = fat mass / that weight.
	auto Update = [](FMOBodyComposition& Body)
	{
		const float Fat = Body.GetFatMass();
		Body.TotalWeight = Fat + Body.MuscleMass + Body.BoneMass + FMOBodyComposition::OtherMassKg;
		Body.BodyFatPercent = Fat / Body.TotalWeight * 100.0f;
	};
	FMOBodyComposition Default;
	Update(Default);
	TestTrue(TEXT("the defaults do not hold: 75 kg at 18 % becomes ~62 kg at ~21.8 % (why a base sets its body with SetBody)"),
		FMath::IsNearlyEqual(Default.BodyFatPercent, 21.77f, 0.05f));
	for (const FVector2f& FatMuscle : { FVector2f(12.4f, 28.5f), FVector2f(8.0f, 20.0f), FVector2f(40.0f, 40.0f), FVector2f(18.0f, 30.0f) })
	{
		FMOBodyComposition Body;
		Body.SetBody(FatMuscle.X, FatMuscle.Y);
		const float Weight = Body.TotalWeight;
		Update(Body);
		TestTrue(FString::Printf(TEXT("%.1f %% / %.1f kg holds its fat %% through a weight update"), FatMuscle.X, FatMuscle.Y), FMath::IsNearlyEqual(Body.BodyFatPercent, FatMuscle.X, 0.01f));
		TestTrue(FString::Printf(TEXT("%.1f %% / %.1f kg holds its weight (%.1f kg)"), FatMuscle.X, FatMuscle.Y, Weight), FMath::IsNearlyEqual(Body.TotalWeight, Weight, 0.01f));
	}
	FMOBodyComposition Lillian;
	Lillian.SetBody(12.4f, 28.5f);
	TestTrue(TEXT("Lillian's base body: ~53.7 kg (BMI 18.3 at 171 cm)"), FMath::IsNearlyEqual(Lillian.TotalWeight, 53.65f, 0.1f));
	return true;
}

#endif
