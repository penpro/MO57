/**
 * MOWorldRemovalSubsystem.h - world instances removed for good (picked up, felled, quarried)
 *
 * See MOWorldRemovalTypes.h for why. The flow:
 *   - FMOHISMHarvestHelper::RemoveInstance (the one place a harvest / pickup / forage removes a PCG instance) calls RecordRemoval BEFORE removing.
 *   - The host adds the entry to AMOGameState's replicated FMORemovedInstanceArray; each client's FMORemovedInstance::PostReplicatedAdd lands in
 *     HandleReplicatedRemoval, which removes the client's local copy of the instance.
 *   - Every MO spawner (biome spawner, item mesh spawner) asks IsRemoved before planting, so a regenerated cell stays harvested.
 *   - IMOSaveDomain: the record is saved and loaded; on load the instances already spawned are removed too.
 * Removals are permanent: a felled tree does not grow back, the dead wood picked off the forest floor is not replaced. (Regrowth over game time
 * is a design question, not implemented: see Docs/Handoff_2026-10-13.md.)
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOSaveDomainInterface.h"
#include "MOWorldRemovalTypes.h"
#include "MOWorldRemovalSubsystem.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

UCLASS()
class MOFRAMEWORK_API UMOWorldRemovalSubsystem : public UWorldSubsystem, public IMOSaveDomain
{
	GENERATED_BODY()

public:
	static UMOWorldRemovalSubsystem* Get(const UObject* WorldContextObject);

	/** Record that the instance of Mesh at WorldLocation is gone for good. Authority: recorded, replicated, saved. Client: recorded locally (the host's record follows). */
	void RecordRemoval(const FVector& WorldLocation, const UStaticMesh* Mesh);

	/** Convenience: record instance InstanceIndex of ISM (its world location and the component's mesh). Call BEFORE removing it. */
	void RecordInstanceRemoval(const UInstancedStaticMeshComponent* ISM, int32 InstanceIndex);

	/** Whether an instance of Mesh at WorldLocation was removed. Spawners call this before planting. */
	bool IsRemoved(const FVector& WorldLocation, const UStaticMesh* Mesh) const;

	/** Client: an entry arrived from the host. Remove the local copy. */
	void HandleReplicatedRemoval(const FMORemovedInstance& Entry);

	int32 GetNumRemoved() const { return Ledger.Num(); }

	/** Remove the spawned instances matching these entries from every instanced mesh component of the world. Returns how many were removed. */
	int32 RemoveSpawnedMatches(TConstArrayView<FMORemovedInstance> Entries);

	//~ Begin IMOSaveDomain
	virtual FName GetSaveDomainName() const override { return TEXT("WorldRemovals"); }
	virtual void CaptureSaveDomain(UMOWorldSaveGame& Save) override;
	virtual void ApplySaveDomain(const UMOWorldSaveGame& Save) override;
	//~ End IMOSaveDomain

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	FMOWorldRemovalLedger Ledger;

	/** Authority: every entry in record order (the save payload; the replicated array mirrors it). */
	TArray<FMORemovedInstance> Entries;

	void AddAuthoritative(const FMORemovedInstance& Entry);
};
