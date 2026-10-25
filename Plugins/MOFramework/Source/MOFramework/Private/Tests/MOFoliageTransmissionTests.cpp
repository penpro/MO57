// The grass glowed (Wes, Oct 25 2026: "for the love of god, fix the weird glowing/weirdly shaded grass"; BR23-11, BR24-21). Megascans Two Sided
// Foliage passes Desaturate(BaseColor x Mask x Controls.G, 1 - Controls.R) ^ Controls.B through its blades (QMF_GenerateTranslucency), and the wild
// grass shipped at strength 5 with a square root (exponent 0.5): the transmitted light out-shone the lit side and drowned every shadow on the blades.
// Content/Python/fix_foliage_transmission.py caps it (strength <= 0.5, exponent >= 1); a plant import or reimport brings the pack's values back.
// Rule checked: every material instance under M_MS_Foliage / M_MS_Billboard in /Game keeps each "Translucency Controls*" within the cap.

#include "Misc/AutomationTest.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/StaticMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMORendering_FoliageTransmissionCapped,
	"MOFramework.Rendering.FoliageTransmissionCapped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMORendering_FoliageTransmissionCapped::RunTest(const FString& Parameters)
{
	constexpr float MaxStrength = 0.5f;
	constexpr float MinExponent = 1.0f;
	const TSet<FName> MasterNames = { TEXT("M_MS_Foliage"), TEXT("M_MS_Billboard") };

	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	FARFilter MasterFilter;
	MasterFilter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
	MasterFilter.PackagePaths.Add(TEXT("/Game"));
	MasterFilter.bRecursivePaths = true;
	TArray<FAssetData> Materials;
	Registry.GetAssets(MasterFilter, Materials);
	TArray<FName> Frontier;
	for (const FAssetData& Material : Materials)
	{
		if (MasterNames.Contains(Material.AssetName))
		{
			Frontier.Add(Material.PackageName);
		}
	}
	if (!TestTrue(TEXT("the Megascans foliage masters are in /Game"), Frontier.Num() >= 1))
	{
		return false;
	}

	// Down the parent chains through the registry (an instance hard-references its parent), loading only the instances found.
	const FTopLevelAssetPath InstanceClass = UMaterialInstanceConstant::StaticClass()->GetClassPathName();
	TSet<FName> Seen(Frontier);
	int32 Checked = 0, Over = 0;
	while (!Frontier.IsEmpty())
	{
		TArray<FName> Referencers;
		Registry.GetReferencers(Frontier.Pop(EAllowShrinking::No), Referencers, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);
		for (const FName Referencer : Referencers)
		{
			if (Seen.Contains(Referencer) || !Referencer.ToString().StartsWith(TEXT("/Game/")))
			{
				continue;
			}
			Seen.Add(Referencer);
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPackageName(Referencer, Assets);
			for (const FAssetData& Asset : Assets)
			{
				const UMaterialInstanceConstant* Instance = Asset.AssetClassPath == InstanceClass ? Cast<UMaterialInstanceConstant>(Asset.GetAsset()) : nullptr;
				const UMaterial* Base = Instance ? Instance->GetMaterial() : nullptr;
				if (!Base || !MasterNames.Contains(Base->GetFName()))
				{
					continue;
				}
				Frontier.Add(Referencer);   // its own child instances
				++Checked;
				for (const FVectorParameterValue& Value : Instance->VectorParameterValues)
				{
					if (Value.ParameterInfo.Name.ToString().StartsWith(TEXT("Translucency Controls"))
						&& (Value.ParameterValue.G > MaxStrength + 1e-3f || Value.ParameterValue.B < MinExponent - 1e-3f))
					{
						++Over;
						AddError(FString::Printf(TEXT("%s %s: strength %.2f (max %.2f), exponent %.2f (min %.2f) -- run Content/Python/fix_foliage_transmission.py"),
							*Referencer.ToString(), *Value.ParameterInfo.Name.ToString(), Value.ParameterValue.G, MaxStrength, Value.ParameterValue.B, MinExponent));
					}
				}
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d foliage material instance(s) checked, %d transmission parameter(s) over the cap"), Checked, Over));
	TestTrue(TEXT("the plants' instances were found (the check is not vacuous)"), Checked >= 10);
	return true;
}

#if WITH_EDITOR   // loads the pack's meshes and reads their build settings: editor only (the game target compiles these tests too)
// The blurry flowers (BR23-10; Wes, Oct 23-24 2026: "a few of these little flowers are blurry", "these little purple flowers look good here ...
// but they're weird and blurry in other places"). Under Nanite the PN flowers' material MA_Flowers writes wrong motion vectors (its Pivot Painter /
// player-bend wind), so motion blur streaked them even with the camera still and TSR ghosted them; the classic meshes are sharp (measured in PIE,
// D:/MOAssets/previews/grass/pal_flowers_before_after.png). Content/Python/flower_meshes_non_nanite.py keeps every MA_Flowers mesh classic with
// cross-fading LODs, and foliage_camera_fade.py gives ground cover the narrow near-camera band (a 40-160 cm screen door under a low third-person
// camera read as dotted blades while moving). enable_palette_nanite.py would have turned them back: it leaves MA_Flowers meshes alone now.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMORendering_FlowersNotNanite,
	"MOFramework.Rendering.FlowersNotNanite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMORendering_FlowersNotNanite::RunTest(const FString& Parameters)
{
	const UMaterial* Flowers = LoadObject<UMaterial>(nullptr, TEXT("/Game/PN_MeadowFlowers/Materials/flowerMaterials/MA_Flowers.MA_Flowers"));
	if (!TestNotNull(TEXT("MA_Flowers loads"), Flowers))
	{
		return false;
	}
	TestTrue(TEXT("MA_Flowers cross-fades its LODs (Dithered LOD Transition)"), Flowers->IsDitheredLODTransition());

	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	FARFilter Filter;
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(TEXT("/Game/PN_MeadowFlowers"));
	Filter.bRecursivePaths = true;
	TArray<FAssetData> Meshes;
	Registry.GetAssets(Filter, Meshes);
	int32 Checked = 0;
	for (const FAssetData& Asset : Meshes)
	{
		const UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
		if (!Mesh || !Mesh->GetStaticMaterials().ContainsByPredicate([Flowers](const FStaticMaterial& M) { return M.MaterialInterface && M.MaterialInterface->GetMaterial() == Flowers; }))
		{
			continue;
		}
		++Checked;
		if (Mesh->IsNaniteEnabled())
		{
			AddError(FString::Printf(TEXT("%s is Nanite: MA_Flowers smears under Nanite -- run Content/Python/flower_meshes_non_nanite.py"), *Asset.PackageName.ToString()));
		}
	}
	TestTrue(TEXT("the flower meshes were found (the check is not vacuous)"), Checked >= 50);

	// Ground cover's near-camera fade: the narrow band (foliage_camera_fade.py GROUND_COVER_BAND)
	const TCHAR* GroundCover[] = {
		TEXT("/Game/PN_MeadowFlowers/Materials/flowerMaterials/MA_Flowers.MA_Flowers"),
		TEXT("/Game/Fab/Megascans/Plants/Grass_smdrailr/Medium/smdrailr_tier_2/Materials/MI_smdrailr.MI_smdrailr"),
		TEXT("/Game/Fab/Megascans/Plants/Wild_Grass_vlkhcbxia/Medium/vlkhcbxia_tier_2/Materials/MI_vlkhcbxia.MI_vlkhcbxia"),
		TEXT("/Game/Fab/Megascans/Plants/Thatching_Grass_uddmcgbia/Medium/uddmcgbia_tier_2/Materials/MI_uddmcgbia.MI_uddmcgbia"),
	};
	for (const TCHAR* Path : GroundCover)
	{
		const UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Path);
		float End = -1.0f;
		if (TestNotNull(FString::Printf(TEXT("%s loads"), Path), Material) && Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("CameraFadeEndCm")), End))
		{
			TestTrue(FString::Printf(TEXT("%s fades only near the lens (CameraFadeEndCm %.0f <= 45)"), Path, End), End > 0.0f && End <= 45.0f);
		}
		else
		{
			AddError(FString::Printf(TEXT("%s has no CameraFadeEndCm (MO_CameraFade missing from its master)"), Path));
		}
	}
	AddInfo(FString::Printf(TEXT("%d MA_Flowers mesh(es) checked"), Checked));
	return true;
}
#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
