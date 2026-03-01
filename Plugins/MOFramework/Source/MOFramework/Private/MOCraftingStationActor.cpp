#include "MOCraftingStationActor.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOUIContractInterface.h"
#include "MOCraftingCapableInterface.h"
#include "MOworldSaveGame.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

AMOCraftingStationActor::AMOCraftingStationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Create inventory component
	StationInventory = CreateDefaultSubobject<UMOInventoryComponent>(TEXT("StationInventory"));

	// Create audio components
	OnSoundComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("OnSoundComponent"));
	OnSoundComponent->SetupAttachment(RootComponent);
	OnSoundComponent->bAutoActivate = false;
	OnSoundComponent->bIsUISound = false;

	UseSoundComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("UseSoundComponent"));
	UseSoundComponent->SetupAttachment(RootComponent);
	UseSoundComponent->bAutoActivate = false;
	UseSoundComponent->bIsUISound = false;

	// Create particle component
	ActiveParticleComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ActiveParticleComponent"));
	ActiveParticleComponent->SetupAttachment(RootComponent);
	ActiveParticleComponent->bAutoActivate = false;

	// Create light component for fire glow
	ActiveLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("ActiveLightComponent"));
	ActiveLightComponent->SetupAttachment(RootComponent);
	ActiveLightComponent->SetVisibility(false);
	ActiveLightComponent->SetIntensity(5000.0f);
	ActiveLightComponent->SetLightColor(FLinearColor(1.0f, 0.6f, 0.2f));
	ActiveLightComponent->SetAttenuationRadius(500.0f);
	ActiveLightComponent->SetCastShadows(false); // Cheaper without shadows
}

void AMOCraftingStationActor::BeginPlay()
{
	Super::BeginPlay();
}

void AMOCraftingStationActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Consume fuel if active and requires fuel
	if (bIsActive && bRequiresFuel && CurrentFuel > 0.0f)
	{
		CurrentFuel = FMath::Max(0.0f, CurrentFuel - FuelConsumptionRate * DeltaTime);

		if (CurrentFuel <= 0.0f)
		{
			SetStationActive(false);
			UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Fuel depleted, station deactivated"));
		}
	}

	// Update light flicker when active
	if (bIsActive)
	{
		UpdateLightFlicker(DeltaTime);
	}
}

void AMOCraftingStationActor::InitializeBuilding(FName InRecipeId)
{
	Super::InitializeBuilding(InRecipeId);

	// Get station configuration from recipe
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(InRecipeId);
	if (Recipe)
	{
		// Get station type directly from recipe
		StationType = Recipe->ProvidedStationType;

		// Fuel settings
		bRequiresFuel = Recipe->bRequiresFuel;
		MaxFuel = Recipe->MaxFuel;
		FuelConsumptionRate = Recipe->FuelConsumptionRate;
		AcceptedFuelItems = Recipe->AcceptedFuelItems;

		// Debug logging for fuel configuration
		UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Fuel config from recipe %s:"), *InRecipeId.ToString());
		UE_LOG(LogMOFramework, Log, TEXT("  bRequiresFuel: %s"), bRequiresFuel ? TEXT("true") : TEXT("false"));
		UE_LOG(LogMOFramework, Log, TEXT("  MaxFuel: %.1f"), MaxFuel);
		UE_LOG(LogMOFramework, Log, TEXT("  FuelConsumptionRate: %.1f"), FuelConsumptionRate);
		UE_LOG(LogMOFramework, Log, TEXT("  AcceptedFuelItems (%d):"), AcceptedFuelItems.Num());
		for (const FName& FuelId : AcceptedFuelItems)
		{
			UE_LOG(LogMOFramework, Log, TEXT("    - '%s'"), *FuelId.ToString());
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingStationActor] Could not find recipe: %s"), *InRecipeId.ToString());
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Initialized station type %d from recipe %s"),
		(int32)StationType, *InRecipeId.ToString());
}

void AMOCraftingStationActor::OnCompleteInteracted_Implementation(AController* Controller)
{
	Super::OnCompleteInteracted_Implementation(Controller);

	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC)
	{
		return;
	}

	// Set this station as the active station via interface - decouples from specific pawn type
	if (APawn* Pawn = PC->GetPawn())
	{
		if (Pawn->Implements<UMOCraftingCapableInterface>())
		{
			IMOCraftingCapableInterface::Execute_SetActiveCraftingStation(Pawn, this);
		}
	}

	// Open crafting menu via interface - decouples from specific controller type
	if (Controller->Implements<UMOUIContractInterface>())
	{
		IMOUIContractInterface::Execute_RequestOpenCraftingMenu(Controller, this);
		UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Opened crafting menu for station type %d"), (int32)StationType);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingStationActor] Controller does not implement IMOUIContractInterface"));
	}
}

