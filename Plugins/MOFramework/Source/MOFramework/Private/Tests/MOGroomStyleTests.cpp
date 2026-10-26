/**
 * The character creator's hair / beard / mustache styles (Wes, Oct 25 2026: "grab all the other hair styles from the metahumans pipeline and prepare
 * them for use in the character customization screen"): the MetaHuman groom library under /Game/MetaHumans/Common/Optional/Grooms/GroomAssets
 * (Tools/import_metahuman_grooms.py), one binding per groom on the survivor's face (Content/Python/make_groom_bindings.py).
 * Run: python Tools/ue.py auto --filter "MOFramework.Appearance"
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/GameInstance.h"
#include "GroomBindingAsset.h"
#include "MOAppearanceSubsystem.h"
#include "MOCharacterAppearance.h"
#include "Misc/PackageName.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAppearance_GroomNames,
	"MOFramework.Appearance.GroomNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAppearance_GroomNames::RunTest(const FString& Parameters)
{
	auto Name = [](const TCHAR* Asset) { return UMOAppearanceSubsystem::MakeGroomDisplayName(Asset).ToString(); };
	TestEqual(TEXT("hair: style + length"), Name(TEXT("Hair_M_SideSweptFringe")), FString(TEXT("Side swept fringe (medium)")));
	TestEqual(TEXT("numbers stand apart"), Name(TEXT("Hair_S_360Waves")), FString(TEXT("360 waves (short)")));
	TestEqual(TEXT("beards say so"), Name(TEXT("Beard_L_Full")), FString(TEXT("Full beard (long)")));
	TestEqual(TEXT("goatees say so"), Name(TEXT("Goatee_S_ChinStrap")), FString(TEXT("Chin strap goatee (short)")));
	TestEqual(TEXT("mustaches say so"), Name(TEXT("Mustache_L_Handlebar")), FString(TEXT("Handlebar mustache (long)")));
	TestEqual(TEXT("any other name: its words"), Name(TEXT("MyCustomGroom")), FString(TEXT("My custom groom")));

	// The creator's look over the pawn's own meshes: colours and grooms, never the body.
	FMOCharacterAppearance Pawn;
	Pawn.BodyAssetPath = TEXT("/Game/MetaHumans/MH_F_1/Body/SK_Body");
	FMOCharacterAppearance Chosen;
	Chosen.HairMelanin = 0.3f;
	Chosen.HairAssetPath = TEXT("/Game/MetaHumans/Common/Optional/Grooms/GroomAssets/Hair/Hair_S_Pixie/Hair_S_Pixie.Hair_S_Pixie");
	Chosen.BeardAssetPath = FMOCharacterAppearance::NoGroom;
	Pawn.CopyLookFrom(Chosen);
	TestEqual(TEXT("hair colour copied"), Pawn.HairMelanin, 0.3f);
	TestEqual(TEXT("hair style copied"), Pawn.HairAssetPath, Chosen.HairAssetPath);
	TestEqual(TEXT("no beard copied"), Pawn.BeardAssetPath, FMOCharacterAppearance::NoGroom);
	TestTrue(TEXT("an untouched slot stays empty (the Blueprint's own)"), Pawn.MustacheAssetPath.IsEmpty());
	TestEqual(TEXT("the pawn keeps its own body mesh"), Pawn.BodyAssetPath, FString(TEXT("/Game/MetaHumans/MH_F_1/Body/SK_Body")));
	TestEqual(TEXT("slot paths by category"), UMOAppearanceSubsystem::GetGroomPath(Pawn, EMOAppearanceCategory::Hair), &Pawn.HairAssetPath);
	TestNull(TEXT("no groom path for the body"), UMOAppearanceSubsystem::GetGroomPath(Pawn, EMOAppearanceCategory::Body));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAppearance_GroomLibrary,
	"MOFramework.Appearance.GroomLibrary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAppearance_GroomLibrary::RunTest(const FString& Parameters)
{
	// The catalog the creator steps through: the whole MetaHuman library, short styles first.
	UMOAppearanceSubsystem* Appearance = NewObject<UMOAppearanceSubsystem>(NewObject<UGameInstance>());   // ClassWithin: a game instance
	Appearance->DiscoverAppearanceAssets();
	const TArray<FMOAppearanceAssetEntry> Hair = Appearance->GetAssetsForCategory(EMOAppearanceCategory::Hair);
	const TArray<FMOAppearanceAssetEntry> Beards = Appearance->GetAssetsForCategory(EMOAppearanceCategory::Beard);
	const TArray<FMOAppearanceAssetEntry> Mustaches = Appearance->GetAssetsForCategory(EMOAppearanceCategory::Mustache);
	TestTrue(FString::Printf(TEXT("38 hair styles (%d)"), Hair.Num()), Hair.Num() >= 38);
	TestTrue(FString::Printf(TEXT("16 beards and goatees (%d)"), Beards.Num()), Beards.Num() >= 16);
	TestTrue(FString::Printf(TEXT("16 mustaches (%d)"), Mustaches.Num()), Mustaches.Num() >= 16);
	if (Hair.Num() > 0)
	{
		TestTrue(TEXT("short styles first"), Hair[0].Tags.Contains(TEXT("Short")));
		TestTrue(TEXT("long styles last"), Hair.Last().Tags.Contains(TEXT("Long")));
	}

	// Every style has a binding to the survivor's face (registry tags only: loading 70 grooms is 2.5 GB). A missing one would still show,
	// projected at run time, but slowly and unfitted -- rerun Content/Python/make_groom_bindings.py.
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Bindings;
	Registry.GetAssetsByClass(UGroomBindingAsset::StaticClass()->GetClassPathName(), Bindings);
	TSet<FSoftObjectPath> Bound;
	for (const FAssetData& Binding : Bindings)
	{
		FString Tag;
		if (Binding.PackagePath == FName(TEXT("/Game/MetaHumans/MH_F_1/Grooms/Bindings")) && Binding.GetTagValue(TEXT("Groom"), Tag))
		{
			Bound.Add(FSoftObjectPath(FPackageName::ExportTextPathToObjectPath(Tag)));
		}
	}
	TArray<FString> Unbound;
	for (const TArray<FMOAppearanceAssetEntry>* Slot : { &Hair, &Beards, &Mustaches })
	{
		for (const FMOAppearanceAssetEntry& Entry : *Slot)
		{
			if (!Bound.Contains(FSoftObjectPath(Entry.AssetPath)))
			{
				Unbound.Add(Entry.AssetPath);
			}
		}
	}
	TestEqual(FString::Printf(TEXT("every style has a binding to MH_F_1's face (missing: %s)"), *FString::Join(Unbound, TEXT(", "))), Unbound.Num(), 0);
	return true;
}

#endif
