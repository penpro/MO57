#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOPCGItemSpawnerSettings.h"
#include "MOPCGResourceSpawnerSettings.generated.h"

class UDataTable;
class UStaticMesh;
class UInstancedStaticMeshComponent;
class UPCGComponent;

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

	UInstancedStaticMeshComponent* GetOrCreateManagedISMC(
		FPCGContext* Context,
		AActor* TargetActor,
		UStaticMesh* Mesh,
		const UMOPCGResourceSpawnerSettings* Settings,
		FName ItemId,
		const TArray<FName>& ComponentTags) const;

	FName GetResourceTypeTag(EMOResourceType Type) const;
};

// ============================================================================
// SPECIALIZED RESOURCE SPAWNERS
// ============================================================================

/**
 * Tree spawner with defaults optimized for tree resources.
 * Includes tree-specific yield options that apply to ALL trees spawned by this node.
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

	// ============================================================================
	// TREE YIELDS (applies to ALL trees spawned by this node)
	// ============================================================================

	/** If true, all trees yield bark when gathered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree Yields", meta=(PCG_Overridable))
	bool bYieldsBark = true;

	/** Item ID for bark yield. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree Yields", meta=(PCG_Overridable, EditCondition="bYieldsBark"))
	FName BarkItemId = FName("Bark");

	/** If true, all trees yield sticks when gathered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree Yields", meta=(PCG_Overridable))
	bool bYieldsSticks = true;

	/** Item ID for sticks yield. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree Yields", meta=(PCG_Overridable, EditCondition="bYieldsSticks"))
	FName SticksItemId = FName("Stick");

	/** If true, all trees yield wood when chopped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree Yields", meta=(PCG_Overridable))
	bool bYieldsWood = false;

	/** Item ID for wood yield. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree Yields", meta=(PCG_Overridable, EditCondition="bYieldsWood"))
	FName WoodItemId = FName("Wood");

	/** If true, all trees yield leaves/foliage when gathered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree Yields", meta=(PCG_Overridable))
	bool bYieldsLeaves = false;

	/** Item ID for leaves yield. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tree Yields", meta=(PCG_Overridable, EditCondition="bYieldsLeaves"))
	FName LeavesItemId = FName("Leaves");

	/** Get all yield item tags for this tree spawner. */
	TArray<FName> GetYieldTags() const;
};

/**
 * Bush spawner with defaults optimized for bush/shrub resources.
 * Includes bush-specific yield options that apply to ALL bushes spawned by this node.
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

	// ============================================================================
	// BUSH YIELDS (applies to ALL bushes spawned by this node)
	// ============================================================================

	/** If true, all bushes yield berries when gathered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bush Yields", meta=(PCG_Overridable))
	bool bYieldsBerries = false;

	/** Item ID for berry yield. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bush Yields", meta=(PCG_Overridable, EditCondition="bYieldsBerries"))
	FName BerriesItemId = FName("Berries");

	/** If true, all bushes yield twigs/small sticks when gathered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bush Yields", meta=(PCG_Overridable))
	bool bYieldsTwigs = true;

	/** Item ID for twigs yield. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bush Yields", meta=(PCG_Overridable, EditCondition="bYieldsTwigs"))
	FName TwigsItemId = FName("Twig");

	/** If true, all bushes yield leaves/foliage when gathered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bush Yields", meta=(PCG_Overridable))
	bool bYieldsLeaves = false;

	/** Item ID for leaves yield. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bush Yields", meta=(PCG_Overridable, EditCondition="bYieldsLeaves"))
	FName LeavesItemId = FName("Leaves");

	/** Get all yield item tags for this bush spawner. */
	TArray<FName> GetYieldTags() const;
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
