#include "MORespirationComponent.h"
#include "MOFrameworkMedical.h"
#include "MOVitalsComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

// ============================================================================
// PURE MATH
// ============================================================================

float FMOBreathMath::StepReserve(const FMOBreathParams& P, float Reserve, float DeltaSeconds, bool bAirwayClosed, float Exertion01)
{
	if (DeltaSeconds <= 0.0f)
	{
		return FMath::Clamp(Reserve, 0.0f, 1.0f);
	}
	if (bAirwayClosed)
	{
		const float Burn = 1.0f + FMath::Clamp(Exertion01, 0.0f, 1.0f) * (P.FullExertionDrainMultiplier - 1.0f);
		Reserve -= DeltaSeconds * Burn / FMath::Max(P.RestEmptySeconds, 1.0f);
	}
	else
	{
		Reserve += DeltaSeconds / FMath::Max(P.RecoverySeconds, 0.1f);
	}
	return FMath::Clamp(Reserve, 0.0f, 1.0f);
}

float FMOBreathMath::SpO2ForReserve(const FMOBreathParams& P, float Reserve, float NormalSpO2)
{
	const float T = FMath::Clamp(Reserve / FMath::Max(P.DesaturationReserve, 0.01f), 0.0f, 1.0f);
	const float Smooth = T * T * (3.0f - 2.0f * T);   // smoothstep: flat near normal, steep in the middle, flat near the floor
	return FMath::Lerp(P.EmptyReserveSpO2, NormalSpO2, Smooth);
}

float FMOBreathMath::SecondsToEmpty(const FMOBreathParams& P, float Exertion01)
{
	const float Burn = 1.0f + FMath::Clamp(Exertion01, 0.0f, 1.0f) * (P.FullExertionDrainMultiplier - 1.0f);
	return FMath::Max(P.RestEmptySeconds, 1.0f) / Burn;
}

// ============================================================================
// COMPONENT
// ============================================================================

UMORespirationComponent::UMORespirationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;   // a breath-hold is measured in tens of seconds
	SetIsReplicatedByDefault(true);
}

void UMORespirationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UMORespirationComponent, OxygenReserve, COND_OwnerOnly);
}

UMOVitalsComponent* UMORespirationComponent::ResolveVitals() const
{
	if (!CachedVitals.IsValid() && GetOwner())
	{
		CachedVitals = GetOwner()->FindComponentByClass<UMOVitalsComponent>();   // also finds a vitals component added after BeginPlay
	}
	return CachedVitals.Get();
}

void UMORespirationComponent::BeginPlay()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UMORespirationComponent_BeginPlay);   // spawn-cost breakdown in traces (-statnamedevents)
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		CachedVitals = Owner->FindComponentByClass<UMOVitalsComponent>();
	}
}

float UMORespirationComponent::GetExertion01() const
{
	const UMOVitalsComponent* Vitals = ResolveVitals();
	if (!Vitals)
	{
		return 0.0f;
	}
	// Panic burns oxygen like effort does: stress counts for half.
	return FMath::Clamp((Vitals->Exertion.CurrentExertion + 0.5f * Vitals->Exertion.StressLevel) / 100.0f, 0.0f, 1.0f);
}

void UMORespirationComponent::SetAirwayClosedBy(FName Source, bool bClosed)
{
	const bool bWasClosed = IsAirwayClosed();
	if (bClosed)
	{
		ClosedSources.Add(Source);
	}
	else
	{
		ClosedSources.Remove(Source);
	}
	if (bWasClosed && !IsAirwayClosed())
	{
		// The first breath after a hold: a gasp, louder the lower the reserve was.
		if (OxygenReserve < 0.8f)
		{
			OnGasp.Broadcast(OxygenReserve);
		}
		bUrgeAnnounced = false;
	}
}

void UMORespirationComponent::DebugSetOxygenReserve(float NewReserve)
{
	OxygenReserve = FMath::Clamp(NewReserve, 0.0f, 1.0f);
	if (UMOVitalsComponent* Vitals = ResolveVitals())
	{
		Vitals->SetAirwayOxygenReserve(OxygenReserve, FMOBreathMath::SpO2ForReserve(Params, OxygenReserve));
	}
}

void UMORespirationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bDied)
	{
		return;   // the authority owns the process; the owning client sees the replicated reserve
	}

	const bool bClosed = IsAirwayClosed();
	const float Old = OxygenReserve;
	OxygenReserve = FMOBreathMath::StepReserve(Params, OxygenReserve, DeltaTime, bClosed, GetExertion01());

	if (bClosed && !bUrgeAnnounced && OxygenReserve < Params.UrgeReserve)
	{
		bUrgeAnnounced = true;
		OnBreathUrge.Broadcast();
	}

	// The vitals turn the reserve into blood oxygen saturation (and so into tunnel vision, unconsciousness, respiratory failure).
	if (UMOVitalsComponent* Vitals = ResolveVitals())
	{
		if (!FMath::IsNearlyEqual(Old, OxygenReserve, 0.0005f) || OxygenReserve < 1.0f)
		{
			Vitals->SetAirwayOxygenReserve(OxygenReserve, FMOBreathMath::SpO2ForReserve(Params, OxygenReserve));
		}
	}

	if (bClosed && OxygenReserve <= 0.0f)
	{
		SecondsAsphyxiating += DeltaTime;
		if (SecondsAsphyxiating >= Params.AsphyxiaGraceSeconds)
		{
			bDied = true;
			UE_LOG(LogMOFrameworkMedical, Warning, TEXT("[MORespiration] %s asphyxiated (airway closed, reserve empty for %.0f s)"), *Owner->GetName(), SecondsAsphyxiating);
			OnAsphyxiaDeath.Broadcast();
		}
	}
	else
	{
		SecondsAsphyxiating = FMath::Max(0.0f, SecondsAsphyxiating - DeltaTime * 4.0f);   // a revived victim's brain recovers faster than it failed
	}
}
