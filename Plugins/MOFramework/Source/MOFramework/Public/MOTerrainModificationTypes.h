/**
 * MOTerrainModificationTypes.h - worked-ground zones, and how the host sends them to clients
 *
 * A zone (FMOTerrainModifiedZone) is a sphere of ground a player terraformed: dug, raised, flattened or cleared with Remove Foliage. Every machine keeps
 * the decorative foliage (AutoSweepTags) out of the zones with UMOTerrainModificationSubsystem's sweep, because every machine generates its own PCG
 * copy. Until Oct 18 2026 the zones existed only on the host, so a client still saw the grass the host had cleared (Wes's Steam test: "remove foliage
 * replicates from client to host but not host to client"). The host now mirrors its zone list into AMOGameState (FMOReplicatedTerrainZoneArray), and
 * each client's subsystem sweeps the zones it received.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MOTerrainModificationTypes.generated.h"

class AMOGameState;

/**
 * A sphere of world that has been deliberately terraformed. Foliage / PCG
 * content inside this sphere is kept clear by the subsystem's periodic sweep.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOTerrainModifiedZone
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|TerrainMod")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="MO|TerrainMod")
	float Radius = 250.0f;

	FMOTerrainModifiedZone() = default;
	FMOTerrainModifiedZone(const FVector& InLoc, float InRadius)
		: Location(InLoc), Radius(InRadius) {}
};

/** One zone as the host sends it (1 cm on the wire). */
USTRUCT()
struct MOFRAMEWORK_API FMOReplicatedTerrainZone : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize Location = FVector::ZeroVector;

	UPROPERTY()
	float Radius = 0.0f;

	/** Client: the host's zone list changed -- the subsystem re-reads it on the next tick. */
	void PostReplicatedAdd(const struct FMOReplicatedTerrainZoneArray& InArraySerializer);
	void PostReplicatedChange(const struct FMOReplicatedTerrainZoneArray& InArraySerializer);
	void PreReplicatedRemove(const struct FMOReplicatedTerrainZoneArray& InArraySerializer);
};

/**
 * The host's zone list on AMOGameState, index for index. A FastArray, not a plain replicated TArray: the list may hold up to MaxZones (5000), past
 * what plain array replication carries (net.MaxRepArraySize 2048), and a new or grown zone sends only itself.
 */
USTRUCT()
struct MOFRAMEWORK_API FMOReplicatedTerrainZoneArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOReplicatedTerrainZone> Items;

	/** Not replicated: the game state that owns the array (the client callbacks reach the world through it). */
	TWeakObjectPtr<AMOGameState> Owner;

	/** Client: tell the world's UMOTerrainModificationSubsystem that the host's list changed (MOTerrainModificationSubsystem.cpp). */
	void NotifyChanged() const;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMOReplicatedTerrainZone, FMOReplicatedTerrainZoneArray>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FMOReplicatedTerrainZoneArray> : public TStructOpsTypeTraitsBase2<FMOReplicatedTerrainZoneArray>
{
	enum { WithNetDeltaSerializer = true };
};
