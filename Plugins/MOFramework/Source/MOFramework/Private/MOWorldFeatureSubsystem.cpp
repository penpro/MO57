#include "MOWorldFeatureSubsystem.h"
#include "MOWorldFeatureSettings.h"
#include "MOWorldSeedSubsystem.h"
#include "MOFramework.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Crc.h"
#include "PCGComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MOCompositionGrounding.h"
#include "MOFeatureTerrain.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"

namespace
{
	TAutoConsoleVariable<int32> CVarWorldFeaturesEnabled(TEXT("mo.WorldFeatures.Enabled"), 1, TEXT("0 = place no world features (POIs)."), ECVF_Default);
	TAutoConsoleVariable<FString> CVarWorldFeaturesSkip(TEXT("mo.WorldFeatures.Skip"), TEXT(""),
		TEXT("Dev / A-B: comma-separated feature ids that are not spawned (e.g. ED_RockSpire)."), ECVF_Default);
	TAutoConsoleVariable<int32> CVarWorldFeaturesGround(TEXT("mo.WorldFeatures.Ground"), 0,
		TEXT("1 = ground every composition on the terrain it landed on when it finishes generating (FMOCompositionGrounding); 0 = leave the authored heights."), ECVF_Default);
	TAutoConsoleVariable<int32> CVarWorldFeaturesSpawnsPerTick(TEXT("mo.WorldFeatures.SpawnsPerTick"), 1, TEXT("World feature actors spawned per update (0.5 s), nearest first: each one builds its whole composition."), ECVF_Default);

	constexpr float WorldFeaturesUpdateSeconds = 0.5f;
	constexpr float WorldFeaturesDespawnFactor = 1.25f;

	FORCEINLINE uint64 WorldFeatureMix64(uint64 X)
	{
		X ^= X >> 30; X *= 0xbf58476d1ce4e5b9ULL;
		X ^= X >> 27; X *= 0x94d049bb133111ebULL;
		X ^= X >> 31;
		return X;
	}

	/** Uniform [0, 1) from a hash and a stream index: integer math only (the same bits on every machine). */
	FORCEINLINE double WorldFeatureU01(uint64 H, uint64 K)
	{
		return static_cast<double>(WorldFeatureMix64(H + K * 0x9E3779B97F4A7C15ULL) >> 11) * (1.0 / 9007199254740992.0);
	}
}

// ============================================================================
// FMOWorldFeaturePlacer
// ============================================================================

FMOWorldFeaturePlacer::FMOWorldFeaturePlacer(const FMOWorldGenParamsRef& InParams, int32 InTerrainSeed, const TArray<TPair<FName, FMOWorldFeatureDefinitionRow>>& Rows)
	: Params(InParams), TerrainSeed(InTerrainSeed), Finder(InParams, InTerrainSeed)
{
	for (const TPair<FName, FMOWorldFeatureDefinitionRow>& Row : Rows)
	{
		if (!Row.Value.bEnabled || Row.Value.DensityPerKm2 <= 0.0f)
		{
			continue;
		}
		FFeature& F = Features.AddDefaulted_GetRef();
		F.Id = Row.Key;
		F.Row = Row.Value;
		F.IdHash = FCrc::StrCrc32(*Row.Key.ToString());
		for (const FName& Biome : Row.Value.Biomes)
		{
			const int32 Index = Params->FindBiomeIndex(Biome);
			if (Index != INDEX_NONE)
			{
				F.BiomeIndices.Add(Index);
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldFeatures] %s: unknown biome '%s' (ignored)"), *Row.Key.ToString(), *Biome.ToString());
			}
		}
		if (Row.Value.Biomes.Num() > 0 && F.BiomeIndices.IsEmpty())
		{
			Features.Pop();   // every listed biome is unknown: placing it anywhere would be wrong
		}
	}
	Features.Sort([](const FFeature& A, const FFeature& B)
	{
		return A.Row.Priority != B.Row.Priority ? A.Row.Priority > B.Row.Priority : A.Id.LexicalLess(B.Id);
	});
}

int32 FMOWorldFeaturePlacer::GetFeatureIndex(FName FeatureId) const
{
	return Features.IndexOfByPredicate([FeatureId](const FFeature& F) { return F.Id == FeatureId; });
}

bool FMOWorldFeaturePlacer::MeasureFootprintPlane(const FMOWorldGenParams& InParams, int32 InTerrainSeed, const FMOBiomeFinder& InFinder, double X, double Y,
	double RadiusCm, float CentreGroundCm, FVector2D& OutGradient, float& OutMaxDeviationCm)
{
	// Least squares over 8 evenly spaced points: the cos/sin sums cancel, so the plane is closed form (gradient = sum(h * dir) / (4 R)).
	float Heights[8];
	double Mean = 0.0, Gx = 0.0, Gy = 0.0;
	for (int32 K = 0; K < 8; ++K)
	{
		const double C = FMath::Cos(K * UE_PI / 4.0), S = FMath::Sin(K * UE_PI / 4.0);
		const double Ax = X + RadiusCm * C, Ay = Y + RadiusCm * S;
		Heights[K] = FMOWorldGen::SampleHeightCm(InParams, Ax, Ay, InTerrainSeed, InParams.Tuning.VoxelSizeCm);
		if (InFinder.IsInWater(Ax, Ay, Heights[K]))
		{
			return false;
		}
		Mean += Heights[K] / 8.0;
		Gx += Heights[K] * C;
		Gy += Heights[K] * S;
	}
	OutGradient = FVector2D(Gx / (4.0 * RadiusCm), Gy / (4.0 * RadiusCm));
	double MaxDev = FMath::Abs(CentreGroundCm - Mean);
	for (int32 K = 0; K < 8; ++K)
	{
		const double Plane = Mean + RadiusCm * (OutGradient.X * FMath::Cos(K * UE_PI / 4.0) + OutGradient.Y * FMath::Sin(K * UE_PI / 4.0));
		MaxDev = FMath::Max(MaxDev, FMath::Abs(Heights[K] - Plane));
	}
	OutMaxDeviationCm = static_cast<float>(MaxDev);
	return true;
}

