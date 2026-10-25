#include "MOCraftingStationActor.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOUIContractInterface.h"
#include "MOCraftingCapableInterface.h"
#include "MOworldSaveGame.h"
#include "Components/AudioComponent.h"
#include "MOGameClockSubsystem.h"
#include "Net/UnrealNetwork.h"
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
	// The fuel sticks and their own materials, before the build look (Super::BeginPlay) can put them in the ghost material.
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (const FName PartName : FuelStickParts)
	{
		UMeshComponent* const* Found = Meshes.FindByPredicate([PartName](const UMeshComponent* Mesh) { return Mesh && Mesh->GetFName() == PartName; });
		if (!Found)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingStationActor] %s: fuel stick '%s' is not one of its mesh components"), *GetName(), *PartName.ToString());
			continue;
		}
		FuelStickComponents.Add(*Found);
		FMOBuildableSavedMaterials& Own = FuelStickOriginalMaterials.AddDefaulted_GetRef();
		for (int32 i = 0; i < (*Found)->GetNumMaterials(); ++i)
		{
			Own.Materials.Add((*Found)->GetMaterial(i));
		}
		(*Found)->SetVisibility(false);
	}

	Super::BeginPlay();
	UpdateFuelSticks(/*bForce=*/true);

	// Every station registers; the registry skips sources whose IsHeatActive()
	// is false, so cold stations cost one comparison per query.
	if (UMOAmbientEnvironmentRegistry* Ambient = UMOAmbientEnvironmentRegistry::Get(this))
	{
		Ambient->RegisterHeatSource(this);
	}
}

void AMOCraftingStationActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMOAmbientEnvironmentRegistry* Ambient = UMOAmbientEnvironmentRegistry::Get(this))
	{
		Ambient->UnregisterHeatSource(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AMOCraftingStationActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMOCraftingStationActor, bIsActive);
	DOREPLIFETIME(AMOCraftingStationActor, CurrentFuel);
	DOREPLIFETIME(AMOCraftingStationActor, CharredFuelSticks);
}

int32 AMOCraftingStationActor::CountVisibleFuelSticks(float Fuel, float MaxFuelAmount, int32 NumSticks)
{
	if (NumSticks <= 0 || Fuel <= 0.0f || MaxFuelAmount <= 0.0f)
	{
		return 0;
	}
	return FMath::Clamp(FMath::CeilToInt(Fuel / MaxFuelAmount * NumSticks - 1.0e-4f), 1, NumSticks);
}

void AMOCraftingStationActor::OnRep_FuelSticks()
{
	UpdateFuelSticks(/*bForce=*/false);
}

void AMOCraftingStationActor::RefreshBuildStateVisual()
{
	Super::RefreshBuildStateVisual();
	UpdateFuelSticks(/*bForce=*/true);   // the build look may just have given the sticks their own materials back: char them again
}

void AMOCraftingStationActor::UpdateFuelSticks(bool bForce)
{
	if (FuelStickComponents.IsEmpty())
	{
		return;
	}
	const bool bBuilt = IsComplete();
	const int32 Visible = bBuilt ? CountVisibleFuelSticks(CurrentFuel, MaxFuel, FuelStickComponents.Num()) : 0;
	if (HasAuthority())
	{
		// Sticks in a burning fire char; when it is out, burnt-away sticks leave the count and fresh wood laid on top stays fresh.
		const int32 Charred = bIsActive ? Visible : FMath::Min(CharredFuelSticks, Visible);
		if (Charred != CharredFuelSticks)
		{
			CharredFuelSticks = Charred;
		}
	}
	const int32 Charred = FMath::Min(CharredFuelSticks, Visible);
	if (!bForce && Visible == ShownFuelSticks && Charred == ShownCharredFuelSticks)
	{
		return;
	}
	ShownFuelSticks = Visible;
	ShownCharredFuelSticks = Charred;
	if (!LoadedCharredFuelMaterial && !CharredFuelMaterial.IsNull())
	{
		LoadedCharredFuelMaterial = CharredFuelMaterial.LoadSynchronous();
	}
	for (int32 i = 0; i < FuelStickComponents.Num(); ++i)
	{
		UMeshComponent* Mesh = FuelStickComponents[i].Get();
		if (!IsValid(Mesh))
		{
			continue;
		}
		Mesh->SetVisibility(i < Visible);
		if (!bBuilt || !FuelStickOriginalMaterials.IsValidIndex(i))
		{
			continue;   // unbuilt: the build look owns its materials
		}
		const TArray<TObjectPtr<UMaterialInterface>>& Own = FuelStickOriginalMaterials[i].Materials;
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			UMaterialInterface* Material = (i < Charred && LoadedCharredFuelMaterial) ? LoadedCharredFuelMaterial.Get()
				: (Own.IsValidIndex(Slot) ? Own[Slot].Get() : nullptr);
			Mesh->SetMaterial(Slot, Material);
		}
	}
}

