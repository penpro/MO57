// Cave generator half of FMOWorldGen (galleries, caverns, sinkholes). Same rules as MOWorldGen.h: pure, deterministic, thread-safe, IEEE-exact ops only.
//
// Design + the measured constants: Docs/Terrain_Generation_Exploration.md ("Caves"). The field is a SIGNED DISTANCE (cm, negative inside cave air) that a
// Subtractive volume stamp carves out of the height terrain. It is only approximately a distance (noise based, gradient ~1 on average): marching cubes needs
// the sign and a roughly unit gradient, not Euclid.

#include "MOWorldGen.h"
#include "MOWorldGenNoise.h"

using namespace MOWorldGenNoise;

namespace
{
	enum : uint32
	{
		SaltCavernA = 0x7201u, SaltCavernB = 0x7202u,
		SaltSinkhole = 0x7401u, SaltSinkholeWall = 0x7402u,
		SaltGallery = 0x7501u, SaltGalleryPath = 0x7502u, SaltGalleryLevel = 0x7503u, SaltGalleryMask = 0x7504u, SaltGalleryRadius = 0x7505u,
		SaltEntrance = 0x7601u,
		SaltRavine = 0x7701u, SaltRavineWall = 0x7702u,
		SaltIsland = 0x7801u, SaltIslandRock = 0x7802u, SaltIslandTop = 0x7803u
	};

	/** Floor on a gallery meander's gradient (per lattice cell) in its distance estimate: |value| / |gradient| blows up at the noise's saddles. */
	constexpr float GalleryPathGradientFloor = 0.35f;
	/** The ground's slope at a column: central differences this far each way (cm). A 12 m baseline: over a terrace's edge the slope (and with it
	 *  the entrance gate) would otherwise jump within a few metres, and the gate's penalty puts a cliff in the field (MOFramework.WorldGen.Caves). */
	constexpr double GroundSlopeStepCm = 600.0;

	// Distance scaling: a noise field with ~1.2 gradient per lattice cell turns into cm as f * (CellCm / Gradient). The cavern field mixes two octaves
	// (~1.1, x ~1.3 for its squash). The galleries divide by their 2D field's own analytic gradient instead (GalleryDistance).
	constexpr float CavernGradient = 1.4f;
	constexpr float CavernSquash = 1.9f;

	// LOD culls: a feature is evaluated while the sample spacing is at most this fraction of its radius (caverns: up to FMOWorldGenDomain::CavernMaxStepCm). CaveMaxLOD reads the same.
	constexpr float SinkholeStepFraction = 0.9f;
	constexpr float TunnelStepFraction = 0.65f;   // v9: 0.65 x 160 cm keeps LOD1 (100 cm samples) carving the galleries

	struct FSinkholeBasics
	{
		double CenterX = 0.0, CenterY = 0.0;
		float Radius = 0.0f, Depth = 0.0f;
	};


	/** The cell's sinkhole without touching the ground (cheap, pure hashing). The 18% jitter margin keeps a sinkhole inside its own cell. */
	bool SinkholeBasics(const FMOWorldGenTuning& T, int64 CellX, int64 CellY, int32 Seed, FSinkholeBasics& Out)
	{
		const uint32 S = (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltSinkhole);
		if (Unit16(HashLattice2(CellX, CellY, S)) >= T.SinkholeChance)
		{
			return false;
		}
		const uint32 H1 = HashLattice2(CellX, CellY, S + 1u);
		const uint32 H2 = HashLattice2(CellX, CellY, S + 2u);
		Out.CenterX = ((double)CellX + (double)(0.18f + 0.64f * Unit16(H1))) * (double)T.SinkholeCellCm;
		Out.CenterY = ((double)CellY + (double)(0.18f + 0.64f * Unit16(H1 >> 16))) * (double)T.SinkholeCellCm;
		Out.Radius = T.SinkholeRadiusCm * (0.7f + 0.6f * Unit16(H2));
		Out.Depth = T.SinkholeDepthCm * (0.8f + 0.6f * Unit16(H2 >> 16));
		return true;
	}

	/**
	 * Horizontal distance from the sinkhole axis beyond which its distance is >= CaveFarCm (so skipping the evaluation there is exact, not a cliff): the widest wall
	 * is 1.8x radius flare with +25% wobble = 2.25x, the chamber is 2.4x. This must stay inside the cell's 18% jitter margin (the tests assert it for the defaults);
	 * a sinkhole whose reach crosses its cell border would be cut off there.
	 */
	FORCEINLINE float SinkholeReach(const FSinkholeBasics& B) { return B.Radius * 2.4f + FMOWorldGenDomain::CaveFarCm + 300.0f; }

	/**
	 * Ground height at a sinkhole centre. A pure function of (params, seed, cell); memoised per thread because a chunk of voxels asks for the same cell
	 * thousands of times and a height sample costs ~0.5 us. The cache can never change a result, only skip recomputing it.
	 */
	float SinkholeGroundCm(const FMOWorldGenParams& P, int64 CellX, int64 CellY, int32 Seed, const FSinkholeBasics& B)
	{
		struct FEntry
		{
			int64 CellX = 0, CellY = 0;
			uint32 Seed = 0, ParamsHash = 0;
			float Ground = 0.0f;
			bool bValid = false;
		};
		thread_local FEntry Cache[16];

		const uint32 Slot = (uint32)(((uint64)CellX * 73856093ull) ^ ((uint64)CellY * 19349663ull)) & 15u;
		FEntry& E = Cache[Slot];
		if (E.bValid && E.CellX == CellX && E.CellY == CellY && E.Seed == (uint32)Seed && E.ParamsHash == P.GetHash())
		{
			return E.Ground;
		}
		E.CellX = CellX;
		E.CellY = CellY;
		E.Seed = (uint32)Seed;
		E.ParamsHash = P.GetHash();
		E.Ground = FMOWorldGenDomain::SampleHeightCm(P, B.CenterX, B.CenterY, Seed, P.Tuning.VoxelSizeCm);
		E.bValid = true;
		return E.Ground;
	}

	/** Does this cell's sinkhole open at all: not on low ground (coast / basin), and never inside an upland lake or its bank (the lake bed would be a hole under the water). */
	float WaterGate(const FMOWorldGenParams& P, double X, double Y, double Z, int32 Seed);   // below

	// --- Ravines and floating islands (v13, fork F62) ------------------------------------------------------------------------------------------
	/** The ground at a feature's anchor (a cell's ravine or island centre): memoised per thread like SinkholeGroundCm. */
	float AnchorGroundCm(const FMOWorldGenParams& P, uint32 Salt, int64 CellX, int64 CellY, int32 Seed, double X, double Y)
	{
		struct FEntry { int64 CellX = 0, CellY = 0; uint32 Salt = 0, Seed = 0, ParamsHash = 0; float Ground = 0.0f; bool bValid = false; };
		thread_local FEntry Cache[32];
		FEntry& E = Cache[(uint32)(((uint64)CellX * 73856093ull) ^ ((uint64)CellY * 19349663ull) ^ Salt) & 31u];
		if (E.bValid && E.CellX == CellX && E.CellY == CellY && E.Salt == Salt && E.Seed == (uint32)Seed && E.ParamsHash == P.GetHash())
		{
			return E.Ground;
		}
		E = { CellX, CellY, Salt, (uint32)Seed, P.GetHash(), FMOWorldGenDomain::SampleHeightCm(P, X, Y, Seed, P.Tuning.VoxelSizeCm), true };
		return E.Ground;
	}

	// An island is a cluster of craggy clumps (round 3): a main mass and up to four smaller chunks round it, at a little different heights and some
	// apart, as if torn out of the ground and held together by nothing. Each clump's outline is a warped, lobed, notched shape; its top is rolling
	// ground with a jagged rim; its underside is torn rock -- ridged hanging chunks over a rough cone, deepest under the middle.
	constexpr int32 MaxIslandClumps = 5;

	struct FIslandClump
	{
		double X = 0.0, Y = 0.0;
		float R = 0.0f, Lift = 0.0f, Thickness = 0.0f;   // its radius, its top over the cluster's top (cm, + or -), its depth under the middle
	};

	struct FIslandBasics
	{
		double CenterX = 0.0, CenterY = 0.0;
		float Radius = 0.0f, Height = 0.0f, Thickness = 0.0f;   // the cluster: its reach (every clump within 1.3 x), its top over the ground, the main mass's depth
		int32 NumClumps = 1;
		uint32 ShapeSalt = 0;   // this island's own: the outlines' noise is in each clump's own frame, scaled by its radius -- one salt for all would give every island the same shape
		FIslandClump Clumps[MaxIslandClumps];
	};

