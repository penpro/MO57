/**
 * =============================================================================
 * MOWorldFeatureTypes.h - World feature catalog (DT_WorldFeatures) and placed sites
 * =============================================================================
 *
 * A WORLD FEATURE is a self-contained, hand-tuned composition placed as ONE unit -- a cliff with its vines, boulders and debris; a grove with a
 * fallen log and ferns; later ruins, camps, springs (Docs/World_Features_Architecture.md). Wes, Oct 14 2026: "we should be building entire pcg
 * POIs that have all these things self contained so they're very fine tuned and then spawning the larger POIs ... rather than just spawning a
 * bunch of one off instances of things".
 *
 * The composition lives in the feature's ACTOR (typically a Blueprint whose own PCG component builds everything, like Electric Dreams'
 * BP_PCG_LargeAssembly). This row only says where it may go and how it stands. Placement is deterministic from the world seed (every co-op
 * machine computes the same sites); actors are spawned locally near players (UMOWorldFeatureSubsystem) and the biome scatter stays out of each
 * feature's footprint.
 *
 * Adding a feature = one actor + one row. No code.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MOWorldFeatureTypes.generated.h"

/** How a feature gets into the world. Only Many is implemented (the rest are the design doc's later phases). */
UENUM(BlueprintType)
enum class EMOWorldFeaturePolicy : uint8
{
	Many UMETA(DisplayName="Many (scattered by density)")
};

/**
 * The ground a composition was AUTHORED on, in its own frame (cm at scale 1, relative to the actor root): the terrain RISES to it under every site
 * (FMOFeatureTerrain), never sinks. Electric Dreams' hillside sets carry their own landform -- the cliff assembly is a 4 m plateau behind a cliff face;
 * on our flat ground the plateau was a slab floating over voids (Steam playtest Oct 16 2026, #15 / #19). Measured in game from the composition's
 * ground-lying pieces by Content/Python/measure_feature_ground.py. Unset = the composition stands on our terrain as it is.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWorldFeatureGround
{
	GENERATED_BODY()

	/** The grid's first cell centre (min X, min Y) in the composition's frame (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ground")
	FVector2D OriginCm = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ground", meta=(ClampMin="10.0"))
	float CellCm = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ground", meta=(ClampMin="0"))
	int32 NumX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ground", meta=(ClampMin="0"))
	int32 NumY = 0;

	/** NumX * NumY authored ground heights (cm, composition frame), X fastest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ground")
	TArray<float> HeightsCm;

	/** NumX * NumY: how much the raise counts there (1 on the authored ground, fading to 0 over a margin around it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ground")
	TArray<float> Weights;

	bool IsSet() const { return NumX > 1 && NumY > 1 && HeightsCm.Num() == NumX * NumY && Weights.Num() == NumX * NumY; }

	/** The grid's extent in the composition's frame (cm). */
	FBox2D GetBounds() const
	{
		return FBox2D(OriginCm, OriginCm + FVector2D((NumX - 1) * CellCm, (NumY - 1) * CellCm));
	}

	/** Bilinear authored height and weight at a point of the composition's frame. False outside the grid. */
	bool Sample(double X, double Y, float& OutHeightCm, float& OutWeight) const
	{
		const double Gx = (X - OriginCm.X) / CellCm, Gy = (Y - OriginCm.Y) / CellCm;
		if (!IsSet() || Gx < 0.0 || Gy < 0.0 || Gx > NumX - 1 || Gy > NumY - 1)
		{
			return false;
		}
		const int32 X0 = FMath::Min(static_cast<int32>(Gx), NumX - 2), Y0 = FMath::Min(static_cast<int32>(Gy), NumY - 2);
		const float Fx = static_cast<float>(Gx - X0), Fy = static_cast<float>(Gy - Y0);
		auto At = [this](const TArray<float>& A, int32 Ix, int32 Iy) { return A[Iy * NumX + Ix]; };
		auto Bi = [&](const TArray<float>& A)
		{
			return FMath::Lerp(FMath::Lerp(At(A, X0, Y0), At(A, X0 + 1, Y0), Fx), FMath::Lerp(At(A, X0, Y0 + 1), At(A, X0 + 1, Y0 + 1), Fx), Fy);
		};
		OutHeightCm = Bi(HeightsCm);
		OutWeight = Bi(Weights);
		return true;
	}
};

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWorldFeatureDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	EMOWorldFeaturePolicy Policy = EMOWorldFeaturePolicy::Many;

	/** False = never placed (keeps the row while it is being tuned). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
	bool bEnabled = true;

	/** The self-contained composition: an actor that builds the whole feature itself (e.g. a Blueprint with its own PCG component and graph). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Composition")
	TSoftClassPtr<AActor> ActorClass;

	/**
	 * Values set on the spawned actor before it finishes spawning, by property name (text form, as the Details panel would take it): picks which
	 * variant of a configurable composition this row places. Several rows may share one ActorClass with different values.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Composition")
	TMap<FName, FString> ActorProperties;

	/** Generator biome ids the feature may stand in (empty = any land). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	TArray<FName> Biomes;

	/** Expected sites per square kilometre of land that passes every placement rule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.0"))
	float DensityPerKm2 = 2.0f;

	/** Two sites of this feature are never closer than half of this (one jittered candidate per square of this size). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="1000.0"))
	float SpacingCm = 25000.0f;

	/** The composition's radius: the biome scatter plants nothing inside it, and a lower-priority feature may not overlap it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.0"))
	float FootprintRadiusCm = 2000.0f;

	/** Where two features would overlap, the higher priority keeps its site. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.0", ClampMax="90.0"))
	float SlopeMinDeg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.0", ClampMax="90.0"))
	float SlopeMaxDeg = 30.0f;

	/** Ground height above sea level the site must have (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	float MinHeightAboveSeaCm = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	float MaxHeightAboveSeaCm = 100000.0f;

	/** Max height difference (cm) between the centre and 8 points on the footprint's edge: compositions built on flat ground stay off bumps (0 = no check).
	 *  With bAlignToSlope: the edge points' distance from the plane through them (the hillside may be steep, not lumpy). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.0"))
	float MaxFootprintReliefCm = 0.0f;

	/** Authored on a hillside (Electric Dreams' big cliff sets): the slope is the plane through the footprint's edge, not the centre's, and the
	 *  composition turns so AuthoredDownhillYawDeg faces the terrain's downhill. SlopeMin/MaxDeg then bound that plane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	bool bAlignToSlope = false;

	/** The composition's own downhill direction (degrees in its local frame, 0 = +X, 90 = +Y). Only with bAlignToSlope. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(EditCondition="bAlignToSlope"))
	float AuthoredDownhillYawDeg = 0.0f;

	/** Random turn either side of the aligned yaw (degrees). Only with bAlignToSlope. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(EditCondition="bAlignToSlope", ClampMin="0.0", ClampMax="180.0"))
	float AlignYawJitterDeg = 10.0f;

	/** Buried this deep (cm at scale 1; it grows with the site's scale): a composition authored on a flat floor shows no gap on uneven ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.0"))
	float SinkCm = 30.0f;

	/** The ground the composition was authored on: the terrain rises to it under the site (FMOFeatureTerrain). Unset = stands on our terrain as it is. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	FMOWorldFeatureGround AuthoredGround;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.1"))
	float MinScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.1"))
	float MaxScale = 1.0f;

	/** Spawned while a player is within this distance (cm), removed past 1.25x of it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Streaming", meta=(ClampMin="5000.0"))
	float SpawnDistanceCm = 40000.0f;

	/**
	 * A CLUSTER of the composition instead of one (Wes, Oct 15 2026, of the rock spires: "a range from 1 to 10 in an area, ranging from 50% original
	 * size up to the 400% big towers so there's a whole neat looking area"): each site places ClusterCountMin..ClusterCountMax copies scattered within
	 * ClusterRadiusCm of its centre, each at its own scale in [MinScale, MaxScale], yaw and ground height, kept apart by their footprints; every member
	 * passes the row's terrain rules on its own. 1 / 1 with no radius (the default) = one composition at the site, as before. Clusters ignore bAlignToSlope.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cluster", meta=(ClampMin="1", ClampMax="32"))
	int32 ClusterCountMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cluster", meta=(ClampMin="1", ClampMax="32"))
	int32 ClusterCountMax = 1;

	/** How far from the site's centre members may stand (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cluster", meta=(ClampMin="0.0"))
	float ClusterRadiusCm = 0.0f;

	/** How member scales spread over [MinScale, MaxScale]: 1 = evenly, above 1 = mostly small with a few giants. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cluster", meta=(ClampMin="0.1"))
	float ClusterScaleBias = 1.0f;

	bool IsCluster() const { return ClusterCountMax > 1 || ClusterRadiusCm > 0.0f; }

	/** The farthest any part of one of this row's sites can reach from its centre (cm): overlap and query margins. */
	float GetMaxSiteRadiusCm() const { return (IsCluster() ? ClusterRadiusCm : 0.0f) + FootprintRadiusCm * FMath::Max(MinScale, MaxScale); }
};

