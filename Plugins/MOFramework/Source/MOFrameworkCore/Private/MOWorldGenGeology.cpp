// Geology half of FMOWorldGen: bedrock provinces, ore bodies, and the points on cave surfaces the cave spawner scatters on. Same rules as MOWorldGen.h: pure,
// deterministic, thread-safe, IEEE-exact ops only (no libm in a value that reaches the world).
//
// Where each ore really occurs (Wes, Oct 17 2026: "check what is actually realistic"), and how it is modelled here -- Docs/Terrain_Generation_Exploration.md
// ("Geology") has the sources:
//   * Coal: SEAMS, beds inside sedimentary rock (sandstone / shale, the "coal measures") of basins and foothills -- laterally continuous, tens of centimetres to
//     a couple of metres thick, dipping gently with the strata, pinching out. Modelled as horizontal beds on a ~16 m cycle with a 1-degree regional dip.
//   * Iron: in limestone, hematite fills joints and fissures (northern England's limestone hematite filled whole fissures and caverns). Modelled as vertical
//     sheets along one joint set, mineralised in patches. (Bog iron -- limonite precipitating in wetlands -- is a SURFACE ore: the biomes' dig tables.)
//   * Copper: veins in basic igneous rock (basalt, gabbro) and at granite margins; native copper and malachite where they weather. Modelled as thin
//     sheets in 3D, sparse.
//   * Tin: cassiterite veins tied to granite (and river placers downstream -- surface). Modelled like copper, in granite.
//   * Flint: chert nodules in thin beds of limestone (and chalk).
//   * Lead: galena in limestone (Mississippi Valley-type: the lead-zinc ores of the Pennines, Derbyshire, the Upper Mississippi), filling fractures and
//     old solution cavities. Modelled as a second, more widely spaced joint set, mineralised in rarer patches than the hematite.
//   * Solution caves form in limestone; the galleries are not restricted to it (yet) -- see the doc's open forks.

#include "MOWorldGen.h"
#include "MOWorldGenNoise.h"
#include "Async/ParallelFor.h"

using namespace MOWorldGenNoise;

namespace
{
	enum : uint32
	{
		SaltGeoProvince = 0x9101u, SaltGeoVolcanic = 0x9102u,
		SaltCoalDip = 0x9201u, SaltCoalSeam = 0x9202u, SaltCoalThick = 0x9203u, SaltCoalPresent = 0x9204u,
		SaltHematiteJoint = 0x9301u, SaltHematiteWidth = 0x9302u, SaltHematiteMask = 0x9303u,
		SaltGalenaJoint = 0x9304u, SaltGalenaWidth = 0x9305u, SaltGalenaMask = 0x9306u,
		SaltChertBed = 0x9401u, SaltChertNodule = 0x9402u,
		SaltCopperVein = 0x9501u, SaltCopperMask = 0x9502u, SaltTinVein = 0x9503u, SaltTinMask = 0x9504u,
		SaltCaveSurfaceJitter = 0x9601u, SaltSoilDepth = 0x9701u, SaltOutcropJitter = 0x9801u
	};

	/** Province size: the bedrock changes over kilometres. */
	constexpr double ProvinceWavelengthCm = 700000.0;
	/** Thresholds on the province fields, measured for the land shares in MOFramework.WorldGen.Geology (limestone ~ coal measures > granite > basalt). */
	constexpr float VolcanicThreshold = 0.38f;
	constexpr float CrystallineContinentalness = 0.5f;
	constexpr float CrystallineProvince = -0.4f;
	constexpr float LimestoneProvince = -0.05f;

	/** Strata dip: a regional field of up to +-25 m over ~4 km (about a degree). Seams and chert beds follow it. */
	float StrataDipCm(double X, double Y, int32 Seed)
	{
		return 2500.0f * GradientNoise2(X * (1.0 / 400000.0), Y * (1.0 / 400000.0), (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltCoalDip)).Value;
	}

