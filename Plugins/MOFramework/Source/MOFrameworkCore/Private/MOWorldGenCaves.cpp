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
		SaltEntrance = 0x7601u
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

	// LOD culls: a feature is evaluated while the sample spacing is at most this fraction of its radius (caverns: up to FMOWorldGen::CavernMaxStepCm). CaveMaxLOD reads the same.
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
		const uint32 S = (uint32)FMOWorldGen::HashSeed(Seed, SaltSinkhole);
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
	FORCEINLINE float SinkholeReach(const FSinkholeBasics& B) { return B.Radius * 2.4f + FMOWorldGen::CaveFarCm + 300.0f; }

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
		E.Ground = FMOWorldGen::SampleHeightCm(P, B.CenterX, B.CenterY, Seed, P.Tuning.VoxelSizeCm);
		E.bValid = true;
		return E.Ground;
	}

	/** Does this cell's sinkhole open at all: not on low ground (coast / basin), and never inside an upland lake or its bank (the lake bed would be a hole under the water). */
	float WaterGate(const FMOWorldGenParams& P, double X, double Y, double Z, int32 Seed);   // below

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
			return FMOWorldGen::CaveFarCm;
		}
		const float Dx = (float)(X - B.CenterX), Dy = (float)(Y - B.CenterY);
		const float H2 = Dx * Dx + Dy * Dy;
		const float Reach = SinkholeReach(B);
		if (H2 > Reach * Reach)
		{
			return FMOWorldGen::CaveFarCm;
		}
		const float Ground = SinkholeGroundCm(P, CellX, CellY, Seed, B);
		if (!SinkholeAllowed(P, B, Ground, Seed))
		{
			return FMOWorldGen::CaveFarCm;
		}

		const float Z0 = (float)Z;
		const float TDepth = Ground - Z0;
		const float ZBottom = Ground - B.Depth;

		// Shaft: flares to 1.8x over the top 9 m, walls wobble +-25% (a clean cylinder reads as a pipe, not a collapse).
		const float Funnel = FMath::Clamp(1.0f - TDepth / 900.0f, 0.0f, 1.0f);
		const double WobScale = 1.0 / (double)(B.Radius * 3.0f);
		const float Wob = GradientNoise3(X * WobScale, Y * WobScale, Z * WobScale, (uint32)FMOWorldGen::HashSeed(Seed, SaltSinkholeWall));
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
		const float Hx = FMOWorldGen::SampleHeightCm(P, X + GroundSlopeStepCm, Y, Seed, P.Tuning.VoxelSizeCm)
			- FMOWorldGen::SampleHeightCm(P, X - GroundSlopeStepCm, Y, Seed, P.Tuning.VoxelSizeCm);
		const float Hy = FMOWorldGen::SampleHeightCm(P, X, Y + GroundSlopeStepCm, Seed, P.Tuning.VoxelSizeCm)
			- FMOWorldGen::SampleHeightCm(P, X, Y - GroundSlopeStepCm, Seed, P.Tuning.VoxelSizeCm);
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
		if (T.bCavesOnlyInLimestone && FMOWorldGen::SampleBedrock(P, X, Y, Seed) != EMOBedrock::Limestone)
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
			return 1.0f - Smooth01(LowCeiling, LowCeiling + 1500.0f, FMOWorldGen::SampleBaseHeightCm(P, Sx, Sy, Seed, T.VoxelSizeCm));
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
			Wet = FMath::Max(Wet, 1.0f - Smooth01(12000.0f, 18000.0f, FMOWorldGen::UplandLakeShoreDistanceCm(P, X, Y, Seed)));
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
		const uint32 Base = (uint32)FMOWorldGen::HashSeed(Seed, SaltGallery);
		float Best = FMOWorldGen::CaveFarCm;
		for (int64 K = K0 - 1; K <= K0 + 1; ++K)
		{
			const uint32 Salt = Base ^ (uint32)((uint64)K * 0x9E3779B97F4A7C15ull >> 32);
			const FNoise2 Level = GradientNoise2(X * InvLevel, Y * InvLevel, Salt ^ SaltGalleryLevel);
			const double Zc = ((double)K + 0.5) * Spacing + (double)(T.GalleryLevelAmplitudeCm * Level.Value);
			const float Dz = (float)(Z - Zc) * T.TunnelVerticalSquash;
			if (FMath::Abs(Dz) > RMax + FMOWorldGen::CaveFarCm)
			{
				continue;
			}
			const FNoise2 Path = GradientNoise2(X * InvPath, Y * InvPath, Salt ^ SaltGalleryPath);
			const float PathGrad = FMath::Sqrt(Path.Dx * Path.Dx + Path.Dy * Path.Dy);
			const float Dxy = FMath::Abs(Path.Value) * T.TunnelWavelengthCm / FMath::Max(PathGrad, GalleryPathGradientFloor);
			if (Dxy > RMax + FMOWorldGen::CaveFarCm)
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
			Best = FMath::Min(Best, Dg + (Radius + FMOWorldGen::CaveFarCm) * Gate + (1.0f - Open) * FMOWorldGen::CaveFarCm);
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
		const uint32 S = (uint32)FMOWorldGen::HashSeed(Seed, SaltEntrance);
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
		const float Fixed = Out.HollowRadius * 0.35f + Out.ChamberRadius + FMOWorldGen::CaveFarCm + 300.0f;
		Out.Run = FMath::Min(2200.0f + 1000.0f * Unit16(H3 >> 4), Margin - Fixed);
		return Out.Run >= 800.0f;
	}

	/** Horizontal distance from the hollow's centre beyond which the entrance's field is >= CaveFarCm (skipping it there is exact). <= the jitter margin. */
	FORCEINLINE float EntranceReach(const FEntranceBasics& B)
	{
		return FMath::Max(B.HollowRadius * 1.6f, B.HollowRadius * 0.35f + B.Run + B.ChamberRadius) + FMOWorldGen::CaveFarCm + 300.0f;
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
		const float EndGround = FMOWorldGen::SampleHeightCm(P, EndX, EndY, Seed, T.VoxelSizeCm);
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
			E.Ground = FMOWorldGen::SampleHeightCm(P, B.CenterX, B.CenterY, Seed, P.Tuning.VoxelSizeCm);
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
			return FMOWorldGen::CaveFarCm;
		}
		const float Dx = (float)(X - B.CenterX), Dy = (float)(Y - B.CenterY);
		const float H2 = Dx * Dx + Dy * Dy;
		const float Reach = EntranceReach(B);
		if (H2 > Reach * Reach)
		{
			return FMOWorldGen::CaveFarCm;
		}
		float Ground = 0.0f;
		if (!EntranceGround(P, CellX, CellY, Seed, B, Ground))
		{
			return FMOWorldGen::CaveFarCm;
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

float FMOWorldGen::SampleCaveDistanceAtGround(const FMOWorldGenParams& Params, double X, double Y, double Z, float GroundCm, int32 Seed, float StepCm)
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

int32 FMOWorldGen::CaveMaxLOD(const FMOWorldGenParams& Params)
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

float FMOWorldGen::SampleCaveDistanceCm(const FMOWorldGenParams& Params, double X, double Y, double Z, int32 Seed, float StepCm)
{
	if (!Params.Tuning.bEnableCaves)
	{
		return CaveFarCm;
	}
	return SampleCaveDistanceAtGround(Params, X, Y, Z, SampleHeightCm(Params, X, Y, Seed, StepCm), Seed, StepCm);
}

bool FMOWorldGen::GetSinkholeInCell(const FMOWorldGenParams& Params, int64 CellX, int64 CellY, int32 Seed, FMOSinkhole& Out)
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

bool FMOWorldGen::FindSinkholeAt(const FMOWorldGenParams& Params, double X, double Y, int32 Seed, FMOSinkhole& Out)
{
	const double Cell = (double)Params.Tuning.SinkholeCellCm;
	return GetSinkholeInCell(Params, (int64)FMath::FloorToDouble(X / Cell), (int64)FMath::FloorToDouble(Y / Cell), Seed, Out);
}

float FMOWorldGen::DistanceToSinkholeRimZone(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	// Every opening's keep-out: a sinkhole's rim zone and a built entrance's hollow (vegetation, world features and spawns read this one query).
	const float Entrance = DistanceToCaveEntranceHollow(Params, X, Y, Seed);
	FMOSinkhole S;
	if (!FindSinkholeAt(Params, X, Y, Seed, S))
	{
		return Entrance;
	}
	// The flared rim (1.8x) with its +25% wall wobble reaches ~2.3x the mean radius; plant nothing inside that.
	const float Dist = (float)FVector2D::Distance(FVector2D(X, Y), S.Center);
	return FMath::Min(Dist - S.RadiusCm * 2.3f, Entrance);
}

float FMOWorldGen::DistanceToCaveEntranceHollow(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
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
