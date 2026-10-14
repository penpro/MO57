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
	const double Limit = Params->Tuning.WorldHalfSizeCm - R.FootprintRadiusCm - 50000.0;
	if (FMath::Abs(X) > Limit || FMath::Abs(Y) > Limit)
	{
		return false;
	}
	float Ground = 0.0f, Slope = 0.0f;
	const int32 Biome = Finder.BiomeAt(X, Y, Ground, Slope);
	const float AboveSea = Ground - Params->Tuning.SeaLevelCm;
	if ((F.BiomeIndices.Num() > 0 && !F.BiomeIndices.Contains(Biome)) || Slope < R.SlopeMinDeg || Slope > R.SlopeMaxDeg
		|| AboveSea < R.MinHeightAboveSeaCm || AboveSea > R.MaxHeightAboveSeaCm || Finder.IsInWater(X, Y, Ground)
		|| FMOWorldGen::DistanceToSinkholeRimZone(*Params, X, Y, TerrainSeed) < R.FootprintRadiusCm)
	{
		return false;
	}
	if (R.MaxFootprintReliefCm > 0.0f && R.FootprintRadiusCm > 0.0f)
	{
		for (int32 K = 0; K < 8; ++K)
		{
			const double Ax = X + R.FootprintRadiusCm * FMath::Cos(K * UE_PI / 4.0), Ay = Y + R.FootprintRadiusCm * FMath::Sin(K * UE_PI / 4.0);
			const float Edge = FMOWorldGen::SampleHeightCm(*Params, Ax, Ay, TerrainSeed, Params->Tuning.VoxelSizeCm);
			if (FMath::Abs(Edge - Ground) > R.MaxFootprintReliefCm || Finder.IsInWater(Ax, Ay, Edge))
			{
				return false;
			}
		}
	}
	Out.FeatureId = F.Id;
	Out.Slot = Slot;
	Out.Location = FVector(X, Y, Ground);
	Out.YawDegrees = static_cast<float>(WorldFeatureU01(H, 3) * 360.0);
	Out.Scale = FMath::Lerp(R.MinScale, FMath::Max(R.MinScale, R.MaxScale), static_cast<float>(WorldFeatureU01(H, 4)));
	Out.FootprintRadiusCm = R.FootprintRadiusCm * Out.Scale;
	Out.Seed = static_cast<int32>(WorldFeatureMix64(H ^ 0x5EEDULL) & 0x7fffffff);
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
		const double Reach = Site.FootprintRadiusCm + Other.FootprintRadiusCm * FMath::Max(Other.MinScale, Other.MaxScale);
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
		const double Grow = R.FootprintRadiusCm * FMath::Max(R.MinScale, R.MaxScale);
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
	for (TPair<FIntVector, TWeakObjectPtr<AActor>>& Pair : Spawned)
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
		Out.Add(FVector3f(static_cast<float>(Site.Location.X), static_cast<float>(Site.Location.Y), FMath::Square(Site.FootprintRadiusCm)));
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

AActor* UMOWorldFeatureSubsystem::SpawnSite(const FMOWorldFeaturePlacer::FFeature& Feature, const FMOWorldFeatureSite& Site)
{
	UWorld* World = GetWorld();
	UClass* Class = Feature.Row.ActorClass.LoadSynchronous();
	if (!World || !Class)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldFeatures] %s: actor class %s did not load"), *Feature.Id.ToString(), *Feature.Row.ActorClass.ToString());
		return nullptr;
	}
	const FTransform Transform(FRotator(0.0f, Site.YawDegrees, 0.0f), Site.Location - FVector(0.0, 0.0, Feature.Row.SinkCm), FVector(Site.Scale));
	AActor* Actor = World->SpawnActorDeferred<AActor>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Actor)
	{
		return nullptr;
	}
	// Every machine builds its own copy from the same site (like PCG): never replicated, so a host's copy does not double a client's.
	Actor->SetReplicates(false);
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
		PCG->Seed = Site.Seed;   // each site its own variation, the same on every machine
	}
	Actor->FinishSpawning(Transform);
	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldFeatures] spawned %s at %s (slot %d,%d seed %d)"), *Feature.Id.ToString(), *Site.Location.ToCompactString(), Site.Slot.X, Site.Slot.Y, Site.Seed);
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

	// Spawn the nearest missing sites, a few per update.
	struct FWanted { int32 Feature; FMOWorldFeatureSite Site; double Dist; };
	TArray<FWanted> Wanted;
	for (int32 F = 0; F < Features.Num(); ++F)
	{
		if (Features[F].Row.ActorClass.IsNull())
		{
			continue;
		}
		const double Reach = Features[F].Row.SpawnDistanceCm;
		for (const FVector2D& V : Viewers)
		{
			TArray<FMOWorldFeatureSite> Sites;
			Placer->SitesOfFeature(F, FBox2D(V - FVector2D(Reach), V + FVector2D(Reach)), Sites);
			for (const FMOWorldFeatureSite& Site : Sites)
			{
				const FIntVector Key(F, Site.Slot.X, Site.Slot.Y);
				const double D = NearestViewerDist(FVector2D(Site.Location));
				if (D <= Reach && !Spawned.Contains(Key) && !Wanted.ContainsByPredicate([&Key](const FWanted& W) { return W.Feature == Key.X && W.Site.Slot == FIntPoint(Key.Y, Key.Z); }))
				{
					Wanted.Add({ F, Site, D });
				}
			}
		}
	}
	Wanted.Sort([](const FWanted& A, const FWanted& B) { return A.Dist < B.Dist; });
	const int32 Budget = FMath::Max(1, CVarWorldFeaturesSpawnsPerTick.GetValueOnGameThread());
	for (int32 I = 0; I < FMath::Min(Budget, Wanted.Num()); ++I)
	{
		const FWanted& W = Wanted[I];
		if (AActor* Actor = SpawnSite(Features[W.Feature], W.Site))
		{
			Spawned.Add(FIntVector(W.Feature, W.Site.Slot.X, W.Site.Slot.Y), Actor);
		}
		else
		{
			Spawned.Add(FIntVector(W.Feature, W.Site.Slot.X, W.Site.Slot.Y), nullptr);   // do not retry every tick; the next pass clears it
		}
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
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WORLDFEATURES   %s at (%.0f, %.0f, %.0f) %.0f m footprint %.0f m seed %d"), *Site.FeatureId.ToString(), Site.Location.X, Site.Location.Y, Site.Location.Z,
				FVector2D::Distance(FVector2D(Site.Location), C) / 100.0, Site.FootprintRadiusCm / 100.0, Site.Seed);
		}
	}));
