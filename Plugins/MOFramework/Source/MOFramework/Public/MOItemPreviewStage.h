/**
 * MOItemPreviewStage.h - an offscreen "photo booth" that renders one item's world mesh into a render target for the UI.
 *
 * PURPOSE:
 * The inventory details panel (and any other UI) shows a turning 3D view of the selected item: the same mesh, row scale and
 * material override the dropped item uses (FMOItemWorldVisual), so the preview IS the item. UMOItemPreviewWidget owns one stage
 * per widget and draws its render target.
 *
 * HOW IT STAYS OUT OF THE GAME:
 * - placed high above the world (StageLocation, 20 km up): nothing of the world is near it;
 * - the capture renders ONLY the preview mesh (PRM_UseShowOnlyList) with fog / atmosphere / clouds off, so the background is
 *   black and the world never appears in it;
 * - its own key / fill / rim point lights and the mesh use lighting channel 1 only: the sun, the time of day and the world's
 *   lights do not change how the item looks, and the stage lights touch nothing in the world;
 * - the capture runs only when the widget asks (CaptureNow, from its tick while painted), never every frame on its own;
 * - Lumen GI and reflections are off in the capture's post-process (a turntable needs neither and they cost).
 *
 * KNOWN PITFALLS:
 * [2026-10] A mesh with no simple collision is fine here: the preview mesh has collision disabled.
 * [2026-10] Light intensities scale with the framed radius squared so a 2 cm berry and a 2 m spear are lit alike.
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOItemPreviewStage.generated.h"

class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UPointLightComponent;
class UTextureRenderTarget2D;

UCLASS(NotPlaceable, Transient)
class MOFRAMEWORK_API AMOItemPreviewStage : public AActor
{
	GENERATED_BODY()

public:
	AMOItemPreviewStage();

	/** Where stages are spawned: far above the playable world. */
	static FVector StageLocation(int32 Slot);

	/** Show an item (its WorldVisual mesh, scale and material override). Returns false when the item has no mesh. */
	bool SetItem(FName ItemId);
	void ClearItem();

	/** Turn the item: yaw around the vertical, pitch tilts the camera (clamped to -60..30 degrees). */
	void SetView(float YawDeg, float PitchDeg);

	/** Render one frame into the target (call while the preview is on screen). */
	void CaptureNow();

	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }
	void EnsureRenderTarget(int32 Resolution);
	bool HasItem() const;

	/** The camera pitch that shows the current item best (set by SetItem from its shape): a pole is stood upright and seen nearly level, a flat item from well above. */
	float GetSuggestedPitch() const { return SuggestedPitchDeg; }

private:
	void FrameCamera();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	/** Rotates under SetView; the mesh is offset inside it so the item turns about its own centre. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PreviewMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> RimLight;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	float FramedRadius = 50.0f;
	float PitchDeg = -20.0f;
	float SuggestedPitchDeg = -25.0f;
};
