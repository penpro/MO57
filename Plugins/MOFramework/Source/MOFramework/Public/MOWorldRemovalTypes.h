/**
 * MOWorldRemovalTypes.h - the record of world instances removed for good
 *
 * A picked-up stick, a felled tree, a quarried boulder: PCG instances the world must not grow back on its own. PCG cells are generated, cleaned up when
 * the player walks away and generated again when the player returns, and every co-op client generates its own copy -- so a removal that only deletes
 * the instance (what harvesting did until Oct 2026) comes back on the next generation, never reaches the other machines, and is gone from a save.
 *
 * The record is keyed by PLACE + MESH: the instance's world X / Y in 10 cm cells, its height in 1 m cells, and its static mesh's name. Generation is
 * deterministic (same seed, same point seeds, same transform), so a regenerated instance lands in the same cell and is recognised. Lookups tolerate one
 * cell either way (the transform's float round trip through the component's local space). The height (Oct 17 2026) tells apart instances stacked in one
 * column -- a cave's floor rubble under its ceiling rock, a gallery under another: by X / Y alone, quarrying one removed both. Entries from saves made
 * before it carry AnyZ and match at every height, as they always did.
 *
 * Owners: UMOWorldRemovalSubsystem (logic, save domain), AMOGameState (replication: FMORemovedInstanceArray).
 */

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MOWorldRemovalTypes.generated.h"

class AMOGameState;
class UStaticMesh;

/** One removed world instance. Also the save entry (the FastArray bookkeeping fields are not UPROPERTYs, so they are not saved). */
USTRUCT()
struct MOFRAMEWORK_API FMORemovedInstance : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** World X / Y in 10 cm cells (rounded). */
	UPROPERTY()
	int32 CellX = 0;

	UPROPERTY()
	int32 CellY = 0;

	/** World Z in 1 m cells (rounded); AnyZ for entries recorded before heights were (they match at any height). */
	UPROPERTY()
	int32 CellZ = MIN_int32;
	static constexpr int32 AnyZ = MIN_int32;

	/** The instance's static mesh (object name). */
	UPROPERTY()
	FName Mesh;

	/** Client: the host removed this instance -- remove the local copy and keep it out of every later generation. */
	void PostReplicatedAdd(const struct FMORemovedInstanceArray& InArraySerializer);
};

/** The replicated list (on AMOGameState): every client, including one that joins later, receives the whole record. */
USTRUCT()
struct MOFRAMEWORK_API FMORemovedInstanceArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMORemovedInstance> Items;

	/** Not replicated: the game state that owns the array (the client callback reaches the world through it). */
	TWeakObjectPtr<AMOGameState> Owner;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMORemovedInstance, FMORemovedInstanceArray>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FMORemovedInstanceArray> : public TStructOpsTypeTraitsBase2<FMORemovedInstanceArray>
{
	enum { WithNetDeltaSerializer = true };
};

/** Save payload (held in UMOWorldSaveGame). Empty for older saves. */
USTRUCT()
struct MOFRAMEWORK_API FMOWorldRemovalSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMORemovedInstance> Removed;
};

/**
 * The lookup itself, a plain value type (so it is unit-testable without a world): a set of (cell X, cell Y, mesh).
 */
struct MOFRAMEWORK_API FMOWorldRemovalLedger
{
	/** Cell size of the key (cm): X / Y, and the height. */
	static constexpr double CellCm = 10.0;
	static constexpr double CellZCm = 100.0;

	static FMORemovedInstance MakeEntry(const FVector& WorldLocation, FName MeshName);

	/** Adds the entry; false when it was already recorded. */
	bool Add(const FMORemovedInstance& Entry);

	/** Whether an instance of this mesh at this place was removed (one cell of tolerance either way). */
	bool IsRemoved(const FVector& WorldLocation, FName MeshName) const;

	int32 Num() const { return NumEntries; }
	void Reset() { Keys.Reset(); NumEntries = 0; }

private:
	struct FKey
	{
		int32 X = 0;
		int32 Y = 0;
		FName Mesh;
		bool operator==(const FKey& O) const { return X == O.X && Y == O.Y && Mesh == O.Mesh; }
		friend uint32 GetTypeHash(const FKey& K) { return HashCombineFast(HashCombineFast(::GetTypeHash(K.X), ::GetTypeHash(K.Y)), GetTypeHash(K.Mesh)); }
	};
	/** The heights recorded at each place + mesh (usually one). */
	TMap<FKey, TArray<int32, TInlineAllocator<2>>> Keys;
	int32 NumEntries = 0;
};
