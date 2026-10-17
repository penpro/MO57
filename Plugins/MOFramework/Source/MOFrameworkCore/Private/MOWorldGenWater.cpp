// Water half of FMOWorldGen: rivers and lake basins carved into LOW ground, always at sea level. Same rules as MOWorldGen.h (pure, deterministic, thread-safe,
// IEEE-exact ops only, no sin/cos). Design: Docs/Terrain_Generation_Exploration.md ("Water").
//
// There is no water geometry here. The world already has a water plane at SeaLevelCm (WaterBodyCustom): anything below it is under water. So a river is a channel
// whose bed is under sea level and whose banks rise above it; a lake is a bowl. Water never needs a level of its own, flows nowhere, and cannot leak.
//
// UPLAND LAKES (tier 2) are the exception: they sit ABOVE sea level, so the sea plane cannot fill them and each has a water surface of its own (FLake::SurfaceCm).
// The site is a low spot: a few jittered candidates per cell roll downhill, and the flattest one whose footprint is high, level and walled-in enough wins. The surface
// is the lowest ground around the lake minus a freeboard, so the whole shore is a dry bank (a lake never hangs over lower ground) and a square water plane laid over
// the ellipse stays hidden under the terrain outside it. The terrain inside the ellipse is carved into a bowl below the surface.

#include "MOWorldGen.h"
#include "MOWorldGenNoise.h"

using namespace MOWorldGenNoise;

namespace
{
	enum : uint32
	{
		SaltRiverA = 0x8101u, SaltRiverB = 0x8102u, SaltRiverWidth = 0x8103u, SaltBasin = 0x8201u, SaltUpland = 0x8301u, SaltLakeBed = 0x8401u
	};

	/** 0 = pond tier, 1 = lake tier. */
	struct FTier
	{
		float CellCm, Chance, RadiusMinCm, RadiusMaxCm;
	};

	FTier TierOf(const FMOWorldGenTuning& T, int32 Tier)
	{
		return Tier == 0 ? FTier{ T.PondCellCm, T.PondChance, T.PondRadiusMinCm, T.PondRadiusMaxCm } : FTier{ T.LakeCellCm, T.LakeChance, T.LakeRadiusMinCm, T.LakeRadiusMaxCm };
	}


	/** Bank reach: a basin is only evaluated out to this many radii (beyond it the ground is untouched). The cell jitter margin (20%) must cover it. */
	constexpr float BasinReachRadii = 2.5f;

	/**
	 * The upland lake site of this cell (tier 2): see the header comment. false = this cell has none. Deterministic in (params, seed, cell) only.
	 */
	bool ComputeUplandLake(const FMOWorldGenParams& P, int64 CellX, int64 CellY, int32 Seed, FMOWorldGen::FLake& Out)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		if (!T.bEnableUplandLakes)
		{
			return false;
		}
		const double Cell = (double)T.UplandLakeCellCm;
		// The freeboard is the margin that keeps the water plane buried between the footprint samples; the ground's small-scale roughness (detail octaves, slopes) grows with
		// VerticalScale, so the margin does too.
		const float Freeboard = T.UplandLakeFreeboardCm * T.VerticalScale;
		const uint32 S = (uint32)FMOWorldGen::HashSeed(Seed, SaltUpland);
		if (Unit16(HashLattice2(CellX, CellY, S)) >= T.UplandLakeChance)
		{
			return false;
		}

		// The centre stays 0.25 cell from every cell border (Finalize caps the radius so 2.5 radii of reach fit inside that margin).
		const double Lo[2] = { ((double)CellX + 0.25) * Cell, ((double)CellY + 0.25) * Cell };
		const double Hi[2] = { ((double)CellX + 0.75) * Cell, ((double)CellY + 0.75) * Cell };
		auto Base = [&](double X, double Y) { return FMOWorldGen::SampleBaseHeightCm(P, X, Y, Seed, T.VoxelSizeCm); };