	/** The grade inside a bed of thickness Thick at depth Local into it: 1 in the middle, falling to 0 over 20% of the thickness at each face. */
	float BedGrade(float Local, float Thick)
	{
		if (Local < 0.0f || Local > Thick)
		{
			return 0.0f;
		}
		const float Edge = FMath::Max(Thick * 0.2f, 1.0f);
		return FMath::Min(Smooth01(0.0f, Edge, Local), 1.0f - Smooth01(Thick - Edge, Thick, Local));
	}

	FMOOreSample Coal(double X, double Y, double Z, int32 Seed)
	{
		// A seam per 16 m of strata, each at its own place in its cycle (300-1200 cm up), 40-220 cm thick, present where a 400 m field opens.
		constexpr double Cycle = 1600.0;
		const double Zs = Z + (double)StrataDipCm(X, Y, Seed);
		const int64 K = (int64)FMath::FloorToDouble(Zs / Cycle);
		const uint32 SeamSalt = (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltCoalSeam);
		const float Base = (float)((double)K * Cycle) + 300.0f + 900.0f * Unit16(HashLattice2(K, 0, SeamSalt));
		const uint32 KSalt = HashLattice2(K, 1, SeamSalt);
		const float Thick = 40.0f + 180.0f * FMath::Clamp(0.5f + 0.7f * GradientNoise2(X * (1.0 / 9000.0), Y * (1.0 / 9000.0), (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltCoalThick) ^ KSalt).Value, 0.0f, 1.0f);
		const float Grade = BedGrade((float)Zs - Base, Thick);
		if (Grade <= 0.0f)
		{
			return FMOOreSample();
		}
		const float Present = Smooth01(-0.2f, 0.05f, GradientNoise2(X * (1.0 / 40000.0), Y * (1.0 / 40000.0), (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltCoalPresent) ^ KSalt).Value);
		return Present > 0.0f ? FMOOreSample{ EMOOreKind::Coal, Grade * Present } : FMOOreSample();
	}

	FMOOreSample Limestone(double X, double Y, double Z, int32 Seed)
	{
		// Hematite along one joint set: the zero set of a 2D field (~60 m between joints), 30-120 cm wide, mineralised where a 3D field (250 m across, 100 m tall) opens.
		const FNoise2 Joint = GradientNoise2(X * (1.0 / 6000.0), Y * (1.0 / 6000.0), (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltHematiteJoint));
		const float JointDist = FMath::Abs(Joint.Value) * 6000.0f / FMath::Max(FMath::Sqrt(Joint.Dx * Joint.Dx + Joint.Dy * Joint.Dy), 0.35f);
		const float Width = 30.0f + 90.0f * FMath::Clamp(0.5f + 0.7f * GradientNoise2(X * (1.0 / 2000.0), Y * (1.0 / 2000.0), (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltHematiteWidth)).Value, 0.0f, 1.0f);
		if (JointDist < 0.5f * Width)
		{
			const float Mask = GradientNoise3(X * (1.0 / 25000.0), Y * (1.0 / 25000.0), Z * (1.0 / 10000.0), (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltHematiteMask));
			const float Grade = Smooth01(0.22f, 0.32f, Mask) * (1.0f - Smooth01(0.3f * Width, 0.5f * Width, JointDist));
			if (Grade > 0.0f)
			{
				return FMOOreSample{ EMOOreKind::Hematite, Grade };
			}
		}
		// Galena along a second joint set (~90 m apart, 20-70 cm wide), mineralised in rarer patches (a 3D field 200 m across, 80 m tall).
		const FNoise2 GJoint = GradientNoise2(X * (1.0 / 9000.0) + 3.7, Y * (1.0 / 9000.0) - 8.1, (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltGalenaJoint));
		const float GJointDist = FMath::Abs(GJoint.Value) * 9000.0f / FMath::Max(FMath::Sqrt(GJoint.Dx * GJoint.Dx + GJoint.Dy * GJoint.Dy), 0.35f);
		const float GWidth = 20.0f + 50.0f * FMath::Clamp(0.5f + 0.7f * GradientNoise2(X * (1.0 / 1500.0), Y * (1.0 / 1500.0), (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltGalenaWidth)).Value, 0.0f, 1.0f);
		if (GJointDist < 0.5f * GWidth)
		{
			const float Mask = GradientNoise3(X * (1.0 / 20000.0), Y * (1.0 / 20000.0), Z * (1.0 / 8000.0), (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltGalenaMask));
			const float Grade = Smooth01(0.26f, 0.36f, Mask) * (1.0f - Smooth01(0.3f * GWidth, 0.5f * GWidth, GJointDist));
			if (Grade > 0.0f)
			{
				return FMOOreSample{ EMOOreKind::Galena, Grade };
			}
		}
		// Chert: a 25 cm bed per 6 m of strata (each at its own height in its cycle), nodules in it.
		constexpr double Cycle = 600.0;
		const double Zs = Z + (double)StrataDipCm(X, Y, Seed);
		const int64 K = (int64)FMath::FloorToDouble(Zs / Cycle);
		const float Base = (float)((double)K * Cycle) + 150.0f + 300.0f * Unit16(HashLattice2(K, 7, (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltChertBed)));
		const float Bed = BedGrade((float)Zs - Base, 25.0f);
		if (Bed > 0.0f)
		{
			const float Nodule = Smooth01(0.12f, 0.22f, GradientNoise3(X * (1.0 / 120.0), Y * (1.0 / 120.0), Z * (1.0 / 120.0), (uint32)FMOWorldGenDomain::HashSeed(Seed, SaltChertNodule)));
			if (Nodule > 0.0f)
			{
				return FMOOreSample{ EMOOreKind::Flint, Bed * Nodule };
			}
		}
		return FMOOreSample();
	}

