/**
 * =============================================================================
 * MOInventoryUIController.h - Inventory Menus and Item Operations UI
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Specialized UI controller for inventory management interfaces. Handles
 * inventory menus, item context menus, container interaction, nearby items,
 * and ground foraging menus. Part of the UIManager split architecture.
 *
 * KEY RESPONSIBILITIES:
 * 1. Manage Inventory and Unified Inventory menus
 * 2. Show item context menus for inventory/world items
 * 3. Show ground context menus for foraging actions
 * 4. Handle nearby world item queries and looting
 * 5. Manage quick transfer between player and container
 *
 * UI WIDGETS MANAGED:
 * - InventoryMenu: Basic inventory grid
 * - UnifiedInventoryMenu: Player + container side-by-side
 * - ItemContextMenu: Right-click actions (drop, inspect, consume)
 * - GroundContextMenu: Foraging actions (search, dig)
 *
 * CONTAINER INTERACTION FLOW:
 * Player interacts with container -> OpenInventoryWithContainer(Actor)
 * -> SetActiveContainer() -> Show UnifiedInventoryMenu
 * -> Player transfers items -> Close menu -> ClearActiveContainer()
 *
 * CRITICAL PATTERNS:
 * 1. Context Menu Sources:
 *    ShowItemContextMenu() - from inventory slot
 *    ShowWorldItemContextMenu() - from nearby items panel
 *    Track source for action handling
 *
 * 2. Quick Transfer:
 *    HandleQuickTransfer(Guid, SourceInventory)
 *    -> If source is player: transfer to container
 *    -> If source is container: transfer to player
 *
 * 3. Nearby Items:
 *    QueryNearbyWorldItems() -> Sphere overlap query
 *    LootAllNearbyItems() -> Add all to inventory
 *    NearbyItemsQueryRadius configurable
 *
 * KNOWN PITFALLS:
 * 1. CONTAINER WEAK REFERENCE: CurrentContainerActor is weak pointer.
 *    Container can be destroyed while menu is open - check validity.
 *
 * 2. WORLD ITEM CONTEXT: ContextMenuWorldItem tracks world item target.
 *    Must clear when context menu closes to avoid stale reference.
 *
 * 3. FORAGING COMPLETION: HandleForagingSearchComplete/DigComplete receive
 *    item arrays. Items may spawn at player feet - avoid overlap.
 *
 * 4. INVENTORY FULL: Transfer/loot operations may fail if inventory full.
 *    Show notification to player.
 *
 * RELATED FILES:
 * - MOUIControllerBase.h - Base class with shared utilities
 * - MOInventoryMenu.h - Basic inventory widget
 * - MOUnifiedInventoryMenu.h - Dual inventory widget
 * - MOItemContextMenu.h - Item action menu widget
 * - MOForagingSubsystem.h - Handles foraging operations
 *
 * TESTING CHECKLIST:
 * [ ] Inventory menu opens/closes correctly
 * [ ] Container opens unified inventory with both panels
 * [ ] Item context menu shows correct actions
 * [ ] Quick transfer moves items between inventories
 * [ ] Nearby items query returns correct items
 * [ ] Loot all picks up all nearby items
 * [ ] Ground context menu shows foraging options
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOUIControllerBase.h"
#include "MOInventoryUIController.generated.h"

class UMOInventoryMenu;
class UMOUnifiedInventoryMenu;
class UMOItemContextMenu;
class UMOGroundContextMenu;
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

	/** Show the context menu for a world item (from nearby items panel). */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void ShowWorldItemContextMenu(AMOWorldItem* WorldItem, FVector2D ScreenPosition);

	/** Close the item context menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory")
	void CloseItemContextMenu();

	/** Check if item context menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory")
	bool IsItemContextMenuOpen() const;

	// ==========================================================================
	// GROUND CONTEXT MENU (FORAGING)
	// ==========================================================================

	/**
	 * Show the ground context menu for foraging actions.
	 * @param WorldLocation - Location in world space where player clicked
	 * @param ScreenPosition - Screen position for menu placement
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Foraging")
	void ShowGroundContextMenu(FVector WorldLocation, FVector2D ScreenPosition);

	/** Close the ground context menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Foraging")
	void CloseGroundContextMenu();

	/** Check if ground context menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|Foraging")
	bool IsGroundContextMenuOpen() const;

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

	/** Frame-based debounce to prevent double-toggle from ECommonInputMode::All */
	uint64 LastToggleFrame = 0;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentContainerActor;

	UFUNCTION()
	void HandleUnifiedInventoryMenuRequestClose();

	UFUNCTION()
	void HandleUnifiedInventoryMenuContextMenuRequested(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid, int32 SlotIndex, FVector2D ScreenPosition);

	UFUNCTION()
	void HandleUnifiedInventoryMenuWorldItemContextMenuRequested(AMOWorldItem* WorldItem, FVector2D ScreenPosition);

	// --- Item Context Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOItemContextMenu> ItemContextMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 ItemContextMenuZOrder = 150;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOItemContextMenu> ItemContextMenuWidget;

	/** World item currently being operated on via context menu (for pickup action). */
	UPROPERTY(Transient)
	TWeakObjectPtr<AMOWorldItem> ContextMenuWorldItem;

	UFUNCTION()
	void HandleContextMenuClosed();

	UFUNCTION()
	void HandleContextMenuAction(FName ActionId, const FGuid& ItemGuid);

	// --- Ground Context Menu (Foraging) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOGroundContextMenu> GroundContextMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Foraging", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 GroundContextMenuZOrder = 150;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOGroundContextMenu> GroundContextMenuWidget;

	UFUNCTION()
	void HandleGroundContextMenuClosed();

	UFUNCTION()
	void HandleForagingSearchComplete(const TArray<AMOWorldItem*>& RevealedItems);

	UFUNCTION()
	void HandleForagingDigComplete(const TArray<AMOWorldItem*>& DugItems);

	// --- Nearby Items ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Inventory", meta=(ClampMin="0", AllowPrivateAccess="true"))
	float NearbyItemsQueryRadius = 400.0f;

	UFUNCTION()
	void HandleLootAllNearby();
};
