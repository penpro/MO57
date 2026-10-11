// New-game spawn search of the world generator: land on the shore of a real ocean, looking across the water at relief on the far side.
// Same rules as MOWorldGen.h (pure, deterministic, thread-safe, IEEE-exact ops in anything that decides the result). The only transcendental, Atan2, produces the cosmetic
// facing angle AFTER the position is decided -- it can never change where the spawn is.
//
// Two stages, because the real terrain is expensive and the question is global:
//   1. A coarse grid over +-24 km (400 m cells, base height only: oceans are just base ground under sea level). Every shore cell (land next to water) casts 16 rays
//      over the grid: unbroken open water in front of it (the "fetch") and the highest ground behind that water (the "far relief"). A real sea is open in a 45 degree cone, which
//      rules out rivers, inlets and the odd sea-level pond; the score rewards a long view that ends in relief and mildly prefers the origin.
//   2. The best candidates are refined on the real terrain (rivers, lakes, detail octaves): find the waterline, step inland, and accept the first dry, gentle, hole-free spot.

#include "MOWorldGen.h"
#include "MOWorldGenNoise.h"

using namespace MOWorldGenNoise;

namespace
{
	constexpr float WaterBelowSeaCm = 20.0f;    // ground this far under sea level counts as water
	constexpr int32 MaxRefineTries = 160;
	constexpr float EyeAboveSeaCm = 200.0f;     // the eye stands about 2 m above the water: ground lower than that is not a skyline

	struct FCandidate
	{
		float Score = 0.0f;
		float OpenWaterCm = 0.0f, FarReliefCm = 0.0f, FarDistanceCm = 0.0f;
		int32 CellX = 0, CellY = 0, Dir = 0;
	};

	FORCEINLINE int32 WrapDir(int32 D) { return (D + 16) & 15; }

	/** True when `H` (a point of the real terrain) is open water. */
	FORCEINLINE bool IsWater(float H, float SeaCm) { return H < SeaCm - WaterBelowSeaCm; }
}

