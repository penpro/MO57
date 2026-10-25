#include "MOWorldFeatureCook.h"

#include "MOFramework.h"
#include "MOWorldFeatureSettings.h"
#include "MOWorldFeatureTypes.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/World.h"
#endif

namespace MOWorldFeatureCookPrivate
{
	bool IsObjectPathChar(TCHAR C)
	{
		return FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('/') || C == TEXT('.') || C == TEXT('-');
	}

	/** Every long package name inside a property's text: Variants = ("/Game/A/B.B", "/Game/C/D.D"), Class'/Game/X/BP_Y.BP_Y_C', a bare path. */
	void AddPackagesInText(const FString& Text, TSet<FName>& Out)
	{
		for (int32 I = 0; I < Text.Len(); ++I)
		{
			if (Text[I] != TEXT('/') || (I > 0 && IsObjectPathChar(Text[I - 1])))
			{
				continue;
			}
			int32 End = I;
			while (End < Text.Len() && IsObjectPathChar(Text[End]))
			{
				++End;
			}
			FString Package = Text.Mid(I, End - I);
			int32 Dot = INDEX_NONE;
			if (Package.FindChar(TEXT('.'), Dot))
			{
				Package.LeftInline(Dot);
			}
			if (FPackageName::IsValidLongPackageName(Package))
			{
				Out.Add(FName(*Package));
			}
			I = End;
		}
	}
}

void FMOWorldFeatureCook::CollectNamedPackages(const UDataTable& Table, TArray<FName>& OutPackages)
{
	TSet<FName> Packages;
	if (Table.GetRowStruct() == FMOWorldFeatureDefinitionRow::StaticStruct())
	{
		for (const TPair<FName, uint8*>& Pair : Table.GetRowMap())
		{
			const FMOWorldFeatureDefinitionRow& Row = *reinterpret_cast<const FMOWorldFeatureDefinitionRow*>(Pair.Value);
			const FString ClassPackage = Row.ActorClass.ToSoftObjectPath().GetLongPackageName();
			if (!ClassPackage.IsEmpty() && !FPackageName::IsScriptPackage(ClassPackage))   // a native class (AMOWorldFeatureActor) is code, not content
			{
				Packages.Add(FName(*ClassPackage));
			}
			for (const TPair<FName, FString>& Property : Row.ActorProperties)
			{
				MOWorldFeatureCookPrivate::AddPackagesInText(Property.Value, Packages);
			}
		}
	}
	OutPackages = Packages.Array();
	OutPackages.Sort(FNameLexicalLess());
}

#if WITH_EDITOR
void FMOWorldFeatureCook::CollectCookPackages(TArray<FName>& OutPackages)
{
	OutPackages.Reset();
	const UMOWorldFeatureSettings* Settings = GetDefault<UMOWorldFeatureSettings>();
	const UDataTable* Table = Settings ? Settings->FeatureTable.LoadSynchronous() : nullptr;
	IAssetRegistry* Registry = IAssetRegistry::Get();
	if (!Table || !Registry)
	{
		return;
	}
	if (Registry->IsLoadingAssets())
	{
		Registry->WaitForCompletion();
	}

	TArray<FName> Stack;
	CollectNamedPackages(*Table, Stack);
	const FTopLevelAssetPath WorldClass = UWorld::StaticClass()->GetClassPathName();
	TSet<FName> Seen;
	TSet<FName> Cook;
	while (!Stack.IsEmpty())
	{
		const FName Package = Stack.Pop(EAllowShrinking::No);
		if (Seen.Contains(Package))
		{
			continue;
		}
		Seen.Add(Package);
		const FString Name = Package.ToString();
		if (FPackageName::IsScriptPackage(Name) || Name.Contains(TEXT("/__ExternalActors__/")) || Name.Contains(TEXT("/__ExternalObjects__/")))
		{
			continue;
		}
		TArray<FAssetData> Assets;
		Registry->GetAssetsByPackageName(Package, Assets, /*bIncludeOnlyOnDiskAssets=*/true);
		if (Assets.IsEmpty() || Assets.ContainsByPredicate([&WorldClass](const FAssetData& Asset) { return Asset.AssetClassPath == WorldClass; }))
		{
			continue;   // not a package on disk (a stale name), or a level: an assembly's source level is editor data, never placed by a feature
		}
		Cook.Add(Package);
		TArray<FName> Dependencies;
		Registry->GetDependencies(Package, Dependencies, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Game);
		for (const FName Dependency : Dependencies)
		{
			if (!Seen.Contains(Dependency))
			{
				Stack.Add(Dependency);
			}
		}
	}
	OutPackages = Cook.Array();
	OutPackages.Sort(FNameLexicalLess());
}

void FMOWorldFeatureCook::ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& InOutPackagesToCook, TArray<FName>& InOutPackagesToNeverCook)
{
	TArray<FName> Packages;
	CollectCookPackages(Packages);
	const int32 Before = InOutPackagesToCook.Num();
	for (const FName Package : Packages)
	{
		InOutPackagesToCook.AddUnique(Package);
	}
	UE_LOG(LogMOFramework, Display, TEXT("[MOWorldFeatures] cook: %d package(s) the world-feature rows reach, %d new to the cook list"), Packages.Num(), InOutPackagesToCook.Num() - Before);
}
#endif
