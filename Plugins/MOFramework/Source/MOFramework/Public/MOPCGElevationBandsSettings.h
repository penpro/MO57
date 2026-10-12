/**
 * =============================================================================
 * MOPCGElevationBandsSettings.h - PCG Elevation-Based Point Filtering
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * PCG node that splits input points into elevation bands with configurable
 * falloff. Useful for biome distribution based on altitude.
 *
 * FEATURES:
 * - Up to 8 elevation bands with named outputs
 * - Smooth falloff at band boundaries
 * - Density-based probabilistic filtering in transition zones
 * - Sea level reference for height calculations
 *
 * BAND STRUCTURE:
 * Each band defines: MinHeight, MaxHeight, BottomFalloff, TopFalloff
 * Points in falloff zones get density 0-1, used for probabilistic selection.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] BAND OVERLAP: Points CAN appear in multiple bands if falloff
 *   zones overlap. Design bands with gaps if exclusive output needed.
 *
 * [2024-02] SEA LEVEL Z: Heights are relative to SeaLevelZ. Default is 0.
 *   Adjust for your terrain's world-space elevation.
 *
 * [2024-02] MAX 8 BANDS: OutputPinProperties generates pins for bands 0-7.
 *   Adding more bands will have no output.
 *
 * [2026-10] WATER (bExcludeWater, default ON): every spawner chain in the scatter graphs passes through one of these nodes, so it is THE chokepoint that keeps trees, grass, stones and spawn
 *   points out of water -- the sea / rivers / ponds (ground under the sea plane) AND the upland lakes, which sit ABOVE sea level and which the bands alone cannot see (MO.PCG.WaterAudit found
 *   51,374 of 56,937 instances in one lake). The test is FMOPCGWaterMask (MOPCGWaterMask.h). Because the answer depends on the world seed, not only on the input points, the element is neither cacheable
 *   nor off the main thread while the exclusion is on.
 *
 * =============================================================================
 * RELATED FILES: MOPCGDistanceCullingSettings.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOPCGElevationBandsSettings.generated.h"

/**
 * Defines a single elevation band with height range and falloff.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOPCGElevationBand
{
	GENERATED_BODY()

	/** Name for this band (used for output pin labeling). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Elevation")
	FName BandName = NAME_None;

	/** Minimum height of this band (in world units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Elevation")
	float MinHeight = 0.0f;

	/** Maximum height of this band (in world units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Elevation")
	float MaxHeight = 100.0f;

	/** Falloff distance at the bottom of the band. Points within this range fade from 0 to 1 density. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Elevation", meta=(ClampMin="0"))
	float BottomFalloff = 10.0f;

	/** Falloff distance at the top of the band. Points within this range fade from 1 to 0 density. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Elevation", meta=(ClampMin="0"))
	float TopFalloff = 10.0f;

	/**
	 * Calculate the density for a given height within this band.
	 * Returns 0 if outside band, 0-1 in falloff zones, 1 in core zone.
	 */
	float GetDensityForHeight(float Height) const
	{
		if (Height < MinHeight || Height > MaxHeight)
		{
			return 0.0f;
		}

		float Density = 1.0f;

		// Bottom falloff
		if (BottomFalloff > 0.0f && Height < MinHeight + BottomFalloff)
		{
			Density = FMath::Min(Density, (Height - MinHeight) / BottomFalloff);
		}

		// Top falloff
		if (TopFalloff > 0.0f && Height > MaxHeight - TopFalloff)
		{
			Density = FMath::Min(Density, (MaxHeight - Height) / TopFalloff);
		}

		return FMath::Clamp(Density, 0.0f, 1.0f);
	}
};

/**
 * PCG node that splits input points into elevation bands with configurable falloff.
 *
 * Takes input points and outputs up to 8 separate point sets based on Z height.
 * Points are filtered using density-based selection with smooth falloff between bands.
 * Points can appear in multiple bands if their height falls within overlapping falloff zones.
 */
UCLASS(BlueprintType, ClassGroup=(MO))
class MOFRAMEWORK_API UMOPCGElevationBandsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UMOPCGElevationBandsSettings();

	// UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;
	virtual bool UseSeed() const override { return true; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MO Elevation Bands")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOFramework", "MOElevationBandsTitle", "MO Elevation Bands"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

public:
	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Elevation bands configuration. Each band outputs to its corresponding pin (Band0, Band1, etc). Up to 8 bands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable))
	TArray<FMOPCGElevationBand> Bands;

	/** World Z coordinate that represents sea level (height 0). Point heights are calculated relative to this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable))
	float SeaLevelZ = 0.0f;

	/** If true, points in falloff zones use probabilistic selection based on density. If false, all points in range are included. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable))
	bool bUseDensityFiltering = true;

	/** Random seed offset for density filtering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable))
	int32 SeedOffset = 0;

	/**
	 * Drop every input point that stands in water (under the sea plane, or inside an upland lake) before banding. ON by default: nothing in this game is planted in water. A graph that really wants
	 * aquatic scatter (reeds in the shallows) turns it off on ITS elevation node and filters with its own water band.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water", meta=(PCG_Overridable))
	bool bExcludeWater = true;

	/**
	 * Drop every input point that belongs to the cave system: inside a sinkhole's rim zone, or far below the height terrain (a tunnel / cavern surface). ON by default: the cave stamp carves holes the
	 * height terrain does not know about, and a chain that samples that height planted grass and trees across the openings. The test is FMOPCGWaterMask::IsInCave; same reasoning as the water exclusion
	 * (the answer depends on the world seed, so the node is neither cacheable nor off the main thread while it is on).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Caves", meta=(PCG_Overridable))
	bool bExcludeCaves = true;

	/**
	 * Keep only points that stand in one of these biomes (DT_Biomes row ids, resolved the way the biome spawner does: the point's height, slope and the generator's climate). Empty = every biome. This
	 * lets a plain Static Mesh Spawner chain ("black locust trees") belong to the biomes it suits instead of covering the whole world; the node is then not cacheable (the answer depends on the world seed).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome", meta=(PCG_Overridable))
	TArray<FName> AllowedBiomeIds;

	/** How far past an upland lake's shore (1.0 = the shore ellipse) still counts as water. 1.05 = the bank's first metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water", meta=(PCG_Overridable, ClampMin="1.0", ClampMax="2.0", EditCondition="bExcludeWater"))
	float WaterBankRadii = 1.05f;
};

/**
 * PCG Element that executes the elevation bands filtering logic.
 */
class FMOPCGElevationBandsElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
