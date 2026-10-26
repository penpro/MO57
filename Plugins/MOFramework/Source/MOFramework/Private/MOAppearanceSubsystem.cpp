#include "MOAppearanceSubsystem.h"
#include "MOFramework.h"
#include "MOMetaHumanCharacter.h"
#include "Engine/SkeletalMesh.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Algo/Find.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

void UMOAppearanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogMOFramework, Log, TEXT("UMOAppearanceSubsystem: Initialized"));
}

void UMOAppearanceSubsystem::Deinitialize()
{
	AssetCatalog.Empty();
	KnownAssetPaths.Empty();

	Super::Deinitialize();
}

// ============================================================================
// ASSET DISCOVERY
// ============================================================================

void UMOAppearanceSubsystem::DiscoverAppearanceAssets()
{
	UE_LOG(LogMOFramework, Log, TEXT("UMOAppearanceSubsystem: Beginning asset discovery..."));

	AssetCatalog.Empty();
	KnownAssetPaths.Empty();

	// Initialize empty arrays for each category
	AssetCatalog.Add(EMOAppearanceCategory::Face, TArray<FMOAppearanceAssetEntry>());
	AssetCatalog.Add(EMOAppearanceCategory::Hair, TArray<FMOAppearanceAssetEntry>());
	AssetCatalog.Add(EMOAppearanceCategory::Eyebrows, TArray<FMOAppearanceAssetEntry>());
	AssetCatalog.Add(EMOAppearanceCategory::Eyelashes, TArray<FMOAppearanceAssetEntry>());
	AssetCatalog.Add(EMOAppearanceCategory::Beard, TArray<FMOAppearanceAssetEntry>());
	AssetCatalog.Add(EMOAppearanceCategory::Mustache, TArray<FMOAppearanceAssetEntry>());
	AssetCatalog.Add(EMOAppearanceCategory::Body, TArray<FMOAppearanceAssetEntry>());

	// Scan base content paths
	ScanDirectoryForSkeletalMeshes(FacesContentPath, EMOAppearanceCategory::Face, false);
	ScanDirectoryForSkeletalMeshes(BodiesContentPath, EMOAppearanceCategory::Body, false);

	// Scan hair paths for grooms
	ScanDirectoryForGrooms(HairContentPath + TEXT("/Hair"), EMOAppearanceCategory::Hair, false);
	ScanDirectoryForGrooms(HairContentPath + TEXT("/Eyebrows"), EMOAppearanceCategory::Eyebrows, false);
	ScanDirectoryForGrooms(HairContentPath + TEXT("/Eyelashes"), EMOAppearanceCategory::Eyelashes, false);
	ScanDirectoryForGrooms(HairContentPath + TEXT("/Beards"), EMOAppearanceCategory::Beard, false);
	ScanDirectoryForGrooms(HairContentPath + TEXT("/Mustaches"), EMOAppearanceCategory::Mustache, false);

	// Scan mod paths
	for (const FString& ModPath : AdditionalModPaths)
	{
		ScanDirectoryForSkeletalMeshes(ModPath + TEXT("/Faces"), EMOAppearanceCategory::Face, true);
		ScanDirectoryForSkeletalMeshes(ModPath + TEXT("/Bodies"), EMOAppearanceCategory::Body, true);
		ScanDirectoryForGrooms(ModPath + TEXT("/Hair"), EMOAppearanceCategory::Hair, true);
		ScanDirectoryForGrooms(ModPath + TEXT("/Eyebrows"), EMOAppearanceCategory::Eyebrows, true);
		ScanDirectoryForGrooms(ModPath + TEXT("/Eyelashes"), EMOAppearanceCategory::Eyelashes, true);
		ScanDirectoryForGrooms(ModPath + TEXT("/Beards"), EMOAppearanceCategory::Beard, true);
		ScanDirectoryForGrooms(ModPath + TEXT("/Mustaches"), EMOAppearanceCategory::Mustache, true);
	}

	bDiscoveryComplete = true;

	// Log discovery results
	for (const auto& Pair : AssetCatalog)
	{
		UE_LOG(LogMOFramework, Log, TEXT("UMOAppearanceSubsystem: Discovered %d assets for category %d"),
			Pair.Value.Num(), static_cast<int32>(Pair.Key));
	}
}

void UMOAppearanceSubsystem::ScanDirectoryForSkeletalMeshes(const FString& Path, EMOAppearanceCategory Category, bool bIsModded)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*Path), AssetDataList, true);

	TArray<FMOAppearanceAssetEntry>& CategoryAssets = AssetCatalog.FindOrAdd(Category);

	for (const FAssetData& AssetData : AssetDataList)
	{
		// Filter to skeletal meshes only
		if (AssetData.AssetClassPath.GetAssetName() != TEXT("SkeletalMesh"))
		{
			continue;
		}

		FMOAppearanceAssetEntry Entry;
		Entry.AssetPath = AssetData.GetObjectPathString();
		Entry.DisplayName = GetDisplayNameFromPath(AssetData.AssetName.ToString());
		Entry.Tags = InferTagsFromAssetName(AssetData.AssetName.ToString());
		Entry.bIsModded = bIsModded;

		CategoryAssets.Add(Entry);
		KnownAssetPaths.Add(Entry.AssetPath);

		// DEBUG (MOAppearanceSubsystem, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("  Found skeletal mesh: %s"), *Entry.AssetPath);
	}
}

