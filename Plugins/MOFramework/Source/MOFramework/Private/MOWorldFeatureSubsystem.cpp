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

namespace
{
	TAutoConsoleVariable<int32> CVarWorldFeaturesEnabled(TEXT("mo.WorldFeatures.Enabled"), 1, TEXT("0 = place no world features (POIs)."), ECVF_Default);
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

	Out.FeatureId = F.Id;
	Out.Slot = Slot;
	Out.Members.Reset();

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
		float Reach = 0.0f;
		for (int32 I = 0; I < Count; ++I)
		{
			for (int32 Try = 0; Try < 6; ++Try)
			{
				const uint64 K = 16 + static_cast<uint64>(I * 6 + Try) * 5;
				const double Radius = R.ClusterRadiusCm * FMath::Sqrt(WorldFeatureU01(H, K));   // even over the disc
				const double Angle = 2.0 * UE_DOUBLE_PI * WorldFeatureU01(H, K + 1);
				const float Scale = FMath::Lerp(R.MinScale, FMath::Max(R.MinScale, R.MaxScale), FMath::Pow(static_cast<float>(WorldFeatureU01(H, K + 2)), R.ClusterScaleBias));
				const double Footprint = static_cast<double>(R.FootprintRadiusCm) * Scale;
				const double Mx = X + Radius * FMath::Cos(Angle), My = Y + Radius * FMath::Sin(Angle);
				// Members may touch, not stand inside each other.
				const bool bCrowded = Out.Members.ContainsByPredicate([Mx, My, Footprint](const FMOWorldFeatureMember& M)
				{
					return FVector2D::Distance(FVector2D(M.Location), FVector2D(Mx, My)) < 0.6 * (Footprint + M.FootprintRadiusCm);
				});
				float Ground = 0.0f, Slope = 0.0f;
				if (bCrowded || !PassesRules(Mx, My, Footprint, Ground, Slope) || Slope < R.SlopeMinDeg || Slope > R.SlopeMaxDeg || !ReliefOk(Mx, My, Footprint, Ground))
				{
					continue;
				}
				FMOWorldFeatureMember& Member = Out.Members.AddDefaulted_GetRef();
				Member.Location = FVector(Mx, My, Ground);
				Member.YawDegrees = static_cast<float>(WorldFeatureU01(H, K + 3) * 360.0);
				Member.Scale = Scale;
				Member.FootprintRadiusCm = static_cast<float>(Footprint);
				Member.Seed = static_cast<int32>(WorldFeatureMix64(H ^ (0x5EEDULL + K)) & 0x7fffffff);
				Reach = FMath::Max(Reach, static_cast<float>(Radius + Footprint));
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
	Member.Seed = Out.Seed;
	return true;
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
	const UMOWorldFeatureSettings* Settings = GetDefault<UMOWorldFeatureSettings>();
	UDataTable* Table = Settings ? Settings->FeatureTable.LoadSynchronous() : nullptr;
	if (!Table || Table->GetRowStruct() != FMOWorldFeatureDefinitionRow::StaticStruct())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldFeatures] no feature catalog (%s): no POIs will be placed"), Settings ? *Settings->FeatureTable.ToString() : TEXT("no settings"));
		bCatalogFailed = true;
		return false;
	}
	TArray<TPair<FName, FMOWorldFeatureDefinitionRow>> Rows;
	Table->ForeachRow<FMOWorldFeatureDefinitionRow>(TEXT("MOWorldFeatures"), [&Rows](const FName& Name, const FMOWorldFeatureDefinitionRow& Row) { Rows.Emplace(Name, Row); });
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

void UMOWorldFeatureSubsystem::GetFootprints(const FBox2D& Bounds, TArray<FVector3f>& Out)
{
	TArray<FMOWorldFeatureSite> Sites;
	GetSites(Bounds, Sites);
	for (const FMOWorldFeatureSite& Site : Sites)
	{
		for (const FMOWorldFeatureMember& Member : Site.Members)
		{
			Out.Add(FVector3f(static_cast<float>(Member.Location.X), static_cast<float>(Member.Location.Y), FMath::Square(Member.FootprintRadiusCm)));
		}
	}
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
	UClass* Class = Feature.Row.ActorClass.LoadSynchronous();
	if (!World || !Class || !Site.Members.IsValidIndex(MemberIndex))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldFeatures] %s: actor class %s did not load"), *Feature.Id.ToString(), *Feature.Row.ActorClass.ToString());
		return nullptr;
	}
	const FMOWorldFeatureMember& Member = Site.Members[MemberIndex];
	// The sink grows with the composition: a gap under a 4x outcrop is 4x the gap under the authored one.
	const FTransform Transform(FRotator(0.0f, Member.YawDegrees, 0.0f), Member.Location - FVector(0.0, 0.0, Feature.Row.SinkCm * Member.Scale), FVector(Member.Scale));
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
	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldFeatures] spawned %s at %s (slot %d,%d member %d/%d scale %.2f seed %d)"), *Feature.Id.ToString(), *Member.Location.ToCompactString(),
		Site.Slot.X, Site.Slot.Y, MemberIndex + 1, Site.Members.Num(), Member.Scale, Member.Seed);
	return Actor;
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
