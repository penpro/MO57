/**
 * =============================================================================
 * MOBuildingTypes.h - Building System Type Definitions
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Contains all enums, structs, and delegates used by the building system.
 * Separates type definitions from implementation for clean include dependencies.
 *
 * -----------------------------------------------------------------------------
 * BUILDING SYSTEM OVERVIEW
 * -----------------------------------------------------------------------------
 *
 * The building system follows this workflow:
 *
 * 1. PLACEMENT MODE (UMOBuildingComponent)
 *    - Player presses B key -> Opens building menu
 *    - Player selects building -> Enters placement mode
 *    - Ghost actor spawned, follows camera trace
 *    - Ghost shows green (valid) or red (invalid) placement
 *    - Click to place, Escape to cancel
 *
 * 2. GHOST STATE (AMOBuildableActor with EMOBuildState::Ghost)
 *    - Building placed but not yet started
 *    - Player can interact to open build widget
 *    - Configure material sources (inventory, containers, area)
 *    - Click Start to begin construction
 *
 * 3. CONSTRUCTION (UMOBuildProgressComponent with EMOBuildState::Constructing)
 *    - Timed construction based on recipe TotalBuildTime
 *    - Materials consumed progressively based on weighted parts
 *    - Can be paused/resumed/cancelled
 *    - Visual updates during construction
 *
 * 4. COMPLETE (EMOBuildState::Complete)
 *    - Building fully functional
 *    - Interaction opens appropriate UI (crafting station, container, etc.)
 *
 * -----------------------------------------------------------------------------
 * DELEGATE FLOW
 * -----------------------------------------------------------------------------
 *
 * UMOBuildingComponent delegates:
 *   OnPlacementModeEntered(FName RecipeId)
 *     -> Broadcast when entering placement mode
 *     -> Listeners: UI systems for mode indicators
 *
 *   OnPlacementModeExited(bool bPlaced)
 *     -> Broadcast when exiting placement mode
 *     -> bPlaced=true if building was placed, false if cancelled
 *     -> Listeners: UI systems, input context managers
 *
 *   OnGhostPlaced(AMOBuildableActor* Ghost)
 *     -> Broadcast when ghost successfully placed in world
 *     -> Listeners: Persistence system (for tracking), UI
 *
 * UMOBuildProgressComponent delegates:
 *   OnConstructionStarted()
 *     -> Broadcast when construction begins
 *     -> Listeners: UI for progress display, audio
 *
 *   OnConstructionProgress(float Progress)
 *     -> Broadcast every tick with 0.0-1.0 progress
 *     -> Listeners: UI progress bars, visual effects
 *
 *   OnPartCompleted(int32 PartIndex, FMOBuildPart Part)
 *     -> Broadcast when a build part finishes
 *     -> Listeners: Audio (material placement sounds)
 *
 *   OnConstructionCompleted()
 *     -> Broadcast when construction finishes
 *     -> Listeners: AMOBuildableActor::OnConstructionCompleted_Implementation
 *
 *   OnConstructionCancelled()
 *     -> Broadcast when construction is cancelled
 *     -> Listeners: Refund systems, UI
 *
 *   OnMaterialNeeded(FName ItemId, int32 Quantity)
 *     -> Broadcast when construction pauses due to missing material
 *     -> Listeners: UI for material needed notification
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] GHOST LIFETIME: Ghost buildings persist until started or cancelled.
 *   They save/load with the world. Don't assume they're temporary.
 *
 * [2024-02] MATERIAL SOURCE PRIORITY: Sources are checked by priority.
 *   Inventory (100) before Containers (50) before WorldItems (25).
 *   See MOMaterialSourceInterface.h for details.
 *
 * [2024-02] PART ORDERING: Build parts execute in array order. Weighted parts
 *   consume proportionally - if part has Weight=2 and total is 10, it takes
 *   20% of total build time and materials.
 *
 * [2024-02] GATHER RANGE: GatherRange is in Unreal Units. 150 UU ≈ 5 feet.
 *   This affects which containers and world items can be used.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOBuildingComponent.h - Placement mode management
 * - MOBuildProgressComponent.h - Construction progress
 * - MOBuildableActor.h - Base class for buildings
 * - MOMaterialSourceInterface.h - Material gathering
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MORecipeDefinitionRow.h"
#include "MOBuildingTypes.generated.h"

class AMOBuildableActor;

// FMOBuildingPlacementData and FMOBuildPart are defined in MORecipeDefinitionRow.h
// (they are needed there for UPROPERTY usage in the recipe definition struct)

/**
 * Build state for placed buildings.
 */