	/** The cell's island without the ground (pure hashing). Its centre keeps a quarter of the cell from the cell's edges, and the cluster's reach (1.3 x its
	 *  radius) fits in that margin, so an island stays inside its own cell. Sizes skew small (radius log-spread by U^2); the bigger ones fly higher and
	 *  have more chunks. */
	bool IslandBasics(const FMOWorldGenTuning& T, int64 CellX, int64 CellY, int32 Seed, FIslandBasics& Out)
	{
		if (T.IslandChance <= 0.0f)
		{
			return false;
		}
		const uint32 S = (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltIsland);
		if (Unit16(HashLattice2(CellX, CellY, S)) >= T.IslandChance)
		{
			return false;
		}
		const uint32 H1 = HashLattice2(CellX, CellY, S + 1u);
		const uint32 H2 = HashLattice2(CellX, CellY, S + 2u);
		const uint32 H3 = HashLattice2(CellX, CellY, S + 3u);
		Out.CenterX = ((double)CellX + (double)(0.25f + 0.5f * Unit16(H1))) * (double)T.IslandCellCm;
		Out.CenterY = ((double)CellY + (double)(0.25f + 0.5f * Unit16(H1 >> 16))) * (double)T.IslandCellCm;
		const float RMin = FMath::Max(1000.0f, T.IslandRadiusMinCm);
		const float RMax = FMath::Max(RMin, FMath::Min(T.IslandRadiusMaxCm, (0.25f * T.IslandCellCm - 200.0f) / 1.3f));
		const float Size01 = FMath::Square(Unit16(H2));
		Out.Radius = RMin * FMath::Pow(FMath::Max(1.0f, RMax / RMin), Size01);
		Out.Height = T.IslandHeightMinCm + (T.IslandHeightMaxCm - T.IslandHeightMinCm) * FMath::Clamp(0.55f * Size01 + 0.45f * Unit16(H2 >> 16), 0.0f, 1.0f);
		// Round 3b (Nov 4: the first cut's clumps merged into one wobbly blob -- 8 of 12 outlines as round as an ellipse, 1 of 12 with a chunk
		// apart): a smaller main mass, at least two clumps, the others further out (about half float apart), rougher outlines (ClumpOutline).
		const float MainR = 0.5f * Out.Radius;
		Out.Thickness = FMath::Clamp(MainR * (0.65f + 0.35f * Unit16(H3)), 4000.0f, 0.6f * Out.Height);
		Out.Clumps[0] = { Out.CenterX, Out.CenterY, MainR, 0.0f, Out.Thickness };
		Out.NumClumps = FMath::Clamp(2 + (int32)(Unit16(H3 >> 16) * (2.0f + 2.0f * Size01)), 2, MaxIslandClumps);
		Out.ShapeSalt = HashLattice2(CellX, CellY, S + 40u);
		for (int32 K = 1; K < Out.NumClumps; ++K)
		{
			const uint32 Hk = HashLattice2(CellX, CellY, S + 10u + (uint32)K);
			const uint32 Hj = HashLattice2(CellX, CellY, S + 20u + (uint32)K);
			const float Angle = 2.0f * UE_PI * Unit16(Hk);
			// Its outline reaches at most ~2 x its radius from its centre (ClumpOutline's warp, lobes and notches at their extremes): 0.85 + 2 x 0.22
			// stays inside the cluster's reach (1.3 x), where FindIslandAt stops looking.
			const float Dist = Out.Radius * (0.55f + 0.3f * Unit16(Hk >> 16));
			const float R = Out.Radius * (0.1f + 0.12f * Unit16(Hj));
			FIslandClump& C = Out.Clumps[K];
			C.X = Out.CenterX + Dist * FMath::Cos(Angle);
			C.Y = Out.CenterY + Dist * FMath::Sin(Angle);
			C.R = R;
			C.Lift = Out.Thickness * (-0.3f + 0.45f * Unit16(Hj >> 16));
			C.Thickness = FMath::Clamp(R * (0.7f + 0.4f * Unit16(HashLattice2(CellX, CellY, S + 30u + (uint32)K))), 1500.0f, 0.6f * Out.Height);
		}
		return true;
	}

