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
			// The underside is one height per column (v14, N-331; Wes, Nov 5: "make the bottom of the island a little smoother in terms of the voxels and just use large rocks to make it look more jagged and
			// craggy"). A cone with broad ridged folds; the crags are rocks (UMOWorldFeatureSubsystem::SpawnIslandRocks). With one height per column no piece of the underside can hang loose, and the far
			// stand-in draws this same surface, so it no longer changes shape as an island's LOD changes. The rim keeps a minimum thickness: a knife edge read as holes.
			const double Lu = FMath::Max(1500.0, 0.2 * (double)C.R);
			const float Folds = 1.0f - 2.0f * FMath::Abs(GradientNoise2(X / Lu, Y / Lu, ShapeSalt + 60u + (uint32)K).Value);
			const float Cone = C.Thickness * FMath::Pow(Deep01, 0.6f) * (1.0f + FMOFloatingIsland::UndersideFoldShare * Folds);
			Col.Under2D[K] = Top - FMath::Max(Cone, FMOFloatingIsland::RimThicknessCm);
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

	/** The island's rock (cm, negative inside a clump): each clump's rock lies between its underside and its top; off the cluster's footprint, the least outline. */
	float IslandRockAt(const FIslandBasics& B, const FIslandColumn& Col, double Z)
	{
		if (Col.Union >= 0.0f)
		{
			return FMath::Max(Col.Union, 1.0f);
		}
		float Rock = 1.0e9f;   // the nearest clump's rock: negative inside one
		for (int32 K = 0; K < B.NumClumps; ++K)
		{
			Rock = FMath::Min(Rock, Col.Outline[K] > 0.25f * B.Clumps[K].Thickness
				? Col.Outline[K]   // well off this clump: its outline is the distance
				: FMath::Max3(Col.Under2D[K] - (float)Z, (float)Z - Col.Top[K], Col.Outline[K]));
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

	/** A deep cave's jitter margin (share of the cell): its centre keeps to the middle third, so its branches have room and stay in the cell. */
	constexpr float DeepEntranceMargin = 0.33f;
	constexpr float EntranceMargin = 0.18f;
	// An overhang is a shelter cut into a hillside (N-357): native to whatever is there, in any rock, the hill's own ground and plants right up to its lip and over its roof. Only the dome is carved, so only it wears bare rock.
	/** The slopes that hold one: over 20 deg a roof forms within a dome's depth, past 50 deg a mouth is a cliff. */
	constexpr double OverhangMinSlope = 0.364, OverhangMaxSlope = 1.19;
	/** Places in its cell it tries for such a slope. Flat country has none: an overhang there simply isn't. */
	constexpr int32 OverhangTries = 32;
	/** A dome is about 10 m across: fine enough spacing holds it to a few hundred metres off. */
	constexpr float OverhangMaxStepCm = 400.0f;
	/** The rock wanted over the dome where the settle measures it. */
	constexpr float OverhangRoofCm = 80.0f;

	struct FEntranceBasics
	{
		double CenterX = 0.0, CenterY = 0.0;   // the hollow's centre (cm)
		float HollowRadius = 0.0f;             // across the top (cm)
		float HollowDepth = 0.0f;              // at its middle (cm)
		float DirX = 1.0f, DirY = 0.0f;        // the passage's heading in plan (unit, one of 16 exact directions)
		float Run = 0.0f;                      // the passage's length in plan (cm)
		float ChamberRadius = 0.0f;            // (cm)
		EMOCaveEntranceKind Kind = EMOCaveEntranceKind::Shallow;
		float ShelterHalfDepth = 0.0f, ShelterHalfWidth = 0.0f, ShelterHeight = 0.0f;   // an overhang's (cm)
	};

	/** A deep cave's branching passages past its chamber (BR23-18): straight runs down a walkable grade, each from where its parent ended; a passage that forks no further ends in a small room.
	 *  Offsets in plan from the hollow's centre, absolute Z. */
	struct FEntrancePassage
	{
		float Ax = 0.0f, Ay = 0.0f, Az = 0.0f;   // its start, on its axis
		float Vx = 0.0f, Vy = 0.0f, Vz = 0.0f;   // start to end
		float Radius = 0.0f;
		float RoomRadius = 0.0f;                 // 0 = no room at its end
	};
	constexpr int32 MaxEntrancePassages = 8;

	/** One bowl of an entrance (N-355): a spherical cap Depth deep, not quite round in plan: its rim at heading a lies at Radius x BowlStretch. Its own ground, so a bowl down a slope sits on that slope. Offsets in plan from the hollow's centre. */
	struct FEntranceBowl
	{
		float X = 0.0f, Y = 0.0f;
		float Radius = 0.0f, Depth = 0.0f;
		float Ground = 0.0f;
		float Mean = 1.0f;
		float C2 = 0.0f, S2 = 0.0f, C3 = 0.0f, S3 = 0.0f;   // the outline's two waves as cos / sin weights: polynomials of the heading's cos and sin, no libm in the field
	};
	constexpr int32 MaxEntranceBowls = 12;

	/** One shelter of an overhang (N-357): a half dome on a flat floor, its front edge where that floor meets the hillside. Its middle in plan from the entrance's centre, Dir the way into the hill. Not an exact ellipse: the same two waves as a bowl's outline. */
	struct FEntranceShelter
	{
		float X = 0.0f, Y = 0.0f;
		float DirX = 1.0f, DirY = 0.0f;
		float HalfDepth = 0.0f, HalfWidth = 0.0f, Height = 0.0f;
		float Floor = 0.0f;
		float C2 = 0.0f, S2 = 0.0f, C3 = 0.0f, S3 = 0.0f;
	};
	constexpr int32 MaxEntranceShelters = 5;

	FORCEINLINE float ShelterOutline(const FEntranceShelter& S, float Cos, float Sin)
	{
		return 1.0f + S.C2 * (Cos * Cos - Sin * Sin) + S.S2 * (2.0f * Sin * Cos) + S.C3 * Cos * (4.0f * Cos * Cos - 3.0f) + S.S3 * Sin * (3.0f - 4.0f * Sin * Sin);
	}

	/** How far out in its plan a column is: 1 at the dome's foot. */
	FORCEINLINE float ShelterShareAt(const FEntranceShelter& S, float Dx, float Dy)
	{
		const float Qx = Dx - S.X, Qy = Dy - S.Y;
		const float Along = (Qx * S.DirX + Qy * S.DirY) / S.HalfDepth, Across = (Qy * S.DirX - Qx * S.DirY) / S.HalfWidth;
		const float Run = FMath::Sqrt(Along * Along + Across * Across);
		return Run < 0.01f ? 0.0f : Run / ShelterOutline(S, Along / Run, Across / Run);
	}

	/** A half dome cut flat by its floor: nothing below the floor is carved, so a fire and a sleeper have level ground. Rough by a hand's breadth or so, so its ceiling and walls don't read as a machined ellipsoid. */
	float ShelterDistance(const FEntranceShelter& S, float Dx, float Dy, float Z, uint32 RoughSalt)
	{
		const float Qx = Dx - S.X, Qy = Dy - S.Y;
		const float Along = Qx * S.DirX + Qy * S.DirY, Across = Qy * S.DirX - Qx * S.DirY, Over = Z - S.Floor;
		const float Ux = Along / S.HalfDepth, Uy = Across / S.HalfWidth;
		const float Run = FMath::Sqrt(Ux * Ux + Uy * Uy);
		const float Outline = Run < 0.01f ? 1.0f : ShelterOutline(S, Ux / Run, Uy / Run);
		const float Rx = S.HalfDepth * Outline, Ry = S.HalfWidth * Outline, Rz = S.Height;
		const float K0 = FMath::Sqrt(Along * Along / (Rx * Rx) + Across * Across / (Ry * Ry) + Over * Over / (Rz * Rz));
		const float K1 = FMath::Sqrt(Along * Along / (Rx * Rx * Rx * Rx) + Across * Across / (Ry * Ry * Ry * Ry) + Over * Over / (Rz * Rz * Rz * Rz));
		float Dome = K1 > 1.0e-6f ? K0 * (K0 - 1.0f) / K1 : -Rz;
		if (Dome < 150.0f && Dome > -150.0f)
		{
			Dome += 28.0f * GradientNoise3((double)Dx / 230.0, (double)Dy / 230.0, (double)Z / 230.0, RoughSalt);
		}
		return FMath::Max(Dome, -Over);
	}
	/** The outline never narrower than 0.92 of the radius (its rim at most ~9 % steeper: 37 deg at the main bowl's 35), at most 1.22 wider. */
	constexpr float BowlMeanStretch = 1.07f;
	constexpr float BowlMaxStretch = 1.22f;

	FORCEINLINE float BowlStretch(const FEntranceBowl& Bowl, float Cos, float Sin)
	{
		return Bowl.Mean + Bowl.C2 * (Cos * Cos - Sin * Sin) + Bowl.S2 * (2.0f * Sin * Cos) + Bowl.C3 * Cos * (4.0f * Cos * Cos - 3.0f) + Bowl.S3 * Sin * (3.0f - 4.0f * Sin * Sin);
	}

	/** 0 at the bowl's middle, 1 at its rim. */
	FORCEINLINE float BowlShareAt(const FEntranceBowl& Bowl, float Dx, float Dy)
	{
		const float Run = FMath::Sqrt(Dx * Dx + Dy * Dy);
		return Run < 1.0f ? 0.0f : Run / (Bowl.Radius * BowlStretch(Bowl, Dx / Run, Dy / Run));
	}

	FORCEINLINE float BowlDistance(const FEntranceBowl& Bowl, float Dx, float Dy, float Z)
	{
		const float SphereR = (Bowl.Radius * Bowl.Radius + Bowl.Depth * Bowl.Depth) / (2.0f * Bowl.Depth);
		const float PlanSq = Dx * Dx + Dy * Dy;
		const float Run = FMath::Sqrt(PlanSq);
		const float Stretch = Run < 1.0f ? Bowl.Mean : BowlStretch(Bowl, Dx / Run, Dy / Run);
		const float Dz = Z - (Bowl.Ground - Bowl.Depth + SphereR);
		return FMath::Sqrt(PlanSq / (Stretch * Stretch) + Dz * Dz) - SphereR;   // a round bowl (Stretch exactly 1) is v12's cap to the bit
	}

	struct FEntranceTree
	{
		int32 Num = 0;
		FEntrancePassage Passages[MaxEntrancePassages];
		int32 NumBowls = 0;
		FEntranceBowl Bowls[MaxEntranceBowls];
		int32 NumShelters = 0;
		FEntranceShelter Shelters[MaxEntranceShelters];
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
		const uint32 H4 = HashLattice2(CellX, CellY, S + 4u);
		const float KindRoll = Unit16(H4);
		Out.Kind = KindRoll < T.CaveOverhangShare ? EMOCaveEntranceKind::Overhang : KindRoll < T.CaveOverhangShare + T.CaveDeepShare ? EMOCaveEntranceKind::Deep : EMOCaveEntranceKind::Shallow;
		const float MarginShare = Out.Kind == EMOCaveEntranceKind::Deep ? DeepEntranceMargin : EntranceMargin;
		Out.CenterX = ((double)CellX + (double)(MarginShare + (1.0f - 2.0f * MarginShare) * Unit16(H1))) * (double)T.CaveEntranceCellCm;
		Out.CenterY = ((double)CellY + (double)(MarginShare + (1.0f - 2.0f * MarginShare) * Unit16(H1 >> 16))) * (double)T.CaveEntranceCellCm;
		Out.HollowRadius = EntranceHollowMinRadiusCm + 400.0f * Unit16(H2);
		Out.HollowDepth = 350.0f + 150.0f * Unit16(H2 >> 16);
		const uint32 Dir = H3 & 15u;
		Out.DirX = (float)ShoreRing[Dir][0];
		Out.DirY = (float)ShoreRing[Dir][1];
		if (Out.Kind == EMOCaveEntranceKind::Overhang)
		{
			// Deeper than a passage's hollow: the roof is what stands between the shelter's height and the ground.
			Out.HollowDepth = 450.0f + 100.0f * Unit16(H2 >> 16);
			Out.ShelterHalfDepth = 550.0f + 150.0f * Unit16(H4 >> 16);
			Out.ShelterHalfWidth = 450.0f + 250.0f * Unit16(H3 >> 4);
			Out.ShelterHeight = 270.0f + 50.0f * Unit16(H3 >> 20);
			return true;
		}
		Out.ChamberRadius = 450.0f + 200.0f * Unit16(H3 >> 20);
		const float Margin = MarginShare * T.CaveEntranceCellCm;
		const float Fixed = Out.HollowRadius * 0.35f + Out.ChamberRadius + FMOWorldGenDomain::CaveFarCm + 300.0f;
		Out.Run = FMath::Min(2200.0f + 1000.0f * Unit16(H3 >> 4), Margin - Fixed);
		return Out.Run >= 800.0f;
	}

	/** Horizontal distance from the hollow's centre beyond which the entrance's field is >= CaveFarCm (skipping it there is exact). <= the jitter margin. */
	FORCEINLINE float EntranceReach(const FEntranceBasics& B, const FMOWorldGenTuning& T)
	{
		if (B.Kind == EMOCaveEntranceKind::Deep)
		{
			return DeepEntranceMargin * T.CaveEntranceCellCm;   // its branches keep CaveFarCm + 300 inside this (BuildEntranceTree)
		}
		// Its chained bowls keep inside the margin too (BuildEntranceBowls), and so do an overhang's shelters (SettleOverhang).
		const float Margin = EntranceMargin * T.CaveEntranceCellCm;
		if (B.Kind == EMOCaveEntranceKind::Overhang)
		{
			return Margin;
		}
		return FMath::Max(Margin, FMath::Max(B.HollowRadius * 1.6f, B.HollowRadius * 0.35f + B.Run + B.ChamberRadius) + FMOWorldGenDomain::CaveFarCm + 300.0f);
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

	/**
	 * A deep cave's branches, laid out once its heading has settled: two or three passages from the main chamber (one on each side, sometimes one ahead), each forking once or twice more. Every run is
	 * kept only if it stays inside the cell's margin and has 3 m of rock over its middle and end, dry and on karst there; a passage with nothing past it ends in a room. All pure functions of the cell.
	 */
	void BuildEntranceTree(const FMOWorldGenParams& P, const FEntranceBasics& B, int64 CellX, int64 CellY, float GroundCm, int32 Seed, FEntranceTree& Out)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		const uint32 S = (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltEntrance) + 16u;
		const float Limit = DeepEntranceMargin * T.CaveEntranceCellCm - FMOWorldGenDomain::CaveFarCm - 300.0f;

		int32 Heading = 0;
		double BestDot = -2.0;
		for (int32 K = 0; K < 16; ++K)
		{
			const double Dot = ShoreRing[K][0] * B.DirX + ShoreRing[K][1] * B.DirY;
			if (Dot > BestDot) { BestDot = Dot; Heading = K; }
		}
		struct FTip { float X = 0.0f, Y = 0.0f, Z = 0.0f; int32 Heading = 0; };
		const auto TryRun = [&](const FTip& From, int32 Dir, float Run, float Grade, float Radius, FTip& OutTip) -> bool
		{
			if (Out.Num >= MaxEntrancePassages)
			{
				return false;
			}
			const float Dx = (float)ShoreRing[Dir & 15][0], Dy = (float)ShoreRing[Dir & 15][1];
			const float RoomRoom = 450.0f;
			const float Ex = From.X + Dx * Run, Ey = From.Y + Dy * Run, Ez = From.Z - Run * Grade;
			if (Ex * Ex + Ey * Ey > FMath::Square(Limit - RoomRoom))
			{
				return false;
			}
			for (const float Share : { 0.5f, 1.0f })
			{
				const double X = B.CenterX + (double)(From.X + Dx * Run * Share), Y = B.CenterY + (double)(From.Y + Dy * Run * Share);
				const float Z = From.Z - Run * Grade * Share;
				if (FMOWorldGenDomain::SampleHeightCm(P, X, Y, Seed, T.VoxelSizeCm) - (Z + Radius) < 300.0f || WaterGate(P, X, Y, Z, Seed) > 0.0f)
				{
					return false;
				}
			}
			FEntrancePassage& Pass = Out.Passages[Out.Num++];
			Pass.Ax = From.X; Pass.Ay = From.Y; Pass.Az = From.Z;
			Pass.Vx = Dx * Run; Pass.Vy = Dy * Run; Pass.Vz = -Run * Grade;
			Pass.Radius = Radius;
			Pass.RoomRadius = 0.0f;   // the slot is reused from whichever cell this thread's cache held before: nothing of that may survive
			OutTip.X = Ex; OutTip.Y = Ey; OutTip.Z = Ez; OutTip.Heading = Dir;
			return true;
		};

		const float Lead = B.HollowRadius * 0.35f;
		FTip Chamber;
		Chamber.X = B.DirX * (Lead + B.Run);
		Chamber.Y = B.DirY * (Lead + B.Run);
		Chamber.Z = GroundCm - B.HollowDepth + EntrancePassageRadiusCm * 0.8f - B.Run * EntranceGrade;
		Chamber.Heading = Heading;

		const uint32 H0 = HashLattice2(CellX, CellY, S);
		const int32 First[3] = { Heading + 4 + (int32)(H0 & 3u), Heading - 4 - (int32)((H0 >> 2) & 3u), Heading - 1 + (int32)((H0 >> 4) & 3u) };
		const int32 NumFirst = 2 + (int32)((H0 >> 6) & 1u);
		for (int32 F = 0; F < NumFirst; ++F)
		{
			const uint32 H = HashLattice2(CellX, CellY, S + 1u + (uint32)F);
			FTip Tip;
			const int32 ParentIndex = Out.Num;
			if (!TryRun(Chamber, First[F], 2500.0f + 2000.0f * Unit16(H), 0.08f + 0.14f * Unit16(H >> 16), 150.0f + 40.0f * Unit16(H >> 8), Tip))
			{
				continue;
			}
			const uint32 H5 = HashLattice2(CellX, CellY, S + 8u + (uint32)F);
			const int32 NumForks = 1 + (int32)(H5 & 1u);
			int32 Forked = 0;
			for (int32 K = 0; K < NumForks; ++K)
			{
				const uint32 Hf = HashLattice2(CellX, CellY, S + 12u + (uint32)(F * 2 + K));
				const int32 Offset = (K == 0 ? 1 : -1) * (2 + (int32)(Hf & 1u) + (int32)((Hf >> 1) & 1u));
				FTip End;
				if (TryRun(Tip, Tip.Heading + Offset, 1500.0f + 1500.0f * Unit16(Hf >> 2), 0.05f + 0.15f * Unit16(Hf >> 16), 140.0f + 40.0f * Unit16(Hf >> 8), End))
				{
					Out.Passages[Out.Num - 1].RoomRadius = 300.0f + 150.0f * Unit16(Hf >> 24);
					++Forked;
				}
			}
			if (Forked == 0)
			{
				Out.Passages[ParentIndex].RoomRadius = 300.0f + 150.0f * Unit16(H5 >> 16);
			}
		}
	}

	/**
	 * An entrance's bowls, laid out once its heading has settled (N-355, Wes Nov 6: "as with lakes, make them non uniform, right now they're like perfect little circles, chain some together, give then non-uniformity, random
	 * scatter of like 2-5 of them"; said of the overhangs, which then became native to their hillsides: SettleOverhang). Every bowl has an outline that isn't round and a lobe or two overlapping it, and a chain of 1-5 bowls runs off the main one, away from the way in. A smaller bowl is proportionally shallower, so
	 * its rim is no steeper than the main one's and walks the same. A bowl stands only where it leaves the way in its rock (the passage's first stretch), keeps inside the cell's margin (the entrance's whole
	 * field stays in its cell), and sits on ground that is dry, karst and not steep. All pure functions of the cell.
	 */
	void BuildEntranceBowls(const FMOWorldGenParams& P, const FEntranceBasics& B, int64 CellX, int64 CellY, float GroundCm, int32 Seed, FEntranceTree& Out)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		uint32 Salt = (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltEntrance) + 40u;
		const auto NextHash = [&]() { return HashLattice2(CellX, CellY, Salt++); };

		const auto Outline = [&](FEntranceBowl& Bowl)
		{
			const uint32 H = NextHash();
			const uint32 K = NextHash();
			const float A2 = 0.06f + 0.04f * Unit16(H), A3 = 0.03f + 0.02f * Unit16(H >> 16);
			Bowl.Mean = BowlMeanStretch;
			Bowl.C2 = A2 * (float)ShoreRing[K & 15u][0];
			Bowl.S2 = A2 * (float)ShoreRing[K & 15u][1];
			Bowl.C3 = A3 * (float)ShoreRing[(K >> 4) & 15u][0];
			Bowl.S3 = A3 * (float)ShoreRing[(K >> 4) & 15u][1];
		};

		// The way in keeps its rock: the passage's first stretch.
		const float WayFrom = B.HollowRadius * 0.5f;
		const float WayTo = B.HollowRadius * 1.3f;
		const float WayRadius = EntrancePassageRadiusCm + 200.0f;
		const float Limit = (B.Kind == EMOCaveEntranceKind::Deep ? DeepEntranceMargin : EntranceMargin) * T.CaveEntranceCellCm - 1000.0f;
		const float DepthShare = FMath::Min(B.HollowDepth / B.HollowRadius, 0.32f);

		const auto TryAdd = [&](float X, float Y, float Radius) -> bool
		{
			if (Out.NumBowls >= MaxEntranceBowls || FMath::Sqrt(X * X + Y * Y) + Radius * BowlMaxStretch > Limit)
			{
				return false;
			}
			const float Along = FMath::Clamp(X * B.DirX + Y * B.DirY, WayFrom, WayTo);
			const float Qx = X - B.DirX * Along, Qy = Y - B.DirY * Along;
			const float Need = Radius * BowlMaxStretch + WayRadius;
			if (Qx * Qx + Qy * Qy < Need * Need)
			{
				return false;
			}
			const double Wx = B.CenterX + (double)X, Wy = B.CenterY + (double)Y;
			const float Ground = FMOWorldGenDomain::SampleHeightCm(P, Wx, Wy, Seed, T.VoxelSizeCm);
			const float Depth = FMath::Max(120.0f, Radius * DepthShare);
			if (Ground < T.SeaLevelCm + T.CaveMinGroundAboveSeaCm + 600.0f || (T.CaveMaxGroundCm > 0.0f && Ground > T.CaveMaxGroundCm - T.CaveHighFadeCm)
				|| GroundGradient(P, Wx, Wy, Ground, Seed).Size() > 0.364 || WaterGate(P, Wx, Wy, Ground - Depth, Seed) > 0.0f)
			{
				return false;
			}
			FEntranceBowl& Bowl = Out.Bowls[Out.NumBowls++];
			Bowl.X = X;
			Bowl.Y = Y;
			Bowl.Radius = Radius;
			Bowl.Depth = Depth;
			Bowl.Ground = Ground;
			Outline(Bowl);
			return true;
		};

		const auto AddLobes = [&](const FEntranceBowl Parent, int32 Count)
		{
			for (int32 L = 0; L < Count; ++L)
			{
				const uint32 H = NextHash();
				const double* Dir = ShoreRing[H & 15u];
				const float Off = Parent.Radius * (0.45f + 0.3f * Unit16(H >> 4));
				TryAdd(Parent.X + (float)Dir[0] * Off, Parent.Y + (float)Dir[1] * Off, Parent.Radius * (0.55f + 0.25f * Unit16(H >> 16)));
			}
		};

		Out.NumBowls = 1;
		FEntranceBowl& Main = Out.Bowls[0];
		Main = FEntranceBowl();
		Main.Radius = B.HollowRadius;
		Main.Depth = B.HollowDepth;
		Main.Ground = GroundCm;
		if (!T.bCaveBowlsVaried)
		{
			return;
		}
		Outline(Main);
		AddLobes(Main, 2);

		// The chain: 1-5 bowls in all, wandering away from the way in, each just overlapping the last, just touching it, or a few metres on (seen in game: at more overlap than this a chain reads as one blob).
		int32 Heading = 0;
		double BestDot = 2.0;
		for (int32 K = 0; K < 16; ++K)
		{
			const double Dot = ShoreRing[K][0] * B.DirX + ShoreRing[K][1] * B.DirY;
			if (Dot < BestDot) { BestDot = Dot; Heading = K; }
		}
		const uint32 ChainHash = NextHash();
		const float Roll = Unit16(ChainHash);
		const int32 Chained = Roll < 0.2f ? 0 : Roll < 0.45f ? 1 : Roll < 0.7f ? 2 : Roll < 0.9f ? 3 : 4;
		Heading += (int32)((ChainHash >> 16) % 9u) - 4;
		FEntranceBowl Last = Main;
		for (int32 C = 0; C < Chained; ++C)
		{
			const uint32 H = NextHash();
			const float Radius = B.HollowRadius * (0.35f + 0.4f * Unit16(H >> 3));
			const float Step = (Last.Radius + Radius) * (0.95f + 0.45f * Unit16(H >> 16));
			const double* Dir = ShoreRing[Heading & 15];
			if (!TryAdd(Last.X + (float)Dir[0] * Step, Last.Y + (float)Dir[1] * Step, Radius))
			{
				break;
			}
			Last = Out.Bowls[Out.NumBowls - 1];
			AddLobes(Last, 1);
			Heading += (int32)(H & 3u) * ((H & 4u) ? 1 : -1);
		}
	}

	/** One shelter with its mouth at (MouthX, MouthY), or false if the ground there won't hold one. Mouth and middle are world XY here; the caller makes them offsets. */
	bool SettleShelter(const FMOWorldGenParams& P, double MouthX, double MouthY, float HalfDepth, float HalfWidth, float Height, int32 Seed, FEntranceShelter& Out, double& OutMiddleX, double& OutMiddleY)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		const float Floor = FMOWorldGenDomain::SampleHeightCm(P, MouthX, MouthY, Seed, T.VoxelSizeCm);
		if (Floor < T.SeaLevelCm + T.CaveMinGroundAboveSeaCm + 600.0f)
		{
			return false;
		}
		const FVector2D Rise = GroundGradient(P, MouthX, MouthY, Floor, Seed);
		const double Slope = Rise.Size();
		if (Slope < OverhangMinSlope || Slope > OverhangMaxSlope)
		{
			return false;
		}
		int32 BestK = 0;
		double BestDot = -2.0e9;
		for (int32 K = 0; K < 16; ++K)
		{
			const double Dot = ShoreRing[K][0] * Rise.X + ShoreRing[K][1] * Rise.Y;
			if (Dot > BestDot) { BestDot = Dot; BestK = K; }
		}
		const double Dx = ShoreRing[BestK][0], Dy = ShoreRing[BestK][1];
		// Far enough in that the hill has climbed over the dome by its back half, on gentler ground a longer dome. Its front edge is the mouth, so you walk in level from the hillside.
		const float Depth = FMath::Clamp(FMath::Max(HalfDepth, 0.8f * (Height + 60.0f) / (float)Slope), 400.0f, 1000.0f);
		const double MiddleX = MouthX + Dx * Depth, MiddleY = MouthY + Dy * Depth;
		// The roof: real ground over the dome at the back half's middle, near the back, and to each side.
		const float Checks[4][2] = { { 0.45f, 0.0f }, { 0.75f, 0.0f }, { 0.55f, 0.55f }, { 0.55f, -0.55f } };
		for (const float* Check : Checks)
		{
			const double At = (double)(Check[0] * Depth), Side = (double)(Check[1] * HalfWidth);
			const double X = MiddleX + Dx * At - Dy * Side, Y = MiddleY + Dy * At + Dx * Side;
			const float Top = Floor + Height * FMath::Sqrt(FMath::Max(0.0f, 1.0f - Check[0] * Check[0] - Check[1] * Check[1])) + 30.0f;
			if (FMOWorldGenDomain::SampleHeightCm(P, X, Y, Seed, T.VoxelSizeCm) - Top < OverhangRoofCm)
			{
				return false;
			}
		}
		// Dry: no water on its floor, none filling its mouth.
		float Surface = 0.0f;
		if ((FMOWorldGenDomain::GetWaterSurfaceCm(P, MouthX, MouthY, Seed, Surface) && Surface > Floor - 200.0f)
			|| (FMOWorldGenDomain::GetWaterSurfaceCm(P, MiddleX, MiddleY, Seed, Surface) && Surface > Floor - 200.0f)
			|| FMOWorldGenDomain::IsInUplandLakeZone(P, MiddleX, MiddleY, Seed))
		{
			return false;
		}
		Out.DirX = (float)Dx;
		Out.DirY = (float)Dy;
		Out.HalfDepth = Depth;
		Out.HalfWidth = HalfWidth;
		Out.Height = Height;
		Out.Floor = Floor;
		OutMiddleX = MiddleX;
		OutMiddleY = MiddleY;
		return true;
	}

	/**
	 * An overhang looks for its hillside (N-357, Wes Nov 6: "the overhangs should be "native" to their biome ... the native biome vegetation and such should go right up to the overhang, it's suppose to give the existing biome some nice
	 * variation not be its own little poi type thing"). Up to OverhangTries hashed places in the cell's middle; the first that holds one is its centre (its mouth). Then 0-4 more along the slope (his "random scatter of like 2-5"), each a
	 * shelter's width or so on from the last, for as long as the ground holds them and they stay inside the cell's margin. Any rock, any biome: this isn't a solution cave. All pure functions of the cell.
	 */
	bool SettleOverhang(const FMOWorldGenParams& P, FEntranceBasics& B, int64 CellX, int64 CellY, int32 Seed, float& OutGround, FEntranceTree& Out)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		uint32 Salt = (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltEntrance) + 300u;
		const auto NextHash = [&]() { return HashLattice2(CellX, CellY, Salt++); };
		const auto Waves = [&](FEntranceShelter& S)
		{
			const uint32 H = NextHash();
			const uint32 K = NextHash();
			const float A2 = 0.05f + 0.05f * Unit16(H), A3 = 0.03f + 0.03f * Unit16(H >> 16);
			S.C2 = A2 * (float)ShoreRing[K & 15u][0];
			S.S2 = A2 * (float)ShoreRing[K & 15u][1];
			S.C3 = A3 * (float)ShoreRing[(K >> 4) & 15u][0];
			S.S3 = A3 * (float)ShoreRing[(K >> 4) & 15u][1];
		};
		Out.NumShelters = 0;
		double MiddleX = 0.0, MiddleY = 0.0;
		for (int32 Try = 0; Try < OverhangTries && Out.NumShelters == 0; ++Try)
		{
			const uint32 H = NextHash();
			const double X = ((double)CellX + (double)(EntranceMargin + (1.0f - 2.0f * EntranceMargin) * Unit16(H))) * (double)T.CaveEntranceCellCm;
			const double Y = ((double)CellY + (double)(EntranceMargin + (1.0f - 2.0f * EntranceMargin) * Unit16(H >> 16))) * (double)T.CaveEntranceCellCm;
			if (SettleShelter(P, X, Y, B.ShelterHalfDepth, B.ShelterHalfWidth, B.ShelterHeight, Seed, Out.Shelters[0], MiddleX, MiddleY))
			{
				B.CenterX = X;
				B.CenterY = Y;
				Out.NumShelters = 1;
			}
		}
		if (Out.NumShelters == 0)
		{
			return false;
		}
		FEntranceShelter& First = Out.Shelters[0];
		First.X = (float)(MiddleX - B.CenterX);
		First.Y = (float)(MiddleY - B.CenterY);
		Waves(First);
		B.DirX = First.DirX;
		B.DirY = First.DirY;
		OutGround = First.Floor;

		const float Limit = EntranceMargin * T.CaveEntranceCellCm - 1000.0f;
		const uint32 ChainHash = NextHash();
		const float Roll = Unit16(ChainHash);
		const int32 More = Roll < 0.3f ? 0 : Roll < 0.6f ? 1 : Roll < 0.8f ? 2 : Roll < 0.93f ? 3 : 4;
		const float Side = (ChainHash >> 16) & 1u ? 1.0f : -1.0f;
		double MouthX = B.CenterX, MouthY = B.CenterY;
		float LastWidth = First.HalfWidth, LastDirX = First.DirX, LastDirY = First.DirY;
		for (int32 M = 0; M < More; ++M)
		{
			const uint32 H = NextHash();
			const uint32 H2 = NextHash();
			const float HalfWidth = 300.0f + 400.0f * Unit16(H), HalfDepth = 400.0f + 300.0f * Unit16(H >> 16), Height = 240.0f + 80.0f * Unit16(H2);
			// Along the slope, a width or so on, wandering a little up or down it.
			const float Step = (LastWidth + HalfWidth) * (1.1f + 0.7f * Unit16(H2 >> 16));
			const float Wander = 400.0f * (Unit16(H2 >> 8) - 0.5f);
			const double NextX = MouthX - (double)(Side * LastDirY * Step) + (double)(LastDirX * Wander);
			const double NextY = MouthY + (double)(Side * LastDirX * Step) + (double)(LastDirY * Wander);
			FEntranceShelter& Next = Out.Shelters[Out.NumShelters];
			if (!SettleShelter(P, NextX, NextY, HalfDepth, HalfWidth, Height, Seed, Next, MiddleX, MiddleY)
				|| FMath::Sqrt(FMath::Square((float)(MiddleX - B.CenterX)) + FMath::Square((float)(MiddleY - B.CenterY))) + Next.HalfDepth * 1.2f > Limit)
			{
				break;
			}
			Next.X = (float)(MiddleX - B.CenterX);
			Next.Y = (float)(MiddleY - B.CenterY);
			Waves(Next);
			++Out.NumShelters;
			MouthX = NextX;
			MouthY = NextY;
			LastWidth = HalfWidth;
			LastDirX = Next.DirX;
			LastDirY = Next.DirY;
		}
		return true;
	}

	/** Ground at the hollow's centre and whether the entrance is allowed there, memoised per thread like the sinkhole's ground (pure functions of params, seed
	 *  and cell: the cache only saves work). A chunk asks for the same cell thousands of times, and the slope check alone costs four height samples. */
	bool EntranceGround(const FMOWorldGenParams& P, int64 CellX, int64 CellY, int32 Seed, FEntranceBasics& B, float& OutGround, const FEntranceTree** OutTree = nullptr)
	{
		struct FEntry
		{
			int64 CellX = 0, CellY = 0;
			uint32 Seed = 0, ParamsHash = 0;
			float Ground = 0.0f;
			float DirX = 1.0f, DirY = 0.0f;
			double CenterX = 0.0, CenterY = 0.0;   // an overhang's is where it found its hillside
			bool bAllowed = false;
			bool bValid = false;
			FEntranceTree Tree;
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
			E.Tree.Num = 0;
			E.Tree.NumBowls = 0;
			E.Tree.NumShelters = 0;
			if (B.Kind == EMOCaveEntranceKind::Overhang)
			{
				E.Ground = 0.0f;
				E.bAllowed = SettleOverhang(P, B, CellX, CellY, Seed, E.Ground, E.Tree);
			}
			else
			{
				E.Ground = FMOWorldGenDomain::SampleHeightCm(P, B.CenterX, B.CenterY, Seed, P.Tuning.VoxelSizeCm);
				E.bAllowed = SettleEntrance(P, B, E.Ground, Seed);
				if (E.bAllowed && B.Kind == EMOCaveEntranceKind::Deep)
				{
					BuildEntranceTree(P, B, CellX, CellY, E.Ground, Seed, E.Tree);
				}
				if (E.bAllowed)
				{
					BuildEntranceBowls(P, B, CellX, CellY, E.Ground, Seed, E.Tree);
				}
			}
			E.DirX = B.DirX;
			E.DirY = B.DirY;
			E.CenterX = B.CenterX;
			E.CenterY = B.CenterY;
			E.bValid = true;
		}
		B.DirX = E.DirX;
		B.DirY = E.DirY;
		B.CenterX = E.CenterX;
		B.CenterY = E.CenterY;
		OutGround = E.Ground;
		if (OutTree)
		{
			*OutTree = &E.Tree;
		}
		return E.bAllowed;
	}

	/** The entrance's distance (cm, negative inside) for the sample's cell. The passage and chamber need sample spacing fine enough to resolve a tube. bOutNative: it is an overhang's, which isn't a cave: the caves' gates (water, karst, high ground) are not its (it settled its own). */
	float EntranceDistance(const FMOWorldGenParams& P, double X, double Y, double Z, int32 Seed, float StepCm, bool* bOutNative = nullptr)
	{
		const FMOWorldGenTuning& T = P.Tuning;
		const int64 CellX = (int64)FMath::FloorToDouble(X / (double)T.CaveEntranceCellCm);
		const int64 CellY = (int64)FMath::FloorToDouble(Y / (double)T.CaveEntranceCellCm);
		FEntranceBasics B;
		if (!EntranceBasics(T, CellX, CellY, Seed, B))
		{
			return FMOWorldGenDomain::CaveFarCm;
		}
		const bool bOverhang = B.Kind == EMOCaveEntranceKind::Overhang;
		const float Reach = EntranceReach(B, T);
		if (!bOverhang)   // an overhang's centre is only known once it has found its hillside
		{
			const float Fx0 = (float)(X - B.CenterX), Fy0 = (float)(Y - B.CenterY);
			if (Fx0 * Fx0 + Fy0 * Fy0 > Reach * Reach)
			{
				return FMOWorldGenDomain::CaveFarCm;
			}
		}
		else if (StepCm > OverhangMaxStepCm)
		{
			return FMOWorldGenDomain::CaveFarCm;
		}
		float Ground = 0.0f;
		const FEntranceTree* Tree = nullptr;
		if (!EntranceGround(P, CellX, CellY, Seed, B, Ground, &Tree))
		{
			return FMOWorldGenDomain::CaveFarCm;
		}
		const float Dx = (float)(X - B.CenterX), Dy = (float)(Y - B.CenterY);
		const float Z0 = (float)Z;
		if (bOverhang)
		{
			if (bOutNative)
			{
				*bOutNative = true;
			}
			if (Dx * Dx + Dy * Dy > Reach * Reach)
			{
				return FMOWorldGenDomain::CaveFarCm;
			}
			const uint32 RoughSalt = (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltEntrance) + 500u;
			float Shelter = FMOWorldGenDomain::CaveFarCm;
			for (int32 K = 0; K < Tree->NumShelters; ++K)
			{
				Shelter = FMath::Min(Shelter, ShelterDistance(Tree->Shelters[K], Dx, Dy, Z0, RoughSalt));
			}
			return Shelter;
		}

		// The bowls: each a spherical cap, its rim meeting the ground at ~35 deg (walkable) and its floor gentle (BuildEntranceBowls). The small ones stay while the sample spacing can hold them.
		float D = BowlDistance(Tree->Bowls[0], Dx, Dy, Z0);
		for (int32 K = 1; K < Tree->NumBowls; ++K)
		{
			const FEntranceBowl& Bowl = Tree->Bowls[K];
			if (StepCm <= Bowl.Radius * EntranceHollowStepFraction)
			{
				D = SmoothMin(D, BowlDistance(Bowl, Dx - Bowl.X, Dy - Bowl.Y, Z0), 200.0f);
			}
		}

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

			// A deep cave's branches past the chamber, each flattened like the passage, with its room.
			for (int32 K = 0; Tree && K < Tree->Num; ++K)
			{
				const FEntrancePassage& Pass = Tree->Passages[K];
				const float Qx0 = Dx - Pass.Ax, Qy0 = Dy - Pass.Ay, Qz0 = (Z0 - Pass.Az) * Sq;
				const float PassVzS = Pass.Vz * Sq;
				const float Along = FMath::Clamp((Qx0 * Pass.Vx + Qy0 * Pass.Vy + Qz0 * PassVzS) / (Pass.Vx * Pass.Vx + Pass.Vy * Pass.Vy + PassVzS * PassVzS), 0.0f, 1.0f);
				const float Rx = Qx0 - Along * Pass.Vx, Ry = Qy0 - Along * Pass.Vy, Rz = Qz0 - Along * PassVzS;
				float Branch = FMath::Sqrt(Rx * Rx + Ry * Ry + Rz * Rz) - Pass.Radius;
				if (Pass.RoomRadius > 0.0f)
				{
					const float Mx = Pass.Ax + Pass.Vx - Dx, My = Pass.Ay + Pass.Vy - Dy;
					const float Mz = (Z0 - (Pass.Az + Pass.Vz + 0.3f * Pass.RoomRadius)) * 1.5f;
					Branch = SmoothMin(Branch, FMath::Sqrt(Mx * Mx + My * My + Mz * Mz) - Pass.RoomRadius, 150.0f);
				}
				D = SmoothMin(D, Branch, 150.0f);
			}
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
	if (T.CaveEntranceChance > 0.0f && StepCm <= EntranceHollowMinRadiusCm * EntranceHollowStepFraction)
	{
		bool bNative = false;
		const float Entrance = EntranceDistance(Params, X, Y, Z, Seed, StepCm, &bNative);
		if (bNative)
		{
			D = FMath::Min(D, Entrance);   // an overhang: part of the hillside, not a cave
		}
		else if (HighGround < 1.0f)
		{
			D = FMath::Min(D, Entrance + CaveFarCm * FMath::Max(HighGround, Wet));
		}
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
	const FEntranceTree* Tree = nullptr;
	if (!EntranceBasics(T, CellX, CellY, Seed, B) || !EntranceGround(Params, CellX, CellY, Seed, B, Ground, &Tree))
	{
		return false;
	}
	Out.Center = FVector2D(B.CenterX, B.CenterY);
	Out.RadiusCm = B.HollowRadius;
	Out.DepthCm = B.HollowDepth;
	Out.GroundCm = Ground;
	Out.Kind = B.Kind;
	Out.Uphill = FVector2D(B.DirX, B.DirY);
	Out.ShelterMiddle = Out.Center;
	Out.ShelterHalfDepthCm = 0.0f;
	Out.ShelterHalfWidthCm = 0.0f;
	Out.ShelterHeightCm = 0.0f;
	Out.Shelters.Reset();
	for (int32 K = 0; Tree && K < Tree->NumShelters; ++K)
	{
		const FEntranceShelter& S = Tree->Shelters[K];
		FMOCaveEntranceHollow::FShelter& Copy = Out.Shelters.AddDefaulted_GetRef();
		Copy.Middle = Out.Center + FVector2D(S.X, S.Y);
		Copy.Uphill = FVector2D(S.DirX, S.DirY);
		Copy.HalfDepthCm = S.HalfDepth;
		Copy.HalfWidthCm = S.HalfWidth;
		Copy.HeightCm = S.Height;
		Copy.FloorCm = S.Floor;
	}
	if (B.Kind == EMOCaveEntranceKind::Overhang && Out.Shelters.Num() > 0)
	{
		// Center is its mouth, on the floor; nothing of it is a hollow.
		const FMOCaveEntranceHollow::FShelter& First = Out.Shelters[0];
		Out.RadiusCm = First.HalfWidthCm;
		Out.DepthCm = 0.0f;
		Out.ShelterMiddle = First.Middle;
		Out.ShelterHalfDepthCm = First.HalfDepthCm;
		Out.ShelterHalfWidthCm = First.HalfWidthCm;
		Out.ShelterHeightCm = First.HeightCm;
	}
	Out.Passages.Reset();
	Out.NumRooms = 0;
	Out.Bowls.Reset();
	for (int32 K = 0; Tree && K < Tree->NumBowls; ++K)
	{
		const FEntranceBowl& Bowl = Tree->Bowls[K];
		FMOCaveEntranceHollow::FBowl& Copy = Out.Bowls.AddDefaulted_GetRef();
		Copy.Center = Out.Center + FVector2D(Bowl.X, Bowl.Y);
		Copy.RadiusCm = Bowl.Radius;
		Copy.DepthCm = Bowl.Depth;
		Copy.GroundCm = Bowl.Ground;
	}
	if (B.Kind != EMOCaveEntranceKind::Overhang)
	{
		const float Lead = B.HollowRadius * 0.35f;
		Out.MainChamber = FVector(B.CenterX + B.DirX * (Lead + B.Run), B.CenterY + B.DirY * (Lead + B.Run), Ground - B.HollowDepth + EntrancePassageRadiusCm * 0.8f - B.Run * EntranceGrade);
	}
	for (int32 K = 0; Tree && K < Tree->Num; ++K)
	{
		const FEntrancePassage& Pass = Tree->Passages[K];
		const FVector Start(B.CenterX + Pass.Ax, B.CenterY + Pass.Ay, Pass.Az);
		Out.Passages.Add(TPair<FVector, FVector>(Start, Start + FVector(Pass.Vx, Pass.Vy, Pass.Vz)));
		Out.NumRooms += Pass.RoomRadius > 0.0f ? 1 : 0;
	}
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

bool FMOWorldGenDomain::SampleRavineWall(const FMOWorldGenParams& Params, int64 CellX, int64 CellY, int32 Seed, float S, float Side, float DepthShare, FVector& OutPoint, FVector2D& OutAirward)
{
	FRavineBasics B;
	if (!Params.Tuning.bEnableCaves || !RavineBasics(Params.Tuning, CellX, CellY, Seed, B))
	{
		return false;
	}
	const float Ground = AnchorGroundCm(Params, SaltRavine, CellX, CellY, Seed, B.CenterX, B.CenterY);
	const float Env = RavineEnvelope(S);
	if (Env < 0.15f || !RavineAllowed(Params, B, Ground, Seed))
	{
		return false;   // near its ends it has no walls to speak of
	}
	const double Z = (double)Ground - (double)DepthShare * (double)RavineDepth(Params, B, Ground) * (double)FMath::Pow(Env, 0.5f);
	auto PathAt = [&B](float U)
	{
		const float Along = (U - 0.5f) * B.Length;
		const float Across = B.Bend * FMath::Sin(2.0f * UE_PI * B.Waves * U + B.Phase);
		return FVector2D(B.CenterX + B.DirX * Along - B.DirY * Across, B.CenterY + B.DirY * Along + B.DirX * Across);
	};
	const FVector2D Path = PathAt(S);
	const FVector2D Tangent = (PathAt(S + 0.01f) - PathAt(S - 0.01f)).GetSafeNormal();
	const FVector2D Out = FVector2D(-Tangent.Y, Tangent.X) * (Side < 0.0f ? -1.0 : 1.0);
	constexpr float Resolve = 50.0f;
	auto RockAt = [&](double T) { const FVector2D Q = Path + Out * T; return RavineDistance(Params, Q.X, Q.Y, Z, Seed, Resolve) >= 0.0f; };
	if (RockAt(0.0))
	{
		return false;   // no slot at this depth here
	}
	// Out from the path, air to rock: coarse to find it, then halved to about 10 cm. A wall's crags can make a pocket: the first rock is the wall.
	double Air = 0.0, Rock = -1.0;
	for (double T = 150.0; T <= 2.5 * (double)B.HalfWidth; T += 150.0)
	{
		if (RockAt(T))
		{
			Rock = T;
			break;
		}
		Air = T;
	}
	if (Rock < 0.0)
	{
		return false;
	}
	for (int32 I = 0; I < 4; ++I)
	{
		const double Mid = 0.5 * (Air + Rock);
		(RockAt(Mid) ? Rock : Air) = Mid;
	}
	const FVector2D Wall = Path + Out * Rock;
	// The anchor's ground is not the ground here: the slot's top can stand over lower land, where this height is open air and a rock would float.
	if (FMOWorldGenDomain::SampleHeightCm(Params, Wall.X, Wall.Y, Seed) < Z + 150.0)
	{
		return false;
	}
	OutPoint = FVector(Wall.X, Wall.Y, Z);
	OutAirward = -Out;
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
	return IslandRockAt(B, IslandColumnAt(B, Ground, X, Y, Seed), Z);
}

float FMOWorldGenDomain::IslandUndersideCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	FIslandBasics B;
	float Ground = 0.0f;
	if (!FindIslandAt(Params, X, Y, Seed, B, Ground))
	{
		return -1.0e9f;
	}
	const FIslandColumn& Col = IslandColumnAt(B, Ground, X, Y, Seed);
	float Lowest = -1.0e9f;
	for (int32 K = 0; K < B.NumClumps; ++K)
	{
		if (Col.Outline[K] < 0.0f && (Lowest < -1.0e8f || Col.Under2D[K] < Lowest))
		{
			Lowest = Col.Under2D[K];
		}
	}
	return Lowest;
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
	if (IslandRockAt(B, IslandColumnAt(B, Ground, X, Y, Seed), (double)Top - SolidCm) >= 0.0f)
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
	const FEntranceTree* Tree = nullptr;
	if (!EntranceBasics(T, CellX, CellY, Seed, B) || !EntranceGround(Params, CellX, CellY, Seed, B, Ground, &Tree))
	{
		return NoSinkholeDistanceCm;
	}
	// The nearest bowl's rim plus a margin of a fifth of its radius (the passage is open only inside the hollow; beyond it the rock is whole).
	const float Dx = (float)(X - B.CenterX), Dy = (float)(Y - B.CenterY);
	float Best = NoSinkholeDistanceCm;
	if (B.Kind == EMOCaveEntranceKind::Overhang)
	{
		// Part of the hillside: never "inside" a keep-out, so the biome's plants come right up to it and grow on its roof (a point over its open floor is dropped by SampleOpeningDistanceUnderCm, as over any carved ground). Still a distance, so a building or a spawn keeps off it.
		for (int32 K = 0; K < Tree->NumShelters; ++K)
		{
			const FEntranceShelter& S = Tree->Shelters[K];
			Best = FMath::Min(Best, (ShelterShareAt(S, Dx, Dy) - 1.0f) * FMath::Min(S.HalfDepth, S.HalfWidth));
		}
		return FMath::Max(Best, 1.0f);
	}
	for (int32 K = 0; K < Tree->NumBowls; ++K)
	{
		const FEntranceBowl& Bowl = Tree->Bowls[K];
		const float Qx = Dx - Bowl.X, Qy = Dy - Bowl.Y;
		const float Run = FMath::Sqrt(Qx * Qx + Qy * Qy);
		const float Stretch = Run < 1.0f ? Bowl.Mean : BowlStretch(Bowl, Qx / Run, Qy / Run);
		Best = FMath::Min(Best, Run - Bowl.Radius * (Stretch + 0.2f));
	}
	return Best;
}

float FMOWorldGenDomain::CaveBowlShare(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableCaves || T.CaveEntranceChance <= 0.0f)
	{
		return FMOWorldGenDomain::NoCaveBowlShare;
	}
	const int64 CellX = (int64)FMath::FloorToDouble(X / (double)T.CaveEntranceCellCm);
	const int64 CellY = (int64)FMath::FloorToDouble(Y / (double)T.CaveEntranceCellCm);
	FEntranceBasics B;
	float Ground = 0.0f;
	const FEntranceTree* Tree = nullptr;
	if (!EntranceBasics(T, CellX, CellY, Seed, B) || !EntranceGround(Params, CellX, CellY, Seed, B, Ground, &Tree))
	{
		return FMOWorldGenDomain::NoCaveBowlShare;
	}
	const float Dx = (float)(X - B.CenterX), Dy = (float)(Y - B.CenterY);
	float Best = FMOWorldGenDomain::NoCaveBowlShare;
	for (int32 K = 0; K < Tree->NumBowls; ++K)
	{
		Best = FMath::Min(Best, BowlShareAt(Tree->Bowls[K], Dx - Tree->Bowls[K].X, Dy - Tree->Bowls[K].Y));
	}
	return Best;
}