bool FMOWorldFeaturePlacer::Candidate(int32 FeatureIndex, const FIntPoint& Slot, FMOWorldFeatureSite& Out) const
{
	const FFeature& F = Features[FeatureIndex];
	const FMOWorldFeatureDefinitionRow& R = F.Row;
	const double Spacing = R.SpacingCm;
	const uint64 H = WorldFeatureMix64(WorldFeatureMix64(WorldFeatureMix64((static_cast<uint64>(F.IdHash) << 32) ^ static_cast<uint32>(TerrainSeed)) ^ static_cast<uint32>(Slot.X)) ^ (static_cast<uint64>(static_cast<uint32>(Slot.Y)) << 1));

	// At most one site per square; DensityPerKm2 x the square's area is its chance (a density above one per square saturates).
	const double Chance = FMath::Min(1.0, static_cast<double>(R.DensityPerKm2) * FMath::Square(Spacing / 100000.0));
	if (WorldFeatureU01(H, 0) >= Chance)
	{
		return false;
	}
	const double X = (Slot.X + 0.25 + 0.5 * WorldFeatureU01(H, 1)) * Spacing;
	const double Y = (Slot.Y + 0.25 + 0.5 * WorldFeatureU01(H, 2)) * Spacing;
	const double Limit = Params->Tuning.WorldHalfSizeCm - R.GetMaxSiteRadiusCm() - 50000.0;
	if (FMath::Abs(X) > Limit || FMath::Abs(Y) > Limit)
	{
		return false;
	}

	// The row's terrain rules at one composition's spot, for a footprint of FootprintCm (a 4x spire needs 4x the clear ground).
	auto PassesRules = [this, &F, &R](double Px, double Py, double FootprintCm, float& OutGround, float& OutSlope) -> bool
	{
		const int32 Biome = Finder.BiomeAt(Px, Py, OutGround, OutSlope);
		const float AboveSea = OutGround - Params->Tuning.SeaLevelCm;
		return !((F.BiomeIndices.Num() > 0 && !F.BiomeIndices.Contains(Biome)) || AboveSea < R.MinHeightAboveSeaCm || AboveSea > R.MaxHeightAboveSeaCm
			|| Finder.IsInWater(Px, Py, OutGround) || FMOWorldGen::DistanceToSinkholeRimZone(*Params, Px, Py, TerrainSeed) < FootprintCm);
	};
	auto ReliefOk = [this, &R](double Px, double Py, double FootprintCm, float Ground) -> bool
	{
		if (R.MaxFootprintReliefCm <= 0.0f || FootprintCm <= 0.0)
		{
			return true;
		}
		for (int32 K = 0; K < 8; ++K)
		{
			const double Ax = Px + FootprintCm * FMath::Cos(K * UE_PI / 4.0), Ay = Py + FootprintCm * FMath::Sin(K * UE_PI / 4.0);
			const float Edge = FMOWorldGen::SampleHeightCm(*Params, Ax, Ay, TerrainSeed, Params->Tuning.VoxelSizeCm);
			if (FMath::Abs(Edge - Ground) > R.MaxFootprintReliefCm || Finder.IsInWater(Ax, Ay, Edge))
			{
				return false;
			}
		}
		return true;
	};

	// How far the ground falls from Ground at (Px, Py) to the lowest of 8 points round a core of CoreCm: the member sinks that much more.
	auto SlopeSink = [this](double Px, double Py, double CoreCm, float Ground) -> float
	{
		float Lowest = Ground;
		for (int32 K = 0; K < 8 && CoreCm > 0.0; ++K)
		{
			Lowest = FMath::Min(Lowest, FMOWorldGen::SampleHeightCm(*Params, Px + CoreCm * FMath::Cos(K * UE_PI / 4.0), Py + CoreCm * FMath::Sin(K * UE_PI / 4.0),
				TerrainSeed, Params->Tuning.VoxelSizeCm));
		}
		return Ground - Lowest;
	};

	Out.FeatureId = F.Id;
	Out.Slot = Slot;
	Out.Members.Reset();

	if (R.IsCluster() && R.bClusterLine && R.bAlignToSlope)
	{
		// A line along the contour (a cliff band): the largest at the centre, then alternately right and left of it, each new member one pair of
		// cores beyond the last on its side, the direction re-read from the local contour so the band bends with the hill. Every member is judged
		// on its own footprint plane and faces its own downhill.
		const int32 CountMin = FMath::Max(1, R.ClusterCountMin);
		const int32 CountMax = FMath::Max(CountMin, R.ClusterCountMax);
		const int32 Count = CountMin + FMath::Min(CountMax - CountMin, static_cast<int32>(WorldFeatureU01(H, 5) * (CountMax - CountMin + 1)));
		TArray<float, TInlineAllocator<32>> Scales;
		for (int32 I = 0; I < Count; ++I)
		{
			Scales.Add(FMath::Lerp(R.MinScale, FMath::Max(R.MinScale, R.MaxScale), FMath::Pow(static_cast<float>(WorldFeatureU01(H, 16 + static_cast<uint64>(I))), R.ClusterScaleBias)));
		}
		Scales.Sort(TGreater<float>());
		// One member at (Mx, My): the row's rules over its core, its plane's slope and relief; false = this spot ends that side of the line.
		auto TryMember = [&](double Mx, double My, float Scale, uint64 K, FVector2D& OutGradient) -> bool
		{
			const double Core = R.GetCoreRadiusCm(Scale);
			const double Footprint = static_cast<double>(R.FootprintRadiusCm) * Scale;
			float Ground = 0.0f, CentreSlope = 0.0f, Deviation = 0.0f;
			if ((R.ClusterRadiusCm > 0.0f && FVector2D::Distance(FVector2D(Mx, My), FVector2D(X, Y)) > R.ClusterRadiusCm)
				|| !PassesRules(Mx, My, Core, Ground, CentreSlope)
				|| !MeasureFootprintPlane(*Params, TerrainSeed, Finder, Mx, My, Core, Ground, OutGradient, Deviation))
			{
				return false;
			}
			const float Slope = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan(OutGradient.Size())));
			if (Slope < R.SlopeMinDeg || Slope > R.SlopeMaxDeg || (R.MaxFootprintReliefCm > 0.0f && Deviation > R.MaxFootprintReliefCm) || OutGradient.IsNearlyZero())
			{
				return false;
			}
			const double Downhill = FMath::RadiansToDegrees(FMath::Atan2(-OutGradient.Y, -OutGradient.X));
			const double Jitter = (2.0 * WorldFeatureU01(H, K + 3) - 1.0) * R.AlignYawJitterDeg;
			FMOWorldFeatureMember& Member = Out.Members.AddDefaulted_GetRef();
			Member.Location = FVector(Mx, My, Ground);
			Member.YawDegrees = static_cast<float>(FMath::Fmod(Downhill - R.AuthoredDownhillYawDeg + Jitter + 720.0, 360.0));
			Member.Scale = Scale;
			Member.FootprintRadiusCm = static_cast<float>(Footprint);
			Member.SlopeSinkCm = 0.0f;   // aligned, as a single hillside composition
			Member.Seed = static_cast<int32>(WorldFeatureMix64(H ^ (0x5EEDULL + K)) & 0x7fffffff);
			return true;
		};
		FVector2D CentreGradient;
		if (!TryMember(X, Y, Scales[0], 300, CentreGradient))
		{
			return false;
		}
		// The two sides of the line: their last member, the direction they grow in (along the contour), and whether they still can.
		FVector2D End[2] = { FVector2D(X, Y), FVector2D(X, Y) };
		float EndScale[2] = { Scales[0], Scales[0] };
		const FVector2D Along = FVector2D(-CentreGradient.Y, CentreGradient.X).GetSafeNormal();
		FVector2D Dir[2] = { Along, -Along };
		bool bOpen[2] = { true, true };
		for (int32 I = 1; I < Count && (bOpen[0] || bOpen[1]); ++I)
		{
			const int32 Side = bOpen[I % 2] ? I % 2 : 1 - I % 2;
			const float Scale = Scales[I];
			const double Step = (R.GetCoreRadiusCm(EndScale[Side]) + R.GetCoreRadiusCm(Scale)) * (1.0 + 0.15 * WorldFeatureU01(H, 300 + static_cast<uint64>(I) * 4 + 1));
			const FVector2D P = End[Side] + Dir[Side] * Step;
			FVector2D Gradient;
			if (!TryMember(P.X, P.Y, Scale, 300 + static_cast<uint64>(I) * 4, Gradient))
			{
				bOpen[Side] = false;
				continue;
			}
			// Along the local contour, the same way round as before: the band follows the hill.
			const FVector2D LocalAlong = FVector2D(-Gradient.Y, Gradient.X).GetSafeNormal();
			Dir[Side] = FVector2D::DotProduct(LocalAlong, Dir[Side]) >= 0.0 ? LocalAlong : -LocalAlong;
			End[Side] = P;
			EndScale[Side] = Scale;
		}
		Out.Location = Out.Members[0].Location;
		Out.YawDegrees = Out.Members[0].YawDegrees;
		Out.Scale = 0.0f;
		float Reach = 0.0f;
		for (const FMOWorldFeatureMember& Member : Out.Members)
		{
			Out.Scale = FMath::Max(Out.Scale, Member.Scale);
			Reach = FMath::Max(Reach, static_cast<float>(FVector2D::Distance(FVector2D(Member.Location), FVector2D(X, Y))) + Member.FootprintRadiusCm);
		}
		Out.FootprintRadiusCm = Reach;
		Out.Seed = Out.Members[0].Seed;
		return true;
	}

	if (R.IsCluster())
	{
		// The centre only has to be the right kind of land; every member is judged at its own spot.
		float CentreGround = 0.0f, CentreSlope = 0.0f;
		if (!PassesRules(X, Y, 0.0, CentreGround, CentreSlope))
		{
			return false;
		}
		const int32 CountMin = FMath::Max(1, R.ClusterCountMin);
		const int32 CountMax = FMath::Max(CountMin, R.ClusterCountMax);
		const int32 Count = CountMin + FMath::Min(CountMax - CountMin, static_cast<int32>(WorldFeatureU01(H, 5) * (CountMax - CountMin + 1)));
		// The scales first, largest first: the largest stands at the centre and the rest pack round the ones already standing, their solid cores
		// (CoreFraction) nearly touching -- one formation, not scattered singles (Wes, Oct 23 2026, BR23-6: "the large ones are central ... and the
		// smaller ones are scattered around the larger ones ... push them all much closer together so they almost overlap"). Every member passes the
		// row's terrain rules over its core.
		TArray<float, TInlineAllocator<32>> Scales;
		for (int32 I = 0; I < Count; ++I)
		{
			Scales.Add(FMath::Lerp(R.MinScale, FMath::Max(R.MinScale, R.MaxScale), FMath::Pow(static_cast<float>(WorldFeatureU01(H, 16 + static_cast<uint64>(I))), R.ClusterScaleBias)));
		}
		Scales.Sort(TGreater<float>());
		float Reach = 0.0f;
		for (int32 I = 0; I < Count; ++I)
		{
			const float Scale = Scales[I];
			const double Footprint = static_cast<double>(R.FootprintRadiusCm) * Scale;
			const double Core = R.GetCoreRadiusCm(Scale);
			const int32 Tries = Out.Members.IsEmpty() ? 24 : 8;   // the first to stand looks over the whole disc: the formation grows round it
			for (int32 Try = 0; Try < Tries; ++Try)
			{
				const uint64 K = 300 + static_cast<uint64>(I * 24 + Try) * 4;
				double Mx = X, My = Y;
				if (!Out.Members.IsEmpty())
				{
					// Beside one already standing (the earlier, larger ones more often): cores touching, a small gap.
					const int32 A = FMath::Min(Out.Members.Num() - 1, static_cast<int32>(FMath::Square(WorldFeatureU01(H, K)) * Out.Members.Num()));
					const FMOWorldFeatureMember& Anchor = Out.Members[A];
					const double AnchorCore = R.GetCoreRadiusCm(Anchor.Scale);
					const double Dist = (AnchorCore + Core) * (1.0 + 0.25 * WorldFeatureU01(H, K + 1));
					const double Angle = 2.0 * UE_DOUBLE_PI * WorldFeatureU01(H, K + 2);
					Mx = Anchor.Location.X + Dist * FMath::Cos(Angle);
					My = Anchor.Location.Y + Dist * FMath::Sin(Angle);
				}
				else if (Try > 0)
				{
					// The centre would not take the largest: anywhere in the disc (the flatter ground the old scatter found), nearer first.
					const double Radius = R.ClusterRadiusCm * FMath::Min(1.0, (Try / static_cast<double>(Tries - 1))) * FMath::Sqrt(WorldFeatureU01(H, K + 1));
					const double Angle = 2.0 * UE_DOUBLE_PI * WorldFeatureU01(H, K + 2);
					Mx = X + Radius * FMath::Cos(Angle);
					My = Y + Radius * FMath::Sin(Angle);
				}
				if (R.ClusterRadiusCm > 0.0f && FVector2D::Distance(FVector2D(Mx, My), FVector2D(X, Y)) > R.ClusterRadiusCm)
				{
					continue;
				}
				// Cores may touch, not stand inside each other.
				const bool bCrowded = Out.Members.ContainsByPredicate([&R, Mx, My, Core](const FMOWorldFeatureMember& M)
				{
					return FVector2D::Distance(FVector2D(M.Location), FVector2D(Mx, My)) < 0.9 * (Core + R.GetCoreRadiusCm(M.Scale));
				});
				float Ground = 0.0f, Slope = 0.0f;
				if (bCrowded || !PassesRules(Mx, My, Core, Ground, Slope) || Slope < R.SlopeMinDeg || Slope > R.SlopeMaxDeg || !ReliefOk(Mx, My, Core, Ground))
				{
					continue;
				}
				FMOWorldFeatureMember& Member = Out.Members.AddDefaulted_GetRef();
				Member.Location = FVector(Mx, My, Ground);
				Member.YawDegrees = static_cast<float>(WorldFeatureU01(H, K + 3) * 360.0);
				Member.Scale = Scale;
				Member.FootprintRadiusCm = static_cast<float>(Footprint);
				Member.SlopeSinkCm = SlopeSink(Mx, My, Core, Ground);
				Member.Seed = static_cast<int32>(WorldFeatureMix64(H ^ (0x5EEDULL + K)) & 0x7fffffff);
				Reach = FMath::Max(Reach, static_cast<float>(FVector2D::Distance(FVector2D(Mx, My), FVector2D(X, Y)) + Footprint));
				break;
			}
		}
		if (Out.Members.IsEmpty())
		{
			return false;
		}
		Out.Location = FVector(X, Y, CentreGround);
		Out.YawDegrees = Out.Members[0].YawDegrees;
		Out.Scale = 0.0f;
		for (const FMOWorldFeatureMember& Member : Out.Members)
		{
			Out.Scale = FMath::Max(Out.Scale, Member.Scale);
		}
		Out.FootprintRadiusCm = Reach;
		Out.Seed = Out.Members[0].Seed;
		return true;
	}

	// A single composition. Its scale first: every footprint check covers it at the size it will stand.
	const float Scale = FMath::Lerp(R.MinScale, FMath::Max(R.MinScale, R.MaxScale), static_cast<float>(WorldFeatureU01(H, 4)));
	const double Footprint = static_cast<double>(R.FootprintRadiusCm) * Scale;
	float Ground = 0.0f, Slope = 0.0f;
	const bool bAlign = R.bAlignToSlope && Footprint > 0.0;
	FVector2D Gradient = FVector2D::ZeroVector;
	float Deviation = 0.0f;
	if (!PassesRules(X, Y, Footprint, Ground, Slope))
	{
		return false;
	}
	if (bAlign)
	{
		// A hillside composition: the slope that matters is the footprint's, not the centre's.
		if (!MeasureFootprintPlane(*Params, TerrainSeed, Finder, X, Y, Footprint, Ground, Gradient, Deviation))
		{
			return false;
		}
		Slope = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan(Gradient.Size())));
		if (R.MaxFootprintReliefCm > 0.0f && Deviation > R.MaxFootprintReliefCm)
		{
			return false;
		}
	}
	else if (!ReliefOk(X, Y, Footprint, Ground))
	{
		return false;
	}
	if (Slope < R.SlopeMinDeg || Slope > R.SlopeMaxDeg)
	{
		return false;
	}
	Out.Location = FVector(X, Y, Ground);
	if (bAlign)
	{
		const double Downhill = FMath::RadiansToDegrees(FMath::Atan2(-Gradient.Y, -Gradient.X));
		const double Jitter = (2.0 * WorldFeatureU01(H, 3) - 1.0) * R.AlignYawJitterDeg;
		Out.YawDegrees = static_cast<float>(FMath::Fmod(Downhill - R.AuthoredDownhillYawDeg + Jitter + 720.0, 360.0));
	}
	else
	{
		Out.YawDegrees = static_cast<float>(WorldFeatureU01(H, 3) * 360.0);
	}
	Out.Scale = Scale;
	Out.FootprintRadiusCm = static_cast<float>(Footprint);
	Out.Seed = static_cast<int32>(WorldFeatureMix64(H ^ 0x5EEDULL) & 0x7fffffff);
	FMOWorldFeatureMember& Member = Out.Members.AddDefaulted_GetRef();
	Member.Location = Out.Location;
	Member.YawDegrees = Out.YawDegrees;
	Member.Scale = Out.Scale;
	Member.FootprintRadiusCm = Out.FootprintRadiusCm;
	Member.SlopeSinkCm = bAlign ? 0.0f : SlopeSink(X, Y, R.GetCoreRadiusCm(Scale), Ground);
	Member.Seed = Out.Seed;
	return true;
}

