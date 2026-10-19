// Foliage wind keeps its precision far from the world origin (Docs/PCG.md section 9, BACKLOG N-166 / N-167). A Pivot Painter wind graph that adds
// Time x speed to a WORLD-space pivot term in float stalls and snaps 5-15 km from the origin (MO57's trees stand there): the sum's float
// spacing reaches a texel of the turbulence texture. Content/Python/foliage_wind_precision.py reduces the world term with an Fmod (description
// MO_WindPrecision) before the add. A Megascans / PN pack reimport silently brings the unpatched graph back: this test catches that.
// Rule checked: in every /Game material function that depends on a PivotPainter2 package, an Add whose one operand reaches a Time node (and no
// pivot) and whose other reaches the decoded pivot (and no Time) must take the pivot operand straight from a MO_WindPrecision Fmod.

#include "Misc/AutomationTest.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionFmod.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionTime.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITORONLY_DATA

namespace MOWindPrecision
{
	const TCHAR* Tag = TEXT("MO_WindPrecision");

	/** The Pivot Painter pivot: a call of ms_PivotPainter2_DecodePostion (Megascans' spelling) / DecodePosition, or the "Position And Index"
	 *  texture input it decodes (the PN packs pass the texture in). */
	bool IsPivotSource(const UMaterialExpression* Expr)
	{
		if (const UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
		{
			return Call->MaterialFunction && (Call->MaterialFunction->GetName().Contains(TEXT("PivotPainter2_DecodePostion"))
				|| Call->MaterialFunction->GetName().Contains(TEXT("PivotPainter2_DecodePosition")));
		}
		if (const UMaterialExpressionFunctionInput* Input = Cast<UMaterialExpressionFunctionInput>(Expr))
		{
			return Input->InputName.ToString().Contains(TEXT("Position And Index"), ESearchCase::IgnoreCase);
		}
		return false;
	}

	enum EReach : uint8 { None = 0, Time = 1, Pivot = 2 };

	/** What `Expr` (or anything upstream of it in this graph) reads: Time, the pivot, both. */
	uint8 Reaches(UMaterialExpression* Expr, TSet<UMaterialExpression*>& Seen, int32 Depth = 48)
	{
		if (!Expr || Depth < 0 || Seen.Contains(Expr))
		{
			return None;
		}
		Seen.Add(Expr);
		uint8 Result = Expr->IsA<UMaterialExpressionTime>() ? Time : (IsPivotSource(Expr) ? Pivot : None);
		for (FExpressionInputIterator It{ Expr }; It; ++It)
		{
			Result |= Reaches(It->Expression, Seen, Depth - 1);
		}
		return Result;
	}

	uint8 Reaches(UMaterialExpression* Expr)
	{
		TSet<UMaterialExpression*> Seen;
		return Reaches(Expr, Seen);
	}

	bool IsTaggedFmod(UMaterialExpression* Expr)
	{
		while (UMaterialExpressionReroute* Reroute = Cast<UMaterialExpressionReroute>(Expr))
		{
			Expr = Reroute->Input.Expression;
		}
		const UMaterialExpressionFmod* Fmod = Cast<UMaterialExpressionFmod>(Expr);
		return Fmod && Fmod->Desc == Tag;
	}

	/** Every Add of a time-only and a pivot-only operand: Reduced = the pivot operand comes straight from the tagged Fmod. */
	void FindSites(TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions, int32& OutSites, TArray<FString>& OutUnreduced)
	{
		for (UMaterialExpression* Expr : Expressions)
		{
			UMaterialExpressionAdd* Add = Cast<UMaterialExpressionAdd>(Expr);
			if (!Add || !Add->A.Expression || !Add->B.Expression)
			{
				continue;
			}
			const uint8 A = Reaches(Add->A.Expression);
			const uint8 B = Reaches(Add->B.Expression);
			UMaterialExpression* PivotOperand = (A == Time && B == Pivot) ? Add->B.Expression : (A == Pivot && B == Time) ? Add->A.Expression : nullptr;
			if (!PivotOperand)
			{
				continue;
			}
			++OutSites;
			if (!IsTaggedFmod(PivotOperand))
			{
				OutUnreduced.Add(Add->GetName());
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOData_FoliageWindPrecision,
	"MOFramework.Data.FoliageWindPrecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOData_FoliageWindPrecision::RunTest(const FString& Parameters)
{
	using namespace MOWindPrecision;

	// Control: a minimal graph Time + pivot -> Add is flagged; with the tagged Fmod between pivot and Add it passes.
	{
		UMaterialFunction* Graph = NewObject<UMaterialFunction>(GetTransientPackage());
		UMaterialExpressionTime* TimeNode = NewObject<UMaterialExpressionTime>(Graph);
		UMaterialExpressionFunctionInput* PivotNode = NewObject<UMaterialExpressionFunctionInput>(Graph);
		PivotNode->InputName = TEXT("Position And Index Texture");
		UMaterialExpressionAdd* AddNode = NewObject<UMaterialExpressionAdd>(Graph);
		AddNode->A.Expression = TimeNode;
		AddNode->B.Expression = PivotNode;
		for (UMaterialExpression* Node : TArray<UMaterialExpression*>{ TimeNode, PivotNode, AddNode })
		{
			Graph->GetExpressionCollection().AddExpression(Node);
		}
		int32 Sites = 0;
		TArray<FString> Unreduced;
		FindSites(Graph->GetExpressions(), Sites, Unreduced);
		TestTrue(TEXT("control: an unreduced time + pivot add is flagged"), Sites == 1 && Unreduced.Num() == 1);

		UMaterialExpressionFmod* FmodNode = NewObject<UMaterialExpressionFmod>(Graph);
		FmodNode->Desc = Tag;
		FmodNode->A.Expression = PivotNode;
		AddNode->B.Expression = FmodNode;
		Graph->GetExpressionCollection().AddExpression(FmodNode);
		Sites = 0;
		Unreduced.Reset();
		FindSites(Graph->GetExpressions(), Sites, Unreduced);
		TestTrue(TEXT("control: the reduced add passes"), Sites == 1 && Unreduced.Num() == 0);
	}

	IAssetRegistry* Registry = IAssetRegistry::Get();
	if (!Registry)
	{
		AddError(TEXT("no asset registry"));
		return false;
	}
	if (Registry->IsLoadingAssets())
	{
		Registry->WaitForCompletion();
	}
	TArray<FAssetData> Functions;
	FARFilter Filter;
	Filter.ClassPaths.Add(UMaterialFunction::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(TEXT("/Game"));
	Filter.bRecursivePaths = true;
	Registry->GetAssets(Filter, Functions);

	int32 Checked = 0, TotalSites = 0;
	bool bSawAdvancedWindLevel = false;
	for (const FAssetData& Asset : Functions)
	{
		TArray<FName> Dependencies;
		Registry->GetDependencies(Asset.PackageName, Dependencies);
		if (!Dependencies.ContainsByPredicate([](const FName& Dep) { return Dep.ToString().Contains(TEXT("PivotPainter2")); }))
		{
			continue;
		}
		UMaterialFunction* Function = Cast<UMaterialFunction>(Asset.GetAsset());
		if (!Function)
		{
			continue;
		}
		++Checked;
		int32 Sites = 0;
		TArray<FString> Unreduced;
		FindSites(Function->GetExpressions(), Sites, Unreduced);
		TotalSites += Sites;
		bSawAdvancedWindLevel |= Asset.AssetName == TEXT("MF_AdvancedWind_Level") && Sites > 0;
		for (const FString& Site : Unreduced)
		{
			AddError(FString::Printf(TEXT("%s: %s adds Time to the world-space pivot without the %s Fmod (stalls and snaps far from the origin). "
				"Fix: python Tools/ue.py py --file Content/Python/foliage_wind_precision.py (add the function to its FUNCTIONS list)"),
				*Asset.PackageName.ToString(), *Site, Tag));
		}
		AddInfo(FString::Printf(TEXT("%s: %d time + pivot site(s), %d unreduced"), *Asset.PackageName.ToString(), Sites, Unreduced.Num()));
	}
	TestTrue(TEXT("found the Megascans MF_AdvancedWind_Level and its time + pivot sites (the scan is not vacuous)"), bSawAdvancedWindLevel);
	AddInfo(FString::Printf(TEXT("%d Pivot Painter function(s), %d site(s) checked"), Checked, TotalSites));
	return true;
}

#endif
