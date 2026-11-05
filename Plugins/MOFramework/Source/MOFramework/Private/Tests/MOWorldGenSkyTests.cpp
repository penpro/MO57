// Generator v13's sky and slots (fork F62 A; Wes, Nov 3: "floating islands and better caves with chasms more like minecraft"): floating islands, ravines, lobed lakes. Headless-safe. The first
// test asserts that with them off the generator is still v12 to the bit, so nothing else moved.

#include "Misc/AutomationTest.h"
#include "MOGeologyYield.h"
#include "MOWorldGen.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 SkyTestSeed = 424242;

	FMOWorldGenParamsRef MakeSkyParams(float Islands, float Ravines)
	{
		TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> P = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
		P->Tuning.IslandChance = Islands;
		P->Tuning.RavineChance = Ravines;
		P->Finalize();
		return FMOWorldGenParamsRef(P);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_SkyOffIsV12,
	"MOFramework.WorldGen.Sky.SkyOffIsV12",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_SkyOffIsV12::RunTest(const FString& Parameters)
{
	// Off: the world v12 made, from this code, to the bit.
	const FMOWorldGenProbe V12 = FMOWorldGen::RunProbe(/*bWater=*/true, /*bWithoutV13Features=*/true);
	TestEqual(TEXT("off: the height checksum is v12's"), V12.Height, FMOWorldGen::PinnedProbeHeightV12);
	TestEqual(TEXT("off: the cave checksum is v12's"), V12.Cave, FMOWorldGen::PinnedProbeCaveV12);
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Off = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Off->Tuning.RavineChance = 0.0f;
	Off->Tuning.IslandChance = 0.0f;
	Off->Tuning.LakeLobes = 1;
	Off->Finalize();
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> On = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	On->Finalize();
	TestTrue(TEXT("CONTROL: the features are on by default (the params hash differs)"), Off->GetHash() != On->GetHash());
	TestTrue(TEXT("CONTROL: the features move the checksums"), FMOWorldGen::PinnedProbeCave != FMOWorldGen::PinnedProbeCaveV12);
	// On: an island is no ground. The height terrain under one is the same with or without it, and so is every column's water.
	FRandomStream R(7);
	int32 Differ = 0;
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Islands = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Islands->Tuning.RavineChance = 0.0f;
	Islands->Tuning.LakeLobes = 1;
	Islands->Tuning.IslandChance = 1.0f;
	Islands->Finalize();
	for (int32 I = 0; I < 4000; ++I)
	{
		const double X = R.FRandRange(-1.0e6f, 1.0e6f), Y = R.FRandRange(-1.0e6f, 1.0e6f);
		Differ += FMOWorldGen::SampleHeightCm(*Off, X, Y, SkyTestSeed) != FMOWorldGen::SampleHeightCm(*Islands, X, Y, SkyTestSeed) ? 1 : 0;
	}
	TestEqual(TEXT("an island raises no ground"), Differ, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_FloatingIslands,
	"MOFramework.WorldGen.Sky.FloatingIslands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_FloatingIslands::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeSkyParams(0.5f, 0.0f);
	int32 Islands = 0, Floating = 0, GroundUnder = 0, TopNearTuned = 0, InRange = 0, Torn = 0, Apart = 0, Twins = 0;
	float MinRadius = 1.0e9f, MaxRadius = 0.0f, MinOver = 1.0e9f, MaxOver = 0.0f, MinCv = 1.0e9f;
	TArray<TArray<double>> Profiles;
	for (int64 Cy = -6; Cy < 6 && Islands < 12; ++Cy)
	{
		for (int64 Cx = -6; Cx < 6 && Islands < 12; ++Cx)
		{
			FMOFloatingIsland I;
			if (!FMOWorldGen::GetFloatingIslandInCell(*P, Cx, Cy, SkyTestSeed, I))
			{
				continue;
			}
			++Islands;
			// At its centre (always well inside the main clump): rock in its body, air under it down to the ground, and the ground the ground.
			const double X = I.Center.X, Y = I.Center.Y;
			const float Real = FMOWorldGen::SampleHeightCm(*P, X, Y, SkyTestSeed);
			const float ClumpTop = FMOWorldGen::IslandTopCm(*P, X, Y, SkyTestSeed);
			// The clump's top is the cluster's, give or take its rolling hills (they grow with the island).
			TopNearTuned += FMath::Abs(ClumpTop - I.TopCm) < 900.0f + 0.07f * I.RadiusCm ? 1 : 0;
			// Round 3: the footprint is torn, not a disc. Along 64 rays from the centre, the last point over rock: its spread (the coefficient of
			// variation of that reach), and the reach profile itself (scaled by its mean) to tell one island's shape from another's.
			{
				constexpr int32 Rays = 64, Steps = 80;
				double Sum = 0.0, Sum2 = 0.0;
				TArray<double> Profile;
				for (int32 Ray = 0; Ray < Rays; ++Ray)
				{
					const double A = 2.0 * UE_DOUBLE_PI * Ray / Rays;
					double Last = 0.0;
					for (int32 S = 0; S <= Steps; ++S)
					{
						const double D = 1.35 * I.RadiusCm * S / Steps;
						Last = FMOWorldGen::IslandTopCm(*P, X + D * FMath::Cos(A), Y + D * FMath::Sin(A), SkyTestSeed) > -1.0e8f ? D : Last;
					}
					Profile.Add(Last);
					Sum += Last;
					Sum2 += Last * Last;
				}
				const double Mean = Sum / Rays, Cv = Mean > 0.0 ? FMath::Sqrt(FMath::Max(0.0, Sum2 / Rays - Mean * Mean)) / Mean : 0.0;
				for (double& V : Profile)
				{
					V /= FMath::Max(1.0, Mean);
				}
				// Pieces: the footprint on a 96 x 96 grid over the cluster's reach, its 4-connected regions of 4+ cells (a fold of a warped outline is
				// still one region; a chunk with open sky all round it is another).
				constexpr int32 G = 96;
				TArray<uint8> In;
				In.SetNumZeroed(G * G);
				for (int32 J = 0; J < G; ++J)
				{
					for (int32 Ii = 0; Ii < G; ++Ii)
					{
						const double Px = X + 1.35 * I.RadiusCm * (2.0 * (Ii + 0.5) / G - 1.0), Py = Y + 1.35 * I.RadiusCm * (2.0 * (J + 0.5) / G - 1.0);
						In[J * G + Ii] = FMOWorldGen::IslandTopCm(*P, Px, Py, SkyTestSeed) > -1.0e8f ? 1 : 0;
					}
				}
				int32 Pieces = 0;
				TArray<int32> Stack;
				for (int32 Start = 0; Start < G * G; ++Start)
				{
					if (In[Start] != 1)
					{
						continue;
					}
					int32 Cells = 0;
					Stack.Reset();
					Stack.Add(Start);
					In[Start] = 2;
					while (Stack.Num() > 0)
					{
						const int32 C = Stack.Pop(EAllowShrinking::No);
						++Cells;
						const int32 Cx0 = C % G, Cy0 = C / G;
						const int32 Nb[4] = { Cx0 > 0 ? C - 1 : -1, Cx0 < G - 1 ? C + 1 : -1, Cy0 > 0 ? C - G : -1, Cy0 < G - 1 ? C + G : -1 };
						for (const int32 N : Nb)
						{
							if (N >= 0 && In[N] == 1)
							{
								In[N] = 2;
								Stack.Add(N);
							}
						}
					}
					Pieces += Cells >= 4 ? 1 : 0;
				}
				// The same shape as an island before it (scaled): the profiles match ray for ray, at any of the 64 turns.
				bool bSameShape = false;
				for (const TArray<double>& Other : Profiles)
				{
					for (int32 Turn = 0; Turn < Rays && !bSameShape; ++Turn)
					{
						double Diff = 0.0;
						for (int32 Ray = 0; Ray < Rays; ++Ray)
						{
							Diff += FMath::Abs(Profile[Ray] - Other[(Ray + Turn) % Rays]);
						}
						bSameShape = Diff / Rays < 0.02;
					}
				}
				Profiles.Add(Profile);
				Torn += Cv >= 0.12 ? 1 : 0;
				Apart += Pieces > 1 ? 1 : 0;
				Twins += bSameShape ? 1 : 0;
				MinCv = FMath::Min(MinCv, (float)Cv);
				AddInfo(FString::Printf(TEXT("  outline: mean reach %.0f m, spread %.2f, %d piece(s)%s"), Mean / 100.0, Cv, Pieces, bSameShape ? TEXT(", THE SAME SHAPE as an earlier island") : TEXT("")));
			}
			MinRadius = FMath::Min(MinRadius, I.RadiusCm);
			MaxRadius = FMath::Max(MaxRadius, I.RadiusCm);
			MinOver = FMath::Min(MinOver, I.TopCm - I.GroundCm);
			MaxOver = FMath::Max(MaxOver, I.TopCm - I.GroundCm);
			InRange += I.RadiusCm >= P->Tuning.IslandRadiusMinCm - 1.0f && I.RadiusCm <= P->Tuning.IslandRadiusMaxCm + 1.0f
				&& I.TopCm - I.GroundCm >= P->Tuning.IslandHeightMinCm - 1.0f && I.ThicknessCm <= 0.6f * (I.TopCm - I.GroundCm) + 1.0f ? 1 : 0;
			auto Rock = [&](double Z) { return FMOWorldGen::IslandRockDistanceCm(*P, X, Y, Z, SkyTestSeed); };
			// The torn underside hangs at most 0.3 x the thickness (+ 3 m) below the main clump's cone: the gap is the middle of the air under that.
			const double Underside = ClumpTop - 1.3 * I.ThicknessCm - 300.0;
			const bool bAir = Rock(0.5 * ((double)Real + Underside)) > 0.0f;
			const bool bBody = Rock(ClumpTop - 0.15 * I.ThicknessCm) < 0.0f;
			const bool bClear = Underside > (double)Real;
			Floating += bAir && bBody ? 1 : 0;
			GroundUnder += bClear ? 1 : 0;
			AddInfo(FString::Printf(TEXT("island at (%.0f, %.0f): radius %.0f, ground %.0f, top %.0f, thickness %.0f -- air under it %d, body %d, clear of the ground %d"),
				X, Y, I.RadiusCm, Real, I.TopCm, I.ThicknessCm, bAir, bBody, bClear));
		}
	}
	AddInfo(FString::Printf(TEXT("%d island(s): %.0f-%.0f m across, their tops %.0f-%.0f m over the ground"), Islands, 2.0f * MinRadius / 100.0f,
		2.0f * MaxRadius / 100.0f, MinOver / 100.0f, MaxOver / 100.0f));
	TestTrue(TEXT("the window holds islands"), Islands > 0);
	TestEqual(TEXT("each island within the tuned sizes and heights, its underside clear of the ground"), InRange, Islands);
	TestEqual(TEXT("each island's top is about where the cell put it"), TopNearTuned, Islands);
	TestEqual(TEXT("each island floats: air under it, rock in its body"), Floating, Islands);
	TestEqual(TEXT("each island's underside is clear of the ground under it"), GroundUnder, Islands);
	AddInfo(FString::Printf(TEXT("outlines: %d of %d torn (spread >= 0.12; the least %.2f), %d in 2+ pieces, %d the same shape as another"), Torn, Islands, MinCv, Apart, Twins));
	TestEqual(TEXT("round 3: every island's outline is torn, not a disc"), Torn, Islands);
	TestTrue(TEXT("round 3: a quarter of the islands or more have a chunk apart from the main mass"), Apart * 4 >= Islands && Apart > 0);
	TestEqual(TEXT("round 3: no two islands share a shape"), Twins, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_LobedLakes,
	"MOFramework.WorldGen.Sky.LobedLakes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_LobedLakes::RunTest(const FString& Parameters)
{
	// Wes, Nov 3: "i still want lakes to be multiple stamps, not just one circle stamp". LakeLobes 4: every upland lake is 2-4 overlapping lobes, its water
	// part of its ellipse (not all of it, not more), the shore inside the water plane's box, the ground of that box outside the water above the surface,
	// and the water's own queries agree with the shape. LakeLobes 1 is the ellipse (SkyOffIsV12 holds the height checksum to v12's pin).
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Raw = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Raw->Tuning.LakeLobes = 4;
	Raw->Finalize();
	const FMOWorldGenParamsRef P(Raw);
	const FMOWorldGenTuning& T = P->Tuning;
	const int32 HalfCells = (int32)(T.WorldHalfSizeCm / T.UplandLakeCellCm);
	int32 Lakes = 0, LobesOk = 0, PartOk = 0, InsideBox = 0, Buried = 0, WaterAgrees = 0;
	float MinPart = 1.0f, MaxPart = 0.0f;
	double PartSum = 0.0;
	for (int64 Cy = -HalfCells; Cy < HalfCells; ++Cy)
	{
		for (int64 Cx = -HalfCells; Cx < HalfCells; ++Cx)
		{
			FMOWorldGen::FLake L;
			if (!FMOWorldGen::GetLakeInCell(*P, 2, Cx, Cy, SkyTestSeed, L))
			{
				continue;
			}
			++Lakes;
			LobesOk += L.NumLobes >= 2 && L.NumLobes <= 4 ? 1 : 0;
			const FVector2D Half = L.PlaneHalfSizeCm();
			constexpr int32 N = 24;   // 49 x 49 over the plane
			int32 InEllipse = 0, InWater = 0, OutOfEllipse = 0, Exposed = 0, Disagree = 0;
			for (int32 J = -N; J <= N; ++J)
			{
				for (int32 I = -N; I <= N; ++I)
				{
					const double Dx = Half.X * (double)I / (double)N, Dy = Half.Y * (double)J / (double)N;
					const float Lx = (float)((L.CosAngle * Dx + L.SinAngle * Dy) / L.RadiusCm), Ly = (float)((-L.SinAngle * Dx + L.CosAngle * Dy) / (L.RadiusCm * L.Aspect));
					const float E2 = Lx * Lx + Ly * Ly, Q2 = FMOWorldGen::LakeShapeQ2(L, Lx, Ly);
					InEllipse += E2 <= 1.0f ? 1 : 0;
					InWater += Q2 <= 1.0f ? 1 : 0;
					OutOfEllipse += Q2 <= 1.0f && E2 > 1.0f ? 1 : 0;
					const double X = L.Center.X + Dx, Y = L.Center.Y + Dy;
					if (Q2 >= 1.06f)   // past the shore (q >= 1.03, the bank's rise; UplandLakePlane's cut): no water may show here
					{
						Exposed += FMOWorldGen::SampleHeightCm(*P, X, Y, SkyTestSeed) < L.SurfaceCm ? 1 : 0;
					}
					else if (Q2 <= 0.64f)   // well inside (q <= 0.8): the water's queries say water, at this surface, over a bed below it
					{
						float Surface = 0.0f;
						const bool bWet = FMOWorldGen::GetWaterSurfaceCm(*P, X, Y, SkyTestSeed, Surface);
						Disagree += !bWet || FMath::Abs(Surface - L.SurfaceCm) > 1.0f || FMOWorldGen::SampleHeightCm(*P, X, Y, SkyTestSeed) >= L.SurfaceCm ? 1 : 0;
					}
				}
			}
			const float Part = InEllipse > 0 ? (float)InWater / (float)InEllipse : 0.0f;
			MinPart = FMath::Min(MinPart, Part);
			MaxPart = FMath::Max(MaxPart, Part);
			PartSum += Part;
			PartOk += Part >= 0.2f && Part <= 0.92f ? 1 : 0;
			InsideBox += OutOfEllipse == 0 ? 1 : 0;
			Buried += Exposed == 0 ? 1 : 0;
			WaterAgrees += Disagree == 0 ? 1 : 0;
			if (Exposed > 0 || Disagree > 0 || OutOfEllipse > 0)
			{
				AddInfo(FString::Printf(TEXT("lake at (%.0f, %.0f), %d lobes, radius %.0f: %d plane samples of ground under the surface outside the water, %d water samples the queries disagree on, %d outside the ellipse"),
					L.Center.X, L.Center.Y, (int32)L.NumLobes, L.RadiusCm, Exposed, Disagree, OutOfEllipse));
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d lakes: water %.0f-%.0f%% of the ellipse (mean %.0f%%)"), Lakes, 100.0f * MinPart, 100.0f * MaxPart, Lakes > 0 ? 100.0 * PartSum / Lakes : 0.0));
	TestTrue(TEXT("lakes exist to check"), Lakes > 20);
	TestEqual(TEXT("every lake is 2-4 lobes"), LobesOk, Lakes);
	TestEqual(TEXT("every lake's water is part of its ellipse (20-92 %), not a circle stamp"), PartOk, Lakes);
	TestEqual(TEXT("every shore lies inside the ellipse, so inside the water plane's box"), InsideBox, Lakes);
	TestEqual(TEXT("no lake's water plane shows over ground outside its water"), Buried, Lakes);
	TestEqual(TEXT("the water queries agree with the lobes"), WaterAgrees, Lakes);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_Ravines,
	"MOFramework.WorldGen.Sky.Ravines",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_Ravines::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeSkyParams(0.0f, 0.35f);
	const FMOWorldGenParamsRef NoRavines = MakeSkyParams(0.0f, 0.0f);
	const float Vox = P->Tuning.VoxelSizeCm;
	int32 Ravines = 0, Open = 0, Floored = 0, Walled = 0;
	double SliceMs = 0.0, ControlMs = 0.0;
	float MinDepth = 1.0e9f, MaxDepth = 0.0f;
	int32 Slices = 0;
	for (int64 Cy = -8; Cy < 8 && Ravines < 8; ++Cy)
	{
		for (int64 Cx = -8; Cx < 8 && Ravines < 8; ++Cx)
		{
			FMORavine V;
			if (!FMOWorldGen::GetRavineInCell(*P, Cx, Cy, SkyTestSeed, V))
			{
				continue;
			}
			++Ravines;
			MinDepth = FMath::Min(MinDepth, V.DepthCm);
			MaxDepth = FMath::Max(MaxDepth, V.DepthCm);
			// The slot across its middle: the cell's centre lies on the ravine's chord, and its bends carry it up to a sixth of its length off it, so
			// look along the perpendicular through the centre at mid-depth for the air, then test there.
			const double Px = -V.Dir.Y, Py = V.Dir.X;
			double X = V.Center.X, Y = V.Center.Y;
			bool bFound = false;
			for (double Off = 0.0; Off <= 0.2 * V.LengthCm && !bFound; Off += 50.0)
			{
				for (const double Sign : { 1.0, -1.0 })
				{
					const double Qx = V.Center.X + Px * Off * Sign, Qy = V.Center.Y + Py * Off * Sign;
					const float Gq = FMOWorldGen::SampleHeightCm(*P, Qx, Qy, SkyTestSeed, Vox);
					if (FMOWorldGen::SampleCaveDistanceAtGround(*P, Qx, Qy, V.GroundCm - V.DepthCm * 0.3, Gq, SkyTestSeed, Vox) < -50.0f)
					{
						// Entered the slot at its edge: walk on to its far wall and take the middle.
						double Far = Off;
						for (double On = Off + 25.0; On <= Off + 4.0 * V.HalfWidthCm; On += 25.0)
						{
							const double Rx = V.Center.X + Px * On * Sign, Ry = V.Center.Y + Py * On * Sign;
							if (FMOWorldGen::SampleCaveDistanceAtGround(*P, Rx, Ry, V.GroundCm - V.DepthCm * 0.3, FMOWorldGen::SampleHeightCm(*P, Rx, Ry, SkyTestSeed, Vox),
								SkyTestSeed, Vox) >= 0.0f)
							{
								break;
							}
							Far = On;
						}
						const double Mid = 0.5 * (Off + Far);
						X = V.Center.X + Px * Mid * Sign;
						Y = V.Center.Y + Py * Mid * Sign;
						bFound = true;
						break;
					}
				}
			}
			const float G = FMOWorldGen::SampleHeightCm(*P, X, Y, SkyTestSeed, Vox);
			int32 AirAlongDepth = 0;
			for (int32 K = 1; K <= 8; ++K)
			{
				AirAlongDepth += FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, V.GroundCm - V.DepthCm * 0.6 * K / 8.0, G, SkyTestSeed, Vox) < 0.0f ? 1 : 0;
			}
			Open += bFound && AirAlongDepth >= 6 ? 1 : 0;
			Floored += FMOWorldGen::SampleCaveDistanceAtGround(*P, X, Y, V.GroundCm - V.DepthCm - 400.0, G, SkyTestSeed, Vox) > 0.0f ? 1 : 0;
			const double Sx = X + Px * V.HalfWidthCm * 4.0, Sy = Y + Py * V.HalfWidthCm * 4.0;
			const float Gs = FMOWorldGen::SampleHeightCm(*P, Sx, Sy, SkyTestSeed, Vox);
			Walled += FMOWorldGen::SampleCaveDistanceAtGround(*P, Sx, Sy, V.GroundCm - V.DepthCm * 0.5, Gs, SkyTestSeed, Vox) > 0.0f ? 1 : 0;
			// Cost: a 34 x 34 slice of voxels round the ravine's middle, 8 deep -- and the same voxels with the ravines off (v12's cave field alone).
			auto Slice = [&](const FMOWorldGenParams& Q)
			{
				const double T0 = FPlatformTime::Seconds();
				float Sink = 0.0f;
				for (int32 Iz = 0; Iz < 8; ++Iz)
				{
					for (int32 Iy = 0; Iy < 34; ++Iy)
					{
						for (int32 Ix = 0; Ix < 34; ++Ix)
						{
							Sink += FMOWorldGen::SampleCaveDistanceAtGround(Q, X + (Ix - 17) * Vox, Y + (Iy - 17) * Vox, V.GroundCm - V.DepthCm * 0.5 + Iz * Vox, G, SkyTestSeed, Vox);
						}
					}
				}
				return (FPlatformTime::Seconds() - T0) * 1000.0 + (Sink == 12345.0f ? 1e-9 : 0.0);
			};
			SliceMs += Slice(*P);
			ControlMs += Slice(*NoRavines);
			++Slices;
		}
	}
	AddInfo(FString::Printf(TEXT("ravines %.0f-%.0f m deep at their middles"), MinDepth / 100.0f, MaxDepth / 100.0f));
	AddInfo(FString::Printf(TEXT("%d ravine(s): open %d, floored %d, walled %d; the field round one: %.2f ms per 34 x 34 x 8 voxels, %.2f ms with the ravines off (x4.25 for a 34^3 chunk)"),
		Ravines, Open, Floored, Walled, Slices > 0 ? SliceMs / Slices : 0.0, Slices > 0 ? ControlMs / Slices : 0.0));
	TestTrue(TEXT("the window holds ravines"), Ravines > 0);
	TestEqual(TEXT("each ravine is open down its middle (air from the lip to 60 % of its depth)"), Open, Ravines);
	TestEqual(TEXT("each ravine has a rock floor"), Floored, Ravines);
	TestEqual(TEXT("rock beside each ravine"), Walled, Ravines);
	TestTrue(TEXT("every ravine is 50-100 m deep at its middle (Wes: \"50-100m deep\")"), Ravines > 0 && MinDepth >= 0.5f * P->Tuning.RavineDepthMaxCm - 1.0f && MaxDepth <= P->Tuning.RavineDepthMaxCm + 1.0f);
	return true;
}

// A dig on an island cuts the island: the same rule as any ground, soil over rock, and the rock's item at the rock's speed. (It asked the ground below: every point "air", and out came earth at earth's speed.)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_IslandDigYields,
	"MOFramework.WorldGen.Sky.IslandDigYields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_IslandDigYields::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeSkyParams(0.5f, 0.0f);
	const FName Dirt(TEXT("Dirt01"));
	int32 Islands = 0, TopNotSoil = 0, DeepNotRock = 0, RockGaveDirt = 0, RockGaveNothing = 0, TopGaveNoDirt = 0, RockTooEasy = 0;
	for (int64 Cy = -6; Cy < 6 && Islands < 8; ++Cy)
	{
		for (int64 Cx = -6; Cx < 6 && Islands < 8; ++Cx)
		{
			FMOFloatingIsland I;
			if (!FMOWorldGen::GetFloatingIslandInCell(*P, Cx, Cy, SkyTestSeed, I))
			{
				continue;
			}
			const double X = I.Center.X, Y = I.Center.Y;
			const float Top = FMOWorldGen::IslandTopCm(*P, X, Y, SkyTestSeed);
			if (Top < -1.0e8f || FMOWorldGen::IslandRockDistanceCm(*P, X, Y, Top - 1500.0, SkyTestSeed) >= 0.0f)
			{
				continue;
			}
			++Islands;
			TopNotSoil += FMOWorldGen::SampleDig(*P, X, Y, Top - 100.0, SkyTestSeed).Material != EMODigMaterial::Soil ? 1 : 0;
			DeepNotRock += FMOWorldGen::SampleDig(*P, X, Y, Top - 1500.0, SkyTestSeed).Material != EMODigMaterial::Rock ? 1 : 0;
			TArray<FMOItemYield> Yields;
			const FVector Deep(X, Y, Top - 1000.0);
			FMOGeologyYield::ForDig(*P, SkyTestSeed, Deep, 75.0f, 0.9f, 80.0f, /*bSlab=*/false, Dirt, Yields);
			int32 Total = 0, GotDirt = 0;
			for (const FMOItemYield& Y2 : Yields)
			{
				Total += Y2.Count;
				GotDirt += Y2.ItemId == Dirt ? Y2.Count : 0;
			}
			RockGaveNothing += Total == 0 ? 1 : 0;
			RockGaveDirt += GotDirt > 0 ? 1 : 0;
			RockTooEasy += FMOGeologyYield::DigTimeMultiplier(*P, SkyTestSeed, Deep, 75.0f, 0.9f, /*bSlab=*/false) <= 1.2f ? 1 : 0;
			// In the grass top: soil comes out.
			Yields.Reset();
			FMOGeologyYield::ForDig(*P, SkyTestSeed, FVector(X, Y, Top - 75.0), 75.0f, 0.9f, 80.0f, /*bSlab=*/false, Dirt, Yields);
			bool bDirt = false;
			for (const FMOItemYield& Y2 : Yields)
			{
				bDirt = bDirt || (Y2.ItemId == Dirt && Y2.Count > 0);
			}
			TopGaveNoDirt += bDirt ? 0 : 1;
		}
	}
	AddInfo(FString::Printf(TEXT("%d island(s) dug at their centres"), Islands));
	TestTrue(TEXT("the window holds islands"), Islands > 0);
	TestEqual(TEXT("the grass top is soil"), TopNotSoil, 0);
	TestEqual(TEXT("under it is rock"), DeepNotRock, 0);
	TestEqual(TEXT("a dig in the rock yields something"), RockGaveNothing, 0);
	TestEqual(TEXT("... and no earth"), RockGaveDirt, 0);
	TestEqual(TEXT("it is harder than earth"), RockTooEasy, 0);
	TestEqual(TEXT("a dig in the grass top yields earth"), TopGaveNoDirt, 0);
	return true;
}

// What a far chunk draws for an island: at 4 km the cells are 64 m and a small island's rock 40 m thick, so the rock itself is missed and its far trees float in empty sky (Nov 4, shots at 4 and 8 km).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_IslandFarStandIn,
	"MOFramework.WorldGen.Sky.IslandFarStandIn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_IslandFarStandIn::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeSkyParams(0.5f, 0.0f);
	constexpr float Cell = 6400.0f;   // LOD 7's
	const float Grow = 0.5f * Cell, Thicken = Cell;
	int32 Islands = 0, TopSunk = 0, TopRose = 0, ThinUnder = 0, RimLost = 0, Rims = 0, GrewPast = 0, Agrees = 0, Samples = 0;
	for (int64 Cy = -6; Cy < 6 && Islands < 12; ++Cy)
	{
		for (int64 Cx = -6; Cx < 6 && Islands < 12; ++Cx)
		{
			FMOFloatingIsland I;
			if (!FMOWorldGen::GetFloatingIslandInCell(*P, Cx, Cy, SkyTestSeed, I))
			{
				continue;
			}
			++Islands;
			auto Far = [&](double X, double Y, double Z) { return FMOWorldGen::IslandFarDistanceCm(*P, X, Y, Z, SkyTestSeed, Grow, Thicken); };
			// Across the island: the top where the rock's top is (the trees stand on it), and rock a whole cell deep under it, whatever the rock itself does there.
			for (int32 K = 0; K < 200; ++K)
			{
				const double A = K * 0.61803, Rr = I.RadiusCm * FMath::Sqrt((K + 0.5) / 200.0);
				const double X = I.Center.X + Rr * FMath::Cos(A * 2.0 * UE_DOUBLE_PI), Y = I.Center.Y + Rr * FMath::Sin(A * 2.0 * UE_DOUBLE_PI);
				const float Top = FMOWorldGen::IslandTopCm(*P, X, Y, SkyTestSeed);
				if (Top < -1.0e8f)
				{
					continue;
				}
				++Samples;
				TopSunk += Far(X, Y, Top - 100.0) >= 0.0f ? 1 : 0;
				TopRose += Far(X, Y, Top + 100.0) <= 0.0f ? 1 : 0;   // where a higher clump's wider outline now reaches this column
				ThinUnder += Far(X, Y, Top - 0.9 * Thicken) >= 0.0f ? 1 : 0;
				Agrees += (FMOWorldGen::IslandRockDistanceCm(*P, X, Y, Top - 100.0, SkyTestSeed) < 0.0f) == (Far(X, Y, Top - 100.0) < 0.0f) ? 1 : 0;
			}
			// Off the rim: going out from the centre, the first column past the top. The stand-in is still rock there (it is half a cell wider), and well past the reach it is not.
			float LastTop = -1.0e9f;
			for (double Rr = 0.0; Rr <= 1.3 * I.RadiusCm; Rr += 200.0)
			{
				const float Top = FMOWorldGen::IslandTopCm(*P, I.Center.X + Rr, I.Center.Y, SkyTestSeed);
				if (Top < -1.0e8f && LastTop > -1.0e8f)
				{
					++Rims;
					// (The rim is rounded down, by up to 0.06 of the clump + 5 m and its jags: look down the column for it.)
					bool bRock = false;
					for (float Down = 0.0f; Down <= 0.5f * Thicken && !bRock; Down += 100.0f)
					{
						bRock = Far(I.Center.X + Rr, I.Center.Y, LastTop - Down) < 0.0f;
					}
					RimLost += bRock ? 0 : 1;
					break;
				}
				LastTop = Top;
			}
			GrewPast += Far(I.Center.X + 1.3 * I.RadiusCm + 200.0, I.Center.Y, I.TopCm - 500.0) < 0.0f ? 1 : 0;
		}
	}
	AddInfo(FString::Printf(TEXT("%d island(s), %d columns: the far stand-in and the rock agree on rock under the top at %d"), Islands, Samples, Agrees));
	TestTrue(TEXT("the window holds islands"), Islands > 0 && Rims > 0);
	TestEqual(TEXT("the top never sinks under the rock's (the trees stand on it)"), TopSunk, 0);
	TestTrue(FString::Printf(TEXT("... and stays there, but for a higher clump's wider outline: at most 5 %% of the columns rise (got %d of %d)"), TopRose, Samples), TopRose * 20 <= Samples);
	TestEqual(TEXT("rock a whole cell deep under every column of the top"), ThinUnder, 0);
	TestEqual(TEXT("it reaches half a cell past the rim"), RimLost, 0);
	TestEqual(TEXT("and stops at the island's reach"), GrewPast, 0);
	return true;
}

// Where the biome spawner plants a floating island (Wes, Nov 4: "make sure they have trees and such on them too though? not just grass").
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_IslandSurface,
	"MOFramework.WorldGen.Sky.IslandSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_IslandSurface::RunTest(const FString& Parameters)
{
	TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> Built = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
	Built->Tuning.IslandChance = 0.5f;
	Built->Finalize();
	const FMOWorldGenParamsRef P(Built);
	constexpr float Inset = 300.0f;
	int32 Islands = 0, Tops = 0, Plantable = 0, NotOnRock = 0, NotUnderSky = 0, Downward = 0, Steep = 0, BadOrigin = 0, Flat30 = 0, IslandsWithLittle = 0, BoxMissed = 0, EdgeTooNear = 0, RimHoles = 0;
	int32 EmptyCellsHit = 0, EmptyCells = 0;
	float LeastShare = 1.0f;
	for (int64 Cy = -6; Cy < 6; ++Cy)
	{
		for (int64 Cx = -6; Cx < 6; ++Cx)
		{
			const double Cell = P->Tuning.IslandCellCm;
			const double CellMinX = Cx * Cell - P->Origin.X, CellMinY = Cy * Cell - P->Origin.Y;   // world XY of the cell
			FMOFloatingIsland I;
			if (!FMOWorldGen::GetFloatingIslandInCell(*P, Cx, Cy, SkyTestSeed, I))
			{
				// A cell with no island: a box inside it reaches none (an island stays inside its own cell).
				++EmptyCells;
				EmptyCellsHit += FMOWorldGen::IslandReachesBox(*P, CellMinX + 100.0, CellMinY + 100.0, CellMinX + Cell - 100.0, CellMinY + Cell - 100.0, SkyTestSeed) ? 1 : 0;
				continue;
			}
			++Islands;
			BoxMissed += FMOWorldGen::IslandReachesBox(*P, I.Center.X - 1000.0, I.Center.Y - 1000.0, I.Center.X + 1000.0, I.Center.Y + 1000.0, SkyTestSeed) ? 0 : 1;
			// A lattice over the island's reach: every point the surface says yes to has rock under it and sky over it, and a top that goes on for the inset each way.
			constexpr int32 N = 48;
			int32 TopsHere = 0, PlantableHere = 0;
			const double Reach = 1.3 * I.RadiusCm;
			for (int32 Iy = 0; Iy <= N; ++Iy)
			{
				for (int32 Ix = 0; Ix <= N; ++Ix)
				{
					const double X = I.Center.X - Reach + 2.0 * Reach * Ix / N, Y = I.Center.Y - Reach + 2.0 * Reach * Iy / N;
					const float RawTop = FMOWorldGen::IslandTopCm(*P, X, Y, SkyTestSeed);
					TopsHere += RawTop > -1.0e8f ? 1 : 0;
					RimHoles += RawTop > -1.0e8f && FMOWorldGen::IslandRockDistanceCm(*P, X, Y, (double)RawTop - 100.0, SkyTestSeed) >= 0.0f ? 1 : 0;
					FMOIslandSurfacePoint S;
					if (!FMOWorldGen::SampleIslandSurface(*P, X, Y, SkyTestSeed, Inset, S))
					{
						continue;
					}
					++PlantableHere;
					NotOnRock += FMOWorldGen::IslandRockDistanceCm(*P, X, Y, (double)S.TopCm - 100.0, SkyTestSeed) < 0.0f ? 0 : 1;
					NotUnderSky += FMOWorldGen::IslandRockDistanceCm(*P, X, Y, (double)S.TopCm + 100.0, SkyTestSeed) > 0.0f ? 0 : 1;
					Downward += S.Normal.Z > 0.0 ? 0 : 1;
					const float SlopeDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((float)S.Normal.Z, -1.0f, 1.0f)));
					Flat30 += SlopeDeg <= 30.0f ? 1 : 0;
					Steep += SlopeDeg > 60.0f ? 1 : 0;
					BadOrigin += FMath::Abs(S.OriginGroundCm - I.GroundCm) < 1.0f ? 0 : 1;
					for (const FVector2D Tap : { FVector2D(Inset, 0.0), FVector2D(-Inset, 0.0), FVector2D(0.0, Inset), FVector2D(0.0, -Inset) })
					{
						EdgeTooNear += FMOWorldGen::IslandTopCm(*P, X + Tap.X, Y + Tap.Y, SkyTestSeed) > -1.0e8f ? 0 : 1;
					}
				}
			}
			Tops += TopsHere;
			Plantable += PlantableHere;
			LeastShare = FMath::Min(LeastShare, (float)PlantableHere / FMath::Max(TopsHere, 1));
			IslandsWithLittle += PlantableHere < TopsHere / 2 ? 1 : 0;
		}
	}
	AddInfo(FString::Printf(TEXT("%d islands: %d lattice points with a top (%d rim holes), %d plantable (the least on one island %.2f of its top), %d of those under 30 deg, %d over 60"),
		Islands, Tops, RimHoles, Plantable, LeastShare, Flat30, Steep));
	TestTrue(TEXT("some islands in the test's cells"), Islands >= 8);
	TestEqual(TEXT("every plantable point has rock a metre under its top"), NotOnRock, 0);
	TestEqual(TEXT("every plantable point has sky a metre over its top"), NotUnderSky, 0);
	TestEqual(TEXT("every normal points up"), Downward, 0);
	TestEqual(TEXT("the top goes on for the inset each way"), EdgeTooNear, 0);
	TestEqual(TEXT("the origin ground is the island's"), BadOrigin, 0);
	TestEqual(TEXT("no island is out of the box round its own centre"), BoxMissed, 0);
	TestEqual(TEXT("no empty cell's box reaches an island"), EmptyCellsHit, 0);
	TestTrue(TEXT("CONTROL: some cells are empty"), EmptyCells > 0);
	TestEqual(TEXT("more than half of every island's top can hold a plant"), IslandsWithLittle, 0);
	// Measured 0.69 (Nov 4): a clump's outer third is its broken rim by design, so a good part of the top is steep; a wrong normal would read far lower.
	TestTrue(TEXT("most of what can hold a plant is under 30 deg (trees stand there)"), Flat30 >= Plantable * 3 / 5);
	const FMOWorldGenParamsRef Off = MakeSkyParams(0.0f, 0.0f);
	TestFalse(TEXT("islands off: no box reaches one"), FMOWorldGen::IslandReachesBox(*Off, -1.0e6, -1.0e6, 1.0e6, 1.0e6, SkyTestSeed));
	FMOIslandSurfacePoint None;
	TestFalse(TEXT("islands off: no surface"), FMOWorldGen::SampleIslandSurface(*Off, 0.0, 0.0, SkyTestSeed, Inset, None));
	return true;
}

#endif