namespace
{
	/** The ground as it is carved at a column: the terrain, or under a bowl the first rock below it. False when that is further down than a bowl goes (a shaft, a gallery's mouth: nowhere for a rim rock). */
	bool CarvedGroundAt(const FMOWorldGenParams& P, double X, double Y, int32 Seed, float& OutGround, float& OutSurface)
	{
		OutGround = FMOWorldGenDomain::SampleHeightCm(P, X, Y, Seed, P.Tuning.VoxelSizeCm);
		const auto Field = [&](double Z) { return FMOWorldGenDomain::SampleCaveDistanceAtGround(P, X, Y, Z, OutGround, Seed, 50.0f); };
		double Rock = (double)OutGround - 5.0;
		float D = Field(Rock);
		if (D >= 0.0f)
		{
			OutSurface = OutGround;
			return true;
		}
		double Air = Rock;
		for (int32 Step = 0; Step < 24 && D < 0.0f; ++Step)
		{
			Air = Rock;
			Rock -= (double)FMath::Max(-D * 0.7f, 20.0f);
			if (Rock < (double)OutGround - 1200.0)
			{
				return false;
			}
			D = Field(Rock);
		}
		if (D < 0.0f)
		{
			return false;
		}
		for (int32 Step = 0; Step < 6; ++Step)
		{
			const double Middle = 0.5 * (Rock + Air);
			if (Field(Middle) < 0.0f)
			{
				Air = Middle;
			}
			else
			{
				Rock = Middle;
			}
		}
		OutSurface = (float)(0.5 * (Rock + Air));
		return true;
	}

