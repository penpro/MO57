#pragma once

#include "CoreMinimal.h"
#include "MOBuildableActor.h"
#include "MORecipeDefinitionRow.h"
#include "MOInventoryHolderInterface.h"
#include "MOMaterialSourceInterface.h"
#include "MOCraftingStationActor.generated.h"

class UMOInventoryComponent;
class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;

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

	/** Sound component for active/idle loop (fire crackling, etc.). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|CraftingStation|Audio")
	TObjectPtr<UAudioComponent> OnSoundComponent;

	/** Sound component for crafting activity loop. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|CraftingStation|Audio")
	TObjectPtr<UAudioComponent> UseSoundComponent;

	/** Particle effect for active station (fire, smoke, etc.). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|CraftingStation|Visual")
	TObjectPtr<UNiagaraComponent> ActiveParticleComponent;

	// ============================================================================
	// AUDIO/VISUAL CONFIGURATION
	// ============================================================================

	/** Sound to play while station is active (looping). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Audio")
	TSoftObjectPtr<USoundBase> OnSound;

	/** Sound to play while crafting at this station (looping). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Audio")
	TSoftObjectPtr<USoundBase> UseSound;

	/** Particle system for active state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Visual")
	TSoftObjectPtr<UNiagaraSystem> ActiveParticleSystem;

	/** Volume multiplier for station sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Audio", meta=(ClampMin="0.0", ClampMax="2.0"))
	float SoundVolume = 1.0f;

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

	/**
	 * Consume fuel items from the station's inventory and add to fuel.
	 * @return Total fuel added from consumed items
	 */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|Fuel")
	float ConsumeFuelFromInventory();

	/**
	 * Check if the station inventory contains any accepted fuel items.
	 */
	UFUNCTION(BlueprintPure, Category="MO|CraftingStation|Fuel")
	bool HasFuelInInventory() const;

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

	/** Get fuel time remaining in seconds. */
	UFUNCTION(BlueprintPure, Category="MO|CraftingStation|Fuel")
	float GetFuelTimeRemaining() const;

	// ============================================================================
	// AUDIO/VISUAL CONTROL
	// ============================================================================

	/** Start playing the active/on sound loop. */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|Audio")
	void PlayOnSound();

	/** Stop the active/on sound loop. */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|Audio")
	void StopOnSound();

	/** Start playing the crafting/use sound loop. */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|Audio")
	void PlayUseSound();

	/** Stop the crafting/use sound loop. */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|Audio")
	void StopUseSound();

	/** Activate particle effects. */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|Visual")
	void ActivateParticles();

	/** Deactivate particle effects. */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|Visual")
	void DeactivateParticles();

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

	/** Override secondary interact to show station context menu. */
	virtual bool HandleSecondaryInteract(AController* Controller) override;

	/** Initialize station type from recipe data. */
	virtual void InitializeBuilding(FName InRecipeId) override;

	/** Save station inventory and fuel state. */
	virtual void BuildSaveData(FMOPersistedBuildingRecord& OutRecord) const override;

	/** Restore station inventory and fuel state. */
	virtual void ApplySaveData(const FMOPersistedBuildingRecord& InRecord) override;

private:
	/** Whether the station is currently active. */
	bool bIsActive = false;
};
