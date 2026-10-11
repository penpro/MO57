// Internal noise helpers shared by the generator's .cpp files (not part of the public API: callers use FMOWorldGen).
// Same rules as MOWorldGen.h: IEEE-exact operations only, integer hashing, literal constant tables, no CRT transcendentals.

#pragma once

#include "CoreMinimal.h"

namespace MOWorldGenNoise
{
	// 16 unit gradient directions, literal constants (no sin/cos at runtime: see RULES in MOWorldGen.h).
	constexpr float GGrad2[16][2] = {
		{ 1.0f, 0.0f }, { 0.92387953f, 0.38268343f }, { 0.70710678f, 0.70710678f }, { 0.38268343f, 0.92387953f },
		{ 0.0f, 1.0f }, { -0.38268343f, 0.92387953f }, { -0.70710678f, 0.70710678f }, { -0.92387953f, 0.38268343f },
		{ -1.0f, 0.0f }, { -0.92387953f, -0.38268343f }, { -0.70710678f, -0.70710678f }, { -0.38268343f, -0.92387953f },
		{ 0.0f, -1.0f }, { 0.38268343f, -0.92387953f }, { 0.70710678f, -0.70710678f }, { 0.92387953f, -0.38268343f } };

	// 12 edge-midpoint directions of a cube (Perlin's improved gradient set), literal like the 2D table in MOWorldGen.cpp.
	constexpr float GGrad3[12][3] = {
		{ 1.0f, 1.0f, 0.0f }, { -1.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f }, { -1.0f, -1.0f, 0.0f },
		{ 1.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, -1.0f }, { -1.0f, 0.0f, -1.0f },
		{ 0.0f, 1.0f, 1.0f }, { 0.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, -1.0f }, { 0.0f, -1.0f, -1.0f } };

	FORCEINLINE uint32 HashLattice2(int64 X, int64 Y, uint32 Salt)
	{
		uint64 H = (uint64)X * 0x9E3779B97F4A7C15ull;
		H ^= ((uint64)Y + 0xC2B2AE3D27D4EB4Full) * 0xD6E8FEB86659FD93ull;
		H ^= (uint64)Salt * 0xFF51AFD7ED558CCDull;
		H ^= H >> 32;
		H *= 0xD6E8FEB86659FD93ull;
		H ^= H >> 32;
		H *= 0xD6E8FEB86659FD93ull;
		H ^= H >> 32;
		return (uint32)H;
	}

	FORCEINLINE uint32 HashLattice3(int64 X, int64 Y, int64 Z, uint32 Salt)
	{
		uint64 H = (uint64)X * 0x9E3779B97F4A7C15ull;
		H ^= ((uint64)Y + 0xC2B2AE3D27D4EB4Full) * 0xD6E8FEB86659FD93ull;
		H ^= ((uint64)Z + 0x165667B19E3779F9ull) * 0xFF51AFD7ED558CCDull;
		H ^= (uint64)Salt * 0x9FB21C651E98DF25ull;
		H ^= H >> 32;
		H *= 0xD6E8FEB86659FD93ull;
		H ^= H >> 32;
		H *= 0xD6E8FEB86659FD93ull;
		H ^= H >> 32;
		return (uint32)H;
	}

	struct FNoise2
	{
		float Value;
		float Dx;
		float Dy;
	};