bool AMOCraftingStationActor::HandleSecondaryInteract(AController* Controller)
{
	// For complete stations, show the station context menu instead of crafting menu
	if (IsComplete())
	{
		if (Controller && Controller->Implements<UMOUIContractInterface>())
		{
			IMOUIContractInterface::Execute_RequestShowStationContextMenu(Controller, this, GetActorLocation());
			UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Showing station context menu"));
			return true;
		}
	}

	// For ghost/constructing states, use base class behavior
	return Super::HandleSecondaryInteract(Controller);
}

float AMOCraftingStationActor::AddFuel(FName ItemDefinitionId, int32 Quantity)
{
	// Check if this item is accepted as fuel
	if (!AcceptedFuelItems.Contains(ItemDefinitionId))
	{
		return 0.0f;
	}

	// Add fuel (simple: each item adds 10 fuel)
	float FuelToAdd = Quantity * 10.0f;
	float PreviousFuel = CurrentFuel;
	CurrentFuel = FMath::Min(MaxFuel, CurrentFuel + FuelToAdd);

	float ActualAdded = CurrentFuel - PreviousFuel;
	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Added %.1f fuel (item: %s x%d)"), ActualAdded, *ItemDefinitionId.ToString(), Quantity);

	return ActualAdded;
}

float AMOCraftingStationActor::ConsumeFuelFromInventory()
{
	if (!StationInventory)
	{
		return 0.0f;
	}

	float TotalFuelAdded = 0.0f;

	// Get all items in inventory
	TArray<FMOInventoryEntry> Entries;
	StationInventory->GetInventoryEntries(Entries);

	// Find and consume fuel items
	for (const FMOInventoryEntry& Entry : Entries)
	{
		if (AcceptedFuelItems.Contains(Entry.ItemDefinitionId))
		{
			// Add fuel from this stack
			float FuelAdded = AddFuel(Entry.ItemDefinitionId, Entry.Quantity);
			TotalFuelAdded += FuelAdded;

			// Remove the items from inventory
			StationInventory->RemoveItemByGuid(Entry.ItemGuid, Entry.Quantity);

			UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Consumed %d x %s for %.1f fuel"),
				Entry.Quantity, *Entry.ItemDefinitionId.ToString(), FuelAdded);
		}
	}

	return TotalFuelAdded;
}

bool AMOCraftingStationActor::HasFuelInInventory() const
{
	if (!StationInventory)
	{
		return false;
	}

	// Check if any accepted fuel items are in inventory
	for (const FName& FuelItemId : AcceptedFuelItems)
	{
		if (StationInventory->HasItem(FuelItemId, 1))
		{
			return true;
		}
	}

	return false;
}

float AMOCraftingStationActor::GetFuelPercent() const
{
	if (MaxFuel <= 0.0f)
	{
		return 1.0f; // No fuel required
	}
	return CurrentFuel / MaxFuel;
}

bool AMOCraftingStationActor::IsStationActive() const
{
	if (!bRequiresFuel)
	{
		return true; // Always active if no fuel required
	}
	return bIsActive && CurrentFuel > 0.0f;
}

void AMOCraftingStationActor::SetStationActive(bool bActive)
{
	if (bActive && bRequiresFuel && CurrentFuel <= 0.0f)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingStationActor] Cannot activate - no fuel"));
		return;
	}

	bIsActive = bActive;
	// Tick when active (for fuel consumption and light flicker)
	SetActorTickEnabled(bIsActive);

	// Activate/deactivate audio and visual effects
	if (bIsActive)
	{
		PlayOnSound();
		ActivateParticles();
		ActivateLight();
	}
	else
	{
		StopOnSound();
		StopUseSound();
		DeactivateParticles();
		DeactivateLight();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Station %s"), bIsActive ? TEXT("activated") : TEXT("deactivated"));
}

// ============================================================================
// FUEL SYSTEM
// ============================================================================

float AMOCraftingStationActor::GetFuelTimeRemaining() const
{
	if (!bRequiresFuel || FuelConsumptionRate <= 0.0f)
	{
		return -1.0f; // Infinite/not applicable
	}
	return CurrentFuel / FuelConsumptionRate;
}

