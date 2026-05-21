/**
 * =============================================================================
 * MOInterruptibleInterface.h - Contract for activities that can be interrupted
 * =============================================================================
 *
 * PURPOSE
 *   Any UObject (component, subsystem, UI controller, etc.) that owns a
 *   time-based activity can implement this interface to receive interrupt
 *   events from the centralized broadcast on AMOCharacter.
 *
 *   The interface deliberately makes NO policy decisions. Each implementer
 *   inspects `Context.Reason` (and Severity, InstigatorActor) and decides
 *   whether to pause, cancel, log, or ignore. This keeps interrupt semantics
 *   close to the activity they affect — building knows to preserve progress,
 *   inspection knows to discard it.
 *
 * INTEGRATION
 *   1. Inherit IMOInterruptibleInterface on your UObject.
 *   2. Provide NotifyInterrupt_Implementation(const FMOInterruptContext&).
 *   3. Call Character->RegisterInterruptListener(this) at the start of the
 *      interruptible activity. Use the AMOCharacter whose actions are being
 *      tracked — that's whoever's interrupt broadcasts should affect you.
 *   4. Call Character->UnregisterInterruptListener(this) when the activity
 *      ends (completed, cancelled, or torn down).
 *
 *   Inside NotifyInterrupt, switch on Context.Reason. Reasons your activity
 *   doesn't care about should early-return. Reasons your activity DOES care
 *   about should call your existing pause/cancel methods — do not duplicate
 *   that logic in the interrupt handler.
 *
 * SOURCES OF INTERRUPTS
 *   See MOInterruptTypes.h for the full list of EMOInterruptReason values.
 *   AMOCharacter currently broadcasts Movement automatically; combat/medical
 *   systems will broadcast Damage, Knockdown, Unconscious, Death, etc.
 *
 * RELATED FILES
 *   - MOInterruptTypes.h    — FMOInterruptContext and EMOInterruptReason
 *   - MOCharacter.h         — broadcaster + listener registration
 *   - MOBuildProgressComponent.cpp - example listener (pauses on Movement)
 *   - MOHarvestSubsystem.cpp        - example listener (cancels on Movement)
 *   - MOCharacterUIController.cpp   - example listener (cancels inspection)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOInterruptTypes.h"
#include "MOInterruptibleInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UMOInterruptibleInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract for systems that can be interrupted by a character event.
 * One method; everything else is the implementer's policy.
 */
class MOFRAMEWORK_API IMOInterruptibleInterface
{
	GENERATED_BODY()

public:
	/**
	 * Fired once per broadcast on every registered listener of the character.
	 * The implementer reads Context.Reason (and optionally Severity /
	 * InstigatorActor) and decides what to do.
	 *
	 * Re-entrancy: a listener MAY call UnregisterInterruptListener on itself
	 * from inside this method — the broadcaster iterates a snapshot.
	 *
	 * @param Context - Why the interrupt fired, who caused it, how severe.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Interrupts")
	void NotifyInterrupt(const FMOInterruptContext& Context);
};
