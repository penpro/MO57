#pragma once

#include "CoreMinimal.h"
#include "MOUIControllerBase.h"
#include "MOInventoryUIController.generated.h"

class UMOInventoryMenu;
class UMOUnifiedInventoryMenu;
class UMOItemContextMenu;
class UMOInventoryComponent;
class AMOWorldItem;

/**
 * Specialized UI controller for inventory-related UI.
 *
 * Handles:
 * - Basic inventory menu
 * - Unified inventory menu (with container support)
 * - Item context menu (right-click actions)
 * - Nearby world items query and looting
 * - Quick transfer between inventories
 * - Item dropping
 *
 * This controller is extracted from MOUIManagerComponent to reduce its size
 * and provide clear ownership of the inventory UI subsystem.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOInventoryUIController : public UMOUIControllerBase
{
	GENERATED_BODY()

public:
	UMOInventoryUIController();

	// ==========================================================================
	// INVENTORY MENU
	// ==========================================================================

	/** Toggle inventory menu visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void ToggleInventoryMenu();

	/** Open the inventory menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void OpenInventoryMenu();

	/** Close the inventory menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void CloseInventoryMenu();

	/** Check if inventory menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory")
	bool IsInventoryMenuOpen() const;

	// ==========================================================================
	// UNIFIED INVENTORY / CONTAINER SUPPORT
	// ==========================================================================

	/** Open the unified inventory menu with an optional container. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void OpenInventoryWithContainer(AActor* ContainerActor);

	/** Set the active container for the unified inventory. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void SetActiveContainer(AActor* ContainerActor);

	/** Clear the active container. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void ClearActiveContainer();

	/** Get the active container (may be null). */
	UFUNCTION(BlueprintPure, Category="MO|Inventory")
	AActor* GetActiveContainer() const;

	/** Check if there's an active container. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory")
	bool HasActiveContainer() const;

	// ==========================================================================
	// ITEM CONTEXT MENU
	// ==========================================================================

	/** Show the item context menu at the given screen position. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void ShowItemContextMenu(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid, int32 SlotIndex, FVector2D ScreenPosition);

	/** Close the item context menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void CloseItemContextMenu();

	/** Check if item context menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory")
	bool IsItemContextMenuOpen() const;

	// ==========================================================================
	// NEARBY WORLD ITEMS
	// ==========================================================================

	/** Query for nearby world items that can be picked up. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	TArray<AMOWorldItem*> QueryNearbyWorldItems() const;

	/** Loot all nearby world items. Returns the count of looted items. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	int32 LootAllNearbyItems();

	/** Get the radius for nearby item queries. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory")
	float GetNearbyItemsQueryRadius() const { return NearbyItemsQueryRadius; }

	/** Set the radius for nearby item queries. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void SetNearbyItemsQueryRadius(float NewRadius) { NearbyItemsQueryRadius = FMath::Max(0.0f, NewRadius); }

	// ==========================================================================
	// ITEM TRANSFER & DROP
	// ==========================================================================

	/** Quick transfer an item between player inventory and container. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void HandleQuickTransfer(const FGuid& ItemGuid, UMOInventoryComponent* SourceInventory);

	/** Drop an item from inventory into the world. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void DropItemToWorldByGuid(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// --- Inventory Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOInventoryMenu> InventoryMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 InventoryMenuZOrder = 50;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOInventoryMenu> InventoryMenuWidget;

	UFUNCTION()
	void HandleInventoryMenuRequestClose();

	UFUNCTION()
	void HandleInventoryMenuSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition);

	// --- Unified Inventory Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOUnifiedInventoryMenu> UnifiedInventoryMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 UnifiedInventoryMenuZOrder = 50;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOUnifiedInventoryMenu> UnifiedInventoryWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentContainerActor;

	UFUNCTION()
	void HandleUnifiedInventoryMenuRequestClose();

	UFUNCTION()
	void HandleUnifiedInventoryMenuContextMenuRequested(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid, int32 SlotIndex, FVector2D ScreenPosition);

	// --- Item Context Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOItemContextMenu> ItemContextMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 ItemContextMenuZOrder = 150;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOItemContextMenu> ItemContextMenuWidget;

	UFUNCTION()
	void HandleContextMenuClosed();

	UFUNCTION()
	void HandleContextMenuAction(FName ActionId, const FGuid& ItemGuid);

	// --- Nearby Items ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(ClampMin="0", AllowPrivateAccess="true"))
	float NearbyItemsQueryRadius = 400.0f;

	UFUNCTION()
	void HandleLootAllNearby();
};