// ============================================================================
// AUDIO/VISUAL CONTROL
// ============================================================================

void AMOCraftingStationActor::PlayOnSound()
{
	if (!OnSoundComponent)
	{
		return;
	}

	// Load and set sound if configured
	if (!OnSound.IsNull())
	{
		USoundBase* Sound = OnSound.LoadSynchronous();
		if (Sound)
		{
			OnSoundComponent->SetSound(Sound);
			OnSoundComponent->SetVolumeMultiplier(SoundVolume);
			OnSoundComponent->Play();
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Playing OnSound"));
		}
	}
}

void AMOCraftingStationActor::StopOnSound()
{
	if (OnSoundComponent && OnSoundComponent->IsPlaying())
	{
		OnSoundComponent->Stop();
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Stopped OnSound"));
	}
}

void AMOCraftingStationActor::PlayUseSound()
{
	if (!UseSoundComponent)
	{
		return;
	}

	// Load and set sound if configured
	if (!UseSound.IsNull())
	{
		USoundBase* Sound = UseSound.LoadSynchronous();
		if (Sound)
		{
			UseSoundComponent->SetSound(Sound);
			UseSoundComponent->SetVolumeMultiplier(SoundVolume);
			UseSoundComponent->Play();
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Playing UseSound"));
		}
	}
}

void AMOCraftingStationActor::StopUseSound()
{
	if (UseSoundComponent && UseSoundComponent->IsPlaying())
	{
		UseSoundComponent->Stop();
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Stopped UseSound"));
	}
}

void AMOCraftingStationActor::ActivateParticles()
{
	if (!ActiveParticleComponent)
	{
		return;
	}

	// Load and set particle system if configured
	if (!ActiveParticleSystem.IsNull())
	{
		UNiagaraSystem* System = ActiveParticleSystem.LoadSynchronous();
		if (System)
		{
			ActiveParticleComponent->SetAsset(System);
			ActiveParticleComponent->Activate();
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Activated particles"));
		}
	}
	else
	{
		// If no system configured, just activate (might be set in Blueprint)
		ActiveParticleComponent->Activate();
	}
}

void AMOCraftingStationActor::DeactivateParticles()
{
	if (ActiveParticleComponent && ActiveParticleComponent->IsActive())
	{
		ActiveParticleComponent->Deactivate();
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Deactivated particles"));
	}
}

void AMOCraftingStationActor::ActivateLight()
{
	if (!ActiveLightComponent)
	{
		return;
	}

	// Apply configured settings
	ActiveLightComponent->SetIntensity(LightBaseIntensity);
	ActiveLightComponent->SetLightColor(LightColor);
	ActiveLightComponent->SetAttenuationRadius(LightRadius);
	ActiveLightComponent->SetVisibility(true);

	// Reset flicker accumulator
	FlickerTimeAccumulator = FMath::FRand() * 100.0f; // Random start for variety

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Activated light"));
}

void AMOCraftingStationActor::DeactivateLight()
{
	if (ActiveLightComponent)
	{
		ActiveLightComponent->SetVisibility(false);
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Deactivated light"));
	}
}

void AMOCraftingStationActor::UpdateLightFlicker(float DeltaTime)
{
	if (!ActiveLightComponent || !ActiveLightComponent->IsVisible())
	{
		return;
	}

	// Accumulate time
	FlickerTimeAccumulator += DeltaTime * LightFlickerSpeed;

	// Use multiple overlapping sine waves for organic fire-like flicker
	// This creates an irregular, natural-looking pattern
	const float Wave1 = FMath::Sin(FlickerTimeAccumulator * 1.0f);
	const float Wave2 = FMath::Sin(FlickerTimeAccumulator * 2.3f) * 0.5f;
	const float Wave3 = FMath::Sin(FlickerTimeAccumulator * 5.7f) * 0.25f;

	// Combine waves and normalize to -1 to 1 range
	const float CombinedWave = (Wave1 + Wave2 + Wave3) / 1.75f;

	// Map to intensity variation
	const float FlickerMultiplier = 1.0f + (CombinedWave * LightFlickerAmount);

	// Apply to light
	const float NewIntensity = LightBaseIntensity * FlickerMultiplier;
	ActiveLightComponent->SetIntensity(NewIntensity);
}

// ============================================================================
// IMOInventoryHolderInterface IMPLEMENTATION
// ============================================================================

UMOInventoryComponent* AMOCraftingStationActor::GetInventory_Implementation() const
{
	return StationInventory;
}

