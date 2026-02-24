#pragma once

#include "CoreMinimal.h"
#include "MOSpawnTypes.generated.h"

class APawn;

/**
 * Category of spawnable entity.
 */
UENUM(BlueprintType)
enum class EMOSpawnCategory : uint8
{
	Survivor	UMETA(DisplayName = "Survivor"),
	Prey		UMETA(DisplayName = "Prey"),
	Predator	UMETA(DisplayName = "Predator"),
	Ambient		UMETA(DisplayName = "Ambient"),
	Custom		UMETA(DisplayName = "Custom")
};

/**
 * State of a survivor's recruitment process.
 */
UENUM(BlueprintType)
enum class EMORecruitmentState : uint8
{
	Wandering		UMETA(DisplayName = "Wandering"),		// Not interacted, can despawn normally
	Interacted		UMETA(DisplayName = "Interacted"),		// Talked to, persistence timer started
	QuestActive		UMETA(DisplayName = "Quest Active"),	// Has active quest requirement
	Recruited		UMETA(DisplayName = "Recruited"),		// Fully recruited, possessable
	Dead			UMETA(DisplayName = "Dead")				// Failed quest, convert to corpse
};

/**
 * Quest type for survivor recruitment.
 */
UENUM(BlueprintType)
enum class EMORecruitmentQuestType : uint8
{
	None			UMETA(DisplayName = "None (Simple Recruit)"),
	NeedsFood		UMETA(DisplayName = "Needs Food"),
	NeedsMedical	UMETA(DisplayName = "Needs Medical Aid"),
	NeedsWarmth		UMETA(DisplayName = "Needs Warmth"),
	NeedsProtection	UMETA(DisplayName = "Needs Protection"),
	Custom			UMETA(DisplayName = "Custom")
};

/**
 * Configuration for a recruitment quest.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMORecruitmentQuest
{
	GENERATED_BODY()

	/** Type of quest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	EMORecruitmentQuestType QuestType = EMORecruitmentQuestType::None;

	/** Item required to complete the quest (if applicable) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName RequiredItemId = NAME_None;

	/** Quantity of item required */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 RequiredItemQuantity = 1;

	/** Skill required to complete (e.g., "Medical" for NeedsMedical) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName RequiredSkillId = NAME_None;

	/** Minimum skill level required */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 RequiredSkillLevel = 0;

	/** Time until quest fails (seconds). Default: 10 hours */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	float TimeoutSeconds = 36000.0f;

	/** Dialogue shown when first interacting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText QuestDialogue;

	/** Dialogue shown on quest completion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText SuccessDialogue;

	/** Dialogue shown on quest failure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText FailureDialogue;
};

/**
 * Configuration for a spawn category.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSpawnCategoryConfig
{
	GENERATED_BODY()

	/** Category this config applies to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	EMOSpawnCategory Category = EMOSpawnCategory::Prey;

	/** Minimum time between spawns (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	float MinCooldownSeconds = 1800.0f;  // 30 min

	/** Maximum time between spawns (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	float MaxCooldownSeconds = 3600.0f;  // 1 hour

	/** Maximum number of this category that can exist at once */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1"))
	int32 MaxSpawnedCount = 5;

	/** Minimum distance from player to spawn (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	float MinSpawnDistance = 50.0f;

	/** Maximum distance from player to spawn (meters) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	float MaxSpawnDistance = 400.0f;

	/** Minimum distance from any structure with BuildableComponent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	float MinDistanceFromStructure = 200.0f;

	/** Minimum group size when spawning */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1"))
	int32 MinGroupSize = 1;

	/** Maximum group size when spawning */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1"))
	int32 MaxGroupSize = 1;

	/** Classes that can be spawned for this category */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TArray<TSubclassOf<APawn>> SpawnableClasses;

	/** If true, first spawn uses faster cooldown */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bFirstSpawnFaster = false;

	/** Maximum cooldown for first spawn (only used if bFirstSpawnFaster) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0", EditCondition = "bFirstSpawnFaster"))
	float FirstSpawnMaxCooldown = 1800.0f;  // 30 min

	/** Whether this category is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bEnabled = true;
};

/**
 * Record of a spawned entity for tracking and FIFO management.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSpawnedEntityRecord
{
	GENERATED_BODY()

	/** The spawned pawn (weak reference to allow GC) */
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> SpawnedPawn;

	/** Category this entity belongs to */
	UPROPERTY()
	EMOSpawnCategory Category = EMOSpawnCategory::Prey;

	/** When this entity was spawned */
	UPROPERTY()
	FDateTime SpawnTime;

	/** If true, this entity won't be despawned by FIFO until persistence expires */
	UPROPERTY()
	bool bPersistent = false;

	/** When persistence expires (for interacted survivors) */
	UPROPERTY()
	FDateTime PersistenceExpiry;

	FMOSpawnedEntityRecord()
		: SpawnTime(FDateTime::Now())
		, PersistenceExpiry(FDateTime::MinValue())
	{
	}

	FMOSpawnedEntityRecord(APawn* InPawn, EMOSpawnCategory InCategory)
		: SpawnedPawn(InPawn)
		, Category(InCategory)
		, SpawnTime(FDateTime::Now())
		, bPersistent(false)
		, PersistenceExpiry(FDateTime::MinValue())
	{
	}

	bool IsValid() const { return SpawnedPawn.IsValid(); }
	bool IsPersistenceExpired() const { return bPersistent && FDateTime::Now() > PersistenceExpiry; }
};
