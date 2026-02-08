#pragma once

#include "CoreMinimal.h"
#include "MOWaterActorBase.h"
#include "MOInfiniteOceanActor.generated.h"

/**
 * Infinite ocean that always sits at Z=0 and follows the camera.
 * The mesh is repositioned each frame to stay centered on the player.
 */
UCLASS(Blueprintable)
class MOFRAMEWORK_API AMOInfiniteOceanActor : public AMOWaterActorBase
{
	GENERATED_BODY()

public:
	AMOInfiniteOceanActor();

	virtual void Tick(float DeltaTime) override;

	// ============================================================================
	// OCEAN CONFIGURATION
	// ============================================================================

	/** Size of the ocean mesh in each direction (total size = 2x this).
	 *  Keep this reasonable - mesh density = (Extent*2)/Resolution per cell.
	 *  Wavelengths should be ~4-8x cell size for smooth waves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Ocean", meta=(ClampMin="1000.0"))
	float OceanExtent = 8000.0f;

	/** The ocean is always at this Z height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Ocean")
	float OceanLevel = 0.0f;

	/** How often to update the ocean position to follow camera (in cm).
	 *  Smaller = smoother but more mesh regeneration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Ocean", meta=(ClampMin="100.0"))
	float SnapGridSize = 1000.0f;

	/** Whether to snap the ocean mesh to follow the player camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Ocean")
	bool bFollowCamera = true;

protected:
	virtual float GetBaseWaterLevel() const override { return OceanLevel; }
	virtual bool IsInWaterBounds(const FVector2D& WorldXY) const override { return true; } // Infinite
	virtual FVector2D GetMeshSize() const override { return FVector2D(OceanExtent * 2.0f, OceanExtent * 2.0f); }

private:
	/** Last snapped position of the ocean mesh. */
	FVector2D LastSnappedPosition;

	/** Update ocean position to follow camera. */
	void UpdateOceanPosition();

	/** Get the current camera/player position for following. */
	FVector2D GetFollowPosition() const;
};