UENUM(BlueprintType)
enum class EMOBuildState : uint8
{
	Ghost UMETA(DisplayName="Ghost"),           // Placed but not started
	Constructing UMETA(DisplayName="Constructing"), // Build in progress
	Paused UMETA(DisplayName="Paused"),         // Construction paused
	Complete UMETA(DisplayName="Complete")      // Fully built
};

/**
 * Sources from which materials can be drawn during construction.
 */
UENUM(BlueprintType)
enum class EMOMaterialSource : uint8
{
	Inventory UMETA(DisplayName="Inventory"),
	NearbyContainers UMETA(DisplayName="Nearby Containers"),
	SurroundingArea UMETA(DisplayName="Surrounding Area")
};

/** EMOMaterialSource as one bit of a uint8 (the sources a build request may draw from). */
inline constexpr uint8 MOMaterialSourceBit(EMOMaterialSource Source) { return static_cast<uint8>(1u << static_cast<uint8>(Source)); }
inline constexpr uint8 MOAllMaterialSources = 0x7;

/**
 * A construction step a player asks for on a placed building. The server runs it (UMOBuildingComponent::RequestBuildAction): the building's
 * state, the deposited materials and the inventories they come from are server state in co-op.
 */
UENUM(BlueprintType)
enum class EMOBuildRequest : uint8
{
	AddMaterials UMETA(DisplayName="Add Materials"),         // gather what is still missing from the enabled sources into the building
	Start UMETA(DisplayName="Start"),                        // start construction (all materials deposited), or resume a paused one
	Pause UMETA(DisplayName="Pause"),
	CancelAndRemove UMETA(DisplayName="Cancel And Remove"),  // ghost menu Cancel: skill-scaled refund to the builder, the building goes
	CancelKeepGhost UMETA(DisplayName="Cancel Keep Ghost")   // build queue Cancel: every deposited material dropped beside it, the ghost stays
};

/** One entry of the deposited-materials ledger, as it replicates (TMap does not). */
USTRUCT()
struct MOFRAMEWORK_API FMOBuildDeposit
{
	GENERATED_BODY()

	UPROPERTY()
	FName ItemId = NAME_None;

	UPROPERTY()
	int32 Count = 0;

	bool operator==(const FMOBuildDeposit& Other) const { return ItemId == Other.ItemId && Count == Other.Count; }
};

/** What clients need of a building's progress: the server publishes it (UMOBuildProgressComponent::PublishRepState), clients apply it to their FMOBuildProgress. */
USTRUCT()
struct MOFRAMEWORK_API FMOBuildRepState
{
	GENERATED_BODY()

	UPROPERTY()
	EMOBuildState State = EMOBuildState::Ghost;

	/** Server elapsed time when published (refreshed about once a second while constructing; clients advance it locally in between). */
	UPROPERTY()
	float ElapsedTime = 0.0f;

	/** Sorted by ItemId, so the same ledger always replicates the same bytes. */
	UPROPERTY()
	TArray<FMOBuildDeposit> Deposits;

	/** The build's whole duration, once the first builder's skill has scaled it (F31); 0 = the recipe's own, as every machine already knows it. */
	UPROPERTY()
	float TotalBuildTime = 0.0f;
};

/**
 * A building's SKILL line says how well the work goes, never whether it may be done (F31, Wes Oct 19 2026: "building skill slows the work and
 * wastes material, never gates"; PROGRESSION_ROADMAP section 1, "no hard locks"). Below the recipe's level, linearly in the shortfall: the work
 * takes longer (twice as long with no skill at all) and spoils part of its material along the way (a quarter of it with none). One rule for the
 * construction (UMOBuildProgressComponent) and for what the building menu shows.
 */
struct FMOBuildSkillPenalty
{
	static constexpr float MaxExtraTime = 1.0f;
	static constexpr float MaxWasteFraction = 0.25f;

	float TimeScale = 1.0f;
	float WasteFraction = 0.0f;

	static FMOBuildSkillPenalty For(int32 CurrentLevel, int32 RequiredLevel)
	{
		FMOBuildSkillPenalty Penalty;
		if (RequiredLevel > 0)
		{
			const float Shortfall = 1.0f - FMath::Clamp(static_cast<float>(CurrentLevel) / static_cast<float>(RequiredLevel), 0.0f, 1.0f);
			Penalty.TimeScale = 1.0f + MaxExtraTime * Shortfall;
			Penalty.WasteFraction = MaxWasteFraction * Shortfall;
		}
		return Penalty;
	}
};