void UMOAppearanceSubsystem::ScanDirectoryForGrooms(const FString& Path, EMOAppearanceCategory Category, bool bIsModded)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*Path), AssetDataList, true);

	TArray<FMOAppearanceAssetEntry>& CategoryAssets = AssetCatalog.FindOrAdd(Category);

	for (const FAssetData& AssetData : AssetDataList)
	{
		// Filter to groom assets only
		if (AssetData.AssetClassPath.GetAssetName() != TEXT("GroomAsset"))
		{
			continue;
		}

		const FString Name = AssetData.AssetName.ToString();
		FMOAppearanceAssetEntry Entry;
		Entry.AssetPath = AssetData.GetObjectPathString();
		Entry.DisplayName = MakeGroomDisplayName(Name);
		Entry.Tags = InferTagsFromAssetName(Name);
		Entry.bIsModded = bIsModded;
		// The MetaHuman library's previews sit beside GroomAssets: Thumbnails/<Category>/<Groom>_Thumb (editor content, not cooked).
		const FString Thumbnail = FString::Printf(TEXT("%s/Thumbnails/%s/%s_Thumb"), *FPaths::GetPath(HairContentPath), *FPaths::GetCleanFilename(Path), *Name);
		if (AssetRegistry.GetAssetPackageDataCopy(FName(*Thumbnail)).IsSet())
		{
			Entry.ThumbnailPath = Thumbnail + TEXT(".") + Name + TEXT("_Thumb");
		}

		CategoryAssets.Add(Entry);
		KnownAssetPaths.Add(Entry.AssetPath);

		// DEBUG (MOAppearanceSubsystem, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("  Found groom asset: %s"), *Entry.AssetPath);
	}
	// Short styles first, then medium, then long; by name within a length (the creator steps through them in this order).
	auto LengthRank = [](const FMOAppearanceAssetEntry& E) { return E.Tags.Contains(TEXT("Short")) ? 0 : E.Tags.Contains(TEXT("Medium")) ? 1 : E.Tags.Contains(TEXT("Long")) ? 2 : 3; };
	CategoryAssets.StableSort([&LengthRank](const FMOAppearanceAssetEntry& A, const FMOAppearanceAssetEntry& B)
	{
		const int32 RankA = LengthRank(A), RankB = LengthRank(B);
		return RankA != RankB ? RankA < RankB : A.DisplayName.ToString() < B.DisplayName.ToString();
	});
}

FText UMOAppearanceSubsystem::MakeGroomDisplayName(const FString& AssetName)
{
	// CamelCase -> lower-case words, numbers apart ("SideSweptFringe" -> side swept fringe, "360Waves" -> 360 waves).
	auto Words = [](const FString& Camel)
	{
		FString Out;
		for (int32 I = 0; I < Camel.Len(); ++I)
		{
			const TCHAR C = Camel[I];
			if (I > 0 && (FChar::IsUpper(C) || FChar::IsDigit(C) != FChar::IsDigit(Camel[I - 1])))
			{
				Out.AppendChar(TEXT(' '));
			}
			Out.AppendChar(FChar::ToLower(C));
		}
		return Out;
	};
	// MetaHuman grooms: <Kind>_<S|M|L>_<Style>.
	TArray<FString> Parts;
	AssetName.ParseIntoArray(Parts, TEXT("_"));
	static const TMap<FString, FString> Lengths = { {TEXT("S"), TEXT("short")}, {TEXT("M"), TEXT("medium")}, {TEXT("L"), TEXT("long")} };
	const FString* Length = Parts.Num() == 3 ? Lengths.Find(Parts[1]) : nullptr;
	if (!Length)
	{
		FString Plain = Words(AssetName.Replace(TEXT("_"), TEXT("")));
		return FText::FromString(Plain.Left(1).ToUpper() + Plain.Mid(1));
	}
	FString Style = Words(Parts[2]);
	if (Parts[0] != TEXT("Hair"))
	{
		Style += TEXT(" ") + Parts[0].ToLower();   // "full beard", "chin strap goatee", "handlebar mustache"
	}
	return FText::FromString(FString::Printf(TEXT("%s%s (%s)"), *Style.Left(1).ToUpper(), *Style.Mid(1), **Length));
}

TArray<FMOAppearanceAssetEntry> UMOAppearanceSubsystem::GetAssetsForCategory(EMOAppearanceCategory Category) const
{
	if (const TArray<FMOAppearanceAssetEntry>* Assets = AssetCatalog.Find(Category))
	{
		return *Assets;
	}
	return TArray<FMOAppearanceAssetEntry>();
}

