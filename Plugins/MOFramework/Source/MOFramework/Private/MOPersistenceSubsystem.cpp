#include "MOPersistenceSubsystem.h"
#include "MOFramework.h"
#include "MOHarvestDebugSubsystem.h"

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "EngineUtils.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "MOPlayerController.h"
#include "MOSpectatorPawn.h"
#include "MOIdentityComponent.h"
#include "MOIdentityRegistrySubsystem.h"
#include "MOInventoryComponent.h"
#include "MOItemComponent.h"
#include "MOPersistenceSettings.h"
#include "MOGameSettings.h"
#include "MOCraftingQueueComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOVitalsComponent.h"
#include "MOAnatomyComponent.h"
#include "MOMetabolismComponent.h"
#include "MOMentalStateComponent.h"
#include "MOSkillsComponent.h"
#include "MOEquipmentComponent.h"
#include "MORecruitmentComponent.h"
#include "MOCombatComponent.h"               // (H30/H38) combat weapon-wear restore
#include "MOSurvivorJobQueueComponent.h"     // (H39) survivor job-queue restore
#include "MOCreature.h"
#include "MOBuildableActor.h"
#include "MOBuildProgressComponent.h"
#include "MOIdentifiableInterface.h"
#include "MOInventoryHolderInterface.h"
#include "MOCreature.h"
#include "MOQuestSubsystem.h"
#include "MOWeatherIntegrationSubsystem.h"
#include "MOTerrainModificationSubsystem.h"
#include "MOGameClockSubsystem.h"            // (H34) game clock persistence
#include "MOResourceDepletionSubsystem.h"    // (H37) resource depletion persistence
#include "MOSpawnManagerSubsystem.h"         // (H35) adopt restored creatures into spawn tracking

// Voxel plugin sculpt persistence
#include "VoxelMinimal/Utilities/VoxelThreadingUtilities.h"
#include "MOVoxelAlias.h"  // all Voxel sculpt save/load goes through this facade
#include "Sculpt/VoxelSculptSave.h"

// For screenshot capture
#include "Engine/GameViewportClient.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

static FString StripUEDPIEPrefixes(const FString& InPath)
{
    FString Out = InPath;

    int32 SearchIndex = 0;
    while ((SearchIndex = Out.Find(TEXT("UEDPIE_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchIndex)) != INDEX_NONE)
    {
        const int32 PrefixStart = SearchIndex;         // points at 'U'
        int32 Cursor = PrefixStart + 7;                // after "UEDPIE_"

        while (Cursor < Out.Len() && FChar::IsDigit(Out[Cursor]))
        {
            ++Cursor;
        }

        if (Cursor < Out.Len() && Out[Cursor] == TEXT('_'))
        {
            // Remove "UEDPIE_<digits>_"
            const int32 RemoveCount = (Cursor + 1) - PrefixStart;
            Out.RemoveAt(PrefixStart, RemoveCount, EAllowShrinking::No);

        }
        else
        {
            SearchIndex = Cursor;
        }
    }

    return Out;
}

static UClass* TryLoadPawnClassFromSoftPath(const FSoftClassPath& InPath)
{
    if (!InPath.IsValid())
    {
        return nullptr;
    }

    // First try direct
    if (UClass* Loaded = InPath.TryLoadClass<APawn>())
    {
        return Loaded;
    }

    // Then try sanitized PIE-stripped path
    const FString Raw = InPath.ToString();
    const FString Sanitized = StripUEDPIEPrefixes(Raw);
    if (Sanitized != Raw)
    {
        return FSoftClassPath(Sanitized).TryLoadClass<APawn>();
    }

    return nullptr;
}


static bool AssignGuidToIdentityComponent(UMOIdentityComponent* IdentityComponent, const FGuid& DesiredGuid)
{
    if (!IsValid(IdentityComponent) || !DesiredGuid.IsValid())
    {
        return false;
    }

    IdentityComponent->SetGuid(DesiredGuid);
    return IdentityComponent->HasValidGuid() && IdentityComponent->GetGuid() == DesiredGuid;
}

void UMOPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    PostWorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
        this,
        &UMOPersistenceSubsystem::HandlePostWorldInitialization
    );
}

void UMOPersistenceSubsystem::Deinitialize()
{
    UnbindFromWorld();

    if (PostWorldInitHandle.IsValid())
    {
        FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitHandle);
        PostWorldInitHandle.Reset();
    }

    Super::Deinitialize();
}

void UMOPersistenceSubsystem::ResetForNewWorld(const FString& NewSlotName)
{
    LoadedWorldSave = nullptr;
    SessionDestroyedGuids.Empty();
    SessionPlayTimeSeconds = 0.0f;
    bNextSaveIsAutosave = false;
    // Clear the load-suppression latch too: it's normally cleared by a 0.25s world-lifetime
    // timer, but if the world was torn down inside that window (quit-to-menu right after a
    // load) the timer died with it and the flag stuck true, silently stopping ALL destroyed-
    // GUID recording for the rest of the GameInstance -> destroyed items/pawns resurrect on
    // later save/loads. Every new-world/load path funnels through here, so reset it (audit).
    bSuppressDestroyedGuidRecording = false;
    CurrentSlotName = NewSlotName;

    UE_LOG(LogMOFramework, Log,
        TEXT("[MOPersistence] ResetForNewWorld: session state cleared, slot is now '%s'"),
        *NewSlotName);
}

void UMOPersistenceSubsystem::HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues /*IVS*/)
{
    BindToWorld(World);
}

void UMOPersistenceSubsystem::BindToWorld(UWorld* World)
{
    if (!World || !World->IsGameWorld())
    {
        return;
    }

    // Server / listen-server only.
    if (World->GetNetMode() == NM_Client)
    {
        return;
    }

    if (BoundWorld.Get() == World)
    {
        return;
    }

    UnbindFromWorld();
    BoundWorld = World;

    UMOIdentityRegistrySubsystem* RegistrySubsystem = World->GetSubsystem<UMOIdentityRegistrySubsystem>();
    if (!RegistrySubsystem)
    {
        return;
    }

    BoundRegistry = RegistrySubsystem;
    RegistrySubsystem->OnIdentityRegistered.AddDynamic(this, &UMOPersistenceSubsystem::HandleIdentityRegistered);

    ApplyDestroyedGuidsToWorld(World);
}

void UMOPersistenceSubsystem::UnbindFromWorld()
{
    if (UMOIdentityRegistrySubsystem* RegistrySubsystem = BoundRegistry.Get())
    {
        RegistrySubsystem->OnIdentityRegistered.RemoveDynamic(this, &UMOPersistenceSubsystem::HandleIdentityRegistered);
    }

    BoundRegistry.Reset();
    BoundWorld.Reset();
}

bool UMOPersistenceSubsystem::SaveWorldToSlot(const FString& SlotName)
{
    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] *** SaveWorldToSlot CALLED: %s ***"), *SlotName);

    UWorld* World = BoundWorld.Get();
    if (!World)
    {
        World = GetWorld();
        UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Using GetWorld() fallback"));
    }

    if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
    {
        UE_LOG(LogMOFramework, Error, TEXT("[MOPersist] Save FAILED - no valid authority game world. World=%s NetMode=%d"),
            *GetNameSafe(World),
            World ? (int32)World->GetNetMode() : -1);
        return false;
    }

    UMOWorldSaveGame* SaveObject = Cast<UMOWorldSaveGame>(UGameplayStatics::CreateSaveGameObject(UMOWorldSaveGame::StaticClass()));
    if (!SaveObject)
    {
        return false;
    }

    // ============================================================================
    // METADATA
    // ============================================================================

    // Display name - use slot name if not explicitly set
    SaveObject->DisplayName = SlotName;

    // Timestamp
    SaveObject->SaveTimestamp = FDateTime::Now();

    // Total play time - accumulate session time
    // If we have a previous save loaded, use its total time as base
    float BasePlayTime = 0.0f;
    if (LoadedWorldSave)
    {
        BasePlayTime = LoadedWorldSave->TotalPlayTimeSeconds;
    }
    SaveObject->TotalPlayTimeSeconds = BasePlayTime + SessionPlayTimeSeconds;

    // World name
    SaveObject->WorldName = GetCurrentWorldIdentifier();

    // Autosave flag
    SaveObject->bIsAutosave = bNextSaveIsAutosave;
    bNextSaveIsAutosave = false; // Reset after use

    // Last possessed pawn GUID (for camera positioning on load)
    if (UGameInstance* GI = GetGameInstance())
    {
        if (APlayerController* PC = GI->GetFirstLocalPlayerController(World))
        {
            if (AMOPlayerController* MOPC = Cast<AMOPlayerController>(PC))
            {
                SaveObject->LastPossessedPawnGuid = MOPC->GetLastPossessedPawnGuid();
            }
        }
    }

    // World seed (for voxel terrain regeneration)
    SaveObject->WorldSeed = UMOGameSettings::GetWorldSeed();
    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Saving world seed: %d"), SaveObject->WorldSeed);
    MOHARVEST_LOG(this, "Seed", "SAVE: writing WorldSeed=%d to slot='%s'", SaveObject->WorldSeed, *SlotName);

    // Screenshot capture (80x80 thumbnail)
    CaptureScreenshotForSave(SaveObject);

    // ============================================================================
    // WORLD DATA
    // ============================================================================

    SaveObject->DestroyedGuids.Reset(SessionDestroyedGuids.Num());
    for (const FGuid& Guid : SessionDestroyedGuids)
    {
        SaveObject->DestroyedGuids.Add(Guid);
    }

    CapturePersistedPawnsAndInventories(World, SaveObject);
    CaptureWorldItems(World, SaveObject);
    CaptureBuildings(World, SaveObject);
    CaptureVoxelSculptData(World, SaveObject);
    CaptureQuestData(SaveObject);
    CaptureWeatherData(World, SaveObject);
    CaptureTerrainModificationData(World, SaveObject);
    CaptureGameClockData(World, SaveObject);          // (H34) in-game date/time, TimeScale, accumulators
    CaptureResourceDepletionData(World, SaveObject);  // (H37) per-node yield depletion + respawn timers

    const bool bOk = UGameplayStatics::SaveGameToSlot(SaveObject, SlotName, 0);

    if (bOk)
    {
        CurrentSlotName = SlotName;
    }

    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Save slot=%s ok=%d destroyed=%d pawns=%d inventories=%d worldItems=%d buildings=%d netmode=%d"),
        *SlotName,
        bOk ? 1 : 0,
        SaveObject->DestroyedGuids.Num(),
        SaveObject->PersistedPawns.Num(),
        SaveObject->PawnInventoriesByGuid.Num(),
        SaveObject->WorldItems.Num(),
        SaveObject->Buildings.Num(),
        (int32)World->GetNetMode());

    return bOk;
}

bool UMOPersistenceSubsystem::SaveToCurrentSlot()
{
    if (CurrentSlotName.IsEmpty())
    {
        // Auto-generate a slot name for new games that haven't been explicitly saved yet
        CurrentSlotName = GenerateAutoSaveSlotName();
        UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] SaveToCurrentSlot - Auto-generated slot name: %s"), *CurrentSlotName);
    }
    return SaveWorldToSlot(CurrentSlotName);
}

FString UMOPersistenceSubsystem::GenerateAutoSaveSlotName() const
{
    // Format: AutoSave_<WorldName>_<Timestamp>
    FString WorldName = GetCurrentWorldIdentifier();
    if (WorldName.IsEmpty())
    {
        WorldName = TEXT("Unknown");
    }

    // Use date-time for uniqueness: YYYYMMDD_HHMMSS
    FDateTime Now = FDateTime::Now();
    FString Timestamp = Now.ToString(TEXT("%Y%m%d_%H%M%S"));

    return FString::Printf(TEXT("AutoSave_%s_%s"), *WorldName, *Timestamp);
}

