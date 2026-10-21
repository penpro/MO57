/**
 * =============================================================================
 * MOVitalsComponent.h - Vital Signs and Physiological State
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Pawn component managing vital signs (HR, BP, SpO2, temp, glucose) and
 * physiological states (exertion, stress, fatigue, activity level). Central
 * integration point between medical, metabolism, and activity systems.
 *
 * KEY RESPONSIBILITIES:
 * 1. Track vital signs: HeartRate, BloodPressure, SpO2, Temperature, Glucose
 * 2. Track blood volume and blood loss stages (Class I-IV hemorrhage)
 * 3. Manage exertion state (stress, pain, fatigue)
 * 4. Manage activity levels and stamina (Idle/Walking/Jogging/Sprinting/Combat)
 * 5. Coordinate calorie burn with MetabolismComponent
 * 6. Detect critical conditions (cardiac arrest, respiratory failure)
 *
 * ARCHITECTURE NOTES:
 * - Tick interval 0.5s (configurable via TickInterval)
 * - Activity system uses EMOActivityLevel enum
 * - Stamina drains/recovers based on current activity
 * - Caches references to Anatomy, Metabolism, MentalState components
 *
 * MEDICAL CASCADE (from CLAUDE.md):
 * Blood loss -> BloodLossStage -> HR/BP changes -> Shock
 * Dehydration -> HR increase, BP decrease, Temp increase
 * Activity -> Calorie burn -> Glucose consumption
 *
 * CRITICAL PATTERNS:
 * 1. Blood Loss Processing:
 *    ApplyBloodLoss() -> Update BloodVolume -> Check stage thresholds
 *    -> Broadcast OnBloodLossStageChanged -> Adjust HR/BP automatically
 *
 * 2. Activity Integration:
 *    SetActivityLevel(Sprint) -> Update stamina drain rate
 *    -> ProcessActivityEffects() per tick -> Drain stamina, burn calories
 *    -> OnStaminaDepleted -> Movement system should downgrade activity
 *
 * 3. Vital Sign Calculation:
 *    TickVitals() -> CalculateHeartRate() based on:
 *    - Base HR
 *    - Blood loss modifier
 *    - Exertion modifier
 *    - Pain modifier
 *    - Temperature modifier
 *
 * KNOWN PITFALLS:
 * 1. ACTIVITY LEVEL SYNC: Movement system must call SetActivityLevel()
 *    when movement speed changes. Mismatched state causes wrong calorie burn.
 *
 * 2. STAMINA DEPLETION: When stamina hits 0, OnStaminaDepleted fires.
 *    Movement system MUST respond by reducing activity level. If not handled,
 *    character continues at high activity with no stamina.
 *
 * 3. BLOOD LOSS STAGES: Class IV (>40% loss) is immediately life-threatening.
 *    Ensure UI shows urgent warnings at Class III/IV.
 *
 * 4. TIME SCALE: This component reads TimeScale from UMOGameClockSubsystem
 *    (single source of truth across all medical components). To tune the
 *    overall simulation speed, change it on the subsystem — not here.
 *
 * RELATED FILES:
 * - MOMedicalTypes.h - FMOVitalSigns, FMOExertionState, FMOActivityState
 * - MOAnatomyComponent.h - Provides blood loss from wounds
 * - MOMetabolismComponent.h - Consumes calories from activity
 * - MOMentalStateComponent.h - Affected by vital sign changes
 *
 * INTEGRATION POINTS (call SetActivityLevel from):
 * - Character movement component when speed changes
 * - Crafting system when crafting starts/stops
 * - Building system when construction activity changes
 * - Combat system when entering/exiting combat
 *
 * TESTING CHECKLIST:
 * [ ] Blood loss progresses through stages correctly
 * [ ] HR/BP respond to blood loss
 * [ ] Activity level changes drain stamina at correct rates
 * [ ] Stamina depleted fires delegate
 * [ ] Calorie burn applies to metabolism
 * [ ] Blood regenerates over time when stable
 * [ ] Save/load preserves vital state
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOMedicalTypes.h"
#include "MOVitalsComponent.generated.h"

class UMOAnatomyComponent;
class UMOMetabolismComponent;
class UMOMentalStateComponent;

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMOOnVitalSignChanged, FName, VitalName, float, OldValue, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnBloodLossStageChanged, EMOBloodLossStage, OldStage, EMOBloodLossStage, NewStage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnCardiacArrest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnRespiratoryFailure);

/** A whole-body failure that kills when it is SUSTAINED (see UMOVitalsComponent::StepSystemicFailure). Distinct causes, so the death recap can say what happened. */
UENUM(BlueprintType)
enum class EMOSystemicFailure : uint8
{
	None,
	/** Circulatory arrest from blood loss (blood volume under 30 % of normal): the brain dies of ischaemia. */
	Exsanguination,
	/** The heart stopped (rate under 10 / min) with the blood still there. */
	CardiacArrest,
	/** Oxygen saturation under 50 % or breathing under 4 / min: hypoxic cardiac arrest follows. */
	RespiratoryFailure,
	/** A core of 42 C or more for long enough (a thermal dose): the proteins denature and the organs fail. */
	HeatStroke,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnSystemicDeath, EMOSystemicFailure, Cause);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnVitalsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnActivityChanged, EMOActivityLevel, OldActivity, EMOActivityLevel, NewActivity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnStaminaChanged, float, OldStamina, float, NewStamina);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnStaminaDepleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnThermalComfortChanged, EMOThermalComfort, OldComfort, EMOThermalComfort, NewComfort);

