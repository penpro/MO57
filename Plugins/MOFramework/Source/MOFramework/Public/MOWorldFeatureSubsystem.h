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
#include "MOCompositionGrounding.h"
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

	/** The terrain height (cm) at (X, Y): the world generator's, the same on every machine and known where no terrain collision exists yet. */
	float GroundHeightAt(double X, double Y) const;

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

	/** Footprints touching Bounds, as (X, Y, radius^2), one per composition (a cluster's members each have their own, so the scatter grows
	 *  between them) -- the biome scatter's exclusion list. Game thread. Empty when not ready. */
	void GetFootprints(const FBox2D& Bounds, TArray<FVector3f>& Out);

	/** Sites whose footprint touches Bounds. Game thread. */
	void GetSites(const FBox2D& Bounds, TArray<FMOWorldFeatureSite>& Out);

	/** The nearest site of a feature to From within MaxRadiusCm (search ring by ring). */
	bool FindNearestSite(FName FeatureId, const FVector2D& From, double MaxRadiusCm, FMOWorldFeatureSite& Out);

	int32 GetNumSpawned() const { return Spawned.Num(); }

	// UTickableWorldSubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

private:
	/** A feature's PCG composition finished: hide what it built from engine basic shapes (editor markers baked into Epic's assembly data), then ground it
	 *  on the terrain it landed on (FMOCompositionGrounding: authored heights from Epic's hillsides left plants floating and rocks hovering). */
	void HandleCompositionGenerated(class UPCGComponent* PCG);

public:
	/** Measure (bApply: and ground) one spawned composition; the census verb MO.WorldFeatures.Grounding and HandleCompositionGenerated share it. */
	FMOCompositionGroundingStats GroundComposition(AActor* Composition, bool bApply) const;

	/**
	 * Dev: replace one feature's authored ground (the terrain it raises, FMOFeatureTerrain) with a JSON file's (FMOWorldFeatureGround fields), or clear it
	 * (empty path), for this session only, and rebuild the voxel terrain -- tuning Content/Python/measure_feature_ground.py without saving the catalog.
	 */
	bool DevOverrideAuthoredGround(FName FeatureId, const FString& JsonPath);

	/** The spawned composition actors (any feature) within RadiusCm of Center, with their feature id. */
	void GetSpawnedNear(const FVector& Center, double RadiusCm, TArray<TPair<FName, AActor*>>& Out) const;

private:

	TUniquePtr<FMOWorldFeaturePlacer> Placer;
	int32 PlacerSeed = 0;
	bool bCatalogFailed = false;
	float TickAccumulator = 0.0f;

	/** Spawned actors by (feature index, slot x, slot y, member): each member of a cluster streams on its own distance. */
	TMap<FIntVector4, TWeakObjectPtr<AActor>> Spawned;

	bool EnsurePlacer();

	/** Dev overrides of rows' authored ground (DevOverrideAuthoredGround): applied over the catalog for the terrain. */
	TMap<FName, FMOWorldFeatureGround> DevGroundOverrides;

	/** DT_WorldFeatures' rows (the settings' table). False (logged once) if there is none. */
	bool LoadCatalog(TArray<TPair<FName, FMOWorldFeatureDefinitionRow>>& OutRows);
	AActor* SpawnMember(const FMOWorldFeaturePlacer::FFeature& Feature, const FMOWorldFeatureSite& Site, int32 MemberIndex);
};