bool FMOWorldGen::FindCoastalSpawn(const FMOWorldGenParams& Params, int32 Seed, FMOCoastalSpawn& Out, const FMOCoastalSearch& Search)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	const float Sea = T.SeaLevelCm;
	const float GridStepCm = Search.GridStepCm;
	const int32 MaxRayCells = Search.MaxViewCells, MinFetchCells = Search.MinOpenWaterCells, FarWindowCells = Search.FarWindowCells;
	const int32 Half = FMath::Min(Search.GridHalfCells, (int32)((T.WorldHalfSizeCm - Search.BorderMarginCm) / GridStepCm));
	if (Half < 8)
	{
		return false;
	}
	const int32 N = 2 * Half + 1;

	// ---- stage 1: the coarse grid --------------------------------------------------------------------------------------------------------------------------------
	TArray<float> Ground;
	Ground.SetNumUninitialized(N * N);
	for (int32 J = 0; J < N; ++J)
	{
		for (int32 I = 0; I < N; ++I)
		{
			Ground[J * N + I] = SampleBaseHeightCm(Params, (double)(I - Half) * (double)GridStepCm, (double)(J - Half) * (double)GridStepCm, Seed, 4000.0f);
		}
	}
	auto At = [&](int32 I, int32 J) { return Ground[J * N + I]; };

	// Landmasses of the coarse grid (4-connected land cells): a spawn on a small one is an island start. A landmass that reaches the edge of the grid continues beyond it, so it
	// is judged by a lower bar; the fine check on the real terrain (MeasureLandmassCells) has the last word on the spawn that is picked.
	const float CellKm2 = FMath::Square(GridStepCm / 100000.0f);
	TArray<int32> Landmass;
	Landmass.Init(-1, N * N);
	TArray<float> LandmassKm2;
	TArray<uint8> LandmassTouchesEdge;
	{
		TArray<int32> Stack;
		for (int32 J0 = 0; J0 < N; ++J0)
		{
			for (int32 I0 = 0; I0 < N; ++I0)
			{
				if (Landmass[J0 * N + I0] >= 0 || IsWater(At(I0, J0), Sea))
				{
					continue;
				}
				const int32 Id = LandmassKm2.Num();
				int32 Cells = 0;
				bool bEdge = false;
				Stack.Reset();
				Stack.Add(J0 * N + I0);
				Landmass[J0 * N + I0] = Id;
				while (!Stack.IsEmpty())
				{
					const int32 Cell = Stack.Pop();
					const int32 I = Cell % N, J = Cell / N;
					++Cells;
					bEdge = bEdge || I == 0 || J == 0 || I == N - 1 || J == N - 1;
					const int32 Neighbours[4] = { I > 0 ? Cell - 1 : -1, I < N - 1 ? Cell + 1 : -1, J > 0 ? Cell - N : -1, J < N - 1 ? Cell + N : -1 };
					for (const int32 Next : Neighbours)
					{
						if (Next >= 0 && Landmass[Next] < 0 && !IsWater(Ground[Next], Sea))
						{
							Landmass[Next] = Id;
							Stack.Add(Next);
						}
					}
				}
				LandmassKm2.Add((float)Cells * CellKm2);
				LandmassTouchesEdge.Add(bEdge ? 1 : 0);
			}
		}
	}
	auto IsMainland = [&](int32 I, int32 J)
	{
		const int32 Id = Landmass[J * N + I];
		return Id >= 0 && LandmassKm2[Id] >= Search.MinLandmassKm2 * (LandmassTouchesEdge[Id] ? 0.3f : 0.8f);
	};

	TArray<FCandidate> Candidates;
	for (int32 J = 2; J < N - 2; ++J)
	{
		for (int32 I = 2; I < N - 2; ++I)
		{
			if (At(I, J) < Sea + 100.0f || !IsMainland(I, J))
			{
				continue;   // a shore cell is LAND (of a landmass worth the name) ...
			}
			if (!IsWater(At(I - 1, J), Sea) && !IsWater(At(I + 1, J), Sea) && !IsWater(At(I, J - 1), Sea) && !IsWater(At(I, J + 1), Sea))
			{
				continue;   // ... with water next to it
			}

			// Per direction: unbroken water in front (the first land cell ends it; one cell of beach at the start is allowed), then the highest ground behind it.
			float FetchCells[16], FarRatio[16], FarReliefCm[16], FarDistCm[16];
			for (int32 D = 0; D < 16; ++D)
			{
				FetchCells[D] = 0.0f;
				FarRatio[D] = FarReliefCm[D] = FarDistCm[D] = 0.0f;
				int32 M = 1, Water = 0;
				bool bLeftBeach = false;
				for (; M <= MaxRayCells; ++M)
				{
					const int32 Ri = I + (int32)FMath::RoundToInt(GGrad2[D][0] * (float)M), Rj = J + (int32)FMath::RoundToInt(GGrad2[D][1] * (float)M);
					if (Ri < 0 || Rj < 0 || Ri >= N || Rj >= N)
					{
						break;
					}
					if (IsWater(At(Ri, Rj), Sea))
					{
						++Water;
						bLeftBeach = true;
					}
					else if (bLeftBeach || M > 1)
					{
						break;
					}
				}
				FetchCells[D] = (float)Water;
				if (Water >= MinFetchCells)
				{
					// The far shore as the eye sees it: what matters is how high the ground stands in the view (its elevation angle), not its height alone -- a
					// 200 m range at 5 km fills the horizon, the same range at 12 km is a smudge. Ratio = (height above the eye) / distance, best over the window.
					for (int32 K = M; K < M + FarWindowCells && K <= MaxRayCells; ++K)
					{
						const int32 Ri = I + (int32)FMath::RoundToInt(GGrad2[D][0] * (float)K), Rj = J + (int32)FMath::RoundToInt(GGrad2[D][1] * (float)K);
						if (Ri < 0 || Rj < 0 || Ri >= N || Rj >= N)
						{
							break;
						}
						const float Relief = At(Ri, Rj) - Sea;
						const float Distance = (float)K * GridStepCm;
						const float Ratio = (Relief - EyeAboveSeaCm) / Distance;
						if (Ratio > FarRatio[D])
						{
							FarRatio[D] = Ratio;
							FarReliefCm[D] = Relief;
							FarDistCm[D] = Distance;
						}
					}
				}
			}

			// A direction is a view when its +-22.5 degree neighbours are open water as well (a river or an inlet is not).
			const float DistCm = (float)FMath::Sqrt((double)((I - Half) * (I - Half) + (J - Half) * (J - Half))) * GridStepCm;
			FCandidate Best;
			Best.Score = -TNumericLimits<float>::Max();
			for (int32 D = 0; D < 16; ++D)
			{
				const float Open = FMath::Min3(FetchCells[WrapDir(D - 1)], FetchCells[D], FetchCells[WrapDir(D + 1)]);
				if (Open < (float)MinFetchCells)
				{
					continue;
				}
				int32 BestRay = D;
				for (const int32 Neighbour : { WrapDir(D - 1), WrapDir(D + 1) })
				{
					BestRay = FarRatio[Neighbour] > FarRatio[BestRay] ? Neighbour : BestRay;
				}
				const float OpenCm = Open * GridStepCm;
				const float Score = 1.0f * FMath::Clamp(FarRatio[BestRay] / Search.FarElevationCap, 0.0f, 1.0f)
					+ 0.5f * FMath::Clamp((OpenCm - 200000.0f) / 600000.0f, 0.0f, 1.0f)
					- 0.3f * FMath::Clamp(DistCm / 2400000.0f, 0.0f, 1.0f);
				if (Score > Best.Score)
				{
					Best.Score = Score;
					Best.OpenWaterCm = OpenCm;
					Best.FarReliefCm = FarReliefCm[BestRay];
					Best.FarDistanceCm = FarDistCm[BestRay];
					Best.CellX = I;
					Best.CellY = J;
					Best.Dir = BestRay;   // look at the best of the three open directions: the one with the highest skyline
				}
			}
			if (Best.Score > -TNumericLimits<float>::Max())
			{
				Candidates.Add(Best);
			}
		}
	}
	if (Candidates.IsEmpty())
	{
		return false;
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.Score != B.Score)
		{
			return A.Score > B.Score;
		}
		return A.CellY != B.CellY ? A.CellY < B.CellY : A.CellX < B.CellX;   // total order: ties must not depend on the sort's internals
	});

	// ---- stage 2: refine on the real terrain ---------------------------------------------------------------------------------------------------------------------
	auto Real = [&](double X, double Y) { return SampleHeightCm(Params, X, Y, Seed, T.VoxelSizeCm); };
	const int32 Tries = FMath::Min(Candidates.Num(), MaxRefineTries);
	for (int32 C = 0; C < Tries; ++C)
	{
		const FCandidate& Cand = Candidates[C];
		const double Ox = (double)(Cand.CellX - Half) * (double)GridStepCm, Oy = (double)(Cand.CellY - Half) * (double)GridStepCm;
		const double Dx = (double)GGrad2[Cand.Dir][0], Dy = (double)GGrad2[Cand.Dir][1];
		const double Px = -Dy, Py = Dx;   // perpendicular (left of the view direction)

		if (IsWater(Real(Ox, Oy), Sea))
		{
			continue;   // the coarse grid called this cell land; the real terrain disagrees
		}

		// The waterline: first real water sample along the view direction within two cells.
		double Wx = 0.0, Wy = 0.0;
		bool bFound = false;
		for (double S = 500.0; S <= 2.0 * (double)GridStepCm; S += 500.0)
		{
			if (IsWater(Real(Ox + Dx * S, Oy + Dy * S), Sea))
			{
				Wx = Ox + Dx * S;
				Wy = Oy + Dy * S;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			continue;
		}
		// The island test runs on two lattices (200 m and 300 m): a landmass joined to the rest only by a neck one cell wide at the finer lattice is cut by the coarser one, and
		// then it does not count as joined. FineLandmass = -1: not measured yet for this candidate (every spawn point tried below is within ~200 m of the others).
		int32 FineLandmass = -1;
		bool bMainland = false;
		const float LatticesCm[3] = { 17500.0f, 25000.0f, 35000.0f };
		static constexpr float Setbacks[] = { 5000.0f, 3000.0f, 8000.0f, 12000.0f, 2000.0f, 18000.0f };
		static constexpr float Laterals[] = { 0.0f, 4000.0f, -4000.0f };
		for (const float Setback : Setbacks)
		{
			for (const float Lateral : Laterals)
			{
				const double Sx = Wx - Dx * (double)Setback + Px * (double)Lateral, Sy = Wy - Dy * (double)Setback + Py * (double)Lateral;
				const float H = Real(Sx, Sy);
				if (H < Sea + 150.0f || H > Sea + 3000.0f)
				{
					continue;   // wet sand, or a cliff top
				}
				const float Gx = (Real(Sx + 150.0, Sy) - Real(Sx - 150.0, Sy)) / 300.0f, Gy = (Real(Sx, Sy + 150.0) - Real(Sx, Sy - 150.0)) / 300.0f;
				if (Gx * Gx + Gy * Gy > 0.25f)
				{
					continue;   // steeper than ~26 degrees
				}
				if (DistanceToSinkholeRimZone(Params, Sx, Sy, Seed) < 3000.0f || IsInUplandLakeZone(Params, Sx, Sy, Seed, 2.6f))
				{
					continue;
				}
				// What the player will see from HERE: water within 400 m ahead, and 85% water over the next 1.5 km -- on the real terrain (the 400 m grid can smear islets
				// and spits away, and the lateral offset moves the view line).
				double FirstWater = -1.0;
				for (double Along = 0.0; Along <= 40000.0; Along += 500.0)
				{
					if (IsWater(Real(Sx + Dx * Along, Sy + Dy * Along), Sea))
					{
						FirstWater = Along;
						break;
					}
				}
				if (FirstWater < 0.0)
				{
					continue;
				}
				int32 Wet = 0;
				for (int32 K = 0; K <= 60; ++K)   // 25 m steps over 1.5 km: fine enough to see a spit or a rock
				{
					const double Along = FirstWater + 2500.0 * (double)K;
					Wet += IsWater(Real(Sx + Dx * Along, Sy + Dy * Along), Sea) ? 1 : 0;
				}
				if (Wet * 100 < 61 * 85)
				{
					continue;
				}
				// Room to stand: not a spit or a one-tree headland. Of 16 points on rings 150 m and 300 m around the spawn at least 10 must be land (the water ahead takes a few).
				int32 DryAround = 0;
				for (int32 Ring = 1; Ring <= 2; ++Ring)
				{
					for (int32 D = 0; D < 16; D += 2)
					{
						DryAround += IsWater(Real(Sx + (double)GGrad2[D][0] * 15000.0 * Ring, Sy + (double)GGrad2[D][1] * 15000.0 * Ring), Sea) ? 0 : 1;
					}
				}
				if (DryAround < 10)
				{
					continue;
				}
				// Never an island: the real terrain, flood-filled on a 200 m lattice, must hold a landmass of at least MinLandmassKm2 under this point.
				if (FineLandmass < 0)
				{
					bMainland = true;
					for (const float LatticeCm : LatticesCm)
					{
						const int32 NeedCells = FMath::Max((int32)(Search.MinLandmassKm2 / FMath::Square(LatticeCm / 100000.0f)), 1);
						bMainland = bMainland && MeasureLandmassCells(Params, Seed, Sx, Sy, LatticeCm, NeedCells) >= NeedCells;
					}
					FineLandmass = bMainland ? 1 : 0;
				}
				if (!bMainland)
				{
					break;   // an island: no spawn point of this candidate will do
				}
				Out.LandmassKm2 = Search.MinLandmassKm2;
				Out.Location = FVector2D(Sx, Sy);
				Out.GroundCm = H;
				Out.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2((float)Dy, (float)Dx));
				Out.OpenWaterCm = Cand.OpenWaterCm;
				Out.FarReliefCm = Cand.FarReliefCm;
				Out.FarDistanceCm = Cand.FarDistanceCm;
				Out.DistanceFromOriginCm = (float)FMath::Sqrt(Sx * Sx + Sy * Sy);
				Out.Score = Cand.Score;
				return true;
			}
			if (FineLandmass == 0)
			{
				break;   // ... and the setbacks are no different: next candidate
			}
		}
	}
	return false;
}