float FMOWorldFeaturePlacer::GroundHeightAt(double X, double Y) const
{
	return FMOWorldGen::SampleHeightCm(*Params, X, Y, TerrainSeed, Params->Tuning.VoxelSizeCm);
}

bool FMOWorldFeaturePlacer::SiteAt(int32 FeatureIndex, const FIntPoint& Slot, FMOWorldFeatureSite& Out)
{
	const FIntVector Key(FeatureIndex, Slot.X, Slot.Y);
	if (const TOptional<FMOWorldFeatureSite>* Hit = Cache.Find(Key))
	{
		if (Hit->IsSet())
		{
			Out = Hit->GetValue();
		}
		return Hit->IsSet();
	}
	FMOWorldFeatureSite Site;
	bool bOk = Candidate(FeatureIndex, Slot, Site);
	// Placed only near sites of the features it names (BR23-5): within NearWithinCm of one's footprint (the overlap rule below keeps it off it).
	// Only higher-precedence features count, so the search never recurses into this feature.
	const FMOWorldFeatureDefinitionRow& Mine = Features[FeatureIndex].Row;
	if (bOk && Mine.NearFeatureIds.Num() > 0)
	{
		bool bNear = false;
		for (int32 G = 0; G < FeatureIndex && !bNear; ++G)
		{
			if (!Mine.NearFeatureIds.Contains(Features[G].Id))
			{
				continue;
			}
			const FMOWorldFeatureDefinitionRow& Other = Features[G].Row;
			const double Reach = Site.FootprintRadiusCm + Mine.NearWithinCm + Other.GetMaxSiteRadiusCm();
			const int32 X0 = FMath::FloorToInt((Site.Location.X - Reach) / Other.SpacingCm), X1 = FMath::FloorToInt((Site.Location.X + Reach) / Other.SpacingCm);
			const int32 Y0 = FMath::FloorToInt((Site.Location.Y - Reach) / Other.SpacingCm), Y1 = FMath::FloorToInt((Site.Location.Y + Reach) / Other.SpacingCm);
			for (int32 Sx = X0; Sx <= X1 && !bNear; ++Sx)
			{
				for (int32 Sy = Y0; Sy <= Y1 && !bNear; ++Sy)
				{
					FMOWorldFeatureSite Theirs;
					bNear = SiteAt(G, FIntPoint(Sx, Sy), Theirs)
						&& FVector2D::Distance(FVector2D(Theirs.Location), FVector2D(Site.Location)) < Site.FootprintRadiusCm + Theirs.FootprintRadiusCm + Mine.NearWithinCm;
				}
			}
		}
		bOk = bNear;
	}
	// A higher-precedence feature (earlier in the sorted list) keeps its ground: this site may not overlap its footprint.
	for (int32 G = 0; G < FeatureIndex && bOk; ++G)
	{
		const FMOWorldFeatureDefinitionRow& Other = Features[G].Row;
		const double Reach = Site.FootprintRadiusCm + Other.GetMaxSiteRadiusCm();
		const int32 X0 = FMath::FloorToInt((Site.Location.X - Reach) / Other.SpacingCm), X1 = FMath::FloorToInt((Site.Location.X + Reach) / Other.SpacingCm);
		const int32 Y0 = FMath::FloorToInt((Site.Location.Y - Reach) / Other.SpacingCm), Y1 = FMath::FloorToInt((Site.Location.Y + Reach) / Other.SpacingCm);
		for (int32 Sx = X0; Sx <= X1 && bOk; ++Sx)
		{
			for (int32 Sy = Y0; Sy <= Y1 && bOk; ++Sy)
			{
				FMOWorldFeatureSite Theirs;
				if (SiteAt(G, FIntPoint(Sx, Sy), Theirs)
					&& FVector2D::Distance(FVector2D(Theirs.Location), FVector2D(Site.Location)) < Site.FootprintRadiusCm + Theirs.FootprintRadiusCm)
				{
					bOk = false;
				}
			}
		}
	}
	Cache.Add(Key, bOk ? TOptional<FMOWorldFeatureSite>(Site) : TOptional<FMOWorldFeatureSite>());
	if (bOk)
	{
		Out = Site;
	}
	return bOk;
}

