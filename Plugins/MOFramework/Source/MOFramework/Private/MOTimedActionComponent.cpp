#include "MOTimedActionComponent.h"
#include "MOCharacter.h"
#include "MOGameClockSubsystem.h"
#include "MOFramework.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

UMOTimedActionComponent::UMOTimedActionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(true);
}

void UMOTimedActionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMOTimedActionComponent, State);
}

double UMOTimedActionComponent::ServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

bool UMOTimedActionComponent::Begin(FName ActionId, const FText& Label, float GameSeconds, TFunction<void()> OnComplete, TFunction<void()> OnCancelled, float MaxDriftCm, TFunction<bool()> CanContinue)
{
	if (GetOwnerRole() != ROLE_Authority || !GetOwner() || bStarting)
	{
		return false;
	}
	// A cancellation observer must not start an action that this Begin would silently overwrite.
	TGuardValue<bool> Starting(bStarting, true);
	if (State.bActive)
	{
		Finish(false, TEXT("replaced by another action"));
	}
	// The clock's time scale turns game seconds into the real seconds the bar runs for (TimeScale 1 = real time).
	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	const float TimeScale = Clock ? FMath::Max(0.01f, Clock->GetTimeScale()) : 1.0f;

	State.ActionId = ActionId;
	State.Label = Label;
	State.RealSeconds = FMath::Max(0.1f, GameSeconds / TimeScale);
	State.StartServerSeconds = ServerNow();
	State.bActive = true;
	State.Posture = EMOActionPosture::Stand;
	++State.Serial;
	PendingComplete = MoveTemp(OnComplete);
	PendingCancelled = MoveTemp(OnCancelled);
	PendingCanContinue = MoveTemp(CanContinue);
	StartLocation = GetOwner()->GetActorLocation();
	MaxDrift = MaxDriftCm;

	if (AMOCharacter* Character = Cast<AMOCharacter>(GetOwner()))
	{
		Character->RegisterInterruptListener(this);
		bListening = true;
	}
	SetComponentTickEnabled(true);
	UE_LOG(LogMOFramework, Log, TEXT("[MOTimedAction] %s: %s (%.1f s)"), *GetNameSafe(GetOwner()), *ActionId.ToString(), State.RealSeconds);
	OnStateChanged.Broadcast(State);
	return true;
}

bool UMOTimedActionComponent::BeginKneeling(FName ActionId, const FText& Label, float GameSeconds, TFunction<void()> OnComplete, TFunction<void()> OnCancelled,
	float MaxDriftCm, TFunction<bool()> CanContinue)
{
	if (!Begin(ActionId, Label, GameSeconds, MoveTemp(OnComplete), MoveTemp(OnCancelled), MaxDriftCm, MoveTemp(CanContinue)))
	{
		return false;
	}
	SetPosture(EMOActionPosture::Kneel);
	return true;
}

void UMOTimedActionComponent::SetPosture(EMOActionPosture InPosture)
{
	if (GetOwnerRole() != ROLE_Authority || !State.bActive)
	{
		return;
	}
	State.Posture = InPosture;
	ApplyPosture();
}

void UMOTimedActionComponent::ApplyPosture()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->IsLocallyControlled())
	{
		return;   // a joiner's pawn on the server: the joiner crouches it (OnRep_State), its moves bring the crouch here
	}
	const bool bWantKneel = State.bActive && State.Posture == EMOActionPosture::Kneel;
	// The wish, not bIsCrouched (that follows a movement tick later): a completion that stands the pawn up and begins the next kneeling action
	// (butchering the next part) in the same frame keeps it down.
	const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	const bool bDownAlready = Move ? Move->bWantsToCrouch : Character->bIsCrouched;
	if (bWantKneel && !bCrouchedForAction && !bDownAlready)
	{
		Character->Crouch();
		bCrouchedForAction = true;
		UE_LOG(LogMOFramework, Log, TEXT("[MOTimedAction] %s kneels for %s"), *Character->GetName(), *State.ActionId.ToString());
	}
	else if (!bWantKneel && bCrouchedForAction)
	{
		bCrouchedForAction = false;
		Character->UnCrouch();
		UE_LOG(LogMOFramework, Log, TEXT("[MOTimedAction] %s stands up after %s"), *Character->GetName(), *State.ActionId.ToString());
	}
}

