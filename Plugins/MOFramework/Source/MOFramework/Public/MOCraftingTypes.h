/**
 * =============================================================================
 * MOCraftingTypes.h - Crafting System Type Definitions
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Contains all enums, structs, delegates for the crafting system. Separates
 * type definitions from implementation for clean include dependencies.
 *
 * CRAFTING WORKFLOW:
 * 1. Player opens crafting menu (station or hand crafting)
 * 2. Recipes filtered by: knowledge, skill level, station type
 * 3. Player queues craft -> FMOCraftingQueueEntry created
 * 4. Queue owns reserved materials; timed work updates progress
 * 5. Completion consumes one repeat, installs owned outputs, then grants XP/notifies
 *
 * KEY TYPES:
 * - EMODiscoveryMethod: How a recipe was learned
 * - FMOCraftingQueueEntry: Single item in crafting queue
 * - FMOCraftingQueueList: FastArray replicated queue
 * - FMOCraftingQueueSaveData: Persistence for queue
 * - FMORecipeDiscoverySaveData: Persistence for discoveries
 *
 * =============================================================================
 * BEST PRACTICES
 * =============================================================================
 *
 * 1. Use EntryId (GUID) to reference queue entries, not array indices.
 *    Indices change during replication; GUIDs are stable.
 *
 * 2. bHasCapturedReservation distinguishes owned lots from old preconsumed saves.
 *    Never recreate refunds from a live recipe or consume inventory again on resume.
 *
 * 3. FastArraySerializer pattern is used for efficient replication.
 *    See MOInventoryComponent.h for detailed FastArray documentation.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-01] ENTRY ID GENERATION: FMOCraftingQueueEntry generates its own
 *   GUID in the default constructor. Don't overwrite it unless loading.
 *
 * [2024-02] STATION VALIDATION: Station enum is stored in queue entry to
 *   validate on resume. If player moves away from station, craft may fail.
 *
 * [2026-10] PROGRESS TIMING: Progress is 0.0-1.0 normalized. The queue uses
 *   a UTC anchor for elapsed real-time work and persists its current progress.
 *
 * [2024-02] SAVE/LOAD TIMING: PausedAt timestamp in save data is used to
 *   calculate offline crafting progress. Uses FDateTime, not game time.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOCraftingQueueComponent.h - Uses FMOCraftingQueueEntry/List
 * - MORecipeDiscoveryComponent.h - Uses FMORecipeDiscoverySaveData
 * - MOCraftingSubsystem.h - Defines FMOCraftingValidation, FMOCraftResult
 * - MORecipeDefinitionRow.h - Defines FMOToolRequirement (included here)
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MORecipeDefinitionRow.h"
#include "MOIngredientReservation.h"

#include "MOCraftingTypes.generated.h"

/**
 * Method by which a recipe was discovered.
 */
UENUM(BlueprintType)
enum class EMODiscoveryMethod : uint8
{
	Manual UMETA(DisplayName="Manual"),                 // Explicitly granted (e.g., starting recipes)
	Inspection UMETA(DisplayName="Inspection"),         // Learned by inspecting items
	SkillLevel UMETA(DisplayName="Skill Level"),        // Auto-unlocked at skill level
	Schematic UMETA(DisplayName="Schematic"),           // Consumed schematic item
	Experimentation UMETA(DisplayName="Experimentation") // Combined ingredients blindly
};

// Note: FMOToolRequirement is defined in MORecipeDefinitionRow.h (included above)

