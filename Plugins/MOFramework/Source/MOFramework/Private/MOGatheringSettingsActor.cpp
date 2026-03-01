#include "MOGatheringSettingsActor.h"
#include "MOFramework.h"
#include "EngineUtils.h"
#include "Engine/World.h"

TWeakObjectPtr<AMOGatheringSettingsActor> AMOGatheringSettingsActor::CachedInstance;
TWeakObjectPtr<UWorld> AMOGatheringSettingsActor::CachedWorld;

AMOGatheringSettingsActor::AMOGatheringSettingsActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// This actor doesn't need any components - it's just a settings container
	// Make it hidden in game but visible in editor
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false; // Always load this actor
#endif
}

void AMOGatheringSettingsActor::BeginPlay()
{
	Super::BeginPlay();
	RegisterAsSettingsProvider();
}

void AMOGatheringSettingsActor::RegisterAsSettingsProvider()
{
	UWorld* MyWorld = GetWorld();

	// Check if cached instance is from a different world (stale from previous PIE session)
	if (CachedInstance.IsValid() && CachedWorld.IsValid() && CachedWorld.Get() != MyWorld)
	{
		// Clear stale cache from previous world
		CachedInstance = nullptr;
		CachedWorld = nullptr;
	}

	if (CachedInstance.IsValid() && CachedInstance.Get() != this)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOGatheringSettings] Multiple gathering settings actors found! Using %s, ignoring %s"),
			*CachedInstance->GetName(), *GetName());
		return;
	}

	CachedInstance = this;
	CachedWorld = MyWorld;
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGatheringSettings] Registered settings actor: %s (WanderRadius=%.1f, MinMoveDistance=%.1f)"),
		*GetName(), ForageBehavior.WanderRadius, ForageBehavior.MinMoveDistance);
}

void AMOGatheringSettingsActor::UnregisterAsSettingsProvider()
{
	if (CachedInstance.Get() == this)
	{
		CachedInstance = nullptr;
		CachedWorld = nullptr;
		UE_LOG(LogMOFramework, Log, TEXT("[MOGatheringSettings] Unregistered settings actor: %s"), *GetName());
	}
}

void AMOGatheringSettingsActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterAsSettingsProvider();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void AMOGatheringSettingsActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Log changes for easier debugging
	if (PropertyChangedEvent.Property)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGatheringSettings] Property changed: %s"),
			*PropertyChangedEvent.Property->GetName());
	}
}
#endif

AMOGatheringSettingsActor* AMOGatheringSettingsActor::GetGatheringSettings(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return nullptr;
	}

	// Return cached instance if valid AND from the same world
	if (CachedInstance.IsValid() && CachedWorld.IsValid() && CachedWorld.Get() == World)
	{
		return CachedInstance.Get();
	}

	// Clear stale cache
	CachedInstance = nullptr;
	CachedWorld = nullptr;

	// Search for instance in world
	for (TActorIterator<AMOGatheringSettingsActor> It(World); It; ++It)
	{
		CachedInstance = *It;
		CachedWorld = World;
		return CachedInstance.Get();
	}

	return nullptr;
}

FMOForageBehaviorSettings AMOGatheringSettingsActor::GetForageBehaviorSettings(const UObject* WorldContextObject)
{
	if (AMOGatheringSettingsActor* Settings = GetGatheringSettings(WorldContextObject))
	{
		return Settings->ForageBehavior;
	}

	// Return defaults
	return FMOForageBehaviorSettings();
}

FMOForageDetectionSettings AMOGatheringSettingsActor::GetForageDetectionSettings(const UObject* WorldContextObject)
{
	if (AMOGatheringSettingsActor* Settings = GetGatheringSettings(WorldContextObject))
	{
		return Settings->ForageDetection;
	}

	// Return defaults
	return FMOForageDetectionSettings();
}

FMODigSettings AMOGatheringSettingsActor::GetDigSettings(const UObject* WorldContextObject)
{
	if (AMOGatheringSettingsActor* Settings = GetGatheringSettings(WorldContextObject))
	{
		return Settings->DigSettings;
	}

	// Return defaults
	return FMODigSettings();
}

FMOForageXPSettings AMOGatheringSettingsActor::GetForageXPSettings(const UObject* WorldContextObject)
{
	if (AMOGatheringSettingsActor* Settings = GetGatheringSettings(WorldContextObject))
	{
		return Settings->ForageXP;
	}

	// Return defaults
	return FMOForageXPSettings();
}