TArray<FMOAppearanceAssetEntry> UMOAppearanceSubsystem::GetAssetsForCategoryWithTags(EMOAppearanceCategory Category, const TArray<FName>& RequiredTags) const
{
	TArray<FMOAppearanceAssetEntry> Result;

	if (const TArray<FMOAppearanceAssetEntry>* Assets = AssetCatalog.Find(Category))
	{
		for (const FMOAppearanceAssetEntry& Entry : *Assets)
		{
			bool bHasAllTags = true;
			for (const FName& Tag : RequiredTags)
			{
				if (!Entry.Tags.Contains(Tag))
				{
					bHasAllTags = false;
					break;
				}
			}

			if (bHasAllTags)
			{
				Result.Add(Entry);
			}
		}
	}

	return Result;
}

bool UMOAppearanceSubsystem::IsAssetPathValid(const FString& AssetPath) const
{
	return KnownAssetPaths.Contains(AssetPath);
}

FText UMOAppearanceSubsystem::GetDisplayNameFromPath(const FString& AssetPath) const
{
	// Extract filename and convert to display name
	// e.g., "Face_Male_01" -> "Male 01"
	FString Name = FPaths::GetBaseFilename(AssetPath);

	// Remove common prefixes
	Name.RemoveFromStart(TEXT("Face_"));
	Name.RemoveFromStart(TEXT("Body_"));
	Name.RemoveFromStart(TEXT("Hair_"));

	// Replace underscores with spaces
	Name.ReplaceInline(TEXT("_"), TEXT(" "));

	return FText::FromString(Name);
}

TArray<FName> UMOAppearanceSubsystem::InferTagsFromAssetName(const FString& AssetName) const
{
	TArray<FName> Tags;

	FString NameLower = AssetName.ToLower();

	// Gender tags
	if (NameLower.Contains(TEXT("male")) && !NameLower.Contains(TEXT("female")))
	{
		Tags.Add(FName(TEXT("Male")));
	}
	if (NameLower.Contains(TEXT("female")))
	{
		Tags.Add(FName(TEXT("Female")));
	}

	// Age tags
	if (NameLower.Contains(TEXT("young")))
	{
		Tags.Add(FName(TEXT("Young")));
	}
	if (NameLower.Contains(TEXT("old")) || NameLower.Contains(TEXT("elder")))
	{
		Tags.Add(FName(TEXT("Old")));
	}

	// Style tags (MetaHuman grooms say their length in the name: Hair_S_ / _M_ / _L_)
	if (NameLower.Contains(TEXT("short")) || NameLower.Contains(TEXT("_s_")))
	{
		Tags.Add(FName(TEXT("Short")));
	}
	if (NameLower.Contains(TEXT("_m_")))
	{
		Tags.Add(FName(TEXT("Medium")));
	}
	if (NameLower.Contains(TEXT("long")) || NameLower.Contains(TEXT("_l_")))
	{
		Tags.Add(FName(TEXT("Long")));
	}
	if (NameLower.Contains(TEXT("curly")))
	{
		Tags.Add(FName(TEXT("Curly")));
	}
	if (NameLower.Contains(TEXT("bald")))
	{
		Tags.Add(FName(TEXT("Bald")));
	}

	return Tags;
}

// ============================================================================
// APPEARANCE APPLICATION
// ============================================================================

USkeletalMesh* UMOAppearanceSubsystem::LoadSkeletalMesh(const FString& AssetPath) const
{
	if (AssetPath.IsEmpty())
	{
		return nullptr;
	}

	return Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *AssetPath));
}

UGroomAsset* UMOAppearanceSubsystem::LoadGroomAsset(const FString& AssetPath) const
{
	if (AssetPath.IsEmpty())
	{
		return nullptr;
	}

	return Cast<UGroomAsset>(StaticLoadObject(UGroomAsset::StaticClass(), nullptr, *AssetPath));
}

bool UMOAppearanceSubsystem::ApplyAppearance(AMOMetaHumanCharacter* Character, const FMOCharacterAppearance& Appearance)
{
	if (!Character)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("UMOAppearanceSubsystem::ApplyAppearance: Null character"));
		return false;
	}

	bool bSuccess = true;

	// Apply body mesh
	if (!Appearance.BodyAssetPath.IsEmpty())
	{
		if (USkeletalMesh* BodyMesh = LoadSkeletalMesh(Appearance.BodyAssetPath))
		{
			Character->GetMesh()->SetSkeletalMesh(BodyMesh);
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("UMOAppearanceSubsystem: Failed to load body mesh: %s"), *Appearance.BodyAssetPath);
			bSuccess = false;
		}
	}

	// Apply face mesh
	if (!Appearance.FaceAssetPath.IsEmpty())
	{
		if (USkeletalMesh* FaceMesh = LoadSkeletalMesh(Appearance.FaceAssetPath))
		{
			if (USkeletalMeshComponent* FaceComp = Character->GetFaceMesh())
			{
				FaceComp->SetSkeletalMesh(FaceMesh);
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("UMOAppearanceSubsystem: Failed to load face mesh: %s"), *Appearance.FaceAssetPath);
			bSuccess = false;
		}
	}

	// The grooms before the colours: a new groom brings its own materials, which the colours then tint (a groom still loading swaps in later,
	// AMOMetaHumanCharacter::ApplyGrooms tints it then).
	bSuccess &= Character->ApplyGrooms(Appearance);

	// Apply body parameters (Mutable integration point)
	ApplyBodyParameters(Character, Appearance.BodyFat, Appearance.MuscleMass, Appearance.Height);

	// Apply colors
	ApplyColors(Character, Appearance);

	UE_LOG(LogMOFramework, Log, TEXT("UMOAppearanceSubsystem: Applied appearance to character %s"), *Character->GetName());

	return bSuccess;
}

