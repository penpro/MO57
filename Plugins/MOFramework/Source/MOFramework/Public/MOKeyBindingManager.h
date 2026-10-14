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
 * [2026-10] KEEP THE MAPPING, CHANGE THE KEY (audit H54): ApplyBinding used UnmapKey +
 *   MapKey, which re-added a bare {Action, Key} at the end of the list -- the movement keys
 *   lost their Swizzle / Negate modifiers (a rebound S moved the pawn sideways) and every
 *   later slot index shifted. SetMappingKey edits the mapping's key in place.
 *
 * [2026-06] CONTEXT MODIFICATION: ApplyBinding edits the InputMappingContext in
 *   place (SetMappingKey). AMOPlayerController::EnsureRuntimeInputContexts()
 *   duplicates the shared IMC assets into per-controller RF_Transient copies at
 *   BeginPlay, so these edits hit THIS controller's copy, never the shared asset.
 *   Persisted custom bindings live in MOGameSettings; the asset stays immutable.
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

	/** Set the key of an action's SlotIndex-th mapping IN PLACE (its modifiers, triggers and position stay). False if the action has no such slot. Pure: no player needed. */
	static bool SetMappingKey(UInputMappingContext* Context, const UInputAction* Action, int32 SlotIndex, FKey NewKey, FKey& OutOldKey);

	/**
	 * Apply a key binding at runtime: SetMappingKey on the controller's own copy of the context, then rebuild the active mappings.
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