		bool bHave = false;
		float BestRelief = TNumericLimits<float>::Max();
		for (int32 K = 0; K < T.UplandLakeCandidates; ++K)
		{
			const uint32 SK = S + 16u * (uint32)(K + 1);
			const uint32 H1 = HashLattice2(CellX, CellY, SK + 1u);
			const uint32 H2 = HashLattice2(CellX, CellY, SK + 2u);
			FMOWorldGen::FLake L;
			L.CellX = CellX;
			L.CellY = CellY;
			L.RadiusCm = T.UplandLakeRadiusMinCm + (T.UplandLakeRadiusMaxCm - T.UplandLakeRadiusMinCm) * Unit16(H2);
			L.Aspect = 0.6f + 0.4f * Unit16(H2 >> 16);
			const float* Dir = GGrad2[HashLattice2(CellX, CellY, SK + 3u) & 15u];
			L.CosAngle = Dir[0];
			L.SinAngle = Dir[1];
			L.DepthCm = T.UplandLakeDepthBaseCm + T.UplandLakeDepthPerRadius * L.RadiusCm;

			// 1. Roll downhill from a jittered start: a few steps of "move to the lowest of 8 neighbours", so the lake settles into the low spot nearby.
			double Px = Lo[0] + (Hi[0] - Lo[0]) * (double)Unit16(H1), Py = Lo[1] + (Hi[1] - Lo[1]) * (double)Unit16(H1 >> 16);
			float Hc = Base(Px, Py);
			const double StepCm = 0.6 * (double)L.RadiusCm;
			for (int32 Iter = 0; Iter < 4; ++Iter)
			{
				double BestX = Px, BestY = Py;
				float BestH = Hc - 1.0f;   // a move must go down by more than a centimetre: no dithering on a plateau
				for (int32 D = 0; D < 16; D += 2)
				{
					const double Nx = FMath::Clamp(Px + StepCm * (double)GGrad2[D][0], Lo[0], Hi[0]);
					const double Ny = FMath::Clamp(Py + StepCm * (double)GGrad2[D][1], Lo[1], Hi[1]);
					const float Hn = Base(Nx, Ny);
					if (Hn < BestH)
					{
						BestH = Hn;
						BestX = Nx;
						BestY = Ny;
					}
				}
				if (BestX == Px && BestY == Py)
				{
					break;
				}
				Px = BestX;
				Py = BestY;
				Hc = BestH;
			}
			L.Center = FVector2D(Px, Py);

			// 2. The footprint: ground in the lake's own frame (u along the long axis, v across), as multiples of the radii.
			auto GroundAt = [&](float U, float V)
			{
				const float Lx = U * L.RadiusCm, Ly = V * L.RadiusCm * L.Aspect;
				return Base(Px + (double)(L.CosAngle * Lx - L.SinAngle * Ly), Py + (double)(L.SinAngle * Lx + L.CosAngle * Ly));
			};
			float MinOutside = TNumericLimits<float>::Max(), MaxAll = -TNumericLimits<float>::Max();
			auto Take = [&](float Ground, bool bOutside)
			{
				MaxAll = FMath::Max(MaxAll, Ground);
				if (bOutside)
				{
					MinOutside = FMath::Min(MinOutside, Ground);
				}
			};
			constexpr int32 Grid = 9;
			constexpr float Reach = 1.05f;   // the water square is the +-1 box; the margin covers the plane's edge
			for (int32 J = 0; J < Grid; ++J)
			{
				for (int32 I = 0; I < Grid; ++I)
				{
					const float U = Reach * (2.0f * (float)I / (float)(Grid - 1) - 1.0f), V = Reach * (2.0f * (float)J / (float)(Grid - 1) - 1.0f);
					Take(GroundAt(U, V), U * U + V * V >= 0.9f);
				}
			}
			for (int32 D = 0; D < 16; ++D)
			{
				Take(GroundAt(GGrad2[D][0], GGrad2[D][1]), true);                       // the shore itself
				Take(GroundAt(1.2f * GGrad2[D][0], 1.2f * GGrad2[D][1]), true);         // just beyond it
			}
			if (MinOutside < T.SeaLevelCm + T.UplandLakeMinGroundAboveSeaCm)
			{
				continue;   // too low: that is river / sea-level-basin country
			}
			L.SurfaceCm = MinOutside - Freeboard;
			const float Relief = MaxAll - L.SurfaceCm;
			if (Relief > T.UplandLakeMaxReliefCm + Freeboard || Relief >= BestRelief)
			{
				continue;   // a hillside, or a worse spot than one already found in this cell
			}

			// 2b. The sparse grid above can miss a dip between its samples (the v6 foothills have crags and gullies): the whole +-1 box outside the ellipse must stay buried 30 cm. Re-check it on a finer
			// grid (only for a candidate that got this far) and sink the surface under the lowest ground if needed -- the freeboard is that margin; a lake whose box showed a bare water plane over a dip
			// was found by MOFramework.WorldGen.UplandLakes (1 lake in 2470).
			{
				constexpr int32 DenseGrid = 17;
				float MinDense = TNumericLimits<float>::Max();
				for (int32 J = 0; J < DenseGrid; ++J)
				{
					for (int32 I = 0; I < DenseGrid; ++I)
					{
						const float U = 2.0f * (float)I / (float)(DenseGrid - 1) - 1.0f, V = 2.0f * (float)J / (float)(DenseGrid - 1) - 1.0f;
						if (U * U + V * V >= 1.0f)
						{
							MinDense = FMath::Min(MinDense, GroundAt(U, V));
						}
					}
				}
				// The water plane is the world-aligned bounding box of the ellipse (FLake::PlaneHalfSizeCm), not the rotated box above: its corners reach ground the rotated box never looked at
				// (46 of 2470 lakes showed water over low ground there, MOFramework.WorldGen.UplandLakePlane). Bury that rectangle too, plus 3 m: the plane is whole tiles from a snapped centre.
				{
					const FVector2D Half = L.PlaneHalfSizeCm() + FVector2D(300.0, 300.0);
					constexpr int32 PlaneGrid = 25;
					for (int32 J = 0; J < PlaneGrid; ++J)
					{
						for (int32 I = 0; I < PlaneGrid; ++I)
						{
							const double Dx = Half.X * (2.0 * (double)I / (double)(PlaneGrid - 1) - 1.0), Dy = Half.Y * (2.0 * (double)J / (double)(PlaneGrid - 1) - 1.0);
							const float Lx = (float)((double)L.CosAngle * Dx + (double)L.SinAngle * Dy) / L.RadiusCm, Ly = (float)(-(double)L.SinAngle * Dx + (double)L.CosAngle * Dy) / (L.RadiusCm * L.Aspect);
							if (Lx * Lx + Ly * Ly >= 1.0f)
							{
								MinDense = FMath::Min(MinDense, Base(Px + Dx, Py + Dy) - 30.0f);   // an extra 30 cm: this grid is coarser than the ground's crags (one lake in 2464 still showed 15 cm of ground out of the water at 30)
							}
						}
					}
				}
				if (MinDense < L.SurfaceCm + 30.0f)
				{
					L.SurfaceCm = MinDense - 30.0f;
					if (L.SurfaceCm < T.SeaLevelCm + T.UplandLakeMinGroundAboveSeaCm - Freeboard - 1.0f || MaxAll - L.SurfaceCm > T.UplandLakeMaxReliefCm + Freeboard || MaxAll - L.SurfaceCm >= BestRelief)
					{
						continue;   // sunk too far: no longer a fit lake site
					}
				}
			}

			// 3. Walled in: at the outer reach of the bank the ground must still be under the bank (else the bank would end in a cliff).
			bool bWalled = true;
			const float BankAtReach = L.SurfaceCm + 0.85f * T.BasinBankSlope * L.RadiusCm * (BasinReachRadii - 1.0f);
			for (int32 D = 0; D < 16 && bWalled; ++D)
			{
				bWalled = GroundAt(BasinReachRadii * GGrad2[D][0], BasinReachRadii * GGrad2[D][1]) <= BankAtReach;
			}
			if (!bWalled)
			{
				continue;
			}
			BestRelief = MaxAll - L.SurfaceCm;   // the final surface (2b may have sunk it)
			Out = L;
			bHave = true;
		}
		return bHave;
	}

	/**
	 * The cell's basin (cheap hashing) and whether the land under it is fit for one: the whole rim must sit between BasinRimMin and BasinRimMax above sea level
	 * (tiers 0 and 1), or the footprint must be a high, level low spot (tier 2).
	 * Memoised per thread -- a voxel chunk asks for the same cell thousands of times and the fit check costs dozens to hundreds of base-height samples. A pure function
	 * of (params, seed, tier, cell): the cache only ever skips recomputing, it cannot change a result. 256 entries: a coarse-LOD chunk spans many cells.
	 */
	bool ResolveBasin(const FMOWorldGenParams& P, int32 Tier, int64 CellX, int64 CellY, int32 Seed, FMOWorldGen::FLake& Out)
	{
		struct FEntry
		{
			int64 CellX = 0, CellY = 0;
			uint32 Seed = 0, ParamsHash = 0;
			int32 Tier = -1;
			bool bValid = false, bHasLake = false;
			FMOWorldGen::FLake Lake;
		};
		// One direct-mapped table PER TIER. Every height sample looks up three basins (tiers 0, 1 and 2 via ApplyWater); with one shared table, a tier-2 lake
		// cell and a tier-0/1 cell that hash to the same slot evicted each other on EVERY sample, so each point recomputed the upland lake
		// (ComputeUplandLake: hundreds of base-height samples) -- 304 us a height sample in one 500 m cell of seed 1029024224 against 0.6 elsewhere, a
		// 550 ms spawner hitch in every packaged perf tour (Oct 13 2026; MOFramework.PCG.SampleCosts "SLOW CELL"). Separate tables: the three lookups of a
		// sample can no longer collide.
		thread_local FEntry Cache[3][256];

		const uint32 Slot = (uint32)(((uint64)CellX * 73856093ull) ^ ((uint64)CellY * 19349663ull)) & 255u;
		FEntry& E = Cache[FMath::Clamp(Tier, 0, 2)][Slot];
		if (E.bValid && E.CellX == CellX && E.CellY == CellY && E.Tier == Tier && E.Seed == (uint32)Seed && E.ParamsHash == P.GetHash())
		{
			Out = E.Lake;
			return E.bHasLake;
		}

		E.CellX = CellX;
		E.CellY = CellY;
		E.Tier = Tier;
		E.Seed = (uint32)Seed;
		E.ParamsHash = P.GetHash();
		E.bValid = true;
		E.bHasLake = false;

		const FMOWorldGenTuning& T = P.Tuning;
		if (Tier == 2)
		{
			E.bHasLake = ComputeUplandLake(P, CellX, CellY, Seed, E.Lake);
			Out = E.Lake;
			return E.bHasLake;
		}

		const FTier Cfg = TierOf(T, Tier);
		const uint32 S = (uint32)FMOWorldGen::HashSeed(Seed, SaltBasin + (uint32)Tier * 0x101u);
		if (Unit16(HashLattice2(CellX, CellY, S)) < Cfg.Chance)
		{
			const uint32 H1 = HashLattice2(CellX, CellY, S + 1u);
			const uint32 H2 = HashLattice2(CellX, CellY, S + 2u);
			FMOWorldGen::FLake L;
			L.CellX = CellX;
			L.CellY = CellY;
			L.Center = FVector2D(((double)CellX + (double)(0.22f + 0.56f * Unit16(H1))) * (double)Cfg.CellCm, ((double)CellY + (double)(0.22f + 0.56f * Unit16(H1 >> 16))) * (double)Cfg.CellCm);
			L.RadiusCm = Cfg.RadiusMinCm + (Cfg.RadiusMaxCm - Cfg.RadiusMinCm) * Unit16(H2);
			L.Aspect = 0.6f + 0.4f * Unit16(H2 >> 16);
			const float* Dir = GGrad2[HashLattice2(CellX, CellY, S + 3u) & 15u];   // the long axis points along one of 16 literal unit directions
			L.CosAngle = Dir[0];
			L.SinAngle = Dir[1];
			L.DepthCm = T.BasinDepthBaseCm + T.BasinDepthPerRadius * L.RadiusCm;
			L.SurfaceCm = T.SeaLevelCm;

			// The whole rim must be low land: rim samples at 16 literal directions, scaled into the ellipse.
			bool bFit = true;
			for (int32 I = 0; I < 16 && bFit; ++I)
			{
				const float Lx = GGrad2[I][0] * L.RadiusCm, Ly = GGrad2[I][1] * L.RadiusCm * L.Aspect;
				const double Wx = L.Center.X + (double)(L.CosAngle * Lx - L.SinAngle * Ly), Wy = L.Center.Y + (double)(L.SinAngle * Lx + L.CosAngle * Ly);
				const float Above = FMOWorldGen::SampleBaseHeightCm(P, Wx, Wy, Seed, T.VoxelSizeCm) - T.SeaLevelCm;
				bFit = Above >= T.BasinRimMinAboveSeaCm && Above <= T.BasinRimMaxAboveSeaCm;
			}
			if (bFit)
			{
				E.bHasLake = true;
				E.Lake = L;
			}
		}
		Out = E.Lake;
		return E.bHasLake;
	}

	/** The basin height at (X,Y) for the basin in this tier's cell: a bowl under the water surface inside the shore (q < 1), a bank rising from it outside. +inf if none. */
	float BasinHeight(const FMOWorldGenParams& P, int32 Tier, double X, double Y, int32 Seed)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		const float Cell = Tier == 2 ? T.UplandLakeCellCm : TierOf(T, Tier).CellCm;
		const int64 CellX = (int64)FMath::FloorToDouble(X / (double)Cell), CellY = (int64)FMath::FloorToDouble(Y / (double)Cell);
		FMOWorldGen::FLake L;
		if (!ResolveBasin(P, Tier, CellX, CellY, Seed, L))
		{
			return TNumericLimits<float>::Max();
		}
		const float Dx = (float)(X - L.Center.X), Dy = (float)(Y - L.Center.Y);
		const float Lx = (L.CosAngle * Dx + L.SinAngle * Dy) / L.RadiusCm;
		const float Ly = (-L.SinAngle * Dx + L.CosAngle * Dy) / (L.RadiusCm * L.Aspect);
		const float Q2 = Lx * Lx + Ly * Ly;
		if (Q2 >= BasinReachRadii * BasinReachRadii)
		{
			return TNumericLimits<float>::Max();
		}
		if (Q2 < 1.0f)
		{
			return L.SurfaceCm - L.DepthCm * (1.0f - Q2);
		}
		return L.SurfaceCm + T.BasinBankSlope * L.RadiusCm * (FMath::Sqrt(Q2) - 1.0f);
	}

	/** The river bed height at (X,Y): a channel along the zero contour of a smooth field, fading out as the ground rises. +inf away from any river. */
	float RiverHeight(const FMOWorldGenParams& P, double X, double Y, int32 Seed, float BaseHeightCm)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		const float AboveSea = BaseHeightCm - T.SeaLevelCm;
		// Headwaters: the channel fades out between RiverMaxGroundAboveSeaCm and 6 m higher.
		const float Lowland = 1.0f - Smooth01(T.RiverMaxGroundAboveSeaCm, T.RiverMaxGroundAboveSeaCm + 600.0f, AboveSea);
		if (Lowland <= 0.0f)
		{
			return TNumericLimits<float>::Max();
		}

		const double Inv = 1.0 / (double)T.RiverWavelengthCm;
		const double Px = X * Inv, Py = Y * Inv;
		const FNoise2 A = GradientNoise2(Px, Py, (uint32)FMOWorldGen::HashSeed(Seed, SaltRiverA));
		const FNoise2 B = GradientNoise2(Px * 2.0 + 3.1, Py * 2.0 - 1.7, (uint32)FMOWorldGen::HashSeed(Seed, SaltRiverB));
		const float F = A.Value + 0.35f * B.Value;                          // the field: rivers run where it crosses zero
		const float Gx = A.Dx + 0.7f * B.Dx, Gy = A.Dy + 0.7f * B.Dy;     // d(field)/d(lattice unit)
		// First-order distance to the contour = |field| / |gradient| (per cm). Flat spots (tiny gradient) would make rivers swell, so the gradient is floored.
		const float Grad = FMath::Max(FMath::Sqrt(Gx * Gx + Gy * Gy), 0.4f) * (float)Inv;
		const float Dist = FMath::Abs(F) / Grad;

		const float WidthMod = 0.7f + 0.3f * GradientNoise2(Px * 0.9 - 7.3, Py * 0.9 + 2.9, (uint32)FMOWorldGen::HashSeed(Seed, SaltRiverWidth)).Value;
		const float Half = FMath::Max(0.5f * T.RiverWidthCm * WidthMod, 50.0f);

		float Floor;
		if (Dist <= Half)
		{
			const float R = Dist / Half;
			Floor = T.SeaLevelCm - T.RiverDepthCm * (1.0f - 0.6f * R * R);   // deepest in the middle, 40% of the depth at the water line
		}
		else
		{
			Floor = T.SeaLevelCm - 0.4f * T.RiverDepthCm + T.RiverBankSlope * (Dist - Half);
		}
		// Push the channel out of the way as the ground rises past the headwater height (continuous: a hard cut would be a cliff).
		return Floor + (1.0f - Lowland) * 3000.0f;
	}
}