void FMOWorldFeaturePlacer::SitesOfFeature(int32 FeatureIndex, const FBox2D& Bounds, TArray<FMOWorldFeatureSite>& Out)
{
	const double Spacing = Features[FeatureIndex].Row.SpacingCm;
	const int32 X0 = FMath::FloorToInt(Bounds.Min.X / Spacing), X1 = FMath::FloorToInt(Bounds.Max.X / Spacing);
	const int32 Y0 = FMath::FloorToInt(Bounds.Min.Y / Spacing), Y1 = FMath::FloorToInt(Bounds.Max.Y / Spacing);
	for (int32 Sx = X0; Sx <= X1; ++Sx)
	{
		for (int32 Sy = Y0; Sy <= Y1; ++Sy)
		{
			FMOWorldFeatureSite Site;
			if (SiteAt(FeatureIndex, FIntPoint(Sx, Sy), Site) && Bounds.IsInside(FVector2D(Site.Location)))
			{
				Out.Add(Site);
			}
		}
	}
}

void FMOWorldFeaturePlacer::SitesTouching(const FBox2D& Bounds, TArray<FMOWorldFeatureSite>& Out)
{
	for (int32 F = 0; F < Features.Num(); ++F)
	{
		const FMOWorldFeatureDefinitionRow& R = Features[F].Row;
		const double Grow = R.GetMaxSiteRadiusCm();
		TArray<FMOWorldFeatureSite> Sites;
		SitesOfFeature(F, FBox2D(Bounds.Min - FVector2D(Grow), Bounds.Max + FVector2D(Grow)), Sites);
		for (const FMOWorldFeatureSite& Site : Sites)
		{
			const FVector2D C(Site.Location);
			const FVector2D Closest(FMath::Clamp(C.X, Bounds.Min.X, Bounds.Max.X), FMath::Clamp(C.Y, Bounds.Min.Y, Bounds.Max.Y));
			if (FVector2D::DistSquared(C, Closest) <= FMath::Square(Site.FootprintRadiusCm))
			{
				Out.Add(Site);
			}
		}
	}
}

