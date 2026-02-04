#include "MOContainerActor.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOUIManagerComponent.h"
#include "MOPlayerController.h"

AMOContainerActor::AMOContainerActor()
{
	// Create inventory component
	ContainerInventory = CreateDefaultSubobject<UMOInventoryComponent>(TEXT("ContainerInventory"));
}

void AMOContainerActor::BeginPlay()
{
	Super::BeginPlay();

	// Initialize inventory with slot count
	if (ContainerInventory)
	{
		ContainerInventory->SlotCount = SlotCount;
	}
}

void AMOContainerActor::InitializeBuilding(FName InRecipeId)
{
	Super::InitializeBuilding(InRecipeId);

	// Get slot count from recipe
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(InRecipeId);
	if (Recipe && Recipe->ContainerSlotCount > 0)
	{
		SlotCount = Recipe->ContainerSlotCount;
		if (ContainerInventory)
		{
			ContainerInventory->SlotCount = SlotCount;
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOContainerActor] Initialized with %d slots from recipe %s"), SlotCount, *InRecipeId.ToString());
}

void AMOContainerActor::OnCompleteInteracted_Implementation(AController* Controller)
{
	Super::OnCompleteInteracted_Implementation(Controller);

	// Open container inventory UI
	AMOPlayerController* PC = Cast<AMOPlayerController>(Controller);
	if (PC && PC->UIManagerComponent)
	{
		// TODO: Open container-specific inventory UI
		// For now, just log
		UE_LOG(LogMOFramework, Log, TEXT("[MOContainerActor] Container interaction - would open inventory UI"));
	}
}
