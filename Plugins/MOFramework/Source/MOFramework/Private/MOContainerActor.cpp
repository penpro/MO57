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

// ============================================================================
// IMOInventoryHolderInterface IMPLEMENTATION
// ============================================================================

UMOInventoryComponent* AMOContainerActor::GetInventory_Implementation() const
{
	return ContainerInventory;
}

bool AMOContainerActor::HasInventoryItem_Implementation(FName ItemDefinitionId, int32 Quantity) const
{
	return ContainerInventory ? ContainerInventory->HasItem(ItemDefinitionId, Quantity) : false;
}

int32 AMOContainerActor::GetInventoryItemCount_Implementation(FName ItemDefinitionId) const
{
	return ContainerInventory ? ContainerInventory->GetItemCountByDefinitionId(ItemDefinitionId) : 0;
}

// ============================================================================
// IMOMaterialSourceInterface IMPLEMENTATION
// ============================================================================

bool AMOContainerActor::CanProvideMaterial_Implementation(FName MaterialId, int32 Quantity) const
{
	// Only provide materials if the container is fully built
	if (!IsComplete())
	{
		return false;
	}
	return ContainerInventory ? ContainerInventory->HasItem(MaterialId, Quantity) : false;
}

int32 AMOContainerActor::GatherMaterial_Implementation(FName MaterialId, int32 Quantity)
{
	if (!IsComplete() || !ContainerInventory)
	{
		return 0;
	}

	int32 Gathered = 0;
	for (int32 i = 0; i < Quantity; ++i)
	{
		if (ContainerInventory->RemoveItemByDefinitionId(MaterialId, 1))
		{
			++Gathered;
		}
		else
		{
			break;
		}
	}
	return Gathered;
}

int32 AMOContainerActor::GetMaterialSourcePriority_Implementation() const
{
	// Containers have medium priority (50)
	return 50;
}