/**
 * Fired only when the wetness STATE crosses a bucket boundary (Dry → Damp,
 * Damp → Wet, Wet → Soaked, etc). Continuous WetnessLevel changes don't
 * fire — listeners (HUD moodle) re-render on state change only.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnWetnessStateChanged, EMOWetnessState, OldState, EMOWetnessState, NewState);

// ============================================================================
// SAVE DATA
// ============================================================================

USTRUCT(BlueprintType)
struct MOFRAMEWORKMEDICAL_API FMOVitalsSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	FMOVitalSigns Vitals;

	UPROPERTY()
	FMOExertionState Exertion;

	UPROPERTY()
	FMOActivityState Activity;
};

// ============================================================================
// COMPONENT
// ============================================================================

/**
 * Component managing vital signs and physiological state.
 * Tracks blood volume, heart rate, blood pressure, SpO2, temperature, glucose.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORKMEDICAL_API UMOVitalsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOVitalsComponent();

	// ============================================================================
	// REPLICATED STATE
	// ============================================================================

	/** Current vital signs. Replicated to the owner; OnRep_Vitals gives a joiner the same OnVitalsChanged / OnThermalComfortChanged the host's
	 *  tick fires (N-178, Oct 19 2026: they fired on the server only, so a joiner's thermal moodle and status panel never changed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Vitals, Category="MO|Vitals")
	FMOVitalSigns Vitals;

	UFUNCTION()
	void OnRep_Vitals();

	/** Fire OnThermalComfortChanged if the comfort bucket changed (server tick or client OnRep). */
	void BroadcastThermalComfortIfChanged();

	/** Current exertion and stress state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Vitals")
	FMOExertionState Exertion;

	/** Current activity state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Vitals")
	FMOActivityState Activity;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Blood regeneration rate in mL/day (natural recovery). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Config", meta=(ClampMin="0"))
	float BloodRegenerationRate = 500.0f;

	/**
	 * Body-core temperature thresholds for the thermal-comfort HUD indicator.
	 * Mapped to EMOThermalComfort buckets in GetThermalComfort(). Defaults are
	 * medically meaningful — keep unless the game uses different units.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Config")
	FMOThermalComfortThresholds ThermalThresholds;

	/** Bucket thresholds for the wet-state moodle (Dry/Damp/Wet/Soaked). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Config")
	FMOWetnessThresholds WetnessThresholds;

	/**
	 * How fast WetnessLevel rises per second of full exposure to rain (intensity 1.0, zero overhead cover). Default 1/300: a downpour soaks
	 * ordinary clothing in 5 min, UDW's "Rain" (0.7) in ~7, a light rain (0.3) in ~17 -- the real phenomenon (Principle 11, N-176, Oct 19 2026;
	 * was 0.10 = soaked in 10 s). Scaled by rain intensity and shelter coverage. Wading / swimming soaks far faster (MOCharacter's immersion).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Wetness",
		meta=(ClampMin="0.0001", ClampMax="1.0"))
	float WetnessGainPerSecond = 1.0f / 300.0f;

	/**
	 * How fast WetnessLevel decays per second while sheltered or in clear weather. Default 1/9000: soaked clothing dries on the body in
	 * ~2.5 h of mild air (was 0.005 = 3 min, a game rate; N-176). A fire is how you dry out fast (HeatDryingBoostPerCelsius).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Wetness",
		meta=(ClampMin="0.00001", ClampMax="1.0"))
	float WetnessDecayPerSecond = 1.0f / 9000.0f;

	/**
	 * Drying-rate boost per °C of local heat (F1: fire dries clothes). At the default 0.14 a campfire's +20 °C dries 3.8x faster than open
	 * air: soaked to dry in ~40 min by the fire vs ~2.5 h without (was 0.5: 18 s vs 200 s; N-176). Applied even in rain: a hot fire under
	 * cover out-dries a drizzle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Wetness",
		meta=(ClampMin="0.0", ClampMax="5.0"))
	float HeatDryingBoostPerCelsius = 0.14f;

	/**
	 * Seconds between weather/shelter polls. Wetness update is cheap once we
	 * have a rain intensity + overhead coverage number; the EXPENSIVE part
	 * is the 9-ray shelter trace. Default 2s = trace cost amortized.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Vitals|Wetness",
		meta=(ClampMin="0.25", ClampMax="10.0"))
	float WetnessPollInterval = 2.0f;

	// ============================================================================
	// ENVIRONMENTAL TEMPERATURE (H12) — weather→body exposure pipeline
	// ============================================================================
	//
	// Drives ApplyEnvironmentalTemperature() from the live weather provider on a
	// throttled cadence (NOT every frame, NOT every vitals tick). The ambient
	// signal is the weather subsystem's "feels-like" temperature (already folds
	// in wind chill, wet-cold, and heat index). Insulation is a tunable default
	// plus a cheap shelter bonus reusing the wetness overhead-cover trace.
	//
	// SAFETY: every value here is gentle and EditDefaultsOnly so the lead can
	// tune in playtest. Body temp is hard-clamped 25–45°C inside
	// ApplyEnvironmentalTemperature regardless of these settings.

	/** (H12) Master switch — environmental temperature only affects the body when true. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Vitals|Temperature")
	bool bEnableEnvironmentalTemperature = true;

	/**
	 * (H12) Seconds between environmental-temperature applications. The weather
	 * query + (cheap) feels-like math is light, but there is no reason to run it
	 * every tick — exposure drift is slow. Default 2s matches the wetness poll so
	 * both weather-driven systems update on the same cadence. The full elapsed
	 * interval is passed to ApplyEnvironmentalTemperature so drift stays correct.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Vitals|Temperature",
		meta=(ClampMin="0.25", ClampMax="30.0"))
	float EnvironmentalTempUpdateInterval = 2.0f;

	/**
	 * (H12) Baseline insulation (0..1, 1 = fully insulated/no exposure) used when
	 * per-item clothing warmth is not available. v1 stop-gap: equipment exposes
	 * no first-class warmth field yet (only a generic ScalarProperties bag), so
	 * we start from this tunable. 0.5 = moderately clothed — a deliberately
	 * forgiving default so an un-tuned game never cooks or freezes the player.
	 * FLAG: replace with real clothing-warmth aggregation when equipment gains a
	 * warmth field (see report).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Vitals|Temperature",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float BaseInsulationFactor = 0.55f;   // skin + underclothes baseline. REAL warmth
	                                      // now comes from equipped clothing via
	                                      // IMOInsulationSource (Warmth item scalars) —
	                                      // dress for winter or stay by the fire.

	/**
	 * (H12) Extra insulation granted by full overhead cover (a roof). Scaled by
	 * the OverheadCover value from the shelter trace (0 open sky → 1 fully
	 * covered) and ADDED to BaseInsulationFactor, then clamped to 1.0. Being
	 * under cover blunts environmental drift (out of wind/sun/radiative loss).
	 * Default 0.35 → standing under a solid roof reaches ~0.85 insulation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Vitals|Temperature",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float ShelterInsulationBonus = 0.35f;

	/** One-shot missing-ambient-provider warning latch (per instance). */
	bool bWarnedNoAmbientProvider = false;

	// NOTE: TimeScaleMultiplier removed. The single source of truth is now
	// UMOGameClockSubsystem::GetTimeScale(). Tick handlers in this component
	// query GameClock->GetScaledDeltaTime(TickInterval). To change the
	// simulation speed for all medical components, set it on the subsystem.

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Fired when a vital sign changes significantly. */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnVitalSignChanged OnVitalSignChanged;

	/** Fired when blood loss stage changes. */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnBloodLossStageChanged OnBloodLossStageChanged;

	/** Fired ONCE when circulation fails (heart under 10 / min or blood volume under 30 %); fires again only after a recovery. */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnCardiacArrest OnCardiacArrest;

	/** Fired ONCE when breathing fails (SpO2 under 50 % or under 4 breaths / min); fires again only after a recovery. */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnRespiratoryFailure OnRespiratoryFailure;

	/**
	 * Fired once when a failure has lasted long enough to kill (mo.Medical.CardiacArrestDeathSeconds, default 300: the ischaemic window of the brain;
	 * mo.Medical.RespiratoryFailureDeathSeconds, default 240). Until then it is reversible: restore the blood volume or the oxygen and the clock resets.
	 * AMOCharacter turns it into its one death path (DieOf), with the cause carried to the death recap.
	 */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnSystemicDeath OnSystemicDeath;

	/**
	 * One step of the failure clocks (TickVitals calls it with the game-scaled tick). Public so tests can drive it without a world.
	 * Returns the failure that just became fatal (None otherwise; never twice).
	 */
	EMOSystemicFailure StepSystemicFailure(float DeltaSeconds);

	/** Seconds the current circulatory / respiratory failure has lasted (0 = none). */
	float GetCirculatoryFailureSeconds() const { return CirculatoryFailureSeconds; }
	float GetRespiratoryFailureSeconds() const { return RespiratoryFailureSeconds; }
	/** The heat dose taken so far, in minutes-at-42-C (0 = none; N-063). */
	float GetHeatStrokeDoseMinutes() const { return HeatStrokeDoseMinutes; }

	/** How fast a core of CoreC builds the heat-stroke dose (minutes-at-42-C per minute): 0 under 42 C, doubling each degree above (the
	 *  CEM43 thermal-dose rule). Pure, for the tests. */
	static float HeatDoseRatePerMinute(float CoreC);
	/** The failure that killed (None while alive). */
	EMOSystemicFailure GetFatalSystemicFailure() const { return FatalSystemicFailure; }

	/** Fired when any vital sign changes (for UI updates). */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnVitalsChanged OnVitalsChanged;

	/** Fired when activity level changes. UI can use this for stamina bar updates. */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnActivityChanged OnActivityChanged;

	/** Fired when stamina changes significantly. */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnStaminaChanged OnStaminaChanged;

	/** Fired when stamina is fully depleted. Movement system should respond by downgrading activity. */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnStaminaDepleted OnStaminaDepleted;

	/**
	 * Fired ONLY when the thermal-comfort BUCKET changes (not on every temp
	 * fluctuation). E.g., body temp dropping from 37.0 → 36.8 keeps the
	 * pawn in Comfortable and broadcasts nothing; 36.4 crosses ColdBelow
	 * and fires (Comfortable → Cold). Lets HUD widgets re-render on state
	 * change only — no per-tick spam.
	 */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnThermalComfortChanged OnThermalComfortChanged;

	/**
	 * Fired ONLY when wetness BUCKET crosses (Dry/Damp/Wet/Soaked). Continuous
	 * WetnessLevel ticks don't fire — moodle widget renders on state change only.
	 */
	UPROPERTY(BlueprintAssignable, Category="MO|Vitals|Events")
	FMOOnWetnessStateChanged OnWetnessStateChanged;

	// ============================================================================
	// WETNESS API
	// ============================================================================
	//
	// WetnessLevel rises while the pawn is in active precipitation that isn't
	// blocked by overhead cover, and decays toward zero when sheltered or in
	// clear weather. Bucket transitions broadcast OnWetnessStateChanged so the
	// HUD moodle strip can show a wet/damp/soaked icon.

	/** Current wetness 0–1 (raw accumulator). */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Wetness")
	float GetWetnessLevel() const { return WetnessLevel; }

	/** Current bucket derived from WetnessLevel + WetnessThresholds. */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Wetness")
	EMOWetnessState GetWetnessState() const;

	/**
	 * Force-set wetness. Used by cheats (MO.Player.SetWet) and save-restore.
	 * Clamps to [0,1] and fires OnWetnessStateChanged if the bucket transitions.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Wetness")
	void SetWetnessLevel(float NewLevel);

	// ============================================================================
	// BLOOD API
	// ============================================================================

	/**
	 * Apply blood loss (from wounds).
	 * @param AmountML Amount of blood lost in mL.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Blood")
	void ApplyBloodLoss(float AmountML);

	/**
	 * Apply blood transfusion.
	 * @param AmountML Amount of blood to add in mL.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Blood")
	void ApplyBloodTransfusion(float AmountML);

	/**
	 * Get current blood loss stage.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Blood")
	EMOBloodLossStage GetBloodLossStage() const;

	/**
	 * Get blood volume as percentage of max.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Blood")
	float GetBloodVolumePercent() const;

	// ============================================================================
	// EXERTION API
	// ============================================================================

	/**
	 * Set current exertion level (from movement/actions).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Exertion")
	void SetExertionLevel(float NewExertion);

	/**
	 * Add stress (from pain, fear, combat).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Exertion")
	void AddStress(float Amount);

	/**
	 * Set pain level (usually called by anatomy component).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Exertion")
	void SetPainLevel(float NewPain);

	/**
	 * Add fatigue (long-term exhaustion).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Exertion")
	void AddFatigue(float Amount);

	// ============================================================================
	// ACTIVITY API
	// ============================================================================
	// Integration Points:
	// - Movement Component: Call SetActivityLevel() when movement speed changes
	// - Crafting System: Call SetActivityLevel() when crafting starts/stops
	// - Building System: Call SetActivityLevel() when construction activity changes
	// - Combat System: Call SetActivityLevel(Combat) when entering combat
	// ============================================================================

	/**
	 * Set current activity level.
	 * This is the primary entry point for movement/action systems.
	 *
	 * @param NewActivity The new activity level
	 *
	 * Integration Example (in Character Movement Component):
	 *   void UpdateMovementMode()
	 *   {
	 *       if (VitalsComp)
	 *       {
	 *           if (IsSprinting())
	 *               VitalsComp->SetActivityLevel(EMOActivityLevel::Sprinting);
	 *           else if (IsRunning())
	 *               VitalsComp->SetActivityLevel(EMOActivityLevel::Jogging);
	 *           else if (IsWalking())
	 *               VitalsComp->SetActivityLevel(EMOActivityLevel::Walking);
	 *           else
	 *               VitalsComp->SetActivityLevel(EMOActivityLevel::Idle);
	 *       }
	 *   }
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Activity")
	void SetActivityLevel(EMOActivityLevel NewActivity);

	/**
	 * The breath-hold process (UMORespirationComponent) reports the oxygen reserve of the lungs + blood and the blood oxygen saturation it implies. SpO2 is capped to that value (it never RAISES the
	 * body's normal reading); a reserve of 1 lifts the cap. Authority-side.
	 */
	void SetAirwayOxygenReserve(float Reserve01, float SpO2Cap);

	/**
	 * The fraction (0..1) of the body in water, reported by the movement component. The environmental temperature pipeline then exchanges heat with the WATER instead of the air, through clothes that
	 * no longer insulate (water conducts about 25 times better than air and soaks the layers). Authority-side; 0 on land.
	 */
	void SetWaterImmersion(float Immersion01) { WaterImmersion = FMath::Clamp(Immersion01, 0.0f, 1.0f); }

	UFUNCTION(BlueprintPure, Category="MO|Vitals|Temperature")
	float GetWaterImmersion() const { return WaterImmersion; }

	/** Temperature (C) of open water given the local air feels-like temperature: lakes and seas follow the air slowly and never quite reach it (cold mountain lakes ~5 C, a warm summer sea ~24 C). */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Temperature")
	static float WaterTemperatureForAir(float AirFeelsLikeC);

	/**
	 * The heat exchange with the surroundings of a body Immersion (0..1) in water: blends the ambient toward the water temperature and strips the insulation (x (1 - 0.85 x immersion)).
	 * Pure (no actor state) so the automation tests can check it.
	 */
	static void ApplyWaterToExposure(float Immersion01, float AirFeelsLikeC, float& InOutAmbientC, float& InOutInsulation);

	/**
	 * How far blood loss lowers the systolic pressure (mmHg, <= 0) at a blood volume of VolumeRatio (1 = full), by the haemorrhage classes:
	 * under 15% lost the body compensates (0); 15-30% it still holds the systolic (down to -15, the pulse pressure narrows); 30-40% it falls
	 * (to -45, ~75 mmHg); past 40% it collapses (-75 at half the blood lost). Continuous, so a bleed lowers it smoothly. The diastolic moves
	 * half as much. Pure, for the automation tests. (Until Oct 18 2026 the bands were tested in the wrong order: a FULL volume read as the
	 * worst class, +60, so every healthy pawn showed 180/110 and a bleed never lowered the pressure.)
	 */
	static float BloodLossSystolicShift(float VolumeRatio);

	/** Oxygen reserve last reported by the respiration component (1 = full). */
	UFUNCTION(BlueprintPure, Category="MO|Vitals")
	float GetAirwayOxygenReserve() const { return AirwayOxygenReserve; }

	/**
	 * Get current activity level.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Activity")
	EMOActivityLevel GetActivityLevel() const { return Activity.CurrentActivity; }

	/**
	 * Get current activity config (calorie multiplier, stamina drain, etc.).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Activity")
	FMOActivityConfig GetCurrentActivityConfig() const;

	/**
	 * Check if current activity can be sustained (has enough stamina).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Activity")
	bool CanSustainCurrentActivity() const;

	/**
	 * Check if a given activity can be started (meets minimum stamina requirement).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Activity")
	bool CanStartActivity(EMOActivityLevel ActivityToCheck) const;

	/**
	 * Get current stamina as percentage (0-1).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Activity")
	float GetStaminaPercent() const { return Activity.GetStaminaPercent(); }

	/**
	 * Check if stamina is depleted.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Activity")
	bool IsStaminaDepleted() const { return Activity.IsStaminaDepleted(); }

	/**
	 * Manually modify stamina (for effects, items, etc.).
	 * @param Amount Amount to add (negative to drain)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Activity")
	void ModifyStamina(float Amount);

	/**
	 * Get current calorie burn multiplier (activity * fitness adjustments).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Activity")
	float GetCurrentCalorieMultiplier() const;

	// ============================================================================
	// TEMPERATURE API
	// ============================================================================

	/**
	 * Apply environmental temperature effect.
	 * @param AmbientTemp Ambient temperature in Celsius.
	 * @param InsulationFactor How well protected from environment (0-1, 1=fully insulated).
	 * @param DeltaTimeOverride (H12) Seconds of simulation this call represents. When <= 0
	 *        the function falls back to TickInterval. The internal environmental driver
	 *        was authored against TickInterval; pass the real poll interval here so a
	 *        throttled (slower-than-tick) caller integrates the right amount of drift
	 *        instead of silently under-counting it. Always still scaled by the clock TimeScale.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Temperature")
	void ApplyEnvironmentalTemperature(float AmbientTemp, float InsulationFactor, float DeltaTimeOverride = -1.0f);

	/**
	 * (N-178, Oct 19 2026) The core's heat balance over GameSeconds, pure: the body holds 37 C while its heat production (resting + shivering) covers
	 * what it loses through its insulation to AmbientC, or sweating sheds what it gains; only the deficit or excess moves the core, through the body's
	 * heat capacity -- hours, not seconds (the old drift settled ~10 C above the felt air within a minute: 31 C after 4 min of 10 C rain).
	 * Returns the new core temperature.
	 */
	static float AdvanceCoreTemperature(float CoreC, float AmbientC, float Insulation01, float GameSeconds,
		float HeatCapacityJPerK, float RestingWatts, float MaxShiverWatts, float MaxSweatWatts, float NakedConductanceWPerK);

	/** (N-178) The body's heat capacity (J/K): ~70 kg x 3.5 kJ/(kg K). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Vitals|Temperature", meta=(ClampMin="1000.0"))
	float BodyHeatCapacityJPerK = 245000.0f;

	/** (N-178) Heat the body makes at rest (W): basal metabolism. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Vitals|Temperature", meta=(ClampMin="0.0"))
	float RestingHeatWatts = 100.0f;

	/** (N-178) Extra heat shivering can make (W) when the body loses more than it makes at rest. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Vitals|Temperature", meta=(ClampMin="0.0"))
	float MaxShiveringWatts = 200.0f;

	/** (N-178) Heat sweating can shed (W) when the body gains more than it can lose otherwise. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Vitals|Temperature", meta=(ClampMin="0.0"))
	float MaxSweatCoolingWatts = 400.0f;

	/** (N-178) Heat flow from the core to the air per degree (W/K) with no insulation at all (skin, vasoconstricted); insulation scales it by
	 *  (1 - insulation). The felt temperature already carries the wind and the wet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Vitals|Temperature", meta=(ClampMin="0.0"))
	float NakedConductanceWPerK = 20.0f;

	/**
	 * Map current Vitals.BodyTemperature to a thermal-comfort bucket via the
	 * configured thresholds. HUD widgets call this to render an icon.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Temperature")
	EMOThermalComfort GetThermalComfort() const;

	// ============================================================================
	// GLUCOSE API
	// ============================================================================

	/**
	 * Apply glucose from food digestion.
	 * @param Amount Amount of glucose to add in mg/dL equivalent.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Glucose")
	void ApplyGlucose(float Amount);

	/**
	 * Consume glucose from activity.
	 * @param Amount Amount of glucose consumed.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Glucose")
	void ConsumeGlucose(float Amount);

	// ============================================================================
	// QUERY API
	// ============================================================================

	/**
	 * Get current vital signs (const reference for reading).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Query")
	const FMOVitalSigns& GetVitalSigns() const { return Vitals; }

	/**
	 * Check if in shock (hypovolemic/traumatic).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Query")
	bool IsInShock() const;

	/**
	 * Check if in critical condition.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Query")
	bool IsCritical() const;

	/**
	 * Get formatted blood pressure string.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Vitals|Query")
	FString GetBloodPressureString() const;

	// ============================================================================
	// PERSISTENCE
	// ============================================================================

	/**
	 * Build save data from current state.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Save")
	void BuildSaveData(FMOVitalsSaveData& OutSaveData) const;

	/**
	 * Apply save data to restore state (authority only).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Vitals|Save")
	bool ApplySaveDataAuthority(const FMOVitalsSaveData& InSaveData);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// ============================================================================
	// INTERNAL STATE
	// ============================================================================

	/** Timer for periodic vital sign calculations. */
	FTimerHandle TickTimerHandle;

	/** Tick interval in seconds. */
	float TickInterval = 0.5f;

	/** Previous blood loss stage for change detection. */
	EMOBloodLossStage PreviousBloodLossStage = EMOBloodLossStage::None;

	/**
	 * Last thermal-comfort bucket — for change detection on each tick. We
	 * broadcast OnThermalComfortChanged only when this differs from the
	 * fresh computation. Initial value Comfortable matches default 37 °C.
	 */
	EMOThermalComfort LastThermalComfort = EMOThermalComfort::Comfortable;

	/** Current wetness 0–1, integrated by UpdateWetness on each weather poll (server). Replicated to the owner (N-177, Oct 19 2026): the wet
	 *  moodle on a joiner's screen listens to OnWetnessStateChanged, which fired on the server only, so a joiner in the rain never saw it. */
	UPROPERTY(Transient, ReplicatedUsing=OnRep_WetnessLevel)
	float WetnessLevel = 0.0f;

	UFUNCTION()
	void OnRep_WetnessLevel();

	/** Fire OnWetnessStateChanged if WetnessLevel crossed into another bucket (server set or client OnRep). */
	void BroadcastWetnessStateIfChanged();

	/** Last wetness bucket — broadcast on transition only. */
	EMOWetnessState LastWetnessState = EMOWetnessState::Dry;

	/** Seconds since the last shelter trace + wetness integration. */
	float WetnessAccumulatorSeconds = 0.0f;

	/**
	 * (H12) Most recent overhead-cover value (0 open sky → 1 fully covered) from
	 * the wetness shelter trace. Cached so the environmental-temperature step can
	 * derive a shelter insulation bonus WITHOUT firing a second 9-ray trace.
	 * Updated inside UpdateWetness; only meaningful while it's raining (the trace
	 * is skipped in clear weather), so it decays toward 0 when no trace runs.
	 */
	float CachedOverheadCover = 0.0f;

	/** (H12) Seconds since the last environmental-temperature application. */
	float EnvironmentalTempAccumulatorSeconds = 0.0f;

	/**
	 * Per-tick step that integrates rain intensity + overhead shelter into
	 * WetnessLevel. Called from TickComponent at WetnessPollInterval to
	 * amortize the 9-ray shelter trace.
	 */
	void UpdateWetness(float DeltaSeconds);

	/**
	 * (H12) Throttled environmental-temperature step. Reads feels-like ambient
	 * from the weather subsystem, derives an insulation factor, and calls
	 * ApplyEnvironmentalTemperature with the real elapsed interval. Called from
	 * TickVitals at EnvironmentalTempUpdateInterval. No weather provider →
	 * weather subsystem returns its default temp, so this is a gentle no-op-ish
	 * pull toward 20°C, never a freeze/cook.
	 */
	void UpdateEnvironmentalTemperature(float DeltaSeconds);

	/** (H15) Cached adrenaline sibling (weak; resolved lazily via the medical interface). */
	UPROPERTY(Transient)
	TWeakObjectPtr<class UMOAdrenalineComponent> CachedAdrenalineComp;

	/**
	 * (H15) Resolve the owner's adrenaline component, caching it. Returns nullptr
	 * for pawns without one (creatures, etc) so the HR modifier is simply skipped.
	 * Mirrors the adrenaline component's own resolution: medical interface first,
	 * FindComponentByClass fallback.
	 */
	class UMOAdrenalineComponent* GetAdrenalineComponent();

	/** Convert WetnessLevel to bucket via WetnessThresholds. */
	EMOWetnessState ComputeWetnessState(float Level) const;

	/** Prevents multiple death triggers from blood loss. */
	bool bBloodLossDeathTriggered = false;

	/** Cached reference to anatomy component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOAnatomyComponent> CachedAnatomyComp;

	/** Cached reference to metabolism component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOMetabolismComponent> CachedMetabolismComp;

	/** Cached reference to mental state component. */
	UPROPERTY(Transient)
	TObjectPtr<UMOMentalStateComponent> CachedMentalComp;

	// ============================================================================
	// INTERNAL METHODS
	// ============================================================================

	/** Periodic tick to update vitals. */
	void TickVitals();

	/** Calculate heart rate based on all factors. */
	void CalculateHeartRate();

	/** Calculate blood pressure based on all factors. */
	void CalculateBloodPressure();

	/** Calculate respiratory rate. */
	void CalculateRespiratoryRate();

	/** Oxygen reserve + the SpO2 cap it implies, from UMORespirationComponent (defaults: full reserve, no cap). */
	float AirwayOxygenReserve = 1.0f;
	float AirwaySpO2Cap = 100.0f;

	/** Fraction of the body in water (SetWaterImmersion). */
	float WaterImmersion = 0.0f;

	/** How long the current circulatory / respiratory failure has lasted (game-scaled seconds; 0 = none). */
	float CirculatoryFailureSeconds = 0.0f;
	float RespiratoryFailureSeconds = 0.0f;
	/** The heat dose of the current overheating (minutes-at-42-C; reset once the core is back under 41 C). */
	float HeatStrokeDoseMinutes = 0.0f;
	/** Set once a failure became fatal: OnSystemicDeath never fires twice. */
	EMOSystemicFailure FatalSystemicFailure = EMOSystemicFailure::None;

	/** Calculate oxygen saturation. */
	void CalculateOxygenSaturation();

	/** Regenerate blood over time. */
	void RegenerateBlood(float DeltaTime);

	/** Process exertion recovery. */
	void ProcessExertionRecovery(float DeltaTime);

	/** Check for critical conditions. */
	void CheckCriticalConditions(float DeltaSeconds);

	/** Broadcast vital sign change if significant. */
	void CheckAndBroadcastChange(FName VitalName, float OldValue, float NewValue, float Threshold = 1.0f);

	/** Process activity-based calorie burn and stamina drain. */
	void ProcessActivityEffects(float DeltaTime);

	/** Update stamina based on current activity. */
	void UpdateStamina(float DeltaTime);

	/** Apply calorie burn to metabolism component. */
	void ApplyActivityCalorieBurn(float DeltaTime);

	/** Update maximum stamina based on fitness. */
	void UpdateMaxStamina();
};
