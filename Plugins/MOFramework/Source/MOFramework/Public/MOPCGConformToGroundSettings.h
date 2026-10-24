/**
 * =============================================================================
 * MOPCGConformToGroundSettings.h - PCG node: a composition stands ON our terrain
 * =============================================================================
 *
 * A world-feature composition (an Electric Dreams rock spire, an embankment set) was authored on someone else's ground: Electric Dreams' rock
 * assemblies put their floor ~2.6 m ABOVE the assembly root, so a 4x outcrop floated 7-11 m over our terrain. After the points are copied onto
 * the actor (world space), this node:
 *   1. pushes the BASE pieces outward from the centre (BaseSpread; Wes, Oct 14 2026: "the smaller base cliffs need spread out a little more").
 *      Base = pivots within FollowBelowCm of the actor root, in the actor's own (unscaled) units, fading to rigid over FadeCm;
 *   2. measures, for every full-weight base piece, how far its mesh's lowest point is from the generator ground under it (minus EmbedCm x scale);
 *   3. moves the WHOLE composition by the median of those corrections (the stacked column stays attached to its foot), then settles each base
 *      piece the rest of its own way onto the ground (a slope, a hummock).
 * Heights come from FMOWorldGen (the function the voxel terrain is built from): no collision needed, so it works while the terrain under a far
 * composition has not streamed in. Without a published generator / world seed (an editor preview) it only spreads.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOPCGConformToGroundSettings.generated.h"

UCLASS(BlueprintType, ClassGroup=(MO))
class MOFRAMEWORK_API UMOPCGConformToGroundSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MOConformToGround")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOFramework", "MOConformToGroundTitle", "MO Conform To Ground"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("MOFramework", "MOConformToGroundTip", "The composition stands on the generator's ground: its base pieces spread, settle and embed; the pieces stacked above move with them."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

	/** Pieces whose pivot is at most this high above the composition root (actor units, before its scale) are the base. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable, ClampMin="0", Units="cm"))
	float FollowBelowCm = 400.0f;

	/** Above FollowBelowCm the base behaviour fades out over this height (actor units): 0 = a hard cut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable, ClampMin="0", Units="cm"))
	float FadeCm = 100.0f;

	/** Base pieces move this many times farther from the centre (1 = where they were authored). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable, ClampMin="0.1", ClampMax="4"))
	float BaseSpread = 1.0f;

	/** Stand the composition on the generator ground (off: spread only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable))
	bool bFollowGround = true;

	/** How deep a base piece's lowest point goes into the ground (actor units: it grows with the composition's scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable, ClampMin="0", Units="cm"))
	float EmbedCm = 50.0f;

	/** The point attribute holding each piece's static mesh (its bounds give the piece's lowest point). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable))
	FName MeshAttribute = TEXT("Mesh");

	/** The weight of a point at LocalHeightCm above the root: 1 inside the base band, 0 above the fade. Pure (tests use it). */
	static float BaseWeight(float LocalHeightCm, float FollowBelowCm, float FadeCm);

	/** The lowest world Z of a local box placed by Transform (its 8 corners). Pure. */
	static double LowestZ(const FBox& LocalBox, const FTransform& Transform);

	/** The lowest ground under a piece: GroundZ at its pivot and under the four corners of its box's footprint. On a slope the downhill side of a
	 *  wide base reaches lower ground than its pivot stands on; resting it there keeps its bottom out of sight (Wes's vote, Oct 23 2026: "an
	 *  'average slope' over the area it's being pushed down into ... so the lower side of the slope doesn't expose the bottom"). Pure. */
	static double LowestGroundUnder(const FBox& LocalBox, const FTransform& Transform, const TFunctionRef<double(double X, double Y)>& GroundZ);

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return Super::DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
};

class FMOPCGConformToGroundElement : public IPCGElement
{
public:
	// Reads the target actor, the world seed subsystem and mesh assets: game thread.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
};
