/**
 * =============================================================================
 * MOUIControllerBase.h - Abstract Base for Specialized UI Controllers
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Abstract base class for UI controller components. Provides shared utilities
 * for controller resolution, UIManager delegation, and cached pawn component
 * access. Part of the UIManager split architecture.
 *
 * KEY RESPONSIBILITIES:
 * 1. Provide PlayerController resolution utilities
 * 2. Delegate shared UI operations to UIManager (modal, input mode)
 * 3. Provide cached access to current pawn's components
 * 4. Serve as base class for specialized controllers
 *
 * ARCHITECTURE NOTES:
 * - All UI controllers live as sibling components on PlayerController
 * - Controllers find each other via GetOwner()->FindComponentByClass<T>()
 * - UIManager is the orchestrator - controllers delegate shared ops to it
 * - Component caching handled by UIManager, controllers access via getters
 *
 * CONTROLLER HIERARCHY:
 *   PlayerController
 *   +-- MOUIManagerComponent (orchestrator)
 *   +-- MOInventoryUIController
 *   +-- MOCraftingUIController
 *   +-- MOBuildingUIController
 *   +-- MOCharacterUIController
 *   +-- MOSystemMenuUIController
 *
 * CRITICAL PATTERNS:
 * 1. UIManager Access:
 *    GetUIManager() returns cached weak pointer to UIManager
 *    Use for delegation, not direct operation
 *
 * 2. Modal Background:
 *    ShowModalBackground() / HideModalBackground() delegate to UIManager
 *    UIManager tracks modal reference count
 *
 * 3. Pawn Component Access:
 *    GetCachedInventory(), GetCachedSkills(), etc.
 *    All delegate to UIManager's cached component pointers
 *    Components refreshed on possession change
 *
 * KNOWN PITFALLS:
 * 1. TIMING: UIManager may not be available in constructor.
 *    Use GetUIManager() only after BeginPlay.
 *
 * 2. CIRCULAR REFERENCE: Don't cache strong references to sibling controllers.
 *    Use weak pointers or call FindComponentByClass each time.
 *
 * 3. LOCAL PLAYER: IsLocalOwningPlayerController() required for UI operations.
 *    Remote controllers should not spawn UI.
 *
 * RELATED FILES:
 * - MOUIManagerComponent.h - Orchestrator that controllers delegate to
 * - MOInventoryUIController.h - Inventory UI specialization
 * - MOCraftingUIController.h - Crafting UI specialization
 * - MOCharacterUIController.h - Character/skills UI specialization
 * - MOSystemMenuUIController.h - Menus/system UI specialization
 *
 * TESTING CHECKLIST:
 * [ ] GetUIManager() returns valid pointer after BeginPlay
 * [ ] Modal background shows/hides correctly
 * [ ] Cached pawn components update on possession
 * [ ] IsLocalOwningPlayerController() correct for multiplayer
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CommonActivatableWidget.h"  // Needed for the RegisterCachedMenu<T> template body
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
	// NOTE: Input mode is now handled automatically by CommonUI via GetDesiredInputConfig()

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
	// COMMONUI LAYER SYSTEM
	// =========================================================================
	// Use these methods to push widgets to the appropriate layer.
	// Requires WBP_PrimaryGameLayout to be configured in project settings.

	/**
	 * Push a widget to the specified UI layer.
	 *
	 * @param LayerTag The layer to push to (e.g., MOUILayerTags::Layer_Game)
	 * @param WidgetClass The widget class to create and push
	 * @return The created widget, or nullptr if failed
	 */
	UCommonActivatableWidget* PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/**
	 * Push a widget to the specified UI layer, creating a new instance if needed.
	 *
	 * CommonUI stacks don't support adding existing widget instances directly.
	 * If the passed widget is already activated, returns it as-is.
	 * Otherwise, creates a NEW widget of the same class via the stack.
	 *
	 * IMPORTANT: The returned widget may be different from the passed widget!
	 * Callers must update their cached references with the returned value.
	 *
	 * @param LayerTag The layer to push to
	 * @param Widget The widget instance (used for class lookup if new widget needed)
	 * @return The actual widget in the stack (may be different from passed widget)
	 */
	UCommonActivatableWidget* PushWidgetInstanceToLayer(FGameplayTag LayerTag, UCommonActivatableWidget* Widget);

	/**
	 * Pop/deactivate a widget from its layer.
	 *
	 * @param Widget The widget to remove/deactivate
	 */
	void PopWidgetFromLayer(UCommonActivatableWidget* Widget);

	/**
	 * Cache a widget reference AND bind to its OnDeactivated so the cache auto-clears
	 * regardless of how the widget closes — controller's CloseX, outside-click handler,
	 * stack pop, Escape key, anything.
	 *
	 * SYSTEM RULE: every cached UCommonActivatableWidget reference in a controller is
	 * registered via this helper, never set directly. This is the single source of
	 * truth for "is the menu open?" cache invalidation. Skipping it brings back the
	 * stale-cache / press-twice symptom.
	 *
	 * Lifetime safety: lambda captures a WeakObjectPtr to this controller. If the
	 * controller is destroyed before the widget, the lambda becomes a no-op rather
	 * than dereferencing a dangling cache pointer.
	 */
	template<typename TWidget>
	void RegisterCachedMenu(TWidget* Widget, TWeakObjectPtr<TWidget>& Cache)
	{
		static_assert(TIsDerivedFrom<TWidget, UCommonActivatableWidget>::Value,
			"RegisterCachedMenu requires a UCommonActivatableWidget-derived type.");

		if (!Widget) return;
		Cache = Widget;

		TWeakObjectPtr<UMOUIControllerBase> WeakSelf(this);
		TWeakObjectPtr<TWidget>* CachePtr = &Cache;
		Widget->OnDeactivated().AddLambda([WeakSelf, CachePtr]()
		{
			if (WeakSelf.IsValid())
			{
				CachePtr->Reset();
			}
		});
	}

	/**
	 * SYSTEM RULE: every controller's IsXOpen() forwards here. Single source of truth
	 * for "is the cached menu currently displayed?"
	 *
	 * Two flavors of cached widget need two different checks:
	 *
	 *   1. UCommonActivatableWidget (stack-managed) — cache is cleared on deactivate by
	 *      RegisterCachedMenu's OnDeactivated lambda. So `IsValid(W)` is reliable, AND
	 *      we deliberately do NOT call IsInViewport on these: stack widgets are nested
	 *      inside a UCommonActivatableWidgetContainer, so IsInViewport returns false
	 *      even when they're visibly active. Using IsInViewport here causes the
	 *      "menu thinks it's closed while still showing" bug → close-then-reopen bounce
	 *      when a toggle key is pressed.
	 *
	 *   2. Non-activatable popup (UCommonUserWidget added via AddToViewport) — there's
	 *      no OnDeactivated, so the cache may be stale after RemoveFromParent. We
	 *      gate on IsInViewport to catch that case.
	 *
	 * Skipping this helper and rolling per-controller checks is how both the press-twice
	 * bug AND the close-then-reopen bounce came back. Stay in this helper.
	 */
	template<typename TWidget>
	bool IsCachedMenuOpen(const TWeakObjectPtr<TWidget>& Cache) const
	{
		UWidget* W = Cache.Get();
		if (!IsValid(W)) return false;

		if (Cast<UCommonActivatableWidget>(W))
		{
			// Activatable: RegisterCachedMenu auto-clears Cache on deactivate, so
			// a valid Cache means the widget is still in the activation lifecycle.
			return true;
		}
		// Non-activatable popup: cache can outlive viewport presence.
		return W->IsInViewport();
	}

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