	/** Over land and below the high ground. */
	bool IslandAllowed(const FMOWorldGenParams& P, float Ground)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		return Ground >= T.SeaLevelCm + 300.0f && (T.CaveMaxGroundCm <= 0.0f || Ground < T.CaveMaxGroundCm - T.CaveHighFadeCm);
	}

	/** The island whose reach covers (X, Y), if any, with the real ground at its centre. An island stays inside its cell, so one cell is enough. */
	bool FindIslandAt(const FMOWorldGenParams& P, double X, double Y, int32 Seed, FIslandBasics& B, float& OutGround)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		const int64 CellX = (int64)FMath::FloorToDouble(X / (double)T.IslandCellCm);
		const int64 CellY = (int64)FMath::FloorToDouble(Y / (double)T.IslandCellCm);
		if (!IslandBasics(T, CellX, CellY, Seed, B))
		{
			return false;
		}
		const double Dx = X - B.CenterX, Dy = Y - B.CenterY;
		if (Dx * Dx + Dy * Dy > FMath::Square(1.3 * (double)B.Radius + 100.0))
		{
			return false;
		}
		OutGround = AnchorGroundCm(P, SaltIsland, CellX, CellY, Seed, B.CenterX, B.CenterY);
		return IslandAllowed(P, OutGround);
	}

	/** A clump's outline at (X, Y): cm, negative inside -- a circle warped by 0.4 of its radius, lobed (+-40 %) and notched by ridged noise (crags, 0.18). */
	float ClumpOutline(const FIslandClump& C, double X, double Y, uint32 Salt)
	{
		const double Px = X - C.X, Py = Y - C.Y;
		const double Wl = FMath::Max(500.0, 0.7 * (double)C.R);
		const double Qx = Px + 0.4 * C.R * GradientNoise2(Px / Wl, Py / Wl, Salt).Value;
		const double Qy = Py + 0.4 * C.R * GradientNoise2(Px / Wl + 17.3, Py / Wl - 9.1, Salt).Value;
		const double Ll = FMath::Max(400.0, 0.4 * (double)C.R), Lc = FMath::Max(300.0, 0.13 * (double)C.R);
		const float Edge = C.R * (1.0f + 0.4f * GradientNoise2(Qx / Ll, Qy / Ll, Salt + 1u).Value
			+ 0.18f * (1.0f - 2.0f * FMath::Abs(GradientNoise2(Qx / Lc, Qy / Lc, Salt + 2u).Value)));
		return (float)FMath::Sqrt(Qx * Qx + Qy * Qy) - Edge;
	}

	/** One column of an island: each clump's outline there, its top and the column part of its underside (the 3D crags come per voxel). A chunk asks for
	 *  the same column at every voxel, so the last few columns are kept per thread. */
	struct FIslandColumn
	{
		float Outline[MaxIslandClumps];
		float Top[MaxIslandClumps];
		float Under2D[MaxIslandClumps];
		float Union = 1.0e9f;   // the footprint of the cluster: the least outline (negative = over a clump)
		float MaxTop = -1.0e9f;
	};

	const FIslandColumn& IslandColumnAt(const FIslandBasics& B, float Ground, double X, double Y, int32 Seed)
	{
		struct FEntry { double X = 0.0, Y = 0.0, Cx = 0.0, Cy = 0.0; uint32 Seed = 0; float Ground = 0.0f, Radius = 0.0f; bool bValid = false; FIslandColumn Column; };
		thread_local FEntry Cache[64];
		const uint64 Bits = (uint64)(int64)FMath::FloorToDouble(X) * 73856093ull ^ (uint64)(int64)FMath::FloorToDouble(Y) * 19349663ull;
		FEntry& E = Cache[Bits & 63u];
		if (E.bValid && E.X == X && E.Y == Y && E.Cx == B.CenterX && E.Cy == B.CenterY && E.Seed == (uint32)Seed && E.Ground == Ground && E.Radius == B.Radius)
		{
			return E.Column;
		}
		FIslandColumn& Col = E.Column;
		Col.Union = 1.0e9f;
		Col.MaxTop = -1.0e9f;
		const uint32 ShapeSalt = (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltIslandTop) ^ B.ShapeSalt;
		const double Wave = 3000.0 + 0.25 * (double)B.Radius;
		const float Bump = (300.0f + 0.03f * B.Radius) * (0.7f * GradientNoise2(X / Wave, Y / Wave, ShapeSalt).Value
			+ 0.3f * GradientNoise2(X / (Wave * 0.37), Y / (Wave * 0.37), ShapeSalt + 1u).Value);
		for (int32 K = 0; K < B.NumClumps; ++K)
		{
			const FIslandClump& C = B.Clumps[K];
			const float Outline = ClumpOutline(C, X, Y, ShapeSalt + 7u * (uint32)K + 3u);
			const float In01 = FMath::Clamp(-Outline / FMath::Max(100.0f, 0.35f * C.R), 0.0f, 1.0f);    // 0 at the edge, 1 a third of the way in
			const float Deep01 = FMath::Clamp(-Outline / FMath::Max(100.0f, 0.8f * C.R), 0.0f, 1.0f);   // 1 under the middle
			// The rim: rounded down near the edge, and jagged (ridged noise) -- a broken edge, not a lip.
			const float Rim = FMath::Min(0.25f * C.Thickness, 0.06f * C.R + 500.0f) * FMath::Pow(1.0f - In01, 1.5f);
			const float Jag = 0.08f * C.Thickness * (1.0f - In01)
				* (1.0f - 2.0f * FMath::Abs(GradientNoise2(X / FMath::Max(300.0, 0.08 * C.R), Y / FMath::Max(300.0, 0.08 * C.R), ShapeSalt + 50u).Value));
			const float Top = Ground + B.Height + C.Lift + Bump - Rim - Jag;
			Col.Outline[K] = Outline;
			Col.Top[K] = Top;
			Col.Under2D[K] = Top - C.Thickness * FMath::Pow(Deep01, 0.6f);
			Col.Union = FMath::Min(Col.Union, Outline);
			if (Outline < 0.0f)
			{
				Col.MaxTop = FMath::Max(Col.MaxTop, Top);
			}
		}
		E.X = X;
		E.Y = Y;
		E.Cx = B.CenterX;
		E.Cy = B.CenterY;
		E.Seed = (uint32)Seed;
		E.Ground = Ground;
		E.Radius = B.Radius;
		E.bValid = true;
		return Col;
	}

	/** The island's top at (X, Y): the highest clump over the column, or -1e9 off every clump. */
	float IslandTopAt(const FIslandBasics& B, float Ground, double X, double Y, int32 Seed)
	{
		const FIslandColumn& Col = IslandColumnAt(B, Ground, X, Y, Seed);
		return Col.Union < 0.0f ? Col.MaxTop : -1.0e9f;
	}

	/** The island's rock (cm, negative inside a clump): each clump's rock lies between its torn underside and its top; off the cluster's footprint, the least outline. */
	float IslandRockAt(const FIslandBasics& B, const FIslandColumn& Col, double X, double Y, double Z, int32 Seed)
	{
		if (Col.Union >= 0.0f)
		{
			return FMath::Max(Col.Union, 1.0f);
		}
		const uint32 RockSalt = (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltIslandRock);
		float Rock = 1.0e9f;   // the nearest clump's rock: negative inside one
		for (int32 K = 0; K < B.NumClumps; ++K)
		{
			const FIslandClump& C = B.Clumps[K];
			if (Col.Outline[K] > 0.25f * C.Thickness)
			{
				Rock = FMath::Min(Rock, Col.Outline[K]);   // well off this clump: its outline is the distance
				continue;
			}
			// The torn underside: ridged hanging chunks (sharp creases) and finer broken rock over the column's cone.
			const double Lc = FMath::Max(800.0, 0.15 * (double)C.R), Lf = FMath::Max(300.0, 0.05 * (double)C.R);
			const float Chunks = 1.0f - 2.0f * FMath::Abs(GradientNoise3(X / Lc, Y / Lc, Z / Lc, RockSalt + (uint32)K));
			const float Fine = GradientNoise3(X / Lf, Y / Lf, Z / Lf, RockSalt + 11u);
			const float Under = Col.Under2D[K] + C.Thickness * (0.22f * Chunks + 0.08f * Fine) + 300.0f * GradientNoise3(X / 700.0, Y / 700.0, Z / 700.0, RockSalt + 23u);
			Rock = FMath::Min(Rock, FMath::Max3(Under - (float)Z, (float)Z - Col.Top[K], Col.Outline[K]));
		}
		return Rock;
	}

	struct FRavineBasics
	{
		double CenterX = 0.0, CenterY = 0.0;
		float DirX = 1.0f, DirY = 0.0f, Length = 0.0f, HalfWidth = 0.0f, Depth = 0.0f, Bend = 0.0f, Waves = 1.0f, Phase = 0.0f;
		float StepH = 700.0f, LedgeW = 200.0f;   // its ledges: the wall steps in by LedgeW every StepH going down
	};

	bool RavineBasics(const FMOWorldGenTuning& T, int64 CellX, int64 CellY, int32 Seed, FRavineBasics& Out)
	{
		if (T.RavineChance <= 0.0f)
		{
			return false;
		}
		const uint32 S = (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltRavine);
		if (Unit16(HashLattice2(CellX, CellY, S)) >= T.RavineChance)
		{
			return false;
		}
		const uint32 H1 = HashLattice2(CellX, CellY, S + 1u);
		const uint32 H2 = HashLattice2(CellX, CellY, S + 2u);
		const uint32 H3 = HashLattice2(CellX, CellY, S + 3u);
		Out.CenterX = ((double)CellX + (double)(0.25f + 0.5f * Unit16(H1))) * (double)T.RavineCellCm;
		Out.CenterY = ((double)CellY + (double)(0.25f + 0.5f * Unit16(H1 >> 16))) * (double)T.RavineCellCm;
		const float Angle = 2.0f * UE_PI * Unit16(H2);
		Out.DirX = FMath::Cos(Angle);
		Out.DirY = FMath::Sin(Angle);
		Out.Length = T.RavineLengthMaxCm * (0.4f + 0.6f * Unit16(H2 >> 16));
		Out.HalfWidth = T.RavineHalfWidthMaxCm * (0.5f + 0.5f * Unit16(H3));
		Out.Depth = T.RavineDepthMaxCm * (0.5f + 0.5f * Unit16(H3 >> 16));
		const uint32 H4 = HashLattice2(CellX, CellY, S + 4u);
		Out.Bend = Out.Length * (0.05f + 0.1f * Unit16(H4));
		Out.Waves = 0.6f + 0.8f * Unit16(H4 >> 16);
		const uint32 H5 = HashLattice2(CellX, CellY, S + 5u);
		Out.Phase = 2.0f * UE_PI * Unit16(H5);
		Out.StepH = 500.0f + 500.0f * Unit16(H5 >> 16);
		Out.LedgeW = Out.HalfWidth * (0.15f + 0.15f * Unit16(HashLattice2(CellX, CellY, S + 6u)));
		return true;
	}

	/** How deep the ravine cuts here: its depth, but its floor kept above the sea's clearance (a ravine near the coast is shallower). */
	FORCEINLINE float RavineDepth(const FMOWorldGenParams& P, const FRavineBasics& B, float Ground)
	{
		return FMath::Min(B.Depth, Ground - (P.Tuning.SeaLevelCm + P.Tuning.CaveWaterClearanceCm));
	}

	/** Dry ground in the cave band: above the coast's low ground, below the high ground (as the other cave features), and room above the sea for a
	 *  ravine at least half the deepest one (Wes: "50-100m deep"; a ravine the sea's clearance would cut shallower is not made at all). */
	bool RavineAllowed(const FMOWorldGenParams& P, const FRavineBasics& B, float Ground, int32 Seed)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		if (RavineDepth(P, B, Ground) < FMath::Max(3000.0f, 0.5f * T.RavineDepthMaxCm) || Ground < T.SeaLevelCm + T.CaveMinGroundAboveSeaCm
			|| (T.CaveMaxGroundCm > 0.0f && Ground >= T.CaveMaxGroundCm - T.CaveHighFadeCm))
		{
			return false;
		}
		// Out of the water, as every cave feature (the shore lattice of WaterGate: low ground, a river, a basin, an upland lake's shore): nine points
		// along its path at the ground. A ravine cut through an upland lake (round 3's lab run, Nov 4) drained into the slot under the lake's water
		// plane. Once per ravine: kept per thread, since a chunk asks at every voxel.
		struct FEntry { double X = 0.0, Y = 0.0; uint32 Seed = 0, ParamsHash = 0; bool bDry = false, bValid = false; };
		thread_local FEntry Cache[32];
		FEntry& E = Cache[(uint32)(((uint64)(int64)B.CenterX * 73856093ull) ^ ((uint64)(int64)B.CenterY * 19349663ull)) & 31u];
		if (E.bValid && E.X == B.CenterX && E.Y == B.CenterY && E.Seed == (uint32)Seed && E.ParamsHash == P.GetHash())
		{
			return E.bDry;
		}
		bool bDry = true;
		for (int32 I = 0; I <= 8 && bDry; ++I)
		{
			const float S = (float)I / 8.0f;
			const float Along = (S - 0.5f) * B.Length;
			const float Side = B.Bend * FMath::Sin(2.0f * UE_PI * B.Waves * S + B.Phase);
			bDry = WaterGate(P, B.CenterX + B.DirX * Along - B.DirY * Side, B.CenterY + B.DirY * Along + B.DirX * Side, Ground, Seed) <= 0.0f;
		}
		E = { B.CenterX, B.CenterY, (uint32)Seed, P.GetHash(), bDry, true };
		return bDry;
	}

	/** The nearest point of a ravine's path (a polyline of 16 pieces): its distance and where along the ravine it is (S in 0..1). The path's points are a pure function of the cell: kept per
	 *  thread, since a chunk asks for them at every voxel (17 sines a time). */
	void NearestOnRavinePath(const FMOWorldGenParams& P, const FRavineBasics& B, int64 Cx, int64 Cy, int32 Seed, double X, double Y, float& OutDist, float& OutS)
	{
		constexpr int32 Segments = 16;
		struct FPathCache { int64 CellX = 0, CellY = 0; uint32 Seed = 0, ParamsHash = 0; FVector2D Points[Segments + 1]; bool bValid = false; };
		thread_local FPathCache PathCache[16];
		FPathCache& Path = PathCache[(uint32)(((uint64)Cx * 73856093ull) ^ ((uint64)Cy * 19349663ull)) & 15u];
		if (!Path.bValid || Path.CellX != Cx || Path.CellY != Cy || Path.Seed != (uint32)Seed || Path.ParamsHash != P.GetHash())
		{
			for (int32 I = 0; I <= Segments; ++I)
			{
				const float S = (float)I / Segments;
				const float Along = (S - 0.5f) * B.Length;
				const float Side = B.Bend * FMath::Sin(2.0f * UE_PI * B.Waves * S + B.Phase);
				Path.Points[I] = FVector2D(B.CenterX + B.DirX * Along - B.DirY * Side, B.CenterY + B.DirY * Along + B.DirX * Side);
			}
			Path.CellX = Cx;
			Path.CellY = Cy;
			Path.Seed = (uint32)Seed;
			Path.ParamsHash = P.GetHash();
			Path.bValid = true;
		}
		const FVector2D Q(X, Y);
		OutDist = TNumericLimits<float>::Max();
		OutS = 0.0f;
		FVector2D A = Path.Points[0];
		for (int32 I = 1; I <= Segments; ++I)
		{
			const FVector2D Bp = Path.Points[I];
			const FVector2D Ab = Bp - A;
			const double L2 = FMath::Max(Ab.SizeSquared(), 1.0);
			const double U = FMath::Clamp(FVector2D::DotProduct(Q - A, Ab) / L2, 0.0, 1.0);
			const float Dist = (float)FVector2D::Distance(Q, A + Ab * U);
			if (Dist < OutDist)
			{
				OutDist = Dist;
				OutS = ((float)(I - 1) + (float)U) / Segments;
			}
			A = Bp;
		}
	}

	/** How much of a ravine there is at S along it: 1 at the middle, tapering to nothing at both ends. */
	FORCEINLINE float RavineEnvelope(float S)
	{
		return FMath::Pow(FMath::Max(0.0f, FMath::Sin(UE_PI * S)), 0.5f);
	}

	/** The keep-out round the ravines (cm, negative inside): 2D, out from the path by the ravine's half width at that point of its length, plus a margin. Plants, world features and spawns ask
	 *  this through DistanceToSinkholeRimZone: 9 cell hashes where no ravine is near. */
	float RavineZoneDistance(const FMOWorldGenParams& P, double X, double Y, int32 Seed)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		float D = FMOWorldGenDomain::NoSinkholeDistanceCm;
		if (T.RavineChance <= 0.0f)
		{
			return D;
		}
		const int64 Cx0 = (int64)FMath::FloorToDouble(X / (double)T.RavineCellCm);
		const int64 Cy0 = (int64)FMath::FloorToDouble(Y / (double)T.RavineCellCm);
		for (int64 Cy = Cy0 - 1; Cy <= Cy0 + 1; ++Cy)
		{
			for (int64 Cx = Cx0 - 1; Cx <= Cx0 + 1; ++Cx)
			{
				FRavineBasics B;
				if (!RavineBasics(T, Cx, Cy, Seed, B))
				{
					continue;
				}
				const double Reach = 0.5 * B.Length + B.Bend + 30000.0;   // past the widest footprint that asks
				if (FMath::Square(X - B.CenterX) + FMath::Square(Y - B.CenterY) > Reach * Reach)
				{
					continue;
				}
				const float Ground = AnchorGroundCm(P, SaltRavine, Cx, Cy, Seed, B.CenterX, B.CenterY);
				if (!RavineAllowed(P, B, Ground, Seed))
				{
					continue;
				}
				float Dist = 0.0f, S = 0.0f;
				NearestOnRavinePath(P, B, Cx, Cy, Seed, X, Y, Dist, S);
				D = FMath::Min(D, Dist - B.HalfWidth * FMath::Pow(RavineEnvelope(S), 0.6f) - 100.0f);
			}
		}
		return D;
	}

	/** The ravines' distance (cm, negative = air) at a point: the nearest point of each ravine's meandering path in the 3 x 3 cells round it. A Minecraft
	 *  ravine (round 2): open to the sky, 50-100 m deep at mid-length, widest a little above mid-depth, the lip a little narrower and the bottom narrowing
	 *  to a third; its walls stepped into ledges every 5-10 m (leaning out within a step, then a shelf), craggy (ridged noise: sharp creases at 3 m,
	 *  lumps at 10 m), its floor uneven. Skipped when the sample spacing cannot resolve the slot. */
	float RavineDistance(const FMOWorldGenParams& P, double X, double Y, double Z, int32 Seed, float StepCm)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		if (T.RavineChance <= 0.0f || StepCm > T.RavineHalfWidthMaxCm * 0.9f)
		{
			return FMOWorldGenDomain::CaveFarCm;
		}
		float D = FMOWorldGenDomain::CaveFarCm;
		const int64 Cx0 = (int64)FMath::FloorToDouble(X / (double)T.RavineCellCm);
		const int64 Cy0 = (int64)FMath::FloorToDouble(Y / (double)T.RavineCellCm);
		for (int64 Cy = Cy0 - 1; Cy <= Cy0 + 1; ++Cy)
		{
			for (int64 Cx = Cx0 - 1; Cx <= Cx0 + 1; ++Cx)
			{
				FRavineBasics B;
				if (!RavineBasics(T, Cx, Cy, Seed, B))
				{
					continue;
				}
				const double Reach = 0.5 * B.Length + B.Bend + 1.5 * B.HalfWidth + FMOWorldGenDomain::CaveFarCm + 300.0;
				if (FMath::Square(X - B.CenterX) + FMath::Square(Y - B.CenterY) > Reach * Reach)
				{
					continue;
				}
				const float Ground = AnchorGroundCm(P, SaltRavine, Cx, Cy, Seed, B.CenterX, B.CenterY);
				if (!RavineAllowed(P, B, Ground, Seed))
				{
					continue;
				}
				float BestDist = 0.0f, BestS = 0.0f;
				NearestOnRavinePath(P, B, Cx, Cy, Seed, X, Y, BestDist, BestS);
				const float Env = RavineEnvelope(BestS);
				if (Env < 0.02f)
				{
					continue;
				}
				const uint32 WallSalt = (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltRavineWall);
				const bool bNearWall = BestDist < 1.8f * B.HalfWidth + 400.0f;   // the walls' detail only matters near them
				const float FloorDepth = RavineDepth(P, B, Ground) * FMath::Pow(Env, 0.5f);
				const float Floor = Ground - FloorDepth + (bNearWall ? 150.0f * GradientNoise2(X / 900.0, Y / 900.0, WallSalt + 2u).Value : 0.0f);
				const float F = FMath::Clamp((Ground - (float)Z) / FMath::Max(100.0f, FloorDepth), 0.0f, 1.0f);
				const float Profile = (0.62f + 0.38f * FMath::Sin(UE_PI * FMath::Min(1.0f, F * 1.15f))) * (1.0f - 0.55f * Smooth01(0.7f, 1.0f, F));
				float HalfWidth = B.HalfWidth * FMath::Pow(Env, 0.6f) * Profile;
				if (bNearWall)
				{
					// Ledges: going down, the wall leans out across a step and then steps in by LedgeW over its last 6 % -- a shelf; the steps wander with
					// the rock (a 2D noise shifts them by up to 0.6 of a step).
					const float Steps = (Ground - (float)Z) / B.StepH + 0.6f * GradientNoise2(X / 2000.0, Y / 2000.0, WallSalt + 3u).Value;
					const float Frac = Steps - FMath::FloorToFloat(Steps);
					HalfWidth += B.LedgeW * (Frac - 0.5f - Smooth01(0.94f, 1.0f, Frac));
					// Crags: ridged noise for sharp creases (3 m) and lumps (10 m).
					const float Crease = 1.0f - 2.0f * FMath::Abs(GradientNoise3(X / 300.0, Y / 300.0, Z / 300.0, WallSalt));
					const float Lumps = GradientNoise3(X / 1000.0, Y / 1000.0, Z / 700.0, WallSalt + 1u);
					HalfWidth += B.HalfWidth * (0.16f * Crease + 0.22f * Lumps);
				}
				HalfWidth = FMath::Max(HalfWidth, 60.0f);
				D = FMath::Min(D, FMath::Max(BestDist - HalfWidth, Floor - (float)Z));
			}
		}
		return D;
	}


	/** A sinkhole exists only where its whole shaft stays dry: above the water floor and outside the water keep-out (v10; upland lakes included). Otherwise the
	 *  gate would close it while GetSinkholeInCell / DistanceToSinkholeRimZone still reported it (the world features' keep-out, the dev verbs, PCG suppression). */
	bool SinkholeAllowed(const FMOWorldGenParams& P, const FSinkholeBasics& B, float GroundCm, int32 Seed)
	{
		return GroundCm >= P.Tuning.SeaLevelCm + P.Tuning.CaveMinGroundAboveSeaCm && WaterGate(P, B.CenterX, B.CenterY, GroundCm - B.Depth, Seed) <= 0.0f;
	}

	/** Shaft + chamber distance of the sinkhole in the sample's cell (cm, negative inside). */
	float SinkholeDistance(const FMOWorldGenParams& P, double X, double Y, double Z, int32 Seed)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		const int64 CellX = (int64)FMath::FloorToDouble(X / (double)T.SinkholeCellCm);
		const int64 CellY = (int64)FMath::FloorToDouble(Y / (double)T.SinkholeCellCm);
		FSinkholeBasics B;
		if (!SinkholeBasics(T, CellX, CellY, Seed, B))
		{
			return FMOWorldGenDomain::CaveFarCm;
		}
		const float Dx = (float)(X - B.CenterX), Dy = (float)(Y - B.CenterY);
		const float H2 = Dx * Dx + Dy * Dy;
		const float Reach = SinkholeReach(B);
		if (H2 > Reach * Reach)
		{
			return FMOWorldGenDomain::CaveFarCm;
		}
		const float Ground = SinkholeGroundCm(P, CellX, CellY, Seed, B);
		if (!SinkholeAllowed(P, B, Ground, Seed))
		{
			return FMOWorldGenDomain::CaveFarCm;
		}

		const float Z0 = (float)Z;
		const float TDepth = Ground - Z0;
		const float ZBottom = Ground - B.Depth;

		// Shaft: flares to 1.8x over the top 9 m, walls wobble +-25% (a clean cylinder reads as a pipe, not a collapse).
		const float Funnel = FMath::Clamp(1.0f - TDepth / 900.0f, 0.0f, 1.0f);
		const double WobScale = 1.0 / (double)(B.Radius * 3.0f);
		const float Wob = GradientNoise3(X * WobScale, Y * WobScale, Z * WobScale, (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltSinkholeWall));
		const float RWall = B.Radius * (1.0f + 0.8f * Funnel * Funnel) * (1.0f + 0.25f * FMath::Clamp(1.5f * Wob, -1.0f, 1.0f));
		const float DShaft = FMath::Max(FMath::Sqrt(H2) - RWall, ZBottom - Z0);

		// Chamber: a flattened sphere sitting on the shaft bottom.
		const float Rb = B.Radius * 2.4f;
		const float Dz = (Z0 - (ZBottom + 0.35f * Rb)) * 1.5f;
		const float DBell = FMath::Sqrt(H2 + Dz * Dz) - Rb;
		return SmoothMin(DShaft, DBell, 300.0f);
	}
}

