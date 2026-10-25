/**
 * =============================================================================
 * MOStartSiteTypes.h - the start site: what a new world's first survivor wakes beside
 * =============================================================================
 *
 * Wes, Oct 24 2026: "the egg should be near where the first player starts". The survivor grew in a stone clone egg that came down from the
 * sky (Art/References/ImageTo3D/2026-10-18-clone-egg/README.md): the egg and its thrown-off hatch lie a few metres from where they land.
 * UMOStartSiteSubsystem places the props once and saves them; this header holds the data and the placement maths (plain functions, so the
 * maths is testable without a world: MOFramework.StartSite.*).
 *
 * THE SITE FRAME: origin at the site, +Y pointing from the site to the survivor, +X across. A prop's mesh +Y therefore faces the survivor --
 * the clone egg's opening is on its mesh +Y side (measured in PIE, Oct 24), so the survivor looks into the egg they came out of.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOStartSiteTypes.generated.h"

class UStaticMesh;

/** One prop of the start site, placed in the site frame. */
USTRUCT()
struct MOFRAMEWORK_API FMOStartSitePropSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Start Site")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Where it lies in the site frame (cm): +Y toward the survivor. */
	UPROPERTY(EditAnywhere, Category="Start Site")
	FVector2D OffsetCm = FVector2D::ZeroVector;

	/** Its turn from the site frame (degrees). */
	UPROPERTY(EditAnywhere, Category="Start Site")
	float YawDeg = 0.0f;

	/** How far it is pressed below the LOWEST ground under its footprint (cm): it came down hard, it does not perch. */
	UPROPERTY(EditAnywhere, Category="Start Site")
	float SinkCm = 0.0f;
};

/** A prop as placed: the save record, and what a load spawns again. */
USTRUCT()
struct MOFRAMEWORK_API FMOStartSitePropRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FSoftObjectPath Mesh;

	UPROPERTY()
	FTransform Transform;
};

/** Save payload (held in UMOWorldSaveGame). A world saved before Oct 24 2026 has none, and so no egg. */
USTRUCT()
struct MOFRAMEWORK_API FMOStartSiteSaveData
{
	GENERATED_BODY()

	/** The site was placed (it never is twice). */
	UPROPERTY()
	bool bPlaced = false;

	UPROPERTY()
	TArray<FMOStartSitePropRecord> Props;
};

namespace MOStartSite
{
	/** The site frame's yaw (degrees): its +Y axis points from Site to Survivor. */
	MOFRAMEWORK_API float FrameYawDeg(const FVector2D& Site, const FVector2D& Survivor);

	/** A site-frame offset as a world XY point. */
	MOFRAMEWORK_API FVector2D ToWorld(const FVector2D& Site, float FrameYawDeg, const FVector2D& OffsetCm);

	/**
	 * Where the site may go, best first: DistanceCm from the survivor at BearingDeg from their facing, then the bearings 30 degrees either side,
	 * 60, ... all the way round, then the same ring half as far again (24 candidates). The first that fits wins.
	 */
	MOFRAMEWORK_API void Candidates(const FVector2D& Survivor, float FacingYawDeg, float BearingDeg, float DistanceCm, TArray<FVector2D>& Out);
}