void UMOPersistenceSubsystem::CaptureScreenshotForSave(UMOWorldSaveGame* SaveObject) const
{
    if (!SaveObject)
    {
        return;
    }

    // Get game viewport
    UGameViewportClient* ViewportClient = GEngine ? GEngine->GameViewport : nullptr;
    if (!ViewportClient)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Cannot capture screenshot - no viewport client"));
        return;
    }

    FViewport* Viewport = ViewportClient->Viewport;
    if (!Viewport)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Cannot capture screenshot - no viewport"));
        return;
    }

    // Read pixels from viewport
    TArray<FColor> Bitmap;
    FIntVector ViewportSize(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, 0);

    if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Cannot capture screenshot - invalid viewport size"));
        return;
    }

    bool bReadSuccess = Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX));
    if (!bReadSuccess || Bitmap.Num() == 0)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Failed to read pixels from viewport"));
        return;
    }

    // Target thumbnail size
    constexpr int32 ThumbnailSize = 80;

    // Resize to 80x80 (simple bilinear-ish downsampling)
    TArray<FColor> ResizedBitmap;
    ResizedBitmap.SetNumUninitialized(ThumbnailSize * ThumbnailSize);

    const float ScaleX = static_cast<float>(ViewportSize.X) / ThumbnailSize;
    const float ScaleY = static_cast<float>(ViewportSize.Y) / ThumbnailSize;

    for (int32 Y = 0; Y < ThumbnailSize; ++Y)
    {
        for (int32 X = 0; X < ThumbnailSize; ++X)
        {
            const int32 SrcX = FMath::Clamp(FMath::FloorToInt(X * ScaleX), 0, ViewportSize.X - 1);
            const int32 SrcY = FMath::Clamp(FMath::FloorToInt(Y * ScaleY), 0, ViewportSize.Y - 1);
            const int32 SrcIndex = SrcY * ViewportSize.X + SrcX;
            const int32 DstIndex = Y * ThumbnailSize + X;

            if (SrcIndex < Bitmap.Num())
            {
                ResizedBitmap[DstIndex] = Bitmap[SrcIndex];
            }
            else
            {
                ResizedBitmap[DstIndex] = FColor::Black;
            }
        }
    }

    // Compress to PNG using ImageWrapper module
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

    if (!ImageWrapper.IsValid())
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Failed to create image wrapper for PNG"));
        return;
    }

    // SetRaw expects raw BGRA data
    if (!ImageWrapper->SetRaw(ResizedBitmap.GetData(), ResizedBitmap.Num() * sizeof(FColor), ThumbnailSize, ThumbnailSize, ERGBFormat::BGRA, 8))
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Failed to set raw image data"));
        return;
    }

    // Get compressed PNG data
    TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(90);
    if (CompressedData.Num() == 0)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Failed to compress image to PNG"));
        return;
    }

    // Store in save object
    SaveObject->ScreenshotData.Reset(CompressedData.Num());
    SaveObject->ScreenshotData.Append(CompressedData.GetData(), CompressedData.Num());

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Captured screenshot thumbnail: %dx%d -> %d bytes PNG"),
        ViewportSize.X, ViewportSize.Y, SaveObject->ScreenshotData.Num());
}

bool UMOPersistenceSubsystem::LoadWorldFromSlot(const FString& SlotName)
{
    FMOLoadResult Result = LoadWorldFromSlotWithResult(SlotName);
    return Result.bSuccess;
}

FMOLoadResult UMOPersistenceSubsystem::LoadWorldFromSlotWithResult(const FString& SlotName)
{
    LastLoadResult = FMOLoadResult();

    UWorld* World = BoundWorld.Get();
    if (!World)
    {
        World = GetWorld();
    }

    if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
    {
        LastLoadResult.ErrorMessage = FString::Printf(TEXT("No valid authority game world. World=%s NetMode=%d"),
            *GetNameSafe(World),
            World ? (int32)World->GetNetMode() : -1);
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Save/Load ignored (%s)"), *LastLoadResult.ErrorMessage);
        return LastLoadResult;
    }

    USaveGame* LoadedBase = UGameplayStatics::LoadGameFromSlot(SlotName, 0);
    UMOWorldSaveGame* LoadedTyped = Cast<UMOWorldSaveGame>(LoadedBase);
    if (!LoadedTyped)
    {
        LastLoadResult.ErrorMessage = FString::Printf(TEXT("Failed to load save from slot '%s'"), *SlotName);
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] %s"), *LastLoadResult.ErrorMessage);
        return LastLoadResult;
    }

    LoadedWorldSave = LoadedTyped;

    // Reset session play time - we're starting a new play session
    // The loaded save's TotalPlayTimeSeconds will be used as base for next save
    SessionPlayTimeSeconds = 0.0f;

    SessionDestroyedGuids.Reset();
    for (const FGuid& Guid : LoadedTyped->DestroyedGuids)
    {
        SessionDestroyedGuids.Add(Guid);
    }

    PawnInventoryGuidsAppliedThisLoad.Reset();
    ReplacedGuidsThisLoad.Reset();

    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] LOAD: slot=%s destroyed=%d pawns=%d inventories=%d worldItems=%d buildings=%d netmode=%d"),
        *SlotName,
        LoadedTyped->DestroyedGuids.Num(),
        LoadedTyped->PersistedPawns.Num(),
        LoadedTyped->PawnInventoriesByGuid.Num(),
        LoadedTyped->WorldItems.Num(),
        LoadedTyped->Buildings.Num(),
        (int32)World->GetNetMode());

    // Debug: Dump all pawn records from save
    for (int32 i = 0; i < LoadedTyped->PersistedPawns.Num(); i++)
    {
        const FMOPersistedPawnRecord& Record = LoadedTyped->PersistedPawns[i];
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] LOAD: PawnRecord[%d] GUID=%s Class=%s Location=%s"),
            i,
            *Record.PawnGuid.ToString(EGuidFormats::DigitsWithHyphens),
            *Record.PawnClassPath.ToString(),
            *Record.Transform.GetLocation().ToString());
    }

    // Suppress destroyed GUID recording during the load pass.
    bSuppressDestroyedGuidRecording = true;
    if (World->GetTimerManager().IsTimerActive(ClearSuppressionTimerHandle))
    {
        World->GetTimerManager().ClearTimer(ClearSuppressionTimerHandle);
    }
    World->GetTimerManager().SetTimer(ClearSuppressionTimerHandle, this, &UMOPersistenceSubsystem::ClearLoadSuppression, LoadSuppressionDuration, false);

    UnpossessAllControllers(World);

    // Apply destroyed actors first (prevents placed actors from reappearing).
    ApplyDestroyedGuidsToWorld(World);

    // Bring runtime state in line with save.
    DestroyAllPersistedWorldItems(World);
    DestroyAllPersistedPawns(World);
    DestroyAllPersistedBuildings(World);

    RespawnPersistedPawns(World, LoadedTyped->PersistedPawns, LastLoadResult);
    RespawnWorldItems(World, LoadedTyped->WorldItems, LastLoadResult);
    RespawnBuildings(World, LoadedTyped->Buildings, LastLoadResult);
    RestoreVoxelSculptData(World, LoadedTyped->VoxelSculptData);
    RestoreQuestData(LoadedTyped->QuestData);
    RestoreWeatherData(World, LoadedTyped->WeatherData);
    RestoreTerrainModificationData(World, LoadedTyped->TerrainModificationData);
    RestoreGameClockData(World, LoadedTyped->GameClockData);                 // (H34)
    RestoreResourceDepletionData(World, LoadedTyped->ResourceDepletionData); // (H37)

    ApplyInventoriesToSpawnedPawns(World, LoadedTyped->PawnInventoriesByGuid);

    // Track the current slot for auto-save support
    CurrentSlotName = SlotName;

    // Extract world seed from save and store it for voxel regeneration
    LastLoadResult.WorldSeed = LoadedTyped->WorldSeed;
    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Loaded world seed: %d"), LastLoadResult.WorldSeed);
    MOHARVEST_LOG(this, "Seed", "LOAD: read WorldSeed=%d from save slot='%s'", LastLoadResult.WorldSeed, *SlotName);

    // Surface the last-possessed pawn GUID so the game mode can re-possess
    // it after voxel terrain has finished generating (otherwise the player
    // gets left as a spectator 50m above the pawn).
    LastLoadResult.LastPossessedPawnGuid = LoadedTyped->LastPossessedPawnGuid;

    // Apply loaded seed to game settings so voxel world can use it
    if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
    {
        Settings->PendingWorldSeed = LastLoadResult.WorldSeed;
        MOHARVEST_LOG(this, "Seed", "LOAD: Settings->PendingWorldSeed set to %d", LastLoadResult.WorldSeed);
    }

    // Determine overall success - we succeed even with partial failures, but log them
    LastLoadResult.bSuccess = true;

    if (LastLoadResult.PawnsFailed > 0)
    {
        LastLoadResult.ErrorMessage = FString::Printf(TEXT("Loaded with %d pawn(s) failed to spawn"), LastLoadResult.PawnsFailed);
        UE_LOG(LogMOFramework, Error, TEXT("[MOPersist] WARNING: %s. Failed GUIDs: "), *LastLoadResult.ErrorMessage);
        for (const FGuid& FailedGuid : LastLoadResult.FailedPawnGuids)
        {
            UE_LOG(LogMOFramework, Error, TEXT("[MOPersist]   - %s"), *FailedGuid.ToString(EGuidFormats::DigitsWithHyphens));
        }
    }

    if (LastLoadResult.ItemsFailed > 0)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] %d world item(s) failed to spawn"), LastLoadResult.ItemsFailed);
    }

    if (LastLoadResult.BuildingsFailed > 0)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] %d building(s) failed to spawn"), LastLoadResult.BuildingsFailed);
    }

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Load complete: Pawns=%d/%d, Items=%d/%d, Buildings=%d/%d"),
        LastLoadResult.PawnsLoaded, LastLoadResult.PawnsLoaded + LastLoadResult.PawnsFailed,
        LastLoadResult.ItemsLoaded, LastLoadResult.ItemsLoaded + LastLoadResult.ItemsFailed,
        LastLoadResult.BuildingsLoaded, LastLoadResult.BuildingsLoaded + LastLoadResult.BuildingsFailed);

    // Position camera above last possessed pawn's location (spectator view on load)
    SetupSpectatorCameraForLoad(World, LoadedTyped);

    return LastLoadResult;
}

void UMOPersistenceSubsystem::ClearLoadSuppression()
{
    bSuppressDestroyedGuidRecording = false;
    ReplacedGuidsThisLoad.Reset();
    PawnInventoryGuidsAppliedThisLoad.Reset();
}

bool UMOPersistenceSubsystem::IsGuidDestroyed(const FGuid& Guid) const
{
    return Guid.IsValid() && SessionDestroyedGuids.Contains(Guid);
}

void UMOPersistenceSubsystem::ClearDestroyedGuid(const FGuid& Guid)
{
    if (Guid.IsValid())
    {
        SessionDestroyedGuids.Remove(Guid);
    }
}

// =============================================================================
// Pawn Record Access (for Possession Menu)
// =============================================================================

TArray<FMOPersistedPawnRecord> UMOPersistenceSubsystem::GetAllPawnRecords() const
{
    if (LoadedWorldSave)
    {
        return LoadedWorldSave->PersistedPawns;
    }
    return TArray<FMOPersistedPawnRecord>();
}

bool UMOPersistenceSubsystem::GetPawnRecord(const FGuid& PawnGuid, FMOPersistedPawnRecord& OutRecord) const
{
    if (!LoadedWorldSave || !PawnGuid.IsValid())
    {
        return false;
    }

    for (const FMOPersistedPawnRecord& Record : LoadedWorldSave->PersistedPawns)
    {
        if (Record.PawnGuid == PawnGuid)
        {
            OutRecord = Record;
            return true;
        }
    }
    return false;
}

bool UMOPersistenceSubsystem::UpdatePawnRecord(const FMOPersistedPawnRecord& Record, bool bAutoSave)
{
    if (!LoadedWorldSave || !Record.PawnGuid.IsValid())
    {
        return false;
    }

    for (FMOPersistedPawnRecord& ExistingRecord : LoadedWorldSave->PersistedPawns)
    {
        if (ExistingRecord.PawnGuid == Record.PawnGuid)
        {
            ExistingRecord = Record;

            if (bAutoSave)
            {
                SaveToCurrentSlot();
            }
            return true;
        }
    }
    return false;
}

