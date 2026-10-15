#include "MOTimedActionComponent.h"
#include "MOCharacter.h"
#include "MOGameClockSubsystem.h"
#include "MOFramework.h"
#include "Engine/World.h"
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

bool UMOTimedActionComponent::Begin(FName ActionId, const FText& Label, float GameSeconds, TFunction<void()> OnComplete, TFunction<void()> OnCancelled, float MaxDriftCm)
{
	if (GetOwnerRole() != ROLE_Authority || !GetOwner())
	{
		return false;
	}
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
	++State.Serial;
	PendingComplete = MoveTemp(OnComplete);
	PendingCancelled = MoveTemp(OnCancelled);
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
	return State.bActive ? FMath::Max(0.0f, State.RealSeconds - static_cast<float>(ServerNow() - State.StartServerSeconds)) : 0.0f;
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
	const FName Id = State.ActionId;
	State.bActive = false;
	SetComponentTickEnabled(false);
	if (bListening)
	{
		if (AMOCharacter* Character = Cast<AMOCharacter>(GetOwner()))
		{
			Character->UnregisterInterruptListener(this);
		}
		bListening = false;
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOTimedAction] %s: %s %s (%s)"), *GetNameSafe(GetOwner()), *Id.ToString(), bCompleted ? TEXT("completed") : TEXT("cancelled"), Why);
	OnStateChanged.Broadcast(State);
	if (bCompleted && Complete)
	{
		Complete();
	}
	else if (!bCompleted && Cancelled)
	{
		Cancelled();
	}
}

void UMOTimedActionComponent::OnRep_State()
{
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
