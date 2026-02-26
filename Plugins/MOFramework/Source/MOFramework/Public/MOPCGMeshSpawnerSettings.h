/**
 * =============================================================================
 * MOPCGMeshSpawnerSettings.h - All-in-One PCG Item Mesh Spawner
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * All-in-one PCG node that spawns item meshes as tagged HISM components.
 * Combines functionality of MO Item Spawner, Static Mesh Spawner, and
 * MO HISM Tagger into single efficient node.
 *
 * PROCESS:
 * 1. Selects item based on weighted random
 * 2. Gets mesh from item's WorldVisual.StaticMesh
 * 3. Groups points by mesh, creates HISM components
 * 4. Tags each HISM with "MOItem_<ItemId>"
 * 5. Registers with MOPCGInteractionSubsystem
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] MAIN THREAD: Element runs on main thread only. Required for
 *   component spawning and subsystem access.
 *
 * [2024-02] DISCARD INVALID: bDiscardInvalidPoints removes points without
 *   valid item/mesh assignment from output.
 *
 * [2024-02] MANAGED ISM: Creates managed ISM components via PCG system.
 *   Component lifetime tied to PCG component.
 *
 * =============================================================================
 * RELATED FILES: MOPCGItemSpawnerSettings.h, MOPCGHISMTaggerSettings.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOPCGItemSpawnerSettings.h"  // For FMOPCGItemSpawnEntry
#include "MOPCGMeshSpawnerSettings.generated.h"

class UDataTable;
class UStaticMesh;
class UInstancedStaticMeshComponent;
class UPCGComponent;

/**
 * All-in-one PCG node that spawns item meshes as tagged HISM components.
 *
 * Combines the functionality of:
 * - MO Item Spawner (item selection, metadata)
 * - Static Mesh Spawner (HISM creation)
 * - MO HISM Tagger (component tagging)
 *
 * For each input point:
 * 1. Selects an item based on weighted random
 * 2. Gets the mesh from the item's WorldVisual.StaticMesh
 * 3. Groups points by mesh and creates HISM components
 * 4. Tags each HISM with "MOItem_<ItemId>"
 * 5. Registers tag mappings with MOPCGInteractionSubsystem
 *
 * This makes spawned items immediately discoverable by the foraging system.
 */
UCLASS(BlueprintType, ClassGroup=(MO))
class MOFRAMEWORK_API UMOPCGMeshSpawnerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UMOPCGMeshSpawnerSettings();

	// UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;
	virtual bool UseSeed() const override { return true; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MO Mesh Spawner")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOFramework", "MOMeshSpawnerTitle", "MO Mesh Spawner"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

public:
	// ============================================================================
	// ITEM CONFIGURATION
	// ============================================================================

	/** Item datatable containing FMOItemDefinitionRow entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Items", meta=(PCG_Overridable))
	TObjectPtr<UDataTable> ItemDataTable;

	/** Items to spawn with their weights and quantity ranges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Items", meta=(PCG_Overridable))
	TArray<FMOPCGItemSpawnEntry> ItemsToSpawn;

	/** Random seed offset for item selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Items", meta=(PCG_Overridable))
	int32 SeedOffset = 0;

	/** If true, removes points that don't have a valid item/mesh assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Items", meta=(PCG_Overridable))
	bool bDiscardInvalidPoints = true;

	// ============================================================================
	// MESH SPAWNING
	// ============================================================================

	/** Collision profile for spawned HISM components. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh", meta=(PCG_Overridable))
	FCollisionProfileName CollisionProfile = FCollisionProfileName(TEXT("NoCollision"));

	/** If true, HISM instances will cast shadows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh", meta=(PCG_Overridable))
	bool bCastShadows = true;

	/** Number of instances per cluster for HISM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mesh", meta=(PCG_Overridable, ClampMin="1"))
	int32 InstancesPerCluster = 64;

	// ============================================================================
	// TAGGING
	// ============================================================================

	/** Tag prefix for item tags. Default: "MOItem_" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tagging", meta=(PCG_Overridable))
	FString TagPrefix = TEXT("MOItem_");

	/** If true, registers tag mappings with MOPCGInteractionSubsystem for runtime lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tagging", meta=(PCG_Overridable))
	bool bRegisterWithSubsystem = true;
};

/**
 * PCG Element that executes the mesh spawning logic.
 */
class FMOPCGMeshSpawnerElement : public IPCGElement
{
public:
	// Must run on main thread to spawn components and access subsystems
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	/** Data for a single item type being spawned. */
	struct FItemSpawnData
	{
		FName ItemId;
		UStaticMesh* Mesh = nullptr;
		TArray<FTransform> Transforms;
	};

	/** Build item spawn data map from datatable. */
	TMap<FName, FItemSpawnData> BuildItemDataMap(
		const UDataTable* DataTable,
		const TArray<FMOPCGItemSpawnEntry>& Items) const;

	/** Create or find managed ISM component for a mesh on the target actor. */
	UInstancedStaticMeshComponent* GetOrCreateManagedISMC(
		FPCGContext* Context,
		AActor* TargetActor,
		UStaticMesh* Mesh,
		const UMOPCGMeshSpawnerSettings* Settings,
		FName ItemId,
		const TArray<FName>& ComponentTags) const;
};