namespace
{
	/** The ground's gradient (cm per cm) at a column, central differences; memoised per thread for the column's run of Z samples (a height sample ~0.5 us). */
	FVector2D GroundGradient(const FMOWorldGenParams& P, double X, double Y, float GroundCm, int32 Seed)
	{
		struct FEntry
		{
			uint64 BitsX = 0, BitsY = 0;
			uint32 Seed = 0, ParamsHash = 0;
			FVector2D Gradient = FVector2D::ZeroVector;
			bool bValid = false;
		};
		thread_local FEntry Last;
		uint64 BitsX, BitsY;
		FMemory::Memcpy(&BitsX, &X, sizeof(BitsX));
		FMemory::Memcpy(&BitsY, &Y, sizeof(BitsY));
		if (Last.bValid && Last.BitsX == BitsX && Last.BitsY == BitsY && Last.Seed == (uint32)Seed && Last.ParamsHash == P.GetHash())
		{
			return Last.Gradient;
		}
		const float Hx = FMOWorldGenDomain::SampleHeightCm(P, X + GroundSlopeStepCm, Y, Seed, P.Tuning.VoxelSizeCm)
			- FMOWorldGenDomain::SampleHeightCm(P, X - GroundSlopeStepCm, Y, Seed, P.Tuning.VoxelSizeCm);
		const float Hy = FMOWorldGenDomain::SampleHeightCm(P, X, Y + GroundSlopeStepCm, Seed, P.Tuning.VoxelSizeCm)
			- FMOWorldGenDomain::SampleHeightCm(P, X, Y - GroundSlopeStepCm, Seed, P.Tuning.VoxelSizeCm);
		Last.BitsX = BitsX;
		Last.BitsY = BitsY;
		Last.Seed = (uint32)Seed;
		Last.ParamsHash = P.GetHash();
		Last.Gradient = FVector2D(Hx / (2.0 * GroundSlopeStepCm), Hy / (2.0 * GroundSlopeStepCm));
		Last.bValid = true;
		return Last.Gradient;
	}

