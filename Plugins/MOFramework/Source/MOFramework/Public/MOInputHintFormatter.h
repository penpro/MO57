/**
 * =============================================================================
 * MOInputHintFormatter.h - Tutorial Text Token Resolver
 * =============================================================================
 *
 * PURPOSE:
 * Static helper that turns templated hint text into resolved player-facing
 * text. Reads live key bindings from the player controller's input mapping
 * contexts so rebinds update the hint banner text on the next refresh.
 *
 * TOKEN GRAMMAR:
 *   {key:ActionName}           - Primary binding in default lookup order
 *   {key:ActionName:Building}  - Force a specific mapping context name
 *                                (matches the controller's *Context property
 *                                 by name, e.g. PawnControlContext,
 *                                 BaseBuildingContext, MenuContext)
 *   {key:ActionName:1}         - Secondary slot binding (default slot is 0)
 *
 * KEY GLYPHS:
 * Resolved keys are wrapped in square brackets, e.g. "[E]" or "[Mouse Wheel
 * Up]". If the action has no binding, the token resolves to "[unbound]".
 *
 * USAGE:
 *   FText Out = FMOInputHintFormatter::Format(
 *       NSLOCTEXT("Tutorial", "MoveHint", "Use {key:MoveAction} to move"),
 *       PlayerController);
 *
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UInputAction;
class UInputMappingContext;
class AMOPlayerController;

/**
 * Static helper for resolving tutorial hint text tokens against live input
 * bindings. See file header for token grammar.
 */
class MOFRAMEWORK_API FMOInputHintFormatter
{
public:
	/**
	 * Resolve all {key:...} tokens in Template against the player controller's
	 * current input mapping contexts.
	 *
	 * @param Template The text containing tokens.
	 * @param PC The owning player controller (must be a MOPlayerController to
	 *           access mapping contexts). If null or not a MOPlayerController,
	 *           tokens resolve to "[unbound]".
	 * @return The text with tokens substituted by bracketed key glyphs.
	 */
	static FText Format(const FText& Template, const APlayerController* PC);

	/**
	 * Convenience helper: look up the live key bound to an action by name on
	 * the given controller. Returns FKey() (invalid) if not found.
	 *
	 * Searches all configured mapping contexts in priority order if
	 * ContextName is None; otherwise searches only the named context.
	 */
	static FKey ResolveActionKey(const AMOPlayerController* MOPC, FName ActionPropertyName,
		FName ContextName = NAME_None, int32 SlotIndex = 0);

	/**
	 * Format a key as a bracketed glyph for display. "[E]", "[Space]",
	 * "[Mouse Wheel Up]", or "[unbound]" for an invalid key.
	 */
	static FString FormatKeyGlyph(const FKey& Key);
};
