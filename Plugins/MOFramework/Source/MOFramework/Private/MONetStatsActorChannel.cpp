#include "MONetStatsActorChannel.h"
#include "MONetDiagnostics.h"
#include "GameFramework/Actor.h"
#include "Net/DataBunch.h"

UMONetStatsActorChannel::UMONetStatsActorChannel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

namespace
{
	FName ActorClassName(const AActor* Actor)
	{
		static const FName Opening(TEXT("(opening)"));
		return Actor ? Actor->GetClass()->GetFName() : Opening;
	}
}

FPacketIdRange UMONetStatsActorChannel::SendBunch(FOutBunch* Bunch, bool Merge)
{
	if (Bunch && !Bunch->IsError())
	{
		FMONetCounters::NoteChannelBits(ActorClassName(Actor), Bunch->GetNumBits(), true);
	}
	return Super::SendBunch(Bunch, Merge);
}

void UMONetStatsActorChannel::ReceivedBunch(FInBunch& Bunch)
{
	const int64 Bits = Bunch.GetNumBits();
	Super::ReceivedBunch(Bunch);   // an opening bunch spawns the actor: attribute it after
	FMONetCounters::NoteChannelBits(ActorClassName(Actor), Bits, false);
}
