// Copyright MO57. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOHISMCullingSubsystem.generated.h"

class UPCGComponent;

/**
 * World subsystem that automatically refreshes PCG-spawned HISM components to ensure
 * InstanceMinDrawDistance culling works correctly.
 *
 * To use: Add the tag specified in PCGActorTag (default "FarTreesPCG") to any PCG actor
 * that needs periodic refresh for distance culling to work.
 *
 * Note: This uses editor-only PCG APIs and won't function in packaged builds.
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