float FMOWorldGen::ApplyWater(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float BaseHeightCm, float StepCm)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableWater)
	{
		return BaseHeightCm;
	}
	float H = BaseHeightCm;

	// Sea-level water only exists on low ground. Everything above the river headwater fade and the basin rim limit is left alone (the common case, ~free).
	const float AboveSea = BaseHeightCm - T.SeaLevelCm;
	// A basin bank rises at most BasinBankSlope x radius x (reach - 1) above sea level before the ground beyond it is left alone.
	const float HighestBank = T.BasinBankSlope * FMath::Max(T.LakeRadiusMaxCm, T.PondRadiusMaxCm) * (BasinReachRadii - 1.0f);
	if (AboveSea <= FMath::Max(T.RiverMaxGroundAboveSeaCm + 600.0f, HighestBank))
	{
		H = FMath::Min(H, RiverHeight(Params, X, Y, Seed, BaseHeightCm));
		// A coarse LOD skips the basins it cannot resolve (each one is a cache-missing cell lookup there, and sub-voxel anyway).
		if (IsWaterTierResolved(Params, 0, StepCm))
		{
			H = FMath::Min(H, BasinHeight(Params, 0, X, Y, Seed));
		}
		if (IsWaterTierResolved(Params, 1, StepCm))
		{
			H = FMath::Min(H, BasinHeight(Params, 1, X, Y, Seed));
		}
	}

	// Upland lakes: a basin only ever lowers ground down to its own floor, and no floor is lower than this -- ground under it needs no lookup.
	if (T.bEnableUplandLakes && IsWaterTierResolved(Params, 2, StepCm))
	{
		const float LowestUplandFloor = T.UplandLakeMinGroundAboveSeaCm - T.UplandLakeFreeboardCm * T.VerticalScale - (T.UplandLakeDepthBaseCm + T.UplandLakeDepthPerRadius * T.UplandLakeRadiusMaxCm);
		if (AboveSea > LowestUplandFloor)
		{
			H = FMath::Min(H, BasinHeight(Params, 2, X, Y, Seed));
		}
	}
	return H;
}

