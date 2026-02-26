/**
 * =============================================================================
 * MOBuildableActor.h - Base Class for Placeable Buildings
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] GHOST STATE: Ghost uses translucent material via SetGhostMode(true).
 *   Ghost buildings are interactable but not functional. SetGhostVisual(Color)
 *   sets tint (green=valid, red=invalid placement). Ghost material created from
 *   GhostMaterialBase soft reference - ensure asset path is valid.
 *
 * [2024-02] IDENTITY COMPONENT: All buildables have MOIdentityComponent for
 *   persistence. GUID assigned on spawn in IdentityComponent::BeginPlay().
 *   Don't access GetPersistentGuid() in constructor - returns invalid GUID.
 *
 * [2024-02] COMPONENT INIT ORDER: In constructor, components created as:
 *   RootSceneComponent -> MeshComponent -> IdentityComponent ->
 *   InteractableComponent -> BuildProgressComponent. In BeginPlay(),
 *   InteractableComponent.OnHandleInteract bound to HandleInteract().
 *   BuildProgressComponent.OnConstructionCompleted bound to OnConstructionCompleted.
 *
 * [2024-02] RECIPE ID CONTRACT: RecipeId must be valid row name in building
 *   recipes DataTable (see MOBuildingTypes.h for FMOBuildingRecipeRow).
 *   InitializeBuilding(RecipeId) sets this. If invalid, BuildProgressComponent
 *   won't have material requirements and construction will instant-complete.
 *
 * [2024-02] SUBCLASS OVERRIDE PATTERN: When subclassing:
 *   - Override OnGhostInteracted_Implementation() for custom ghost interaction
 *     (default shows build widget via player controller)
 *   - Override OnCompleteInteracted_Implementation() for completed building use
 *     (default does nothing - subclass must implement)
 *   - Override OnConstructionCompleted_Implementation() to init functionality
 *     (e.g., crafting stations enable crafting queue here)
 *   Always call Super:: version to maintain base behavior.
 *
 * [2024-02] MATERIAL RESTORATION: SaveOriginalMaterials() caches mesh materials
 *   in OriginalMaterials array. RestoreOriginalMaterials() applies them back.
 *   Called automatically on SetGhostMode(false) and SetCompletedVisual().
 *   If original materials array empty, no restoration occurs (stays ghost).
 *
 * [2024-02] STATE MACHINE TRANSITIONS: Only valid transitions are:
 *   Ghost -> Constructing (via StartConstruction on BuildProgressComponent)
 *   Constructing <-> Paused (via Pause/Resume)
 *   Constructing -> Complete (auto when progress reaches 100%)
 *   Check GetBuildState() before attempting state-dependent operations.
 *
 * [2024-02] COLLISION VALIDATION: IsOverlappingBlockingActors() used for
 *   placement validation. Returns true if overlapping actors with blocking
 *   collision. Ghost mode sets collision to Overlap, Complete sets to Block.
 *
 * =============================================================================
 * RELATED FILES: MOBuildingComponent.h, MOBuildProgressComponent.h, MOBuildingTypes.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOBuildingTypes.h"
#include "MOIdentifiableInterface.h"

#include "MOBuildableActor.generated.h"

class UStaticMeshComponent;
class UMOIdentityComponent;
class UMOInteractableComponent;
class UMOBuildProgressComponent;
class UMaterialInstanceDynamic;

/**
 * =============================================================================
 * AMOBuildableActor - Base Class for Placeable Buildings
 * =============================================================================
 *
 * PURPOSE:
 * Base actor for all placeable buildings in the game. Handles the full
 * lifecycle from ghost preview through construction to completed building.
 *
 * -----------------------------------------------------------------------------
 * COMPONENT COMPOSITION
 * -----------------------------------------------------------------------------
 *
 *   USceneComponent (RootSceneComponent)
 *     Root component for transform hierarchy
 *
 *   UStaticMeshComponent (MeshComponent)
 *     Visual mesh, attached to root
 *     Materials changed for ghost/construction/complete states
 *
 *   UMOIdentityComponent (IdentityComponent)
 *     Provides unique GUID for persistence
 *     See MOIdentityComponent.h
 *
 *   UMOInteractableComponent (InteractableComponent)
 *     Enables player interaction with building
 *     OnHandleInteract delegate bound to HandleInteract()
 *     See MOInteractableComponent.h
 *
 *   UMOBuildProgressComponent (BuildProgressComponent)
 *     Tracks construction progress
 *     Manages timed building with weighted parts
 *     See MOBuildProgressComponent.h
 *
 * -----------------------------------------------------------------------------
 * STATE MACHINE (EMOBuildState)
 * -----------------------------------------------------------------------------
 *
 *   Ghost -> Constructing -> Complete
 *     |         |
 *     v         v
 *   (cancelled) Paused <-> Constructing
 *
 * GHOST:
 *   - Building placed but construction not started
 *   - Translucent ghost material applied
 *   - Interaction opens build widget to configure and start
 *
 * CONSTRUCTING:
 *   - Active timed construction in progress
 *   - Materials being consumed progressively
 *   - Can transition to Paused if materials unavailable
 *
 * PAUSED:
 *   - Construction paused (manual or material shortage)
 *   - Can resume when conditions met
 *
 * COMPLETE:
 *   - Building fully constructed and functional
 *   - Original materials restored
 *   - Interaction depends on building type (override OnCompleteInteracted)
 *
 * -----------------------------------------------------------------------------
 * INTERACTION FLOW
 * -----------------------------------------------------------------------------
 *
 * 1. Player looks at building (within interaction range)
 * 2. Player presses interact key
 * 3. Interaction system calls InteractableComponent->ServerInteract()
 * 4. InteractableComponent calls bound OnHandleInteract delegate
 * 5. Our HandleInteract() is called
 * 6. Based on state:
 *    - Ghost/Paused: OnGhostInteracted() -> Show build widget
 *    - Constructing: Currently just logs (could show progress)
 *    - Complete: OnCompleteInteracted() -> Override in subclasses
 *
 * -----------------------------------------------------------------------------
 * DELEGATE CONNECTIONS
 * -----------------------------------------------------------------------------
 *
 * In BeginPlay():
 *   InteractableComponent->OnHandleInteract.BindUObject(this, &HandleInteract)
 *     -> Routes interaction to our state-based handler
 *
 *   BuildProgressComponent->OnConstructionCompleted.AddDynamic(this, &OnConstructionCompleted)
 *     -> Updates visuals when construction finishes
 *
 * -----------------------------------------------------------------------------
 * SUBCLASSING
 * -----------------------------------------------------------------------------
 *
 * Override these for specific building behavior:
 *   - OnGhostInteracted_Implementation: Custom UI for ghost state
 *   - OnCompleteInteracted_Implementation: Custom behavior when used
 *   - SetConstructionVisual: Custom construction appearance
 *   - SetCompletedVisual: Custom final appearance
 *
 * Subclasses:
 *   - AMOCraftingStationActor: Campfire, forge, etc. (adds crafting)
 *   - AMOContainerActor: Storage containers (adds inventory)
 *
 * -----------------------------------------------------------------------------
 * PERSISTENCE
 * -----------------------------------------------------------------------------
 *
 * Buildings are saved via FMOPersistedBuildingRecord:
 *   - BuildingGuid: From IdentityComponent
 *   - Transform: World location/rotation/scale
 *   - ActorClassPath: For spawning correct class on load
 *   - RecipeId: For initialization
 *   - Progress: Full construction progress state
 *
 * Save: BuildSaveData() populates record
 * Load: ApplySaveData() restores state after spawn
 *
 * =============================================================================
 */