/**
 * A single entry in the crafting queue.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCraftingQueueEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Unique ID for this queue entry. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting", meta=(IgnoreForMemberInitializationTest))
	FGuid EntryId;

	/** Native assignment requested a durable result, acknowledged after its cargo is delivered. */
	UPROPERTY()
	bool bRetainCompletionReceipt = false;

	/** Recipe being crafted. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	FName RecipeId = NAME_None;

	/** How many times to repeat this craft. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	int32 Count = 1;

	/** Number of crafts completed so far. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	int32 CompletedCount = 0;

	/** Current progress (0.0 - 1.0) on the active craft. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	float Progress = 0.0f;

	/** Station this was queued at (for validation on resume). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	EMOCraftingStation Station = EMOCraftingStation::None;

	/** Physical station bound at enqueue. Menu changes never retarget work already queued. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	FGuid StationGuid;

	/** Materials have left inventory. New entries retain them in ReservedIngredients; old saves preconsumed them. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	bool bIngredientsConsumed = false;

	/** New entries own exact material lots; false identifies pre-ledger saves with unavailable provenance. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	bool bHasCapturedReservation = false;

	/** Queued in creative mode (FMOWorldRules::IsCreative): it took no materials, so a cancel returns none and owes none. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	bool bFree = false;

	/** Captured at enqueue, combined across duplicate ingredient rows. Never reread a changed recipe to refund. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	TMap<FName, int32> IngredientsPerCraft;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	FMOIngredientReservation ReservedIngredients;

	/** Timestamp when this craft was started. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	float StartTime = 0.0f;

	FMOCraftingQueueEntry()
	{
		EntryId = FGuid::NewGuid();
	}
};

/**
 * FastArray container for crafting queue entries (for replication).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCraftingQueueList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOCraftingQueueEntry> Entries;

	UPROPERTY(Transient)
	TObjectPtr<class UMOCraftingQueueComponent> OwnerComponent;

	void SetOwner(UMOCraftingQueueComponent* InOwner) { OwnerComponent = InOwner; }
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FMOCraftingQueueEntry, FMOCraftingQueueList>(Entries, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FMOCraftingQueueList> : public TStructOpsTypeTraitsBase2<FMOCraftingQueueList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

/** Result ownership survives offline completion before an assignment executor binds. No material is copied here. */
USTRUCT()
struct MOFRAMEWORK_API FMOCraftingCompletionReceipt
{
	GENERATED_BODY()
	UPROPERTY() FGuid EntryId;
	UPROPERTY() TMap<FName, int32> ProducedItems;
};

/** Save data for a pawn's crafting queue. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCraftingQueueSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	TArray<FMOCraftingQueueEntry> QueuedCrafts;

	UPROPERTY()
	TArray<FMOCraftingCompletionReceipt> CompletionReceipts;

	/** Timestamp when crafting was paused (for calculating elapsed time on load). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	FDateTime PausedAt;

	/** Whether crafting was in progress when saved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	bool bWasActive = false;

	/** Work is suspended until the bound station becomes usable again. No offline credit while suspended. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	bool bAwaitingContext = false;

	/** Station GUID that was being used (to resume at correct station). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	FGuid ActiveStationGuid;

	/** Materials continue to exist when a full inventory or a missing vessel prevents delivery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	FMOIngredientReservation PendingReturns;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	FMOIngredientReservation PendingProducts;

	/** Cancelled old-save entries whose material identity/liquid provenance was never recorded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	TArray<FMOCraftingQueueEntry> UnresolvedLegacyRefunds;
};

/**
 * Save data for a pawn's discovered recipes.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMORecipeDiscoverySaveData
{
	GENERATED_BODY()

	/** Recipe IDs that have been discovered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	TArray<FName> DiscoveredRecipes;

	/** Experimentation history (ingredient combo hashes tried). Stored as int32 for Blueprint compatibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	TArray<int32> ExperimentationHistory;
};

// Note: FMOCraftingValidation and FMOCraftResult are defined in MOCraftingSubsystem.h
// Additional fields (MissingTools, bRecipeDiscovered, bHasFuel) will be added there in Phase 2

/**
 * Delegate signatures for crafting events.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnCraftProgressSignature, const FGuid&, EntryId, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnCraftCompletedSignature, const FGuid&, EntryId, const FMOCraftResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnCraftCancelledSignature, const FGuid&, EntryId, bool, bWasRefunded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnCraftQueueChangedSignature);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnRecipeDiscoveredSignature, FName, RecipeId, EMODiscoveryMethod, Method);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnExperimentFailedSignature, const TArray<FName>&, Ingredients);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnStationActivatedSignature, AController*, User);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnStationDeactivatedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnFuelChangedSignature, float, NewPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnFuelDepletedSignature);
