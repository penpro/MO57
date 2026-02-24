#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOSpawnTypes.h"
#include "MORecruitmentComponent.generated.h"

class UMOInventoryComponent;
class UMOSkillsComponent;

/**
 * Save data for recruitment component.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMORecruitmentSaveData
{
	GENERATED_BODY()

	/** Current recruitment state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	EMORecruitmentState RecruitmentState = EMORecruitmentState::Wandering;

	/** Active quest data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FMORecruitmentQuest ActiveQuest;

	/** When the quest started (ticks since epoch) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	int64 QuestStartTimeTicks = 0;

	/** Whether this save data is valid/populated */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	bool bHasValidData = false;
};

/**
 * Delegate fired when recruitment state changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnRecruitmentStateChanged, EMORecruitmentState, OldState, EMORecruitmentState, NewState);

/**
 * Delegate fired when a recruitment quest is completed.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnRecruitmentQuestCompleted);

/**
 * Delegate fired when a recruitment quest fails.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnRecruitmentQuestFailed);

/**
 * Component that manages survivor recruitment state and quest requirements.
 *
 * Survivors spawn with this component in Wandering state. When interacted with:
 * - If quest type is None: immediate recruitment
 * - Otherwise: quest is assigned and survivor becomes persistent
 *
 * Once recruited, the pawn becomes possessable by the possession system.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMORecruitmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMORecruitmentComponent();

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Possible quests this survivor can have (random selection on spawn) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recruitment")
	TArray<FMORecruitmentQuest> PossibleQuests;

	/** Chance that this survivor requires a quest (0.0 = never, 1.0 = always) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recruitment", meta = (ClampMin = "0", ClampMax = "1"))
	float QuestChance = 0.5f;

	// ============================================================================
	// STATE
	// ============================================================================

	/** Current recruitment state */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RecruitmentState, Category = "Recruitment")
	EMORecruitmentState RecruitmentState = EMORecruitmentState::Wandering;

	/** Currently active quest (if any) */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Recruitment")
	FMORecruitmentQuest ActiveQuest;

	/** When the current quest was started */
	UPROPERTY(BlueprintReadOnly, Category = "Recruitment")
	FDateTime QuestStartTime;

	// ============================================================================
	// EVENTS
	// ============================================================================

	/** Fired when recruitment state changes */
	UPROPERTY(BlueprintAssignable, Category = "Recruitment|Events")
	FMOOnRecruitmentStateChanged OnRecruitmentStateChanged;

	/** Fired when a quest is completed successfully */
	UPROPERTY(BlueprintAssignable, Category = "Recruitment|Events")
	FMOOnRecruitmentQuestCompleted OnQuestCompleted;

	/** Fired when a quest fails (timeout or other condition) */
	UPROPERTY(BlueprintAssignable, Category = "Recruitment|Events")
	FMOOnRecruitmentQuestFailed OnQuestFailed;

	// ============================================================================
	// API
	// ============================================================================

	/** Begin interaction with this survivor. Returns true if interaction started successfully. */
	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	bool BeginInteraction(APawn* InteractingPawn);

	/** Attempt to complete the active quest. Returns true if quest completed. */
	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	bool TryCompleteQuest(APawn* InteractingPawn);

	/** Force recruit this survivor (bypasses quest requirements) */
	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	void ForceRecruit();

	/** Fail the current quest */
	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	void FailQuest();

	// ============================================================================
	// QUERIES
	// ============================================================================

	/** Check if this survivor is possessable (fully recruited) */
	UFUNCTION(BlueprintPure, Category = "Recruitment")
	bool IsPossessable() const { return RecruitmentState == EMORecruitmentState::Recruited; }

	/** Check if this survivor has an active quest */
	UFUNCTION(BlueprintPure, Category = "Recruitment")
	bool HasActiveQuest() const { return RecruitmentState == EMORecruitmentState::QuestActive; }

	/** Check if this survivor is waiting for interaction */
	UFUNCTION(BlueprintPure, Category = "Recruitment")
	bool IsWandering() const { return RecruitmentState == EMORecruitmentState::Wandering; }

	/** Get remaining time on the quest (seconds) */
	UFUNCTION(BlueprintPure, Category = "Recruitment")
	float GetQuestTimeRemaining() const;

	/** Check if an interacting pawn can complete the current quest */
	UFUNCTION(BlueprintPure, Category = "Recruitment")
	bool CanCompleteQuest(APawn* InteractingPawn) const;

	/** Get quest progress text for UI */
	UFUNCTION(BlueprintPure, Category = "Recruitment")
	FText GetQuestProgressText() const;

	// ============================================================================
	// SAVE/LOAD
	// ============================================================================

	/** Build save data for persistence. */
	UFUNCTION(BlueprintCallable, Category = "Recruitment|Save")
	void BuildSaveData(FMORecruitmentSaveData& OutSaveData) const;

	/** Apply save data (authority only). */
	UFUNCTION(BlueprintCallable, Category = "Recruitment|Save")
	bool ApplySaveDataAuthority(const FMORecruitmentSaveData& InSaveData);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_RecruitmentState();

private:
	void SetRecruitmentState(EMORecruitmentState NewState);
	void RollRandomQuest();
	void CheckQuestTimeout();
	void NotifySpawnManagerOfInteraction();

	bool CheckQuestRequirements(APawn* InteractingPawn, bool bConsumeItems) const;
	bool HasRequiredItem(UMOInventoryComponent* Inventory) const;
	bool HasRequiredSkill(UMOSkillsComponent* Skills) const;

	/**
	 * Ensure the pawn has a survivor AI controller if recruited.
	 * Called after applying save data when the Recruited state is loaded.
	 */
	void EnsureSurvivorAIController();
};