/** One composition a site places: the site itself for a single feature, one of several for a cluster. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWorldFeatureMember
{
	GENERATED_BODY()

	/** XY on the generator's ground; Z = generator ground (before SinkCm). */
	UPROPERTY(BlueprintReadOnly, Category="WorldFeature")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="WorldFeature")
	float YawDegrees = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="WorldFeature")
	float Scale = 1.0f;

	/** The row's footprint at this member's scale. */
	UPROPERTY(BlueprintReadOnly, Category="WorldFeature")
	float FootprintRadiusCm = 0.0f;

	/** Seed for this member's own PCG variation. */
	UPROPERTY(BlueprintReadOnly, Category="WorldFeature")
	int32 Seed = 0;
};

/** One placed feature: a pure function of (world seed, row, slot). */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWorldFeatureSite
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="WorldFeature")
	FName FeatureId;

	/** The candidate square (in SpacingCm units) this site came from: with FeatureId, its identity. */
	UPROPERTY(BlueprintReadOnly, Category="WorldFeature")
	FIntPoint Slot = FIntPoint::ZeroValue;

	/** XY on the generator's ground; Z = generator ground (before SinkCm). */
	UPROPERTY(BlueprintReadOnly, Category="WorldFeature")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="WorldFeature")
	float YawDegrees = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="WorldFeature")
	float Scale = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="WorldFeature")
	float FootprintRadiusCm = 0.0f;

	/** Seed for the composition's own randomness (its PCG components get it), so each site looks different and every machine builds the same one. */
	UPROPERTY(BlueprintReadOnly, Category="WorldFeature")
	int32 Seed = 0;

	/** What the site places: one entry (the site itself) for a single feature, ClusterCountMin..Max for a cluster. Location / Scale / Seed above
	 *  are then the cluster's centre, its largest member's scale and its first member's seed; FootprintRadiusCm covers every member. */
	UPROPERTY(BlueprintReadOnly, Category="WorldFeature")
	TArray<FMOWorldFeatureMember> Members;
};
