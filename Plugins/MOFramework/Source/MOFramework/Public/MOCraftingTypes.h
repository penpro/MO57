#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MORecipeDefinitionRow.h"

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

	/** Whether ingredients have been consumed for current craft. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting")
	bool bIngredientsConsumed = false;

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

/**
 * Save data for a pawn's crafting queue.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCraftingQueueSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	TArray<FMOCraftingQueueEntry> QueuedCrafts;

	/** Timestamp when crafting was paused (for calculating elapsed time on load). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	FDateTime PausedAt;

	/** Whether crafting was in progress when saved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	bool bWasActive = false;

	/** Station GUID that was being used (to resume at correct station). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
	FGuid ActiveStationGuid;
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
