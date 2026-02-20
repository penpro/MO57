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