	/** A sparse vein network: the zero set of a 3D field (Wavelength apart), HalfWidth thick, mineralised where a mask field (Mask x Wavelength across) is above Threshold. */
	float Vein(double X, double Y, double Z, int32 Seed, uint32 VeinSalt, uint32 MaskSalt, double Wavelength, float HalfWidth, float Threshold)
	{
		const double Inv = 1.0 / Wavelength;
		const float V = GradientNoise3(X * Inv, Y * Inv, Z * Inv, (uint32)FMOWorldGenDomain::HashSeed(Seed, VeinSalt));
		const float Dist = FMath::Abs(V) * (float)Wavelength / 1.2f;   // ~1.2 gradient per lattice cell
		if (Dist >= HalfWidth)
		{
			return 0.0f;
		}
		const double InvMask = Inv / 4.0;
		const float Mask = GradientNoise3(X * InvMask + 5.1, Y * InvMask - 3.3, Z * InvMask + 1.7, (uint32)FMOWorldGenDomain::HashSeed(Seed, MaskSalt));
		return Smooth01(Threshold, Threshold + 0.1f, Mask) * (1.0f - Smooth01(0.5f * HalfWidth, HalfWidth, Dist));
	}
}

EMOBedrock FMOWorldGenDomain::SampleBedrock(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	const double Inv = 1.0 / ProvinceWavelengthCm;
	const float Volcanic = GradientNoise2(X * Inv * 0.6 + 11.3, Y * Inv * 0.6 - 4.7, (uint32)HashSeed(Seed, SaltGeoVolcanic)).Value;
	if (Volcanic > VolcanicThreshold)
	{
		return EMOBedrock::Basalt;
	}
	const float Province = GradientNoise2(X * Inv, Y * Inv, (uint32)HashSeed(Seed, SaltGeoProvince)).Value;
	if (Province < CrystallineProvince || SampleClimate(Params, X, Y, Seed).Continentalness > CrystallineContinentalness)
	{
		return EMOBedrock::Granite;   // the shield of the continental interior, the cores of the ranges
	}
	return Province > LimestoneProvince ? EMOBedrock::Limestone : EMOBedrock::CoalMeasures;
}