void UMOTimedActionComponent::Cancel(const TCHAR* Why)
{
	if (GetOwnerRole() == ROLE_Authority && State.bActive)
	{
		Finish(false, Why);
	}
}

void UMOTimedActionComponent::ServerCancel_Implementation()
{
	Cancel(TEXT("the player stopped it"));
}

float UMOTimedActionComponent::GetProgress01() const
{
	return State.bActive && State.RealSeconds > 0.0f ? FMath::Clamp(static_cast<float>(ServerNow() - State.StartServerSeconds) / State.RealSeconds, 0.0f, 1.0f) : 0.0f;
}

float UMOTimedActionComponent::GetRemainingRealSeconds() const
{
	// A newly joined client's server clock can briefly trail the replicated start. Project
	// from the same bounded progress as the bar, so remaining time never exceeds the full course.
	return State.bActive ? State.RealSeconds * (1.0f - GetProgress01()) : 0.0f;
}

void UMOTimedActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwnerRole() != ROLE_Authority || !State.bActive)
	{
		SetComponentTickEnabled(false);
		return;
	}
	// Kneeling at the water's edge is a stay-put action: walking off (or being pushed) ends it, whatever the interrupt bus saw.
	if (GetOwner() && FVector::Dist2D(GetOwner()->GetActorLocation(), StartLocation) > MaxDrift)
	{
		Finish(false, TEXT("moved away"));
		return;
	}
	if (PendingCanContinue && !PendingCanContinue())
	{
		Finish(false, TEXT("action context lost"));
		return;
	}
	if (ServerNow() - State.StartServerSeconds >= State.RealSeconds)
	{
		Finish(true, TEXT("done"));
	}
}

void UMOTimedActionComponent::NotifyInterrupt_Implementation(const FMOInterruptContext& Context)
{
	switch (Context.Reason)
	{
	case EMOInterruptReason::Movement:
		// A stay-put action (kneeling to drink) ends at the first step; one that allows moving about (tending a fire) ends only when the pawn leaves its radius
		// (the drift check in the tick).
		if (MaxDrift <= 100.0f)
		{
			Cancel(TEXT("moved"));
		}
		break;
	case EMOInterruptReason::Damage:
	case EMOInterruptReason::Knockdown:
	case EMOInterruptReason::Unconscious:
	case EMOInterruptReason::Death:
	case EMOInterruptReason::EnteredCombat:
	case EMOInterruptReason::LostControl:
	case EMOInterruptReason::External:
		Cancel(TEXT("interrupted"));
		break;
	default:
		break;
	}
}

void UMOTimedActionComponent::Finish(bool bCompleted, const TCHAR* Why)
{
	TFunction<void()> Complete = MoveTemp(PendingComplete);
	TFunction<void()> Cancelled = MoveTemp(PendingCancelled);
	PendingComplete = nullptr;
	PendingCancelled = nullptr;
	PendingCanContinue = nullptr;
	const FName Id = State.ActionId;
	const uint8 FinishedSerial = State.Serial;
	State.bActive = false;
	SetComponentTickEnabled(false);
	ApplyPosture();   // stands up (a completion that begins the next kneeling action crouches again)
	if (bListening)
	{
		if (AMOCharacter* Character = Cast<AMOCharacter>(GetOwner()))
		{
			Character->UnregisterInterruptListener(this);
		}
		bListening = false;
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOTimedAction] %s: %s %s (%s)"), *GetNameSafe(GetOwner()), *Id.ToString(), bCompleted ? TEXT("completed") : TEXT("cancelled"), Why);
	if (bCompleted && Complete)
	{
		Complete();
	}
	else if (!bCompleted && Cancelled)
	{
		Cancelled();
	}
	// Install the result before notifying observers. Completion may intentionally begin the next
	// action (butchering a second part); that Begin already published its own state.
	if (!State.bActive && State.Serial == FinishedSerial)
	{
		OnStateChanged.Broadcast(State);
	}
}

void UMOTimedActionComponent::OnRep_State()
{
	ApplyPosture();   // the owning client's own pawn
	OnStateChanged.Broadcast(State);
}

void UMOTimedActionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (State.bActive && GetOwnerRole() == ROLE_Authority)
	{
		Finish(false, TEXT("the pawn left play"));
	}
	Super::EndPlay(EndPlayReason);
}
