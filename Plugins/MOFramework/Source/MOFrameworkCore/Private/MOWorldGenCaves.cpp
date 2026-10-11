// Cave generator half of FMOWorldGen (tunnels, caverns, sinkholes). Same rules as MOWorldGen.h: pure, deterministic, thread-safe, IEEE-exact ops only.
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
		SaltTunnelA = 0x7101u, SaltTunnelB = 0x7102u, SaltTunnelRadius = 0x7103u, SaltCavernA = 0x7201u, SaltCavernB = 0x7202u,
		SaltSinkhole = 0x7401u, SaltSinkholeWall = 0x7402u
	};

	// Distance scaling: a noise field with ~1.2 gradient per lattice cell turns into cm as f * (CellCm / Gradient). The tunnel field is sqrt(A^2 + B^2)
	// (gradient ~1.2, x ~1.2 for the vertical squash); the cavern field mixes two octaves (~1.1, x ~1.3 for its squash). MOFramework.WorldGen.Caves checks
	// that the tunnels really come out about TunnelRadiusCm wide.
	constexpr float TunnelGradient = 1.45f;
	constexpr float CavernGradient = 1.4f;
	constexpr float CavernSquash = 1.9f;

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
	bool SinkholeAllowed(const FMOWorldGenParams& P, const FSinkholeBasics& B, float GroundCm, int32 Seed)
	{
		return GroundCm >= P.Tuning.SeaLevelCm + P.Tuning.CaveMinGroundAboveSeaCm && !FMOWorldGen::IsInUplandLakeZone(P, B.CenterX, B.CenterY, Seed, 1.8f);
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

	// Sinkholes: the only features that reach the ground. Skipped when the sample spacing cannot resolve a shaft.
	if (StepCm <= T.SinkholeRadiusCm * 0.9f)
	{
		D = FMath::Min(D, SinkholeDistance(Params, X, Y, Z, Seed));
	}

	// Everything else stays under the cover, and not on low ground (coasts / basins). Each gate is a 0..1 factor that adds a penalty of at least CaveFarCm when
	// it is 1, so the field is continuous where a feature is skipped (skipped == saturated).
	const float LowGround = 1.0f - Smooth01(T.SeaLevelCm + T.CaveMinGroundAboveSeaCm, T.SeaLevelCm + T.CaveMinGroundAboveSeaCm + 600.0f, GroundCm);
	if (Depth > T.CaveCoverCm && LowGround < 1.0f)
	{
		const float Shallow = 1.0f - Smooth01(T.CaveCoverCm, T.CaveCoverCm + T.CaveFadeCm, Depth);
		const float DeepFade = Smooth01(T.CaveMaxDepthCm, T.CaveMaxDepthCm + T.CaveFadeCm, Depth);
		const float Gate = FMath::Max3(Shallow, DeepFade, LowGround);

		// Tunnels: sqrt(A^2 + B^2) is small near the curve where two independent 3D noise fields are both zero.
		if (StepCm <= T.TunnelRadiusCm * 0.6f)
		{
			const float Lambda = T.TunnelWavelengthCm;
			const double Inv = 1.0 / (double)Lambda;
			const double Px = X * Inv, Py = Y * Inv, Pz = Z * Inv * (double)T.TunnelVerticalSquash;
			const float A = GradientNoise3(Px, Py, Pz, (uint32)HashSeed(Seed, SaltTunnelA));
			const float B = GradientNoise3(Px + 31.7, Py - 12.3, Pz + 7.9, (uint32)HashSeed(Seed, SaltTunnelB));
			const float Wobble = GradientNoise3(Px * 0.6 + 5.1, Py * 0.6 - 8.3, Pz * 0.6 + 2.2, (uint32)HashSeed(Seed, SaltTunnelRadius));
			const float Radius = T.TunnelRadiusCm * (1.0f + T.TunnelRadiusVariation * FMath::Clamp(1.6f * Wobble, -1.0f, 1.0f));
			const float Dt = FMath::Sqrt(A * A + B * B) * (Lambda / TunnelGradient) - Radius;
			// The penalty cancels the deepest possible interior (-Radius) and adds CaveFarCm, so with the gate closed the term is saturated; in between it tapers.
			D = FMath::Min(D, Dt + (Radius + CaveFarCm) * Gate);
		}

		// Caverns: where a smooth 3D field rises above the threshold.
		if (Depth > T.CavernMinDepthCm && StepCm <= 800.0f)
		{
			const float Lambda = T.CavernWavelengthCm;
			const double Inv = 1.0 / (double)Lambda;
			const double Px = X * Inv, Py = Y * Inv, Pz = Z * Inv * (double)CavernSquash;
			const float C = 0.7f * GradientNoise3(Px, Py, Pz, (uint32)HashSeed(Seed, SaltCavernA))
				+ 0.3f * GradientNoise3(Px * 2.0 + 5.3, Py * 2.0 - 2.9, Pz * 2.0 + 8.1, (uint32)HashSeed(Seed, SaltCavernB));
			const float Dc = (T.CavernThreshold - C) * (Lambda / CavernGradient);
			const float CavernShallow = 1.0f - Smooth01(T.CavernMinDepthCm, T.CavernMinDepthCm + T.CaveFadeCm, Depth);
			// The deepest interior is (Cmax - threshold) * wavelength / gradient <= 0.4 * wavelength over the allowed threshold range; cancel it and add CaveFarCm.
			D = FMath::Min(D, Dc + (0.4f * Lambda + CaveFarCm) * FMath::Max3(CavernShallow, DeepFade, LowGround));
		}
	}

	return FMath::Clamp(D, -CaveFarCm, CaveFarCm);
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
	FMOSinkhole S;
	if (!FindSinkholeAt(Params, X, Y, Seed, S))
	{
		return CaveFarCm;
	}
	// The flared rim (1.8x) with its +25% wall wobble reaches ~2.3x the mean radius; plant nothing inside that.
	const float Dist = (float)FVector2D::Distance(FVector2D(X, Y), S.Center);
	return Dist - S.RadiusCm * 2.3f;
}