FMOOreSample FMOWorldGenDomain::SampleOreIn(const FMOWorldGenParams& Params, EMOBedrock Bedrock, double X, double Y, double Z, int32 Seed)
{
	switch (Bedrock)
	{
	case EMOBedrock::CoalMeasures:
		return Coal(X, Y, Z, Seed);
	case EMOBedrock::Limestone:
		return Limestone(X, Y, Z, Seed);
	case EMOBedrock::Basalt:
	{
		const float Grade = Vein(X, Y, Z, Seed, SaltCopperVein, SaltCopperMask, 7000.0, 40.0f, 0.15f);
		return Grade > 0.0f ? FMOOreSample{ EMOOreKind::Copper, Grade } : FMOOreSample();
	}
	case EMOBedrock::Granite:
	{
		const float Tin = Vein(X, Y, Z, Seed, SaltTinVein, SaltTinMask, 8000.0, 35.0f, 0.18f);
		if (Tin > 0.0f)
		{
			return FMOOreSample{ EMOOreKind::Tin, Tin };
		}
		const float Copper = Vein(X, Y, Z, Seed, SaltCopperVein, SaltCopperMask, 7000.0, 30.0f, 0.35f);   // rarer at the granite margins
		return Copper > 0.0f ? FMOOreSample{ EMOOreKind::Copper, Copper } : FMOOreSample();
	}
	}
	return FMOOreSample();
}

float FMOWorldGenDomain::SoilDepthCm(const FMOWorldGenParams& Params, double X, double Y, int32 Seed)
{
	// Soil and regolith pile up on flats and wash off steep ground: 250 cm on the level, 40 cm from 35 degrees on (a 12 m baseline, like the caves' slope).
	const float Voxel = Params.Tuning.VoxelSizeCm;
	constexpr double Step = 600.0;
	const float Gx = (SampleHeightCm(Params, X + Step, Y, Seed, Voxel) - SampleHeightCm(Params, X - Step, Y, Seed, Voxel)) / (float)(2.0 * Step);
	const float Gy = (SampleHeightCm(Params, X, Y + Step, Seed, Voxel) - SampleHeightCm(Params, X, Y - Step, Seed, Voxel)) / (float)(2.0 * Step);
	const float Slope = FMath::Sqrt(Gx * Gx + Gy * Gy);
	constexpr float Tan5 = 0.0874887f, Tan35 = 0.7002075f;
	const float Base = 40.0f + 210.0f * (1.0f - Smooth01(Tan5, Tan35, Slope));
	const float Vary = 0.75f + 0.5f * FMath::Clamp(0.5f + 0.7f * GradientNoise2(X * (1.0 / 6000.0), Y * (1.0 / 6000.0), (uint32)HashSeed(Seed, SaltSoilDepth)).Value, 0.0f, 1.0f);
	return Base * Vary;
}

FMODigSample FMOWorldGenDomain::SampleDig(const FMOWorldGenParams& Params, double X, double Y, double Z, int32 Seed, float MinOreGrade)
{
	FMODigSample Out;
	const float Ground = SampleHeightCm(Params, X, Y, Seed, Params.Tuning.VoxelSizeCm);
	if (Z >= (double)(Ground - SoilDepthCm(Params, X, Y, Seed)))
	{
		Out.Material = EMODigMaterial::Soil;
		return Out;
	}
	Out.Bedrock = SampleBedrock(Params, X, Y, Seed);
	const FMOOreSample Ore = SampleOreIn(Params, Out.Bedrock, X, Y, Z, Seed);
	if (Ore.Kind != EMOOreKind::None && Ore.Grade >= MinOreGrade)
	{
		Out.Material = EMODigMaterial::Ore;
		Out.Ore = Ore.Kind;
		Out.Grade = Ore.Grade;
	}
	else
	{
		Out.Material = EMODigMaterial::Rock;
	}
	return Out;
}