EMOWaterBodyKind FMOWorldGen::ClassifyWaterBody(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableWater)
	{
		return EMOWaterBodyKind::None;
	}
	float UplandSurface = 0.0f;
	if (GetWaterSurfaceCm(Params, X, Y, Seed, UplandSurface) && UplandSurface != T.SeaLevelCm)
	{
		return EMOWaterBodyKind::UplandLake;   // inside an upland lake's shore: its own surface (GetWaterSurfaceCm answers the sea plane everywhere else)
	}
	// Everything else is water only where the ground lies under the sea plane -- the same carving ApplyWater does, at the terrain's own resolution.
	const float Base = SampleBaseHeightCm(Params, X, Y, Seed, T.VoxelSizeCm);
	if (Base < T.SeaLevelCm)
	{
		return EMOWaterBodyKind::Sea;   // the land itself lies below sea level: the ocean basin
	}
	if (ApplyWater(Params, X, Y, Seed, Base, T.VoxelSizeCm) >= T.SeaLevelCm)
	{
		return EMOWaterBodyKind::None;
	}
	// Carved below the plane: whichever channel or basin reaches deepest here owns the water.
	const float River = RiverHeight(Params, X, Y, Seed, Base);
	const float Pond = BasinHeight(Params, 0, X, Y, Seed);
	const float Lake = BasinHeight(Params, 1, X, Y, Seed);
	if (River <= Pond && River <= Lake)
	{
		return EMOWaterBodyKind::River;
	}
	return Pond <= Lake ? EMOWaterBodyKind::Pond : EMOWaterBodyKind::Lake;
}