	/** How far a rim scatter reaches past its entrance's outermost bowl (cm): the far clusters' 3 x radius of the largest bowl, and the rocks round them. */
	constexpr float BowlRockReachCm = 6000.0f;
}

bool FMOWorldGenDomain::IsUnderOverhang(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, float* OutInFromMouthCm)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableCaves || T.CaveEntranceChance <= 0.0f)
	{
		return false;
	}
	const int64 CellX = (int64)FMath::FloorToDouble(X / (double)T.CaveEntranceCellCm);
	const int64 CellY = (int64)FMath::FloorToDouble(Y / (double)T.CaveEntranceCellCm);
	FEntranceBasics B;
	float Ground = 0.0f;
	const FEntranceTree* Tree = nullptr;
	if (!EntranceBasics(T, CellX, CellY, Seed, B) || B.Kind != EMOCaveEntranceKind::Overhang || !EntranceGround(Params, CellX, CellY, Seed, B, Ground, &Tree))
	{
		return false;
	}
	const float Dx = (float)(X - B.CenterX), Dy = (float)(Y - B.CenterY);
	for (int32 K = 0; K < Tree->NumShelters; ++K)
	{
		const FEntranceShelter& S = Tree->Shelters[K];
		if (ShelterShareAt(S, Dx, Dy) <= 1.15f)
		{
			if (OutInFromMouthCm)
			{
				*OutInFromMouthCm = (Dx - S.X) * S.DirX + (Dy - S.Y) * S.DirY + S.HalfDepth;
			}
			return true;
		}
	}
	return false;
}