/**
 * Build progress state for a building under construction.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBuildProgress
{
	GENERATED_BODY()

	/** Current build state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Progress")
	EMOBuildState State = EMOBuildState::Ghost;

	/** Total time in seconds required to build. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Progress")
	float TotalBuildTime = 0.0f;

	/** Time elapsed in construction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Progress")
	float ElapsedTime = 0.0f;

	/** Index of the current build part being processed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Progress")
	int32 CurrentPartIndex = 0;

	/** Progress (0.0-1.0) within the current part. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Progress")
	float CurrentPartProgress = 0.0f;

	/** Number of items/actions consumed per part (indexed by part). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Progress")
	TArray<int32> ConsumedPerPart;

	// --- Material Source Options ---

	/** If true, draw materials from player's inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Sources")
	bool bDrawFromInventory = true;

	/** If true, draw materials from nearby container actors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Sources")
	bool bDrawFromNearbyContainers = true;

	/** If true, gather materials from surrounding world items. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Sources")
	bool bDrawFromSurroundingArea = true;

	/** Range in Unreal Units for gathering materials (~150 UU = 5 feet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Sources", meta=(ClampMin="0.0"))
	float GatherRange = 150.0f;

	/**
	 * Materials deposited into this building (ItemId -> quantity). Lives in
	 * the save payload: these items were already removed from inventories,
	 * so a ledger outside the persisted struct destroys them on save/load.
	 * The build component owns this; UI displays and refunds FROM it, never
	 * from a widget-local copy.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Progress")
	TMap<FName, int32> DepositedMaterials;

	/** The first builder's skill against the recipe (FMOBuildSkillPenalty), applied when the work first starts: the build time's multiplier.
	 *  0 = not started yet; kept through pauses, cancels and saves so the time is never scaled twice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Progress")
	float SkillTimeScale = 0.0f;

	/** Units of material the unskilled work spoils along the way (F31): all of them, and those still to come (spread evenly through the work). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Progress")
	int32 SpoilsPlanned = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building|Progress")
	int32 SpoilsLeft = 0;

	/** Get overall progress (0.0 - 1.0). */
	float GetOverallProgress() const
	{
		if (TotalBuildTime <= 0.0f)
		{
			return State == EMOBuildState::Complete ? 1.0f : 0.0f;
		}
		return FMath::Clamp(ElapsedTime / TotalBuildTime, 0.0f, 1.0f);
	}

	/** Get remaining time in seconds. */
	float GetRemainingTime() const
	{
		return FMath::Max(0.0f, TotalBuildTime - ElapsedTime);
	}
};

/**
 * Per-part tracking data for material consumption during construction.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBuildPartProgress
{
	GENERATED_BODY()

	/** Number of items/actions consumed for this part. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building")
	int32 ConsumedCount = 0;

	/** Whether this part is fully complete. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building")
	bool bComplete = false;
};

// FMOPersistedBuildingRecord is defined in MOworldSaveGame.h with other save data structs

// ============================================================================
// DELEGATES
// ============================================================================

/** Broadcast when placement mode is entered. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnPlacementModeEnteredSignature, FName, RecipeId);

/** Broadcast when placement mode is exited. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnPlacementModeExitedSignature, bool, bPlaced);

/** Broadcast when a ghost building is placed in the world. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnGhostPlacedSignature, AMOBuildableActor*, Ghost);

/** Broadcast when construction starts on a building. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnConstructionStartedSignature);

/** Broadcast when construction progress updates. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnConstructionProgressSignature, float, Progress);

/** Broadcast when a build part completes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnPartCompletedSignature, int32, PartIndex, FMOBuildPart, Part);

/** Broadcast when construction completes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnConstructionCompletedSignature);

/** Broadcast when construction is cancelled. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnConstructionCancelledSignature);

/** Broadcast when a material is needed during construction. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnMaterialNeededSignature, FName, ItemId, int32, Quantity);

/** Broadcast when construction state changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnConstructionStateChangedSignature, EMOBuildState, NewState);

/** Broadcast when the deposited-materials ledger changes (a deposit, a cancel, or the server's ledger arriving on a client). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnConstructionDepositsChangedSignature);
