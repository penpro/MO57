/**
 * =============================================================================
 * MOAmbientEnvironmentSubsystem.h - What the local player hears, from where they stand
 * =============================================================================
 *
 * PURPOSE:
 * UMOAudioSubsystem already knows layered ambience for Outdoor (day / dusk / night), Water and Cave, but only the clock ever changed it: walk into a cave or onto a shore and the forest kept
 * playing. This driver closes the loop. Every ~0.75 s it asks the world generator (the same pure functions that built the ground) where the local pawn is -- deep under the surface, near
 * the sea / a river / an upland lake, or in the open -- and tells the audio subsystem. Entering is easy, leaving needs a bigger margin (hysteresis) so walking along a shore does not flap.
 *
 * Rules: Indoor (a building) is never overridden; Cave beats Water beats Outdoor; Outdoor means "whatever the clock says" (day / dusk / night), so the clock's own flips keep working
 * (UMOAudioSubsystem only auto-flips while it is in an outdoor state).
 *
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * - Ambient.Cave must exist in DT_AmbientLayers (/MOFramework/Audio/Mix); without it the audio subsystem falls back to a single track or silence.
 * - Water is detected from the generator's height (sea level and the carved river / basin / lake bowls) and IsInUplandLakeZone, not from the water actors: it works before any water body exists.
 * - FLAPPING (found 2026-10-10 in a session log: Water <-> Outdoor_Day once a SECOND while standing still, every flip restarting every ambient layer with a 3 s crossfade = a wall of noise).
 *   The first version sampled rings whose RADIUS depended on the previous state (35 m to enter, 56 m to stay); with only two rings per radius a narrow river or pool can be hit by the 17 m
 *   ring and missed by the 28 m one, so the answer depended on the state it was asked from and never settled. Now ONE measurement (DistanceToWaterCm: fixed 7 m rings, the same for
 *   every caller) is compared against the entry or the exit distance, so a fixed position always settles in one step (test: MOFramework.Audio.AmbientEnvironment, "settles").
 *   On top of that a state change is held for MinDwellSeconds, so even a player walking along a ragged shore cannot restart the layers more than once per few seconds.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOAudioTypes.h"
#include "MOWorldGen.h"
#include "MOAmbientEnvironmentSubsystem.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOAmbientEnvironmentSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * Where the listener stands: Cave (deep under the ground), Water (near the sea / a river / a lake, and not high above it) or Outdoor_Day (= "in the open": the caller maps it to the
	 * clock's day / dusk / night). Previous gives the hysteresis: it is harder to leave a state than to enter it. Pure: tests drive it.
	 */
	static EMOAmbientState Classify(const FMOWorldGenParams& Params, int32 TerrainSeed, const FVector& Where, EMOAmbientState Previous);

	/**
	 * Distance (cm) from (X, Y) to the nearest water (sea / river / pond / lake, by the generator's ground), measured on fixed rings (every 1/8 of MaxCm, 8 directions) so the answer
	 * never depends on who asks; returns a value above MaxCm when there is none within it. OutSurfaceCm = the height of that water's surface.
	 */
	static float DistanceToWaterCm(const FMOWorldGenParams& Params, int32 TerrainSeed, double X, double Y, float MaxCm, float& OutSurfaceCm);

	/** Metres of ground over the listener at which Cave begins (it ends at two thirds of this). */
	static constexpr float CaveDepthCm = 1200.0f;

	/** Distance to water (cm) at which Water begins (it ends at 1.6x this). */
	static constexpr float WaterRadiusCm = 3500.0f;

	/** A state is held at least this long (real seconds) before the next change: the layers crossfade over 3 s and every change restarts them. */
	static constexpr float MinDwellSeconds = 4.0f;

private:
	float TimeSinceEvaluate = 100.0f;
	float TimeSinceChange = 100.0f;
};