bool UMOAppearanceSubsystem::ApplyFace(AMOMetaHumanCharacter* Character, const FString& FaceAssetPath)
{
	if (!Character || FaceAssetPath.IsEmpty())
	{
		return false;
	}

	if (USkeletalMesh* FaceMesh = LoadSkeletalMesh(FaceAssetPath))
	{
		if (USkeletalMeshComponent* FaceComp = Character->GetFaceMesh())
		{
			FaceComp->SetSkeletalMesh(FaceMesh);
			return true;
		}
	}

	return false;
}

namespace MOAppearancePrivate
{
	/** A groom slot: the MetaHuman Blueprint's component name for it (Hair, Eyebrows1, Eyelashes, Beard, Mustache) and the face bone it rides on. */
	struct FGroomSlot
	{
		EMOAppearanceCategory Category;
		const TCHAR* Name;
		const TCHAR* Bone;
	};
	const FGroomSlot GroomSlots[] = {
		{ EMOAppearanceCategory::Hair, TEXT("Hair"), TEXT("FACIAL_C_FacialRoot") },
		{ EMOAppearanceCategory::Eyebrows, TEXT("Eyebrows"), TEXT("FACIAL_C_FacialRoot") },
		{ EMOAppearanceCategory::Eyelashes, TEXT("Eyelashes"), TEXT("FACIAL_C_FacialRoot") },
		{ EMOAppearanceCategory::Beard, TEXT("Beard"), TEXT("Facial_C_JAW") },
		{ EMOAppearanceCategory::Mustache, TEXT("Mustache"), TEXT("FACIAL_C_LipUpper") },
	};

	void ApplyHairSimulationSetting(UGroomComponent* Component);

	/**
	 * Groom physics is OFF by default (Wes, Oct 25 2026: the simulated strands -- the shipped updo's bangs, a whole bob -- "shifting back and
	 * forth" / "fluttery" whenever the head moved; "they just stopped" with physics off). The cause is the ENGINE's hair simulation far from
	 * the world origin: UNiagaraDataInterfaceHairStrands' ComputeWorldTransform casts the groom's and the head bone's world transforms to float
	 * BEFORE taking their relative transform "in double precision", so ~17 km out (single precision ~1 mm) the simulation's frame jumps by
	 * rounding error every frame. Measured: 0.8-0.9 px of back-and-forth per frame at the start site, 0.19-0.21 px (= physics off) 5 m from the
	 * origin with the same groom and damping. Unfixable in a launcher install: Docs/Engine_Patches.md has the patch. With physics off the
	 * strands follow the head skinned (no sway). 1 = each groom as its asset says. Flips every groom in the world at once.
	 */
	TAutoConsoleVariable<int32> CVarHairSimulation(
		TEXT("MO.Hair.Simulation"), 0,
		TEXT("Groom hair physics: 0 = no groom simulates (default: the engine's hair simulation jitters far from the world origin), 1 = each groom as its asset says. Applies at once."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*)
		{
			for (TObjectIterator<UGroomComponent> It; It; ++It)
			{
				if (!It->IsTemplate() && It->IsRegistered() && It->GetWorld() && It->GetWorld()->IsGameWorld())
				{
					ApplyHairSimulationSetting(*It);
				}
			}
		}));

	void ApplyHairSimulationSetting(UGroomComponent* Component)
	{
		const bool bOff = CVarHairSimulation.GetValueOnGameThread() == 0;
		FHairSimulationSettings& Simulation = Component->SimulationSettings;
		if (bOff == (Simulation.bOverrideSettings && !Simulation.SolverSettings.bEnableSimulation))
		{
			return;   // already so
		}
		Simulation.bOverrideSettings = bOff;   // off: the component's own settings, simulation disabled; on: the groom asset's own physics again
		Simulation.SolverSettings.bEnableSimulation = false;
		if (Component->GroomAsset)
		{
			Component->SetGroomAsset(Component->GroomAsset, Component->BindingAsset);   // rebuilds the groom's resources with that physics
		}
	}
}

FString* UMOAppearanceSubsystem::GetGroomPath(FMOCharacterAppearance& Appearance, EMOAppearanceCategory Slot)
{
	switch (Slot)
	{
	case EMOAppearanceCategory::Hair: return &Appearance.HairAssetPath;
	case EMOAppearanceCategory::Eyebrows: return &Appearance.EyebrowsAssetPath;
	case EMOAppearanceCategory::Eyelashes: return &Appearance.EyelashesAssetPath;
	case EMOAppearanceCategory::Beard: return &Appearance.BeardAssetPath;
	case EMOAppearanceCategory::Mustache: return &Appearance.MustacheAssetPath;
	default: return nullptr;
	}
}

