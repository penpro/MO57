/**
 * =============================================================================
 * MOSubterraneanLightingSubsystem.h - no sunlight from beneath the world
 * =============================================================================
 *
 * PURPOSE:
 * A directional light that shines UP (the sun or moon while it is below the horizon) must not light anything: the world is in the way. This subsystem
 * attenuates every directional light in a played world by how far below the horizon it points -- full strength at the horizon, zero from 5 degrees below -- so a
 * below-horizon light never leaks through the terrain and lights the walls and ceilings of caves from underneath.
 *
 * WHY A GATE AND NOT "BETTER SHADOWS" (the investigation, 2026-10, so nobody repeats it):
 *   Measured with MO.Light.Test (a 20 000 lux light, pitch 40 = from below) standing in a sinkhole chamber:
 *     - light from ABOVE (pitch -50): the chamber stays dark, only blue sky bounce. Terrain casts shadows correctly.
 *     - light from BELOW: the whole chamber is lit like direct light, with shadows ON or OFF identically.
 *   None of these changed it: UVoxelMegaMaterial::bGenerateTwoSidedMaterial (the generated Nanite material really was two-sided), VoxelWorld
 *   ComponentSettings.bCastShadowAsTwoSided, non-Nanite voxel meshes, disabling virtual shadow maps, and a 180 km shadow-only quad facing down 200 m under the lowest
 *   cave. The engine's Nanite shadow raster only reads the MATERIAL's two-sidedness, a far quad falls outside the fine virtual-shadow-map clipmaps' depth range, and the
 *   one-sided voxel surface has no bottom to be a front face for a light below. Rather than fight the shadow renderer, the light is simply not allowed to point up
 *   at full strength. The sky already fades the sun to zero around the horizon, so this removes a leak and costs no visible effect.
 *
 * HOW: it never owns a light's intensity. Each check it compares the intensity it last wrote with the light's current one; a difference means the sky (Ultra
 * Dynamic Sky) wrote a new value, which becomes the "intended" intensity. It then writes intended x attenuation. When the light points down again it writes the
 * intended value back. The sky can update at any time and nothing is lost.
 *
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * [2026-10] ULightComponent::bAffectsWorld looks like the switch but "cannot be changed at runtime" (it is read when the render state is created): do not use it.
 *   SetVisibility is also overwritten by the sky blueprint. Intensity is the one knob that works, hence the intended/last-written bookkeeping.
 * [2026-10] CVar MO.Light.BlockFromBelow 0 turns it off (to A/B the lighting, and to run MO.Light.Test from below). Deinitialize restores every light.
 * [2026-10] Checked at 10 Hz: the sun moves a degree per ~4 game minutes, so a tenth of a second is far finer than needed.
 * [2026-10] Directional lights only. A point/spot light under the ground (a torch in a cave) is a gameplay light and is untouched.
 *
 * RELATED FILES: MOCheatSubsystem.cpp (MO.Light.Test, MO.Light.Cube, MO.Cam.LookAt), Docs/Terrain_Generation_Exploration.md ("Caves and light")
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOSubterraneanLightingSubsystem.generated.h"

class UDirectionalLightComponent;

UCLASS()
class MOFRAMEWORK_API UMOSubterraneanLightingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * How much of its intended intensity a directional light shining along LightForward is allowed: 1 at or above the horizon, falling linearly to 0 at
	 * 5 degrees below it.
	 */
	static float AllowedFraction(const FVector& LightForward);

private:
	float TimeSinceCheck = 0.0f;

	struct FLightState
	{
		float Intended = 0.0f;   // what the sky last asked for
		float LastWritten = 0.0f;   // what we last set (so a different current value means the sky wrote)
	};
	TMap<TWeakObjectPtr<UDirectionalLightComponent>, FLightState> Tracked;

	void Update();
};
