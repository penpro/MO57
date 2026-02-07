#include "MOContainerActor.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOUIContractInterface.h"
#include "MOworldSaveGame.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

AMOContainerActor::AMOContainerActor()
{
	// Create inventory component
	ContainerInventory = CreateDefaultSubobject<UMOInventoryComponent>(TEXT("ContainerInventory"));

	// Create audio components
	OpenSoundComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("OpenSoundComponent"));
	OpenSoundComponent->SetupAttachment(RootComponent);
	OpenSoundComponent->bAutoActivate = false;
	OpenSoundComponent->bIsUISound = false;

	CloseSoundComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("CloseSoundComponent"));
	CloseSoundComponent->SetupAttachment(RootComponent);
	CloseSoundComponent->bAutoActivate = false;
	CloseSoundComponent->bIsUISound = false;
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

	// Play open sound
	PlayOpenSound();

	// Use interface to request UI - decouples from specific controller type
	if (Controller && Controller->Implements<UMOUIContractInterface>())
	{
		IMOUIContractInterface::Execute_RequestOpenContainerInventory(Controller, this);
		UE_LOG(LogMOFramework, Log, TEXT("[MOContainerActor] Opening container inventory UI"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOContainerActor] Controller does not implement IMOUIContractInterface"));
	}
}

// ============================================================================
// AUDIO CONTROL
// ============================================================================

void AMOContainerActor::PlayOpenSound()
{
	if (!OpenSoundComponent)
	{
		return;
	}

	// Load and play sound if configured
	if (!OpenSound.IsNull())
	{
		USoundBase* Sound = OpenSound.LoadSynchronous();
		if (Sound)
		{
			OpenSoundComponent->SetSound(Sound);
			OpenSoundComponent->SetVolumeMultiplier(SoundVolume);
			OpenSoundComponent->Play();
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOContainerActor] Playing OpenSound"));
		}
	}
}

void AMOContainerActor::PlayCloseSound()
{
	if (!CloseSoundComponent)
	{
		return;
	}

	// Load and play sound if configured
	if (!CloseSound.IsNull())
	{
		USoundBase* Sound = CloseSound.LoadSynchronous();
		if (Sound)
		{
			CloseSoundComponent->SetSound(Sound);
			CloseSoundComponent->SetVolumeMultiplier(SoundVolume);
			CloseSoundComponent->Play();
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOContainerActor] Playing CloseSound"));
		}
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

// ============================================================================
// SAVE/LOAD
// ============================================================================

void AMOContainerActor::BuildSaveData(FMOPersistedBuildingRecord& OutRecord) const
{
	// Call parent to save building state
	Super::BuildSaveData(OutRecord);

	// Save container inventory
	if (ContainerInventory)
	{
		FMOInventorySaveData InventorySave;
		ContainerInventory->BuildSaveData(InventorySave);

		// Copy to building record format
		OutRecord.InventorySlotCount = InventorySave.SlotCount;
		OutRecord.InventorySlotGuids = InventorySave.SlotItemGuids;
		OutRecord.InventoryItems = InventorySave.Items;

		UE_LOG(LogMOFramework, Log, TEXT("[MOContainerActor] Saved %d items in %d slots"),
			OutRecord.InventoryItems.Num(), OutRecord.InventorySlotCount);
	}
}

void AMOContainerActor::ApplySaveData(const FMOPersistedBuildingRecord& InRecord)
{
	// Call parent to restore building state
	Super::ApplySaveData(InRecord);

	// Restore container inventory
	if (ContainerInventory && InRecord.InventorySlotCount > 0)
	{
		// Build inventory save data from building record
		FMOInventorySaveData InventorySave;
		InventorySave.SlotCount = InRecord.InventorySlotCount;
		InventorySave.SlotItemGuids = InRecord.InventorySlotGuids;
		InventorySave.Items = InRecord.InventoryItems;

		// Ensure slot count matches
		ContainerInventory->SlotCount = InRecord.InventorySlotCount;

		// Apply the save data
		ContainerInventory->ApplySaveDataAuthority(InventorySave);

		UE_LOG(LogMOFramework, Log, TEXT("[MOContainerActor] Restored %d items in %d slots"),
			InRecord.InventoryItems.Num(), InRecord.InventorySlotCount);
	}
}
