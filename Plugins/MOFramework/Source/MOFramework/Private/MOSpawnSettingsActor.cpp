#include "MOSpawnSettingsActor.h"
#include "MOSpawnSettings.h"
#include "MOFramework.h"
#include "EngineUtils.h"
#include "Engine/World.h"

TWeakObjectPtr<AMOSpawnSettingsActor> AMOSpawnSettingsActor::CachedInstance;
TWeakObjectPtr<UWorld> AMOSpawnSettingsActor::CachedWorld;

AMOSpawnSettingsActor::AMOSpawnSettingsActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// This actor doesn't need any components - it's just a settings container
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false; // Always load this actor
#endif

	InitializeDefaults();
}

void AMOSpawnSettingsActor::InitializeDefaults()
{
	// Set up default category types
	SurvivorConfig.Category = EMOSpawnCategory::Survivor;
	SurvivorConfig.MinCooldownSeconds = 1800.0f;  // 30 min
	SurvivorConfig.MaxCooldownSeconds = 3600.0f;  // 1 hour
	SurvivorConfig.MaxSpawnedCount = 3;
	SurvivorConfig.MinSpawnDistance = 100.0f;
	SurvivorConfig.MaxSpawnDistance = 500.0f;
	SurvivorConfig.MinDistanceFromStructure = 200.0f;
	SurvivorConfig.MinGroupSize = 1;
	SurvivorConfig.MaxGroupSize = 1;
	SurvivorConfig.bFirstSpawnFaster = true;
	SurvivorConfig.FirstSpawnMaxCooldown = 300.0f;  // 5 min for first spawn
	SurvivorConfig.bEnabled = true;

	PreyConfig.Category = EMOSpawnCategory::Prey;
	PreyConfig.MinCooldownSeconds = 600.0f;   // 10 min
	PreyConfig.MaxCooldownSeconds = 1800.0f;  // 30 min
	PreyConfig.MaxSpawnedCount = 8;
	PreyConfig.MinSpawnDistance = 80.0f;
	PreyConfig.MaxSpawnDistance = 400.0f;
	PreyConfig.MinDistanceFromStructure = 150.0f;
	PreyConfig.MinGroupSize = 2;
	PreyConfig.MaxGroupSize = 4;
	PreyConfig.bFirstSpawnFaster = true;
	PreyConfig.FirstSpawnMaxCooldown = 120.0f;  // 2 min for first spawn
	PreyConfig.bEnabled = true;

	PredatorConfig.Category = EMOSpawnCategory::Predator;
	PredatorConfig.MinCooldownSeconds = 2400.0f;  // 40 min
	PredatorConfig.MaxCooldownSeconds = 4800.0f;  // 80 min
	PredatorConfig.MaxSpawnedCount = 3;
	PredatorConfig.MinSpawnDistance = 150.0f;
	PredatorConfig.MaxSpawnDistance = 600.0f;
	PredatorConfig.MinDistanceFromStructure = 300.0f;
	PredatorConfig.MinGroupSize = 1;
	PredatorConfig.MaxGroupSize = 3;
	PredatorConfig.bFirstSpawnFaster = true;
	PredatorConfig.FirstSpawnMaxCooldown = 600.0f;  // 10 min for first spawn
	PredatorConfig.bEnabled = true;

	AmbientConfig.Category = EMOSpawnCategory::Ambient;
	AmbientConfig.MinCooldownSeconds = 300.0f;   // 5 min
	AmbientConfig.MaxCooldownSeconds = 900.0f;   // 15 min
	AmbientConfig.MaxSpawnedCount = 20;
	AmbientConfig.MinSpawnDistance = 30.0f;
	AmbientConfig.MaxSpawnDistance = 200.0f;
	AmbientConfig.MinDistanceFromStructure = 50.0f;
	AmbientConfig.MinGroupSize = 1;
	AmbientConfig.MaxGroupSize = 5;
	AmbientConfig.bFirstSpawnFaster = false;
	AmbientConfig.bEnabled = true;
}

void AMOSpawnSettingsActor::BeginPlay()
{
	Super::BeginPlay();
	RegisterAsSettingsProvider();
}

void AMOSpawnSettingsActor::RegisterAsSettingsProvider()
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
			TEXT("[MOSpawnSettings] Multiple spawn settings actors found! Using %s, ignoring %s"),
			*CachedInstance->GetName(), *GetName());
		return;
	}

	CachedInstance = this;
	CachedWorld = MyWorld;
	UE_LOG(LogMOFramework, Warning, TEXT("[MOSpawnSettings] Registered settings actor: %s (Prey.MinCooldown=%.1f, Multiplier=%.2f)"),
		*GetName(), PreyConfig.MinCooldownSeconds, DebugSettings.CooldownTimeMultiplier);
}

void AMOSpawnSettingsActor::UnregisterAsSettingsProvider()
{
	if (CachedInstance.Get() == this)
	{
		CachedInstance = nullptr;
		CachedWorld = nullptr;
		UE_LOG(LogMOFramework, Log, TEXT("[MOSpawnSettings] Unregistered settings actor: %s"), *GetName());
	}
}

void AMOSpawnSettingsActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterAsSettingsProvider();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void AMOSpawnSettingsActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Log changes for easier debugging
	if (PropertyChangedEvent.Property)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSpawnSettings] Property changed: %s"),
			*PropertyChangedEvent.Property->GetName());
	}
}
#endif

AMOSpawnSettingsActor* AMOSpawnSettingsActor::GetSpawnSettingsActor(const UObject* WorldContextObject)
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
	for (TActorIterator<AMOSpawnSettingsActor> It(World); It; ++It)
	{
		CachedInstance = *It;
		CachedWorld = World;
		return CachedInstance.Get();
	}

	return nullptr;
}

