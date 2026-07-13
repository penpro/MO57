/**
 * =============================================================================
 * MOWorldSaveGame.h - World Save/Load Data Structure
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Main save game class containing all world state data. Stores pawn records,
 * inventories, world items, buildings, voxel terrain modifications, quest
 * progress, weather state, and metadata for save slot display.
 *
 * DATA SECTIONS:
 * - Metadata: DisplayName, SaveTimestamp, PlayTime, Screenshot, WorldSeed
 * - Pawns: PersistedPawns with full component state
 * - Inventories: PawnInventoriesByGuid
 * - World Items: WorldItems dropped in world
 * - Buildings: Buildings array with progress/inventory
 * - Voxel: VoxelSculptData for terrain modifications
 * - Quests: QuestData (active/completed)
 * - Weather: WeatherData for UDW/UDS state
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] GUID CONSISTENCY: All GUIDs must come from UMOIdentityComponent.
 *   Never generate new GUIDs for existing entities during save.
 *
 * [2024-02] COMPONENT DATA FLAG: bHasComponentData in FMOPersistedPawnRecord
 *   indicates if component save data is populated (for legacy save compat).
 *
 * [2024-02] WORLD SEED: WorldSeed must match voxel generation seed.
 *   Stored in metadata for restoration on load.
 *
 * =============================================================================
 * RELATED FILES: MOPersistenceSubsystem.h, MOIdentityComponent.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UObject/SoftObjectPath.h"
#include "MOCraftingTypes.h"
#include "MOColonyTypes.h"
#include "MOBuildingTypes.h"
#include "MOCharacterAppearance.h"
#include "MOVitalsComponent.h"
#include "MOAnatomyComponent.h"
#include "MOMetabolismComponent.h"
#include "MOMentalStateComponent.h"
#include "MOSkillsComponent.h"
#include "MOEquipmentComponent.h"
#include "MORecruitmentComponent.h"
#include "MOQuestTypes.h"
#include "MOWeatherTypes.h"
#include "MOTerrainModificationSubsystem.h"
#include "MOSurvivorJobTypes.h"      // (H39) FMOSurvivorJobQueueSaveData
#include "MOCombatComponent.h"       // (H38) FMOCombatSaveData
#include "MOGameClockSubsystem.h"    // (H34) FMOGameClockSaveData
#include "MOResourceDepletionSubsystem.h" // (H37) FMOResourceNodeDepletionSaveEntry
#include "MODesignationTypes.h"           // (unit 3) FMODesignationSaveData
#include "MOworldSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FMOInventoryItemSaveEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FGuid ItemGuid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FName ItemDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    int32 Quantity = 0;

    /** Current durability for tools (-1 = infinite/not applicable). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    int32 CurrentDurability = -1;
};

USTRUCT(BlueprintType)
struct FMOInventorySaveData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    int32 SlotCount = 0;

    // Size should be SlotCount. Invalid GUID means empty slot.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FGuid> SlotItemGuids;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FMOInventoryItemSaveEntry> Items;
};

USTRUCT(BlueprintType)
struct FMOPersistedPawnRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FGuid PawnGuid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FTransform Transform;

    // Saved pawn class (soft) so we can respawn the same pawn type.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FSoftClassPath PawnClassPath;

    // Whether this pawn can be possessed by the player (false for creatures, NPCs, etc.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    bool bIsPlayerControllable = true;

    // Character identity
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FString CharacterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FString Gender;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    int32 AgeInDays = 0;

    // Status
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    bool bIsDeceased = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    float HealthPercent = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FString StatusText;

    // Tracking
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FDateTime LastPlayedTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FString LocationName;

    // Portrait (asset path for now)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FSoftObjectPath PortraitPath;

    // Character appearance (MetaHuman customization data)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FMOCharacterAppearance Appearance;

    // ============================================================================
    // COMPONENT STATE
    // ============================================================================

    /** Vitals component state (HR, BP, SpO2, blood volume, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Components")
    FMOVitalsSaveData VitalsData;

    /** Anatomy component state (body parts, wounds, conditions) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Components")
    FMOAnatomySaveData AnatomyData;

    /** Metabolism component state (nutrition, digestion, body composition) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Components")
    FMOMetabolismSaveData MetabolismData;

    /** Mental state component (consciousness, shock, effects) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Components")
    FMOMentalStateSaveData MentalStateData;

    /** Skills component state (skill levels, XP) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Components")
    FMOSkillsSaveData SkillsData;

    /** Equipment component state (equipped items per slot) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Components")
    FMOEquipmentSaveData EquipmentData;

    /** Recruitment component state (for survivors) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Components")
    FMORecruitmentSaveData RecruitmentData;

    /**
     * (H39) Survivor job queue state (queued jobs, home binding). Default-
     * constructs (bHasValidData=false) for old saves, so ApplySaveDataAuthority
     * no-ops on legacy records rather than wiping a live queue.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Components")
    FMOSurvivorJobQueueSaveData JobQueueData;

    /**
     * (H38) Combat component state — weapon wear (durability) for main/off hand
     * and combat decay timer. Persisted so weapon durability survives reload.
     * Default-constructs for old saves.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Components")
    FMOCombatSaveData CombatData;

    /** Flag to indicate if component data has been populated (for legacy save compatibility) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Components")
    bool bHasComponentData = false;
};

USTRUCT(BlueprintType)
struct FMOPersistedWorldItemRecord
{
    GENERATED_BODY()

    // This MUST be the Identity GUID. Do not store GUIDs on UMOItemComponent.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FGuid ItemGuid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FTransform Transform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FSoftClassPath ItemClassPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FName ItemDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    int32 Quantity = 1;
};

/**
 * Save data for a voxel sculpt actor (terrain modifications).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOVoxelSculptSaveRecord
{
    GENERATED_BODY()

    /** Name of the sculpt actor in the level. Used to match on load. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FString ActorName;

    /** Whether this is volume (true) or height (false) sculpting. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    bool bIsVolumeSculpt = false;

    /** Serialized sculpt data (compressed binary). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<uint8> SculptData;

    /** Whether the data was successfully captured. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    bool bHasValidData = false;
};

/**
 * Save data for a persisted building in the world.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOPersistedBuildingRecord
{
    GENERATED_BODY()

    /** Unique identifier for this building. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FGuid BuildingGuid;

    /** World transform of the building. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FTransform Transform;

    /** Class path of the buildable actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FSoftClassPath ActorClassPath;

    /** Recipe ID used to create this building. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FName RecipeId;

    /** Current build progress state. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    FMOBuildProgress Progress;

    /** Inventory data for containers/crafting stations. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FMOInventoryItemSaveEntry> InventoryItems;

    /** Slot count for inventory (to restore proper slot layout). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    int32 InventorySlotCount = 0;

    /** Slot GUIDs for inventory order. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FGuid> InventorySlotGuids;

    /** Current fuel level (for crafting stations). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    float CurrentFuel = 0.0f;

    /** Whether the station is currently active (for crafting stations). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    bool bIsActive = false;
};

UCLASS()
class MOFRAMEWORK_API UMOWorldSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    // ============================================================================
    // METADATA (for save slot display)
    // ============================================================================

    /** Display name for this save (if empty, uses slot name). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Metadata")
    FString DisplayName;

    /** Timestamp when this save was created. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Metadata")
    FDateTime SaveTimestamp;

    /** Total playtime at the time of save. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Metadata")
    float TotalPlayTimeSeconds = 0.0f;

    /** World/level name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Metadata")
    FString WorldName;

    /** Whether this is an autosave. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Metadata")
    bool bIsAutosave = false;

    /** Screenshot thumbnail data (PNG compressed, 80x80). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Metadata")
    TArray<uint8> ScreenshotData;

    /** GUID of the last possessed pawn (for camera positioning on load). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Metadata")
    FGuid LastPossessedPawnGuid;

    /** World seed used for procedural generation (voxel terrain, etc.). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Metadata")
    int32 WorldSeed = 0;

    // ============================================================================
    // WORLD DATA
    // ============================================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FGuid> DestroyedGuids;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FMOPersistedPawnRecord> PersistedPawns;

    // PawnGuid -> InventorySaveData
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TMap<FGuid, FMOInventorySaveData> PawnInventoriesByGuid;

    // Runtime spawned item actors that still exist in the world at save time.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save")
    TArray<FMOPersistedWorldItemRecord> WorldItems;

    // --- Crafting System Save Data ---

    /** Crafting queue state per pawn (PawnGuid -> QueueData). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Crafting")
    TMap<FGuid, FMOCraftingQueueSaveData> PawnCraftingQueuesByGuid;

    /** Discovered recipes per pawn (PawnGuid -> DiscoveryData). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Crafting")
    TMap<FGuid, FMORecipeDiscoverySaveData> PawnDiscoveredRecipesByGuid;

    // --- Building System Save Data ---

    /** All placed buildings in the world (including ghosts and under-construction). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Building")
    TArray<FMOPersistedBuildingRecord> Buildings;

    // --- Voxel Sculpt Save Data ---

    /** All voxel sculpt actor states (terrain modifications). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Voxel")
    TArray<FMOVoxelSculptSaveRecord> VoxelSculptData;

    // --- Quest System Save Data ---

    /** Quest progress (active quests, completed quest IDs). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Quest")
    FMOQuestSaveData QuestData;

    // --- Weather/Time Save Data ---

    /** Weather and time of day state (UDW/UDS integration). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Weather")
    FMOWeatherSaveData WeatherData;

    // --- Terrain Modification Save Data ---

    /**
     * Tracked "worked ground" spheres. Distinct from VoxelSculptData (which
     * is the voxel mesh itself, owned by the Voxel plugin's save system) —
     * this is the metadata that says "PCG / foliage shouldn't grow back on
     * this patch even though the terrain mesh has been restored."
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|TerrainMod")
    FMOTerrainModificationSaveData TerrainModificationData;

    // --- Game Clock Save Data (H34) ---

    /**
     * (H34) Game clock state: in-game date/time, TimeScale, and cumulative
     * real/game-time accumulators. Default-constructs (bIsValid=false) for old
     * saves, so restore falls back to the clock's fresh-start defaults.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Clock")
    FMOGameClockSaveData GameClockData;

    /** Settlement layer (V1): settlement record, residency, mood, histories. */
    UPROPERTY()
    FMOColonySaveData ColonyData;

    // --- Resource Depletion Save Data (H37) ---

    /**
     * (H37) Per-resource-node yield depletion + respawn timers. Default-
     * constructs (bHasValidData=false) for old saves, so a legacy load leaves
     * the fresh runtime depletion map untouched.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Resources")
    FMOResourceDepletionSaveData ResourceDepletionData;

    // --- Terraform excavation designations (unit 3) ---

    /**
     * Player-placed dig / dump / flatten work areas. Owned + serialized by
     * UMODesignationSubsystem via IMOSaveDomain. Default-constructs empty for
     * old saves.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Designation")
    FMODesignationSaveData DesignationData;
};