void AMOCraftingStationActor::OnRep_IsActive()
{
	ApplyActiveVisuals();
}

void AMOCraftingStationActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// The server burns the fuel, on the world clock like everything else in the simulation (TimeScale 1 = real time); clients see the replicated level.
	if (HasAuthority() && bIsActive && bRequiresFuel && CurrentFuel > 0.0f)
	{
		const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
		CurrentFuel = FMath::Max(0.0f, CurrentFuel - FuelConsumptionRate * (Clock ? Clock->GetScaledDeltaTime(DeltaTime) : DeltaTime));

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
	UpdateFuelSticks(/*bForce=*/false);   // the fire eats its sticks one by one
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
		AchievableProcessTemperatureCelsius = Recipe->ProvidedProcessTemperatureCelsius;

		// Fuel settings
		bRequiresFuel = Recipe->bRequiresFuel;
		MaxFuel = Recipe->MaxFuel;
		FuelConsumptionRate = Recipe->FuelConsumptionRate;
		AcceptedFuelItems = Recipe->AcceptedFuelItems;

		// DEBUG (MOCraftingStationActor, disabled Oct 18): re-enable to trace the fuel config
		// // Debug logging for fuel configuration
		// UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Fuel config from recipe %s:"), *InRecipeId.ToString());
		// UE_LOG(LogMOFramework, Log, TEXT("  bRequiresFuel: %s"), bRequiresFuel ? TEXT("true") : TEXT("false"));
		// UE_LOG(LogMOFramework, Log, TEXT("  MaxFuel: %.1f"), MaxFuel);
		// UE_LOG(LogMOFramework, Log, TEXT("  FuelConsumptionRate: %.1f"), FuelConsumptionRate);
		// UE_LOG(LogMOFramework, Log, TEXT("  AcceptedFuelItems (%d):"), AcceptedFuelItems.Num());
		// for (const FName& FuelId : AcceptedFuelItems)
		// {
		// 	UE_LOG(LogMOFramework, Log, TEXT("    - '%s'"), *FuelId.ToString());
		// }
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingStationActor] Could not find recipe: %s"), *InRecipeId.ToString());
	}

	// Fire stations warm their surroundings. Type defaults apply only when the
	// instance hasn't been configured (HeatOutputCelsius == 0), so a recipe or
	// placed instance can still override. Values are radiant-heat scaled: a
	// campfire at arm's length (~1.5m -> ~75% falloff) must beat a winter
	// night hard enough that a body RECOVERS there, not merely stops freezing
	// (vitals regulation fully wins once local ambient clears ~15C).
	if (HeatOutputCelsius <= 0.0f)
	{
		switch (StationType)
		{
		case EMOCraftingStation::Campfire:
			HeatOutputCelsius = 40.0f;
			HeatRadiusCm = 600.0f;
			break;
		case EMOCraftingStation::Forge:
			HeatOutputCelsius = 40.0f;
			HeatRadiusCm = 450.0f;
			break;
		case EMOCraftingStation::Kitchen:
			HeatOutputCelsius = 20.0f;
			HeatRadiusCm = 400.0f;
			break;
		default:
			break;
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Initialized station type %d from recipe %s (heat %.0fC @ %.0fcm)"),
		(int32)StationType, *InRecipeId.ToString(), HeatOutputCelsius, HeatRadiusCm);
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

int32 AMOCraftingStationActor::ComputeAcceptedFuelItems(float CurrentFuel, float MaxFuel, float FuelPerItem, int32 Offered)
{
	if (Offered <= 0 || FuelPerItem <= 0.0f)
	{
		return 0;
	}
	const float Space = MaxFuel - CurrentFuel;
	if (Space <= 0.0f)
	{
		return 0;
	}
	// Whole items only — never accept an item that would overfill (its excess
	// fuel would be lost = matter destroyed). A near-full fire simply declines
	// the log and the carrier keeps it.
	return FMath::Min(Offered, FMath::FloorToInt(Space / FuelPerItem));
}

int32 AMOCraftingStationActor::AddFuel(FName ItemDefinitionId, int32 Quantity)
{
	if (!AcceptedFuelItems.Contains(ItemDefinitionId))
	{
		return 0;
	}

	const int32 Accepted = ComputeAcceptedFuelItems(CurrentFuel, MaxFuel, FuelPerItemUnit, Quantity);
	CurrentFuel = FMath::Min(MaxFuel, CurrentFuel + Accepted * FuelPerItemUnit);

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Accepted %d/%d fuel item(s) '%s' (fuel now %.1f/%.1f)"),
		Accepted, Quantity, *ItemDefinitionId.ToString(), CurrentFuel, MaxFuel);
	UpdateFuelSticks(/*bForce=*/false);

	return Accepted;
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

	// Find and consume fuel items — only the count the tank actually accepts;
	// the rest of the stack stays in the station's inventory (no matter lost).
	for (const FMOInventoryEntry& Entry : Entries)
	{
		if (AcceptedFuelItems.Contains(Entry.ItemDefinitionId))
		{
			const int32 Accepted = AddFuel(Entry.ItemDefinitionId, Entry.Quantity);
			if (Accepted <= 0)
			{
				continue; // tank full — leave this stack untouched
			}
			TotalFuelAdded += Accepted * FuelPerItemUnit;
			StationInventory->RemoveItemByGuid(Entry.ItemGuid, Accepted);

			UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Consumed %d/%d x %s for %.1f fuel"),
				Accepted, Entry.Quantity, *Entry.ItemDefinitionId.ToString(), Accepted * FuelPerItemUnit);
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
	ApplyActiveVisuals();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Station %s"), bIsActive ? TEXT("activated") : TEXT("deactivated"));
}

void AMOCraftingStationActor::ApplyActiveVisuals()
{
	// Tick when active (for fuel consumption and light flicker)
	SetActorTickEnabled(bIsActive);
	UpdateFuelSticks(/*bForce=*/false);   // lit: the sticks in it char

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
			// DEBUG (MOCraftingStationActor, disabled Oct 18): re-enable to trace verbose detail
			// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Playing OnSound"));
		}
	}
}

