/**
 * =============================================================================
 * MOColonyManagerSubsystem.h - Settlement loop (pipeline V1, task #170)
 * =============================================================================
 *
 * PURPOSE:
 * The window into the community (colony design pillar): a settlement record,
 * a residency map, and the UPKEEP TICK that keeps villagers alive through the
 * REAL simulations —
 *   - hungry AI villagers eat REAL food withdrawn from communal storage via
 *     UMOMetabolismComponent::ConsumeFood (the player's own eating path);
 *     empty storage means real hunger, and the existing metabolism cascade
 *     (glycogen -> fat -> starvation) does the rest. No abstract food points.
 *   - mood is a DERIVED score over the real sims (starvation, dehydration,
 *     wetness, shock/stress/fatigue, housing), modulated by the personality
 *     Stability axis. ComputeVillagerMood is a pure static so headless
 *     automation can pin the math.
 *   - housing: residency (pawn GUID <-> house building GUID) capacity-checked
 *     against the recipe's HousingCapacity; unhoused villagers accrue
 *     UnhousedHours and their mood decays. (Deeper shelter-quality coupling —
 *     insulation, overhead trace at the house — is the V2 slice; flagged.)
 *   - every meal / mood swing lands in UMOCharacterHistoryComponent.
 *
 * SAVE: FMOColonySaveData captured/restored by UMOPersistenceSubsystem
 * alongside the other world-subsystem sections.
 *
 * =============================================================================
 * RELATED FILES: MOColonyTypes.h, MOCharacterHistoryComponent.h,
 *   MOMetabolismComponent.h, MOPersonalityComponent.h, Docs/MO57_Master_Plan.md
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOColonyTypes.h"
#include "MODesignationTypes.h"
#include "MOSurvivorJobTypes.h"

#include "MOSaveDomainInterface.h"
#include "MOColonyManagerSubsystem.generated.h"

class AMOContainerActor;
class APawn;

/** One excavation trip the colony should start now (terraform unit 3, stage 4): which dig zone, and where its spoil goes. */
struct FMOExcavationAssignment
{
	FGuid DigZoneId;
	EMOExcavateDumpMode DumpMode = EMOExcavateDumpMode::FillZone;
	/** FillZone mode: the designated fill zone that takes the earth. */
	FGuid DumpZoneId;
	/** Container mode: the dig zone's own default container (by identity GUID), or invalid = the nearest settlement container. */
	FGuid DumpContainerGuid;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnVillagerMoodChanged, const FGuid&, PawnGuid, float, NewMood);

UCLASS()
class MOFRAMEWORK_API UMOColonyManagerSubsystem : public UWorldSubsystem, public IMOSaveDomain
{
	GENERATED_BODY()

public:
	//~ Begin IMOSaveDomain (Colony save data lives here, not in the persistence subsystem)
	virtual FName GetSaveDomainName() const override { return TEXT("Colony"); }
	virtual int32 GetSaveDomainApplyPriority() const override { return 60; }
	virtual void CaptureSaveDomain(UMOWorldSaveGame& Save) override;
	virtual void ApplySaveDomain(const UMOWorldSaveGame& Save) override;
	//~ End IMOSaveDomain

	static UMOColonyManagerSubsystem* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// =========================================================================
	// SETTLEMENT
	// =========================================================================

	/** Found the settlement (V1: exactly one). Idempotent — refounding moves it. */
	UFUNCTION(BlueprintCallable, Category="MO|Colony")
	const FMOSettlementRecord& FoundSettlement(const FString& Name, const FVector& Center, float Radius = 20000.0f);

	UFUNCTION(BlueprintPure, Category="MO|Colony")
	const FMOSettlementRecord& GetSettlement() const { return Settlement; }

	UFUNCTION(BlueprintPure, Category="MO|Colony")
	bool IsInSettlement(const FVector& Location) const;

	/** Recruited, GUID-identified pawns (the roster). Player-possessed included. */
	UFUNCTION(BlueprintCallable, Category="MO|Colony")
	TArray<APawn*> GetColonyRoster() const;

	// =========================================================================
	// HOUSING
	// =========================================================================

	/** Assign a villager to a house (capacity from the recipe's HousingCapacity). */
	UFUNCTION(BlueprintCallable, Category="MO|Colony")
	bool AssignResidence(APawn* Villager, AActor* House);

	UFUNCTION(BlueprintCallable, Category="MO|Colony")
	void ClearResidence(const FGuid& PawnGuid);