UGroomComponent* UMOAppearanceSubsystem::FindGroomComponent(const AActor* Actor, EMOAppearanceCategory Slot)
{
	const MOAppearancePrivate::FGroomSlot* Def = Algo::FindByPredicate(MOAppearancePrivate::GroomSlots, [Slot](const MOAppearancePrivate::FGroomSlot& S) { return S.Category == Slot; });
	if (!Actor || !Def)
	{
		return nullptr;
	}
	TInlineComponentArray<UGroomComponent*> Components(Actor);
	UGroomComponent* const* Found = Components.FindByPredicate([Def](const UGroomComponent* C) { return C->GetName().StartsWith(Def->Name); });
	return Found ? *Found : nullptr;
}

bool UMOAppearanceSubsystem::ApplyHair(AMOMetaHumanCharacter* Character, const FString& HairAssetPath)
{
	FMOCharacterAppearance HairOnly;   // every other groom path empty: those slots stay as they are
	HairOnly.HairAssetPath = HairAssetPath;
	return ApplyGroomsToCharacter(Character, HairOnly, /*bWaitUntilReady=*/false);
}

TArray<FSoftObjectPath> UMOAppearanceSubsystem::FindGroomBindingPaths(const FSoftObjectPath& Groom)
{
	// The binding's Groom property is AssetRegistrySearchable: the registry names each binding's groom without loading it.
	TArray<FSoftObjectPath> Paths;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Bindings;
	Registry.GetAssetsByClass(UGroomBindingAsset::StaticClass()->GetClassPathName(), Bindings);
	for (const FAssetData& Binding : Bindings)
	{
		FString Tag;
		if (Binding.GetTagValue(TEXT("Groom"), Tag) && FSoftObjectPath(FPackageName::ExportTextPathToObjectPath(Tag)) == Groom)
		{
			Paths.Add(Binding.GetSoftObjectPath());
		}
	}
	return Paths;
}

UGroomBindingAsset* UMOAppearanceSubsystem::FindGroomBinding(const UGroomAsset* Groom, const USkeletalMesh* Face)
{
	if (!Groom || !Face)
	{
		return nullptr;
	}
	for (const FSoftObjectPath& Path : FindGroomBindingPaths(FSoftObjectPath(Groom)))
	{
		UGroomBindingAsset* Asset = Cast<UGroomBindingAsset>(Path.TryLoad());
		if (Asset && Asset->GetTargetSkeletalMesh() == Face)
		{
			return Asset;
		}
	}
	return nullptr;
}

bool UMOAppearanceSubsystem::ApplyGroomsToCharacter(AMOMetaHumanCharacter* Character, const FMOCharacterAppearance& Appearance, bool bWaitUntilReady,
	bool* bOutPending, bool* bOutChanged)
{
	if (bOutPending)
	{
		*bOutPending = false;
	}
	if (bOutChanged)
	{
		*bOutChanged = false;
	}
	// Before BeginPlay the Blueprint's groom components do not exist yet (a deferred spawn): AMOMetaHumanCharacter::ApplySkinLook applies them then.
	if (!Character || Character->GetNetMode() == NM_DedicatedServer || !(Character->HasActorBegunPlay() || Character->IsActorBeginningPlay()))
	{
		return true;
	}
	USkeletalMeshComponent* Face = Character->GetFaceMesh();
	// The Blueprint's own grooms follow MO.Hair.Simulation too (rebuilt once when it changes them; cheap afterwards).
	TInlineComponentArray<UGroomComponent*> Grooms(Character);
	for (UGroomComponent* Groom : Grooms)
	{
		MOAppearancePrivate::ApplyHairSimulationSetting(Groom);
	}
	FMOCharacterAppearance Paths = Appearance;
	bool bAllLoaded = true;
	for (const MOAppearancePrivate::FGroomSlot& Slot : MOAppearancePrivate::GroomSlots)
	{
		const FString& Path = *GetGroomPath(Paths, Slot.Category);
		if (Path.IsEmpty())
		{
			continue;   // the Blueprint's own groom
		}
		UGroomAsset* Groom = nullptr;
		if (Path != FMOCharacterAppearance::NoGroom)
		{
			Groom = Cast<UGroomAsset>(FSoftObjectPath(Path).TryLoad());
			if (!Groom)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOAppearance] %s: no groom at '%s' for %s; the slot keeps what it has"), *Character->GetName(), *Path, Slot.Name);
				bAllLoaded = false;
				continue;
			}
		}
		UGroomComponent* Component = FindGroomComponent(Character, Slot.Category);
		if (Component ? Component->GroomAsset == Groom : Groom == nullptr)
		{
			continue;   // already wearing it (or nothing to take off)
		}
		if (!Face || !Face->GetSkeletalMeshAsset())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAppearance] %s has no face mesh: %s groom not applied"), *Character->GetName(), Slot.Name);
			continue;
		}
		UGroomBindingAsset* Binding = Groom ? FindGroomBinding(Groom, Face->GetSkeletalMeshAsset()) : nullptr;
		// The slot keeps what it wears until the new groom (and its binding) can draw: swapped in earlier, the component drops the old groom's
		// resources and draws nothing -- bald -- while an editor build caches the new one's data (seconds) or a packaged one streams it (Wes,
		// Oct 25 2026: "it takes like 5-10 seconds for hair changes to take effect, until it updates they look bald").
		if (Groom && bWaitUntilReady && !(Groom->IsValid() && (!Binding || Binding->IsValid())))
		{
			if (bOutPending)
			{
				*bOutPending = true;
			}
			continue;
		}
		if (bOutChanged)
		{
			*bOutChanged = true;
		}
		if (!Component)
		{
			// The survivor Blueprint has no beard / mustache component: one like the MetaHuman Blueprint's, on the face at its bone.
			Component = NewObject<UGroomComponent>(Character, MakeUniqueObjectName(Character, UGroomComponent::StaticClass(), Slot.Name));
			Component->AttachmentName = Slot.Bone;
			// Its physics in the head's space like the Blueprint's hair (the engine default, the skeleton's root, shakes a beard with every sway).
			if (const UGroomComponent* Hair = FindGroomComponent(Character, EMOAppearanceCategory::Hair))
			{
				Component->SimulationSettings.SimulationSetup = Hair->SimulationSettings.SimulationSetup;
				Component->PhysicsAsset = Hair->PhysicsAsset;
			}
			else
			{
				Component->SimulationSettings.SimulationSetup.LocalBone = TEXT("head");
				Component->SimulationSettings.SimulationSetup.LinearVelocityScale = 0.5f;
				Component->SimulationSettings.SimulationSetup.AngularVelocityScale = 0.5f;
			}
			Component->SetupAttachment(Face);
			Component->RegisterComponent();
			Character->AddInstanceComponent(Component);
		}
		Component->EmptyOverrideMaterials();   // the old groom's material overrides (the MetaHuman assembly's per-wardrobe instances) don't fit the new groom
		Component->SetGroomAsset(Groom, Binding);
		MOAppearancePrivate::ApplyHairSimulationSetting(Component);
		if (Groom && !Binding)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAppearance] %s: no binding of %s to %s -- the groom is projected at run time (Content/Python/make_groom_bindings.py makes one)"),
				*Character->GetName(), *Groom->GetName(), *Face->GetSkeletalMeshAsset()->GetName());
		}
		UE_LOG(LogMOFramework, Log, TEXT("[MOAppearance] %s: %s -> %s"), *Character->GetName(), Slot.Name, Groom ? *Groom->GetName() : TEXT("none"));
	}
	return bAllLoaded;
}

