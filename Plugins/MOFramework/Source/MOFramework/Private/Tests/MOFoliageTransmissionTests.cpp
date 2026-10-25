// The grass glowed (Wes, Oct 25 2026: "for the love of god, fix the weird glowing/weirdly shaded grass"; BR23-11, BR24-21). Megascans Two Sided
// Foliage passes Desaturate(BaseColor x Mask x Controls.G, 1 - Controls.R) ^ Controls.B through its blades (QMF_GenerateTranslucency), and the wild
// grass shipped at strength 5 with a square root (exponent 0.5): the transmitted light out-shone the lit side and drowned every shadow on the blades.
// Content/Python/fix_foliage_transmission.py caps it (strength <= 0.5, exponent >= 1); a plant import or reimport brings the pack's values back.
// Rule checked: every material instance under M_MS_Foliage / M_MS_Billboard in /Game keeps each "Translucency Controls*" within the cap.

#include "Misc/AutomationTest.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
