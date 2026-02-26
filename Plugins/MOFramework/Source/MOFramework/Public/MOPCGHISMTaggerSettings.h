/**
 * =============================================================================
 * MOPCGHISMTaggerSettings.h - PCG Node for HISM Item Tagging
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * PCG node that tags HISM components with their corresponding item IDs.
 * Place AFTER Static Mesh Spawner in PCG graph. Finds all HISM components
 * created by PCG and adds tags based on mesh-to-item lookup.
 *
 * TAG FORMAT: "MOItem_<ItemId>" (e.g., "MOItem_FlintNodule01")
 *
 * Also registers mappings with MOPCGInteractionSubsystem for runtime lookup.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] MAIN THREAD: Element runs on main thread only (CanExecuteOnlyOnMainThread).
 *   Required for world subsystem access and component modification.
 *
 * [2024-02] NOT CACHEABLE: IsCacheable returns false. Tags must be reapplied
 *   each time PCG graph executes.
 *
 * [2024-02] MESH LOOKUP: Uses WorldVisual.StaticMesh from FMOItemDefinitionRow
 *   to build mesh-to-item mapping.
 *
 * =============================================================================
 * RELATED FILES: MOPCGInteractionSubsystem.h, MOItemDefinitionRow.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOPCGHISMTaggerSettings.generated.h"

class UDataTable;

/**
 * PCG node that tags HISM components with their corresponding item IDs.
 *
 * Place this node AFTER the Static Mesh Spawner in your PCG graph.
 * It will find all HISM components created by PCG and add tags based on
 * mesh-to-item lookup, making them discoverable by the foraging system.
 *
 * Tag format: "MOItem_<ItemId>" (e.g., "MOItem_FlintNodule01")
 *
 * The node also registers these mappings with MOPCGInteractionSubsystem
 * for runtime tag-based item lookup.
 */
UCLASS(BlueprintType, ClassGroup=(MO))
class MOFRAMEWORK_API UMOPCGHISMTaggerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UMOPCGHISMTaggerSettings();

	// UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MO HISM Tagger")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOFramework", "MOHISMTaggerTitle", "MO HISM Tagger"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

public:
	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Item datatable containing FMOItemDefinitionRow entries for mesh lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable))
	TObjectPtr<UDataTable> ItemDataTable;

	/** Tag prefix for item tags. Default: "MOItem_" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable))
	FString TagPrefix = TEXT("MOItem_");

	/** If true, also registers tag mappings with MOPCGInteractionSubsystem at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(PCG_Overridable))
	bool bRegisterWithSubsystem = true;
};

/**
 * PCG Element that executes the HISM tagging logic.
 */
class FMOPCGHISMTaggerElement : public IPCGElement
{
public:
	// Must run on main thread to access world subsystems and modify components
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	/** Build mesh-to-item lookup from datatable. */
	TMap<FSoftObjectPath, FName> BuildMeshToItemMap(const UDataTable* DataTable) const;
};
