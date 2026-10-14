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

	/** Max height difference (cm) between the centre and 8 points on the footprint's edge: compositions built on flat ground stay off bumps (0 = no check). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.0"))
	float MaxFootprintReliefCm = 0.0f;

	/** Buried this deep (cm): a composition authored on a flat floor shows no gap on uneven ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.0"))
	float SinkCm = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.1"))
	float MinScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.1"))
	float MaxScale = 1.0f;

	/** Spawned while a player is within this distance (cm), removed past 1.25x of it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Streaming", meta=(ClampMin="5000.0"))
	float SpawnDistanceCm = 40000.0f;
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
};
