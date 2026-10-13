#include "MOGameState.h"
#include "MOWorldSeedSubsystem.h"
#include "MOWorldSyncSubsystem.h"
#include "Net/UnrealNetwork.h"

AMOGameState::AMOGameState()
{
	RemovedInstances.Owner = this;   // the client callback (FMORemovedInstance::PostReplicatedAdd) reaches the world through it
}

void AMOGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMOGameState, WorldSeed);
	DOREPLIFETIME(AMOGameState, WorldClock);
	DOREPLIFETIME(AMOGameState, WorldWeather);
	DOREPLIFETIME(AMOGameState, RemovedInstances);
}

void AMOGameState::AddRemovedInstance(const FMORemovedInstance& Entry)
{
	check(HasAuthority());
	RemovedInstances.Owner = this;
	FMORemovedInstance& Added = RemovedInstances.Items.Add_GetRef(Entry);
	RemovedInstances.MarkItemDirty(Added);
}

void AMOGameState::ResetRemovedInstances(const TArray<FMORemovedInstance>& All)
{
	check(HasAuthority());
	RemovedInstances.Owner = this;
	RemovedInstances.Items = All;
	for (FMORemovedInstance& Item : RemovedInstances.Items)
	{
		Item.ReplicationID = INDEX_NONE;   // fresh entries for the new array (copied from the save, not from this array)
		RemovedInstances.MarkItemDirty(Item);
	}
	RemovedInstances.MarkArrayDirty();
}

void AMOGameState::SetWorldClockInfo(const FMOWorldClockInfo& NewInfo)
{
	check(HasAuthority());
	WorldClock = NewInfo;
}

void AMOGameState::SetWorldWeatherInfo(const FMOWorldWeatherInfo& NewInfo)
{
	check(HasAuthority());
	WorldWeather = NewInfo;
	ForceNetUpdate(); // a weather change is rare and worth sending now
}

void AMOGameState::OnRep_WorldClock()
{
	if (UMOWorldSyncSubsystem* Sync = UMOWorldSyncSubsystem::Get(this))
	{
		Sync->HandleReplicatedClock(WorldClock);
	}
}

void AMOGameState::OnRep_WorldWeather()
{
	if (UMOWorldSyncSubsystem* Sync = UMOWorldSyncSubsystem::Get(this))
	{
		Sync->HandleReplicatedWeather(WorldWeather);
	}
}

void AMOGameState::SetWorldSeedInfo(const FMOWorldSeedInfo& NewInfo)
{
	check(HasAuthority());
	WorldSeed = NewInfo;
	ForceNetUpdate();
}

void AMOGameState::OnRep_WorldSeed()
{
	if (UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(this))
	{
		Seeds->HandleReplicatedWorldSeed(WorldSeed);
	}
}
