#pragma once

#include "CoreMinimal.h"
#include "MOBuildableActor.h"
#include "MORecipeDefinitionRow.h"
#include "MOInventoryHolderInterface.h"
#include "MOMaterialSourceInterface.h"
#include "MOCraftingStationActor.generated.h"

class UMOInventoryComponent;

/**
 * A buildable crafting station actor (campfire, forge, etc.).
 * When complete, interaction opens the crafting menu filtered to this station type.
 */
UCLASS()
class MOFRAMEWORK_API AMOCraftingStationActor : public AMOBuildableActor,
	public IMOInventoryHolderInterface,
	public IMOMaterialSourceInterface
{
	GENERATED_BODY()

public:
	AMOCraftingStationActor();

	// ============================================================================
	// COMPONENTS
	// ============================================================================

	/** Inventory component for fuel and items being processed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|CraftingStation")
	TObjectPtr<UMOInventoryComponent> StationInventory;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Type of crafting station. Set from recipe data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation")
	EMOCraftingStation StationType = EMOCraftingStation::None;

	/** Whether this station requires fuel to operate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Fuel")
	bool bRequiresFuel = false;

	/** Current fuel amount (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Fuel")
	float CurrentFuel = 0.0f;

	/** Maximum fuel capacity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Fuel")
	float MaxFuel = 100.0f;

	/** Fuel consumption rate per second when active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Fuel")
	float FuelConsumptionRate = 1.0f;

	/** Item IDs that can be used as fuel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Fuel")
	TArray<FName> AcceptedFuelItems;

	// ============================================================================
	// FUEL SYSTEM
	// ============================================================================

	/** Add fuel to the station. Returns amount actually added. */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|Fuel")
	float AddFuel(FName ItemDefinitionId, int32 Quantity);

	/** Get current fuel percentage (0.0 - 1.0). */
	UFUNCTION(BlueprintPure, Category="MO|CraftingStation|Fuel")
	float GetFuelPercent() const;

	/** Check if the station is currently active (has fuel if required). */
	UFUNCTION(BlueprintPure, Category="MO|CraftingStation")
	bool IsStationActive() const;

	/** Set station active state. */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation")
	void SetStationActive(bool bActive);

	// ============================================================================
	// ACCESSORS
	// ============================================================================

	/** Get the station inventory component. */
	UFUNCTION(BlueprintPure, Category="MO|CraftingStation")
	UMOInventoryComponent* GetStationInventory() const { return StationInventory; }

	/** Get the station type. */
	UFUNCTION(BlueprintPure, Category="MO|CraftingStation")
	EMOCraftingStation GetStationType() const { return StationType; }

	// ============================================================================
	// IMOInventoryHolderInterface IMPLEMENTATION
	// ============================================================================

	virtual UMOInventoryComponent* GetInventory_Implementation() const override;
	virtual bool HasInventoryItem_Implementation(FName ItemDefinitionId, int32 Quantity) const override;
	virtual int32 GetInventoryItemCount_Implementation(FName ItemDefinitionId) const override;

	// ============================================================================
	// IMOMaterialSourceInterface IMPLEMENTATION
	// ============================================================================

	virtual bool CanProvideMaterial_Implementation(FName MaterialId, int32 Quantity) const override;
	virtual int32 GatherMaterial_Implementation(FName MaterialId, int32 Quantity) override;
	virtual int32 GetMaterialSourcePriority_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Override to open crafting menu when complete. */
	virtual void OnCompleteInteracted_Implementation(AController* Controller) override;

	/** Initialize station type from recipe data. */
	virtual void InitializeBuilding(FName InRecipeId) override;

private:
	/** Whether the station is currently active. */
	bool bIsActive = false;
};