void AMOCraftingStationActor::StopOnSound()
{
	if (OnSoundComponent && OnSoundComponent->IsPlaying())
	{
		OnSoundComponent->Stop();
		// DEBUG (MOCraftingStationActor, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Stopped OnSound"));
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
			// DEBUG (MOCraftingStationActor, disabled Oct 18): re-enable to trace verbose detail
			// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Playing UseSound"));
		}
	}
}

void AMOCraftingStationActor::StopUseSound()
{
	if (UseSoundComponent && UseSoundComponent->IsPlaying())
	{
		UseSoundComponent->Stop();
		// DEBUG (MOCraftingStationActor, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Stopped UseSound"));
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
			// DEBUG (MOCraftingStationActor, disabled Oct 18): re-enable to trace verbose detail
			// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Activated particles"));
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
		// DEBUG (MOCraftingStationActor, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Deactivated particles"));
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

	// DEBUG (MOCraftingStationActor, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Activated light"));
}

void AMOCraftingStationActor::DeactivateLight()
{
	if (ActiveLightComponent)
	{
		ActiveLightComponent->SetVisibility(false);
		// DEBUG (MOCraftingStationActor, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCraftingStationActor] Deactivated light"));
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
		AchievableProcessTemperatureCelsius = Recipe->ProvidedProcessTemperatureCelsius;
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
	UpdateFuelSticks(/*bForce=*/true);   // the charred count is not saved: a fire loaded lit chars its sticks again, a cold one shows them fresh

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