void FMOWorldGenDomain::FindCaveBowlRocks(const FMOWorldGenParams& Params, int32 Seed, const FBox2D& Area, TArray<FMOCaveSurfacePoint>& Out)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableCaves || T.CaveEntranceChance <= 0.0f || !Area.bIsValid)
	{
		return;
	}
	const double Cell = (double)T.CaveEntranceCellCm;
	const int64 Cx0 = (int64)FMath::FloorToDouble((Area.Min.X - BowlRockReachCm) / Cell), Cx1 = (int64)FMath::FloorToDouble((Area.Max.X + BowlRockReachCm) / Cell);
	const int64 Cy0 = (int64)FMath::FloorToDouble((Area.Min.Y - BowlRockReachCm) / Cell), Cy1 = (int64)FMath::FloorToDouble((Area.Max.Y + BowlRockReachCm) / Cell);
	const uint32 SaltRocks = (uint32)HashSeed(Seed, SaltEntrance) + 200u;

	for (int64 Cy = Cy0; Cy <= Cy1; ++Cy)
	{
		for (int64 Cx = Cx0; Cx <= Cx1; ++Cx)
		{
			FEntranceBasics B;
			float EntranceGroundCm = 0.0f;
			const FEntranceTree* Tree = nullptr;
			if (!EntranceBasics(T, Cx, Cy, Seed, B) || B.Kind == EMOCaveEntranceKind::Overhang || !EntranceGround(Params, Cx, Cy, Seed, B, EntranceGroundCm, &Tree))
			{
				continue;   // an overhang has no bowl and no rim to ring: it is part of its hillside (N-357)
			}
			const FEntranceTree Bowls = *Tree;   // the thread's cache slot may be reused by the samples below
			const float WayTo = B.HollowRadius * 1.3f;
			const float WayRadius = EntrancePassageRadiusCm + 350.0f;

			const auto ShareAt = [&](float Dx, float Dy)
			{
				float Best = FMOWorldGenDomain::NoCaveBowlShare;
				for (int32 K = 0; K < Bowls.NumBowls; ++K)
				{
					Best = FMath::Min(Best, BowlShareAt(Bowls.Bowls[K], Dx - Bowls.Bowls[K].X, Dy - Bowls.Bowls[K].Y));
				}
				return Best;
			};
			const auto Place = [&](float Dx, float Dy, EMOCaveSurfaceKind Kind, float MinShare) -> bool
			{
				const float Along = FMath::Clamp(Dx * B.DirX + Dy * B.DirY, 0.0f, WayTo);
				const float Qx = Dx - B.DirX * Along, Qy = Dy - B.DirY * Along;
				if (ShareAt(Dx, Dy) < MinShare || Qx * Qx + Qy * Qy < WayRadius * WayRadius)
				{
					return false;   // inside a bowl, or in the way in
				}
				const double X = B.CenterX + (double)Dx, Y = B.CenterY + (double)Dy;
				if (X < Area.Min.X || X >= Area.Max.X || Y < Area.Min.Y || Y >= Area.Max.Y)
				{
					return true;   // another PCG cell's (its children still get their own test)
				}
				float Ground = 0.0f, Surface = 0.0f;
				if (!CarvedGroundAt(Params, X, Y, Seed, Ground, Surface) || WaterGate(Params, X, Y, Surface, Seed) > 0.0f)
				{
					return true;
				}
				FVector Normal = FVector::UpVector;
				float Lowest = Surface;
				if (Kind != EMOCaveSurfaceKind::BowlRockSmall)
				{
					// It rests on the lowest ground under it, tipped with the slope.
					const float Foot = Kind == EMOCaveSurfaceKind::BowlRockLarge ? 220.0f : 110.0f;
					float Around[4] = { Surface, Surface, Surface, Surface };
					const double Offsets[4][2] = { { Foot, 0.0 }, { -Foot, 0.0 }, { 0.0, Foot }, { 0.0, -Foot } };
					for (int32 K = 0; K < 4; ++K)
					{
						float G = 0.0f;
						if (CarvedGroundAt(Params, X + Offsets[K][0], Y + Offsets[K][1], Seed, G, Around[K]))
						{
							Lowest = FMath::Min(Lowest, Around[K]);
						}
					}
					Normal = FVector((double)(Around[1] - Around[0]), (double)(Around[3] - Around[2]), 2.0 * (double)Foot).GetSafeNormal(1.0e-6, FVector::UpVector);
				}
				FMOCaveSurfacePoint& Point = Out.AddDefaulted_GetRef();
				Point.Location = FVector(X, Y, (double)Lowest);
				Point.AirNormal = Normal;
				Point.Kind = Kind;
				Point.AreaM2 = 1.0f;
				Point.GroundCm = Ground;
				return true;
			};

			uint32 Salt = SaltRocks;
			const auto NextHash = [&]() { return HashLattice2(Cx, Cy, Salt++); };
			// Small rocks round a medium one, medium ones round a large one, each a shrinking step out.
			const auto SmallRound = [&](float Dx, float Dy, int32 Count)
			{
				for (int32 S = 0; S < Count; ++S)
				{
					const uint32 H = NextHash();
					const float Off = 90.0f + 170.0f * Unit16(H >> 4);
					Place(Dx + (float)ShoreRing[H & 15u][0] * Off, Dy + (float)ShoreRing[H & 15u][1] * Off, EMOCaveSurfaceKind::BowlRockSmall, 0.55f);
				}
			};
			const auto MediumRound = [&](float Dx, float Dy, int32 Count, float MinShare)
			{
				for (int32 M = 0; M < Count; ++M)
				{
					const uint32 H = NextHash();
					const float Off = 260.0f + 360.0f * Unit16(H >> 4);
					const float Mx = Dx + (float)ShoreRing[H & 15u][0] * Off, My = Dy + (float)ShoreRing[H & 15u][1] * Off;
					if (Place(Mx, My, EMOCaveSurfaceKind::BowlRockMedium, MinShare))
					{
						SmallRound(Mx, My, 1 + (int32)((H >> 24) & 3u));
					}
				}
			};
			for (int32 K = 0; K < Bowls.NumBowls; ++K)
			{
				const FEntranceBowl& Bowl = Bowls.Bowls[K];
				// Round the rim, larger rocks about 9 m apart (seen in game: 15 m read as a few strays, not a ring), some a little inside it and some out past it.
				const int32 NumLarge = FMath::Clamp(FMath::RoundToInt32(6.28f * Bowl.Radius / 900.0f), 3, 9);
				const uint32 Start = NextHash();
				for (int32 L = 0; L < NumLarge; ++L)
				{
					const uint32 H = NextHash();
					const uint32 Slot = (Start + (uint32)(L * 16 / NumLarge) + ((H >> 28) & 1u)) & 15u;
					const float Cos = (float)ShoreRing[Slot][0], Sin = (float)ShoreRing[Slot][1];
					const float Out2 = Bowl.Radius * BowlStretch(Bowl, Cos, Sin) * (0.88f + 0.42f * Unit16(H));
					const float Lx = Bowl.X + Cos * Out2, Ly = Bowl.Y + Sin * Out2;
					if (Place(Lx, Ly, EMOCaveSurfaceKind::BowlRockLarge, 0.82f))
					{
						MediumRound(Lx, Ly, 2 + (int32)((H >> 16) & 3u), 0.72f);
					}
				}
				// Further out and thinning: smaller clusters that lead up to the bowl.
				const int32 NumFar = 2 + (int32)(NextHash() & 3u);
				for (int32 F = 0; F < NumFar; ++F)
				{
					const uint32 H = NextHash();
					const float Way = FMath::Min(1.5f + 1.5f * Unit16(H) * Unit16(H >> 16), 3.0f);   // two rolls: most near, few far
					const float Off = Bowl.Radius * Way;
					const uint32 Slot = (H >> 8) & 15u;
					MediumRound(Bowl.X + (float)ShoreRing[Slot][0] * Off, Bowl.Y + (float)ShoreRing[Slot][1] * Off, 1 + (int32)((H >> 24) & 1u), 1.05f);
				}
			}
		}
	}
}