APawn* UMOPersistenceSubsystem::SpawnPawnFromRecord(const FGuid& PawnGuid)
{
    if (!PawnGuid.IsValid())
    {
        return nullptr;
    }

    FMOPersistedPawnRecord Record;
    if (!GetPawnRecord(PawnGuid, Record))
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SpawnPawnFromRecord: No record found for GUID %s"), *PawnGuid.ToString());
        return nullptr;
    }

    if (Record.bIsDeceased)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SpawnPawnFromRecord: Pawn %s is deceased"), *PawnGuid.ToString());
        return nullptr;
    }

    UClass* PawnClass = TryLoadPawnClassFromSoftPath(Record.PawnClassPath);
    if (!PawnClass)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SpawnPawnFromRecord: Failed to load class %s"), *Record.PawnClassPath.ToString());
        return nullptr;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    APawn* SpawnedPawn = World->SpawnActor<APawn>(PawnClass, Record.Transform, SpawnParams);
    if (!SpawnedPawn)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SpawnPawnFromRecord: Failed to spawn pawn"));
        return nullptr;
    }

    // Apply GUID to the spawned pawn
    UMOIdentityComponent* IdentityComp = SpawnedPawn->FindComponentByClass<UMOIdentityComponent>();
    if (IdentityComp)
    {
        IdentityComp->SetGuid(PawnGuid);
    }

    // Apply inventory if available
    if (LoadedWorldSave && LoadedWorldSave->PawnInventoriesByGuid.Contains(PawnGuid))
    {
        UMOInventoryComponent* InvComp = SpawnedPawn->FindComponentByClass<UMOInventoryComponent>();
        if (InvComp)
        {
            const FMOInventorySaveData& InvData = LoadedWorldSave->PawnInventoriesByGuid[PawnGuid];
            InvComp->ApplySaveDataAuthority(InvData);
        }
    }

    // Apply crafting queue if available
    if (LoadedWorldSave && LoadedWorldSave->PawnCraftingQueuesByGuid.Contains(PawnGuid))
    {
        UMOCraftingQueueComponent* CraftingQueue = SpawnedPawn->FindComponentByClass<UMOCraftingQueueComponent>();
        if (CraftingQueue)
        {
            const FMOCraftingQueueSaveData& QueueData = LoadedWorldSave->PawnCraftingQueuesByGuid[PawnGuid];
            CraftingQueue->ApplySaveData(QueueData, true); // true = calculate offline progress
        }
    }

    // Apply recipe discovery if available
    if (LoadedWorldSave && LoadedWorldSave->PawnDiscoveredRecipesByGuid.Contains(PawnGuid))
    {
        UMORecipeDiscoveryComponent* Discovery = SpawnedPawn->FindComponentByClass<UMORecipeDiscoveryComponent>();
        if (Discovery)
        {
            const FMORecipeDiscoverySaveData& DiscoveryData = LoadedWorldSave->PawnDiscoveredRecipesByGuid[PawnGuid];
            Discovery->ApplySaveData(DiscoveryData);
        }
    }

    // (H30) Restore the 7 component states (+ combat/jobs) via the SAME routine
    // the full-load path uses. Before this, the possession-menu spawn skipped
    // all component restoration, so the character spawned full-healed and
    // skill-wiped — and the next autosave then captured that wiped state over
    // the good record. `Record` is a snapshot from GetPawnRecord; component data
    // lives on it.
    ApplyPawnComponentState(SpawnedPawn, Record);

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] SpawnPawnFromRecord: Spawned pawn %s (%s)"),
        *Record.CharacterName, *PawnGuid.ToString());

    return SpawnedPawn;
}

void UMOPersistenceSubsystem::RegisterPawnRecord(const FMOPersistedPawnRecord& Record)
{
    if (!LoadedWorldSave)
    {
        LoadedWorldSave = NewObject<UMOWorldSaveGame>(this);
    }

    // Check if already exists
    for (FMOPersistedPawnRecord& ExistingRecord : LoadedWorldSave->PersistedPawns)
    {
        if (ExistingRecord.PawnGuid == Record.PawnGuid)
        {
            ExistingRecord = Record;
            return;
        }
    }

    LoadedWorldSave->PersistedPawns.Add(Record);
    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Registered new pawn record: %s (%s)"),
        *Record.CharacterName, *Record.PawnGuid.ToString());
}

void UMOPersistenceSubsystem::MarkPawnDeceased(const FGuid& PawnGuid)
{
    if (!LoadedWorldSave || !PawnGuid.IsValid())
    {
        return;
    }

    for (FMOPersistedPawnRecord& Record : LoadedWorldSave->PersistedPawns)
    {
        if (Record.PawnGuid == PawnGuid)
        {
            Record.bIsDeceased = true;
            Record.StatusText = TEXT("Deceased");
            UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Marked pawn as deceased: %s"), *PawnGuid.ToString());
            return;
        }
    }
}

bool UMOPersistenceSubsystem::HasLivingPawns() const
{
    if (!LoadedWorldSave)
    {
        return false;
    }

    for (const FMOPersistedPawnRecord& Record : LoadedWorldSave->PersistedPawns)
    {
        if (!Record.bIsDeceased)
        {
            return true;
        }
    }
    return false;
}

void UMOPersistenceSubsystem::ApplyDestroyedGuidsToWorld(UWorld* World)
{
    if (!World)
    {
        return;
    }

    const ENetMode NetMode = World->GetNetMode();
    int32 MatchesFound = 0;
    int32 DestroyIssued = 0;

    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] ApplyDestroyedGuidsToWorld World=%s NetMode=%d DestroyedCount=%d"),
        *World->GetName(), (int32)NetMode, SessionDestroyedGuids.Num());

    if (SessionDestroyedGuids.Num() == 0)
    {
        return;
    }

    for (TActorIterator<AActor> ActorIterator(World); ActorIterator; ++ActorIterator)
    {
        AActor* Actor = *ActorIterator;
        if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
        {
            continue;
        }

        UMOIdentityComponent* IdentityComponent = Actor->FindComponentByClass<UMOIdentityComponent>();
        if (!IsValid(IdentityComponent) || !IdentityComponent->HasValidGuid())
        {
            continue;
        }

        const FGuid ActorGuid = IdentityComponent->GetGuid();
        if (!SessionDestroyedGuids.Contains(ActorGuid))
        {
            continue;
        }

        MatchesFound++;

        const bool bActorReplicates = Actor->GetIsReplicated();
        const bool bHasAuthority = Actor->HasAuthority();

        if (bActorReplicates)
        {
            if (bHasAuthority)
            {
                Actor->Destroy();
                DestroyIssued++;
            }
            continue;
        }

        Actor->Destroy();
        DestroyIssued++;
    }

    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Apply complete MatchesFound=%d DestroyIssued=%d"),
        MatchesFound, DestroyIssued);
}

void UMOPersistenceSubsystem::HandleIdentityRegistered(const FGuid& StableGuid, AActor* Actor)
{
    if (!StableGuid.IsValid() || !IsValid(Actor))
    {
        return;
    }

    if (SessionDestroyedGuids.Contains(StableGuid))
    {
        Actor->Destroy();
        return;
    }

    if (UMOIdentityComponent* IdentityComponent = Actor->FindComponentByClass<UMOIdentityComponent>())
    {
        IdentityComponent->OnOwnerDestroyedWithGuid.RemoveDynamic(this, &UMOPersistenceSubsystem::HandleIdentityDestroyed);
        IdentityComponent->OnOwnerDestroyedWithGuid.AddDynamic(this, &UMOPersistenceSubsystem::HandleIdentityDestroyed);
    }
}

void UMOPersistenceSubsystem::HandleIdentityDestroyed(const FGuid& StableGuid)
{
    if (!StableGuid.IsValid())
    {
        return;
    }

    // During load we intentionally destroy and respawn actors. Never record those as destroyed in save.
    if (bSuppressDestroyedGuidRecording || ReplacedGuidsThisLoad.Contains(StableGuid))
    {
        return;
    }

    SessionDestroyedGuids.Add(StableGuid);
}

/*
 * PAWNS + INVENTORY
 */

bool UMOPersistenceSubsystem::IsPersistedPawn(const APawn* Pawn) const
{
    if (!IsValid(Pawn))
    {
        return false;
    }

    // Use interfaces to check for required components
    // A persisted pawn must implement both IMOIdentifiable and IMOInventoryHolder
    if (!Pawn->Implements<UMOIdentifiableInterface>())
    {
        return false;
    }

    if (!Pawn->Implements<UMOInventoryHolderInterface>())
    {
        return false;
    }

    return true;
}

