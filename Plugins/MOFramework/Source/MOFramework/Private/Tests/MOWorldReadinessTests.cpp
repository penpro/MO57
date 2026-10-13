// FAR-GRID HOLD of UMOWorldReadinessSubsystem. The PCG runtime scheduler generates the LARGEST hi-gen grid first, so the far-canopy tier (GRID512, 2 km cells out to 7.5 km) was generated in
// full before the first near cell and the packaged new-game reveal went from ~29 s to 46 s. A readiness cycle now takes the radius of every grid of mo.Readiness.HoldGridsFrom and larger
// away (a negative radius: the scheduler walks ceil(radius / cell) cells, so 0 would still generate the viewer's own cell) and writes the authored radii back at Ready.

#include "Misc/AutomationTest.h"
#include "MOWorldReadinessSubsystem.h"
#include "PCGCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using URS = UMOWorldReadinessSubsystem;

	bool SameRadius(const FPerQualityLevelFloat* A, const FPerQualityLevelFloat* B)
	{
		if (!A || !B || A->Default != B->Default || A->PerQuality.Num() != B->PerQuality.Num())
		{
			return false;
		}
		for (const TPair<int32, float>& Q : A->PerQuality)
		{
			const float* Other = B->PerQuality.Find(Q.Key);
			if (!Other || *Other != Q.Value)
			{
				return false;
			}
		}
		return true;
	}

	bool IsHeld(const FPerQualityLevelFloat* R)
	{
		const float Held = URS::GetHeldRadiusCm();
		if (!R || R->Default != Held)
		{
			return false;
		}
		for (const TPair<int32, float>& Q : R->PerQuality)
		{
			if (Q.Value != Held)
			{
				return false;
			}
		}
		return true;
	}

	void SetRadius(FPCGRuntimeGenerationRadii& R, int32 Grid, float Default, TMap<int32, float> PerQuality = {})
	{
		if (FPerQualityLevelFloat* F = URS::GetMutableRadiusField(R, Grid))
		{
			F->Default = Default;
			F->PerQuality = MoveTemp(PerQuality);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOReadiness_FarGridHold,
	"MOFramework.Readiness.FarGridHold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOReadiness_FarGridHold::RunTest(const FString& Parameters)
{
	// Every grid that owns a field is reachable (the fields are protected: a rename in a later engine would leave the hold silently doing nothing).
	FPCGRuntimeGenerationRadii Authored;
	for (const int32 Grid : { 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048 })
	{
		TestNotNull(*FString::Printf(TEXT("the radius field of GRID%d is reachable"), Grid), URS::GetMutableRadiusField(Authored, Grid));
	}

	// The game level's radii (set_game_radii.py): per-quality values on the tiers it overrides.
	SetRadius(Authored, 16, 18000.0f);
	SetRadius(Authored, 128, 150000.0f);
	SetRadius(Authored, 256, 250000.0f, { { 0, 87500.0f }, { 3, 250000.0f } });
	SetRadius(Authored, 512, 750000.0f, { { 0, 262500.0f }, { 1, 375000.0f }, { 3, 750000.0f } });
	SetRadius(Authored, 2048, 1000.0f);
	Authored.ComputeHash();

	FPCGRuntimeGenerationRadii R = Authored;
	URS::HoldRadii(R, 512);
	TestTrue(TEXT("GRID512 (the canopy tier) is held, default and every quality level"), IsHeld(URS::GetRadiusField(R, 512)));
	TestTrue(TEXT("GRID1024 is held"), IsHeld(URS::GetRadiusField(R, 1024)));
	TestTrue(TEXT("GRID2048 is held (the grids above it derive from its field)"), IsHeld(URS::GetRadiusField(R, 2048)));
	TestTrue(TEXT("GRID256 (stand-in trees) is NOT held"), SameRadius(URS::GetRadiusField(R, 256), URS::GetRadiusField(Authored, 256)));
	TestTrue(TEXT("GRID128 (the tree tier) is NOT held"), SameRadius(URS::GetRadiusField(R, 128), URS::GetRadiusField(Authored, 128)));
	TestTrue(TEXT("GRID16 (near ground cover) is NOT held"), SameRadius(URS::GetRadiusField(R, 16), URS::GetRadiusField(Authored, 16)));
	TestTrue(TEXT("the held radii differ from the authored ones (the scheduler's change detector keys on them)"), !(R == Authored));
	TestTrue(TEXT("the held radii's cached hash was recomputed"), GetTypeHash(R) != GetTypeHash(Authored));
	TestTrue(TEXT("GetGenerationRadiusFromGrid reads the held value for GRID512"), R.GetGenerationRadiusFromGrid(EPCGHiGenGrid::Grid512) == URS::GetHeldRadiusCm());

	// The scheduler walks ceil(radius / cell) cells each way around a source (PCGRuntimeGenScheduler, UE 5.8): a held grid must walk none, even with a large grid-size multiplier.
	for (const double CellCm : { 51200.0 * 4.0, 51200.0 * 8.0, 204800.0 * 8.0 })
	{
		const int32 Walk = FMath::CeilToInt32(static_cast<double>(URS::GetHeldRadiusCm()) / CellCm);
		TestTrue(*FString::Printf(TEXT("a held radius walks no cell of a %.0f cm grid (ceil = %d)"), CellCm, Walk), Walk < 0);
	}

	URS::RestoreRadii(R, Authored);
	TestTrue(TEXT("GRID512 comes back exactly (default + per quality)"), SameRadius(URS::GetRadiusField(R, 512), URS::GetRadiusField(Authored, 512)));
	TestTrue(TEXT("GRID1024 comes back exactly"), SameRadius(URS::GetRadiusField(R, 1024), URS::GetRadiusField(Authored, 1024)));
	TestTrue(TEXT("GRID2048 comes back exactly"), SameRadius(URS::GetRadiusField(R, 2048), URS::GetRadiusField(Authored, 2048)));
	TestTrue(TEXT("the restored radii equal the authored ones, hash included"), R == Authored && GetTypeHash(R) == GetTypeHash(Authored));

	FPCGRuntimeGenerationRadii Off = Authored;
	URS::HoldRadii(Off, 0);
	TestTrue(TEXT("HoldGridsFrom 0 holds nothing"), Off == Authored);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