bool AMOCraftingStationActor::HasInventoryItem_Implementation(FName ItemDefinitionId, int32 Quantity) const
{
	return StationInventory ? StationInventory->HasItem(ItemDefinitionId, Quantity) : false;
}

int32 AMOCraftingStationActor::GetInventoryItemCount_Implementation(FName ItemDefinitionId) const
{
	return StationInventory ? StationInventory->GetItemCountByDefinitionId(ItemDefinitionId) : 0;
}

// ============================================================================
// IMOMaterialSourceInterface IMPLEMENTATION
// ============================================================================

bool AMOCraftingStationActor::CanProvideMaterial_Implementation(FName MaterialId, int32 Quantity) const
{
	// Only provide materials if the station is fully built
	if (!IsComplete())
	{
		return false;
	}
	return StationInventory ? StationInventory->HasItem(MaterialId, Quantity) : false;
}

int32 AMOCraftingStationActor::GatherMaterial_Implementation(FName MaterialId, int32 Quantity)
{
	if (!IsComplete() || !StationInventory)
	{
		return 0;
	}

	int32 Gathered = 0;
	for (int32 i = 0; i < Quantity; ++i)
	{
		if (StationInventory->RemoveItemByDefinitionId(MaterialId, 1))
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

int32 AMOCraftingStationActor::GetMaterialSourcePriority_Implementation() const
{
	// Crafting stations have same priority as containers (50)
	return 50;
}

// ============================================================================
// SAVE/LOAD
// ============================================================================

void AMOCraftingStationActor::BuildSaveData(FMOPersistedBuildingRecord& OutRecord) const
{
	// Call parent to save building state
	Super::BuildSaveData(OutRecord);

	// Save station inventory
	if (StationInventory)
	{
		FMOInventorySaveData InventorySave;
		StationInventory->BuildSaveData(InventorySave);

		// Copy to building record format
		OutRecord.InventorySlotCount = InventorySave.SlotCount;
		OutRecord.InventorySlotGuids = InventorySave.SlotItemGuids;
		OutRecord.InventoryItems = InventorySave.Items;

		UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Saved %d items in %d slots"),
			OutRecord.InventoryItems.Num(), OutRecord.InventorySlotCount);
	}

	// Save fuel state
	OutRecord.CurrentFuel = CurrentFuel;
	OutRecord.bIsActive = bIsActive;

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Saved fuel=%.1f active=%d"),
		CurrentFuel, bIsActive ? 1 : 0);
}

void AMOCraftingStationActor::ApplySaveData(const FMOPersistedBuildingRecord& InRecord)
{
	// Call parent to restore building state
	Super::ApplySaveData(InRecord);

	// Restore recipe-based fuel configuration (not saved, comes from recipe)
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(GetRecipeId());
	if (Recipe)
	{
		StationType = Recipe->ProvidedStationType;
		bRequiresFuel = Recipe->bRequiresFuel;
		MaxFuel = Recipe->MaxFuel;
		FuelConsumptionRate = Recipe->FuelConsumptionRate;
		AcceptedFuelItems = Recipe->AcceptedFuelItems;

		UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Restored fuel config from recipe, AcceptedFuelItems: %d"), AcceptedFuelItems.Num());
	}

	// Restore station inventory
	if (StationInventory && InRecord.InventorySlotCount > 0)
	{
		// Build inventory save data from building record
		FMOInventorySaveData InventorySave;
		InventorySave.SlotCount = InRecord.InventorySlotCount;
		InventorySave.SlotItemGuids = InRecord.InventorySlotGuids;
		InventorySave.Items = InRecord.InventoryItems;

		// Ensure slot count matches
		StationInventory->SlotCount = InRecord.InventorySlotCount;

		// Apply the save data
		StationInventory->ApplySaveDataAuthority(InventorySave);

		UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Restored %d items in %d slots"),
			InRecord.InventoryItems.Num(), InRecord.InventorySlotCount);
	}

	// Restore fuel state
	CurrentFuel = InRecord.CurrentFuel;
	bIsActive = InRecord.bIsActive;

	// Re-enable tick if station was active and requires fuel
	if (bIsActive && bRequiresFuel)
	{
		SetActorTickEnabled(true);
	}

	// Restore audio/visual state
	if (bIsActive)
	{
		PlayOnSound();
		ActivateParticles();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Restored fuel=%.1f active=%d"),
		CurrentFuel, bIsActive ? 1 : 0);
}