void UMOPersistenceSubsystem::CapturePersistedPawnsAndInventories(UWorld* World, UMOWorldSaveGame* SaveObject) const
{
    if (!World || !SaveObject)
    {
        return;
    }

    SaveObject->PersistedPawns.Reset();
    SaveObject->PawnInventoriesByGuid.Reset();

    int32 TotalPawns = 0;
    int32 SkippedNoIdentity = 0;
    int32 SkippedNoInventory = 0;

    for (TActorIterator<APawn> PawnIterator(World); PawnIterator; ++PawnIterator)
    {
        APawn* Pawn = *PawnIterator;
        if (!IsValid(Pawn))
        {
            continue;
        }

        TotalPawns++;

        // Use interfaces to access components
        if (!Pawn->Implements<UMOIdentifiableInterface>())
        {
            SkippedNoIdentity++;
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE: Skipping pawn '%s' - no IMOIdentifiable"), *Pawn->GetName());
            continue;
        }

        if (!Pawn->Implements<UMOInventoryHolderInterface>())
        {
            SkippedNoInventory++;
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE: Skipping pawn '%s' - no IMOInventoryHolder"), *Pawn->GetName());
            continue;
        }

        UMOIdentityComponent* IdentityComponent = IMOIdentifiableInterface::Execute_GetIdentityComponent(Pawn);
        UMOInventoryComponent* InventoryComponent = IMOInventoryHolderInterface::Execute_GetInventory(Pawn);

        if (!IsValid(IdentityComponent))
        {
            SkippedNoIdentity++;
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE: Skipping pawn '%s' - IdentityComponent invalid"), *Pawn->GetName());
            continue;
        }

        if (!IsValid(InventoryComponent))
        {
            SkippedNoInventory++;
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE: Skipping pawn '%s' - InventoryComponent invalid"), *Pawn->GetName());
            continue;
        }

        const FGuid PawnGuid = IdentityComponent->GetOrCreateGuid();
        if (!PawnGuid.IsValid())
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE: Skipping pawn '%s' - invalid GUID"), *Pawn->GetName());
            continue;
        }

        FMOPersistedPawnRecord PawnRecord;

        // Try to preserve existing record data (CharacterName, Gender, etc.)
        // This is important because UI edits update LoadedWorldSave but not the pawn itself
        if (LoadedWorldSave)
        {
            for (const FMOPersistedPawnRecord& ExistingRecord : LoadedWorldSave->PersistedPawns)
            {
                if (ExistingRecord.PawnGuid == PawnGuid)
                {
                    PawnRecord = ExistingRecord;
                    break;
                }
            }
        }

        // Update fields that come from the actual pawn state
        PawnRecord.PawnGuid = PawnGuid;
        PawnRecord.Transform = Pawn->GetActorTransform();

        // Permadeath capture: record live death state so a pawn that died THIS session is
        // saved deceased even if it had no prior record for MarkPawnDeceased to flip (spawned
        // + died before any save). Latches -- an already-deceased record stays deceased.
        if (const AMOCharacter* AsChar = Cast<AMOCharacter>(Pawn))
        {
            if (AsChar->IsDead())
            {
                PawnRecord.bIsDeceased = true;
            }
        }

        // [DIAG] Flag whether THIS pawn is the locally-controlled one. Helps
        // correlate save-vs-load: we want the player pawn's transform to
        // round-trip perfectly. Also dump SCALE — a "character shrinks each
        // load" bug means scale != (1,1,1) is being saved.
        const bool bIsPlayer = Pawn->IsPlayerControlled();
        UE_LOG(LogMOFramework, Warning,
            TEXT("[MOPersist] SAVE/CAPTURE: pawn '%s' GUID=%s  Player=%s  Location=%s  Scale=%s"),
            *Pawn->GetName(),
            *PawnGuid.ToString(EGuidFormats::Short),
            bIsPlayer ? TEXT("YES") : TEXT("no"),
            *PawnRecord.Transform.GetLocation().ToString(),
            *PawnRecord.Transform.GetScale3D().ToString());
        const FSoftObjectPath PawnClassSoftPath(Pawn->GetClass());
        PawnRecord.PawnClassPath = FSoftClassPath(PawnClassSoftPath.ToString());

        // Determine if pawn is player-controllable:
        // - Must not be a creature (deer, wolves, etc.)
        // - Must be recruited (via RecruitmentComponent)
        bool bCanControl = !Pawn->IsA<AMOCreature>();
        if (bCanControl)
        {
            if (UMORecruitmentComponent* RecruitComp = Pawn->FindComponentByClass<UMORecruitmentComponent>())
            {
                bCanControl = RecruitComp->IsPossessable();
            }
            // If no recruitment component, assume controllable (legacy support)
        }
        PawnRecord.bIsPlayerControllable = bCanControl;

        // Get character name: prefer IdentityComponent's DisplayName, then existing record, then generate default
        FText CurrentDisplayName = IdentityComponent->DisplayName;
        if (!CurrentDisplayName.IsEmpty())
        {
            PawnRecord.CharacterName = CurrentDisplayName.ToString();
        }
        else if (PawnRecord.CharacterName.IsEmpty())
        {
            // No name on component or in existing record - generate default
            FString GuidStr = PawnGuid.ToString(EGuidFormats::DigitsWithHyphens);
            FString ShortId = GuidStr.Right(4).ToUpper();
            PawnRecord.CharacterName = FString::Printf(TEXT("Character %s"), *ShortId);
        }
        // else: keep the name from the existing record

        // Capture component state data
        if (UMOVitalsComponent* VitalsComp = Pawn->FindComponentByClass<UMOVitalsComponent>())
        {
            VitalsComp->BuildSaveData(PawnRecord.VitalsData);
            PawnRecord.bHasComponentData = true;
        }
        if (UMOAnatomyComponent* AnatomyComp = Pawn->FindComponentByClass<UMOAnatomyComponent>())
        {
            AnatomyComp->BuildSaveData(PawnRecord.AnatomyData);
            PawnRecord.bHasComponentData = true;
        }
        if (UMOMetabolismComponent* MetabolismComp = Pawn->FindComponentByClass<UMOMetabolismComponent>())
        {
            MetabolismComp->BuildSaveData(PawnRecord.MetabolismData);
            PawnRecord.bHasComponentData = true;
        }
        if (UMOMentalStateComponent* MentalStateComp = Pawn->FindComponentByClass<UMOMentalStateComponent>())
        {
            MentalStateComp->BuildSaveData(PawnRecord.MentalStateData);
            PawnRecord.bHasComponentData = true;
        }
        if (UMOSkillsComponent* SkillsComp = Pawn->FindComponentByClass<UMOSkillsComponent>())
        {
            SkillsComp->BuildSaveData(PawnRecord.SkillsData);
            PawnRecord.bHasComponentData = true;
        }
        if (UMOEquipmentComponent* EquipmentComp = Pawn->FindComponentByClass<UMOEquipmentComponent>())
        {
            EquipmentComp->BuildSaveData(PawnRecord.EquipmentData);
            PawnRecord.bHasComponentData = true;
        }
        if (UMORecruitmentComponent* RecruitmentComp = Pawn->FindComponentByClass<UMORecruitmentComponent>())
        {
            RecruitmentComp->BuildSaveData(PawnRecord.RecruitmentData);
            PawnRecord.bHasComponentData = true;

            UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] SAVE: Recruitment state for '%s': State=%d, HasValidData=%d"),
                *Pawn->GetName(),
                static_cast<int32>(PawnRecord.RecruitmentData.RecruitmentState),
                PawnRecord.RecruitmentData.bHasValidData ? 1 : 0);
        }
        // (H38) Capture combat weapon state (durability/wear) so it survives reload.
        if (UMOCombatComponent* CombatComp = Pawn->FindComponentByClass<UMOCombatComponent>())
        {
            CombatComp->BuildSaveData(PawnRecord.CombatData);
            PawnRecord.bHasComponentData = true;
        }
        // (H39) Capture survivor job queue (queued jobs + home binding).
        if (UMOSurvivorJobQueueComponent* JobQueueComp = Pawn->FindComponentByClass<UMOSurvivorJobQueueComponent>())
        {
            JobQueueComp->BuildSaveData(PawnRecord.JobQueueData);
            PawnRecord.bHasComponentData = true;
        }

        UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] SAVE: Capturing pawn '%s' GUID=%s Name='%s' Class=%s Location=%s ComponentData=%s"),
            *Pawn->GetName(),
            *PawnGuid.ToString(EGuidFormats::DigitsWithHyphens),
            *PawnRecord.CharacterName,
            *PawnRecord.PawnClassPath.ToString(),
            *PawnRecord.Transform.GetLocation().ToString(),
            PawnRecord.bHasComponentData ? TEXT("YES") : TEXT("NO"));

        SaveObject->PersistedPawns.Add(PawnRecord);

        FMOInventorySaveData InventorySaveData;
        InventoryComponent->BuildSaveData(InventorySaveData);
        SaveObject->PawnInventoriesByGuid.Add(PawnGuid, InventorySaveData);

        // Capture crafting queue data
        if (UMOCraftingQueueComponent* CraftingQueue = Pawn->FindComponentByClass<UMOCraftingQueueComponent>())
        {
            FMOCraftingQueueSaveData QueueSaveData;
            CraftingQueue->BuildSaveData(QueueSaveData);
            SaveObject->PawnCraftingQueuesByGuid.Add(PawnGuid, QueueSaveData);
        }

        // Capture recipe discovery data
        if (UMORecipeDiscoveryComponent* Discovery = Pawn->FindComponentByClass<UMORecipeDiscoveryComponent>())
        {
            FMORecipeDiscoverySaveData DiscoverySaveData;
            Discovery->BuildSaveData(DiscoverySaveData);
            SaveObject->PawnDiscoveredRecipesByGuid.Add(PawnGuid, DiscoverySaveData);
        }
    }

    // ========================================================================
    // (H29) MERGE: preserve records for pawns NOT currently in the world.
    // ------------------------------------------------------------------------
    // The loop above rebuilt PersistedPawns from LIVE actors only. Records for
    // pawns that aren't spawned right now — survivors that failed to spawn
    // ("PAWN LOST", which the load path explicitly anticipates), or simply
    // pawns the streaming/possession system hasn't instantiated — would be
    // silently dropped by the next save, permanently destroying that character.
    //
    // Re-add any record from the previously-loaded save whose GUID we did NOT
    // capture from a live actor this pass, along with its inventory / crafting
    // queue / recipe-discovery side tables. Deceased records are preserved too
    // (HasLivingPawns / permadeath rely on them persisting).
    if (LoadedWorldSave)
    {
        // GUIDs captured from live actors this pass.
        TSet<FGuid> CapturedGuids;
        CapturedGuids.Reserve(SaveObject->PersistedPawns.Num());
        for (const FMOPersistedPawnRecord& Captured : SaveObject->PersistedPawns)
        {
            CapturedGuids.Add(Captured.PawnGuid);
        }

        int32 PreservedCount = 0;
        for (const FMOPersistedPawnRecord& ExistingRecord : LoadedWorldSave->PersistedPawns)
        {
            if (!ExistingRecord.PawnGuid.IsValid() || CapturedGuids.Contains(ExistingRecord.PawnGuid))
            {
                continue; // captured live above, or invalid — skip
            }

            // Don't resurrect records the player explicitly destroyed this
            // session (item/pawn deletion ledger). A destroyed-GUID pawn should
            // stay gone rather than be re-preserved.
            if (SessionDestroyedGuids.Contains(ExistingRecord.PawnGuid))
            {
                continue;
            }

            SaveObject->PersistedPawns.Add(ExistingRecord);
            CapturedGuids.Add(ExistingRecord.PawnGuid);
            ++PreservedCount;

            // Carry over the side tables keyed by this GUID so the preserved
            // pawn keeps its inventory / queue / discoveries on next load.
            if (const FMOInventorySaveData* Inv = LoadedWorldSave->PawnInventoriesByGuid.Find(ExistingRecord.PawnGuid))
            {
                SaveObject->PawnInventoriesByGuid.Add(ExistingRecord.PawnGuid, *Inv);
            }
            if (const FMOCraftingQueueSaveData* Queue = LoadedWorldSave->PawnCraftingQueuesByGuid.Find(ExistingRecord.PawnGuid))
            {
                SaveObject->PawnCraftingQueuesByGuid.Add(ExistingRecord.PawnGuid, *Queue);
            }
            if (const FMORecipeDiscoverySaveData* Disc = LoadedWorldSave->PawnDiscoveredRecipesByGuid.Find(ExistingRecord.PawnGuid))
            {
                SaveObject->PawnDiscoveredRecipesByGuid.Add(ExistingRecord.PawnGuid, *Disc);
            }
        }

        if (PreservedCount > 0)
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE: (H29) preserved %d unspawned pawn record(s) not present in the live world"), PreservedCount);
        }
    }

    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE SUMMARY: TotalPawns=%d Captured=%d SkippedNoIdentity=%d SkippedNoInventory=%d Queues=%d Discoveries=%d"),
        TotalPawns, SaveObject->PersistedPawns.Num(), SkippedNoIdentity, SkippedNoInventory,
        SaveObject->PawnCraftingQueuesByGuid.Num(), SaveObject->PawnDiscoveredRecipesByGuid.Num());
}

void UMOPersistenceSubsystem::UnpossessAllControllers(UWorld* World) const
{
    if (!World)
    {
        return;
    }

    for (FConstPlayerControllerIterator ControllerIterator = World->GetPlayerControllerIterator(); ControllerIterator; ++ControllerIterator)
    {
        APlayerController* PlayerController = ControllerIterator->Get();
        if (!IsValid(PlayerController))
        {
            continue;
        }

        if (IsValid(PlayerController->GetPawn()))
        {
            PlayerController->UnPossess();
        }
    }
}

void UMOPersistenceSubsystem::DestroyAllPersistedPawns(UWorld* World)
{
    if (!World || World->GetNetMode() == NM_Client)
    {
        return;
    }

    TArray<APawn*> PawnsToDestroy;
    PawnsToDestroy.Reserve(16);

    for (TActorIterator<APawn> It(World); It; ++It)
    {
        APawn* Pawn = *It;
        if (!IsValid(Pawn))
        {
            continue;
        }

        if (!IsPersistedPawn(Pawn))
        {
            continue;
        }

        // Use interface to access identity - IsPersistedPawn already verified it implements the interface
        if (IMOIdentifiableInterface::Execute_HasValidIdentity(Pawn))
        {
            ReplacedGuidsThisLoad.Add(IMOIdentifiableInterface::Execute_GetPersistentGuid(Pawn));
        }

        PawnsToDestroy.Add(Pawn);
    }

    for (APawn* Pawn : PawnsToDestroy)
    {
        if (IsValid(Pawn))
        {
            Pawn->Destroy();
        }
    }
}