bool UMOAppearanceSubsystem::ApplyBodyParameters(AMOMetaHumanCharacter* Character, float BodyFat, float MuscleMass, float Height)
{
	if (!Character)
	{
		return false;
	}

	// TODO: Integrate with Mutable plugin for actual body morphing
	// For now, we store the values but don't apply visual changes
	// This is where Mutable's CustomizableObject would be updated

	// DEBUG (MOAppearanceSubsystem, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("UMOAppearanceSubsystem: Body params - Fat: %.2f, Muscle: %.2f, Height: %.2f"),
	// 	BodyFat, MuscleMass, Height);

	// Height can be applied via actor scale as a simple fallback
	Character->SetActorScale3D(FVector(1.0f, 1.0f, Height));

	return true;
}

bool UMOAppearanceSubsystem::ApplyColors(AMOMetaHumanCharacter* Character, const FMOCharacterAppearance& Appearance)
{
	if (!Character)
	{
		return false;
	}
	ApplyColorsToActor(Character, Appearance);
	return true;
}

void UMOAppearanceSubsystem::ComputeSkinParameters(float SkinTone, float& OutValue, float& OutSaturation, FLinearColor& OutMultiply)
{
	// The baked skin sits at DefaultSkinTone. Lighter: up to +10 % value. Darker: melanin both darkens and warms skin, and deeper skin reads more
	// saturated -- value falls to 0.2 of the baked brightness at 1, saturation rises a little, and the multiply leans toward a warm brown.
	const float Tone = FMath::Clamp(SkinTone, 0.0f, 1.0f);
	const float Baked = FMOCharacterAppearance::DefaultSkinTone;
	if (Tone <= Baked)
	{
		const float Lighter = (Baked - Tone) / FMath::Max(Baked, KINDA_SMALL_NUMBER);   // 0..1
		OutValue = 1.0f + 0.1f * Lighter;
		OutSaturation = -0.05f * Lighter;
		OutMultiply = FLinearColor::White;
		return;
	}
	const float Darker = FMath::Pow((Tone - Baked) / (1.0f - Baked), 0.85f);   // 0..1
	OutValue = FMath::Lerp(1.0f, 0.2f, Darker);
	OutSaturation = 0.12f * Darker;
	OutMultiply = FMath::Lerp(FLinearColor::White, FLinearColor(1.0f, 0.88f, 0.78f, 1.0f), Darker);
}

