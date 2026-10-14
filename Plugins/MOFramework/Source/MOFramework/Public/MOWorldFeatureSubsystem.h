/**
 * =============================================================================
 * MOWorldFeatureSubsystem.h - places and streams self-contained world features (POIs)
 * =============================================================================
 *
 * FMOWorldFeaturePlacer: WHERE features are -- a pure function of (generator params, terrain seed, catalog). Each row has a lattice of SpacingCm
 *   squares; a square holds at most one candidate (probability DensityPerKm2 x its area, jittered inside its middle half, so two sites of one row
 *   are >= SpacingCm/2 apart) that must pass the row's terrain rules (biome by the generator with its slope, slope band, height above sea, dry, clear
 *   of sinkholes, optional footprint relief) and must not overlap the footprint of a higher-priority row's site. Same answer on every machine.
 * UMOWorldFeatureSubsystem: spawns each site's actor (locally, never replicated -- like PCG, every machine builds its own) while a player is within the
 *   row's SpawnDistanceCm, removes it past 1.25x, a few per tick nearest first; answers footprint queries for the biome scatter
 *   (UMOPCGBiomeSpawner skips points inside a feature).
 *
 * Commands: MO.WorldFeatures.Dump [radius_m], MO.WorldFeatures.Go <FeatureId>, cvars mo.WorldFeatures.Enabled / .SpawnsPerTick.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOWorldFeatureTypes.h"
#include "MOWorldGen.h"
#include "MOBiomeFinder.h"
#include "MOWorldFeatureSubsystem.generated.h"

struct MOFRAMEWORK_API FMOWorldFeaturePlacer
{
	struct FFeature
	{
		FName Id;
		FMOWorldFeatureDefinitionRow Row;
		TArray<int32> BiomeIndices;   // into the generator's biome list (empty = any)
		uint32 IdHash = 0;
	};

	/** Rows in any order: they are sorted by Priority (high first), then name, which fixes who wins an overlap. */
	FMOWorldFeaturePlacer(const FMOWorldGenParamsRef& InParams, int32 InTerrainSeed, const TArray<TPair<FName, FMOWorldFeatureDefinitionRow>>& Rows);

	const TArray<FFeature>& GetFeatures() const { return Features; }

	/** The site of feature FeatureIndex in lattice square Slot, if there is one (cached). */
	bool SiteAt(int32 FeatureIndex, const FIntPoint& Slot, FMOWorldFeatureSite& Out);

	/** Every site of one feature whose CENTRE is within Bounds. */
	void SitesOfFeature(int32 FeatureIndex, const FBox2D& Bounds, TArray<FMOWorldFeatureSite>& Out);

	/** Every site (all features) whose footprint touches Bounds. */
	void SitesTouching(const FBox2D& Bounds, TArray<FMOWorldFeatureSite>& Out);

	int32 GetFeatureIndex(FName FeatureId) const;

	/** The plane through 8 generator heights on a circle of RadiusCm around (X, Y): OutGradient = height change per cm (downhill = -gradient),
	 *  OutMaxDeviationCm = the farthest of those points and the centre (CentreGroundCm) from that plane. False if an edge point is in water. */
	static bool MeasureFootprintPlane(const FMOWorldGenParams& Params, int32 TerrainSeed, const FMOBiomeFinder& Finder, double X, double Y, double RadiusCm,
		float CentreGroundCm, FVector2D& OutGradient, float& OutMaxDeviationCm);

private:
	FMOWorldGenParamsRef Params;
	int32 TerrainSeed = 0;
	FMOBiomeFinder Finder;
	TArray<FFeature> Features;
	TMap<FIntVector, TOptional<FMOWorldFeatureSite>> Cache;   // (feature, slot x, slot y)

	/** The row's own rules only (no other feature considered). */
	bool Candidate(int32 FeatureIndex, const FIntPoint& Slot, FMOWorldFeatureSite& Out) const;
};

UCLASS()
class MOFRAMEWORK_API UMOWorldFeatureSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMOWorldFeatureSubsystem* Get(const UObject* WorldContext);

	/** A seed is active and the catalog loaded: sites can be computed. */
	bool IsReady();

	/** Footprints touching Bounds, as (X, Y, radius^2) -- the biome scatter's exclusion list. Game thread. Empty when not ready. */
	void GetFootprints(const FBox2D& Bounds, TArray<FVector3f>& Out);

	/** Sites whose footprint touches Bounds. Game thread. */
	void GetSites(const FBox2D& Bounds, TArray<FMOWorldFeatureSite>& Out);

	/** The nearest site of a feature to From within MaxRadiusCm (search ring by ring). */
	bool FindNearestSite(FName FeatureId, const FVector2D& From, double MaxRadiusCm, FMOWorldFeatureSite& Out);

	int32 GetNumSpawned() const { return Spawned.Num(); }

	// UTickableWorldSubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

private:
	TUniquePtr<FMOWorldFeaturePlacer> Placer;
	int32 PlacerSeed = 0;
	bool bCatalogFailed = false;
	float TickAccumulator = 0.0f;

	/** Spawned actors by (feature index, slot). */
	TMap<FIntVector, TWeakObjectPtr<AActor>> Spawned;

	bool EnsurePlacer();
	AActor* SpawnSite(const FMOWorldFeaturePlacer::FFeature& Feature, const FMOWorldFeatureSite& Site);
};
