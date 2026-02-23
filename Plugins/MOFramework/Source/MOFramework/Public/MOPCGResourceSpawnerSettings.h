#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOPCGItemSpawnerSettings.h"
#include "MOPCGResourceSpawnerSettings.generated.h"

class UDataTable;
class UStaticMesh;
class UHierarchicalInstancedStaticMeshComponent;

/**
 * Configuration for a single harvestable resource (tree, bush, rock, etc.)
 * Extends FMOPCGItemSpawnEntry with visual and harvest behavior settings.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOPCGResourceEntry
{
	GENERATED_BODY()

	/** Base item spawn entry (ItemId, Weight, Quantity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	FMOPCGItemSpawnEntry ItemEntry;

	/** Static mesh to use for this resource (overrides datatable mesh if set) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	TSoftObjectPtr<UStaticMesh> OverrideMesh;

	/** Scale range for random variation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	FVector MinScale = FVector(0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	FVector MaxScale = FVector(1.1f);

	/** If true, this resource keeps its HISM instance after harvest (yields resources without destroying) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	bool bKeepOnHarvest = false;

	/** Required by FMOWeightedSelector */
	float GetWeight() const { return ItemEntry.Weight; }
};

/**
 * Resource type enum for specialized spawner behavior.
 */
UENUM(BlueprintType)
enum class EMOResourceType : uint8
{
	Generic UMETA(DisplayName = "Generic"),
	Tree UMETA(DisplayName = "Tree"),
	Bush UMETA(DisplayName = "Bush"),
	Rock UMETA(DisplayName = "Rock"),
	Ore UMETA(DisplayName = "Ore Deposit"),
	Plant UMETA(DisplayName = "Plant/Herb")
};

/**
 * Base PCG settings for spawning harvestable resources.
 * Provides common functionality for trees, bushes, rocks, etc.
 *
 * Features:
 * - Weighted random resource selection
 * - Scale randomization per instance
 * - KeepOnHarvest tagging for renewable resources
 * - Automatic tag registration for foraging discovery
 */
UCLASS(BlueprintType, ClassGroup=(MO))
class MOFRAMEWORK_API UMOPCGResourceSpawnerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UMOPCGResourceSpawnerSettings();

	// UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;
	virtual bool UseSeed() const override { return true; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MO Resource Spawner")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOFramework", "MOResourceSpawnerTitle", "MO Resource Spawner"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

public:
	// ============================================================================
	// RESOURCE TYPE
	// ============================================================================

	/** Type of resource being spawned (affects tags and behavior) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resource", meta=(PCG_Overridable))
	EMOResourceType ResourceType = EMOResourceType::Generic;

	// ============================================================================
	// ITEM CONFIGURATION
	// ============================================================================

	/** Item datatable containing FMOItemDefinitionRow entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Items", meta=(PCG_Overridable))
	TObjectPtr<UDataTable> ItemDataTable;

	/** Resources to spawn with their weights and settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Items", meta=(PCG_Overridable))
	TArray<FMOPCGResourceEntry> ResourcesToSpawn;

	/** Random seed offset for selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Items", meta=(PCG_Overridable))
	int32 SeedOffset = 0;

	/** If true, removes points that don't have a valid mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Items", meta=(PCG_Overridable))
	bool bDiscardInvalidPoints = true;

	// ============================================================================
	// MESH SPAWNING
	// ============================================================================

	/** Collision profile for spawned HISM components. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh", meta=(PCG_Overridable))
	FCollisionProfileName CollisionProfile = FCollisionProfileName(TEXT("BlockAll"));

	/** If true, HISM instances will cast shadows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh", meta=(PCG_Overridable))
	bool bCastShadows = true;

	/** If true, apply random rotation on Z axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh", meta=(PCG_Overridable))
	bool bRandomizeRotation = true;

	// ============================================================================
	// TAGGING
	// ============================================================================

	/** Tag prefix for item tags. Default: "MOItem_" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tagging", meta=(PCG_Overridable))
	FString TagPrefix = TEXT("MOItem_");

	/** If true, registers tag mappings with MOPCGInteractionSubsystem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tagging", meta=(PCG_Overridable))
	bool bRegisterWithSubsystem = true;

	/** Additional tags to add to all spawned HISM components. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tagging", meta=(PCG_Overridable))
	TArray<FName> AdditionalTags;
};

/**
 * PCG Element for resource spawning.
 */
class FMOPCGResourceSpawnerElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	struct FResourceSpawnData
	{
		FName ItemId;
		UStaticMesh* Mesh = nullptr;
		bool bKeepOnHarvest = false;
		FVector MinScale = FVector(0.9f);
		FVector MaxScale = FVector(1.1f);
		TArray<FTransform> Transforms;
	};

	TMap<FName, FResourceSpawnData> BuildResourceDataMap(
		const UDataTable* DataTable,
		const TArray<FMOPCGResourceEntry>& Resources) const;

	UHierarchicalInstancedStaticMeshComponent* GetOrCreateHISMComponent(
		AActor* TargetActor,
		UStaticMesh* Mesh,
		const UMOPCGResourceSpawnerSettings* Settings,
		FName ItemId) const;

	FName GetResourceTypeTag(EMOResourceType Type) const;
};

// ============================================================================
// SPECIALIZED RESOURCE SPAWNERS
// ============================================================================

/**
 * Tree spawner with defaults optimized for tree resources.
 */
UCLASS(BlueprintType, ClassGroup=(MO))
class MOFRAMEWORK_API UMOPCGTreeSpawnerSettings : public UMOPCGResourceSpawnerSettings
{
	GENERATED_BODY()

public:
	UMOPCGTreeSpawnerSettings();

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MO Tree Spawner")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOFramework", "MOTreeSpawnerTitle", "MO Tree Spawner"); }
	virtual FText GetNodeTooltipText() const override;
#endif
};

/**
 * Bush spawner with defaults optimized for bush/shrub resources.
 */
UCLASS(BlueprintType, ClassGroup=(MO))
class MOFRAMEWORK_API UMOPCGBushSpawnerSettings : public UMOPCGResourceSpawnerSettings
{
	GENERATED_BODY()

public:
	UMOPCGBushSpawnerSettings();

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MO Bush Spawner")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOFramework", "MOBushSpawnerTitle", "MO Bush Spawner"); }
	virtual FText GetNodeTooltipText() const override;
#endif
};

/**
 * Rock spawner with defaults optimized for rock/mineral resources.
 */
UCLASS(BlueprintType, ClassGroup=(MO))
class MOFRAMEWORK_API UMOPCGRockSpawnerSettings : public UMOPCGResourceSpawnerSettings
{
	GENERATED_BODY()

public:
	UMOPCGRockSpawnerSettings();

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MO Rock Spawner")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOFramework", "MORockSpawnerTitle", "MO Rock Spawner"); }
	virtual FText GetNodeTooltipText() const override;
#endif
};
