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

#include "MOColonyManagerSubsystem.generated.h"

class AMOContainerActor;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnVillagerMoodChanged, const FGuid&, PawnGuid, float, NewMood);

UCLASS()
class MOFRAMEWORK_API UMOColonyManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
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
	static FGuid GetPawnGuid(const APawn* Pawn);

	UPROPERTY() FMOSettlementRecord Settlement;
	UPROPERTY() TMap<FGuid, FGuid> Residency;
	UPROPERTY() TMap<FGuid, float> VillagerMood;
	UPROPERTY() TMap<FGuid, float> VillagerUnhousedHours;

	FTimerHandle UpkeepTimer;
	double LastUpkeepGameSeconds = -1.0;
};
