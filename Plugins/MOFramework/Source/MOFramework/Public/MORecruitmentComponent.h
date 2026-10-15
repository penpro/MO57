/**
 * =============================================================================
 * MORecruitmentComponent.h - Survivor Recruitment State & Quests
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Manages survivor recruitment state and quest requirements. Survivors spawn
 * in Wandering state and transition through quest completion to Recruited,
 * at which point they become possessable by the player.
 *
 * KEY RESPONSIBILITIES:
 * 1. Track recruitment state (Wandering, QuestActive, Recruited)
 * 2. Manage recruitment quests (item delivery, skill checks)
 * 3. Handle quest timeout and failure
 * 4. Notify spawn manager when survivors become persistent
 * 5. Support save/load of recruitment state
 *
 * OWNERSHIP:
 * - Owner: Survivor pawn (AMOCharacter)
 * - Lifespan: Exists for pawn lifetime
 *
 * RECRUITMENT STATE MACHINE:
 * Wandering -> (player interaction) -> QuestActive (if quest) or Recruited
 * QuestActive -> (quest complete) -> Recruited
 * QuestActive -> (timeout/fail) -> Wandering or despawn
 *
 * QUEST TYPES:
 * - None: Immediate recruitment on interaction
 * - ItemDelivery: Requires specific item(s) from player inventory
 * - SkillCheck: Requires player to have skill at level
 * - Custom: Blueprint-defined requirements
 *
 * INTERACTION FLOW:
 * Player interacts -> BeginInteraction(InteractingPawn)
 * -> If no quest required: ForceRecruit()
 * -> Else: RollRandomQuest() -> Set QuestActive
 * Player returns -> TryCompleteQuest(Pawn)
 * -> CheckQuestRequirements() -> If pass: Recruit, else: show requirements
 *
 * PERSISTENCE INTEGRATION:
 * - QuestActive survivors become tracked in persistence
 * - NotifySpawnManagerOfInteraction() marks as persistent
 * - BuildSaveData/ApplySaveDataAuthority for save/load
 * - EnsureSurvivorAIController() sets controller after load
 *
 * CRITICAL PATTERNS:
 * 1. STATE CHANGE: Use SetRecruitmentState() for broadcasts
 * 2. QUEST ROLL: RollRandomQuest() uses QuestChance and PossibleQuests
 * 3. ITEM CONSUMPTION: CheckQuestRequirements can optionally consume
 *
 * KNOWN PITFALLS:
 * 1. REPLICATION: RecruitmentState replicated, OnRep updates locally
 * 2. QUEST TIMEOUT: CheckQuestTimeout runs in Tick, may despawn survivor
 * 3. AI CONTROLLER: EnsureSurvivorAIController needed after load
 * 4. SILENT REFUSAL (fixed 2026-10-10): the standing gate used to return a bare `false` and log a warning, so a survivor you could not recruit looked like a broken interaction
 *    ("no quest or anything pops up"). BeginInteractionDetailed returns an EMORecruitmentOutcome (+ the standing numbers) and AMOCharacter::HandleRecruitmentInteraction sends it to the
 *    player's screen through AMOPlayerController::ClientShowNotification. Add a new refusal reason as a new outcome, never as another `return false`.
 * 5. THE GATE AND THE FIRST SURVIVOR: UMOColonyManagerSubsystem::RecruitGateMinRoster (3) -- the standing gate only judges strangers once the colony has that many settlers; before that
 *    the survivors you meet simply join (the founder alone counted as "the colony" and made the first recruit take ~8 real hours on the real-time clock).
 *
 * RELATED FILES:
 * - MOSpawnTypes.h - EMORecruitmentState, FMORecruitmentQuest
 * - MOSurvivorController.h - AI controller for recruited pawns
 * - MOPossessionSubsystem.h - Possession of recruited pawns
 * - MOSpawnManager.h - Tracks persistent survivors
 *
 * TESTING CHECKLIST:
 * [ ] Wandering survivor shows recruitment prompt
 * [ ] Quest assigned correctly based on QuestChance
 * [ ] Item requirements check player inventory
 * [ ] Skill requirements check player skills
 * [ ] Quest timeout fails recruitment
 * [ ] Recruited survivor is possessable
 * [ ] Save/load preserves recruitment state
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

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
 * What came of a player's interaction with a survivor. Every refusal has its own value so the UI can say WHY: a bare `false` used to leave the player standing in front of a survivor
 * who "did nothing", with no quest and no message (the standing gate refused silently).
 */
UENUM(BlueprintType)
enum class EMORecruitmentOutcome : uint8
{
	NotRecruitable		UMETA(DisplayName = "Not recruitable"),		// animals and other pawns that never join
	Unavailable			UMETA(DisplayName = "Unavailable"),			// dead
	AlreadyRecruited	UMETA(DisplayName = "Already recruited"),
	DeclinedStanding	UMETA(DisplayName = "Declined (standing)"),	// does not know the colony well enough yet: see Standing / StandingNeeded / HoursToPass
	QuestStarted		UMETA(DisplayName = "Quest started"),		// the survivor asked for something; the quest is now active
	QuestPending		UMETA(DisplayName = "Quest pending"),		// a quest is active and its requirements are not met yet
	Recruited			UMETA(DisplayName = "Recruited")
};

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMORecruitmentResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Recruitment")
	EMORecruitmentOutcome Outcome = EMORecruitmentOutcome::NotRecruitable;

	/** DeclinedStanding: the candidate's standing toward the colony, what is needed, and how many game hours of shared time that still takes. */
	UPROPERTY(BlueprintReadOnly, Category = "Recruitment")
	float Standing = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Recruitment")
	float StandingNeeded = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Recruitment")
	float HoursToPass = 0.0f;

	/** The interaction moved the recruitment forward (quest given, or joined). */
	bool Succeeded() const { return Outcome == EMORecruitmentOutcome::Recruited || Outcome == EMORecruitmentOutcome::QuestStarted; }
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

	/**
	 * Whether this pawn can be recruited at all.
	 * Set to false for creatures (deer, wolves) that shouldn't be recruitable.
	 * Can be used later for tameable animals by enabling this flag.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recruitment")
	bool bIsRecruitable = true;

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

	/** Begin interaction with this survivor. Returns true if interaction started successfully (quest given or joined); BeginInteractionDetailed says what happened when it did not. */
	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	bool BeginInteraction(APawn* InteractingPawn);

	/** Same interaction, with the full outcome (and the standing numbers behind a refusal). */
	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	FMORecruitmentResult BeginInteractionDetailed(APawn* InteractingPawn);

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

	/** Check if this pawn can be recruited (not a creature, or a tameable animal) */
	UFUNCTION(BlueprintPure, Category = "Recruitment")
	bool IsRecruitable() const { return bIsRecruitable; }

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
	/** BeginInteractionDetailed's decision; the caller reports a recruit to the recruiting player's quests once, whatever path recruited. */
	FMORecruitmentResult ResolveInteraction(APawn* InteractingPawn);

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
