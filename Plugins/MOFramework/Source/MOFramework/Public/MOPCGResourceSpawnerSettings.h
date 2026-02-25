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
 * Select a resource via dropdown, optionally override spawn weight.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOPCGResourceEntry
{
	GENERATED_BODY()

	/**
	 * Reference to a row in DT_ResourceNodes.
	 * Use the dropdown to select the resource type (e.g., "BlackAlder", "IronOre").
	 * All tags, meshes, and yields come from the DataTable definition.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG", meta=(RowType="/Script/MOFramework.MOResourceNodeDefinitionRow"))
	FDataTableRowHandle ResourceNodeRow;

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

	/** Get the resource definition from the DataTable. Returns nullptr if invalid. */
	const FMOResourceNodeDefinitionRow* GetResourceDefinition() const
	{
		if (!ResourceNodeRow.DataTable || ResourceNodeRow.RowName.IsNone())
		{
			return nullptr;
		}
		return ResourceNodeRow.DataTable->FindRow<FMOResourceNodeDefinitionRow>(
			ResourceNodeRow.RowName, TEXT("FMOPCGResourceEntry::GetResourceDefinition"));
	}

	/** Get effective min scale (override if non-zero, else from DataTable). */
	FVector GetEffectiveMinScale() const
	{
		if (!ScaleOverrideMin.IsNearlyZero())
		{
			return ScaleOverrideMin;
		}
		if (const FMOResourceNodeDefinitionRow* Def = GetResourceDefinition())
		{
			return Def->MinScale;
		}
		return FVector(0.9f);
	}

	/** Get effective max scale (override if non-zero, else from DataTable). */
	FVector GetEffectiveMaxScale() const
	{
		if (!ScaleOverrideMax.IsNearlyZero())
		{
			return ScaleOverrideMax;
		}
		if (const FMOResourceNodeDefinitionRow* Def = GetResourceDefinition())
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
	 * Resources to spawn. Each entry references a row in DT_ResourceNodes.
	 * Use the dropdown to select resource types (e.g., "BlackAlder", "IronOre").
	 * All tags, meshes, and yields are automatically read from the DataTable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Resources", meta=(PCG_Overridable, TitleProperty="ResourceNodeRow"))
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
		/** Selected mesh for this instance */
		UStaticMesh* Mesh = nullptr;
		/** Optional material override */
		UMaterialInterface* MaterialOverride = nullptr;
		/** Resource type from DataTable */
		EMOResourceType ResourceType = EMOResourceType::Generic;
		/** Scale range from DataTable or override */
		FVector MinScale = FVector(0.9f);
		FVector MaxScale = FVector(1.1f);
		/** Rotation randomization from DataTable */
		bool bRandomizeRotation = true;
		/** All auto-generated tags from DataTable definition */
		TArray<FName> AllTags;
		/** Accumulated transforms for this resource */
		TArray<FTransform> Transforms;
	};

	/**
	 * Build spawn data map from ResourcesToSpawn entries.
	 * Each entry looks up its DataTable definition and populates FResourceSpawnData.
	 */
	TMap<FName, FResourceSpawnData> BuildResourceDataMap(
		const TArray<FMOPCGResourceEntry>& Resources,
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
