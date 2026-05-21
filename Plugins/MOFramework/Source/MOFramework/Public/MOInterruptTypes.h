/**
 * =============================================================================
 * MOInterruptTypes.h - Generalized "something interrupted this character" event
 * =============================================================================
 *
 * PURPOSE
 *   A single per-pawn event bus for "interrupt my current activity." Sources
 *   broadcast (movement, damage, knockdown, death, combat-start, sleep, etc.),
 *   listeners react (building, harvesting, inspection, crafting, future actions).
 *   The reason is carried in the broadcast so listeners can make per-reason
 *   decisions — pause vs. cancel, ignore trivial events, etc.
 *
 * DESIGN GOALS
 *   1. Adding a new SOURCE = one call to AMOCharacter::BroadcastInterrupt().
 *      No interface changes, no listener changes.
 *   2. Adding a new LISTENER = implement IMOInterruptibleInterface +
 *      RegisterInterruptListener on the relevant character. No source changes.
 *   3. Per-reason policy decisions live in the listener — the broadcaster
 *      doesn't decide what "interrupt" means.
 *
 * ADDING A NEW INTERRUPT SOURCE
 *   When a system has an event that should interrupt activities (combat-start
 *   on AdrenalineComponent, knockdown on AnatomyComponent, sleep transition,
 *   etc.), it builds a context and calls BroadcastInterrupt on the affected
 *   character. Example for combat-start:
 *
 *      FMOInterruptContext Ctx;
 *      Ctx.Reason          = EMOInterruptReason::EnteredCombat;
 *      Ctx.AffectedActor   = OwningCharacter;
 *      Ctx.InstigatorActor = Attacker;
 *      Ctx.Severity        = 1.0f;
 *      OwningCharacter->BroadcastInterrupt(Ctx);
 *
 *   That's the entire integration. No changes to existing listeners are
 *   required — every listener already gets called and decides for itself.
 *
 * ADDING A NEW LISTENER
 *   See MOInterruptibleInterface.h. Implement the interface, register at
 *   action start, unregister at action end, switch on Context.Reason inside.
 *
 * RELATED FILES
 *   - MOInterruptibleInterface.h   — listener contract
 *   - MOCharacter.h                — broadcaster (RegisterInterruptListener
 *                                    / BroadcastInterrupt)
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOInterruptTypes.generated.h"

/**
 * Why an activity is being interrupted. Listeners switch on this to decide
 * whether to pause (preserve progress), cancel (discard progress), or ignore.
 *
 * Add new reasons at the END before MAX. Renaming or reordering values is a
 * breaking change for any serialized data — don't do it casually.
 *
 * Future migration note: this enum is a candidate for conversion to
 * FGameplayTag once that migration runs (see MO57 Refactoring Roadmap).
 * Until then, treat additions as additive only.
 */
UENUM(BlueprintType)
enum class EMOInterruptReason : uint8
{
	/** Default/unset — should never be broadcast in practice. */
	None				UMETA(DisplayName="None"),

	/** The pawn started moving (translation velocity above threshold). */
	Movement			UMETA(DisplayName="Movement"),

	/** Took damage. Listeners can branch on Severity. */
	Damage				UMETA(DisplayName="Damage"),

	/** Knocked down / staggered. Hard interrupt for most activities. */
	Knockdown			UMETA(DisplayName="Knockdown"),

	/** Lost consciousness, fell asleep, fainted. */
	Unconscious			UMETA(DisplayName="Unconscious"),

	/** Pawn died. Listeners should treat this as terminal (cancel, no resume). */
	Death				UMETA(DisplayName="Death"),

	/** Entered combat state. Less severe than Damage — listeners may choose to pause vs. cancel. */
	EnteredCombat		UMETA(DisplayName="EnteredCombat"),

	/** Player or AI explicitly cancelled the activity via UI / input. */
	UserCancel			UMETA(DisplayName="UserCancel"),

	/** Pawn was unpossessed or otherwise lost control. */
	LostControl			UMETA(DisplayName="LostControl"),

	/** Generic catch-all for systems that don't fit existing reasons yet. */
	External			UMETA(DisplayName="External"),

	MAX					UMETA(Hidden)
};

/**
 * Context for a single interrupt broadcast. Carries everything a listener
 * needs to decide whether/how to react. Cheap to copy (no string churn).
 *
 * Fields are filled by the broadcasting system before calling
 * AMOCharacter::BroadcastInterrupt(). Defaults are sensible — most sources
 * only need to set Reason and AffectedActor.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOInterruptContext
{
	GENERATED_BODY()

	/** What kind of event triggered the interrupt. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Interrupts")
	EMOInterruptReason Reason = EMOInterruptReason::None;

	/**
	 * The pawn whose activities should be interrupted. Usually the same actor
	 * that owns the listener list, but stored explicitly so listeners with
	 * multiple registrations can disambiguate.
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Interrupts")
	TWeakObjectPtr<AActor> AffectedActor;

	/**
	 * Who/what caused the interrupt. Self for movement, attacker for damage,
	 * environmental hazard actor for fire/etc. May be null for events with no
	 * meaningful instigator.
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Interrupts")
	TWeakObjectPtr<AActor> InstigatorActor;

	/**
	 * Strength of the interrupt, 0..1. Listeners can use this to filter
	 * (e.g. trivial damage doesn't interrupt crafting, severe damage does).
	 * Default 1.0 means "full strength" — the broadcaster has no opinion.
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Interrupts", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Severity = 1.0f;

	FMOInterruptContext() = default;

	/** Convenience constructor for the common "self-caused, full severity" case. */
	FMOInterruptContext(EMOInterruptReason InReason, AActor* InAffected)
		: Reason(InReason)
		, AffectedActor(InAffected)
		, InstigatorActor(InAffected)
		, Severity(1.0f)
	{
	}
};