bool FMOWorldGen::GetLakeInCell(const FMOWorldGenParams& Params, int32 Tier, int64 CellX, int64 CellY, int32 Seed, FLake& Out)
{
	return Params.Tuning.bEnableWater && (Tier >= 0 && Tier <= 2) && ResolveBasin(Params, Tier, CellX, CellY, Seed, Out);
}

void FMOWorldGen::GetUplandLakesNear(const FMOWorldGenParams& Params, double X, double Y, double RadiusCm, int32 Seed, TArray<FLake>& Out)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableWater || !T.bEnableUplandLakes)
	{
		return;
	}
	const double Cell = (double)T.UplandLakeCellCm;
	const int64 X0 = (int64)FMath::FloorToDouble((X - RadiusCm) / Cell), X1 = (int64)FMath::FloorToDouble((X + RadiusCm) / Cell);
	const int64 Y0 = (int64)FMath::FloorToDouble((Y - RadiusCm) / Cell), Y1 = (int64)FMath::FloorToDouble((Y + RadiusCm) / Cell);
	for (int64 Cy = Y0; Cy <= Y1; ++Cy)
	{
		for (int64 Cx = X0; Cx <= X1; ++Cx)
		{
			FLake L;
			if (ResolveBasin(Params, 2, Cx, Cy, Seed, L) && FVector2D::Distance(FVector2D(X, Y), L.Center) - (double)L.RadiusCm <= RadiusCm)
			{
				Out.Add(L);
			}
		}
	}
}

