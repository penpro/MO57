/**
 * =============================================================================
 * MOPCGSpawnPointSettings.h - PCG Node for Spawn Point Placement
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * PCG Settings for placing spawn point actors at input point locations.
 * These spawn points are used by MOSpawnManagerSubsystem to spawn mobs/survivors.
 *
 * Creates AMOSpawnPoint actors (or subclass) at each input point with
 * configurable category, radius, cooldown, and selection weight.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] MAIN THREAD: Element runs on main thread only for actor spawning.
 *
 * [2024-02] NOT CACHEABLE: IsCacheable returns false. Spawn points must be
 *   re-placed each PCG graph execution.
 *
 * [2024-02] CLASS REQUIREMENT: SpawnPointClass must be AMOSpawnPoint or
 *   subclass. Null class will fail silently.
 *
 * =============================================================================
 * RELATED FILES: MOSpawnPoint.h, MOSpawnTypes.h, MOSpawnManagerSubsystem.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOSpawnTypes.h"
#include "MOPCGSpawnPointSettings.generated.h"

class AMOSpawnPoint;

/**
 * PCG Settings for placing spawn point actors at input point locations.
 * These spawn points are used by the MOSpawnManagerSubsystem to spawn mobs/survivors.
 */
UCLASS(BlueprintType, ClassGroup=(MO))
class MOFRAMEWORK_API UMOPCGSpawnPointSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UMOPCGSpawnPointSettings();

	// UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;
	virtual bool UseSeed() const override { return true; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MO Spawn Point Placer")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOFramework", "MOSpawnPointPlacerTitle", "MO Spawn Point Placer"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

public:
	// ============================================================================
	// SPAWN POINT CONFIGURATION
	// ============================================================================

	/** Class to spawn (must be AMOSpawnPoint or subclass) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point", meta = (PCG_Overridable))
	TSubclassOf<AMOSpawnPoint> SpawnPointClass;

	/** Category of entities this spawn point supports */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point", meta = (PCG_Overridable))
	EMOSpawnCategory SpawnCategory = EMOSpawnCategory::Prey;

	/** Radius within which entities can spawn around this point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point", meta = (PCG_Overridable, ClampMin = "0"))
	float SpawnRadius = 50.0f;

	/** Cooldown before this point can spawn again (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point", meta = (PCG_Overridable, ClampMin = "0"))
	float PointCooldownSeconds = 300.0f;

	/** Selection weight for this point (higher = more likely to be chosen) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point", meta = (PCG_Overridable, ClampMin = "0.1"))
	float SelectionWeight = 1.0f;
};

/**
 * PCG Element for spawn point placement.
 */
class FMOPCGSpawnPointElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