// ============================================================================
// UMOWorldFeatureSubsystem
// ============================================================================

UMOWorldFeatureSubsystem* UMOWorldFeatureSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMOWorldFeatureSubsystem>() : nullptr;
}

bool UMOWorldFeatureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UMOWorldFeatureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The terrain rises to the compositions' authored ground (FMOFeatureTerrain) wherever the voxel terrain is made: it needs the catalog before the
	// voxel runtime generates its first chunk, which follows the world seed -- after this.
	TArray<TPair<FName, FMOWorldFeatureDefinitionRow>> Rows;
	if (LoadCatalog(Rows))
	{
		FMOFeatureTerrain::SetCatalog(Rows);
	}
}

bool UMOWorldFeatureSubsystem::LoadCatalog(TArray<TPair<FName, FMOWorldFeatureDefinitionRow>>& OutRows)
{
	const UMOWorldFeatureSettings* Settings = GetDefault<UMOWorldFeatureSettings>();
	UDataTable* Table = Settings ? Settings->FeatureTable.LoadSynchronous() : nullptr;
	if (!Table || Table->GetRowStruct() != FMOWorldFeatureDefinitionRow::StaticStruct())
	{
		if (!bCatalogFailed)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldFeatures] no feature catalog (%s): no POIs will be placed"), Settings ? *Settings->FeatureTable.ToString() : TEXT("no settings"));
		}
		bCatalogFailed = true;
		return false;
	}
	Table->ForeachRow<FMOWorldFeatureDefinitionRow>(TEXT("MOWorldFeatures"), [&OutRows](const FName& Name, const FMOWorldFeatureDefinitionRow& Row) { OutRows.Emplace(Name, Row); });
	return true;
}

void UMOWorldFeatureSubsystem::Deinitialize()
{
	for (TPair<FIntVector4, TWeakObjectPtr<AActor>>& Pair : Spawned)
	{
		if (AActor* Actor = Pair.Value.Get())
		{
			Actor->Destroy();
		}
	}
	Spawned.Reset();
	Placer.Reset();
	Super::Deinitialize();
}

TStatId UMOWorldFeatureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMOWorldFeatureSubsystem, STATGROUP_Tickables);
}

bool UMOWorldFeatureSubsystem::IsTickable() const
{
	return !IsTemplate() && GetWorld() && GetWorld()->IsGameWorld();
}

bool UMOWorldFeatureSubsystem::EnsurePlacer()
{
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(GetWorld());
	if (bCatalogFailed || !Seeds || !Seeds->HasActiveSeed() || !FMOWorldGenParamsProvider::HasPublished())
	{
		return false;
	}
	const int32 TerrainSeed = Seeds->GetActiveTerrainSeed();
	if (Placer.IsValid() && PlacerSeed == TerrainSeed)
	{
		return true;
	}
	TArray<TPair<FName, FMOWorldFeatureDefinitionRow>> Rows;
	if (!LoadCatalog(Rows))
	{
		return false;
	}
	Placer = MakeUnique<FMOWorldFeaturePlacer>(FMOWorldGenParamsProvider::Get(), TerrainSeed, Rows);
	PlacerSeed = TerrainSeed;
	UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldFeatures] catalog: %d row(s), %d placeable, terrain seed %d"), Rows.Num(), Placer->GetFeatures().Num(), TerrainSeed);
	return true;
}

bool UMOWorldFeatureSubsystem::IsReady()
{
	return CVarWorldFeaturesEnabled.GetValueOnGameThread() != 0 && EnsurePlacer();
}

void UMOWorldFeatureSubsystem::GetSites(const FBox2D& Bounds, TArray<FMOWorldFeatureSite>& Out)
{
	if (IsReady())
	{
		Placer->SitesTouching(Bounds, Out);
	}
}

