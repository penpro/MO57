// The actor channel every MO net driver uses (Config/DefaultEngine.ini [/Script/Engine.NetDriver] ChannelDefinitions): the stock UActorChannel,
// plus a count of the bits each bunch carries, by the class of the channel's actor (FMONetCounters::NoteChannelBits). That is the "which actors
// fill the connection" column of MO.Net.Stats and of the bug report's Net.* rows; nothing about replication changes.
//
// Outgoing bunches are counted in SendBunch (property replication, RPCs and the channel's open / close, before any split into partial bunches);
// incoming ones in ReceivedBunch (whole bunches). Packet headers and acks are not in it, so the sum is a little below the connection's bytes.

#pragma once

#include "CoreMinimal.h"
#include "Engine/ActorChannel.h"
#include "MONetStatsActorChannel.generated.h"

UCLASS(transient, customConstructor)
class MOFRAMEWORK_API UMONetStatsActorChannel : public UActorChannel
{
	GENERATED_BODY()

public:
	UMONetStatsActorChannel(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual FPacketIdRange SendBunch(FOutBunch* Bunch, bool Merge) override;
	virtual void ReceivedBunch(FInBunch& Bunch) override;
};