	UFUNCTION(BlueprintPure, Category="MO|Colony")
	bool HasResidence(const FGuid& PawnGuid) const { return Residency.Contains(PawnGuid); }

	UFUNCTION(BlueprintPure, Category="MO|Colony")
	int32 GetHouseOccupancy(const FGuid& HouseGuid) const;

	// =========================================================================
	// MOOD
	// =========================================================================

	/**
	 * THE mood function — pure math over real-sim inputs so headless tests
	 * pin it. 0..1; 0.5 is neutral-content. Negative pressures (hunger, cold,
	 * trauma, homelessness) are amplified by the personality variance
	 * modifier: volatile characters crash harder.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Colony")
	static float ComputeVillagerMood(const FMOVillagerMoodInputs& Inputs);

	UFUNCTION(BlueprintPure, Category="MO|Colony")
	float GetVillagerMood(const FGuid& PawnGuid) const;

	UPROPERTY(BlueprintAssignable, Category="MO|Colony")
	FMOOnVillagerMoodChanged OnVillagerMoodChanged;

	// =========================================================================
	// QUOTAS / STANDING ORDERS (V2.1)
	// =========================================================================

	/** Set (or update) the standing order for an item. TargetCount 0 removes it. */
	UFUNCTION(BlueprintCallable, Category="MO|Colony")
	void SetQuota(FName OutputItemId, FName RecipeId, int32 TargetCount, int32 Priority = 0);

	UFUNCTION(BlueprintPure, Category="MO|Colony")
	const TArray<FMOColonyQuota>& GetQuotas() const { return Quotas; }

	/**
	 * THE quota decision — pure math for headless tests. Returns the recipes
	 * to assign right now, highest priority first: quotas whose stock is
	 * below target, not already in flight, capped at IdleVillagers.
	 */
	static TArray<FName> DecideQuotaWork(const TArray<FMOColonyQuota>& InQuotas,
		const TMap<FName, int32>& Stock, const TSet<FName>& RecipesInFlight, int32 IdleVillagers);

	// =========================================================================
	// EXCAVATION (terraform unit 3, stage 4: villagers work the designated dig zones)
	// =========================================================================

	/**
	 * THE excavation dispatch decision -- pure math for headless tests. One worker per Dig zone that still has earth to move
	 * (RemainingVolumeM3 > 0) and no trip queued or running (ClaimedDigZones), in the zones' order, capped at IdleVillagers.
	 * Where the spoil goes, in order: the zone's DefaultDumpTargetGuid when it names a fill (Dump) zone with room; the nearest
	 * fill zone with room; a container (the default target when it is not a zone, else any settlement container when
	 * bHaveContainer); otherwise the zone WAITS -- with nowhere to put the earth the honest answer is no trip, not a heap.
	 * A Flatten zone in FlattenZonesWithWork (the caller read the live ground: it has spots above AND below its target) is its own
	 * source and sink: dig its high ground, fill its low ground. Dump zones are never dug. (Exhausted zones are already gone: the
	 * designation subsystem drops a zone whose budget reaches 0.)
	 */
	static TArray<FMOExcavationAssignment> DecideExcavationWork(const TArray<FMODesignationZone>& Zones,
		const TSet<FGuid>& ClaimedDigZones, int32 IdleVillagers, bool bHaveContainer, const TSet<FGuid>& FlattenZonesWithWork = TSet<FGuid>());

	// =========================================================================
	// SCHOOL / TEACHING (V2.2)
	// =========================================================================

	/**
	 * THE teaching function — pure math for headless tests. XP a student
	 * gains per pass: 2x the direct-action baseline (design: being taught by
	 * a skilled pawn is twice as fast as learning by doing).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Colony")
	static float ComputeTeachXP(float GameHoursElapsed, float BaseXPPerGameHour);

	/** Villagers within this range of a completed School are IN school:
	 *  whole skill tree maintained (no decay) + teachable. */
	UPROPERTY(EditAnywhere, Category="MO|Colony|School")
	float SchoolRadius = 1500.0f;

	/** Direct-action XP baseline the 2x teaching multiplier applies to. */
	UPROPERTY(EditAnywhere, Category="MO|Colony|School")
	float TeachBaseXPPerGameHour = 20.0f;

	/** Teacher must lead the student by this many levels in the taught skill. */
	UPROPERTY(EditAnywhere, Category="MO|Colony|School")
	int32 TeachTeacherLevelMargin = 2;

