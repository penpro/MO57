#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOPCGItemSpawnerSettings.generated.h"

class UDataTable;

/**
 * Entry defining an item to spawn with PCG.
 * Implements GetWeight() for use with FMOWeightedSelector.
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
