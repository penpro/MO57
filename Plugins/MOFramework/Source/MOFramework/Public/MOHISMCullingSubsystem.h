/**
 * =============================================================================
 * MOHISMCullingSubsystem.h - HISM Distance Culling Refresh System
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * World subsystem that periodically refreshes PCG-spawned HISM components to
 * ensure InstanceMinDrawDistance culling works correctly. Required because
 * HISM culling state can become stale after PCG generation.
 *
 * USAGE:
 * 1. Add PCGActorTag (default "FarTreesPCG") to PCG actors needing refresh
 * 2. Set bEnabled = true
 * 3. Configure RefreshInterval (default 10s)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] EDITOR ONLY: Uses PCG APIs that are editor-only. Won't function
 *   in packaged builds. Consider alternative for shipping games.
 *
 * [2024-02] PERFORMANCE: RefreshAllHISMComponents can be expensive with many
 *   instances. Don't refresh too frequently.
 *
 * [2024-02] DISABLED BY DEFAULT: bEnabled = false. Must explicitly enable
 *   in code or Blueprint.
 *
 * =============================================================================
 * RELATED FILES: MOPCGResourceSpawnerSettings.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOHISMCullingSubsystem.generated.h"

class UPCGComponent;

/**
 * World subsystem that automatically refreshes PCG-spawned HISM components.
 * See file header for usage and pitfalls.
 */
UCLASS()
class MOFRAMEWORK_API UMOHISMCullingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// UTickableWorldSubsystem interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bEnabled; }
	virtual bool IsTickableInEditor() const override { return false; }

	/** Enable/disable the automatic refresh system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HISM Culling")
	bool bEnabled = false;

	/**
	 * Tag that PCG actors must have to be refreshed by this subsystem.
	 * Add this tag to your far-trees PCG volume to enable automatic refresh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HISM Culling")
	FName PCGActorTag = FName("FarTreesPCG");

	/** Interval in seconds between periodic refreshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HISM Culling")
	float RefreshInterval = 10.0f;

	/** Manually trigger a refresh of tagged HISM components. */
	UFUNCTION(BlueprintCallable, Category = "HISM Culling")
	void RefreshAllHISMComponents();

private:
	float TimeSinceLastRefresh = 0.0f;
	bool bHasRefreshedOnce = false;

	/** Check if a PCG component's owner has the required tag */
	bool ShouldRefreshPCGComponent(UPCGComponent* PCGComp) const;
};