	/**
	 * The cave gate's XY half -- the water keep-out (v10) and the karst rule (v11: caves only in limestone) -- on a lattice of ShoreLatticeCm (bilinear between nodes, so the gate -- and the field -- stays smooth however sharp a coast
	 * is). Every sea-level water body (the sea, a river, a pond or lake basin) can only lie on LOW ground: rivers fade out above RiverMaxGroundAboveSeaCm + 6 m,
	 * basin rims stop at BasinRimMaxAboveSeaCm. There the water table is the sea's, so a node is "wet" (1) where low ground (the pre-water base height, which
	 * has no thin channels to slip between samples) lies within CaveShoreKeepOutCm -- at the node or on three rings (a quarter, half and the whole distance;
	 * 8 / 16 / 16 samples) -- or the shore of the upland lake of the node's cell is within 120 m (easing out by 180 m; a lake's centre keeps 325 m from its cell's
	 * edges, so its own cell's lake is the only one that can be that near). The wetness eases from 1 at the low-ground ceiling to 0 fifteen metres higher. A node is a pure function of (node, seed, params): the thread-local cache only saves work, never a value.
	 * (First cut, Oct 17: water = carved ground at sea level on two 8-sample rings -- MOFramework.WorldGen.CavesStayDry found 6 of 300 cave samples with a river
	 * within 100 m, slipped between the rays.)
	 */
	constexpr double ShoreLatticeCm = 1600.0;
	// Unit directions every 22.5 deg (exact constants: no libm in the generator's math).
	constexpr double ShoreRing[16][2] = { { 1.0, 0.0 }, { 0.92387953251128674, 0.38268343236508977 }, { 0.70710678118654752, 0.70710678118654752 },
		{ 0.38268343236508977, 0.92387953251128674 }, { 0.0, 1.0 }, { -0.38268343236508977, 0.92387953251128674 }, { -0.70710678118654752, 0.70710678118654752 },
		{ -0.92387953251128674, 0.38268343236508977 }, { -1.0, 0.0 }, { -0.92387953251128674, -0.38268343236508977 }, { -0.70710678118654752, -0.70710678118654752 },
		{ -0.38268343236508977, -0.92387953251128674 }, { 0.0, -1.0 }, { 0.38268343236508977, -0.92387953251128674 }, { 0.70710678118654752, -0.70710678118654752 },
		{ 0.92387953251128674, -0.38268343236508977 } };

	float ShoreNodeWetness(const FMOWorldGenParams& P, int64 Ix, int64 Iy, int32 Seed)
	{
		struct FSlot
		{
			int64 Ix = 0, Iy = 0;
			uint32 Seed = 0, ParamsHash = 0;
			float Value = 0.0f;
			bool bValid = false;
		};
		thread_local FSlot Cache[4096];
		const uint32 Hash = P.GetHash();
		FSlot& Slot = Cache[((uint32)Ix * 73856093u ^ (uint32)Iy * 19349663u) & 4095u];
		if (Slot.bValid && Slot.Ix == Ix && Slot.Iy == Iy && Slot.Seed == (uint32)Seed && Slot.ParamsHash == Hash)
		{
			return Slot.Value;
		}
		const FMOWorldGenTuning& T = P.Tuning;
		const double X = (double)Ix * ShoreLatticeCm, Y = (double)Iy * ShoreLatticeCm;
		if (T.bCavesOnlyInLimestone && FMOWorldGenDomain::SampleBedrock(P, X, Y, Seed) != EMOBedrock::Limestone)
		{
			Slot.Ix = Ix;
			Slot.Iy = Iy;
			Slot.Seed = (uint32)Seed;
			Slot.ParamsHash = Hash;
			Slot.Value = 1.0f;   // not karst: no solution caves in this rock
			Slot.bValid = true;
			return 1.0f;
		}
		const float LowCeiling = T.SeaLevelCm + FMath::Max(T.RiverMaxGroundAboveSeaCm + 600.0f, T.BasinRimMaxAboveSeaCm);
		auto WetAt = [&P, &T, Seed, LowCeiling](double Sx, double Sy)
		{
			return 1.0f - Smooth01(LowCeiling, LowCeiling + 1500.0f, FMOWorldGenDomain::SampleBaseHeightCm(P, Sx, Sy, Seed, T.VoxelSizeCm));
		};
		float Wet = WetAt(X, Y);
		const double R = (double)T.CaveShoreKeepOutCm;
		for (int32 K = 0; K < 16 && Wet < 1.0f; K += 2)
		{
			Wet = FMath::Max(Wet, WetAt(X + 0.25 * R * ShoreRing[K][0], Y + 0.25 * R * ShoreRing[K][1]));
		}
		for (int32 K = 0; K < 16 && Wet < 1.0f; ++K)
		{
			Wet = FMath::Max(Wet, WetAt(X + 0.5 * R * ShoreRing[K][0], Y + 0.5 * R * ShoreRing[K][1]));
		}
		for (int32 K = 0; K < 16 && Wet < 1.0f; ++K)
		{
			Wet = FMath::Max(Wet, WetAt(X + R * ShoreRing[K][0], Y + R * ShoreRing[K][1]));
		}
		if (Wet < 1.0f)
		{
			Wet = FMath::Max(Wet, 1.0f - Smooth01(12000.0f, 18000.0f, FMOWorldGenDomain::UplandLakeShoreDistanceCm(P, X, Y, Seed)));
		}
		Slot.Ix = Ix;
		Slot.Iy = Iy;
		Slot.Seed = (uint32)Seed;
		Slot.ParamsHash = Hash;
		Slot.Value = Wet;
		Slot.bValid = true;
		return Wet;
	}