void UMOWorldFeatureSubsystem::GetFootprints(const FBox2D& Bounds, TArray<FFootprint>& Out)
{
	TArray<FMOWorldFeatureSite> Sites;
	GetSites(Bounds, Sites);
	for (const FMOWorldFeatureSite& Site : Sites)
	{
		const int32 F = Placer ? Placer->GetFeatureIndex(Site.FeatureId) : INDEX_NONE;
		const float Core = F != INDEX_NONE ? FMath::Clamp(Placer->GetFeatures()[F].Row.CoreFraction, 0.05f, 1.0f) : 1.0f;
		for (const FMOWorldFeatureMember& Member : Site.Members)
		{
			FFootprint& Foot = Out.AddDefaulted_GetRef();
			Foot.X = static_cast<float>(Member.Location.X);
			Foot.Y = static_cast<float>(Member.Location.Y);
			Foot.LowGrowthSq = FMath::Square(FMath::Min(Member.FootprintRadiusCm, Member.FootprintRadiusCm * Core * TreeClearanceCores));
			Foot.CoreSq = FMath::Square(Member.FootprintRadiusCm * Core);
			Foot.NearSq = FMath::Square(Member.FootprintRadiusCm * TreeBoostFootprints);
		}
	}
}

const FMOWorldFeatureDefinitionRow* UMOWorldFeatureSubsystem::FindRow(FName FeatureId) const
{
	const int32 F = Placer ? Placer->GetFeatureIndex(FeatureId) : INDEX_NONE;
	return F != INDEX_NONE ? &Placer->GetFeatures()[F].Row : nullptr;
}

bool UMOWorldFeatureSubsystem::FindNearestSite(FName FeatureId, const FVector2D& From, double MaxRadiusCm, FMOWorldFeatureSite& Out)
{
	if (!IsReady())
	{
		return false;
	}
	const int32 F = Placer->GetFeatureIndex(FeatureId);
	if (F == INDEX_NONE)
	{
		return false;
	}
	const double Spacing = Placer->GetFeatures()[F].Row.SpacingCm;
	for (double R = Spacing; R <= MaxRadiusCm + Spacing; R *= 2.0)
	{
		TArray<FMOWorldFeatureSite> Sites;
		Placer->SitesOfFeature(F, FBox2D(From - FVector2D(R), From + FVector2D(R)), Sites);
		Sites.Sort([&From](const FMOWorldFeatureSite& A, const FMOWorldFeatureSite& B) { return FVector2D::DistSquared(FVector2D(A.Location), From) < FVector2D::DistSquared(FVector2D(B.Location), From); });
		if (Sites.Num() > 0 && FVector2D::Distance(FVector2D(Sites[0].Location), From) <= R)
		{
			Out = Sites[0];   // nearest within the square's inscribed circle: nothing outside the square can be nearer
			return true;
		}
	}
	return false;
}

AActor* UMOWorldFeatureSubsystem::SpawnMember(const FMOWorldFeaturePlacer::FFeature& Feature, const FMOWorldFeatureSite& Site, int32 MemberIndex)
{
	UWorld* World = GetWorld();
	{
		TArray<FString> Skipped;
		CVarWorldFeaturesSkip.GetValueOnGameThread().ParseIntoArray(Skipped, TEXT(","));
		if (Skipped.Contains(Feature.Id.ToString()))
		{
			return nullptr;   // recorded as spawned-nothing: no retry each tick
		}
	}
	UClass* Class = Feature.Row.ActorClass.LoadSynchronous();
	if (!World || !Class || !Site.Members.IsValidIndex(MemberIndex))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldFeatures] %s: actor class %s did not load"), *Feature.Id.ToString(), *Feature.Row.ActorClass.ToString());
		return nullptr;
	}
	const FMOWorldFeatureMember& Member = Site.Members[MemberIndex];
	// The same frame FMOFeatureTerrain raises the ground in (the composition's authored ground is relative to it).
	const FTransform Transform = FMOFeatureTerrain::MemberRootTransform(Feature.Row, Member);
	AActor* Actor = World->SpawnActorDeferred<AActor>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Actor)
	{
		return nullptr;
	}
	// Every machine builds its own copy from the same site (like PCG): never replicated, so a host's copy does not double a client's.
	Actor->SetReplicates(false);
	Actor->Tags.AddUnique(Feature.Id);   // the row id: a bug report's LookAt names the feature
	for (const TPair<FName, FString>& Value : Feature.Row.ActorProperties)
	{
		FProperty* Property = Class->FindPropertyByName(Value.Key);
		if (!Property || !Property->ImportText_Direct(*Value.Value, Property->ContainerPtrToValuePtr<void>(Actor), Actor, PPF_None))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldFeatures] %s: could not set %s = %s on %s"), *Feature.Id.ToString(), *Value.Key.ToString(), *Value.Value, *Class->GetName());
		}
	}
	TInlineComponentArray<UPCGComponent*> PCGComponents(Actor);
	for (UPCGComponent* PCG : PCGComponents)
	{
		PCG->Seed = Member.Seed;   // each composition its own variation, the same on every machine
	}
	Actor->FinishSpawning(Transform);
	// The composition's PCG component(s) as they exist NOW: a Blueprint composition (Electric Dreams' BP_PCG_*Assembly) gets its component from its
	// construction script, which ran inside FinishSpawning -- binding before it found none and the basic-shape sweep never ran (measured: the
	// embankment's 7 spheres still visible). A composition that generated already is swept at once.
	TInlineComponentArray<UPCGComponent*> Compositions(Actor);
	for (UPCGComponent* PCG : Compositions)
	{
		// The site's seed reaches a Blueprint composition only now: set above, it found no component, and every Electric Dreams assembly in a world was
		// the same arrangement. One that already started generating with the default seed starts again with its own.
		if (PCG->Seed != Member.Seed)
		{
			PCG->Seed = Member.Seed;
			if (PCG->IsGenerating() || PCG->bGenerated)
			{
				// Clean up first: GenerateLocal(force) alone ADDS to what was generated (measured: the piece count doubled per regeneration).
				PCG->CancelGeneration();
				PCG->CleanupLocal(/*bRemoveComponents=*/true);
				PCG->GenerateLocal(/*bForce=*/true);
			}
		}
		if (!PCG->OnPCGGraphGeneratedDelegate.IsBoundToObject(this))
		{
			PCG->OnPCGGraphGeneratedDelegate.AddUObject(this, &UMOWorldFeatureSubsystem::HandleCompositionGenerated);
		}
		if (PCG->bGenerated)
		{
			HandleCompositionGenerated(PCG);
		}
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldFeatures] spawned %s at %s (slot %d,%d member %d/%d scale %.2f seed %d)"), *Feature.Id.ToString(), *Member.Location.ToCompactString(),
		Site.Slot.X, Site.Slot.Y, MemberIndex + 1, Site.Members.Num(), Member.Scale, Member.Seed);
	return Actor;
}

