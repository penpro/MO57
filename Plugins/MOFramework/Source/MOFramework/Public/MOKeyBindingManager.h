/**
 * =============================================================================
 * MOKeyBindingManager.h - Runtime Key Binding Operations
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Static helper class for key binding operations at runtime. Handles getting,
 * setting, and applying key bindings to InputMappingContexts. Used by
 * options panel and player controller to manage custom bindings.
 *
 * KEY OPERATIONS:
 * - GetCurrentBinding: Returns active binding (custom or default)
 * - GetDefaultBinding: Returns original mapping from context
 * - ApplyBinding: Modifies context and refreshes input subsystem
 * - ApplyAllBindingsFromSettings: Bulk apply at game start
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] CONTEXT MODIFICATION: ApplyBinding modifies the InputMappingContext
 *   at runtime. Changes are NOT persisted to asset - use MOGameSettings.
 *
 * [2024-02] SUBSYSTEM REFRESH: After modifying context, must remove and re-add
 *   to UEnhancedInputLocalPlayerSubsystem for changes to take effect.
 *
 * [2024-02] SLOT INDEX: Enhanced Input supports multiple keys per action.
 *   SlotIndex 0 = primary, 1 = secondary. Default is 0.
 *
 * =============================================================================
 * RELATED FILES: MOGameSettings.h, MOKeyBindingTypes.h, MOPlayerController.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class UInputAction;
class UInputMappingContext;
class APlayerController;
class AMOPlayerController;

/**
 * Static helper class for key binding operations.
 * See file header for operations and pitfalls.
 */
class MOFRAMEWORK_API FMOKeyBindingManager
{
public:
	/**
	 * Get the current binding for an action from the mapping context.
	 * Checks custom bindings first, then falls back to default.
	 */
	static FKey GetCurrentBinding(const UInputAction* Action, const UInputMappingContext* Context, int32 SlotIndex = 0);

	/**
	 * Get the default (original) binding for an action from the mapping context.
	 */
	static FKey GetDefaultBinding(const UInputAction* Action, const UInputMappingContext* Context, int32 SlotIndex = 0);

	/**
	 * Apply a key binding at runtime.
	 * Modifies the mapping context and re-applies it to the subsystem.
	 */
	static bool ApplyBinding(UInputAction* Action, FKey NewKey, UInputMappingContext* Context, APlayerController* PC, int32 SlotIndex = 0);

	/**
	 * Apply all saved custom bindings from MOGameSettings to the player controller's mapping contexts.
	 * Call this in BeginPlay after SetupInputComponent.
	 */
	static void ApplyAllBindingsFromSettings(AMOPlayerController* PC);

	/**
	 * Restore all key bindings to their cached defaults.
	 * Use after clearing custom bindings from settings.
	 */
	static void RestoreAllDefaults(AMOPlayerController* PC);

	/**
	 * Get the action ID (FName) from an input action.
	 */
	static FName GetActionId(const UInputAction* Action);

private:
	// Cache of default bindings (populated on first access per action)
	// Key: ActionId_SlotIndex, Value: Default FKey
	static TMap<FName, FKey> DefaultBindingsCache;

	/** Build cache key from action and slot */
	static FName MakeCacheKey(const UInputAction* Action, int32 SlotIndex);
};
