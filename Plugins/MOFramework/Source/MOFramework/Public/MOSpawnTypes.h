/**
 * =============================================================================
 * MOSpawnTypes.h - Dynamic Entity Spawning Types
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Defines types for the dynamic spawn system that spawns survivors, prey,
 * and predators around the player. Handles spawn cooldowns, FIFO despawning,
 * and recruitment persistence.
 *
 * SPAWN CATEGORIES:
 * - Survivor: Recruitable NPCs with quests
 * - Prey: Huntable animals (deer, rabbits)
 * - Predator: Dangerous animals (wolves, bears)
 * - Ambient: Background wildlife (birds, squirrels)
 * - Custom: User-defined categories
 *
 * RECRUITMENT WORKFLOW:
 * 1. Survivor spawns in Wandering state (can despawn)
 * 2. Player interacts → Interacted state (persistence timer starts)
 * 3. Quest assigned → QuestActive state
 * 4. Quest completed → Recruited state (possessable)
 * 5. Quest failed → Dead state (becomes corpse)
 *
 * FIFO DESPAWNING:
 * When MaxSpawnedCount reached, oldest non-persistent entity despawns.
 * Persistent entities (interacted survivors) are protected until expiry.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] PERSISTENCE EXPIRY: Interacted survivors have PersistenceExpiry
 *   timestamp. After expiry, they can despawn again. Default is 1 hour.
 *
 * [2024-02] WEAK REFERENCES: FMOSpawnedEntityRecord uses TWeakObjectPtr.
 *   Always check IsValid() before accessing SpawnedPawn.
 *
 * [2024-02] STRUCTURE AVOIDANCE: MinDistanceFromStructure uses any actor
 *   with UMOBuildProgressComponent. Player buildings deter spawns.
 *
 * [2024-02] GROUP SPAWNING: Min/MaxGroupSize spawn multiple pawns at once.
 *   They share a spawn point with slight offset. Used for deer herds.
 *
 * [2024-02] RECRUITMENT SAVE: EMORecruitmentState is saved per-pawn. On
 *   load, recruited survivors restore their state. See MOPersistenceSubsystem.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOSpawnManagerSubsystem.h - Uses these types for spawn management
 * - MOSpawnSettings.h - Project settings for spawn configuration
 * - MORecruitmentComponent.h - Component storing recruitment state
 * - MOPersistenceSubsystem.h - Saves/loads recruitment state
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOSpawnTypes.generated.h"

class APawn;

/**
 * Category of spawnable entity.
 * See file header for spawn system overview.
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

	// ========================================================================
	// Z-RANGE GATE (terrain band restriction)
	// ========================================================================
	// Useful for "only spawn where terrain looks right" — e.g. survivors only
	// on shoreline (low Z, no trees) rather than in the canopy. The voxel
	// ground trace tells us the resolved Z; this check rejects spawns whose
	// ground Z falls outside the band. Tests Hit.ImpactPoint.Z, not the
	// post-offset spawn location.

	/** If true, gate spawns by world Z range (see Min/MaxSpawnZ). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Terrain Band")
	bool bRestrictSpawnZ = false;

	/** Minimum ground Z for spawn (world units, cm). Inclusive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Terrain Band",
		meta = (EditCondition = "bRestrictSpawnZ"))
	float MinSpawnZ = 0.0f;

	/** Maximum ground Z for spawn (world units, cm). Inclusive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Terrain Band",
		meta = (EditCondition = "bRestrictSpawnZ"))
	float MaxSpawnZ = 300.0f;

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

	/**
	 * Optional discovery beacon spawned alongside the pawn (e.g. lit campfire
	 * next to a survivor so the player can see them from a distance). Owned
	 * by the spawn manager — destroyed when this record is removed.
	 * Null for entities that don't have a beacon.
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> BeaconActor;

	/**
	 * True while the spawn manager has this pawn throttled via
	 * FreezeSpawnedPawn. The wake check gates on THIS, never on
	 * Brain->IsRunning() — a stopped brain also describes a corpse, and
	 * waking those restarts behavior trees on ragdolls.
	 */
	UPROPERTY(Transient)
	bool bFrozenByManager = false;

	/** Pre-freeze anim tick policy (EVisibilityBasedAnimTickOption as uint8 —
	 *  kept raw to avoid the SkeletalMeshComponent include in a types header).
	 *  Restored on wake so a BP-authored cheaper policy isn't promoted to the
	 *  engine's always-tick default. 0 = AlwaysTickPoseAndRefreshBones. */
	uint8 SavedAnimTickOption = 0;

	FMOSpawnedEntityRecord()
		: SpawnTime(FDateTime::UtcNow())
		, PersistenceExpiry(FDateTime::MinValue())
	{
	}

	FMOSpawnedEntityRecord(APawn* InPawn, EMOSpawnCategory InCategory)
		: SpawnedPawn(InPawn)
		, Category(InCategory)
		, SpawnTime(FDateTime::UtcNow())
		, bPersistent(false)
		, PersistenceExpiry(FDateTime::MinValue())
	{
	}

	bool IsValid() const { return SpawnedPawn.IsValid(); }
	// UtcNow everywhere in spawn timing: local Now() jumps with DST/clock
	// changes, instantly expiring cooldowns and persistence windows.
	bool IsPersistenceExpired() const { return bPersistent && FDateTime::UtcNow() > PersistenceExpiry; }
};