int32 FMOWorldGen::MeasureLandmassCells(const FMOWorldGenParams& Params, int32 Seed, double X, double Y, float CellCm, int32 MaxCells)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	const double Limit = (double)T.WorldHalfSizeCm;
	const float StepCm = FMath::Max(0.5f * CellCm, T.VoxelSizeCm);   // octaves finer than the lattice cannot matter to a land / water call at this spacing
	auto IsLand = [&](int32 I, int32 J)
	{
		const double Px = X + (double)I * (double)CellCm, Py = Y + (double)J * (double)CellCm;
		return FMath::Abs(Px) <= Limit && FMath::Abs(Py) <= Limit && !IsWater(SampleBaseHeightCm(Params, Px, Py, Seed, StepCm), T.SeaLevelCm);
	};
	if (MaxCells <= 0 || !IsLand(0, 0))
	{
		return 0;
	}
	auto Key = [](int32 I, int32 J) { return ((int64)(I + (1 << 20)) << 21) | (int64)(J + (1 << 20)); };
	TSet<int64> Seen;
	TArray<FIntPoint> Stack;
	Seen.Add(Key(0, 0));
	Stack.Add(FIntPoint(0, 0));
	int32 Count = 0;
	static constexpr int32 Step8[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
	while (!Stack.IsEmpty() && Count < MaxCells)
	{
		const FIntPoint C = Stack.Pop();
		++Count;
		for (const int32(&D)[2] : Step8)
		{
			const int32 I = C.X + D[0], J = C.Y + D[1];
			bool bAlready = false;
			Seen.Add(Key(I, J), &bAlready);
			if (!bAlready && IsLand(I, J))
			{
				Stack.Add(FIntPoint(I, J));
			}
		}
	}
	return Count;
}
