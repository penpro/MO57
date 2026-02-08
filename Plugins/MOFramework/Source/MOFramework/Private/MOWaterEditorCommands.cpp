#include "MOFramework.h"

#if WITH_EDITOR
#include "MOWaterMaterialGenerator.h"
#include "HAL/IConsoleManager.h"

// Console command to generate water material
static FAutoConsoleCommand GenerateWaterMaterialCommand(
	TEXT("MO.GenerateWaterMaterial"),
	TEXT("Generate a water material at /MOFramework/Materials/M_Water"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UMaterial* Material = UMOWaterMaterialGenerator::CreateSimpleWaterMaterial(TEXT("/MOFramework/Materials/M_Water"));
		if (Material)
		{
			UE_LOG(LogMOFramework, Log, TEXT("Water material created successfully!"));
		}
		else
		{
			UE_LOG(LogMOFramework, Error, TEXT("Failed to create water material"));
		}
	})
);

// Console command to generate ocean material (larger waves)
static FAutoConsoleCommand GenerateOceanMaterialCommand(
	TEXT("MO.GenerateOceanMaterial"),
	TEXT("Generate an ocean material at /MOFramework/Materials/M_Ocean"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UMaterial* Material = UMOWaterMaterialGenerator::CreateSimpleWaterMaterial(TEXT("/MOFramework/Materials/M_Ocean"));
		if (Material)
		{
			UE_LOG(LogMOFramework, Log, TEXT("Ocean material created successfully!"));
		}
		else
		{
			UE_LOG(LogMOFramework, Error, TEXT("Failed to create ocean material"));
		}
	})
);

#endif // WITH_EDITOR