	/** 0..1: how much the water keep-out closes caves at (X, Y, Z) -- the floor above the sea, then the shore lattice. One gate for every cave feature. */
	float WaterGate(const FMOWorldGenParams& P, double X, double Y, double Z, int32 Seed)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		const float Floor = T.SeaLevelCm + T.CaveWaterClearanceCm;
		const float Below = 1.0f - Smooth01(Floor, Floor + T.CaveWaterFadeCm, (float)Z);
		if (Below >= 1.0f || (T.CaveShoreKeepOutCm <= 0.0f && !T.bCavesOnlyInLimestone))
		{
			return Below;
		}
		const double Fx = X / ShoreLatticeCm, Fy = Y / ShoreLatticeCm;
		const double X0 = FMath::FloorToDouble(Fx), Y0 = FMath::FloorToDouble(Fy);
		const int64 Ix = (int64)X0, Iy = (int64)Y0;
		const float Tx = (float)(Fx - X0), Ty = (float)(Fy - Y0);
		const float Shore = FMath::Lerp(FMath::Lerp(ShoreNodeWetness(P, Ix, Iy, Seed), ShoreNodeWetness(P, Ix + 1, Iy, Seed), Tx),
			FMath::Lerp(ShoreNodeWetness(P, Ix, Iy + 1, Seed), ShoreNodeWetness(P, Ix + 1, Iy + 1, Seed), Tx), Ty);
		return FMath::Max(Below, Shore);
	}

	/**
	 * Galleries: distance (cm, negative inside) to the nearest gallery air, every gate applied. Levels sit every GalleryLevelSpacingCm at their own
	 * gently wandering height; in plan a level's passages follow the zero set of a 2D field (its gradient gives the distance and the passage's
	 * direction). The cover holds everywhere except where a passage runs INTO a steep slope: there it opens out of the hillside -- an entrance you
	 * walk into, never a hole in flat ground to fall into.
	 */
	float GalleryDistance(const FMOWorldGenParams& P, double X, double Y, double Z, float GroundCm, float Depth, int32 Seed, float LowGround, float HighGround)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		const double Spacing = FMath::Max(T.GalleryLevelSpacingCm, 1000.0f);
		const int64 K0 = (int64)FMath::FloorToDouble(Z / Spacing);
		const float RMax = T.TunnelRadiusCm * (1.0f + T.TunnelRadiusVariation);
		const double InvPath = 1.0 / (double)T.TunnelWavelengthCm;
		const double InvLevel = 1.0 / (double)T.GalleryLevelWavelengthCm;
		const double InvMask = 0.5 / (double)T.TunnelWavelengthCm;
		const float DeepFade = Smooth01(T.CaveMaxDepthCm, T.CaveMaxDepthCm + T.CaveFadeCm, Depth);
		const uint32 Base = (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltGallery);
		float Best = FMOWorldGenDomain::CaveFarCm;
		for (int64 K = K0 - 1; K <= K0 + 1; ++K)
		{
			const uint32 Salt = Base ^ (uint32)((uint64)K * 0x9E3779B97F4A7C15ull >> 32);
			const FNoise2 Level = GradientNoise2(X * InvLevel, Y * InvLevel, Salt ^ SaltGalleryLevel);
			const double Zc = ((double)K + 0.5) * Spacing + (double)(T.GalleryLevelAmplitudeCm * Level.Value);
			const float Dz = (float)(Z - Zc) * T.TunnelVerticalSquash;
			if (FMath::Abs(Dz) > RMax + FMOWorldGenDomain::CaveFarCm)
			{
				continue;
			}
			const FNoise2 Path = GradientNoise2(X * InvPath, Y * InvPath, Salt ^ SaltGalleryPath);
			const float PathGrad = FMath::Sqrt(Path.Dx * Path.Dx + Path.Dy * Path.Dy);
			const float Dxy = FMath::Abs(Path.Value) * T.TunnelWavelengthCm / FMath::Max(PathGrad, GalleryPathGradientFloor);
			if (Dxy > RMax + FMOWorldGenDomain::CaveFarCm)
			{
				continue;
			}
			const FNoise2 Mask = GradientNoise2(X * InvMask + 3.7, Y * InvMask - 1.9, Salt ^ SaltGalleryMask);
			// Where the mask closes, the passage tapers shut: its radius shrinks AND a penalty saturates the term (skipping the level there instead put a
			// cliff in the field -- MOFramework.WorldGen.Caves measured a gradient of 86).
			const float Open = Smooth01(T.GalleryMaskThreshold - 0.15f, T.GalleryMaskThreshold + 0.05f, Mask.Value);
			const FNoise2 Wob = GradientNoise2(X * InvPath * 2.3 + 7.1, Y * InvPath * 2.3 - 4.4, Salt ^ SaltGalleryRadius);
			const float Radius = T.TunnelRadiusCm * (1.0f + T.TunnelRadiusVariation * FMath::Clamp(1.6f * Wob.Value, -1.0f, 1.0f)) * Open;

			float Cover = T.CaveCoverCm, Fade = T.CaveFadeCm;
			if (Depth < T.CaveCoverCm + T.CaveFadeCm && PathGrad > 1e-4f)
			{
				const FVector2D G = GroundGradient(P, X, Y, GroundCm, Seed);
				const float Slope = (float)G.Size();
				const float Steep = Smooth01(FMath::Tan(FMath::DegreesToRadians(T.CaveEntranceSlopeMinDeg)), FMath::Tan(FMath::DegreesToRadians(T.CaveEntranceSlopeFullDeg)), Slope);
				if (Steep > 0.0f)
				{
					// The passage's direction is along the meander (perpendicular to its gradient); it opens where it heads into or out of the slope.
					const float Along = FMath::Abs((float)((-Path.Dy * G.X + Path.Dx * G.Y) / ((double)PathGrad * (double)Slope)));
					const float Entrance = Steep * Smooth01(0.25f, 0.6f, Along);
					Cover = FMath::Lerp(Cover, -200.0f, Entrance);
					Fade = FMath::Lerp(Fade, 1.0f, Entrance);
				}
			}
			const float Shallow = 1.0f - Smooth01(Cover, Cover + Fade, Depth);
			const float Gate = FMath::Max(FMath::Max3(Shallow, DeepFade, LowGround), HighGround);
			const float Dg = FMath::Sqrt(Dxy * Dxy + Dz * Dz) - Radius;
			// The penalty cancels the deepest possible interior and adds CaveFarCm: with a gate closed the term is saturated; in between it tapers.
			Best = FMath::Min(Best, Dg + (Radius + FMOWorldGenDomain::CaveFarCm) * Gate + (1.0f - Open) * FMOWorldGenDomain::CaveFarCm);
		}
		return Best;
	}

	// --- Built cave entrances (F30, N-170; CaveEntranceChance 0 = off) ---------------------------------------------------------------------------------------------
	// A gallery opens only where it runs into a hillside steeper than CaveEntranceSlopeMinDeg, and ordinary ground is too gentle for that: a walk-in mouth in 0 % of
	// 1 km tiles of cave country (MOFramework.WorldGen.CaveReach, Oct 19 2026). A built entrance is a shallow collapse hollow (a doline, walkable sides) with a
	// sloping passage down out of its side into a small chamber: a slope you walk down, never a shaft to fall into, and a hollow you can see from a way off.

	/** The passage's radius (cm): a little wider than a gallery's mean, so the mouth reads from the hollow. */
	constexpr float EntrancePassageRadiusCm = 170.0f;
	/** The passage's fall per unit of run: ~27 deg, a walkable grade. */
	constexpr float EntranceGrade = 0.5f;
	/** The hollow is evaluated while the sample spacing is at most this fraction of its smallest radius (a terrain-scale feature: it stays through LOD 4). */
	constexpr float EntranceHollowStepFraction = 0.9f;
	constexpr float EntranceHollowMinRadiusCm = 1100.0f;

	struct FEntranceBasics
	{
		double CenterX = 0.0, CenterY = 0.0;   // the hollow's centre (cm)
		float HollowRadius = 0.0f;             // across the top (cm)
		float HollowDepth = 0.0f;              // at its middle (cm)
		float DirX = 1.0f, DirY = 0.0f;        // the passage's heading in plan (unit, one of 16 exact directions)
		float Run = 0.0f;                      // the passage's length in plan (cm)
		float ChamberRadius = 0.0f;            // (cm)
	};

	/** The cell's entrance without touching the ground (pure hashing). The 18 % jitter margin keeps the whole entrance inside its cell: the run is shortened
	 *  until its reach fits (a smaller CaveEntranceCellCm gets shorter passages, never one cut off at a cell border). */
	bool EntranceBasics(const FMOWorldGenTuning& T, int64 CellX, int64 CellY, int32 Seed, FEntranceBasics& Out)
	{
		if (T.CaveEntranceChance <= 0.0f)
		{
			return false;
		}
		const uint32 S = (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltEntrance);
		if (Unit16(HashLattice2(CellX, CellY, S)) >= T.CaveEntranceChance)
		{
			return false;
		}
		const uint32 H1 = HashLattice2(CellX, CellY, S + 1u);
		const uint32 H2 = HashLattice2(CellX, CellY, S + 2u);
		const uint32 H3 = HashLattice2(CellX, CellY, S + 3u);
		Out.CenterX = ((double)CellX + (double)(0.18f + 0.64f * Unit16(H1))) * (double)T.CaveEntranceCellCm;
		Out.CenterY = ((double)CellY + (double)(0.18f + 0.64f * Unit16(H1 >> 16))) * (double)T.CaveEntranceCellCm;
		Out.HollowRadius = EntranceHollowMinRadiusCm + 400.0f * Unit16(H2);
		Out.HollowDepth = 350.0f + 150.0f * Unit16(H2 >> 16);
		const uint32 Dir = H3 & 15u;
		Out.DirX = (float)ShoreRing[Dir][0];
		Out.DirY = (float)ShoreRing[Dir][1];
		Out.ChamberRadius = 450.0f + 200.0f * Unit16(H3 >> 20);
		const float Margin = 0.18f * T.CaveEntranceCellCm;
		const float Fixed = Out.HollowRadius * 0.35f + Out.ChamberRadius + FMOWorldGenDomain::CaveFarCm + 300.0f;
		Out.Run = FMath::Min(2200.0f + 1000.0f * Unit16(H3 >> 4), Margin - Fixed);
		return Out.Run >= 800.0f;
	}

	/** Horizontal distance from the hollow's centre beyond which the entrance's field is >= CaveFarCm (skipping it there is exact). <= the jitter margin. */
	FORCEINLINE float EntranceReach(const FEntranceBasics& B)
	{
		return FMath::Max(B.HollowRadius * 1.6f, B.HollowRadius * 0.35f + B.Run + B.ChamberRadius) + FMOWorldGenDomain::CaveFarCm + 300.0f;
	}

	/**
	 * Settle the entrance on its ground: the passage heads UPHILL (the nearest of 16 exact directions to the slope's rise; the hashed one on level ground), as a
	 * cave runs into its hill -- headed downhill it came back up to the surface past the hollow (MOFramework.WorldGen.CaveEntranceBuilt). Then the gates: above
	 * the coast's ground and below the high ground, ground at most 20 deg (steeper slopes have hillside mouths of their own), dry + karst at both ends (WaterGate
	 * holds the water keep-out AND the limestone rule), and at least 3 m of rock over the chamber.
	 */
	bool SettleEntrance(const FMOWorldGenParams& P, FEntranceBasics& B, float GroundCm, int32 Seed)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		if (GroundCm < T.SeaLevelCm + T.CaveMinGroundAboveSeaCm + 600.0f || (T.CaveMaxGroundCm > 0.0f && GroundCm > T.CaveMaxGroundCm - T.CaveHighFadeCm))
		{
			return false;
		}
		const FVector2D Rise = GroundGradient(P, B.CenterX, B.CenterY, GroundCm, Seed);
		const double Slope = Rise.Size();
		if (Slope > 0.364)   // tan(20 deg)
		{
			return false;
		}
		if (Slope > 0.02)
		{
			int32 BestK = 0;
			double BestDot = -2.0;
			for (int32 K = 0; K < 16; ++K)
			{
				const double Dot = ShoreRing[K][0] * Rise.X + ShoreRing[K][1] * Rise.Y;
				if (Dot > BestDot) { BestDot = Dot; BestK = K; }
			}
			B.DirX = (float)ShoreRing[BestK][0];
			B.DirY = (float)ShoreRing[BestK][1];
		}
		const float Lead = B.HollowRadius * 0.35f;
		const float EndZ = GroundCm - B.HollowDepth - B.Run * EntranceGrade;
		const double EndX = B.CenterX + B.DirX * (Lead + B.Run), EndY = B.CenterY + B.DirY * (Lead + B.Run);
		const float EndGround = FMOWorldGenDomain::SampleHeightCm(P, EndX, EndY, Seed, T.VoxelSizeCm);
		const float ChamberTop = EndZ + EntrancePassageRadiusCm * 0.8f + 1.3f * B.ChamberRadius / 1.5f;
		return EndGround - ChamberTop >= 300.0f
			&& WaterGate(P, B.CenterX, B.CenterY, EndZ, Seed) <= 0.0f
			&& WaterGate(P, EndX, EndY, EndZ, Seed) <= 0.0f;
	}

	/** Ground at the hollow's centre and whether the entrance is allowed there, memoised per thread like the sinkhole's ground (pure functions of params, seed
	 *  and cell: the cache only saves work). A chunk asks for the same cell thousands of times, and the slope check alone costs four height samples. */
	bool EntranceGround(const FMOWorldGenParams& P, int64 CellX, int64 CellY, int32 Seed, FEntranceBasics& B, float& OutGround)
	{
		struct FEntry
		{
			int64 CellX = 0, CellY = 0;
			uint32 Seed = 0, ParamsHash = 0;
			float Ground = 0.0f;
			float DirX = 1.0f, DirY = 0.0f;
			bool bAllowed = false;
			bool bValid = false;
		};
		thread_local FEntry Cache[16];
		const uint32 Slot = (uint32)(((uint64)CellX * 83492791ull) ^ ((uint64)CellY * 2971215073ull)) & 15u;
		FEntry& E = Cache[Slot];
		if (!(E.bValid && E.CellX == CellX && E.CellY == CellY && E.Seed == (uint32)Seed && E.ParamsHash == P.GetHash()))
		{
			E.CellX = CellX;
			E.CellY = CellY;
			E.Seed = (uint32)Seed;
			E.ParamsHash = P.GetHash();
			E.Ground = FMOWorldGenDomain::SampleHeightCm(P, B.CenterX, B.CenterY, Seed, P.Tuning.VoxelSizeCm);
			E.bAllowed = SettleEntrance(P, B, E.Ground, Seed);
			E.DirX = B.DirX;
			E.DirY = B.DirY;
			E.bValid = true;
		}
		B.DirX = E.DirX;
		B.DirY = E.DirY;
		OutGround = E.Ground;
		return E.bAllowed;
	}

	/** The entrance's distance (cm, negative inside) for the sample's cell. The passage and chamber need sample spacing fine enough to resolve a tube. */
	float EntranceDistance(const FMOWorldGenParams& P, double X, double Y, double Z, int32 Seed, float StepCm)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		const int64 CellX = (int64)FMath::FloorToDouble(X / (double)T.CaveEntranceCellCm);
		const int64 CellY = (int64)FMath::FloorToDouble(Y / (double)T.CaveEntranceCellCm);
		FEntranceBasics B;
		if (!EntranceBasics(T, CellX, CellY, Seed, B))
		{
			return FMOWorldGenDomain::CaveFarCm;
		}
		const float Dx = (float)(X - B.CenterX), Dy = (float)(Y - B.CenterY);
		const float H2 = Dx * Dx + Dy * Dy;
		const float Reach = EntranceReach(B);
		if (H2 > Reach * Reach)
		{
			return FMOWorldGenDomain::CaveFarCm;
		}
		float Ground = 0.0f;
		if (!EntranceGround(P, CellX, CellY, Seed, B, Ground))
		{
			return FMOWorldGenDomain::CaveFarCm;
		}
		const float Z0 = (float)Z;

		// The hollow: a spherical cap HollowDepth deep and HollowRadius across, so its rim meets the ground at ~35 deg (walkable) and its floor is gentle.
		const float Rs = (B.HollowRadius * B.HollowRadius + B.HollowDepth * B.HollowDepth) / (2.0f * B.HollowDepth);
		const float DzS = Z0 - (Ground - B.HollowDepth + Rs);
		float D = FMath::Sqrt(H2 + DzS * DzS) - Rs;

		if (StepCm <= EntrancePassageRadiusCm * TunnelStepFraction)
		{
			// The passage: from the hollow's floor, out through its side, down at EntranceGrade; flattened like a gallery (TunnelVerticalSquash).
			const float Lead = B.HollowRadius * 0.35f;
			const float Ax = B.DirX * Lead, Ay = B.DirY * Lead, Az = Ground - B.HollowDepth + EntrancePassageRadiusCm * 0.8f;
			const float Vx = B.DirX * B.Run, Vy = B.DirY * B.Run, Vz = -B.Run * EntranceGrade;
			const float Sq = T.TunnelVerticalSquash;
			const float Px = Dx - Ax, Py = Dy - Ay, Pz = (Z0 - Az) * Sq;
			const float VzS = Vz * Sq;
			const float T01 = FMath::Clamp((Px * Vx + Py * Vy + Pz * VzS) / (Vx * Vx + Vy * Vy + VzS * VzS), 0.0f, 1.0f);
			const float Qx = Px - T01 * Vx, Qy = Py - T01 * Vy, Qz = Pz - T01 * VzS;
			const float Dp = FMath::Sqrt(Qx * Qx + Qy * Qy + Qz * Qz) - EntrancePassageRadiusCm;

			// The chamber at its end: a flattened sphere sitting on the passage's floor.
			const float Cx = Ax + Vx - Dx, Cy = Ay + Vy - Dy;
			const float Cz = (Z0 - (Az + Vz + 0.3f * B.ChamberRadius)) * 1.5f;
			const float Dc = FMath::Sqrt(Cx * Cx + Cy * Cy + Cz * Cz) - B.ChamberRadius;
			D = SmoothMin(D, SmoothMin(Dp, Dc, 200.0f), 250.0f);
		}
		return D;
	}
}