const TArray<FMOEyeColorPreset>& UMOAppearanceSubsystem::GetEyeColorPresets()
{
	// Values for M_eye_eyeball_unified's PROCEDURAL iris (MI_MOEye_Procedural; the shipped MetaHuman eye is baked and ignores them), read off a hue
	// sweep in the game (Oct 15 2026): hue 0.0 grey-violet, 0.15 blue, 0.3 teal, 0.45 olive green, 0.75 amber, 0.9 golden brown; value = how dark.
	static const TArray<FMOEyeColorPreset> Presets = []()
	{
		auto P = [](const TCHAR* Id, FText Name, float H1, float V1, float H2, float V2, float Sat)
		{
			FMOEyeColorPreset E;
			E.Id = Id; E.DisplayName = MoveTemp(Name);
			E.PrimaryHue = H1; E.PrimaryValue = V1; E.SecondaryHue = H2; E.SecondaryValue = V2; E.Saturation = Sat;
			return E;
		};
		return TArray<FMOEyeColorPreset>{
			P(TEXT("Brown"),     NSLOCTEXT("MOAppearance", "EyeBrown", "Brown"),           0.88f, 0.30f, 0.93f, 0.20f, 1.50f),
			P(TEXT("DarkBrown"), NSLOCTEXT("MOAppearance", "EyeDarkBrown", "Dark brown"),  0.90f, 0.15f, 0.95f, 0.10f, 1.20f),
			P(TEXT("Amber"),     NSLOCTEXT("MOAppearance", "EyeAmber", "Amber"),           0.80f, 0.56f, 0.88f, 0.40f, 1.70f),
			P(TEXT("Hazel"),     NSLOCTEXT("MOAppearance", "EyeHazel", "Hazel"),           0.52f, 0.45f, 0.86f, 0.32f, 1.20f),
			P(TEXT("Green"),     NSLOCTEXT("MOAppearance", "EyeGreen", "Green"),           0.42f, 0.50f, 0.38f, 0.40f, 1.20f),
			P(TEXT("Blue"),      NSLOCTEXT("MOAppearance", "EyeBlue", "Blue"),             0.15f, 0.62f, 0.12f, 0.50f, 1.10f),
			P(TEXT("Grey"),      NSLOCTEXT("MOAppearance", "EyeGrey", "Grey"),             0.10f, 0.60f, 0.10f, 0.50f, 0.25f),
		};
	}();
	return Presets;
}

void UMOAppearanceSubsystem::RandomizeColors(FMOCharacterAppearance& Appearance)
{
	Appearance.SkinTone = FMath::FRand();
	Appearance.HairMelanin = FMath::FRandRange(0.15f, 1.0f);
	Appearance.HairRedness = FMath::FRandRange(0.0f, 0.6f);
	Appearance.EyeColor = GetEyeColorPresets()[FMath::RandRange(0, GetEyeColorPresets().Num() - 1)].Id;
}

const FMOEyeColorPreset* UMOAppearanceSubsystem::FindEyeColorPreset(FName Id)
{
	return Id.IsNone() ? nullptr : GetEyeColorPresets().FindByPredicate([Id](const FMOEyeColorPreset& E) { return E.Id == Id; });
}

