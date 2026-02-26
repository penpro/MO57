/**
 * =============================================================================
 * MOCarcassActor.h - Creature Carcass for Butchering
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * Actor representing a dead creature's remains. Stays in world, decays over
 * time, and can be butchered for meat, bones, organs, hide, etc.
 *
 * FEATURES:
 * - Decay system: Fresh -> Rotting -> Skeletal
 * - Multiple harvestable parts with individual timers
 * - Tool requirements (knife, cleaver)
 * - Container requirements (for blood, bile)
 * - Tracks which parts have been harvested
 * - Persistence via IdentityComponent
 *
 * WORKFLOW:
 * 1. Creature dies -> SpawnCarcass() creates AMOCarcassActor
 * 2. Player interacts -> Shows available parts (context menu or UI)
 * 3. Player selects part -> StartHarvestPart() begins timed harvest
 * 4. Progress updates -> OnHarvestProgress broadcasts
 * 5. Complete -> Part added to inventory, part marked harvested
 * 6. Repeat until all parts harvested or carcass decays
 *
 * RELATED FILES:
 * - MOCarcassTypes.h - FMOCarcassPartEntry, EMOCarcassDecayStage
 * - MOCarcassDefinitionRow.h - DataTable definition
 * - MOCreature.h - Spawns carcass on death
 *
 * LAST UPDATED: 2026-02-26
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOCarcassTypes.h"
#include "MOCarcassActor.generated.h"

class UMOInteractableComponent;
class UMOIdentityComponent;
class UMOInventoryComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USphereComponent;
struct FMOCarcassDefinitionRow;

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnCarcassHarvestProgress, float, Progress, float, TotalTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnCarcassPartHarvested, FName, PartId, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnCarcassDecayChanged, EMOCarcassDecayStage, NewStage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnCarcassHarvestCancelled);

/**
 * Actor representing a dead creature's remains.
 * Decays over time and can be butchered for parts.
 */
UCLASS()
class MOFRAMEWORK_API AMOCarcassActor : public AActor
{
	GENERATED_BODY()

public:
	AMOCarcassActor();

	// ============================================================================
	// COMPONENTS
	// ============================================================================

	/** Skeletal mesh for displaying carcass. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Carcass|Components")
	TObjectPtr<USkeletalMeshComponent> CarcassMesh;

	/** Static mesh for skeletal stage (bones pile). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Carcass|Components")
	TObjectPtr<UStaticMeshComponent> BonesMesh;

	/** Interaction sphere for butchering. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Carcass|Components")
	TObjectPtr<USphereComponent> InteractionSphere;

	/** Interactable component for player interaction. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Carcass|Components")
	TObjectPtr<UMOInteractableComponent> InteractableComponent;

	/** Identity component for persistence. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Carcass|Components")
	TObjectPtr<UMOIdentityComponent> IdentityComponent;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Carcass definition from DataTable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Carcass")
	FDataTableRowHandle CarcassDefinition;

	/** Get the carcass definition row. Returns nullptr if not found. C++ only. */
	const FMOCarcassDefinitionRow* GetCarcassDefinitionRow() const;

	// ============================================================================
	// STATE
	// ============================================================================

	/** Current decay stage. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="MO|Carcass|State")
	EMOCarcassDecayStage DecayStage = EMOCarcassDecayStage::Fresh;

	/** Parts that have been harvested. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="MO|Carcass|State")
	TArray<FName> HarvestedParts;

	/** Time since creature died (seconds). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Carcass|State")
	float TimeSinceDeath = 0.0f;

	/** Check if carcass is being harvested. */
	UFUNCTION(BlueprintPure, Category="MO|Carcass|State")
	bool IsBeingHarvested() const { return !CurrentHarvestPart.IsNone(); }

	/** Get current harvest progress (0-1). */
	UFUNCTION(BlueprintPure, Category="MO|Carcass|State")
	float GetHarvestProgress() const;

	// ============================================================================
	// API
	// ============================================================================

	/**
	 * Get all parts currently available for harvest.
	 * Filters by decay stage and already-harvested parts.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Carcass")
	TArray<FMOCarcassPartEntry> GetAvailableParts() const;

	/**
	 * Check if a specific part can be harvested.
	 * @param PartId The part to check
	 * @param Inventory Player's inventory (for tool/container check)
	 * @param OutReason Reason if cannot harvest
	 * @return True if part can be harvested
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Carcass")
	bool CanHarvestPart(FName PartId, UMOInventoryComponent* Inventory, FString& OutReason) const;

	/**
	 * Start harvesting a part.
	 * @param PartId The part to harvest
	 * @param Harvester Controller of the player harvesting
	 * @return True if harvest started successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Carcass")
	bool StartHarvestPart(FName PartId, AController* Harvester);

	/**
	 * Cancel current harvest operation.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Carcass")
	void CancelHarvest();

	/**
	 * Check if a part has been harvested.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Carcass")
	bool IsPartHarvested(FName PartId) const;

	/**
	 * Initialize from a dead creature.
	 * Called by MOCreature::SpawnCarcass().
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Carcass")
	void InitializeFromCreature(class AMOCreature* DeadCreature);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Fired during harvest with progress info. */
	UPROPERTY(BlueprintAssignable, Category="MO|Carcass|Events")
	FMOOnCarcassHarvestProgress OnHarvestProgress;

	/** Fired when a part is harvested. */
	UPROPERTY(BlueprintAssignable, Category="MO|Carcass|Events")
	FMOOnCarcassPartHarvested OnPartHarvested;

	/** Fired when decay stage changes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Carcass|Events")
	FMOOnCarcassDecayChanged OnDecayChanged;

	/** Fired when harvest is cancelled. */
	UPROPERTY(BlueprintAssignable, Category="MO|Carcass|Events")
	FMOOnCarcassHarvestCancelled OnHarvestCancelled;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// ============================================================================
	// INTERNAL STATE
	// ============================================================================

	/** Part currently being harvested. */
	FName CurrentHarvestPart;

	/** Progress on current harvest (seconds). */
	float HarvestProgress = 0.0f;

	/** Total time for current harvest. */
	float HarvestTotalTime = 0.0f;

	/** Controller doing the harvesting. */
	TWeakObjectPtr<AController> HarvesterController;

	// ============================================================================
	// INTERNAL METHODS
	// ============================================================================

	/** Handle primary interaction (butcher menu or start harvest). */
	bool HandleInteract(AController* InteractorController);

	/** Handle secondary interaction (context menu). */
	bool HandleSecondaryInteract(AController* InteractorController);

	/** Update decay stage based on time. */
	void UpdateDecayStage();

	/** Update visual appearance for decay stage. */
	void UpdateVisualForDecayStage();

	/** Tick harvest progress. */
	void TickHarvest(float DeltaTime);

	/** Complete current harvest, give items. */
	void CompleteHarvest();

	/** Check if harvester is still in range. */
	bool IsHarvesterInRange() const;

	/** Max range from carcass while harvesting. */
	static constexpr float MaxHarvestRange = 300.0f;
};