void UMOWorldFeatureSubsystem::HandleCompositionGenerated(UPCGComponent* PCG)
{
	// Electric Dreams' assemblies are point data baked from Epic's source levels (PCGAsset elements), and those levels held editor markers: the
	// embankment assembly (EmbankmentTree_PCG) bakes seven /Engine/BasicShapes spheres, and they stood in the world as white orbs (Steam playtest
	// Oct 16 2026: "WHITE ORB"). An engine basic shape is never content: whatever a composition built from one is hidden and does not collide.
	AActor* Owner = PCG ? PCG->GetOwner() : nullptr;
	if (!Owner)
	{
		return;
	}
	int32 Hidden = 0;
	TInlineComponentArray<UInstancedStaticMeshComponent*> Isms(Owner);
	for (UInstancedStaticMeshComponent* Ism : Isms)
	{
		const UStaticMesh* Mesh = Ism ? Ism->GetStaticMesh() : nullptr;
		if (Mesh && Ism->IsVisible() && Mesh->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/")))
		{
			Hidden += Ism->GetInstanceCount();
			Ism->SetVisibility(false);
			Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	if (Hidden > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldFeatures] %s: hid %d engine basic-shape instance(s) (editor markers in the composition's data)"), *Owner->GetName(), Hidden);
	}

	if (CVarWorldFeaturesGround.GetValueOnGameThread() != 0 && Placer)
	{
		const double Start = FPlatformTime::Seconds();
		const FMOCompositionGroundingStats Stats = GroundComposition(Owner, true);
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldFeatures] %s grounded in %.1f ms: %s"), *Owner->GetName(), (FPlatformTime::Seconds() - Start) * 1000.0, *Stats.Describe());
	}
}

FMOCompositionGroundingStats UMOWorldFeatureSubsystem::GroundComposition(AActor* Composition, bool bApply) const
{
	if (!Placer || !Composition)
	{
		return FMOCompositionGroundingStats();
	}
	// The ground as the voxel terrain makes it: the generator's, raised to the authored ground of the compositions placed over it.
	const FMOWorldFeaturePlacer& P = *Placer;
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	const int32 Seed = PlacerSeed;
	return FMOCompositionGrounding::Ground(Composition, [&P, &Params, Seed](double X, double Y)
	{
		return FMOFeatureTerrain::Apply(Params, Seed, X, Y, P.GroundHeightAt(X, Y));
	}, bApply);
}

bool UMOWorldFeatureSubsystem::DevOverrideAuthoredGround(FName FeatureId, const FString& JsonPath)
{
	if (JsonPath.IsEmpty())
	{
		DevGroundOverrides.Remove(FeatureId);
	}
	else
	{
		FString Text;
		FMOWorldFeatureGround Ground;
		if (!FFileHelper::LoadFileToString(Text, *JsonPath) || !FJsonObjectConverter::JsonObjectStringToUStruct(Text, &Ground) || !Ground.IsSet())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldFeatures] ground override: %s is not a usable FMOWorldFeatureGround"), *JsonPath);
			return false;
		}
		DevGroundOverrides.Add(FeatureId, MoveTemp(Ground));
	}
	TArray<TPair<FName, FMOWorldFeatureDefinitionRow>> Rows;
	if (!LoadCatalog(Rows))
	{
		return false;
	}
	for (TPair<FName, FMOWorldFeatureDefinitionRow>& Row : Rows)
	{
		if (const FMOWorldFeatureGround* Override = DevGroundOverrides.Find(Row.Key))
		{
			Row.Value.AuthoredGround = *Override;
		}
		else if (Row.Key == FeatureId)
		{
			Row.Value.AuthoredGround = FMOWorldFeatureGround();   // cleared
		}
	}
	FMOFeatureTerrain::SetCatalog(Rows);
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(GetWorld());
	const bool bRebuilt = Seeds && Seeds->HasActiveSeed() && UMOWorldSeedSubsystem::RegenerateVoxelWorld(GetWorld(), Seeds->GetActiveSeed(), Seeds->GetActiveParameterName(), this);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] GROUNDOVERRIDE %s %s; terrain rebuilt: %d"), *FeatureId.ToString(), JsonPath.IsEmpty() ? TEXT("cleared") : *JsonPath, bRebuilt ? 1 : 0);
	return true;
}

void UMOWorldFeatureSubsystem::GetSpawnedNear(const FVector& Center, double RadiusCm, TArray<TPair<FName, AActor*>>& Out) const
{
	if (!Placer)
	{
		return;
	}
	const TArray<FMOWorldFeaturePlacer::FFeature>& Features = Placer->GetFeatures();
	for (const TPair<FIntVector4, TWeakObjectPtr<AActor>>& It : Spawned)
	{
		AActor* Actor = It.Value.Get();
		if (Actor && Features.IsValidIndex(It.Key.X) && FVector::Dist2D(Actor->GetActorLocation(), Center) <= RadiusCm)
		{
			Out.Emplace(Features[It.Key.X].Id, Actor);
		}
	}
}

void UMOWorldFeatureSubsystem::Tick(float DeltaTime)
{
	TickAccumulator += DeltaTime;
	if (TickAccumulator < WorldFeaturesUpdateSeconds)
	{
		return;
	}
	TickAccumulator = 0.0f;
	UWorld* World = GetWorld();
	if (!World || !IsReady())
	{
		return;
	}
	// Viewers: every player's pawn this machine knows (the server knows all, so collision holds for joiners' pawns too; a client only its own).
	TArray<FVector2D, TInlineAllocator<8>> Viewers;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
		{
			Viewers.Add(FVector2D(Pawn->GetActorLocation()));
		}
	}
	auto NearestViewerDist = [&Viewers](const FVector2D& P)
	{
		double Best = TNumericLimits<double>::Max();
		for (const FVector2D& V : Viewers)
		{
			Best = FMath::Min(Best, FVector2D::Distance(V, P));
		}
		return Best;
	};

	// Remove what no player is near any more.
	const TArray<FMOWorldFeaturePlacer::FFeature>& Features = Placer->GetFeatures();
	for (auto It = Spawned.CreateIterator(); It; ++It)
	{
		AActor* Actor = It->Value.Get();
		const int32 F = It->Key.X;
		if (!Actor || !Features.IsValidIndex(F))
		{
			It.RemoveCurrent();
			continue;
		}
		if (NearestViewerDist(FVector2D(Actor->GetActorLocation())) > Features[F].Row.SpawnDistanceCm * WorldFeaturesDespawnFactor)
		{
			Actor->Destroy();
			It.RemoveCurrent();
		}
	}

	// Spawn the nearest missing compositions (a cluster's members one by one), a few per update.
	struct FWanted { int32 Feature; FMOWorldFeatureSite Site; int32 Member; double Dist; };
	TArray<FWanted> Wanted;
	TSet<FIntVector4> WantedKeys;
	for (int32 F = 0; F < Features.Num(); ++F)
	{
		const FMOWorldFeatureDefinitionRow& Row = Features[F].Row;
		if (Row.ActorClass.IsNull())
		{
			continue;
		}
		const double Reach = Row.SpawnDistanceCm;
		const double Grow = Reach + (Row.IsCluster() ? Row.ClusterRadiusCm : 0.0);   // a cluster's centre may be farther than its nearest member
		for (const FVector2D& V : Viewers)
		{
			TArray<FMOWorldFeatureSite> Sites;
			Placer->SitesOfFeature(F, FBox2D(V - FVector2D(Grow), V + FVector2D(Grow)), Sites);
			for (const FMOWorldFeatureSite& Site : Sites)
			{
				for (int32 M = 0; M < Site.Members.Num(); ++M)
				{
					const FIntVector4 Key(F, Site.Slot.X, Site.Slot.Y, M);
					const double D = NearestViewerDist(FVector2D(Site.Members[M].Location));
					if (D <= Reach && !Spawned.Contains(Key) && !WantedKeys.Contains(Key))
					{
						WantedKeys.Add(Key);
						Wanted.Add({ F, Site, M, D });
					}
				}
			}
		}
	}
	Wanted.Sort([](const FWanted& A, const FWanted& B) { return A.Dist < B.Dist; });
	const int32 Budget = FMath::Max(1, CVarWorldFeaturesSpawnsPerTick.GetValueOnGameThread());
	for (int32 I = 0; I < FMath::Min(Budget, Wanted.Num()); ++I)
	{
		const FWanted& W = Wanted[I];
		// A failed spawn is recorded as null: no retry every tick; the next pass clears it.
		Spawned.Add(FIntVector4(W.Feature, W.Site.Slot.X, W.Site.Slot.Y, W.Member), SpawnMember(Features[W.Feature], W.Site, W.Member));
	}
}

