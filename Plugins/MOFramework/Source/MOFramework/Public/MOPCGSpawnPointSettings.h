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