UCLASS()
class MOFRAMEWORK_API AMOBuildableActor : public AActor, public IMOIdentifiableInterface
{
	GENERATED_BODY()

public:
	AMOBuildableActor();

	// ============================================================================
	// COMPONENTS
	// ============================================================================

	/** Root scene component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Building|Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	/** Visual mesh component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Building|Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Identity component for GUID tracking. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Building|Components")
	TObjectPtr<UMOIdentityComponent> IdentityComponent;

	/** Interactable component for player interaction. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Building|Components")
	TObjectPtr<UMOInteractableComponent> InteractableComponent;

	/** Build progress component (tracks construction). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Building|Components")
	TObjectPtr<UMOBuildProgressComponent> BuildProgressComponent;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Base ghost material (translucent, unlit). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Visual")
	TSoftObjectPtr<UMaterialInterface> GhostMaterialBase;

	/** Original materials from the mesh (saved for restoration). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;

	// ============================================================================
	// STATE
	// ============================================================================

	/**
	 * Get the recipe ID for this building.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	FName GetRecipeId() const { return RecipeId; }

	/**
	 * Get the current build state.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	EMOBuildState GetBuildState() const;

	/**
	 * Check if this building is in ghost mode.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	bool IsGhostMode() const { return bIsGhost; }

	/**
	 * Check if this building is fully constructed.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	bool IsComplete() const;

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize this building with a recipe.
	 * Called when the ghost is placed and becomes an actual building.
	 * @param InRecipeId - The recipe ID for this building
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	virtual void InitializeBuilding(FName InRecipeId);

	/**
	 * Set ghost mode on/off.
	 * @param bGhost - If true, enables ghost mode (placement preview)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	virtual void SetGhostMode(bool bGhost);

	/**
	 * Set the ghost visual (color tint for valid/invalid placement).
	 * @param Color - The color to apply to the ghost material
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	virtual void SetGhostVisual(const FLinearColor& Color);

	// ============================================================================
	// VISUAL STATES
	// ============================================================================

	/**
	 * Update visuals to match construction progress.
	 * @param Progress - 0.0 to 1.0 construction progress
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|Visual")
	virtual void SetConstructionVisual(float Progress);

	/**
	 * Set visuals to fully completed state.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|Visual")
	virtual void SetCompletedVisual();

	// ============================================================================
	// COLLISION
	// ============================================================================

	/**
	 * Check if this actor is overlapping any blocking actors.
	 * Used for placement validation.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	bool IsOverlappingBlockingActors() const;

	// ============================================================================
	// INTERACTION
	// ============================================================================

	/**
	 * Handle interaction with this building.
	 * Called by the interactable component.
	 * @param Controller - The controller that interacted
	 * @return True if the interaction was handled
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	bool HandleInteract(AController* Controller);

	/**
	 * Handle secondary interaction (right-click) with this building.
	 * Called by the interactable component.
	 * @param Controller - The controller that interacted
	 * @return True if the interaction was handled
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	virtual bool HandleSecondaryInteract(AController* Controller);

	/**
	 * Get the interaction text for this building.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	FText GetInteractionText() const;

	/**
	 * Check if this building can be interacted with.
	 * @param Controller - The controller attempting to interact
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	bool CanInteract(AController* Controller) const;

	// ============================================================================
	// SAVE/LOAD
	// ============================================================================

	/**
	 * Build save data for this building.
	 * @param OutRecord - The record to populate
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|Save")
	virtual void BuildSaveData(FMOPersistedBuildingRecord& OutRecord) const;

	/**
	 * Apply saved data to this building.
	 * @param InRecord - The record to apply
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|Save")
	virtual void ApplySaveData(const FMOPersistedBuildingRecord& InRecord);

	// ============================================================================
	// IMOIdentifiableInterface IMPLEMENTATION
	// ============================================================================

	virtual UMOIdentityComponent* GetIdentityComponent_Implementation() const override;
	virtual FGuid GetPersistentGuid_Implementation() const override;
	virtual bool HasValidIdentity_Implementation() const override;

protected:
	// ============================================================================
	// OVERRIDES
	// ============================================================================

	virtual void BeginPlay() override;

	// ============================================================================
	// EVENTS
	// ============================================================================

	/**
	 * Called when construction completes.
	 * Override in subclasses to add completion behavior.
	 */
	UFUNCTION(BlueprintNativeEvent, Category="MO|Building")
	void OnConstructionCompleted();
	virtual void OnConstructionCompleted_Implementation();