void UMOAppearanceSubsystem::ApplyColorsToActor(AActor* Actor, const FMOCharacterAppearance& Appearance)
{
	if (!Actor || Actor->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	static const FName SkinValueName(TEXT("Basecolor Global Value Post-Bake"));
	static const FName SkinSaturationName(TEXT("Basecolor Global Saturation Post-Bake"));
	static const FName SkinMultiplyName(TEXT("Basecolor Global Multiply Post-Bake"));
	static const FName HairMelaninName(TEXT("hairMelanin"));
	static const FName HairRednessName(TEXT("hairRedness"));
	static const FName IrisHue1Name(TEXT("Iris Primary Color Hue"));
	static const FName IrisValue1Name(TEXT("Iris Primary Color Value"));
	static const FName IrisHue2Name(TEXT("Iris Secondary Color Hue"));
	static const FName IrisValue2Name(TEXT("Iris Secondary Color Value"));
	static const FName IrisSaturationName(TEXT("Iris Global Saturation"));

	// The procedural eye (MI_MOEye_Procedural: the MetaHuman eye with "Use Baked Material" off). /MOFramework is always cooked.
	static const FSoftObjectPath ProceduralEyePath(TEXT("/MOFramework/Characters/Eyes/MI_MOEye_Procedural.MI_MOEye_Procedural"));
	UMaterialInterface* ProceduralEye = Appearance.EyeColor.IsNone() ? nullptr : Cast<UMaterialInterface>(ProceduralEyePath.TryLoad());

	float SkinValue = 1.0f, SkinSaturation = 0.0f;
	FLinearColor SkinMultiply = FLinearColor::White;
	ComputeSkinParameters(Appearance.SkinTone, SkinValue, SkinSaturation, SkinMultiply);
	const FMOEyeColorPreset* Eye = FindEyeColorPreset(Appearance.EyeColor);

	// The dynamic instance of a slot whose material has Parameter (reusing one the MetaHuman component or ApplySkinLook already made).
	auto DynamicFor = [](UMeshComponent* Mesh, int32 Slot, const FName& Parameter) -> UMaterialInstanceDynamic*
	{
		UMaterialInterface* Material = Mesh->GetMaterial(Slot);
		float Probe = 0.0f;
		if (!Material || !Material->GetScalarParameterValue(FHashedMaterialParameterInfo(Parameter), Probe))
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material);
		return Dynamic ? Dynamic : Mesh->CreateAndSetMaterialInstanceDynamic(Slot);
	};

	TInlineComponentArray<UMeshComponent*> Meshes(Actor);
	for (UMeshComponent* Mesh : Meshes)
	{
		// Eyelashes keep their own (near-black) melanin: blond lashes read as no lashes.
		const bool bIsHairGroom = Mesh->IsA<UGroomComponent>() && !Mesh->GetName().Contains(TEXT("Eyelash"));
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* Skin = DynamicFor(Mesh, Slot, SkinValueName))
			{
				Skin->SetScalarParameterValue(SkinValueName, SkinValue);
				Skin->SetScalarParameterValue(SkinSaturationName, SkinSaturation);
				Skin->SetVectorParameterValue(SkinMultiplyName, SkinMultiply);
			}
			else if (bIsHairGroom)
			{
				if (UMaterialInstanceDynamic* Hair = DynamicFor(Mesh, Slot, HairMelaninName))
				{
					Hair->SetScalarParameterValue(HairMelaninName, Appearance.HairMelanin);
					Hair->SetScalarParameterValue(HairRednessName, Appearance.HairRedness);
				}
			}
			else if (Eye && ProceduralEye)
			{
				// The shipped eye is BAKED (static switch "Use Baked Material": the iris colour is a texture, the iris parameters are compiled out).
				// A chosen colour puts the procedural variant of the same eye on the slot; no choice (None) leaves the baked eye alone.
				UMaterialInterface* Current = Mesh->GetMaterial(Slot);
				float Probe = 0.0f;
				if (!Current || !Current->GetScalarParameterValue(FHashedMaterialParameterInfo(IrisHue1Name), Probe))
				{
					continue;
				}
				bool bProcedural = false;
				for (const UMaterialInstance* Chain = Cast<UMaterialInstance>(Current); Chain && !bProcedural; Chain = Cast<UMaterialInstance>(Chain->Parent))
				{
					bProcedural = Chain == ProceduralEye;
				}
				UMaterialInstanceDynamic* Iris = bProcedural ? Cast<UMaterialInstanceDynamic>(Current) : nullptr;
				if (!Iris)
				{
					Iris = UMaterialInstanceDynamic::Create(ProceduralEye, Mesh);
					Mesh->SetMaterial(Slot, Iris);
				}
				if (Iris)
				{
					Iris->SetScalarParameterValue(IrisHue1Name, Eye->PrimaryHue);
					Iris->SetScalarParameterValue(IrisValue1Name, Eye->PrimaryValue);
					Iris->SetScalarParameterValue(IrisHue2Name, Eye->SecondaryHue);
					Iris->SetScalarParameterValue(IrisValue2Name, Eye->SecondaryValue);
					Iris->SetScalarParameterValue(IrisSaturationName, Eye->Saturation);
				}
			}
		}
	}
	// DEBUG (MOAppearance, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOAppearance] %s: skin %.2f (value %.2f), hair melanin %.2f / red %.2f, eyes %s"),
	// 	*Actor->GetName(), Appearance.SkinTone, SkinValue, Appearance.HairMelanin, Appearance.HairRedness, *Appearance.EyeColor.ToString());
}

// ============================================================================
// RANDOM GENERATION
// ============================================================================

FMOCharacterAppearance UMOAppearanceSubsystem::GenerateRandomAppearance(bool bMale)
{
	FMOCharacterAppearance Appearance = bMale ? FMOCharacterAppearance::GetDefaultMale() : FMOCharacterAppearance::GetDefaultFemale();

	TArray<FName> GenderTags;
	GenderTags.Add(bMale ? FName(TEXT("Male")) : FName(TEXT("Female")));

	// Pick random face
	TArray<FMOAppearanceAssetEntry> Faces = GetAssetsForCategoryWithTags(EMOAppearanceCategory::Face, GenderTags);
	if (Faces.Num() > 0)
	{
		Appearance.FaceAssetPath = Faces[FMath::RandRange(0, Faces.Num() - 1)].AssetPath;
	}

	// Pick random body
	TArray<FMOAppearanceAssetEntry> Bodies = GetAssetsForCategoryWithTags(EMOAppearanceCategory::Body, GenderTags);
	if (Bodies.Num() > 0)
	{
		Appearance.BodyAssetPath = Bodies[FMath::RandRange(0, Bodies.Num() - 1)].AssetPath;
	}

	// Pick random hair (don't filter by gender for hair - many styles are unisex)
	TArray<FMOAppearanceAssetEntry> Hairs = GetAssetsForCategory(EMOAppearanceCategory::Hair);
	if (Hairs.Num() > 0)
	{
		Appearance.HairAssetPath = Hairs[FMath::RandRange(0, Hairs.Num() - 1)].AssetPath;
	}

	// Randomize body parameters
	Appearance.BodyFat = FMath::FRandRange(0.1f, 0.6f);
	Appearance.MuscleMass = FMath::FRandRange(0.2f, 0.7f);
	Appearance.Height = FMath::FRandRange(0.9f, 1.1f);

	RandomizeColors(Appearance);

	return Appearance;
}
