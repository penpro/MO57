#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOUIControllerBase.generated.h"

class APlayerController;
class UUserWidget;
class UMOUIManagerComponent;
class UMOInventoryComponent;
class UMOSkillsComponent;
class UMOKnowledgeComponent;
class UMOCraftingQueueComponent;
class UMORecipeDiscoveryComponent;
class UMOVitalsComponent;
class UMOMetabolismComponent;
class UMOMentalStateComponent;
class UMOSurvivalStatsComponent;
class UMONotificationComponent;

/**
 * Base class for specialized UI controllers.
 *
 * UI controllers are extracted from MOUIManagerComponent to reduce its size
 * and provide clear ownership of UI subsystems. Each controller handles a
 * specific category of UI (Inventory, Crafting, Building, Character, System).
 *
 * Controllers delegate shared operations (modal background, input mode, etc.)
 * back to MOUIManagerComponent, which acts as the central orchestrator.
 *
 * ARCHITECTURE:
 *   PlayerController
 *   ├── MOUIManagerComponent (orchestrator, HUD, shared state)
 *   ├── MOInventoryUIController
 *   ├── MOCraftingUIController
 *   ├── MOBuildingUIController
 *   ├── MOCharacterUIController
 *   └── MOSystemMenuUIController
 *
 * All controllers live on the same PlayerController owner and share
 * the UIManager's cached pawn components.
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOUIControllerBase : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOUIControllerBase();

protected:
	// =========================================================================
	// CONTROLLER UTILITIES
	// =========================================================================

	/** Get the owning player controller (returns null if owner is not a PC). */
	APlayerController* ResolveOwningPlayerController() const;

	/** Check if this is the local player's controller. */
	bool IsLocalOwningPlayerController() const;

	/** Get the UIManager component (sibling component on same owner). */
	UMOUIManagerComponent* GetUIManager() const;

	// =========================================================================
	// UI MANAGER DELEGATION
	// =========================================================================
	// These methods delegate to UIManager for shared UI operations.
	// Controllers should use these instead of implementing their own.

	/** Apply input mode suitable for having a menu open. */
	void ApplyInputModeForMenuOpen(UUserWidget* MenuWidget);

	/** Restore input mode for gameplay (no menus open). */
	void ApplyInputModeForMenuClosed();

	/** Show the modal background behind menus. */
	void ShowModalBackground();

	/** Hide the modal background. */
	void HideModalBackground();

	/** Check if the player has a valid possessed pawn. */
	bool HasValidPawn() const;

	/** Show notification that a pawn is required. */
	void ShowNoPawnNotification();

	/** Update reticle visibility based on menu state. */
	void UpdateReticleVisibility();

	/** Check if any menu is currently open. */
	bool IsAnyMenuOpen() const;

	// =========================================================================
	// PAWN COMPONENT ACCESS
	// =========================================================================
	// These delegate to UIManager's cached pawn components.
	// Components are cached on possession change to avoid repeated lookups.

	UMOInventoryComponent* GetCachedInventory() const;
	UMOSkillsComponent* GetCachedSkills() const;
	UMOKnowledgeComponent* GetCachedKnowledge() const;
	UMOCraftingQueueComponent* GetCachedCraftingQueue() const;
	UMORecipeDiscoveryComponent* GetCachedRecipeDiscovery() const;
	UMOVitalsComponent* GetCachedVitals() const;
	UMOMetabolismComponent* GetCachedMetabolism() const;
	UMOMentalStateComponent* GetCachedMentalState() const;
	UMOSurvivalStatsComponent* GetCachedSurvivalStats() const;

	/** Get the notification component (sibling on PlayerController). */
	UMONotificationComponent* GetNotificationComponent() const;

private:
	/** Cached reference to UIManager component. Resolved on first access. */
	mutable TWeakObjectPtr<UMOUIManagerComponent> CachedUIManager;
};
