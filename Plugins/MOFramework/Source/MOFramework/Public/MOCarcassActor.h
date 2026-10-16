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
 * WORKFLOW (Oct 16 2026: server-driven, co-op safe -- it used to run in the client's UI, so a joiner's butchering never progressed):
 * 1. Creature dies -> SpawnCarcass() creates AMOCarcassActor (CarcassDefinition replicates: clients list the parts too)
 * 2. Right-click -> IMOUIContractInterface::RequestShowWorldActionMenu: the owning client lists GetWorldActions (every part
 *    that can still be cut, "Butcher everything" first; a part needing a knife the pawn lacks is greyed with the reason)
 *    E -> cuts the first part the pawn can cut (or opens the menu when it can cut none, so the reason shows)
 * 3. A choice -> FMOWorldActions -> PerformWorldAction (server) -> the PAWN's UMOTimedActionComponent: replicated progress bar,
 *    cancelled by moving or being hurt. "Butcher everything" chains the parts, one timed cut after another.
 * 4. Complete -> the item goes to the pawn's inventory (on the ground by the carcass when it is full), part marked harvested
 * 5. Repeat until all parts harvested (the carcass goes) or they decay. Two pawns may cut two different parts at once.
 *
 * RELATED FILES:
 * - MOCarcassTypes.h - FMOCarcassPartEntry, EMOCarcassDecayStage
 * - MOCarcassDefinitionRow.h - DataTable definition
 * - MOCreature.h - Spawns carcass on death
 *
 * LAST UPDATED: 2026-10-16
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOCarcassTypes.h"
#include "MOWorldActionProvider.h"
#include "MOCarcassActor.generated.h"

class UMOInteractableComponent;
class UMOIdentityComponent;
class UMOInventoryComponent;
class UPoseableMeshComponent;
class UStaticMeshComponent;
class USphereComponent;
struct FMOCarcassDefinitionRow;

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnCarcassPartHarvested, FName, PartId, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnCarcassDecayChanged, EMOCarcassDecayStage, NewStage);

/**
 * Actor representing a dead creature's remains.
 * Decays over time and can be butchered for parts.
 */
UCLASS()
class MOFRAMEWORK_API AMOCarcassActor : public AActor, public IMOWorldActionProvider
{
	GENERATED_BODY()

public:
	AMOCarcassActor();

	// ============================================================================
	// COMPONENTS
	// ============================================================================

	/** Poseable mesh for displaying carcass (allows manual bone positioning). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Carcass|Components")
	TObjectPtr<UPoseableMeshComponent> CarcassMesh;

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

	/** Carcass definition from DataTable. Replicated: a client's menu lists the parts from it (set at spawn, before the first replication). */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category="MO|Carcass")
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

	/** Get current decay stage. */
	UFUNCTION(BlueprintPure, Category="MO|Carcass|State")
	EMOCarcassDecayStage GetDecayStage() const { return DecayStage; }

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
	 * Whether this pawn can cut PartId now (any machine): the part is there and fresh enough, nobody else is cutting it (known on the server only),
	 * and the pawn carries the tool (RequiredToolType, any carried item with that tool capability) and container the part needs. OutReason says why not.
	 */
	bool CanPawnCutPart(const APawn* Pawn, FName PartId, FText& OutReason) const;

	/** The world-action id of "Butcher everything" (the part ids are the other actions). */
	static const FName ButcherAllAction;

	// IMOWorldActionProvider
	virtual FText GetWorldActionTitle() const override;
	virtual void GetWorldActions(const APawn* Pawn, TArray<FMOItemActionOption>& Out) const override;
	virtual bool PerformWorldAction(APawn* Pawn, FName ActionId) override;

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

	/** Fired on the server when a part is cut and given. */
	UPROPERTY(BlueprintAssignable, Category="MO|Carcass|Events")
	FMOOnCarcassPartHarvested OnPartHarvested;

	/** Fired when decay stage changes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Carcass|Events")
	FMOOnCarcassDecayChanged OnDecayChanged;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// ============================================================================
	// INTERNAL STATE
	// ============================================================================

	/** SERVER: the parts being cut right now, and by whom (one pawn per part; a pawn cuts one part at a time). */
	TMap<FName, TWeakObjectPtr<APawn>> PartsInHand;

	// ============================================================================
	// INTERNAL METHODS
	// ============================================================================

	/** SERVER: start the pawn's timed cut of PartId; bThenTheRest chains the next cuttable part when it is done ("Butcher everything"). */
	bool BeginCut(APawn* Pawn, FName PartId, bool bThenTheRest);

	/** SERVER: the first part this pawn can cut, or NAME_None. */
	FName FirstCuttablePart(const APawn* Pawn) const;

	/** SERVER: the timed cut is done -- give the item, mark the part, go when nothing is left. */
	void FinishCut(APawn* Pawn, FName PartId);

	/** Handle primary interaction (butcher menu or start harvest). */
	bool HandleInteract(AController* InteractorController);

	/** Handle secondary interaction (context menu). */
	bool HandleSecondaryInteract(AController* InteractorController);

	/** Update decay stage based on time. */
	void UpdateDecayStage();

	/** Update visual appearance for decay stage. */
	void UpdateVisualForDecayStage();

};
