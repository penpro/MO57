/**
 * =============================================================================
 * MOPCGItemSpawnerSettings.h - PCG Node for Spawning Forageable Items
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * PCG node that assigns item metadata to points for HISM spawning. Takes input
 * points, randomly selects items from weighted table, assigns mesh and item
 * attributes. Output feeds into PCG Static Mesh Spawner.
 *
 * PCG WORKFLOW:
 * 1. Input points from terrain sampler, scatter, etc.
 * 2. This node processes each point:
 *    - Selects item from weighted spawn entries
 *    - Reads item's WorldVisual.StaticMesh
 *    - Adds PCG attributes: MOItemId, MOQuantityMin, MOQuantityMax
 * 3. Output points → Static Mesh Spawner → HISM components
 *
 * INTEGRATION WITH FORAGING:
 * - HISM instances have MOItemId attribute
 * - MOForagingSubsystem reads MOItemId to map mesh → item
 * - Player picks up → HISM instance removed → WorldItem spawned
 *
 * WEIGHTED SELECTION:
 * Uses FMOWeightedSelector for random selection:
 * - Weight=1.0 is baseline
 * - Weight=2.0 is twice as common
 * - All weights are relative within the spawn entries
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] MESH REQUIREMENT: Items must have WorldVisual.StaticMesh set in
 *   DT_Items. If null, the point is skipped (no mesh to spawn).
 *
 * [2024-02] ATTRIBUTE NAMES: PCG attributes are strings. Use exact names:
 *   "MOItemId", "MOQuantityMin", "MOQuantityMax". Typos cause silent failures.
 *
 * [2024-02] SEED USAGE: UseSeed() returns true. Different seeds = different
 *   item distributions. Pass consistent seed for reproducible worlds.
 *
 * [2024-02] DATATABLE LOADING: ItemDataTable is TSoftObjectPtr. Ensure it's
 *   loaded before PCG execution (usually handled automatically).
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOForagingSubsystem.h - Uses MOItemId attribute for pickup
 * - MOItemDefinitionRow.h - Item definitions with WorldVisual
 * - MOItemDatabaseSettings.h - Global item datatable reference
 * - MOWeightedSelector.h - Template for weighted random selection
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOPCGItemSpawnerSettings.generated.h"

class UDataTable;

/**
 * Entry defining an item to spawn with PCG.
 * See file header for PCG workflow details.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOPCGItemSpawnEntry
{
	GENERATED_BODY()

	/** The item definition ID (row name in the item datatable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
	FName ItemId;

	/** Relative spawn weight. Higher = more common. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG", meta=(ClampMin="0.0"))
	float Weight = 1.0f;

	/** Minimum quantity per harvest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG", meta=(ClampMin="1"))
	int32 MinQuantity = 1;

	/** Maximum quantity per harvest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG", meta=(ClampMin="1"))
	int32 MaxQuantity = 1;

	/** Required by FMOWeightedSelector template. */
	float GetWeight() const { return Weight; }
};

/**
 * PCG node that generates points for item spawning.
 *
 * Takes input points and assigns item metadata to them:
 * - Selects items based on weighted random
 * - Sets mesh from item's WorldVisual.StaticMesh
 * - Adds MOItemId, MOQuantityMin, MOQuantityMax attributes
 *
 * Output can be fed into a Static Mesh Spawner to create HISMs,
 * or partitioned by MOItemId for separate HISM components per item type.
 */
UCLASS(BlueprintType, ClassGroup=(MO))
class MOFRAMEWORK_API UMOPCGItemSpawnerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UMOPCGItemSpawnerSettings();

	// UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;
	virtual bool UseSeed() const override { return true; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MO Item Spawner")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOFramework", "MOItemSpawnerTitle", "MO Item Spawner"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

public:
	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Item datatable containing FMOItemDefinitionRow entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable))
	TObjectPtr<UDataTable> ItemDataTable;

	/** Items to spawn with their weights and quantity ranges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable))
	TArray<FMOPCGItemSpawnEntry> ItemsToSpawn;

	/** Random seed offset for item selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable))
	int32 SeedOffset = 0;

	/** If true, removes points that don't have a valid item/mesh assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable))
	bool bDiscardInvalidPoints = true;
};

/**
 * PCG Element that executes the item spawner logic.
 */
class FMOPCGItemSpawnerElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return false; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	/** Get the static mesh path for an item from the datatable (no sync load). */
	FSoftObjectPath GetMeshPathForItem(
		const UDataTable* DataTable,
		FName ItemId) const;
};
