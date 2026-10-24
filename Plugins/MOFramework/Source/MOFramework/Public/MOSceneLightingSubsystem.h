// How the sky lights a scene beyond what the sky actor itself does (Docs/Light_Sources.md).
//
// Under a closed canopy Lumen sees the crowns as a roof: the sky light cannot reach the forest floor and the little bounce from sunlit gaps leaves the
// understory near black (Wes, Oct 23 2026, the x3 rainforest: "now it's BLACK in the rain forest, this also makes me wonder if lumen is working properly
// ... or we need to adjust HDR post processing"). Measured: Lumen works (off, the same view went from 88 to 108 mean brightness -- the sky light then
// reached the shade unshadowed); Ultra Dynamic Sky's interior adjustment only trimmed the sky light by a fifth there (0.89 -> 0.71), it does not take the
// forest for a cave. Lumen's own answer is Skylight Leaking: a share of the sky light reaches occluded places anyway, the light that filters through
// leaves and bounces between them. In a cave the sky light is already zeroed by the interior adjustment, so the leak there is zero too.
//
// This subsystem puts one unbound post process component in every game world with that leak (mo.Light.SkylightLeaking, live), lower in priority
// than anything a level, the water or a camera sets. It also gives the sky actor's interior adjustment a floor (mo.Light.InteriorSkyLight): where
// its occlusion test reads enclosed, the sky light dims to that share instead of off (Oct 24: in a dense forest the test flipped, and the light
// switched on and off as the player walked). And it sets the ocean's shoreline wetness (mo.Water.ShoreWet*): the preset's was metallic.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOSceneLightingSubsystem.generated.h"

class UPostProcessComponent;

UCLASS()
class MOFRAMEWORK_API UMOSceneLightingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** The leak in force (the cvar), applied to this world's component; the sky actor's interior floor. */
	void Apply();

	/** The ocean's shoreline wetness from mo.Water.ShoreWet* (not metallic, a narrow band): set after the ocean has loaded its preset. */
	void ApplyShoreWetness();

private:
	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> SkyLeak;

	FDelegateHandle CVarChangedHandle;
	FDelegateHandle InteriorChangedHandle;
	TArray<FDelegateHandle> WetChangedHandles;
};