float FMOWorldGenDomain::SampleCaveDistanceAtGround(const FMOWorldGenParams& Params, double X, double Y, double Z, float GroundCm, int32 Seed, float StepCm)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableCaves)
	{
		return CaveFarCm;
	}
	const float Caves = SampleCaveDistanceUnderGround(Params, X, Y, Z, GroundCm, Seed, StepCm);
	if (T.RavineChance <= 0.0f || Z > (double)GroundCm + 100.0)
	{
		return Caves;
	}
	return FMath::Clamp(FMath::Min(RavineDistance(Params, X, Y, Z, Seed, StepCm), Caves), -CaveFarCm, CaveFarCm);
}

float FMOWorldGenDomain::SampleCaveDistanceUnderGround(const FMOWorldGenParams& Params, double X, double Y, double Z, float GroundCm, int32 Seed, float StepCm)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableCaves)
	{
		return CaveFarCm;
	}

	// Depth under THIS column's ground. Air above the ground needs no carving, and below the cave band there is nothing.
	const float Depth = GroundCm - (float)Z;
	if (Depth < -100.0f || Depth > T.CaveMaxDepthCm + T.CaveFadeCm)
	{
		return CaveFarCm;
	}

	float D = CaveFarCm;

	// No caves under the high ground (the mountain ranges): 0 below the taper band, 1 at / above the ceiling. A gate like the others: the penalty it adds keeps the field continuous.
	const float HighGround = T.CaveMaxGroundCm > 0.0f ? Smooth01(T.CaveMaxGroundCm - T.CaveHighFadeCm, T.CaveMaxGroundCm, GroundCm) : 0.0f;

	// No cave below the water floor or near water (v10): one gate for every feature below.
	const float Wet = HighGround < 1.0f ? WaterGate(Params, X, Y, Z, Seed) : 1.0f;

	// Sinkholes: the only features that reach the ground. Skipped when the sample spacing cannot resolve a shaft.
	if (StepCm <= T.SinkholeRadiusCm * SinkholeStepFraction && HighGround < 1.0f)
	{
		D = FMath::Min(D, SinkholeDistance(Params, X, Y, Z, Seed) + CaveFarCm * FMath::Max(HighGround, Wet));
	}

	// Built entrances (F30): a hollow with a passage down into a chamber. Gated like the sinkholes (and dry + karst at both ends, uphill, covered: SettleEntrance).
	if (T.CaveEntranceChance > 0.0f && StepCm <= EntranceHollowMinRadiusCm * EntranceHollowStepFraction && HighGround < 1.0f)
	{
		D = FMath::Min(D, EntranceDistance(Params, X, Y, Z, Seed, StepCm) + CaveFarCm * FMath::Max(HighGround, Wet));
	}

	// Everything else stays off low ground (coasts / basins) and out of the water. Each gate is a 0..1 factor that adds a penalty of at least CaveFarCm when
	// it is 1, so the field is continuous where a feature is skipped (skipped == saturated).
	const float LowGround = FMath::Max(Wet, 1.0f - Smooth01(T.SeaLevelCm + T.CaveMinGroundAboveSeaCm, T.SeaLevelCm + T.CaveMinGroundAboveSeaCm + 600.0f, GroundCm));
	if (LowGround < 1.0f && HighGround < 1.0f)
	{
		// Galleries: evaluated up to the surface too -- GalleryDistance keeps the cover except where a passage opens into a steep slope.
		if (StepCm <= T.TunnelRadiusCm * TunnelStepFraction)
		{
			D = FMath::Min(D, GalleryDistance(Params, X, Y, Z, GroundCm, Depth, Seed, LowGround, HighGround));
		}
	}
	if (Depth > T.CaveCoverCm && LowGround < 1.0f && HighGround < 1.0f)
	{
		const float DeepFade = Smooth01(T.CaveMaxDepthCm, T.CaveMaxDepthCm + T.CaveFadeCm, Depth);

		// Caverns: where a smooth 3D field rises above the threshold.
		if (Depth > T.CavernMinDepthCm && StepCm <= CavernMaxStepCm)
		{
			const float Lambda = T.CavernWavelengthCm;
			const double Inv = 1.0 / (double)Lambda;
			const double Px = X * Inv, Py = Y * Inv, Pz = Z * Inv * (double)CavernSquash;
			const float C = 0.7f * GradientNoise3(Px, Py, Pz, (uint32)HashSeed(Seed, SaltCavernA))
				+ 0.3f * GradientNoise3(Px * 2.0 + 5.3, Py * 2.0 - 2.9, Pz * 2.0 + 8.1, (uint32)HashSeed(Seed, SaltCavernB));
			const float Dc = (T.CavernThreshold - C) * (Lambda / CavernGradient);
			const float CavernShallow = 1.0f - Smooth01(T.CavernMinDepthCm, T.CavernMinDepthCm + T.CaveFadeCm, Depth);
			// The deepest interior is (Cmax - threshold) * wavelength / gradient <= 0.4 * wavelength over the allowed threshold range; cancel it and add CaveFarCm.
			D = FMath::Min(D, Dc + (0.4f * Lambda + CaveFarCm) * FMath::Max(FMath::Max3(CavernShallow, DeepFade, LowGround), HighGround));
		}
	}

	return FMath::Clamp(D, -CaveFarCm, CaveFarCm);
}

int32 FMOWorldGenDomain::CaveMaxLOD(const FMOWorldGenParams& Params)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	const float MaxStepCm = FMath::Max3(CavernMaxStepCm, T.SinkholeRadiusCm * SinkholeStepFraction, T.TunnelRadiusCm * TunnelStepFraction);
	const float VoxelCm = FMath::Max(T.VoxelSizeCm, 1.0f);
	int32 LOD = 0;
	while (LOD < 24 && VoxelCm * (float)(1 << (LOD + 1)) <= MaxStepCm)
	{
		++LOD;
	}
	return LOD;
}

float FMOWorldGenDomain::SampleCaveDistanceCm(const FMOWorldGenParams& Params, double X, double Y, double Z, int32 Seed, float StepCm)
{
	if (!Params.Tuning.bEnableCaves)
	{
		return CaveFarCm;
	}
	return SampleCaveDistanceAtGround(Params, X, Y, Z, SampleHeightCm(Params, X, Y, Seed, StepCm), Seed, StepCm);
}

bool FMOWorldGenDomain::GetSinkholeInCell(const FMOWorldGenParams& Params, int64 CellX, int64 CellY, int32 Seed, FMOSinkhole& Out)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	FSinkholeBasics B;
	if (!T.bEnableCaves || !SinkholeBasics(T, CellX, CellY, Seed, B))
	{
		return false;
	}
	const float Ground = SinkholeGroundCm(Params, CellX, CellY, Seed, B);
	if (!SinkholeAllowed(Params, B, Ground, Seed))
	{
		return false;
	}
	Out.Center = FVector2D(B.CenterX, B.CenterY);
	Out.RadiusCm = B.Radius;
	Out.DepthCm = B.Depth;
	Out.GroundCm = Ground;
	return true;
}

bool FMOWorldGenDomain::FindSinkholeAt(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, FMOSinkhole& Out)
{
	const double Cell = (double)Params.Tuning.SinkholeCellCm;
	return GetSinkholeInCell(Params, (int64)FMath::FloorToDouble(X / Cell), (int64)FMath::FloorToDouble(Y / Cell), Seed, Out);
}

