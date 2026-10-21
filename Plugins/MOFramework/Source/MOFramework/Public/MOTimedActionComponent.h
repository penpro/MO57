/**
 * =============================================================================
 * MOTimedActionComponent.h - A pawn's timed body action: drink at a lake, fill a container, boil, wash ...
 * =============================================================================
 *
 * PURPOSE
 *   The simulation owns the wait, not the UI. A timed action belongs to the PAWN: the server times it on the game clock, cancels it when the pawn moves, is hurt
 *   or falls unconscious (IMOInterruptibleInterface on AMOCharacter's interrupt bus, plus a drift check), and runs its completion there. The owning client sees it
 *   through the replicated State (label, length, start) and draws a progress bar -- so a player never waits on a hidden timer. The same component lets a survivor
 *   pawn drink without any UI at all.
 *
 *   One action at a time; starting another cancels the first. The completion is a server-side callback the caller hands in (the water action applies the draught).
 *
 * ADDING AN ACTION: one Begin() call with a label, a length and a completion. No new component, no new widget. Work done at the ground
 *   (drilling a fire, butchering, digging, drinking at the water's edge) calls BeginKneeling instead: the pawn crouches down to it on every
 *   machine and stands up after (N-181).
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOInterruptibleInterface.h"
#include "MOTimedActionComponent.generated.h"

/** The body's pose while an action runs. */
UENUM()
enum class EMOActionPosture : uint8
{
	Stand,
	/** Down at the work on the ground: the pawn's crouch. */
	Kneel
};

/** What the owner's progress bar needs (replicated). */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOTimedActionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|TimedAction")
	FName ActionId;

	UPROPERTY(BlueprintReadOnly, Category="MO|TimedAction")
	FText Label;

	/** Real seconds the action takes (its game seconds at the clock's time scale when it began). */
	UPROPERTY(BlueprintReadOnly, Category="MO|TimedAction")
	float RealSeconds = 0.0f;

	/** Server world time when it began (clients compare against the replicated server time). */
	UPROPERTY(BlueprintReadOnly, Category="MO|TimedAction")
	double StartServerSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category="MO|TimedAction")
	bool bActive = false;

	/** The pose for this action (SetPosture): the owning client crouches its own pawn from it. */
	UPROPERTY()
	EMOActionPosture Posture = EMOActionPosture::Stand;

	/** Bumped on every Begin, so the client tells two back-to-back actions of the same kind apart. */
	UPROPERTY()
	uint8 Serial = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FMOTimedActionChanged, const FMOTimedActionState& /*State*/);

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOTimedActionComponent : public UActorComponent, public IMOInterruptibleInterface
{
	GENERATED_BODY()

public:
	UMOTimedActionComponent();

	/**
	 * Start an action of GameSeconds on the game clock (server). Cancels one already running. OnComplete runs on the server when the time is up and the pawn is still
	 * where it started (within MaxDriftCm) and undisturbed; OnCancelled (optional) if not. MaxDriftCm <= 100 is a stay-put action (the first step ends it); a wider
	 * one lets the pawn move about inside that radius (tending a fire). CanContinue is an optional side-effect-free context query,
	 * checked on every server tick before completion. Returns false off the server or during reentrant replacement.
	 */
	bool Begin(FName ActionId, const FText& Label, float GameSeconds, TFunction<void()> OnComplete, TFunction<void()> OnCancelled = nullptr, float MaxDriftCm = 75.0f,
		TFunction<bool()> CanContinue = nullptr);

	/** Begin, then SetPosture(Kneel) if it started: work at the ground. */
	bool BeginKneeling(FName ActionId, const FText& Label, float GameSeconds, TFunction<void()> OnComplete, TFunction<void()> OnCancelled = nullptr,
		float MaxDriftCm = 75.0f, TFunction<bool()> CanContinue = nullptr);

	/**
	 * The body's pose for the running action (server, after Begin; each Begin resets it to Stand). Kneel crouches the pawn for as long as the action runs
	 * and stands it up after, unless it was crouching already. The machine that moves the pawn does it: the server for its own and AI pawns, the owning
	 * client for a joiner's (from the replicated State: the joiner's moves carry the crouch, and one the server made would be undone by the next move).
	 * Others see the crouch through the movement component's replicated bIsCrouched.
	 */
	void SetPosture(EMOActionPosture InPosture);

	/** Stop the running action without completing it (server). */
	void Cancel(const TCHAR* Why);

	/** The owner stops it (the progress bar's cancel, Escape). */
	UFUNCTION(Server, Reliable)
	void ServerCancel();

	bool IsActive() const { return State.bActive; }
	const FMOTimedActionState& GetState() const { return State; }

	/** 0..1 of the running action, from the replicated start (any machine). */
	float GetProgress01() const;
	float GetRemainingRealSeconds() const;

	/** Fired on the server and on the owning client whenever an action begins or ends. */
	FMOTimedActionChanged OnStateChanged;

	// IMOInterruptibleInterface
	virtual void NotifyInterrupt_Implementation(const FMOInterruptContext& Context) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(ReplicatedUsing=OnRep_State)
	FMOTimedActionState State;

	UFUNCTION()
	void OnRep_State();

	void Finish(bool bCompleted, const TCHAR* Why);
	double ServerNow() const;

	/** Crouch / stand the pawn to State's posture, on the machine that moves it (SetPosture). */
	void ApplyPosture();
	/** This component crouched the pawn (so it stands it up again; a pawn that was crouching already stays down). */
	bool bCrouchedForAction = false;

	TFunction<void()> PendingComplete;
	TFunction<void()> PendingCancelled;
	/** Optional server query, checked before completion on every action tick (target reach, supplies, capability). */
	TFunction<bool()> PendingCanContinue;
	bool bStarting = false;
	FVector StartLocation = FVector::ZeroVector;
	float MaxDrift = 75.0f;
	bool bListening = false;
};