bool FMOWorldGen::IsInUplandLakeZone(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float ZoneRadii)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableWater || !T.bEnableUplandLakes)
	{
		return false;
	}
	const double Cell = (double)T.UplandLakeCellCm;
	FLake L;
	if (!ResolveBasin(Params, 2, (int64)FMath::FloorToDouble(X / Cell), (int64)FMath::FloorToDouble(Y / Cell), Seed, L))
	{
		return false;
	}
	const float Dx = (float)(X - L.Center.X), Dy = (float)(Y - L.Center.Y);
	const float Lx = (L.CosAngle * Dx + L.SinAngle * Dy) / L.RadiusCm;
	const float Ly = (-L.SinAngle * Dx + L.CosAngle * Dy) / (L.RadiusCm * L.Aspect);
	return Lx * Lx + Ly * Ly <= ZoneRadii * ZoneRadii;
}

float FMOWorldGen::UplandLakeShoreDistanceCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableWater || !T.bEnableUplandLakes)
	{
		return 1.0e9f;
	}
	const double Cell = (double)T.UplandLakeCellCm;
	FLake L;
	if (!ResolveBasin(Params, 2, (int64)FMath::FloorToDouble(X / Cell), (int64)FMath::FloorToDouble(Y / Cell), Seed, L))
	{
		return 1.0e9f;
	}
	const float Dx = (float)(X - L.Center.X), Dy = (float)(Y - L.Center.Y);
	return FMath::Sqrt(Dx * Dx + Dy * Dy) - L.RadiusCm;
}

