// Breath-hold physiology: the oxygen reserve of lungs + blood, what drains it, what refills it, and what happens when it runs out (drowning / asphyxia).
//
// Why a component of its own: SpO2 in UMOVitalsComponent is a pure function of the body's state (blood volume, lungs, core temperature); a closed airway is a TIME process (the reserve drains, then the
// body fails) with its own inputs (a swimmer's head below the surface, later smoke or a choke). This component owns that process and feeds one scalar into the vitals (SetAirwayOxygenReserve), so the
// existing consequences (mental state: tunnel vision below SpO2 90, unconsciousness below 70; respiratory failure below 50) apply without any new wiring.
//
// The numbers are the real ones (untrained adult, the realism pillar): a held breath at rest lasts ~60 s before the urge is unbearable and ~110 s to blackout; exertion burns oxygen several times faster
// (a swimmer is blacking out in 40-50 s); one breath of air refills the reserve in ~12-15 s; an unconscious victim whose airway stays closed dies about 75 s after the reserve hits zero (drowning is
// fatal ~3 minutes after submersion). All of it is data (EditDefaultsOnly), and the math is a pure static function so it is tested headless.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MORespirationComponent.generated.h"

class UMOVitalsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnAsphyxiaDeath);
/** The airway reopened while the reserve was low: the gasp (audio, camera shake). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnGasp, float, ReserveAtGasp);
/** The urge to breathe became strong for the first time in this breath-hold (the HUD pulses, the character's breathing audio changes). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnBreathUrge);

/** Tunables of the oxygen-reserve model. Seconds are game-seconds of a healthy untrained adult. */
USTRUCT(BlueprintType)
struct MOFRAMEWORKMEDICAL_API FMOBreathParams
{
	GENERATED_BODY()

	/** Seconds from a full reserve to an empty one at rest with a closed airway (blackout point; the urge to breathe comes at UrgeReserve long before). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Breath", meta=(ClampMin="10"))
	float RestEmptySeconds = 110.0f;

	/** Oxygen burn multiplier at full exertion (1.0 = ExertionLevel 100): a hard-working swimmer empties the reserve this many times faster than a still one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Breath", meta=(ClampMin="1"))
	float FullExertionDrainMultiplier = 4.0f;

	/** Seconds to refill an empty reserve with the airway open at rest (a few deep breaths). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Breath", meta=(ClampMin="1"))
	float RecoverySeconds = 14.0f;

	/** Reserve below which the urge to breathe is strong (about 55 s into a rest breath-hold: the breaking point of an untrained adult). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Breath", meta=(ClampMin="0", ClampMax="1"))
	float UrgeReserve = 0.50f;

	/** Reserve below which the blood starts desaturating (SpO2 falls steeply from here to the floor at reserve 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Breath", meta=(ClampMin="0.05", ClampMax="1"))
	float DesaturationReserve = 0.35f;

	/** SpO2 at an empty reserve (the vitals' normal 98 falls to this). Respiratory failure is declared below 50. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Breath", meta=(ClampMin="0", ClampMax="98"))
	float EmptyReserveSpO2 = 40.0f;

	/** Seconds the reserve may stay at zero with the airway still closed before the body fails (drowning is fatal about 3 minutes after submersion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Breath", meta=(ClampMin="1"))
	float AsphyxiaGraceSeconds = 75.0f;
};

/** Pure oxygen-reserve math (no actor state): the component calls these; the automation tests call them directly. */
struct MOFRAMEWORKMEDICAL_API FMOBreathMath
{
	/** One step of the reserve. Closed airway: drains at (1 / RestEmptySeconds) x (1 + Exertion01 x (FullExertionDrainMultiplier - 1)) per second. Open: refills at 1 / RecoverySeconds. Clamped 0..1. */
	static float StepReserve(const FMOBreathParams& P, float Reserve, float DeltaSeconds, bool bAirwayClosed, float Exertion01);

	/** SpO2 (%) the blood shows at this reserve: the normal 98 until the reserve falls below DesaturationReserve, then a smooth fall to EmptyReserveSpO2 at zero. */
	static float SpO2ForReserve(const FMOBreathParams& P, float Reserve, float NormalSpO2 = 98.0f);

	/** Seconds a closed-airway breath-hold lasts from a full reserve to empty at this exertion (for UI / tests). */
	static float SecondsToEmpty(const FMOBreathParams& P, float Exertion01);
};

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORKMEDICAL_API UMORespirationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMORespirationComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- inputs ----

	/**
	 * Close or reopen the airway on behalf of a SOURCE ("Water", later "Smoke", "Choke"): the airway is closed while ANY source holds it closed. Authority-side (the swimming code runs on the server and on the
	 * owning client's prediction; only the authority's value is replicated back into the reserve).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Breath")
	void SetAirwayClosedBy(FName Source, bool bClosed);

	UFUNCTION(BlueprintPure, Category="MO|Breath")
	bool IsAirwayClosed() const { return ClosedSources.Num() > 0; }

	// ---- state ----

	/** 1 = full lungs + blood oxygen, 0 = empty. Replicated to the owner for the breath bar. */
	UFUNCTION(BlueprintPure, Category="MO|Breath")
	float GetOxygenReserve() const { return OxygenReserve; }

	/** True once the urge to breathe is strong (reserve below UrgeReserve) while the airway is closed. */
	UFUNCTION(BlueprintPure, Category="MO|Breath")
	bool IsUrgeToBreathe() const { return IsAirwayClosed() && OxygenReserve < Params.UrgeReserve; }

	/** Seconds the reserve has been empty with the airway closed (the asphyxia timer, 0 when breathing). */
	UFUNCTION(BlueprintPure, Category="MO|Breath")
	float GetSecondsAsphyxiating() const { return SecondsAsphyxiating; }

	/** True once the body has failed with the airway closed (OnAsphyxiaDeath fired). */
	UFUNCTION(BlueprintPure, Category="MO|Breath")
	bool HasAsphyxiated() const { return bDied; }

	// ---- events ----

	UPROPERTY(BlueprintAssignable, Category="MO|Breath")
	FMOOnAsphyxiaDeath OnAsphyxiaDeath;

	UPROPERTY(BlueprintAssignable, Category="MO|Breath")
	FMOOnGasp OnGasp;

	UPROPERTY(BlueprintAssignable, Category="MO|Breath")
	FMOOnBreathUrge OnBreathUrge;

	// ---- config ----

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Breath")
	FMOBreathParams Params;

	/** Test / debug: set the reserve directly. Authority-side. */
	UFUNCTION(BlueprintCallable, Category="MO|Breath")
	void DebugSetOxygenReserve(float NewReserve);

protected:
	virtual void BeginPlay() override;

private:
	/** The owner's vitals component (looked up lazily: it may be added after BeginPlay). */
	UMOVitalsComponent* ResolveVitals() const;

	/** Exertion 0..1 of the owner right now (the vitals' exertion level / 100; 0 without vitals). */
	float GetExertion01() const;

	UPROPERTY(Replicated)
	float OxygenReserve = 1.0f;

	/** Seconds spent with an empty reserve and a closed airway. Authority only. */
	float SecondsAsphyxiating = 0.0f;

	bool bUrgeAnnounced = false;
	bool bDied = false;

	TSet<FName> ClosedSources;
	mutable TWeakObjectPtr<UMOVitalsComponent> CachedVitals;
};