void FMOWorldGenDomain::FindOutcrops(const FMOWorldGenParams& Params, int32 Seed, const FBox2D& Area, float SpacingCm, float MinSlopeDeg, TArray<FMOCaveSurfacePoint>& Out)
{
	if (!Area.bIsValid || SpacingCm <= 1.0f)
	{
		return;
	}
	const double S = SpacingCm;
	const int64 I0 = (int64)FMath::FloorToDouble(Area.Min.X / S), I1 = (int64)FMath::FloorToDouble(Area.Max.X / S);
	const int64 J0 = (int64)FMath::FloorToDouble(Area.Min.Y / S), J1 = (int64)FMath::FloorToDouble(Area.Max.Y / S);
	const int32 NumI = (int32)(I1 - I0 + 1), NumJ = (int32)(J1 - J0 + 1);
	if (NumI <= 0 || NumJ <= 0 || (int64)NumI * NumJ > 4000000)
	{
		return;
	}
	const uint32 JitterSalt = (uint32)HashSeed(Seed, SaltOutcropJitter);
	const float Voxel = Params.Tuning.VoxelSizeCm;
	const float MinTan = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(MinSlopeDeg, 1.0f, 89.0f)));
	TArray<TArray<FMOCaveSurfacePoint>> Rows;
	Rows.SetNum(NumJ);
	ParallelFor(NumJ, [&](int32 RowIndex)
	{
		const int64 J = J0 + RowIndex;
		for (int64 I = I0; I <= I1; ++I)
		{
			const uint32 H = HashLattice2(I, J, JitterSalt);
			const double X = ((double)I + 0.15 + 0.7 * (double)Unit16(H)) * S;
			const double Y = ((double)J + 0.15 + 0.7 * (double)Unit16(H >> 16)) * S;
			if (X < Area.Min.X || X >= Area.Max.X || Y < Area.Min.Y || Y >= Area.Max.Y)
			{
				continue;
			}
			const float Ground = SampleHeightCm(Params, X, Y, Seed, Voxel);
			if (Ground <= Params.Tuning.SeaLevelCm + 100.0f)
			{
				continue;   // under or at the water
			}
			constexpr double D = 150.0;
			const float Gx = (SampleHeightCm(Params, X + D, Y, Seed, Voxel) - SampleHeightCm(Params, X - D, Y, Seed, Voxel)) / (float)(2.0 * D);
			const float Gy = (SampleHeightCm(Params, X, Y + D, Seed, Voxel) - SampleHeightCm(Params, X, Y - D, Seed, Voxel)) / (float)(2.0 * D);
			const float Slope = FMath::Sqrt(Gx * Gx + Gy * Gy);
			if (Slope < MinTan)
			{
				continue;
			}
			FMOCaveSurfacePoint& P = Rows[RowIndex].AddDefaulted_GetRef();
			P.Location = FVector(X, Y, Ground);
			P.AirNormal = FVector(-Gx, -Gy, 1.0).GetSafeNormal();
			P.Kind = EMOCaveSurfaceKind::Outcrop;
			P.AreaM2 = (float)(S * S / 10000.0) / FMath::Max((float)P.AirNormal.Z, 0.25f);
			P.GroundCm = Ground;
		}
	});
	for (TArray<FMOCaveSurfacePoint>& Row : Rows)
	{
		Out.Append(MoveTemp(Row));
	}
}

