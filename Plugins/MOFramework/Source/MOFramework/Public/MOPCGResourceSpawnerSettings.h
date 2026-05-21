/**
 * =============================================================================
 * MOPCGResourceSpawnerSettings.h - PCG Resource Node Spawner
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * PCG (Procedural Content Generation) settings for spawning harvestable resources
 * like trees, bushes, and rocks. Creates HISM components with auto-generated tags
 * for interaction system integration.
 *
 * DATATABLE-DRIVEN ARCHITECTURE:
 * - Resources defined in DT_ResourceNodes (FMOResourceNodeDefinitionRow)
 * - ResourcesToSpawn entries select rows via FName dropdown
 * - All meshes, scales, and yields come from DataTable definitions
 * - AdditionalTags combined with DataTable tags at spawn time
 *
 * KEY STRUCTS:
 * - FMOPCGResourceEntry: Selects a resource row + optional scale override
 * - FResourceSpawnData (private): Runtime data populated from DataTable
 *
 * EXECUTION FLOW:
 * 1. CreateElement() returns FMOPCGResourceSpawnerElement
 * 2. ExecuteInternal() builds FResourceSpawnData from DataTable
 * 3. Per-point: Select weighted resource, randomize scale/rotation
 * 4. GetOrCreateManagedISMC() creates tagged HISM component
 * 5. If bRegisterWithSubsystem: Register tags with MOPCGInteractionSubsystem
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] DATATABLE NULL: GetResourceDefinition() returns nullptr if row
 *   not found. Always check return value before using.
 *
 * [2024-02] SCALE OVERRIDE ZERO: Zero vector means "use DataTable value".
 *   Use FVector::One() if you want scale 1.0. IsNearlyZero() threshold is
 *   KINDA_SMALL_NUMBER (1e-4).
 *
 * [2024-02] SEED DETERMINISM: UseSeed()=true, SeedOffset affects random stream.
 *   Same inputs produce same spawns. Change SeedOffset for variation.
 *
 * [2024-02] MAIN THREAD ONLY: CanExecuteOnlyOnMainThread()=true. Don't expect
 *   async execution.
 *
 * [2024-02] NOT CACHEABLE: IsCacheable()=false. PCG will re-execute on changes.
 *
 * [2024-02] TAG REGISTRATION: bRegisterWithSubsystem registers with
 *   MOPCGInteractionSubsystem. Required for interaction/harvest system to
 *   find spawned resources.
 *
 * =============================================================================
 * RELATED FILES:
 * - MOResourceNodeDefinitionRow.h - FMOResourceNodeDefinitionRow struct
 * - MOPCGInteractionSubsystem.h - Tag registry for interactions
 * - MOHarvestSubsystem.h - Uses tags to identify harvestable resources
 * - DT_ResourceNodes.uasset - DataTable with resource definitions
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOResourceNodeDefinitionRow.h"
#include "MOPCGResourceSpawnerSettings.generated.h"

class UDataTable;
class UStaticMesh;
class UInstancedStaticMeshComponent;
class UPCGComponent;

/**
 * Entry for spawning a resource from the ResourceNode DataTable.
 * Select a resource via row name dropdown, optionally override spawn weight.
 * The DataTable is specified once at the spawner settings level.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOPCGResourceEntry
{
	GENERATED_BODY()

	/**
	 * Row name in the shared ResourceNodeDataTable.
	 * Use the dropdown to select the resource type (e.g., "BlackAlder", "IronOre").
	 * All tags, meshes, and yields come from the DataTable definition.
	 *
	 * NOTE: The dropdown is populated from the ResourceNodeDataTable on the parent settings.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG", meta=(GetOptions="GetResourceRowNames"))
	FName ResourceRowName = NAME_None;

	/**
	 * Spawn weight for random selection among entries (higher = more common).
	 * This allows different spawn ratios even if resources have equal weights in their definitions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG", meta=(ClampMin="0.01"))
	float SpawnWeight = 1.0f;

	/** Override scale min (if zero vector, uses DataTable value). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG|Override", AdvancedDisplay)
	FVector ScaleOverrideMin = FVector::ZeroVector;

	/** Override scale max (if zero vector, uses DataTable value). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG|Override", AdvancedDisplay)
	FVector ScaleOverrideMax = FVector::ZeroVector;

	/** Required by FMOWeightedSelector */
	float GetWeight() const { return SpawnWeight; }

	/** Get the resource definition from the provided DataTable. Returns nullptr if invalid. */
	const FMOResourceNodeDefinitionRow* GetResourceDefinition(const UDataTable* DataTable) const
	{
		if (!DataTable || ResourceRowName.IsNone())
		{
			return nullptr;
		}
		return DataTable->FindRow<FMOResourceNodeDefinitionRow>(
			ResourceRowName, TEXT("FMOPCGResourceEntry::GetResourceDefinition"));
	}

	/** Get effective min scale (override if non-zero, else from DataTable). */
	FVector GetEffectiveMinScale(const UDataTable* DataTable) const
	{
		if (!ScaleOverrideMin.IsNearlyZero())
		{
			return ScaleOverrideMin;
		}
		if (const FMOResourceNodeDefinitionRow* Def = GetResourceDefinition(DataTable))
		{
			return Def->MinScale;
		}
		return FVector(0.9f);
	}

	/** Get effective max scale (override if non-zero, else from DataTable). */
	FVector GetEffectiveMaxScale(const UDataTable* DataTable) const
	{
		if (!ScaleOverrideMax.IsNearlyZero())
		{
			return ScaleOverrideMax;
		}
		if (const FMOResourceNodeDefinitionRow* Def = GetResourceDefinition(DataTable))
		{
			return Def->MaxScale;
		}
		return FVector(1.1f);
	}
};