	/** Output range ~[-1, 1] (unit-gradient 2D noise peaks at sqrt(2)/2, scaled up). Derivatives are w.r.t. the input coordinates. */
	FORCEINLINE FNoise2 GradientNoise2(double X, double Y, uint32 Salt)
	{
		const double FloorX = FMath::FloorToDouble(X);
		const double FloorY = FMath::FloorToDouble(Y);
		const int64 Xi = (int64)FloorX;
		const int64 Yi = (int64)FloorY;
		const float Fx = (float)(X - FloorX);
		const float Fy = (float)(Y - FloorY);

		const float Ux = Fx * Fx * Fx * (Fx * (Fx * 6.0f - 15.0f) + 10.0f);
		const float Uy = Fy * Fy * Fy * (Fy * (Fy * 6.0f - 15.0f) + 10.0f);
		const float Dux = 30.0f * Fx * Fx * (Fx - 1.0f) * (Fx - 1.0f);
		const float Duy = 30.0f * Fy * Fy * (Fy - 1.0f) * (Fy - 1.0f);

		const float* Ga = GGrad2[HashLattice2(Xi, Yi, Salt) & 15u];
		const float* Gb = GGrad2[HashLattice2(Xi + 1, Yi, Salt) & 15u];
		const float* Gc = GGrad2[HashLattice2(Xi, Yi + 1, Salt) & 15u];
		const float* Gd = GGrad2[HashLattice2(Xi + 1, Yi + 1, Salt) & 15u];

		const float Va = Ga[0] * Fx + Ga[1] * Fy;
		const float Vb = Gb[0] * (Fx - 1.0f) + Gb[1] * Fy;
		const float Vc = Gc[0] * Fx + Gc[1] * (Fy - 1.0f);
		const float Vd = Gd[0] * (Fx - 1.0f) + Gd[1] * (Fy - 1.0f);

		const float K = Va - Vb - Vc + Vd;
		constexpr float Scale = 1.41421356f;

		FNoise2 Result;
		Result.Value = Scale * (Va + Ux * (Vb - Va) + Uy * (Vc - Va) + Ux * Uy * K);
		Result.Dx = Scale * (Ga[0] + Ux * (Gb[0] - Ga[0]) + Uy * (Gc[0] - Ga[0]) + Ux * Uy * (Ga[0] - Gb[0] - Gc[0] + Gd[0]) + Dux * (Uy * K + Vb - Va));
		Result.Dy = Scale * (Ga[1] + Ux * (Gb[1] - Ga[1]) + Uy * (Gc[1] - Ga[1]) + Ux * Uy * (Ga[1] - Gb[1] - Gc[1] + Gd[1]) + Duy * (Ux * K + Vc - Va));
		return Result;
	}

	/**
	 * 3D gradient noise, one lattice cell per unit of input. Measured (Saved/Logs scratch noise3_calib, documented in Docs/Terrain_Generation_Exploration.md,
	 * "Caves"): std 0.27, |value| < 0.95, gradient magnitude ~1.2 per cell (rms 1.22) -- the origin of the cave distance scaling constants.
	 */
	FORCEINLINE float GradientNoise3(double X, double Y, double Z, uint32 Salt)
	{
		const double FloorX = FMath::FloorToDouble(X);
		const double FloorY = FMath::FloorToDouble(Y);
		const double FloorZ = FMath::FloorToDouble(Z);
		const int64 Xi = (int64)FloorX, Yi = (int64)FloorY, Zi = (int64)FloorZ;
		const float Fx = (float)(X - FloorX), Fy = (float)(Y - FloorY), Fz = (float)(Z - FloorZ);

		const float U = Fx * Fx * Fx * (Fx * (Fx * 6.0f - 15.0f) + 10.0f);
		const float V = Fy * Fy * Fy * (Fy * (Fy * 6.0f - 15.0f) + 10.0f);
		const float W = Fz * Fz * Fz * (Fz * (Fz * 6.0f - 15.0f) + 10.0f);

		auto Corner = [&](int32 Dx, int32 Dy, int32 Dz)
		{
			const float* G = GGrad3[(HashLattice3(Xi + Dx, Yi + Dy, Zi + Dz, Salt) >> 4) % 12u];
			return G[0] * (Fx - (float)Dx) + G[1] * (Fy - (float)Dy) + G[2] * (Fz - (float)Dz);
		};

		const float C000 = Corner(0, 0, 0), C100 = Corner(1, 0, 0), C010 = Corner(0, 1, 0), C110 = Corner(1, 1, 0);
		const float C001 = Corner(0, 0, 1), C101 = Corner(1, 0, 1), C011 = Corner(0, 1, 1), C111 = Corner(1, 1, 1);

		const float X00 = C000 + U * (C100 - C000);
		const float X10 = C010 + U * (C110 - C010);
		const float X01 = C001 + U * (C101 - C001);
		const float X11 = C011 + U * (C111 - C011);
		const float Y0 = X00 + V * (X10 - X00);
		const float Y1 = X01 + V * (X11 - X01);
		return Y0 + W * (Y1 - Y0);
	}

	/** Hermite smoothstep, 0 below A and 1 above B (A != B). */
	FORCEINLINE float Smooth01(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** Polynomial smooth minimum (K = blend width, cm): a rounded union of two distance fields. */
	FORCEINLINE float SmoothMin(float A, float B, float K)
	{
		const float H = FMath::Max(K - FMath::Abs(A - B), 0.0f) / K;
		return FMath::Min(A, B) - H * H * K * 0.25f;
	}
}