void UMOPersistenceSubsystem::RespawnPersistedPawns(UWorld* World, const TArray<FMOPersistedPawnRecord>& PersistedPawns, FMOLoadResult& OutResult)
{
    if (!World || World->GetNetMode() == NM_Client)
    {
        return;
    }

    for (const FMOPersistedPawnRecord& PawnRecord : PersistedPawns)
    {
        if (!PawnRecord.PawnGuid.IsValid())
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Skipping pawn with invalid GUID"));
            continue;
        }

        if (SessionDestroyedGuids.Contains(PawnRecord.PawnGuid))
        {
            // This is expected - pawn was destroyed, don't count as failure
            continue;
        }

        if (PawnRecord.bIsDeceased)
        {
            // Permadeath (core pillar): a pawn that died stays dead. The single-pawn
            // SpawnPawnFromRecord path already gates on this; this full-load loop did not,
            // so dead survivors/creatures resurrected as live actors on every load.
            continue;
        }

        UClass* LoadedPawnClass = TryLoadPawnClassFromSoftPath(PawnRecord.PawnClassPath);
        bool bUsedFallback = false;

        UClass* PawnClassToSpawn = LoadedPawnClass;
        if (!PawnClassToSpawn)
        {
            // Try fallback from Project Settings
            PawnClassToSpawn = UMOPersistenceSettings::GetDefaultPersistedPawnClass();
            bUsedFallback = true;

            if (PawnClassToSpawn)
            {
                UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Pawn class '%s' failed to load for Guid=%s, using fallback '%s'"),
                    *PawnRecord.PawnClassPath.ToString(),
                    *PawnRecord.PawnGuid.ToString(EGuidFormats::DigitsWithHyphens),
                    *PawnClassToSpawn->GetName());
            }
        }

        if (!PawnClassToSpawn)
        {
            // CRITICAL: No class available - pawn will be lost!
            UE_LOG(LogMOFramework, Error, TEXT("[MOPersist] PAWN LOST: No pawn class to spawn for Guid=%s (original class: %s). Configure 'DefaultPersistedPawnClass' in Project Settings > Plugins > MO Persistence to prevent data loss."),
                *PawnRecord.PawnGuid.ToString(EGuidFormats::DigitsWithHyphens),
                *PawnRecord.PawnClassPath.ToString());
            OutResult.PawnsFailed++;
            OutResult.FailedPawnGuids.Add(PawnRecord.PawnGuid);
            continue;
        }

        // [DIAG] log the transform we're ABOUT to spawn with — compare with the
        // SAVE log earlier to confirm round-trip integrity. Scale included so
        // we can spot the "character shrinks each load" cumulative bug.
        UE_LOG(LogMOFramework, Warning,
            TEXT("[MOPersist] LOAD/SPAWN: Guid=%s class=%s requested Location=%s Rotation=%s Scale=%s"),
            *PawnRecord.PawnGuid.ToString(EGuidFormats::Short),
            *PawnClassToSpawn->GetName(),
            *PawnRecord.Transform.GetLocation().ToString(),
            *PawnRecord.Transform.GetRotation().Rotator().ToString(),
            *PawnRecord.Transform.GetScale3D().ToString());

        APawn* DeferredPawn = World->SpawnActorDeferred<APawn>(
            PawnClassToSpawn,
            PawnRecord.Transform,
            nullptr,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

        if (!IsValid(DeferredPawn))
        {
            UE_LOG(LogMOFramework, Error, TEXT("[MOPersist] PAWN LOST: SpawnActorDeferred failed for Guid=%s class=%s"),
                *PawnRecord.PawnGuid.ToString(EGuidFormats::DigitsWithHyphens),
                *PawnClassToSpawn->GetName());
            OutResult.PawnsFailed++;
            OutResult.FailedPawnGuids.Add(PawnRecord.PawnGuid);
            continue;
        }

        // [DIAG] log location IMMEDIATELY after deferred spawn (before FinishSpawningActor).
        UE_LOG(LogMOFramework, Warning,
            TEXT("[MOPersist] LOAD/SPAWN: post-deferred actor location=%s (delta from requested: %.1fcm)"),
            *DeferredPawn->GetActorLocation().ToString(),
            FVector::Dist(DeferredPawn->GetActorLocation(), PawnRecord.Transform.GetLocation()));

        // Only disable auto-possession for player-controlled pawns, not AI creatures
        // Creatures need their AutoPossessAI to spawn their AI controller
        if (!DeferredPawn->IsA<AMOCreature>())
        {
            DeferredPawn->AutoPossessAI = EAutoPossessAI::Disabled;
        }
        DeferredPawn->AutoPossessPlayer = EAutoReceiveInput::Disabled;

        UMOIdentityComponent* IdentityComponent = DeferredPawn->FindComponentByClass<UMOIdentityComponent>();
        if (!IsValid(IdentityComponent))
        {
            // Component missing from blueprint - add it dynamically
            IdentityComponent = NewObject<UMOIdentityComponent>(DeferredPawn, UMOIdentityComponent::StaticClass(), TEXT("MOIdentityComponent"));
            if (IdentityComponent)
            {
                IdentityComponent->RegisterComponent();
                UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Added missing IdentityComponent to pawn class=%s"),
                    *PawnClassToSpawn->GetName());
            }
        }

        if (!IsValid(IdentityComponent))
        {
            UE_LOG(LogMOFramework, Error, TEXT("[MOPersist] PAWN LOST: Failed to create IdentityComponent for Guid=%s class=%s"),
                *PawnRecord.PawnGuid.ToString(EGuidFormats::DigitsWithHyphens),
                *PawnClassToSpawn->GetName());
            DeferredPawn->Destroy();
            OutResult.PawnsFailed++;
            OutResult.FailedPawnGuids.Add(PawnRecord.PawnGuid);
            continue;
        }

        // Also ensure InventoryComponent exists
        UMOInventoryComponent* InventoryComponent = DeferredPawn->FindComponentByClass<UMOInventoryComponent>();
        if (!IsValid(InventoryComponent))
        {
            InventoryComponent = NewObject<UMOInventoryComponent>(DeferredPawn, UMOInventoryComponent::StaticClass(), TEXT("MOInventoryComponent"));
            if (InventoryComponent)
            {
                InventoryComponent->RegisterComponent();
                UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Added missing InventoryComponent to pawn class=%s"),
                    *PawnClassToSpawn->GetName());
            }
        }

        if (!AssignGuidToIdentityComponent(IdentityComponent, PawnRecord.PawnGuid))
        {
            UE_LOG(LogMOFramework, Error, TEXT("[MOPersist] PAWN LOST: Failed to assign GUID %s to pawn"),
                *PawnRecord.PawnGuid.ToString(EGuidFormats::DigitsWithHyphens));
            DeferredPawn->Destroy();
            OutResult.PawnsFailed++;
            OutResult.FailedPawnGuids.Add(PawnRecord.PawnGuid);
            continue;
        }

        // Apply the saved character name to the identity component
        if (!PawnRecord.CharacterName.IsEmpty())
        {
            IdentityComponent->SetDisplayName(FText::FromString(PawnRecord.CharacterName));
            UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Applied saved name '%s' to pawn GUID=%s"),
                *PawnRecord.CharacterName, *PawnRecord.PawnGuid.ToString(EGuidFormats::Short));
        }

        UGameplayStatics::FinishSpawningActor(DeferredPawn, PawnRecord.Transform);
        OutResult.PawnsLoaded++;

        // (H35) Adopt restored creatures into spawn tracking so they count toward
        // population caps / FIFO despawn / AI-freeze — otherwise every save/load
        // cycle compounds the creature population. Survivors keep their
        // recruitment-managed handling; the player matches no spawn category and
        // is skipped inside AdoptRestoredEntity.
        if (DeferredPawn->IsA<AMOCreature>())
        {
            if (UMOSpawnManagerSubsystem* SpawnMgr = World->GetSubsystem<UMOSpawnManagerSubsystem>())
            {
                SpawnMgr->AdoptRestoredEntity(DeferredPawn);
            }
        }

        // [DIAG] log final location AND scale after FinishSpawningActor. If
        // scale doesn't match the requested scale, something in BeginPlay
        // (ApplyAppearance, ApplyBodyParameters) is reapplying it.
        UE_LOG(LogMOFramework, Warning,
            TEXT("[MOPersist] LOAD/SPAWN: post-finish location=%s scale=%s (loc drift: %.1fcm)"),
            *DeferredPawn->GetActorLocation().ToString(),
            *DeferredPawn->GetActorScale3D().ToString(),
            FVector::Dist(DeferredPawn->GetActorLocation(), PawnRecord.Transform.GetLocation()));

        if (bUsedFallback)
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Pawn Guid=%s spawned using fallback class"),
                *PawnRecord.PawnGuid.ToString(EGuidFormats::DigitsWithHyphens));
        }
    }
}

void UMOPersistenceSubsystem::ApplyPawnComponentState(APawn* Pawn, const FMOPersistedPawnRecord& PawnRecord) const
{
    // (H30) Single shared "restore component state" routine. This is the ONLY
    // place that replays the live-load component restoration, so the full-load
    // path and the possession-menu spawn path can never drift (the bug: spawn
    // path full-healed + skill-wiped, then the next save overwrote the good
    // record). Mirrors exactly the BuildSaveData captures in
    // CapturePersistedPawnsAndInventories.
    if (!IsValid(Pawn))
    {
        return;
    }

    if (!PawnRecord.bHasComponentData)
    {
        // Legacy record with no component data — nothing to apply. (H38/H39
        // JobQueueData/CombatData also default to invalid here, so they no-op.)
        return;
    }

    // Apply vitals
    if (UMOVitalsComponent* VitalsComp = Pawn->FindComponentByClass<UMOVitalsComponent>())
    {
        VitalsComp->ApplySaveDataAuthority(PawnRecord.VitalsData);
    }
    // Apply anatomy
    if (UMOAnatomyComponent* AnatomyComp = Pawn->FindComponentByClass<UMOAnatomyComponent>())
    {
        AnatomyComp->ApplySaveDataAuthority(PawnRecord.AnatomyData);
    }
    // Apply metabolism
    if (UMOMetabolismComponent* MetabolismComp = Pawn->FindComponentByClass<UMOMetabolismComponent>())
    {
        MetabolismComp->ApplySaveDataAuthority(PawnRecord.MetabolismData);
    }
    // Apply mental state
    if (UMOMentalStateComponent* MentalStateComp = Pawn->FindComponentByClass<UMOMentalStateComponent>())
    {
        MentalStateComp->ApplySaveDataAuthority(PawnRecord.MentalStateData);
    }
    // Apply skills
    if (UMOSkillsComponent* SkillsComp = Pawn->FindComponentByClass<UMOSkillsComponent>())
    {
        SkillsComp->ApplySaveData(PawnRecord.SkillsData);
    }
    // Apply equipment
    if (UMOEquipmentComponent* EquipmentComp = Pawn->FindComponentByClass<UMOEquipmentComponent>())
    {
        EquipmentComp->ApplySaveData(PawnRecord.EquipmentData);
    }
    // Apply recruitment state
    if (UMORecruitmentComponent* RecruitmentComp = Pawn->FindComponentByClass<UMORecruitmentComponent>())
    {
        const bool bHadValidData = PawnRecord.RecruitmentData.bHasValidData;
        const int32 SavedState = static_cast<int32>(PawnRecord.RecruitmentData.RecruitmentState);
        const bool bApplied = RecruitmentComp->ApplySaveDataAuthority(PawnRecord.RecruitmentData);
        const int32 FinalState = static_cast<int32>(RecruitmentComp->RecruitmentState);

        UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Recruitment state: GUID=%s, SavedValid=%d, SavedState=%d, Applied=%d, FinalState=%d"),
            *PawnRecord.PawnGuid.ToString(EGuidFormats::Short), bHadValidData ? 1 : 0, SavedState, bApplied ? 1 : 0, FinalState);
    }
    else
    {
        UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] No RecruitmentComponent on pawn GUID=%s"),
            *PawnRecord.PawnGuid.ToString(EGuidFormats::Short));
    }

    // (H38) Apply combat weapon state (durability/wear). Orphaned before — combat
    // wear evaporated every reload. Only applies if the record carried combat
    // data (old saves default-construct to zeroed weapons; ApplySaveDataAuthority
    // overwrites the live weapon, but a pawn with no combat component is skipped).
    if (UMOCombatComponent* CombatComp = Pawn->FindComponentByClass<UMOCombatComponent>())
    {
        CombatComp->ApplySaveDataAuthority(PawnRecord.CombatData);
    }

    // (H39) Apply survivor job queue (queued jobs + home binding). No-ops on
    // records saved before this field existed (bHasValidData=false).
    if (UMOSurvivorJobQueueComponent* JobQueueComp = Pawn->FindComponentByClass<UMOSurvivorJobQueueComponent>())
    {
        JobQueueComp->ApplySaveDataAuthority(PawnRecord.JobQueueData);
    }

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Applied component state data to pawn GUID=%s"),
        *PawnRecord.PawnGuid.ToString(EGuidFormats::Short));
}