/**
 * Base PCG settings for spawning harvestable resources.
 * Provides common functionality for trees, bushes, rocks, etc.
 *
 * DataTable-Driven Architecture:
 * - Resources are defined in DT_ResourceNodes (FMOResourceNodeDefinitionRow)
 * - ResourcesToSpawn entries select resources via dropdown
 * - All tags, meshes, and yields come from the DataTable
 * - Node-level AdditionalTags are combined with DataTable tags
 *
 * Features:
 * - Weighted random resource selection
 * - Scale randomization per instance
 * - KeepOnHarvest tagging for renewable resources
 * - Automatic tag generation from DataTable definitions
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

	/** Returns list of row names from ResourceNodeDataTable for dropdown menus. */
	UFUNCTION()
	TArray<FName> GetResourceRowNames() const;

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
	// RESOURCE CONFIGURATION
	// ============================================================================

	/**
	 * DataTable containing resource node definitions (FMOResourceNodeDefinitionRow).
	 * All ResourcesToSpawn entries reference rows from this table.
	 * Set once here to avoid redundant DataTable references per entry.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resources", meta=(RowType="/Script/MOFramework.MOResourceNodeDefinitionRow"))
	TObjectPtr<UDataTable> ResourceNodeDataTable;

	/**
	 * Resources to spawn. Each entry references a row name in ResourceNodeDataTable.
	 * Use the dropdown to select resource types (e.g., "BlackAlder", "IronOre").
	 * All tags, meshes, and yields are automatically read from the DataTable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resources", meta=(PCG_Overridable, TitleProperty="ResourceRowName"))
	TArray<FMOPCGResourceEntry> ResourcesToSpawn;

	/** Random seed offset for selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resources", meta=(PCG_Overridable))
	int32 SeedOffset = 0;

	/** If true, removes points that don't have a valid mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resources", meta=(PCG_Overridable))
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

	// ============================================================================
	// TAGGING
	// ============================================================================

	/** If true, registers tag mappings with MOPCGInteractionSubsystem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tagging", meta=(PCG_Overridable))
	bool bRegisterWithSubsystem = true;

	/**
	 * Additional tags to add to ALL spawned resources from this node.
	 * Combined with tags from each resource's DataTable definition.
	 * Useful for location-specific tags like "ForestZone" or "MiningArea".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tagging", meta=(PCG_Overridable))
	TArray<FName> AdditionalTags;

	// ============================================================================
	// TERRAIN MODIFICATION
	// ============================================================================

	/**
	 * If true, skip spawning at any point whose world location is inside a
	 * modified terrain zone (UMOTerrainModificationSubsystem). PCG-respawn
	 * flicker doesn't happen because nothing spawns there in the first place.
	 *
	 * Set false for resource nodes that should ignore terrain modifications
	 * (e.g. infinite ore veins meant to spawn even on worked ground).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TerrainModification", meta=(PCG_Overridable))
	bool bRespectTerrainModifications = true;
};

/**
 * PCG Element for resource spawning.
 * Reads resource definitions from DataTable and spawns HISM components with auto-generated tags.
 */
class FMOPCGResourceSpawnerElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	/**
	 * Runtime data for spawning a specific resource.
	 * Populated from DataTable definitions at execution time.
	 */
	struct FResourceSpawnData
	{
		/** Row name from the ResourceNode DataTable */
		FName ResourceNodeId;
		/** Display name from DataTable */
		FText DisplayName;
		/** Resource type from DataTable */
		EMOResourceType ResourceType = EMOResourceType::Generic;
		/** Scale range from DataTable or override */
		FVector MinScale = FVector(0.9f);
		FVector MaxScale = FVector(1.1f);
		/** Rotation randomization from DataTable */
		bool bRandomizeRotation = true;
		/** All auto-generated tags from DataTable definition */
		TArray<FName> AllTags;
		/** Pointer to the definition row for mesh variation selection */
		const FMOResourceNodeDefinitionRow* Definition = nullptr;
	};

	/**
	 * Per-mesh spawn data for accumulating transforms.
	 * Each unique (ResourceRowName, Mesh) pair gets its own entry.
	 */
	struct FMeshSpawnData
	{
		/** The mesh to spawn */
		UStaticMesh* Mesh = nullptr;
		/** Optional material override */
		UMaterialInterface* MaterialOverride = nullptr;
		/** Resource node ID for tagging */
		FName ResourceNodeId;
		/** All tags from the resource definition */
		TArray<FName> AllTags;
		/** Accumulated transforms for this mesh */
		TArray<FTransform> Transforms;
	};

	/**
	 * Build spawn data map from ResourcesToSpawn entries.
	 * Each entry looks up its DataTable definition and populates FResourceSpawnData.
	 */
	TMap<FName, FResourceSpawnData> BuildResourceDataMap(
		const TArray<FMOPCGResourceEntry>& Resources,
		const UDataTable* DataTable,
		FRandomStream& RandomStream) const;

	/**
	 * Get or create a managed HISM component for spawning instances.
	 */
	UInstancedStaticMeshComponent* GetOrCreateManagedISMC(
		FPCGContext* Context,
		AActor* TargetActor,
		UStaticMesh* Mesh,
		const UMOPCGResourceSpawnerSettings* Settings,
		FName ResourceNodeId,
		const TArray<FName>& ComponentTags) const;
};

// ============================================================================
// SPECIALIZED RESOURCE SPAWNERS
// ============================================================================

/**
 * Tree spawner - convenience class with tree-appropriate defaults.
 * All yields and tags are now defined in the DataTable (DT_ResourceNodes).
 * This class primarily exists for node name/tooltip clarity in the PCG graph.
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
 * Bush spawner - convenience class with bush-appropriate defaults.
 * All yields and tags are now defined in the DataTable (DT_ResourceNodes).
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
 * Rock spawner - convenience class with rock-appropriate defaults.
 * All yields and tags are now defined in the DataTable (DT_ResourceNodes).
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