	/**
	 * Called when this building is interacted with while in ghost state.
	 * Override to show build widget.
	 * @param Controller - The controller that interacted
	 */
	UFUNCTION(BlueprintNativeEvent, Category="MO|Building")
	void OnGhostInteracted(AController* Controller);
	virtual void OnGhostInteracted_Implementation(AController* Controller);

	/**
	 * Called when this building is interacted with while complete.
	 * Override in subclasses for specific behavior.
	 * @param Controller - The controller that interacted
	 */
	UFUNCTION(BlueprintNativeEvent, Category="MO|Building")
	void OnCompleteInteracted(AController* Controller);
	virtual void OnCompleteInteracted_Implementation(AController* Controller);

private:
	// ============================================================================
	// STATE
	// ============================================================================

	/** Whether currently in ghost mode. */
	bool bIsGhost = false;

	/** Recipe ID for this building. */
	UPROPERTY()
	FName RecipeId = NAME_None;

	/** Dynamic material instance for ghost effect. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GhostMaterialInstance;

	// ============================================================================
	// INTERNAL
	// ============================================================================

	/** Create the ghost material instance. */
	void CreateGhostMaterial();

	/** Save original materials from mesh. */
	void SaveOriginalMaterials();

	/** Restore original materials to mesh. */
	void RestoreOriginalMaterials();
};
