#include "MOGameMode.h"
#include "MOFramework.h"
#include "MOPCGInteractionSubsystem.h"

AMOGameMode::AMOGameMode()
{
	// Add default tag mappings - can be overridden in Blueprint
	PCGTagItemMappings.Add({ TEXT("GivesStick"), TEXT("Stick01") });
}

void AMOGameMode::BeginPlay()
{
	Super::BeginPlay();

	RegisterPCGTagMappings();
}

void AMOGameMode::RegisterPCGTagMappings()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMOPCGInteractionSubsystem* PCGSubsystem = World->GetSubsystem<UMOPCGInteractionSubsystem>();
	if (!PCGSubsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGameMode] No PCG Interaction Subsystem found"));
		return;
	}

	for (const FMOTagItemMapping& Mapping : PCGTagItemMappings)
	{
		if (!Mapping.Tag.IsNone() && !Mapping.ItemId.IsNone())
		{
			PCGSubsystem->RegisterTagItemMapping(Mapping.Tag, Mapping.ItemId);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameMode] Registered %d PCG tag-to-item mappings"), PCGTagItemMappings.Num());
}
