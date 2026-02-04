#include "MOCraftingStationActor.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOUIManagerComponent.h"
#include "MOPlayerController.h"

AMOCraftingStationActor::AMOCraftingStationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Create inventory component
	StationInventory = CreateDefaultSubobject<UMOInventoryComponent>(TEXT("StationInventory"));
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
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Initialized station type %d from recipe %s"),
		(int32)StationType, *InRecipeId.ToString());
}

void AMOCraftingStationActor::OnCompleteInteracted_Implementation(AController* Controller)
{
	Super::OnCompleteInteracted_Implementation(Controller);

	// Open crafting menu filtered to this station type
	AMOPlayerController* PC = Cast<AMOPlayerController>(Controller);
	if (PC && PC->UIManagerComponent)
	{
		// TODO: Open crafting menu with station filter
		// For now, open regular crafting menu
		PC->UIManagerComponent->OpenCraftingMenu();
		UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Opened crafting menu for station type %d"), (int32)StationType);
	}
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
	SetActorTickEnabled(bIsActive && bRequiresFuel);

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingStationActor] Station %s"), bIsActive ? TEXT("activated") : TEXT("deactivated"));
}
