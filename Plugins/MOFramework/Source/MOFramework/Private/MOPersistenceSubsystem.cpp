#include "MOPersistenceSubsystem.h"
#include "MOFramework.h"

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "MOIdentityComponent.h"
#include "MOIdentityRegistrySubsystem.h"
#include "MOInventoryComponent.h"
#include "MOItemComponent.h"
#include "MOPersistenceSettings.h"
#include "MOCraftingQueueComponent.h"
#include "MORecipeDiscoveryComponent.h"

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

    SaveObject->DestroyedGuids.Reset(SessionDestroyedGuids.Num());
    for (const FGuid& Guid : SessionDestroyedGuids)
    {
        SaveObject->DestroyedGuids.Add(Guid);
    }

    CapturePersistedPawnsAndInventories(World, SaveObject);
    CaptureWorldItems(World, SaveObject);

    const bool bOk = UGameplayStatics::SaveGameToSlot(SaveObject, SlotName, 0);

    if (bOk)
    {
        CurrentSlotName = SlotName;
    }

    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Save slot=%s ok=%d destroyed=%d pawns=%d inventories=%d worldItems=%d netmode=%d"),
        *SlotName,
        bOk ? 1 : 0,
        SaveObject->DestroyedGuids.Num(),
        SaveObject->PersistedPawns.Num(),
        SaveObject->PawnInventoriesByGuid.Num(),
        SaveObject->WorldItems.Num(),
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

    SessionDestroyedGuids.Reset();
    for (const FGuid& Guid : LoadedTyped->DestroyedGuids)
    {
        SessionDestroyedGuids.Add(Guid);
    }

    PawnInventoryGuidsAppliedThisLoad.Reset();
    ReplacedGuidsThisLoad.Reset();

    UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] LOAD: slot=%s destroyed=%d pawns=%d inventories=%d worldItems=%d netmode=%d"),
        *SlotName,
        LoadedTyped->DestroyedGuids.Num(),
        LoadedTyped->PersistedPawns.Num(),
        LoadedTyped->PawnInventoriesByGuid.Num(),
        LoadedTyped->WorldItems.Num(),
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

    RespawnPersistedPawns(World, LoadedTyped->PersistedPawns, LastLoadResult);
    RespawnWorldItems(World, LoadedTyped->WorldItems, LastLoadResult);

    ApplyInventoriesToSpawnedPawns(World, LoadedTyped->PawnInventoriesByGuid);

    // Track the current slot for auto-save support
    CurrentSlotName = SlotName;

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

    UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] Load complete: Pawns=%d/%d, Items=%d/%d"),
        LastLoadResult.PawnsLoaded, LastLoadResult.PawnsLoaded + LastLoadResult.PawnsFailed,
        LastLoadResult.ItemsLoaded, LastLoadResult.ItemsLoaded + LastLoadResult.ItemsFailed);

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

    const UMOIdentityComponent* IdentityComponent = Pawn->FindComponentByClass<UMOIdentityComponent>();
    if (!IsValid(IdentityComponent))
    {
        return false;
    }

    const UMOInventoryComponent* InventoryComponent = Pawn->FindComponentByClass<UMOInventoryComponent>();
    if (!IsValid(InventoryComponent))
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

        // Debug: Check why pawns might be skipped
        UMOIdentityComponent* IdentityComponent = Pawn->FindComponentByClass<UMOIdentityComponent>();
        UMOInventoryComponent* InventoryComponent = Pawn->FindComponentByClass<UMOInventoryComponent>();

        if (!IsValid(IdentityComponent))
        {
            SkippedNoIdentity++;
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE: Skipping pawn '%s' - no IdentityComponent"), *Pawn->GetName());
            continue;
        }

        if (!IsValid(InventoryComponent))
        {
            SkippedNoInventory++;
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] SAVE: Skipping pawn '%s' - no InventoryComponent"), *Pawn->GetName());
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
        const FSoftObjectPath PawnClassSoftPath(Pawn->GetClass());
        PawnRecord.PawnClassPath = FSoftClassPath(PawnClassSoftPath.ToString());

        // Generate default name for new pawns without an existing record
        if (PawnRecord.CharacterName.IsEmpty())
        {
            // Use last 4 hex digits of GUID for unique identifier
            FString GuidStr = PawnGuid.ToString(EGuidFormats::DigitsWithHyphens);
            FString ShortId = GuidStr.Right(4).ToUpper();
            PawnRecord.CharacterName = FString::Printf(TEXT("Character %s"), *ShortId);
        }

        UE_LOG(LogMOFramework, Log, TEXT("[MOPersist] SAVE: Capturing pawn '%s' GUID=%s Name='%s' Class=%s Location=%s"),
            *Pawn->GetName(),
            *PawnGuid.ToString(EGuidFormats::DigitsWithHyphens),
            *PawnRecord.CharacterName,
            *PawnRecord.PawnClassPath.ToString(),
            *PawnRecord.Transform.GetLocation().ToString());

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

        if (UMOIdentityComponent* IdentityComponent = Pawn->FindComponentByClass<UMOIdentityComponent>())
        {
            if (IdentityComponent->HasValidGuid())
            {
                ReplacedGuidsThisLoad.Add(IdentityComponent->GetGuid());
            }
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

        DeferredPawn->AutoPossessAI = EAutoPossessAI::Disabled;
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

        UGameplayStatics::FinishSpawningActor(DeferredPawn, PawnRecord.Transform);
        OutResult.PawnsLoaded++;

        if (bUsedFallback)
        {
            UE_LOG(LogMOFramework, Warning, TEXT("[MOPersist] Pawn Guid=%s spawned using fallback class"),
                *PawnRecord.PawnGuid.ToString(EGuidFormats::DigitsWithHyphens));
        }
    }
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

        UMOIdentityComponent* IdentityComponent = Pawn->FindComponentByClass<UMOIdentityComponent>();
        if (!IsValid(IdentityComponent) || !IdentityComponent->HasValidGuid())
        {
            continue;
        }

        const FGuid PawnGuid = IdentityComponent->GetGuid();
        if (PawnInventoryGuidsAppliedThisLoad.Contains(PawnGuid))
        {
            continue;
        }

        const FMOInventorySaveData* FoundSaveData = PawnInventoriesByGuid.Find(PawnGuid);
        if (!FoundSaveData)
        {
            continue;
        }

        UMOInventoryComponent* InventoryComponent = Pawn->FindComponentByClass<UMOInventoryComponent>();
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

        if (UMOIdentityComponent* IdentityComponent = Actor->FindComponentByClass<UMOIdentityComponent>())
        {
            if (IdentityComponent->HasValidGuid())
            {
                ReplacedGuidsThisLoad.Add(IdentityComponent->GetGuid());
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

        AssignGuidToIdentityComponent(IdentityComponent, ItemRecord.ItemGuid);

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

bool UMOPersistenceSubsystem::DoesSaveSlotExist(const FString& SlotName) const
{
    return UGameplayStatics::DoesSaveGameExist(SlotName, 0);
}