// ============================================================================
// Commands
// ============================================================================

static FAutoConsoleCommandWithWorldAndArgs GMOWorldFeaturesGroundOverrideCmd(
	TEXT("MO.WorldFeatures.GroundOverride"),
	TEXT("Dev: a feature's authored ground (the terrain it raises) from a JSON file, or 'clear', for this session; rebuilds the terrain. Usage: MO.WorldFeatures.GroundOverride <FeatureId> <path.json|clear>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UMOWorldFeatureSubsystem* Features = UMOWorldFeatureSubsystem::Get(World);
		if (!Features || Args.Num() < 2)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] GROUNDOVERRIDE usage: MO.WorldFeatures.GroundOverride <FeatureId> <path.json|clear>"));
			return;
		}
		Features->DevOverrideAuthoredGround(FName(*Args[0]), Args[1].Equals(TEXT("clear"), ESearchCase::IgnoreCase) ? FString() : Args[1]);
	}));

static FAutoConsoleCommandWithWorldAndArgs GMOWorldFeaturesGroundingCmd(
	TEXT("MO.WorldFeatures.Grounding"),
	TEXT("Dev: the grounding census of every spawned composition near the local pawn -- plants floating / buried, rocks with a gap under them. 'apply' grounds them too. Usage: MO.WorldFeatures.Grounding [radius_m=600] [apply]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UMOWorldFeatureSubsystem* Features = UMOWorldFeatureSubsystem::Get(World);
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Features || !Pawn)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] GROUNDING not ready (needs a pawn and the world features)"));
			return;
		}
		const double R = (Args.Num() > 0 && Args[0].IsNumeric() ? FCString::Atof(*Args[0]) : 600.0) * 100.0;
		const bool bApply = Args.ContainsByPredicate([](const FString& A) { return A.Equals(TEXT("apply"), ESearchCase::IgnoreCase); });
		TArray<TPair<FName, AActor*>> Near;
		Features->GetSpawnedNear(Pawn->GetActorLocation(), R, Near);
		FMOCompositionGroundingStats Total;
		for (const TPair<FName, AActor*>& It : Near)
		{
			const FMOCompositionGroundingStats S = Features->GroundComposition(It.Value, bApply);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] GROUNDING   %s %s at %.0f m: %s"), *It.Key.ToString(), *It.Value->GetName(),
				FVector::Dist2D(It.Value->GetActorLocation(), Pawn->GetActorLocation()) / 100.0, *S.Describe());
			Total.Foliage += S.Foliage; Total.FoliageFloating += S.FoliageFloating; Total.FoliageBuried += S.FoliageBuried;
			Total.MaxFoliageFloatCm = FMath::Max(Total.MaxFoliageFloatCm, S.MaxFoliageFloatCm); Total.MaxFoliageBuryCm = FMath::Max(Total.MaxFoliageBuryCm, S.MaxFoliageBuryCm);
			Total.Rocks += S.Rocks; Total.RocksFloating += S.RocksFloating; Total.MaxRockGapCm = FMath::Max(Total.MaxRockGapCm, S.MaxRockGapCm);
			Total.Clinging += S.Clinging; Total.Moved += S.Moved;
		}
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] GROUNDING %d composition(s) within %.0f m%s: %s"), Near.Num(), R / 100.0, bApply ? TEXT(" (applied)") : TEXT(""), *Total.Describe());
	}));

static FAutoConsoleCommandWithWorldAndArgs GMOWorldFeaturesDumpCmd(
	TEXT("MO.WorldFeatures.Dump"),
	TEXT("Dev: the world features (POIs) around the local pawn: feature, distance, spawned? Usage: MO.WorldFeatures.Dump [radius_m=600]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UMOWorldFeatureSubsystem* Features = UMOWorldFeatureSubsystem::Get(World);
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Features || !Pawn || !Features->IsReady())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WORLDFEATURES not ready (needs a pawn, a world seed and DT_WorldFeatures)"));
			return;
		}
		const double R = (Args.Num() > 0 ? FCString::Atof(*Args[0]) : 600.0) * 100.0;
		const FVector2D C(Pawn->GetActorLocation());
		TArray<FMOWorldFeatureSite> Sites;
		Features->GetSites(FBox2D(C - FVector2D(R), C + FVector2D(R)), Sites);
		Sites.Sort([&C](const FMOWorldFeatureSite& A, const FMOWorldFeatureSite& B) { return FVector2D::DistSquared(FVector2D(A.Location), C) < FVector2D::DistSquared(FVector2D(B.Location), C); });
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WORLDFEATURES %d site(s) within %.0f m, %d actor(s) spawned"), Sites.Num(), R / 100.0, Features->GetNumSpawned());
		for (const FMOWorldFeatureSite& Site : Sites)
		{
			FString Scales;
			for (const FMOWorldFeatureMember& Member : Site.Members)
			{
				Scales += FString::Printf(TEXT("%s%.2f"), Scales.IsEmpty() ? TEXT("") : TEXT(","), Member.Scale);
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WORLDFEATURES   %s at (%.0f, %.0f, %.0f) %.0f m footprint %.0f m seed %d members %d scales %s"), *Site.FeatureId.ToString(), Site.Location.X, Site.Location.Y, Site.Location.Z,
				FVector2D::Distance(FVector2D(Site.Location), C) / 100.0, Site.FootprintRadiusCm / 100.0, Site.Seed, Site.Members.Num(), *Scales);
		}
	}));