bool FMOWorldGen::GetWaterSurfaceCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float& OutSurfaceCm)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableWater)
	{
		return false;
	}
	if (T.bEnableUplandLakes)
	{
		const double Cell = (double)T.UplandLakeCellCm;
		FLake L;
		if (ResolveBasin(Params, 2, (int64)FMath::FloorToDouble(X / Cell), (int64)FMath::FloorToDouble(Y / Cell), Seed, L))
		{
			const float Dx = (float)(X - L.Center.X), Dy = (float)(Y - L.Center.Y);
			const float Lx = (L.CosAngle * Dx + L.SinAngle * Dy) / L.RadiusCm;
			const float Ly = (-L.SinAngle * Dx + L.CosAngle * Dy) / (L.RadiusCm * L.Aspect);
			if (Lx * Lx + Ly * Ly <= 1.0f)
			{
				OutSurfaceCm = L.SurfaceCm;
				return true;
			}
		}
	}
	OutSurfaceCm = T.SeaLevelCm;
	return true;
}

float FMOWorldGen::UplandLakeBedWeight(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableWater || !T.bEnableUplandLakes)
	{
		return 0.0f;
	}
	const double Cell = (double)T.UplandLakeCellCm;
	FLake L;
	if (!ResolveBasin(Params, 2, (int64)FMath::FloorToDouble(X / Cell), (int64)FMath::FloorToDouble(Y / Cell), Seed, L))
	{
		return 0.0f;
	}
	const float Dx = (float)(X - L.Center.X), Dy = (float)(Y - L.Center.Y);
	const float Lx = (L.CosAngle * Dx + L.SinAngle * Dy) / L.RadiusCm;
	const float Ly = (-L.SinAngle * Dx + L.CosAngle * Dy) / (L.RadiusCm * L.Aspect);
	const float Q = FMath::Sqrt(Lx * Lx + Ly * Ly);
	constexpr float Fade = 0.10f;   // of a radius: 7-13 m on the 70-130 m lakes
	if (Q <= 1.0f)
	{
		return 1.0f;
	}
	if (Q >= 1.0f + Fade * 1.6f)
	{
		return 0.0f;
	}
	// Ragged rim: ~6 m noise cells move the 0.5 contour by up to +-0.6 of the fade.
	const float Breakup = GradientNoise2(X * (1.0 / 600.0), Y * (1.0 / 600.0), (uint32)FMOWorldGen::HashSeed(Seed, SaltLakeBed)).Value;
	return FMath::Clamp(1.0f - (Q - 1.0f) / Fade + 0.6f * Breakup, 0.0f, 1.0f);
}
