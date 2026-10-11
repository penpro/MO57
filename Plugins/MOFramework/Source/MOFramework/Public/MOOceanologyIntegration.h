/**
 * =============================================================================
 * MOOceanologyIntegration.h - The seam between MO57 and the Oceanology water plugin
 * =============================================================================
 *
 * PURPOSE:
 * Oceanology (Plugins/Marketplace/Oceanology_Plugin, "Oceanology Legacy" for UE 5.8) is the game's water: the infinite ocean placed in the levels, and a lake actor per
 * upland lake (UMOUplandLakeSubsystem). This file is where project-wide facts about how that plugin behaves in THIS engine live, so no level, test or caller has to rediscover them.
 *
 * WHAT IT DOES:
 * UMOOceanologyIntegrationSubsystem looks at every Oceanology water actor in a played world (placed in the level, or spawned later) and applies
 *  - MOOceanology::EnsureUnderwaterVolumeQueryable (once, see the first pitfall), and
 *  - MOOceanology::ApplyUnderwaterLook (twice a second): the project's cap on the underwater screen distortion.
 *
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * - THE UNDERWATER LOOK NEEDS A QUERYABLE VOLUME (found 2026-10-11). The infinite ocean's UnderwaterBoxVolumeComponent has NO collision, and its UnderwaterPostProcessComponent is a BOUNDED post
 *   process volume attached to it. UE 5.8's UPostProcessComponent::EncompassesPoint asks the attach parent's PHYSICS objects for the distance to the view; a box with no collision has none, the
 *   volume never encompasses the camera, and the underwater post process (the deep-blue fog and tint) NEVER APPLIES -- in game the camera sails under the ocean and sees plain air-lit water. The
 *   plugin's own demo map hides this (it places a separate post process volume). A query-only body that ignores every channel is enough: it blocks and overlaps nothing, so gameplay traces and the
 *   character never notice the ocean-wide box. Regression test: MOFramework.Water.UnderwaterVolumeEncompassesCamera.
 * - THE UNDERWATER SCREEN WOBBLE IS CAPPED BY THE PROJECT (2026-10-11, Wes: "really strong and looks pretty bad, make it 1/4"). The plugin's FOceanologyUnderwater has DistortionIntensity and
 *   VignetteDistortion (both 0.05 by default): a noise warp of the whole frame under water. Presets overwrite them whenever one is (re)loaded, so the cap is ENFORCED (ApplyUnderwaterLook re-clamps
 *   within half a second) rather than set once; MO.Water.UnderwaterDistortion is the cap (default 0.0125 = 1/4 of the plugin default; 0 = no wobble, 0.05 = the plugin's look).
 * - THE PLUGIN'S POST PROCESS IS WATERLINE-GATED, so the volume may (and does) extend above the surface: the air is not tinted (checked in game, 2026-10-11).
 * - Oceanology actors replicate by default (AOceanologyWaterParent sets bReplicates). Anything this project spawns per machine from the world seed (the upland lakes) must turn replication
 *   off before FinishSpawning, or the host's copy arrives on every client next to the client's own.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOOceanologyIntegration.generated.h"

class AOceanologyWaterParent;

namespace MOOceanology
{
	/**
	 * Give the water actor's underwater volume a physics body that blocks and overlaps nothing, if it has none (collision off). Idempotent. Returns true when it changed the volume.
	 * See the file header: without a body, the volume's bounded post process never encompasses the camera in UE 5.8.
	 */
	MOFRAMEWORK_API bool EnsureUnderwaterVolumeQueryable(AOceanologyWaterParent& Water);

	/** Does the water actor's underwater volume have collision enabled (so the engine can measure the camera against it)? */
	MOFRAMEWORK_API bool IsUnderwaterVolumeQueryable(const AOceanologyWaterParent& Water);

	/** The project's cap on the underwater screen distortion (MO.Water.UnderwaterDistortion). */
	MOFRAMEWORK_API float GetUnderwaterDistortionCap();

	/**
	 * Clamp the water actor's underwater distortion to the project cap and push the result into its post process material. Returns true when it changed something (a preset had just loaded
	 * the plugin's stronger values, or the cap was changed). Only touches an actor that has begun play (its underwater material exists then).
	 */
	MOFRAMEWORK_API bool ApplyUnderwaterLook(AOceanologyWaterParent& Water);
}

/** Applies the project's Oceanology fixes to every water actor of a played world. */
UCLASS()
class MOFRAMEWORK_API UMOOceanologyIntegrationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Real seconds between looks at the water actors. */
	UPROPERTY(EditAnywhere, Category = "MO|Water", meta = (ClampMin = "0.1"))
	float TuneIntervalSeconds = 0.5f;

private:
	void HandleActorSpawned(AActor* Actor);

	FDelegateHandle ActorSpawnedHandle;
	float TimeSinceTune = 0.0f;
};
