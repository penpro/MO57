#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class UInputAction;
class UInputMappingContext;
class APlayerController;
class AMOPlayerController;

/**
 * Static helper class for key binding operations.
 * Provides centralized logic for getting, setting, and applying key bindings.
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