	// =========================================================================
	// RELATIONSHIPS / STANDING (V2.3)
	// =========================================================================

	/**
	 * Colony standing of a candidate: mean relationship Strength toward the
	 * current roster. Gates recruitment — strangers must spend REAL time near
	 * settlers before they'll join (ForceRecruit dev verb bypasses).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Colony|Relationships")
	float GetColonyStanding(APawn* Candidate) const;

	UFUNCTION(BlueprintCallable, Category="MO|Colony|Relationships")
	bool CanRecruitByStanding(APawn* Candidate) const;

	/** The standing gate for a candidate WITH the numbers behind it, so a refusal can tell the player why (and how long it would take). */
	struct FStandingGate
	{
		bool bGateActive = false;     // false: the colony is too small to judge a stranger (see RecruitGateMinRoster)
		bool bPasses = true;
		float Standing = 0.0f;        // the candidate's mean standing toward the roster, 0..1
		float Needed = 0.0f;
		float HoursToPass = 0.0f;     // game hours of shared time still needed at the current standing (0 when it passes)
	};
	FStandingGate EvaluateRecruitGate(APawn* Candidate) const;

	/** Pure: does the standing gate judge strangers in a colony with this many settlers (besides the candidate)? Only from RecruitGateMinRoster up. */
	static bool GateApplies(int32 OthersOnRoster, int32 MinRoster) { return OthersOnRoster >= MinRoster; }

	/** Pure: game hours of shared time (by the same bond function the standing grows with) until a standing of `Standing` reaches `Needed`; 0 if it already has. */
	static float HoursToReachStanding(float Standing, float Needed);

	/**
	 * The gate only applies once the colony has at least this many settlers besides the candidate. A founder alone (or a founder and one friend) is not a settlement that
	 * strangers have to be vouched into: the first survivors you meet simply team up with you. (The gate used to apply to the very FIRST recruit -- the founder counted as "the colony" --
	 * and with standing growing 2% per shared game hour on a real-time clock that took ~8 real hours standing next to her, with nothing on screen saying so.)
	 */
	UPROPERTY(EditAnywhere, Category="MO|Colony|Relationships", meta=(ClampMin="1"))
	int32 RecruitGateMinRoster = 3;

	/** Marriage — V2.3 DATA MODEL only (courtship sim is V2.5). Sets Spouse
	 *  both ways with the family strength floor. */
	UFUNCTION(BlueprintCallable, Category="MO|Colony|Relationships")
	bool Marry(APawn* A, APawn* B);

	/** Record a parent->child bond both ways (data model; births are V2.5). */
	UFUNCTION(BlueprintCallable, Category="MO|Colony|Relationships")
	bool RecordParentage(APawn* Parent, APawn* Child);

	/** Villagers within this range of each other share time (familiarity). */
	UPROPERTY(EditAnywhere, Category="MO|Colony|Relationships")
	float FamiliarityRadius = 1200.0f;

	/** Minimum mean standing toward the roster before a stranger will join. */
	UPROPERTY(EditAnywhere, Category="MO|Colony|Relationships")
	float RecruitStandingThreshold = 0.15f;

	// =========================================================================
	// FAMILY / DYNASTY (V2.5) — courtship, marriage, children as REAL sim:
	// bonds must grow to romance, romance must last to marry, gestation runs
	// on the game clock, and a birth is a REAL new villager with parents.
	// =========================================================================

	/** THE courtship gate — pure math for headless tests. Romance forms only
	 *  when the bond is strong BOTH ways (one-sided fondness is not a couple). */
	UFUNCTION(BlueprintPure, Category="MO|Colony|Family")
	static bool ShouldBecomeRomantic(float StrengthAB, float StrengthBA, float MutualThreshold);

	/** THE gestation clock — pure math for headless tests. 0..1. */
	UFUNCTION(BlueprintPure, Category="MO|Colony|Family")
	static float ComputePregnancyProgress(double ConceivedGameSeconds, double NowGameSeconds, float InGestationGameHours);

	UFUNCTION(BlueprintPure, Category="MO|Colony|Family")
	int32 GetPregnancyCount() const { return Pregnancies.Num(); }

	/** Mutual bond strength required before a pair turns Romantic. */
	UPROPERTY(EditAnywhere, Category="MO|Colony|Family")
	float RomanceMutualStrengthThreshold = 0.6f;

