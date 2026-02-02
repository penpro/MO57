#pragma once

#include "CoreMinimal.h"
#include "MORecipeDefinitionRow.h"

#include "MOBuildingTypes.generated.h"

/**
 * =============================================================================
 * MOBuildingTypes - Building System Type Definitions
 * =============================================================================
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
 */

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