void UMOPersistenceSubsystem::ApplyInventoriesToSpawnedPawns(UWorld* World, const TMap<FGuid, FMOInventorySaveData>& PawnInventoriesByGuid)
{
    if (!World || World->GetNetMode() == NM_Client)
    {
        return;
    }

    for (TActorIterator<APawn> PawnIterator(World); PawnIterator; ++PawnIterator)
    {
        APawn* Pawn = *PawnIterator;
        if (!IsValid(Pawn))
        {
            continue;
        }

        if (!IsPersistedPawn(Pawn))
        {
            continue;
        }

        // Use interfaces - IsPersistedPawn already verified they're implemented
        if (!IMOIdentifiableInterface::Execute_HasValidIdentity(Pawn))
        {
            continue;
        }

        const FGuid PawnGuid = IMOIdentifiableInterface::Execute_GetPersistentGuid(Pawn);
        if (PawnInventoryGuidsAppliedThisLoad.Contains(PawnGuid))
        {
            continue;
        }

        const FMOInventorySaveData* FoundSaveData = PawnInventoriesByGuid.Find(PawnGuid);
        if (!FoundSaveData)
        {
            continue;
        }

        UMOInventoryComponent* InventoryComponent = IMOInventoryHolderInterface::Execute_GetInventory(Pawn);
        if (!IsValid(InventoryComponent))
        {
            continue;
        }

        if (InventoryComponent->ApplySaveDataAuthority(*FoundSaveData))
        {
            PawnInventoryGuidsAppliedThisLoad.Add(PawnGuid);
        }

        // Apply crafting queue data if available
        if (LoadedWorldSave)
        {
            if (const FMOCraftingQueueSaveData* QueueData = LoadedWorldSave->PawnCraftingQueuesByGuid.Find(PawnGuid))
            {
                if (UMOCraftingQueueComponent* CraftingQueue = Pawn->FindComponentByClass<UMOCraftingQueueComponent>())
                {
                    CraftingQueue->ApplySaveData(*QueueData, true); // true = calculate offline progress
                    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Applied crafting queue data to pawn GUID=%s"),
                        *PawnGuid.ToString(EGuidFormats::Short));
                }
            }

            // Apply recipe discovery data if available
            if (const FMORecipeDiscoverySaveData* DiscoveryData = LoadedWorldSave->PawnDiscoveredRecipesByGuid.Find(PawnGuid))
            {
                if (UMORecipeDiscoveryComponent* Discovery = Pawn->FindComponentByClass<UMORecipeDiscoveryComponent>())
                {
                    Discovery->ApplySaveData(*DiscoveryData);
                    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Applied recipe discovery data to pawn GUID=%s (%d recipes)"),
                        *PawnGuid.ToString(EGuidFormats::Short), DiscoveryData->DiscoveredRecipes.Num());
                }
            }

            // (H30) Apply component state data from pawn record via the shared
            // routine (also used by SpawnPawnFromRecord, so the two paths match).
            for (const FMOPersistedPawnRecord& PawnRecord : LoadedWorldSave->PersistedPawns)
            {
                if (PawnRecord.PawnGuid == PawnGuid && PawnRecord.bHasComponentData)
                {
                    ApplyPawnComponentState(Pawn, PawnRecord);
                    break;
                }
            }
        }
    }
}

/*
 * WORLD ITEMS
 */

bool UMOPersistenceSubsystem::IsPersistedWorldItemActor(const AActor* Actor) const
{
    if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
    {
        return false;
    }

    // Exclude pawns from the "world item" list.
    if (Cast<APawn>(Actor))
    {
        return false;
    }

    const UMOIdentityComponent* IdentityComponent = Actor->FindComponentByClass<UMOIdentityComponent>();
    const UMOItemComponent* ItemComponent = Actor->FindComponentByClass<UMOItemComponent>();
    if (!IsValid(IdentityComponent) || !IsValid(ItemComponent))
    {
        return false;
    }

    return true;
}

void UMOPersistenceSubsystem::CaptureWorldItems(UWorld* World, UMOWorldSaveGame* SaveObject) const
{
    if (!World || !SaveObject)
    {
        return;
    }

    SaveObject->WorldItems.Reset();

    int32 TotalActors = 0;
    int32 SkippedNoPersist = 0;
    int32 SkippedNoIdentity = 0;
    int32 SkippedNoItem = 0;
    int32 SkippedDestroyed = 0;
    int32 SkippedPawn = 0;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        TotalActors++;

        // Detailed check to see why items might be skipped
        if (Actor->GetClass()->GetName().Contains(TEXT("WorldItem")) || Actor->GetClass()->GetName().Contains(TEXT("Apple")))
        {
            UMOIdentityComponent* DbgIdentity = Actor->FindComponentByClass<UMOIdentityComponent>();
            UMOItemComponent* DbgItem = Actor->FindComponentByClass<UMOItemComponent>();
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] WORLD ITEM CHECK: %s (Class: %s) at %s - Identity: %s, Item: %s, IsPawn: %s"),
                *Actor->GetName(),
                *Actor->GetClass()->GetName(),
                *Actor->GetActorLocation().ToString(),
                DbgIdentity ? TEXT("YES") : TEXT("NO"),
                DbgItem ? TEXT("YES") : TEXT("NO"),
                Cast<APawn>(Actor) ? TEXT("YES") : TEXT("NO"));
        }

        if (!IsPersistedWorldItemActor(Actor))
        {
            SkippedNoPersist++;
            continue;
        }

        UMOIdentityComponent* IdentityComponent = Actor->FindComponentByClass<UMOIdentityComponent>();
        UMOItemComponent* ItemComponent = Actor->FindComponentByClass<UMOItemComponent>();
        if (!IsValid(IdentityComponent))
        {
            SkippedNoIdentity++;
            continue;
        }
        if (!IsValid(ItemComponent))
        {
            SkippedNoItem++;
            continue;
        }

        const FGuid ItemGuid = IdentityComponent->GetOrCreateGuid();
        if (!ItemGuid.IsValid())
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE ITEMS: Skipping item '%s' - invalid GUID"), *Actor->GetName());
            continue;
        }

        // Do not save items that are marked destroyed.
        if (SessionDestroyedGuids.Contains(ItemGuid))
        {
            SkippedDestroyed++;
            UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] SAVE ITEMS: Skipping destroyed item '%s' GUID=%s"),
                *Actor->GetName(), *ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
            continue;
        }

        FMOPersistedWorldItemRecord ItemRecord;
        ItemRecord.ItemGuid = ItemGuid;
        ItemRecord.Transform = Actor->GetActorTransform();
        ItemRecord.ItemClassPath = FSoftClassPath(Actor->GetClass()->GetPathName());
        ItemRecord.ItemDefinitionId = ItemComponent->ItemDefinitionId;
        ItemRecord.Quantity = FMath::Max(1, ItemComponent->Quantity);

        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE ITEMS: Capturing item '%s' GUID=%s Class=%s Location=%s"),
            *Actor->GetName(),
            *ItemGuid.ToString(EGuidFormats::DigitsWithHyphens),
            *ItemRecord.ItemClassPath.ToString(),
            *ItemRecord.Transform.GetLocation().ToString());

        SaveObject->WorldItems.Add(ItemRecord);
    }

    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE ITEMS SUMMARY: TotalActors=%d Captured=%d SkippedNoPersist=%d SkippedNoIdentity=%d SkippedNoItem=%d SkippedDestroyed=%d"),
        TotalActors, SaveObject->WorldItems.Num(), SkippedNoPersist, SkippedNoIdentity, SkippedNoItem, SkippedDestroyed);
}

void UMOPersistenceSubsystem::DestroyAllPersistedWorldItems(UWorld* World)
{
    if (!World || World->GetNetMode() == NM_Client)
    {
        return;
    }

    TArray<AActor*> ActorsToDestroy;
    ActorsToDestroy.Reserve(64);

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsPersistedWorldItemActor(Actor))
        {
            continue;
        }

        // World items implement IMOIdentifiable, use the interface
        if (Actor->Implements<UMOIdentifiableInterface>())
        {
            if (IMOIdentifiableInterface::Execute_HasValidIdentity(Actor))
            {
                ReplacedGuidsThisLoad.Add(IMOIdentifiableInterface::Execute_GetPersistentGuid(Actor));
            }
        }

        ActorsToDestroy.Add(Actor);
    }

    for (AActor* Actor : ActorsToDestroy)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
}

void UMOPersistenceSubsystem::RespawnWorldItems(UWorld* World, const TArray<FMOPersistedWorldItemRecord>& WorldItems, FMOLoadResult& OutResult)
{
    if (!World || World->GetNetMode() == NM_Client)
    {
        return;
    }

    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] LOAD ITEMS: Attempting to respawn %d world items"), WorldItems.Num());

    for (const FMOPersistedWorldItemRecord& ItemRecord : WorldItems)
    {
        if (!ItemRecord.ItemGuid.IsValid())
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] LOAD ITEMS: Skipping world item with invalid GUID"));
            continue;
        }

        if (SessionDestroyedGuids.Contains(ItemRecord.ItemGuid))
        {
            UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] LOAD ITEMS: Skipping destroyed item GUID=%s"),
                *ItemRecord.ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
            continue;
        }

        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] LOAD ITEMS: Respawning item GUID=%s Class=%s at Location=%s"),
            *ItemRecord.ItemGuid.ToString(EGuidFormats::DigitsWithHyphens),
            *ItemRecord.ItemClassPath.ToString(),
            *ItemRecord.Transform.GetLocation().ToString());

        UClass* LoadedItemClass = nullptr;
        if (ItemRecord.ItemClassPath.IsValid())
        {
            LoadedItemClass = ItemRecord.ItemClassPath.TryLoadClass<AActor>();
        }

        if (!LoadedItemClass)
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] World item class failed to load for Guid=%s ClassPath=%s"),
                *ItemRecord.ItemGuid.ToString(EGuidFormats::DigitsWithHyphens),
                *ItemRecord.ItemClassPath.ToString());
            OutResult.ItemsFailed++;
            continue;
        }

        AActor* DeferredActor = World->SpawnActorDeferred<AActor>(
            LoadedItemClass,
            ItemRecord.Transform,
            nullptr,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

        if (!IsValid(DeferredActor))
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SpawnActorDeferred failed for world item Guid=%s"),
                *ItemRecord.ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
            OutResult.ItemsFailed++;
            continue;
        }

        UMOIdentityComponent* IdentityComponent = DeferredActor->FindComponentByClass<UMOIdentityComponent>();
        UMOItemComponent* ItemComponent = DeferredActor->FindComponentByClass<UMOItemComponent>();

        if (!IsValid(IdentityComponent) || !IsValid(ItemComponent))
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Spawned world item missing required components for Guid=%s"),
                *ItemRecord.ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
            DeferredActor->Destroy();
            OutResult.ItemsFailed++;
            continue;
        }

        if (!AssignGuidToIdentityComponent(IdentityComponent, ItemRecord.ItemGuid))
        {
            UE_LOG(LogMOFramework, Error, TEXT("[MOPersist] ITEM LOST: Failed to assign GUID %s to world item"),
                *ItemRecord.ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
            DeferredActor->Destroy();
            OutResult.ItemsFailed++;
            continue;
        }

        ItemComponent->ItemDefinitionId = ItemRecord.ItemDefinitionId;
        ItemComponent->Quantity = FMath::Max(1, ItemRecord.Quantity);

        UGameplayStatics::FinishSpawningActor(DeferredActor, ItemRecord.Transform);

        // Force set the transform after spawn - OnConstruction may have reset it
        DeferredActor->SetActorTransform(ItemRecord.Transform);

        OutResult.ItemsLoaded++;

        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] LOAD ITEMS: Spawned item at final location %s (expected %s)"),
            *DeferredActor->GetActorLocation().ToString(),
            *ItemRecord.Transform.GetLocation().ToString());
    }
}

TArray<FString> UMOPersistenceSubsystem::GetAllSaveSlots() const
{
    TArray<FString> Result;

    const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("SaveGames");

    IFileManager& FileManager = IFileManager::Get();
    TArray<FString> FoundFiles;
    FileManager.FindFiles(FoundFiles, *SaveDir, TEXT("*.sav"));

    for (const FString& FileName : FoundFiles)
    {
        // Remove .sav extension
        FString SlotName = FPaths::GetBaseFilename(FileName);
        Result.Add(SlotName);
    }

    return Result;
}

TArray<FString> UMOPersistenceSubsystem::GetSaveSlotsForWorld(const FString& WorldIdentifier) const
{
    TArray<FString> AllSlots = GetAllSaveSlots();
    TArray<FString> FilteredSlots;

    for (const FString& Slot : AllSlots)
    {
        if (Slot.Contains(WorldIdentifier))
        {
            FilteredSlots.Add(Slot);
        }
    }

    return FilteredSlots;
}

FString UMOPersistenceSubsystem::GetCurrentWorldIdentifier() const
{
    UWorld* World = BoundWorld.Get();
    if (!World)
    {
        World = GetWorld();
    }

    if (!World)
    {
        return FString();
    }

    // Use the map name as the world identifier
    FString MapName = World->GetMapName();

    // Strip PIE prefixes if present
    MapName = StripUEDPIEPrefixes(MapName);

    // Remove path, keep just the map name
    return FPaths::GetBaseFilename(MapName);
}

bool UMOPersistenceSubsystem::DeleteSaveSlot(const FString& SlotName)
{
    return UGameplayStatics::DeleteGameInSlot(SlotName, 0);
}

bool UMOPersistenceSubsystem::RenameSaveSlot(const FString& SlotName, const FText& NewDisplayName)
{
    if (SlotName.IsEmpty())
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersistence] RenameSaveSlot: empty slot name"));
        return false;
    }

    // Load the existing save, mutate DisplayName, write back to the same slot.
    // The underlying file name (slot) stays stable; only the player-visible
    // label changes. Keeping the slot identifier stable means external code
    // that has cached SlotName references (CurrentSlotName, save slot dropdowns
    // mid-frame) doesn't break.
    USaveGame* LoadedBase = UGameplayStatics::LoadGameFromSlot(SlotName, 0);
    UMOWorldSaveGame* LoadedTyped = Cast<UMOWorldSaveGame>(LoadedBase);
    if (!LoadedTyped)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersistence] RenameSaveSlot: failed to load slot '%s'"), *SlotName);
        return false;
    }

    LoadedTyped->DisplayName = NewDisplayName.ToString();

    const bool bSaved = UGameplayStatics::SaveGameToSlot(LoadedTyped, SlotName, 0);
    if (!bSaved)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersistence] RenameSaveSlot: failed to write back slot '%s'"), *SlotName);
        return false;
    }

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersistence] Renamed slot '%s' display name to '%s'"),
        *SlotName, *NewDisplayName.ToString());
    return true;
}

bool UMOPersistenceSubsystem::DoesSaveSlotExist(const FString& SlotName) const
{
    return UGameplayStatics::DoesSaveGameExist(SlotName, 0);
}

bool UMOPersistenceSubsystem::GetSaveSlotMetadata(const FString& SlotName, FMOSaveMetadata& OutMetadata) const
{
    // Load the save game to extract metadata
    USaveGame* LoadedBase = UGameplayStatics::LoadGameFromSlot(SlotName, 0);
    UMOWorldSaveGame* LoadedTyped = Cast<UMOWorldSaveGame>(LoadedBase);
    if (!LoadedTyped)
    {
        return false;
    }

    OutMetadata.SlotName = SlotName;
    OutMetadata.DisplayName = LoadedTyped->DisplayName.IsEmpty()
        ? FText::FromString(SlotName)
        : FText::FromString(LoadedTyped->DisplayName);
    OutMetadata.Timestamp = LoadedTyped->SaveTimestamp;
    OutMetadata.PlayTime = FTimespan::FromSeconds(LoadedTyped->TotalPlayTimeSeconds);
    OutMetadata.WorldName = LoadedTyped->WorldName;
    OutMetadata.bIsAutosave = LoadedTyped->bIsAutosave;

    // Screenshot data (inline PNG bytes)
    OutMetadata.ScreenshotData = LoadedTyped->ScreenshotData;
    OutMetadata.ScreenshotPath = FString(); // Deprecated

    return true;
}

/*
 * BUILDINGS
 */

bool UMOPersistenceSubsystem::IsPersistedBuildingActor(const AActor* Actor) const
{
    if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
    {
        return false;
    }

    // Must be a buildable actor
    const AMOBuildableActor* Building = Cast<AMOBuildableActor>(Actor);
    if (!Building)
    {
        return false;
    }

    // Must have identity component
    const UMOIdentityComponent* IdentityComponent = Building->IdentityComponent;
    if (!IsValid(IdentityComponent))
    {
        return false;
    }

    return true;
}

void UMOPersistenceSubsystem::CaptureBuildings(UWorld* World, UMOWorldSaveGame* SaveObject) const
{
    if (!World || !SaveObject)
    {
        return;
    }

    SaveObject->Buildings.Reset();

    int32 TotalBuildings = 0;
    int32 SkippedNoIdentity = 0;
    int32 SkippedDestroyed = 0;

    for (TActorIterator<AMOBuildableActor> It(World); It; ++It)
    {
        AMOBuildableActor* Building = *It;
        TotalBuildings++;

        if (!IsPersistedBuildingActor(Building))
        {
            SkippedNoIdentity++;
            continue;
        }

        UMOIdentityComponent* IdentityComponent = Building->IdentityComponent;
        if (!IsValid(IdentityComponent))
        {
            SkippedNoIdentity++;
            continue;
        }

        const FGuid BuildingGuid = IdentityComponent->GetOrCreateGuid();
        if (!BuildingGuid.IsValid())
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE BUILDINGS: Skipping building '%s' - invalid GUID"), *Building->GetName());
            continue;
        }

        // Do not save buildings that are marked destroyed
        if (SessionDestroyedGuids.Contains(BuildingGuid))
        {
            SkippedDestroyed++;
            UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] SAVE BUILDINGS: Skipping destroyed building '%s' GUID=%s"),
                *Building->GetName(), *BuildingGuid.ToString(EGuidFormats::DigitsWithHyphens));
            continue;
        }

        FMOPersistedBuildingRecord BuildingRecord;
        Building->BuildSaveData(BuildingRecord);

        UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] SAVE BUILDINGS: Capturing building '%s' GUID=%s Recipe=%s State=%d Location=%s"),
            *Building->GetName(),
            *BuildingGuid.ToString(EGuidFormats::DigitsWithHyphens),
            *BuildingRecord.RecipeId.ToString(),
            (int32)Building->GetBuildState(),
            *BuildingRecord.Transform.GetLocation().ToString());

        SaveObject->Buildings.Add(BuildingRecord);
    }

    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE BUILDINGS SUMMARY: TotalBuildings=%d Captured=%d SkippedNoIdentity=%d SkippedDestroyed=%d"),
        TotalBuildings, SaveObject->Buildings.Num(), SkippedNoIdentity, SkippedDestroyed);
}

void UMOPersistenceSubsystem::DestroyAllPersistedBuildings(UWorld* World)
{
    if (!World || World->GetNetMode() == NM_Client)
    {
        return;
    }

    TArray<AActor*> BuildingsToDestroy;
    BuildingsToDestroy.Reserve(64);

    for (TActorIterator<AMOBuildableActor> It(World); It; ++It)
    {
        AMOBuildableActor* Building = *It;
        if (!IsPersistedBuildingActor(Building))
        {
            continue;
        }

        if (UMOIdentityComponent* IdentityComponent = Building->IdentityComponent)
        {
            if (IdentityComponent->HasValidGuid())
            {
                ReplacedGuidsThisLoad.Add(IdentityComponent->GetGuid());
            }
        }

        BuildingsToDestroy.Add(Building);
    }

    for (AActor* Building : BuildingsToDestroy)
    {
        if (IsValid(Building))
        {
            Building->Destroy();
        }
    }

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Destroyed %d existing buildings for reload"), BuildingsToDestroy.Num());
}

void UMOPersistenceSubsystem::RespawnBuildings(UWorld* World, const TArray<FMOPersistedBuildingRecord>& Buildings, FMOLoadResult& OutResult)
{
    if (!World || World->GetNetMode() == NM_Client)
    {
        return;
    }

    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] LOAD BUILDINGS: Attempting to respawn %d buildings"), Buildings.Num());

    for (const FMOPersistedBuildingRecord& BuildingRecord : Buildings)
    {
        if (!BuildingRecord.BuildingGuid.IsValid())
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] LOAD BUILDINGS: Skipping building with invalid GUID"));
            continue;
        }

        if (SessionDestroyedGuids.Contains(BuildingRecord.BuildingGuid))
        {
            UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] LOAD BUILDINGS: Skipping destroyed building GUID=%s"),
                *BuildingRecord.BuildingGuid.ToString(EGuidFormats::DigitsWithHyphens));
            continue;
        }

        UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] LOAD BUILDINGS: Respawning building GUID=%s Class=%s at Location=%s"),
            *BuildingRecord.BuildingGuid.ToString(EGuidFormats::DigitsWithHyphens),
            *BuildingRecord.ActorClassPath.ToString(),
            *BuildingRecord.Transform.GetLocation().ToString());

        UClass* LoadedBuildingClass = nullptr;
        if (BuildingRecord.ActorClassPath.IsValid())
        {
            LoadedBuildingClass = BuildingRecord.ActorClassPath.TryLoadClass<AMOBuildableActor>();
        }

        if (!LoadedBuildingClass)
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Building class failed to load for GUID=%s ClassPath=%s"),
                *BuildingRecord.BuildingGuid.ToString(EGuidFormats::DigitsWithHyphens),
                *BuildingRecord.ActorClassPath.ToString());
            OutResult.BuildingsFailed++;
            continue;
        }

        AActor* DeferredActor = World->SpawnActorDeferred<AActor>(
            LoadedBuildingClass,
            BuildingRecord.Transform,
            nullptr,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

        if (!IsValid(DeferredActor))
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SpawnActorDeferred failed for building GUID=%s"),
                *BuildingRecord.BuildingGuid.ToString(EGuidFormats::DigitsWithHyphens));
            OutResult.BuildingsFailed++;
            continue;
        }

        AMOBuildableActor* BuildingActor = Cast<AMOBuildableActor>(DeferredActor);
        if (!BuildingActor)
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Spawned actor is not AMOBuildableActor for GUID=%s"),
                *BuildingRecord.BuildingGuid.ToString(EGuidFormats::DigitsWithHyphens));
            DeferredActor->Destroy();
            OutResult.BuildingsFailed++;
            continue;
        }

        UMOIdentityComponent* IdentityComponent = BuildingActor->IdentityComponent;
        if (!IsValid(IdentityComponent))
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Spawned building missing IdentityComponent for GUID=%s"),
                *BuildingRecord.BuildingGuid.ToString(EGuidFormats::DigitsWithHyphens));
            DeferredActor->Destroy();
            OutResult.BuildingsFailed++;
            continue;
        }

        // Assign the saved GUID
        IdentityComponent->SetGuid(BuildingRecord.BuildingGuid);

        // Finish spawning
        UGameplayStatics::FinishSpawningActor(DeferredActor, BuildingRecord.Transform);

        // Apply the rest of the save data (progress, materials, etc.)
        BuildingActor->ApplySaveData(BuildingRecord);

        OutResult.BuildingsLoaded++;

        UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] LOAD BUILDINGS: Spawned building at %s (Recipe: %s, State: %d)"),
            *DeferredActor->GetActorLocation().ToString(),
            *BuildingRecord.RecipeId.ToString(),
            (int32)BuildingActor->GetBuildState());
    }
}

// ============================================================================
// VOXEL SCULPT PERSISTENCE
// ============================================================================

void UMOPersistenceSubsystem::CaptureVoxelSculptData(UWorld* World, UMOWorldSaveGame* SaveObject) const
{
    if (!World || !SaveObject)
    {
        return;
    }

    SaveObject->VoxelSculptData.Reset();

    // Capture height sculpt actors (facade hides the Voxel actor type + async save)
    TArray<AActor*> HeightActors;
    MOVoxel::GetHeightSculptActors(World, HeightActors);

    for (AActor* HeightActor : HeightActors)
    {
        if (!IsValid(HeightActor))
        {
            continue;
        }

        FMOVoxelSculptSaveRecord Record;
        Record.ActorName = HeightActor->GetName();
        Record.bIsVolumeSculpt = false;

        if (MOVoxel::SaveHeightSculpt(HeightActor, Record.SculptData))
        {
            Record.bHasValidData = true;
            UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Captured height sculpt '%s': %d bytes"),
                *Record.ActorName, Record.SculptData.Num());
            SaveObject->VoxelSculptData.Add(Record);
        }
        else
        {
            UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Height sculpt '%s' has no modifications to save"),
                *Record.ActorName);
        }
    }

    // Capture volume sculpt actors
    TArray<AActor*> VolumeActors;
    MOVoxel::GetVolumeSculptActors(World, VolumeActors);

    for (AActor* VolumeActor : VolumeActors)
    {
        if (!IsValid(VolumeActor))
        {
            continue;
        }

        FMOVoxelSculptSaveRecord Record;
        Record.ActorName = VolumeActor->GetName();
        Record.bIsVolumeSculpt = true;

        if (MOVoxel::SaveVolumeSculpt(VolumeActor, Record.SculptData))
        {
            Record.bHasValidData = true;
            UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Captured volume sculpt '%s': %d bytes"),
                *Record.ActorName, Record.SculptData.Num());
            SaveObject->VoxelSculptData.Add(Record);
        }
        else
        {
            UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Volume sculpt '%s' has no modifications to save"),
                *Record.ActorName);
        }
    }

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Captured %d voxel sculpt actor(s) with modifications"),
        SaveObject->VoxelSculptData.Num());
}