	/** Romantic + co-located this many game hours -> marriage. */
	UPROPERTY(EditAnywhere, Category="MO|Colony|Family")
	float MarryAfterRomanticGameHours = 48.0f;

	/** Real gestation on the game clock (~9 game-months by default). */
	UPROPERTY(EditAnywhere, Category="MO|Colony|Family")
	float GestationGameHours = 6480.0f;

	/** Conception chance per shared game-day for a married, co-housed couple. */
	UPROPERTY(EditAnywhere, Category="MO|Colony|Family")
	float ConceptionChancePerGameDay = 0.10f;

	// =========================================================================
	// WINTER SHELTER (V2.5/F1) — cold villagers seek warmth. Body temp below
	// the seek threshold sends an AI villager to the nearest LIT hearth
	// (F1 heat sources), or its residence when nothing burns nearby (roof
	// cover = insulation, see MOVitalsComponent); recovery past the release
	// threshold frees them back to work.
	// =========================================================================

	UPROPERTY(EditAnywhere, Category="MO|Colony|Shelter")
	float ColdSeekShelterBodyTempC = 36.2f;

	UPROPERTY(EditAnywhere, Category="MO|Colony|Shelter")
	float ColdShelterReleaseBodyTempC = 36.8f;

	/** How far a cold villager will walk to a lit fire (cm). */
	UPROPERTY(EditAnywhere, Category="MO|Colony|Shelter", meta=(ClampMin="0.0"))
	float HearthSeekRadiusCm = 15000.0f;

	/** Hearth pass (F1/T3): a settlement fuel station at or below this fuel
	 *  fraction gets a refuel job issued to an idle villager. */
	UPROPERTY(EditAnywhere, Category="MO|Colony|Shelter", meta=(ClampMin="0.0", ClampMax="1.0"))
	float HearthRefuelBelowPercent = 0.4f;

	/** Fuel items a refueling villager withdraws per trip. */
	UPROPERTY(EditAnywhere, Category="MO|Colony|Shelter", meta=(ClampMin="1"))
	int32 HearthFuelWithdrawCount = 5;

	// =========================================================================
	// UPKEEP (driven by timer; public so tests can force a tick)
	// =========================================================================

	UFUNCTION(BlueprintCallable, Category="MO|Colony")
	void RunUpkeepTick();

	// =========================================================================
	// SAVE / LOAD
	// =========================================================================

	FMOColonySaveData BuildSaveData() const;
	bool ApplySaveDataAuthority(const FMOColonySaveData& InData);

	/** Real seconds between upkeep passes (game-time effects scale inside). */
	UPROPERTY(EditAnywhere, Category="MO|Colony")
	float UpkeepIntervalSeconds = 10.0f;

	/** Villager eats when calorie balance drops below this (kcal) or is starving. */
	UPROPERTY(EditAnywhere, Category="MO|Colony")
	float EatCalorieDeficitThreshold = -400.0f;

private:
	void TryFeedVillager(APawn* Villager, const FGuid& PawnGuid);
	AMOContainerActor* FindCommunalFood(FName& OutItemId) const;
	void RunQuotaPass(const TArray<APawn*>& Roster);
	void RunHearthPass(const TArray<APawn*>& Roster);
	void RunExcavationPass(const TArray<APawn*>& Roster);
	void RunSchoolPass(const TArray<APawn*>& Roster, float GameHoursElapsed);
	void RunRelationshipPass(const TArray<APawn*>& Roster, float GameHoursElapsed);
	void RunFamilyPass(const TArray<APawn*>& Roster, float GameHoursElapsed, double NowGameSeconds);
	void RunShelterPass(const TArray<APawn*>& Roster);
	void SpawnChild(APawn* Mother, const FGuid& MotherGuid, const FMOPregnancy& Pregnancy);
	static FGuid GetPawnGuid(const APawn* Pawn);

	UPROPERTY() FMOSettlementRecord Settlement;
	UPROPERTY() TArray<FMOColonyQuota> Quotas;
	UPROPERTY() TMap<FGuid, FGuid> Residency;
	UPROPERTY() TMap<FGuid, float> VillagerMood;
	UPROPERTY() TMap<FGuid, float> VillagerUnhousedHours;
	UPROPERTY() TMap<FGuid, FMOPregnancy> Pregnancies;

	/** Villagers currently ordered home to warm up (transient, not saved). */
	TSet<FGuid> ShelteringVillagers;

	FTimerHandle UpkeepTimer;
	double LastUpkeepGameSeconds = -1.0;
};
