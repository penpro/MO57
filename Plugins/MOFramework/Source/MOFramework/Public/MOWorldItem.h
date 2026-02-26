/**
 * =============================================================================
 * MOWorldItem.h - Physical Item Actor in World
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Actor class representing physical items in the world. Combines identity,
 * item data, interaction, and mesh display. Supports pickup, drop physics,
 * and serving as material source for crafting.
 *
 * KEY RESPONSIBILITIES:
 * 1. Display item mesh based on ItemDefinitionId
 * 2. Handle interaction for pickup via MOInteractableComponent
 * 3. Implement drop physics (enable physics, settle, disable)
 * 4. Implement IMOMaterialSourceInterface for crafting material gathering
 * 5. Implement IMOIdentifiableInterface for GUID-based persistence
 *
 * ARCHITECTURE NOTES:
 * - SceneRoot -> ItemMesh (StaticMesh) + InteractionSphere (collision)
 * - IdentityComponent provides GUID for save/load
 * - ItemComponent stores ItemDefinitionId, Quantity
 * - InteractableComponent handles interaction events
 * - Drop physics uses mesh simulation with timeout/rest detection
 *
 * CRITICAL PATTERNS:
 * 1. Pickup Flow:
 *    Player interacts -> OnHandleInteract() -> ItemComponent.GiveToInteractorInventory()
 *    -> bAddToInventoryOnInteract -> bHideOnPickup/bDestroyAfterPickup
 *
 * 2. Drop Physics:
 *    EnableDropPhysics() -> Simulate -> Tick checks velocity
 *    -> Below RestVelocityThreshold or timeout -> SettleOnGround()
 *
 * 3. Mesh Update:
 *    ItemDefinitionId changes -> HandleItemDefinitionIdChanged()
 *    -> ApplyItemDefinitionToWorldMesh() -> Load mesh from DataTable
 *
 * KNOWN PITFALLS:
 * 1. MESH LOADING: ApplyItemDefinitionToWorldMesh() loads synchronously.
 *    For many items spawning at once, consider async loading.
 *
 * 2. PHYSICS TIMEOUT: DropPhysicsTimeout (3s default) may not be enough
 *    for items falling long distances. Adjust per use case.
 *
 * 3. INTERACTION SPHERE: InteractionSphereRadius (60u) affects pickup ease.
 *    Too small = frustrating, too large = picking up wrong items.
 *
 * 4. DESTROY VS HIDE: bDestroyAfterPickup = true destroys actor.
 *    bHideOnPickup = true only hides. For respawning items, use hide.
 *
 * RELATED FILES:
 * - MOItemComponent.h - Item data component
 * - MOInteractableComponent.h - Interaction handling
 * - MOIdentityComponent.h - GUID identity
 * - MOWorldItemFactory.h - Spawns world items
 * - MOItemDefinitionRow.h - DataTable row with mesh reference
 *
 * TESTING CHECKLIST:
 * [ ] Item mesh displays correctly from DataTable
 * [ ] Pickup adds to inventory and hides/destroys item
 * [ ] Drop physics enables and settles correctly
 * [ ] GUID persists across save/load
 * [ ] Material source interface returns correct materials
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOInteractableComponent.h"
#include "MOMaterialSourceInterface.h"
#include "MOIdentifiableInterface.h"
#include "MOWorldItem.generated.h"

class UDataTable;
struct FMOItemDefinitionRow;


class UMOIdentityComponent;
class UMOItemComponent;
class UMOInteractableComponent;
class UStaticMeshComponent;
class USphereComponent;

UCLASS()
class MOFRAMEWORK_API AMOWorldItem : public AActor,
	public IMOMaterialSourceInterface,
	public IMOIdentifiableInterface
{
	GENERATED_BODY()

public:
	AMOWorldItem();

	virtual void Tick(float DeltaTime) override;

	// Note: GetIdentityComponent() is provided by IMOIdentifiableInterface

	UFUNCTION(BlueprintPure, Category="MO")
	UMOItemComponent* GetItemComponent() const { return ItemComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOInteractableComponent* GetInteractableComponent() const { return InteractableComponent; }

	/** Check if this item can be picked up (not hidden, has valid item component). */
	UFUNCTION(BlueprintPure, Category="MO")
	bool IsPickupable() const;

	UFUNCTION(BlueprintPure, Category="MO")
	UStaticMeshComponent* GetItemMesh() const { return ItemMesh; }

	// Interaction behavior options
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	bool bAddToInventoryOnInteract = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	bool bDestroyAfterPickup = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	bool bHideOnPickup = true;

	/** Apply visuals from the item definition DataTable to the world mesh.
	 *  Call this after setting ItemComponent->ItemDefinitionId to update the mesh. */
	UFUNCTION(BlueprintCallable, Category="MO|Item|Definition")
	bool ApplyItemDefinitionToWorldMesh();

	/** Enable physics for dropped items. Physics will be disabled when the item comes to rest or timeout expires. */
	UFUNCTION(BlueprintCallable, Category="MO|Item|Drop")
	void EnableDropPhysics();

	/** Maximum time physics will be enabled after dropping (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Item|Drop")
	float DropPhysicsTimeout = 3.0f;

	/** Velocity threshold below which the item is considered "at rest". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Item|Drop")
	float RestVelocityThreshold = 5.0f;

	// ============================================================================
	// IMOMaterialSourceInterface IMPLEMENTATION
	// ============================================================================

	virtual bool CanProvideMaterial_Implementation(FName MaterialId, int32 Quantity) const override;
	virtual int32 GatherMaterial_Implementation(FName MaterialId, int32 Quantity) override;
	virtual int32 GetMaterialSourcePriority_Implementation() const override;

	// ============================================================================
	// IMOIdentifiableInterface IMPLEMENTATION
	// ============================================================================

	virtual UMOIdentityComponent* GetIdentityComponent_Implementation() const override;
	virtual FGuid GetPersistentGuid_Implementation() const override;
	virtual bool HasValidIdentity_Implementation() const override;

protected:
	// Override interaction handling
	UFUNCTION()
	bool OnHandleInteract(AController* InteractorController);
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Called when drop physics completes - settles item on ground and disables physics. */
	void SettleOnGround();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	/** DataTable that defines how ItemDefinitionId maps to display name, mesh, icon, etc.
		If empty, the plugin Project Settings database is used. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Item|Definition")
	TSoftObjectPtr<UDataTable> ItemDefinitionsDataTable;

	UFUNCTION()
	void HandleItemDefinitionIdChanged(FName NewItemDefinitionId);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UStaticMeshComponent> ItemMesh;

	/** Collision sphere for interaction traces - easier to hit than the mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<USphereComponent> InteractionSphere;

	/** Radius of the interaction collision sphere. Larger = easier to pick up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Interaction")
	float InteractionSphereRadius = 60.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOIdentityComponent> IdentityComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOItemComponent> ItemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOInteractableComponent> InteractableComponent;

private:
	/** Track if we're currently in drop physics mode. */
	bool bDropPhysicsActive = false;

	/** Time when drop physics was enabled. */
	float DropPhysicsStartTime = 0.0f;
};