float FMOWorldGenDomain::DistanceToSinkholeRimZone(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	// Every opening's keep-out: a sinkhole's rim zone, a built entrance's hollow and a ravine (vegetation, world features and spawns read this one query).
	const float Entrance = FMath::Min(DistanceToCaveEntranceHollow(Params, X, Y, Seed), RavineZoneDistance(Params, X, Y, Seed));
	FMOSinkhole S;
	if (!FindSinkholeAt(Params, X, Y, Seed, S))
	{
		return Entrance;
	}
	// The flared rim (1.8x) with its +25% wall wobble reaches ~2.3x the mean radius; plant nothing inside that.
	const float Dist = (float)FVector2D::Distance(FVector2D(X, Y), S.Center);
	return FMath::Min(Dist - S.RadiusCm * 2.3f, Entrance);
}

bool FMOWorldGenDomain::GetCaveEntranceInCell(const FMOWorldGenParams& Params, int64 CellX, int64 CellY, int32 Seed, FMOCaveEntranceHollow& Out)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableCaves || T.CaveEntranceChance <= 0.0f)
	{
		return false;
	}
	FEntranceBasics B;
	float Ground = 0.0f;
	if (!EntranceBasics(T, CellX, CellY, Seed, B) || !EntranceGround(Params, CellX, CellY, Seed, B, Ground))
	{
		return false;
	}
	Out.Center = FVector2D(B.CenterX, B.CenterY);
	Out.RadiusCm = B.HollowRadius;
	Out.DepthCm = B.HollowDepth;
	Out.GroundCm = Ground;
	return true;
}

bool FMOWorldGenDomain::GetFloatingIslandInCell(const FMOWorldGenParams& Params, int64 CellX, int64 CellY, int32 Seed, FMOFloatingIsland& Out)
{
	FIslandBasics B;
	if (!IslandBasics(Params.Tuning, CellX, CellY, Seed, B))
	{
		return false;
	}
	const float Ground = AnchorGroundCm(Params, SaltIsland, CellX, CellY, Seed, B.CenterX, B.CenterY);
	if (!IslandAllowed(Params, Ground))
	{
		return false;
	}
	Out.Center = FVector2D(B.CenterX, B.CenterY);
	Out.RadiusCm = B.Radius;
	Out.TopCm = Ground + B.Height;
	Out.ThicknessCm = B.Thickness;
	Out.GroundCm = Ground;
	Out.NumClumps = FMath::Min(B.NumClumps, FMOFloatingIsland::MaxClumps);
	for (int32 K = 0; K < Out.NumClumps; ++K)
	{
		Out.ClumpCenter[K] = FVector2D(B.Clumps[K].X, B.Clumps[K].Y);
		Out.ClumpRadiusCm[K] = B.Clumps[K].R;
		Out.ClumpLiftCm[K] = B.Clumps[K].Lift;
		Out.ClumpThicknessCm[K] = B.Clumps[K].Thickness;
	}
	return true;
}

bool FMOWorldGenDomain::GetRavineInCell(const FMOWorldGenParams& Params, int64 CellX, int64 CellY, int32 Seed, FMORavine& Out)
{
	FRavineBasics B;
	if (!Params.Tuning.bEnableCaves || !RavineBasics(Params.Tuning, CellX, CellY, Seed, B))
	{
		return false;
	}
	const float Ground = AnchorGroundCm(Params, SaltRavine, CellX, CellY, Seed, B.CenterX, B.CenterY);
	if (!RavineAllowed(Params, B, Ground, Seed))
	{
		return false;
	}
	Out.Center = FVector2D(B.CenterX, B.CenterY);
	Out.Dir = FVector2D(B.DirX, B.DirY);
	Out.LengthCm = B.Length;
	Out.HalfWidthCm = B.HalfWidth;
	Out.DepthCm = RavineDepth(Params, B, Ground);
	Out.GroundCm = Ground;
	return true;
}

float FMOWorldGenDomain::IslandTopCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	FIslandBasics B;
	float Ground = 0.0f;
	if (!FindIslandAt(Params, X, Y, Seed, B, Ground))
	{
		return -1.0e9f;
	}
	return IslandTopAt(B, Ground, X, Y, Seed);   // -1e9 off every clump
}

float FMOWorldGenDomain::IslandRockDistanceCm(const FMOWorldGenParams& Params, double X, double Y, double Z, int32 Seed)
{
	FIslandBasics B;
	float Ground = 0.0f;
	if (!FindIslandAt(Params, X, Y, Seed, B, Ground))
	{
		return 1.0e6f;
	}
	return IslandRockAt(B, IslandColumnAt(B, Ground, X, Y, Seed), X, Y, Z, Seed);
}

float FMOWorldGenDomain::IslandFarDistanceCm(const FMOWorldGenParams& Params, double X, double Y, double Z, int32 Seed, float GrowCm, float ThickenCm)
{
	FIslandBasics B;
	float Ground = 0.0f;
	if (!FindIslandAt(Params, X, Y, Seed, B, Ground))
	{
		return 1.0e6f;
	}
	const FIslandColumn& Col = IslandColumnAt(B, Ground, X, Y, Seed);
	float Rock = 1.0e9f;
	for (int32 K = 0; K < B.NumClumps; ++K)
	{
		const float Outline = Col.Outline[K] - GrowCm;
		Rock = FMath::Min(Rock, Outline > 0.0f ? Outline : FMath::Max3(Col.Under2D[K] - ThickenCm - (float)Z, (float)Z - Col.Top[K], Outline));
	}
	return Rock;
}

bool FMOWorldGenDomain::SampleIslandSurface(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float InsetCm, FMOIslandSurfacePoint& Out)
{
	FIslandBasics B;
	float Ground = 0.0f;
	if (!FindIslandAt(Params, X, Y, Seed, B, Ground))
	{
		return false;
	}
	const float Top = IslandTopAt(B, Ground, X, Y, Seed);
	if (Top < -1.0e8f)
	{
		return false;
	}
	// Solid under the top: near the rim the torn underside can come up past the top, and the column has a top but no rock (the rim's holes).
	constexpr float SolidCm = 100.0f;
	if (IslandRockAt(B, IslandColumnAt(B, Ground, X, Y, Seed), X, Y, (double)Top - SolidCm, Seed) >= 0.0f)
	{
		return false;
	}
	const double D = FMath::Max(InsetCm, 1.0f);
	const float East = IslandTopAt(B, Ground, X + D, Y, Seed);
	const float West = IslandTopAt(B, Ground, X - D, Y, Seed);
	const float North = IslandTopAt(B, Ground, X, Y + D, Seed);
	const float South = IslandTopAt(B, Ground, X, Y - D, Seed);
	if (FMath::Min(FMath::Min(East, West), FMath::Min(North, South)) < -1.0e8f)
	{
		return false;   // the edge is nearer than that
	}
	Out.TopCm = Top;
	Out.Normal = FVector((double)(West - East) / (2.0 * D), (double)(South - North) / (2.0 * D), 1.0).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	Out.OriginGroundCm = Ground;
	return true;
}

bool FMOWorldGenDomain::IslandReachesBox(const FMOWorldGenParams& Params, double MinX, double MinY, double MaxX, double MaxY, int32 Seed)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (T.IslandChance <= 0.0f)
	{
		return false;
	}
	const double Cell = (double)T.IslandCellCm;
	const int64 X0 = (int64)FMath::FloorToDouble(MinX / Cell), X1 = (int64)FMath::FloorToDouble(MaxX / Cell);
	const int64 Y0 = (int64)FMath::FloorToDouble(MinY / Cell), Y1 = (int64)FMath::FloorToDouble(MaxY / Cell);
	for (int64 Cy = Y0; Cy <= Y1; ++Cy)
	{
		for (int64 Cx = X0; Cx <= X1; ++Cx)
		{
			FIslandBasics B;
			if (!IslandBasics(T, Cx, Cy, Seed, B))
			{
				continue;
			}
			const double Dx = FMath::Max3(MinX - B.CenterX, 0.0, B.CenterX - MaxX), Dy = FMath::Max3(MinY - B.CenterY, 0.0, B.CenterY - MaxY);
			if (Dx * Dx + Dy * Dy <= FMath::Square(1.3 * (double)B.Radius + 100.0)
				&& IslandAllowed(Params, AnchorGroundCm(Params, SaltIsland, Cx, Cy, Seed, B.CenterX, B.CenterY)))
			{
				return true;
			}
		}
	}
	return false;
}

float FMOWorldGenDomain::SampleOpeningDistanceUnderCm(const FMOWorldGenParams& Params, double X, double Y, double GroundZ, int32 Seed)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableCaves)
	{
		return CaveFarCm;
	}
	// Under the point's base. The two features' own early-outs (out of reach: CaveFarCm after one cell hash) keep this cheap for every other point;
	// in reach, the cave field itself decides -- the same gates (water, high ground) the stamp carves with, and a passage or gallery there too.
	const double Under = GroundZ - 30.0;
	float Near = SinkholeDistance(Params, X, Y, Under, Seed);
	if (T.CaveEntranceChance > 0.0f)
	{
		Near = FMath::Min(Near, EntranceDistance(Params, X, Y, Under, Seed, T.VoxelSizeCm));
	}
	if (T.RavineChance > 0.0f)
	{
		Near = FMath::Min(Near, RavineDistance(Params, X, Y, Under, Seed, T.VoxelSizeCm));
	}
	if (Near >= CaveFarCm)
	{
		return CaveFarCm;
	}
	return SampleCaveDistanceAtGround(Params, X, Y, Under, (float)GroundZ, Seed, T.VoxelSizeCm);
}

float FMOWorldGenDomain::DistanceToCaveEntranceHollow(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableCaves || T.CaveEntranceChance <= 0.0f)
	{
		return NoSinkholeDistanceCm;
	}
	const int64 CellX = (int64)FMath::FloorToDouble(X / (double)T.CaveEntranceCellCm);
	const int64 CellY = (int64)FMath::FloorToDouble(Y / (double)T.CaveEntranceCellCm);
	FEntranceBasics B;
	float Ground = 0.0f;
	if (!EntranceBasics(T, CellX, CellY, Seed, B) || !EntranceGround(Params, CellX, CellY, Seed, B, Ground))
	{
		return NoSinkholeDistanceCm;
	}
	// The hollow's rim plus a margin (the passage is open only inside the hollow; beyond it the rock is whole).
	return (float)FVector2D::Distance(FVector2D(X, Y), FVector2D(B.CenterX, B.CenterY)) - B.HollowRadius * 1.2f;
}