bool FMOWorldGenDomain::FindNearestCaveMouth(const FMOWorldGenParams& Params, int32 Seed, const FVector2D& From, double MaxRadiusCm, FVector& OutMouth, float& OutSlopeDeg)
{
	if (!Params.Tuning.bEnableCaves || MaxRadiusCm <= 0.0)
	{
		return false;
	}
	constexpr double CellCm = 50000.0;
	constexpr double StepCm = 400.0;
	constexpr int32 Columns = (int32)(CellCm / StepCm);
	const float Voxel = Params.Tuning.VoxelSizeCm;
	const int64 CX = (int64)FMath::FloorToDouble(From.X / CellCm);
	const int64 CY = (int64)FMath::FloorToDouble(From.Y / CellCm);
	const int32 MaxRing = (int32)FMath::CeilToDouble(MaxRadiusCm / CellCm);
	double BestSq = FMath::Square(MaxRadiusCm);
	bool bFound = false;
	for (int32 Ring = 0; Ring <= MaxRing; ++Ring)
	{
		// Every cell of this ring is at least (Ring - 1) cells from From: past the best mouth found, nothing nearer is left.
		if (bFound && FMath::Square(FMath::Max(0, Ring - 1) * CellCm) > BestSq)
		{
			break;
		}
		for (int32 DY = -Ring; DY <= Ring; ++DY)
		{
			for (int32 DX = -Ring; DX <= Ring; ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Ring)
				{
					continue;   // the ring's perimeter only
				}
				const double X0 = (double)(CX + DX) * CellCm;
				const double Y0 = (double)(CY + DY) * CellCm;
				bool bLimestone = false;
				for (int32 I = 0; I < 3 && !bLimestone; ++I)
				{
					for (int32 J = 0; J < 3 && !bLimestone; ++J)
					{
						bLimestone = SampleBedrock(Params, X0 + (0.17 + 0.33 * I) * CellCm, Y0 + (0.17 + 0.33 * J) * CellCm, Seed) == EMOBedrock::Limestone;
					}
				}
				if (!bLimestone)
				{
					continue;
				}
				TArray<TPair<double, FVector>> RowBest;
				RowBest.Init(TPair<double, FVector>(BestSq, FVector::ZeroVector), Columns);
				ParallelFor(Columns, [&](int32 Row)
				{
					const double Y = Y0 + (Row + 0.5) * StepCm;
					for (int32 Col = 0; Col < Columns; ++Col)
					{
						const double X = X0 + (Col + 0.5) * StepCm;
						const double DistSq = FVector2D::DistSquared(FVector2D(X, Y), From);
						if (DistSq >= RowBest[Row].Key)
						{
							continue;
						}
						const float Ground = SampleHeightCm(Params, X, Y, Seed, Voxel);
						if (SampleCaveDistanceAtGround(Params, X, Y, Ground - 200.0, Ground, Seed, 50.0f) < -50.0f)
						{
							RowBest[Row] = TPair<double, FVector>(DistSq, FVector(X, Y, Ground));
						}
					}
				});
				for (const TPair<double, FVector>& Hit : RowBest)
				{
					if (Hit.Key < BestSq && !Hit.Value.IsZero())
					{
						BestSq = Hit.Key;
						OutMouth = Hit.Value;
						bFound = true;
					}
				}
			}
		}
	}
	if (bFound)
	{
		auto Ground = [&](double X, double Y) { return SampleHeightCm(Params, X, Y, Seed, Voxel); };
		const double Gx = (Ground(OutMouth.X + 600.0, OutMouth.Y) - Ground(OutMouth.X - 600.0, OutMouth.Y)) / 1200.0;
		const double Gy = (Ground(OutMouth.X, OutMouth.Y + 600.0) - Ground(OutMouth.X, OutMouth.Y - 600.0)) / 1200.0;
		OutSlopeDeg = (float)FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(Gx * Gx + Gy * Gy)));
	}
	return bFound;
}