void UMOPersistenceSubsystem::RestoreVoxelSculptData(UWorld* World, const TArray<FMOVoxelSculptSaveRecord>& SculptData)
{
    if (!World || SculptData.Num() == 0)
    {
        return;
    }

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Restoring %d voxel sculpt record(s)"), SculptData.Num());

    for (const FMOVoxelSculptSaveRecord& Record : SculptData)
    {
        if (!Record.bHasValidData || Record.SculptData.Num() == 0)
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Skipping invalid sculpt record '%s'"),
                *Record.ActorName);
            continue;
        }

        if (Record.bIsVolumeSculpt)
        {
            // Find the volume sculpt actor by name (facade hides the Voxel type + async load)
            TArray<AActor*> VolumeActors;
            MOVoxel::GetVolumeSculptActors(World, VolumeActors);

            AActor* FoundActor = nullptr;
            for (AActor* Actor : VolumeActors)
            {
                if (Actor->GetName() == Record.ActorName)
                {
                    FoundActor = Actor;
                    break;
                }
            }

            if (!FoundActor)
            {
                UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Volume sculpt actor '%s' not found in world"),
                    *Record.ActorName);
                continue;
            }

            if (MOVoxel::LoadVolumeSculpt(FoundActor, Record.SculptData))
            {
                UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Restored volume sculpt '%s'"),
                    *Record.ActorName);
            }
        }
        else
        {
            // Find the height sculpt actor by name
            TArray<AActor*> HeightActors;
            MOVoxel::GetHeightSculptActors(World, HeightActors);

            AActor* FoundActor = nullptr;
            for (AActor* Actor : HeightActors)
            {
                if (Actor->GetName() == Record.ActorName)
                {
                    FoundActor = Actor;
                    break;
                }
            }

            if (!FoundActor)
            {
                UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Height sculpt actor '%s' not found in world"),
                    *Record.ActorName);
                continue;
            }

            if (MOVoxel::LoadHeightSculpt(FoundActor, Record.SculptData))
            {
                UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Restored height sculpt '%s'"),
                    *Record.ActorName);
            }
        }
    }
}

// ============================================================================
// SPECTATOR CAMERA SETUP
// ============================================================================

void UMOPersistenceSubsystem::SetupSpectatorCameraForLoad(UWorld* World, const UMOWorldSaveGame* SaveData)
{
    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SetupSpectatorCameraForLoad: CALLED"));

    if (!World || !SaveData)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SetupSpectatorCameraForLoad: World or SaveData null"));
        return;
    }

    // Get the local player controller
    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SetupSpectatorCameraForLoad: No GameInstance"));
        return;
    }

    APlayerController* PC = GI->GetFirstLocalPlayerController(World);
    AMOPlayerController* MOPC = Cast<AMOPlayerController>(PC);
    if (!MOPC)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SetupSpectatorCameraForLoad: No MOPlayerController found (PC=%s)"),
            PC ? *PC->GetClass()->GetName() : TEXT("null"));
        return;
    }

    // Find the pawn location from save data
    FVector PawnLocation = FVector::ZeroVector;
    bool bFoundLocation = false;

    // Try to find the last possessed pawn's location from pawn records
    if (SaveData->LastPossessedPawnGuid.IsValid())
    {
        for (const FMOPersistedPawnRecord& Record : SaveData->PersistedPawns)
        {
            if (Record.PawnGuid == SaveData->LastPossessedPawnGuid)
            {
                PawnLocation = Record.Transform.GetLocation();
                bFoundLocation = true;
                UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SetupSpectatorCameraForLoad: Found last possessed pawn at %s"),
                    *PawnLocation.ToString());
                break;
            }
        }
    }

    // If not found, use the first living pawn
    if (!bFoundLocation && SaveData->PersistedPawns.Num() > 0)
    {
        for (const FMOPersistedPawnRecord& Record : SaveData->PersistedPawns)
        {
            if (!Record.bIsDeceased)
            {
                PawnLocation = Record.Transform.GetLocation();
                bFoundLocation = true;
                UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SetupSpectatorCameraForLoad: Using first living pawn at %s"),
                    *PawnLocation.ToString());
                break;
            }
        }
    }

    if (!bFoundLocation)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SetupSpectatorCameraForLoad: No pawn found, using world origin"));
    }

    // Position camera above the pawn's X,Y at a fixed height, looking down
    const float CameraHeight = 5000.0f;
    const float PitchAngle = -60.0f;

    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SetupSpectatorCameraForLoad: Setting camera above X=%.1f Y=%.1f at height %.1f"),
        PawnLocation.X, PawnLocation.Y, CameraHeight);

    // Store pending view on player controller (MOSpectatorPawn will pick this up in BeginPlay)
    MOPC->SetSpectatorViewAboveLocation(PawnLocation, CameraHeight, PitchAngle);
}

// ============================================================================
// QUEST PERSISTENCE
// ============================================================================

void UMOPersistenceSubsystem::CaptureQuestData(UMOWorldSaveGame* SaveObject) const
{
    if (!SaveObject)
    {
        return;
    }

    UMOQuestSubsystem* QuestSubsystem = GetGameInstance()->GetSubsystem<UMOQuestSubsystem>();
    if (!QuestSubsystem)
    {
        UE_LOG(LogMOFramework, Verbose, TEXT("[MOPersist] CaptureQuestData: No quest subsystem"));
        return;
    }

    QuestSubsystem->BuildSaveData(SaveObject->QuestData);

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Captured quest data: %d active, %d completed"),
        SaveObject->QuestData.ActiveQuests.Num(),
        SaveObject->QuestData.CompletedQuestIds.Num());
}

void UMOPersistenceSubsystem::RestoreQuestData(const FMOQuestSaveData& QuestData)
{
    UMOQuestSubsystem* QuestSubsystem = GetGameInstance()->GetSubsystem<UMOQuestSubsystem>();
    if (!QuestSubsystem)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] RestoreQuestData: No quest subsystem"));
        return;
    }

    QuestSubsystem->ApplySaveData(QuestData);

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Restored quest data: %d active, %d completed"),
        QuestData.ActiveQuests.Num(),
        QuestData.CompletedQuestIds.Num());
}

// ============================================================================
// WEATHER/TIME PERSISTENCE
// ============================================================================

void UMOPersistenceSubsystem::CaptureWeatherData(UWorld* World, UMOWorldSaveGame* SaveObject) const
{
    if (!SaveObject)
    {
        return;
    }

    if (!World)
    {
        UE_LOG(LogMOFramework, Verbose, TEXT("[MOPersist] CaptureWeatherData: No world"));
        return;
    }

    UMOWeatherIntegrationSubsystem* WeatherSubsystem = World->GetSubsystem<UMOWeatherIntegrationSubsystem>();
    if (!WeatherSubsystem)
    {
        UE_LOG(LogMOFramework, Verbose, TEXT("[MOPersist] CaptureWeatherData: No weather integration subsystem"));
        return;
    }

    if (!WeatherSubsystem->HasWeatherProvider())
    {
        UE_LOG(LogMOFramework, Verbose, TEXT("[MOPersist] CaptureWeatherData: No weather provider registered"));
        return;
    }

    SaveObject->WeatherData = WeatherSubsystem->BuildWeatherSaveData();

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Captured weather data: DateTime=%s, Weather=%s, Valid=%s"),
        *SaveObject->WeatherData.DateTime.ToString(),
        *GetNameSafe(SaveObject->WeatherData.WeatherPresetObject.Get()),
        SaveObject->WeatherData.bIsValid ? TEXT("Yes") : TEXT("No"));
}

void UMOPersistenceSubsystem::RestoreWeatherData(UWorld* World, const FMOWeatherSaveData& WeatherData)
{
    if (!World)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] RestoreWeatherData: No world"));
        return;
    }

    if (!WeatherData.bIsValid)
    {
        UE_LOG(LogMOFramework, Verbose, TEXT("[MOPersist] RestoreWeatherData: Save data is not valid (possibly old save or no weather provider when saved)"));
        return;
    }

    UMOWeatherIntegrationSubsystem* WeatherSubsystem = World->GetSubsystem<UMOWeatherIntegrationSubsystem>();
    if (!WeatherSubsystem)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] RestoreWeatherData: No weather integration subsystem"));
        return;
    }

    // Always call ApplyWeatherSaveData - it will store as pending if no provider yet
    const bool bApplied = WeatherSubsystem->ApplyWeatherSaveData(WeatherData);

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Restored weather data: DateTime=%s, Applied=%s"),
        *WeatherData.DateTime.ToString(),
        bApplied ? TEXT("Yes") : TEXT("No"));
}

// ============================================================================
// TERRAIN MODIFICATION PERSISTENCE
// ============================================================================
//
// "Worked-ground" zone metadata — distinct from the Voxel plugin's own sculpt
// save (which handles the terrain mesh). This is just the bookkeeping that
// keeps PCG/foliage out of areas the player has modified.

void UMOPersistenceSubsystem::CaptureTerrainModificationData(UWorld* World, UMOWorldSaveGame* SaveObject) const
{
    if (!SaveObject || !World)
    {
        return;
    }

    UMOTerrainModificationSubsystem* Subsystem = World->GetSubsystem<UMOTerrainModificationSubsystem>();
    if (!Subsystem)
    {
        UE_LOG(LogMOFramework, Verbose, TEXT("[MOPersist] CaptureTerrainModificationData: No terrain-modification subsystem"));
        return;
    }

    Subsystem->BuildSaveData(SaveObject->TerrainModificationData);

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Captured terrain modification data: %d zone(s)"),
        SaveObject->TerrainModificationData.Zones.Num());
}

void UMOPersistenceSubsystem::RestoreTerrainModificationData(UWorld* World, const FMOTerrainModificationSaveData& TerrainModData)
{
    if (!World)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] RestoreTerrainModificationData: No world"));
        return;
    }

    UMOTerrainModificationSubsystem* Subsystem = World->GetSubsystem<UMOTerrainModificationSubsystem>();
    if (!Subsystem)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] RestoreTerrainModificationData: No terrain-modification subsystem"));
        return;
    }

    Subsystem->ApplySaveDataAuthority(TerrainModData);

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Restored terrain modification data: %d zone(s)"),
        TerrainModData.Zones.Num());
}

// ============================================================================
// GAME CLOCK PERSISTENCE  (H34)
// ============================================================================

void UMOPersistenceSubsystem::CaptureGameClockData(UWorld* World, UMOWorldSaveGame* SaveObject) const
{
    if (!SaveObject || !World)
    {
        return;
    }

    UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(World);
    if (!Clock)
    {
        UE_LOG(LogMOFramework, Verbose, TEXT("[MOPersist] CaptureGameClockData: No game clock subsystem"));
        return;
    }

    SaveObject->GameClockData = Clock->BuildSaveData();

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Captured game clock data (GameDateTime=%s, TimeScale=%.2f)"),
        *SaveObject->GameClockData.GameDateTime.ToString(), SaveObject->GameClockData.TimeScale);
}

void UMOPersistenceSubsystem::RestoreGameClockData(UWorld* World, const FMOGameClockSaveData& ClockData)
{
    if (!World)
    {
        return;
    }

    UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(World);
    if (!Clock)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] RestoreGameClockData: No game clock subsystem"));
        return;
    }

    // ApplySaveData no-ops internally on legacy/fresh saves (bIsValid=false),
    // leaving the clock at its fresh-start defaults.
    Clock->ApplySaveData(ClockData);

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Restored game clock data"));
}

// ============================================================================
// RESOURCE DEPLETION PERSISTENCE  (H37)
// ============================================================================

void UMOPersistenceSubsystem::CaptureResourceDepletionData(UWorld* World, UMOWorldSaveGame* SaveObject) const
{
    if (!SaveObject || !World)
    {
        return;
    }

    UMOResourceDepletionSubsystem* Subsystem = World->GetSubsystem<UMOResourceDepletionSubsystem>();
    if (!Subsystem)
    {
        UE_LOG(LogMOFramework, Verbose, TEXT("[MOPersist] CaptureResourceDepletionData: No resource-depletion subsystem"));
        return;
    }

    Subsystem->BuildSaveData(SaveObject->ResourceDepletionData);

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Captured resource depletion data: %d node(s)"),
        SaveObject->ResourceDepletionData.Entries.Num());
}

void UMOPersistenceSubsystem::RestoreResourceDepletionData(UWorld* World, const FMOResourceDepletionSaveData& DepletionData)
{
    if (!World)
    {
        return;
    }

    UMOResourceDepletionSubsystem* Subsystem = World->GetSubsystem<UMOResourceDepletionSubsystem>();
    if (!Subsystem)
    {
        UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] RestoreResourceDepletionData: No resource-depletion subsystem"));
        return;
    }

    Subsystem->ApplySaveDataAuthority(DepletionData);

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Restored resource depletion data: %d node(s)"),
        DepletionData.Entries.Num());
}
