/**
 * =============================================================================
 * MOPCGForceHISMTreeBuild.h - Force HISM Tree Rebuild After PCG Spawn
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * PCG node that forces HISM tree rebuilding after StaticMeshSpawner nodes.
 * Fixes issue where InstanceMinDrawDistance and other tree-dependent properties
 * aren't applied until a tree rebuild occurs.
 *
 * WHY NEEDED:
 * PCG's StaticMeshSpawner calls AddInstances() but never BuildTreeIfOutdated().
 * This means HISM properties are ignored until something else triggers rebuild.
 *
 * PLACEMENT:
 * Place this node AFTER StaticMeshSpawner nodes in your PCG graph.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] FORCE UPDATE: bForceUpdate=true (default) forces rebuild even if
 *   tree appears up-to-date. Recommended to leave enabled.
 *
 * [2024-02] EXECUTION ORDER: Must come AFTER mesh spawners in the graph.
 *   Connect input to spawner outputs or use Dependencies pin.
 *
 * [2024-02] PERFORMANCE: Rebuilding trees can be expensive with many instances.
 *   Use sparingly in runtime generation.
 *
 * =============================================================================
 * RELATED FILES: MOPCGMeshSpawnerSettings.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

// Copyright MO57. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "MOPCGForceHISMTreeBuild.generated.h"

/**
 * PCG Node that forces HISM tree rebuilding on the target actor.
 *
 * Place this node AFTER StaticMeshSpawner nodes to ensure InstanceMinDrawDistance
 * and other tree-dependent properties are properly applied.
 *
 * Why this is needed:
 * PCG's StaticMeshSpawner calls AddInstances() but never calls BuildTreeIfOutdated(),
 * so HISM properties like InstanceMinDrawDistance are never applied until something
 * else triggers a tree rebuild (like reconnecting a pin).
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class MOFRAMEWORK_API UMOPCGForceHISMTreeBuildSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UMOPCGForceHISMTreeBuildSettings();

	//~Begin UPCGSettings interface
	virtual bool UseSeed() const override { return false; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("MO Force HISM Tree Build")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("MOPCGForceHISMTreeBuild", "NodeTitle", "MO Force HISM Tree Build"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** If true, forces rebuild even if the tree appears up-to-date. Recommended to leave true. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bForceUpdate = true;

	/** If true, logs information about each HISM component processed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDebugLog = false;
};

class FPCGForceHISMTreeBuildElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
