#include "MOSpawnSettings.h"

UMOSpawnSettings::UMOSpawnSettings()
{
	// Initialize Survivor config
	SurvivorConfig.Category = EMOSpawnCategory::Survivor;
	SurvivorConfig.MinCooldownSeconds = 3600.0f;   // 1 hour
	SurvivorConfig.MaxCooldownSeconds = 7200.0f;   // 2 hours
	SurvivorConfig.MaxSpawnedCount = 3;
	SurvivorConfig.MinSpawnDistance = 200.0f;
	SurvivorConfig.MaxSpawnDistance = 500.0f;
	SurvivorConfig.MinDistanceFromStructure = 200.0f;
	SurvivorConfig.MinGroupSize = 1;
	SurvivorConfig.MaxGroupSize = 1;
	SurvivorConfig.bFirstSpawnFaster = true;
	SurvivorConfig.FirstSpawnMaxCooldown = 1800.0f;  // 30 min for first spawn
	SurvivorConfig.bEnabled = true;

	// Initialize Prey config
	PreyConfig.Category = EMOSpawnCategory::Prey;
	PreyConfig.MinCooldownSeconds = 2700.0f;   // 45 min
	PreyConfig.MaxCooldownSeconds = 4500.0f;   // 1.25 hours
	PreyConfig.MaxSpawnedCount = 10;
	PreyConfig.MinSpawnDistance = 100.0f;
	PreyConfig.MaxSpawnDistance = 400.0f;
	PreyConfig.MinDistanceFromStructure = 200.0f;
	PreyConfig.MinGroupSize = 2;
	PreyConfig.MaxGroupSize = 4;
	PreyConfig.bFirstSpawnFaster = false;
	PreyConfig.bEnabled = true;

	// Initialize Predator config
	PredatorConfig.Category = EMOSpawnCategory::Predator;
	PredatorConfig.MinCooldownSeconds = 5400.0f;   // 1.5 hours
	PredatorConfig.MaxCooldownSeconds = 9000.0f;   // 2.5 hours
	PredatorConfig.MaxSpawnedCount = 3;
	PredatorConfig.MinSpawnDistance = 300.0f;
	PredatorConfig.MaxSpawnDistance = 600.0f;
	PredatorConfig.MinDistanceFromStructure = 300.0f;
	PredatorConfig.MinGroupSize = 1;
	PredatorConfig.MaxGroupSize = 2;
	PredatorConfig.bFirstSpawnFaster = false;
	PredatorConfig.bEnabled = true;

	// Initialize Ambient config
	AmbientConfig.Category = EMOSpawnCategory::Ambient;
	AmbientConfig.MinCooldownSeconds = 300.0f;    // 5 min
	AmbientConfig.MaxCooldownSeconds = 600.0f;    // 10 min
	AmbientConfig.MaxSpawnedCount = 20;
	AmbientConfig.MinSpawnDistance = 5.0f;
	AmbientConfig.MaxSpawnDistance = 50.0f;
	AmbientConfig.MinDistanceFromStructure = 0.0f;  // Ambient can spawn anywhere
	AmbientConfig.MinGroupSize = 1;
	AmbientConfig.MaxGroupSize = 5;
	AmbientConfig.bFirstSpawnFaster = false;
	AmbientConfig.bEnabled = true;
}

UMOSpawnSettings* UMOSpawnSettings::GetSpawnSettings()
{
	return GetMutableDefault<UMOSpawnSettings>();
}

TArray<FMOSpawnCategoryConfig> UMOSpawnSettings::GetAllCategoryConfigs() const
{
	TArray<FMOSpawnCategoryConfig> Configs;
	Configs.Add(SurvivorConfig);
	Configs.Add(PreyConfig);
	Configs.Add(PredatorConfig);
	Configs.Add(AmbientConfig);
	return Configs;
}