FMOSpawnCategoryConfig AMOSpawnSettingsActor::GetSurvivorConfig(const UObject* WorldContextObject)
{
	if (AMOSpawnSettingsActor* Settings = GetSpawnSettingsActor(WorldContextObject))
	{
		return Settings->SurvivorConfig;
	}

	// Fall back to Project Settings
	if (UMOSpawnSettings* ProjectSettings = UMOSpawnSettings::GetSpawnSettings())
	{
		return ProjectSettings->SurvivorConfig;
	}

	// Return defaults
	FMOSpawnCategoryConfig Default;
	Default.Category = EMOSpawnCategory::Survivor;
	return Default;
}

FMOSpawnCategoryConfig AMOSpawnSettingsActor::GetPreyConfig(const UObject* WorldContextObject)
{
	if (AMOSpawnSettingsActor* Settings = GetSpawnSettingsActor(WorldContextObject))
	{
		return Settings->PreyConfig;
	}

	// Fall back to Project Settings
	if (UMOSpawnSettings* ProjectSettings = UMOSpawnSettings::GetSpawnSettings())
	{
		return ProjectSettings->PreyConfig;
	}

	// Return defaults
	FMOSpawnCategoryConfig Default;
	Default.Category = EMOSpawnCategory::Prey;
	return Default;
}

FMOSpawnCategoryConfig AMOSpawnSettingsActor::GetPredatorConfig(const UObject* WorldContextObject)
{
	if (AMOSpawnSettingsActor* Settings = GetSpawnSettingsActor(WorldContextObject))
	{
		return Settings->PredatorConfig;
	}

	// Fall back to Project Settings
	if (UMOSpawnSettings* ProjectSettings = UMOSpawnSettings::GetSpawnSettings())
	{
		return ProjectSettings->PredatorConfig;
	}

	// Return defaults
	FMOSpawnCategoryConfig Default;
	Default.Category = EMOSpawnCategory::Predator;
	return Default;
}

FMOSpawnCategoryConfig AMOSpawnSettingsActor::GetAmbientConfig(const UObject* WorldContextObject)
{
	if (AMOSpawnSettingsActor* Settings = GetSpawnSettingsActor(WorldContextObject))
	{
		return Settings->AmbientConfig;
	}

	// Fall back to Project Settings
	if (UMOSpawnSettings* ProjectSettings = UMOSpawnSettings::GetSpawnSettings())
	{
		return ProjectSettings->AmbientConfig;
	}

	// Return defaults
	FMOSpawnCategoryConfig Default;
	Default.Category = EMOSpawnCategory::Ambient;
	return Default;
}

FMOSpawnCategoryConfig AMOSpawnSettingsActor::GetCategoryConfig(const UObject* WorldContextObject, EMOSpawnCategory Category)
{
	switch (Category)
	{
	case EMOSpawnCategory::Survivor:
		return GetSurvivorConfig(WorldContextObject);
	case EMOSpawnCategory::Prey:
		return GetPreyConfig(WorldContextObject);
	case EMOSpawnCategory::Predator:
		return GetPredatorConfig(WorldContextObject);
	case EMOSpawnCategory::Ambient:
		return GetAmbientConfig(WorldContextObject);
	default:
		return FMOSpawnCategoryConfig();
	}
}

TArray<FMOSpawnCategoryConfig> AMOSpawnSettingsActor::GetAllCategoryConfigs(const UObject* WorldContextObject)
{
	TArray<FMOSpawnCategoryConfig> Configs;
	Configs.Add(GetSurvivorConfig(WorldContextObject));
	Configs.Add(GetPreyConfig(WorldContextObject));
	Configs.Add(GetPredatorConfig(WorldContextObject));
	Configs.Add(GetAmbientConfig(WorldContextObject));
	return Configs;
}

FMOSpawnGlobalSettings AMOSpawnSettingsActor::GetGlobalSettings(const UObject* WorldContextObject)
{
	if (AMOSpawnSettingsActor* Settings = GetSpawnSettingsActor(WorldContextObject))
	{
		return Settings->GlobalSettings;
	}

	// Fall back to Project Settings
	if (UMOSpawnSettings* ProjectSettings = UMOSpawnSettings::GetSpawnSettings())
	{
		FMOSpawnGlobalSettings Global;
		Global.SurvivorPersistenceHours = ProjectSettings->SurvivorPersistenceHours;
		Global.SpawnCheckInterval = ProjectSettings->SpawnCheckInterval;
		Global.MaxSpawnPointQueryDistance = ProjectSettings->MaxSpawnPointQueryDistance;
		return Global;
	}

	// Return defaults
	return FMOSpawnGlobalSettings();
}

FMOSpawnDebugSettings AMOSpawnSettingsActor::GetDebugSettings(const UObject* WorldContextObject)
{
	if (AMOSpawnSettingsActor* Settings = GetSpawnSettingsActor(WorldContextObject))
	{
		return Settings->DebugSettings;
	}

	// Fall back to Project Settings
	if (UMOSpawnSettings* ProjectSettings = UMOSpawnSettings::GetSpawnSettings())
	{
		FMOSpawnDebugSettings Debug;
		Debug.CooldownTimeMultiplier = ProjectSettings->CooldownTimeMultiplier;
		Debug.bEnableVerboseLogging = ProjectSettings->bEnableVerboseLogging;
		return Debug;
	}

	// Return defaults
	return FMOSpawnDebugSettings();
}