void FMOWorldGenDomain::FindCaveSurfaces(const FMOWorldGenParams& Params, int32 Seed, const FBox2D& Area, float SpacingCm, TArray<FMOCaveSurfacePoint>& Out)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	if (!T.bEnableCaves || !Area.bIsValid || SpacingCm <= 1.0f)
	{
		return;
	}
	const double S = SpacingCm;
	const int64 I0 = (int64)FMath::FloorToDouble(Area.Min.X / S), I1 = (int64)FMath::FloorToDouble(Area.Max.X / S);
	const int64 J0 = (int64)FMath::FloorToDouble(Area.Min.Y / S), J1 = (int64)FMath::FloorToDouble(Area.Max.Y / S);
	const int32 NumI = (int32)(I1 - I0 + 1), NumJ = (int32)(J1 - J0 + 1);
	if (NumI <= 0 || NumJ <= 0 || (int64)NumI * NumJ > 4000000)
	{
		return;
	}
	const uint32 JitterSalt = (uint32)HashSeed(Seed, SaltCaveSurfaceJitter);
	const float Voxel = T.VoxelSizeCm;
	const float DepthBand = T.CaveMaxDepthCm + T.CaveFadeCm;
	TArray<TArray<FMOCaveSurfacePoint>> Rows;
	Rows.SetNum(NumJ);
	ParallelFor(NumJ, [&](int32 RowIndex)
	{
		const int64 J = J0 + RowIndex;
		TArray<FMOCaveSurfacePoint>& Row = Rows[RowIndex];
		for (int64 I = I0; I <= I1; ++I)
		{
			const uint32 H = HashLattice2(I, J, JitterSalt);
			const double X = ((double)I + 0.15 + 0.7 * (double)Unit16(H)) * S;
			const double Y = ((double)J + 0.15 + 0.7 * (double)Unit16(H >> 16)) * S;
			if (X < Area.Min.X || X >= Area.Max.X || Y < Area.Min.Y || Y >= Area.Max.Y)
			{
				continue;   // half-open: a column belongs to exactly one cell
			}
			const float Ground = SampleHeightCm(Params, X, Y, Seed, Voxel);
			const double ZLo = FMath::Max((double)Params.CaveMinZCm, (double)Ground - DepthBand);
			const double ZHi = (double)Ground + 100.0;
			if (ZHi <= ZLo)
			{
				continue;
			}
			auto D = [&](double Z) { return SampleCaveDistanceAtGround(Params, X, Y, Z, Ground, Seed, 50.0f); };
			double Z = ZLo;
			float Dz = D(Z);
			while (Z < ZHi)
			{
				// Sphere-trace: the field is ~a distance (gradient ~1), so a step of 70% of it cannot jump a passage; 20 cm .. 4 m.
				const double Z2 = FMath::Min(Z + (double)FMath::Clamp(FMath::Abs(Dz) * 0.7f, 20.0f, 400.0f), ZHi);
				const float Dz2 = D(Z2);
				if ((Dz < 0.0f) != (Dz2 < 0.0f))
				{
					double A = Z, B = Z2;
					float DA = Dz;
					for (int32 Step = 0; Step < 7; ++Step)
					{
						const double M = 0.5 * (A + B);
						const float DM = D(M);
						if ((DM < 0.0f) == (DA < 0.0f))
						{
							A = M;
							DA = DM;
						}
						else
						{
							B = M;
						}
					}
					const double Zc = 0.5 * (A + B);
					constexpr double Hd = 20.0;
					const FVector Grad(
						(SampleCaveDistanceAtGround(Params, X + Hd, Y, Zc, Ground, Seed, 50.0f) - SampleCaveDistanceAtGround(Params, X - Hd, Y, Zc, Ground, Seed, 50.0f)) / (2.0 * Hd),
						(SampleCaveDistanceAtGround(Params, X, Y + Hd, Zc, Ground, Seed, 50.0f) - SampleCaveDistanceAtGround(Params, X, Y - Hd, Zc, Ground, Seed, 50.0f)) / (2.0 * Hd),
						(D(Zc + Hd) - D(Zc - Hd)) / (2.0 * Hd));
					const FVector AirNormal = -Grad.GetSafeNormal(1.0e-6, FVector(0.0, 0.0, Dz2 < 0.0f ? 1.0 : -1.0));   // the field grows into the rock
					FMOCaveSurfacePoint& P = Row.AddDefaulted_GetRef();
					P.Location = FVector(X, Y, Zc);
					P.AirNormal = AirNormal;
					P.Kind = AirNormal.Z > 0.55 ? EMOCaveSurfaceKind::Floor : (AirNormal.Z < -0.55 ? EMOCaveSurfaceKind::Ceiling : EMOCaveSurfaceKind::Wall);
					P.AreaM2 = (float)(S * S / 10000.0) / FMath::Max((float)FMath::Abs(AirNormal.Z), 0.25f);
					P.GroundCm = Ground;
				}
				Z = Z2;
				Dz = Dz2;
			}
		}
	});
	for (TArray<FMOCaveSurfacePoint>& Row : Rows)
	{
		Out.Append(MoveTemp(Row));
	}
}
