/**
 * =============================================================================
 * MOItemComponent.h - Per-Item Instance Data Component
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Component attached to item actors that holds instance-specific data.
 * References item definition from DataTable via ItemDefinitionId. Manages
 * quantity, stack size, and world presence state for pickup/drop behavior.
 *
 * KEY RESPONSIBILITIES:
 * 1. Store ItemDefinitionId referencing DT_Items row
 * 2. Track Quantity for stackable items
 * 3. Manage bWorldItemActive state for pickup visibility
 * 4. Handle GiveToInteractorInventory for pickup action
 *
 * ARCHITECTURE NOTES:
 * - ItemDefinitionId is replicated for multiplayer
 * - bWorldItemActive controls visibility without destroying actor
 * - Uses OnRep_ functions for client-side state changes
 * - Delegates notify listeners of state changes
 *
 * CRITICAL PATTERNS:
 * 1. Item Pickup Flow:
 *    Interact -> GiveToInteractorInventory() -> Find inventory
 *    -> Add to inventory -> SetWorldItemActive(false) -> Hide mesh
 *
 * 2. Definition Options:
 *    GetItemDefinitionOptions() provides dropdown list for editor
 *    -> Reads from MOItemDatabaseSettings DataTable
 *
 * KNOWN PITFALLS:
 * 1. DEFINITION ID SYNC: ItemDefinitionId must exist in DataTable.
 *    Invalid IDs will fail silently. Validate in BeginPlay if needed.
 *
 * 2. QUANTITY VS STACK: Quantity can exceed MaxStackSize if set manually.
 *    GiveToInteractorInventory handles splitting, but direct access doesn't.
 *
 * 3. WORLD ACTIVE STATE: Setting bWorldItemActive false hides but doesn't
 *    destroy. Item remains in memory. Use Destroy() for permanent removal.
 *
 * RELATED FILES:
 * - MOWorldItem.h - Actor class that owns this component
 * - MOInventoryComponent.h - Target for pickup operations
 * - MOItemDefinitionRow.h - DataTable row struct
 * - MOItemDatabaseSettings.h - Project settings for DataTable path
 *
 * TESTING CHECKLIST:
 * [ ] ItemDefinitionId dropdown shows DataTable entries
 * [ ] Pickup adds item to inventory correctly
 * [ ] bWorldItemActive false hides item mesh
 * [ ] Quantity replicates to clients
 * [ ] OnItemDefinitionIdChanged fires on change
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOItemComponent.generated.h"

class UMOIdentityComponent;
class UMOInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOItemDefinitionIdChangedSignature, FName, NewItemDefinitionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOWorldItemActiveChangedSignature, bool, bIsActive);


UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOItemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOItemComponent();

	// Static item identifier - references row in item database DataTable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_ItemDefinitionId, Category="MO|Item", meta=(GetOptions="GetItemDefinitionOptions"))
	FName ItemDefinitionId = NAME_None;

	// Returns all available ItemDefinitionIds from the configured DataTable (for dropdown).
	UFUNCTION()
	TArray<FName> GetItemDefinitionOptions() const;

	// Current quantity represented by this world item instance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Item", meta=(ClampMin="1"))
	int32 Quantity = 1;

	// For stacking rules. If <= 1 then not stackable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Item", meta=(ClampMin="1"))
	int32 MaxStackSize = 1;

	// Replicated world presence state so clients see picked-up items disappear without destroying the actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_WorldItemActive, Category="MO|Item")
	bool bWorldItemActive = true;
	
	UPROPERTY(BlueprintAssignable, Category="MO|Item")
	FMOItemDefinitionIdChangedSignature OnItemDefinitionIdChanged;

	UPROPERTY(BlueprintAssignable, Category="MO|Item")
	FMOWorldItemActiveChangedSignature OnWorldItemActiveChanged;

	UFUNCTION(BlueprintPure, Category="MO|Item")
	bool IsWorldItemActive() const { return bWorldItemActive; }

	// Called on the server (typically from MOInteractableComponent::HandleInteract) to give item to inventory.
	UFUNCTION(BlueprintCallable, Category="MO|Item")
	bool GiveToInteractorInventory(AController* InteractorController);

	// Toggle active state in world (server sets, clients follow via replication).
	UFUNCTION(BlueprintCallable, Category="MO|Item")
	void SetWorldItemActive(bool bNewWorldItemActive);

	UFUNCTION()
	void OnRep_ItemDefinitionId();

	

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UMOIdentityComponent* FindIdentityComponent() const;
	UMOInventoryComponent* FindInventoryComponentForController(AController* InteractorController) const;

	void ApplyWorldItemActiveState();

	UFUNCTION()
	void OnRep_WorldItemActive();
};
