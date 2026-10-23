#include "MOGameState.h"
#include "MOWorldSeedSubsystem.h"
#include "MOWorldSyncSubsystem.h"
#include "MODevMode.h"
#include "MOFramework.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AMOGameState::AMOGameState()
{
	RemovedInstances.Owner = this;   // the client callback (FMORemovedInstance::PostReplicatedAdd) reaches the world through it
	TerrainZones.Owner = this;       // likewise FMOReplicatedTerrainZone's callbacks
}

void AMOGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMOGameState, WorldSeed);
	DOREPLIFETIME(AMOGameState, WorldClock);
	DOREPLIFETIME(AMOGameState, WorldWeather);
	DOREPLIFETIME(AMOGameState, RemovedInstances);
	DOREPLIFETIME(AMOGameState, TerrainZones);
	DOREPLIFETIME(AMOGameState, bHostDeveloperMode);
	DOREPLIFETIME(AMOGameState, GroundWeather);
	DOREPLIFETIME(AMOGameState, WorldCreation);
}

void AMOGameState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		bHostDeveloperMode = FMODevMode::IsEnabled();
		// The host can flip developer mode in Options at any time; replication sends only a change.
		GetWorldTimerManager().SetTimer(DevModeRefreshHandle, FTimerDelegate::CreateWeakLambda(this, [this]() { bHostDeveloperMode = FMODevMode::IsEnabled(); }), 2.0f, true);
	}
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

void AMOGameState::SyncTerrainZones(const TArray<FMOTerrainModifiedZone>& Zones)
{
	check(HasAuthority());
	TerrainZones.Owner = this;
	const bool bShrank = TerrainZones.Items.Num() > Zones.Num();
	if (bShrank)
	{
		TerrainZones.Items.SetNum(Zones.Num());
	}
	for (int32 i = 0; i < Zones.Num(); ++i)
	{
		if (!TerrainZones.Items.IsValidIndex(i))
		{
			TerrainZones.Items.AddDefaulted();
		}
		FMOReplicatedTerrainZone& Item = TerrainZones.Items[i];
		if (Item.ReplicationID == INDEX_NONE || !Item.Location.Equals(Zones[i].Location, 1.0) || Item.Radius != Zones[i].Radius)
		{
			Item.Location = Zones[i].Location;
			Item.Radius = Zones[i].Radius;
			TerrainZones.MarkItemDirty(Item);
		}
	}
	if (bShrank)
	{
		TerrainZones.MarkArrayDirty();
	}
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

void AMOGameState::SetWorldCreation(const FMOWorldCreationSettings& NewSettings)
{
	check(HasAuthority());
	WorldCreation = NewSettings;
	FMOWorldCreation::Sanitize(WorldCreation);
	ForceNetUpdate();
	UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldCreation] this world: %s"), *FMOWorldCreation::Describe(WorldCreation));
}

void AMOGameState::SetGroundWeather(const FMOGroundWeatherState& NewState)
{
	check(HasAuthority());
	// Wetness and snow move slowly; a step this small is invisible on the ground and keeps the property quiet between changes (a reset to 0 always goes).
	const bool bStep = FMath::Abs(NewState.Wetness - GroundWeather.Wetness) >= GroundWeatherReplicationStep
		|| FMath::Abs(NewState.SnowCover - GroundWeather.SnowCover) >= GroundWeatherReplicationStep
		|| (NewState.Wetness == 0.0f && GroundWeather.Wetness != 0.0f) || (NewState.SnowCover == 0.0f && GroundWeather.SnowCover != 0.0f)
		|| (NewState.Wetness == 1.0f && GroundWeather.Wetness != 1.0f) || (NewState.SnowCover == 1.0f && GroundWeather.SnowCover != 1.0f);
	if (bStep)
	{
		GroundWeather = NewState;
	}
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
