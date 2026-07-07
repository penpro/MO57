/**
 * =============================================================================
 * MOCraftingStationActor.h - Buildable Crafting Station
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Buildable actor that functions as a crafting station (campfire, forge, etc.).
 * Extends MOBuildableActor with fuel system, station inventory, audio/visual
 * effects, and interaction handling to open filtered crafting menus.
 *
 * KEY RESPONSIBILITIES:
 * 1. Provide station-specific crafting (EMOCraftingStation type)
 * 2. Manage fuel consumption and inventory
 * 3. Control audio/visual effects for active state
 * 4. Store items being processed in StationInventory
 * 5. Implement IMOInventoryHolderInterface and IMOMaterialSourceInterface
 *
 * OWNERSHIP:
 * - Owner: World (spawned by building system or persistence)
 * - Lifespan: Persists until destroyed
 *
 * FUEL SYSTEM:
 * - bRequiresFuel: If true, station needs fuel to operate
 * - CurrentFuel/MaxFuel: Current fuel level (0-100)
 * - FuelConsumptionRate: Rate per second when active
 * - AcceptedFuelItems: Item IDs that count as fuel
 * - ConsumeFuelFromInventory(): Converts items to fuel
 *
 * INTERACTION FLOW:
 * Primary interact (complete) -> OnCompleteInteracted_Implementation()
 * -> Open crafting menu filtered by StationType
 * Secondary interact -> HandleSecondaryInteract()
 * -> Show station context menu (fuel, transfer, etc.)
 *
 * AUDIO/VISUAL:
 * - OnSound: Loop while station is active (fire crackling)
 * - UseSound: Loop while player is crafting
 * - ActiveParticleSystem: Niagara particles when active
 * - SetStationActive() controls all effects
 *
 * CRITICAL PATTERNS:
 * 1. INITIALIZATION: StationType set from recipe in InitializeBuilding()
 * 2. PERSISTENCE: BuildSaveData/ApplySaveData save fuel + inventory
 * 3. FUEL CHECK: IsStationActive() requires fuel if bRequiresFuel
 *
 * KNOWN PITFALLS:
 * 1. STATION TYPE: Must match EMOCraftingStation in recipe filter
 * 2. FUEL DEPLETION: Active crafts may stall if fuel runs out
 * 3. SOFT POINTERS: Audio/particle assets use TSoftObjectPtr
 *
 * RELATED FILES:
 * - MOBuildableActor.h - Base buildable class
 * - MOCraftingUIController.h - Opens filtered crafting menu
 * - MOCraftingSubsystem.h - Validates station requirements
 * - MORecipeDefinitionRow.h - Recipes with station requirements
 *
 * TESTING CHECKLIST:
 * [ ] Station builds from ghost correctly
 * [ ] Interaction opens crafting menu
 * [ ] Fuel system consumes and depletes
 * [ ] Audio/visual effects play when active
 * [ ] Save/load preserves fuel and inventory
 * [ ] Recipes filter by station type
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOBuildableActor.h"
#include "MORecipeDefinitionRow.h"
#include "MOInventoryHolderInterface.h"
#include "MOMaterialSourceInterface.h"
#include "MOAmbientEnvironmentProvider.h"
#include "MOCraftingStationActor.generated.h"

class UMOInventoryComponent;
class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
UCLASS()
class MOFRAMEWORK_API AMOCraftingStationActor : public AMOBuildableActor,
	public IMOInventoryHolderInterface,
	public IMOMaterialSourceInterface,
	public IMOLocalHeatSource
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

	/** Point light for active station (fire glow, etc.). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|CraftingStation|Visual")
	TObjectPtr<UPointLightComponent> ActiveLightComponent;

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

	/** Base intensity for the active light (before flicker). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Visual", meta=(ClampMin="0.0"))
	float LightBaseIntensity = 5000.0f;

	/** How much the light intensity varies (0.0-1.0, e.g., 0.3 = +/-30% variation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Visual", meta=(ClampMin="0.0", ClampMax="1.0"))
	float LightFlickerAmount = 0.25f;

	/** Speed of the flicker effect (higher = faster flicker). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Visual", meta=(ClampMin="0.1", ClampMax="20.0"))
	float LightFlickerSpeed = 8.0f;

	/** Light color (warm orange for fire). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Visual")
	FLinearColor LightColor = FLinearColor(1.0f, 0.6f, 0.2f);

	/** Light attenuation radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Visual", meta=(ClampMin="0.0"))
	float LightRadius = 500.0f;

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

	/** Fuel units one fuel item is worth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Fuel", meta=(ClampMin="0.01"))
	float FuelPerItemUnit = 10.0f;

	// ============================================================================
	// HEAT EMISSION (IMOLocalHeatSource)
	// ============================================================================

	/** Temperature boost at the fire itself, °C above ambient. 0 = this station
	 *  emits no heat (workbench, loom). Fire stations get type defaults in
	 *  InitializeBuilding; override per instance if a recipe needs to. Heat only
	 *  radiates while IsStationActive() — an unlit or unfueled fire warms nobody. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Heat", meta=(ClampMin="0.0"))
	float HeatOutputCelsius = 0.0f;

	/** Distance the warmth reaches (cm); linear falloff to zero at this range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|CraftingStation|Heat", meta=(ClampMin="0.0"))
	float HeatRadiusCm = 0.0f;

	// IMOLocalHeatSource
	virtual FVector GetHeatLocation() const override { return GetActorLocation(); }
	virtual float GetHeatDeltaCelsius() const override { return HeatOutputCelsius; }
	virtual float GetHeatRadius() const override { return HeatRadiusCm; }
	virtual bool IsHeatActive() const override { return HeatOutputCelsius > 0.0f && IsStationActive(); }

	// ============================================================================
	// FUEL SYSTEM
	// ============================================================================

	/**
	 * Feed up to Quantity fuel items. Only as many as fit under MaxFuel are
	 * accepted; returns the COUNT actually consumed so the caller removes only
	 * those and leaves the rest in the source inventory. Transactional on
	 * purpose — the old "return fuel added" contract let callers destroy whole
	 * unburned items when the tank was near full (matter vanishing, Principle 11).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|Fuel")
	int32 AddFuel(FName ItemDefinitionId, int32 Quantity);

	/** Pure core: whole fuel items that fit under MaxFuel (never overfills, so
	 *  no fractional item is destroyed). Static for headless testing. */
	static int32 ComputeAcceptedFuelItems(float CurrentFuel, float MaxFuel, float FuelPerItem, int32 Offered);

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

	/** Activate light with flicker. */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|Visual")
	void ActivateLight();

	/** Deactivate light. */
	UFUNCTION(BlueprintCallable, Category="MO|CraftingStation|Visual")
	void DeactivateLight();

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	/** Accumulated time for flicker calculation. */
	float FlickerTimeAccumulator = 0.0f;

	/** Update light flicker effect. Called from Tick when active. */
	void UpdateLightFlicker(float DeltaTime);
};
