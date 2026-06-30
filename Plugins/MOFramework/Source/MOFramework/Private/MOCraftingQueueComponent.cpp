#include "MOCraftingQueueComponent.h"
#include "MOFramework.h"
#include "MOCraftingSubsystem.h"
#include "MOInventoryComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOSkillsComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOItemDatabaseSettings.h"
#include "MOIdentifiableInterface.h"
#include "MOCharacter.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"

UMOCraftingQueueComponent::UMOCraftingQueueComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UMOCraftingQueueComponent::BeginPlay()
{
	Super::BeginPlay();
	CacheComponents();
	Queue.SetOwner(this);
}

void UMOCraftingQueueComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Stop crafting on end play
	if (bIsCraftingActive)
	{
		PauseCrafting();
	}

	// Defensive — PauseCrafting() above already unregisters, but if the
	// component is being destroyed while paused we still want a clean slate
	// on the character's listener list.
	UnregisterFromOwnerInterrupts();

	Super::EndPlay(EndPlayReason);
}

void UMOCraftingQueueComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsCraftingActive && !IsQueueEmpty())
	{
		ProcessCraftingTick(DeltaTime);
	}
}

void UMOCraftingQueueComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMOCraftingQueueComponent, Queue);
	DOREPLIFETIME(UMOCraftingQueueComponent, bIsCraftingActive);
}

void UMOCraftingQueueComponent::CacheComponents()
{
	if (AActor* Owner = GetOwner())
	{
		CachedInventory = Owner->FindComponentByClass<UMOInventoryComponent>();
		CachedDiscovery = Owner->FindComponentByClass<UMORecipeDiscoveryComponent>();
	}

	if (UWorld* World = GetWorld())
	{
		CachedCraftingSubsystem = World->GetSubsystem<UMOCraftingSubsystem>();
	}
}

// =============================================================================
// Queue Management
// =============================================================================

bool UMOCraftingQueueComponent::EnqueueCraft(FName RecipeId, int32 Count, EMOCraftingStation Station)
{
	if (RecipeId.IsNone() || Count <= 0)
	{
		return false;
	}

	// Server-authoritative: only the server mutates the replicated queue and consumes
	// ingredients. On a remote client this no-ops (matches UMOSurvivorJobQueueComponent).
	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingQueue] EnqueueCraft ignored on non-authority"));
		return false;
	}

	// Check queue size limit
	if (MaxQueueSize > 0 && Queue.Entries.Num() >= MaxQueueSize)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingQueue] Queue full (max %d)"), MaxQueueSize);
		return false;
	}

	// Verify recipe exists
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingQueue] Recipe not found: %s"), *RecipeId.ToString());
		return false;
	}

	// Consume ingredients upfront
	if (!ConsumeIngredientsForCraft(RecipeId, Count))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingQueue] Failed to consume ingredients for recipe: %s"), *RecipeId.ToString());
		return false;
	}

	// Create queue entry
	FMOCraftingQueueEntry NewEntry;
	NewEntry.EntryId = FGuid::NewGuid();
	NewEntry.RecipeId = RecipeId;
	NewEntry.Count = Count;
	NewEntry.CompletedCount = 0;
	NewEntry.Progress = 0.0f;
	NewEntry.Station = Station;
	NewEntry.bIngredientsConsumed = true;
	NewEntry.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	Queue.Entries.Add(NewEntry);
	Queue.MarkArrayDirty();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Enqueued %dx %s (EntryId: %s)"),
		Count, *RecipeId.ToString(), *NewEntry.EntryId.ToString(EGuidFormats::DigitsWithHyphens));

	OnQueueChanged.Broadcast();

	// Auto-start if this is the first entry and background crafting is allowed
	if (Queue.Entries.Num() == 1 && bAllowBackgroundCrafting && !bIsCraftingActive)
	{
		StartCrafting();
	}

	return true;
}

bool UMOCraftingQueueComponent::CancelCraft(const FGuid& EntryId, bool bRefundIngredients)
{
	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingQueue] CancelCraft ignored on non-authority"));
		return false;
	}

	for (int32 i = 0; i < Queue.Entries.Num(); ++i)
	{
		if (Queue.Entries[i].EntryId == EntryId)
		{
			FMOCraftingQueueEntry& Entry = Queue.Entries[i];

			// Refund remaining ingredients if requested
			if (bRefundIngredients && Entry.bIngredientsConsumed)
			{
				// Refund for remaining crafts (not yet completed)
				int32 RemainingCount = Entry.Count - Entry.CompletedCount;
				if (RemainingCount > 0)
				{
					RefundIngredientsForCraft(Entry.RecipeId, RemainingCount);
				}
			}

			FGuid CancelledId = Entry.EntryId;
			Queue.Entries.RemoveAt(i);
			Queue.MarkArrayDirty();

			UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Cancelled craft: %s (refunded: %s)"),
				*CancelledId.ToString(EGuidFormats::DigitsWithHyphens), bRefundIngredients ? TEXT("yes") : TEXT("no"));

			OnCraftCancelled.Broadcast(CancelledId, bRefundIngredients);
			OnQueueChanged.Broadcast();

			// If we cancelled the active craft, reset start time for next
			if (i == 0 && !IsQueueEmpty())
			{
				CurrentCraftStartTime = FDateTime::UtcNow();
				Queue.Entries[0].Progress = 0.0f;
				Queue.Entries[0].StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			}

			// Stop ticking if queue is now empty
			if (IsQueueEmpty())
			{
				PauseCrafting();
			}

			return true;
		}
	}

	return false;
}

void UMOCraftingQueueComponent::CancelAllCrafts(bool bRefundIngredients)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (Queue.Entries.Num() == 0)
	{
		return;
	}

	// Iterate backwards to avoid O(N²) from array shifting
	for (int32 i = Queue.Entries.Num() - 1; i >= 0; --i)
	{
		FMOCraftingQueueEntry& Entry = Queue.Entries[i];

		// Refund remaining ingredients if requested
		if (bRefundIngredients && Entry.bIngredientsConsumed)
		{
			int32 RemainingCount = Entry.Count - Entry.CompletedCount;
			if (RemainingCount > 0)
			{
				RefundIngredientsForCraft(Entry.RecipeId, RemainingCount);
			}
		}

		FGuid CancelledId = Entry.EntryId;
		Queue.Entries.RemoveAt(i);

		UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Cancelled craft: %s (refunded: %s)"),
			*CancelledId.ToString(EGuidFormats::DigitsWithHyphens), bRefundIngredients ? TEXT("yes") : TEXT("no"));

		OnCraftCancelled.Broadcast(CancelledId, bRefundIngredients);
	}

	Queue.MarkArrayDirty();
	OnQueueChanged.Broadcast();
	PauseCrafting();
}

bool UMOCraftingQueueComponent::ReorderQueueEntry(const FGuid& EntryId, int32 NewIndex)
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}

	// Can't reorder to position 0 if crafting is active (can't interrupt active craft)
	if (bIsCraftingActive && NewIndex == 0)
	{
		NewIndex = 1;
	}

	int32 CurrentIndex = INDEX_NONE;
	for (int32 i = 0; i < Queue.Entries.Num(); ++i)
	{
		if (Queue.Entries[i].EntryId == EntryId)
		{
			CurrentIndex = i;
			break;
		}
	}

	if (CurrentIndex == INDEX_NONE || CurrentIndex == NewIndex)
	{
		return false;
	}

	// Clamp new index
	NewIndex = FMath::Clamp(NewIndex, 0, Queue.Entries.Num() - 1);

	// Move the entry
	FMOCraftingQueueEntry Entry = Queue.Entries[CurrentIndex];
	Queue.Entries.RemoveAt(CurrentIndex);
	Queue.Entries.Insert(Entry, NewIndex);
	Queue.MarkArrayDirty();

	OnQueueChanged.Broadcast();
	return true;
}

// =============================================================================
// Progress Control
// =============================================================================

bool UMOCraftingQueueComponent::StartCrafting()
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}

	if (IsQueueEmpty())
	{
		return false;
	}

	bIsCraftingActive = true;
	CurrentCraftStartTime = FDateTime::UtcNow();

	// Enable ticking
	SetComponentTickEnabled(true);

	// Subscribe to owner interrupts. Now severe damage / knockdown / death /
	// combat / unconsciousness can pause this craft via the centralized bus.
	// Movement is intentionally policy'd out in NotifyInterrupt — the player
	// can walk around the station while a craft progresses on real-time.
	RegisterWithOwnerForInterrupts();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Crafting started"));

	return true;
}

void UMOCraftingQueueComponent::PauseCrafting()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (!bIsCraftingActive)
	{
		return;
	}

	bIsCraftingActive = false;

	// Disable ticking
	SetComponentTickEnabled(false);

	// Crafting is no longer actively progressing; drop the interrupt
	// subscription. StartCrafting() will re-register on resume.
	UnregisterFromOwnerInterrupts();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Crafting paused"));
}

// =============================================================================
// INTERRUPT HANDLING (IMOInterruptibleInterface)
// =============================================================================

void UMOCraftingQueueComponent::NotifyInterrupt_Implementation(const FMOInterruptContext& Context)
{
	if (!bIsCraftingActive)
	{
		// Defensive — should already be unregistered, but ignore is safer than
		// double-firing on a torn-down state.
		return;
	}

	switch (Context.Reason)
	{
	case EMOInterruptReason::Movement:
		// Crafting at a station is a long-running real-time activity. The
		// player can walk to a barrel, pick up an item, walk back — that
		// shouldn't void hours of progress. Movement is intentionally ignored
		// for crafting (unlike harvesting / building / inspecting).
		break;

	case EMOInterruptReason::Damage:
		// Trivial damage doesn't disrupt skilled work; real injury does.
		if (Context.Severity >= 0.5f)
		{
			UE_LOG(LogMOFramework, Log,
				TEXT("[MOCraftingQueue] Severe damage (severity=%.2f) — pausing crafting"),
				Context.Severity);
			PauseCrafting();
		}
		break;

	case EMOInterruptReason::Knockdown:
	case EMOInterruptReason::Unconscious:
	case EMOInterruptReason::EnteredCombat:
	case EMOInterruptReason::LostControl:
	case EMOInterruptReason::External:
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOCraftingQueue] Interrupt reason=%d — pausing crafting (will resume on next StartCrafting)"),
			(int32)Context.Reason);
		PauseCrafting();
		break;

	case EMOInterruptReason::Death:
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOCraftingQueue] Crafter died — cancelling all queued crafts with refund"));
		CancelAllCrafts(/*bRefundIngredients=*/true);
		break;

	case EMOInterruptReason::UserCancel:
	case EMOInterruptReason::None:
	default:
		break;
	}
}

void UMOCraftingQueueComponent::RegisterWithOwnerForInterrupts()
{
	AMOCharacter* OwnerChar = Cast<AMOCharacter>(GetOwner());
	if (!IsValid(OwnerChar))
	{
		return;
	}

	OwnerChar->RegisterInterruptListener(this);
	RegisteredCrafterCharacter = OwnerChar;
}

void UMOCraftingQueueComponent::UnregisterFromOwnerInterrupts()
{
	if (AMOCharacter* OwnerChar = RegisteredCrafterCharacter.Get())
	{
		OwnerChar->UnregisterInterruptListener(this);
	}
	RegisteredCrafterCharacter.Reset();
}

// =============================================================================
// Query
// =============================================================================

bool UMOCraftingQueueComponent::GetCurrentCraft(FMOCraftingQueueEntry& OutEntry) const
{
	if (Queue.Entries.Num() > 0)
	{
		OutEntry = Queue.Entries[0];
		return true;
	}
	return false;
}

void UMOCraftingQueueComponent::GetAllQueueEntries(TArray<FMOCraftingQueueEntry>& OutEntries) const
{
	OutEntries = Queue.Entries;
}

bool UMOCraftingQueueComponent::GetQueueEntry(const FGuid& EntryId, FMOCraftingQueueEntry& OutEntry) const
{
	for (const FMOCraftingQueueEntry& Entry : Queue.Entries)
	{
		if (Entry.EntryId == EntryId)
		{
			OutEntry = Entry;
			return true;
		}
	}
	return false;
}

float UMOCraftingQueueComponent::GetTotalTimeRemaining() const
{
	float Total = 0.0f;

	for (int32 i = 0; i < Queue.Entries.Num(); ++i)
	{
		const FMOCraftingQueueEntry& Entry = Queue.Entries[i];
		float CraftDuration = GetEffectiveCraftDuration(Entry.RecipeId);

		// For current craft, account for progress
		if (i == 0)
		{
			float RemainingOnCurrent = CraftDuration * (1.0f - Entry.Progress);
			int32 RemainingCrafts = Entry.Count - Entry.CompletedCount - 1; // -1 for current
			Total += RemainingOnCurrent + (RemainingCrafts * CraftDuration);
		}
		else
		{
			// Full duration for all repeats
			Total += CraftDuration * Entry.Count;
		}
	}

	return Total;
}

float UMOCraftingQueueComponent::GetCurrentCraftTimeRemaining() const
{
	if (Queue.Entries.Num() == 0)
	{
		return 0.0f;
	}

	const FMOCraftingQueueEntry& Entry = Queue.Entries[0];
	float CraftDuration = GetEffectiveCraftDuration(Entry.RecipeId);
	return CraftDuration * (1.0f - Entry.Progress);
}

float UMOCraftingQueueComponent::GetCurrentCraftProgress() const
{
	if (Queue.Entries.Num() == 0)
	{
		return 0.0f;
	}

	return Queue.Entries[0].Progress;
}

float UMOCraftingQueueComponent::GetOverallQueueProgress() const
{
	if (Queue.Entries.Num() == 0)
	{
		return 0.0f;
	}

	float TotalTime = 0.0f;
	float CompletedTime = 0.0f;

	for (int32 i = 0; i < Queue.Entries.Num(); ++i)
	{
		const FMOCraftingQueueEntry& Entry = Queue.Entries[i];
		float CraftDuration = GetEffectiveCraftDuration(Entry.RecipeId);

		// Total time for all repeats in this entry
		float EntryTotalTime = CraftDuration * Entry.Count;
		TotalTime += EntryTotalTime;

		// Time completed for this entry
		float EntryCompletedTime = CraftDuration * Entry.CompletedCount;

		// For the current craft (first entry), add partial progress
		if (i == 0)
		{
			EntryCompletedTime += CraftDuration * Entry.Progress;
		}

		CompletedTime += EntryCompletedTime;
	}

	if (TotalTime <= 0.0f)
	{
		return 1.0f; // All instant crafts
	}

	return FMath::Clamp(CompletedTime / TotalTime, 0.0f, 1.0f);
}

// =============================================================================
// Station Tracking
// =============================================================================

void UMOCraftingQueueComponent::SetActiveStation(AActor* Station)
{
	if (Station && !Station->Implements<UMOIdentifiableInterface>())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingQueue] SetActiveStation: Actor does not implement IMOIdentifiableInterface, ignoring"));
		return;
	}

	ActiveStation = Station;

	if (Station)
	{
		FGuid StationGuid = IMOIdentifiableInterface::Execute_GetPersistentGuid(Station);
		UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Active station set to: %s (GUID: %s)"),
			*Station->GetName(), *StationGuid.ToString(EGuidFormats::DigitsWithHyphens));
	}
	else
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Active station cleared"));
	}
}

void UMOCraftingQueueComponent::ClearActiveStation()
{
	ActiveStation.Reset();
	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Active station cleared"));
}

FGuid UMOCraftingQueueComponent::GetActiveStationGuid() const
{
	if (AActor* Station = ActiveStation.Get())
	{
		if (Station->Implements<UMOIdentifiableInterface>())
		{
			return IMOIdentifiableInterface::Execute_GetPersistentGuid(Station);
		}
	}
	return FGuid();
}

AActor* UMOCraftingQueueComponent::GetActiveStation() const
{
	return ActiveStation.Get();
}

// =============================================================================
// Save/Load
// =============================================================================

void UMOCraftingQueueComponent::BuildSaveData(FMOCraftingQueueSaveData& OutSaveData) const
{
	OutSaveData.QueuedCrafts = Queue.Entries;
	OutSaveData.PausedAt = FDateTime::UtcNow();
	OutSaveData.bWasActive = bIsCraftingActive;
	OutSaveData.ActiveStationGuid = GetActiveStationGuid();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Built save data: %d entries, active: %s, station: %s"),
		OutSaveData.QueuedCrafts.Num(),
		OutSaveData.bWasActive ? TEXT("yes") : TEXT("no"),
		OutSaveData.ActiveStationGuid.IsValid() ? *OutSaveData.ActiveStationGuid.ToString(EGuidFormats::DigitsWithHyphens) : TEXT("none"));
}

bool UMOCraftingQueueComponent::ApplySaveData(const FMOCraftingQueueSaveData& InSaveData, bool bCalculateOfflineProgress)
{
	// Clear current queue
	Queue.Entries = InSaveData.QueuedCrafts;
	Queue.MarkArrayDirty();

	if (bCalculateOfflineProgress && InSaveData.bWasActive && Queue.Entries.Num() > 0)
	{
		// Calculate time elapsed since save
		FDateTime Now = FDateTime::UtcNow();
		FTimespan Elapsed = Now - InSaveData.PausedAt;
		float ElapsedSeconds = static_cast<float>(Elapsed.GetTotalSeconds());

		// Clamp to reasonable maximum (e.g., 7 days) to prevent issues
		const float MaxOfflineSeconds = 7.0f * 24.0f * 60.0f * 60.0f;
		ElapsedSeconds = FMath::Min(ElapsedSeconds, MaxOfflineSeconds);

		if (ElapsedSeconds > 0.0f)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Applying %.1f seconds of offline progress"), ElapsedSeconds);
			AdvanceQueueByTime(ElapsedSeconds);
		}

		// Resume crafting if it was active
		if (!IsQueueEmpty())
		{
			StartCrafting();
		}
	}

	OnQueueChanged.Broadcast();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Applied save data: %d entries remaining"),
		Queue.Entries.Num());

	return true;
}

void UMOCraftingQueueComponent::ClearQueue()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	Queue.Entries.Empty();
	Queue.MarkArrayDirty();
	bIsCraftingActive = false;
	SetComponentTickEnabled(false);
	OnQueueChanged.Broadcast();
}

// =============================================================================
// Internal Methods
// =============================================================================

void UMOCraftingQueueComponent::ProcessCraftingTick(float DeltaTime)
{
	// Tick is only enabled by StartCrafting (authority-gated), but guard the body
	// directly — progress mutation is the one thing the queue must never do on a client.
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (Queue.Entries.Num() == 0)
	{
		PauseCrafting();
		return;
	}

	FMOCraftingQueueEntry& CurrentEntry = Queue.Entries[0];
	float CraftDuration = GetEffectiveCraftDuration(CurrentEntry.RecipeId);

	if (CraftDuration <= 0.0f)
	{
		// Instant craft
		CompletCurrentCraft();
		return;
	}

	// Calculate progress based on real time for accuracy
	FDateTime Now = FDateTime::UtcNow();
	FTimespan Elapsed = Now - CurrentCraftStartTime;
	float ElapsedSeconds = static_cast<float>(Elapsed.GetTotalSeconds());
	float NewProgress = FMath::Clamp(ElapsedSeconds / CraftDuration, 0.0f, 1.0f);

	// Update progress
	AccumulatedDeltaTime += DeltaTime;
	if (AccumulatedDeltaTime >= ProgressUpdateInterval)
	{
		AccumulatedDeltaTime = 0.0f;
		CurrentEntry.Progress = NewProgress;
		Queue.MarkItemDirty(CurrentEntry);

		OnCraftProgress.Broadcast(CurrentEntry.EntryId, NewProgress);
	}

	// Check for completion
	if (NewProgress >= 1.0f)
	{
		CompletCurrentCraft();
	}
}

void UMOCraftingQueueComponent::CompletCurrentCraft()
{
	if (Queue.Entries.Num() == 0)
	{
		return;
	}

	FMOCraftingQueueEntry& CurrentEntry = Queue.Entries[0];

	// Get recipe for output generation
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(CurrentEntry.RecipeId);
	if (!Recipe)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCraftingQueue] Recipe not found for completion: %s"), *CurrentEntry.RecipeId.ToString());
		Queue.Entries.RemoveAt(0);
		Queue.MarkArrayDirty();
		OnQueueChanged.Broadcast();
		return;
	}

	// Generate outputs via crafting subsystem
	// Use ProduceOutputsOnly since ingredients were already consumed at enqueue time
	FMOCraftResult Result;
	if (UMOCraftingSubsystem* CraftingSub = CachedCraftingSubsystem.Get())
	{
		// Get skills component from owner for XP rewards
		UMOSkillsComponent* SkillsComp = nullptr;
		if (AActor* Owner = GetOwner())
		{
			SkillsComp = Owner->FindComponentByClass<UMOSkillsComponent>();
		}
		Result = CraftingSub->ProduceOutputsOnly(CurrentEntry.RecipeId, CachedInventory.Get(), SkillsComp);
	}
	else
	{
		// Fallback: manually add outputs
		Result.bSuccess = true;
		if (UMOInventoryComponent* Inventory = CachedInventory.Get())
		{
			for (const FMORecipeOutput& Output : Recipe->Outputs)
			{
				// Check chance
				if (Output.Chance < 1.0f && FMath::FRand() > Output.Chance)
				{
					continue;
				}

				FGuid NewItemGuid = FGuid::NewGuid();
				Inventory->AddItemByGuid(NewItemGuid, Output.ItemDefinitionId, Output.Quantity);
				Result.ProducedItems.Add(Output.ItemDefinitionId, Output.Quantity);
			}
		}
	}

	CurrentEntry.CompletedCount++;
	FGuid CompletedEntryId = CurrentEntry.EntryId;

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Completed craft %d/%d for recipe %s"),
		CurrentEntry.CompletedCount, CurrentEntry.Count, *CurrentEntry.RecipeId.ToString());

	OnCraftCompleted.Broadcast(CompletedEntryId, Result);

	// Check if this entry has more repeats
	if (CurrentEntry.CompletedCount >= CurrentEntry.Count)
	{
		// Entry fully complete, remove it
		Queue.Entries.RemoveAt(0);
		Queue.MarkArrayDirty();
		OnQueueChanged.Broadcast();
	}
	else
	{
		// Reset for next repeat
		CurrentEntry.Progress = 0.0f;
		Queue.MarkItemDirty(CurrentEntry);
	}

	// Reset start time for next craft
	CurrentCraftStartTime = FDateTime::UtcNow();

	// Check if queue is now empty
	if (IsQueueEmpty())
	{
		PauseCrafting();
	}
}

bool UMOCraftingQueueComponent::ConsumeIngredientsForCraft(FName RecipeId, int32 Count)
{
	UMOInventoryComponent* Inventory = CachedInventory.Get();
	if (!Inventory)
	{
		CacheComponents();
		Inventory = CachedInventory.Get();
		if (!Inventory)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingQueue] No inventory component found"));
			return false;
		}
	}

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		return false;
	}

	// Cache inventory entries once - avoid repeated full copies
	TArray<FMOInventoryEntry> CachedEntries;
	Inventory->GetInventoryEntries(CachedEntries);

	// First verify we have enough of all ingredients
	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		int32 Required = Ingredient.Quantity * Count;
		int32 Available = 0;

		for (const FMOInventoryEntry& Entry : CachedEntries)
		{
			if (Entry.ItemDefinitionId == Ingredient.ItemDefinitionId)
			{
				Available += Entry.Quantity;
			}
		}

		if (Available < Required)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingQueue] Not enough %s (have %d, need %d)"),
				*Ingredient.ItemDefinitionId.ToString(), Available, Required);
			return false;
		}
	}

	// Now consume the ingredients (re-fetch entries since validation passed and we need current GUIDs)
	Inventory->GetInventoryEntries(CachedEntries);

	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		int32 ToConsume = Ingredient.Quantity * Count;

		for (const FMOInventoryEntry& Entry : CachedEntries)
		{
			if (Entry.ItemDefinitionId == Ingredient.ItemDefinitionId && ToConsume > 0)
			{
				int32 ConsumeFromThis = FMath::Min(Entry.Quantity, ToConsume);
				Inventory->RemoveItemByGuid(Entry.ItemGuid, ConsumeFromThis);
				ToConsume -= ConsumeFromThis;
			}
		}
	}

	return true;
}

void UMOCraftingQueueComponent::RefundIngredientsForCraft(FName RecipeId, int32 Count)
{
	UMOInventoryComponent* Inventory = CachedInventory.Get();
	if (!Inventory)
	{
		return;
	}

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		return;
	}

	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		int32 ToRefund = Ingredient.Quantity * Count;
		FGuid NewItemGuid = FGuid::NewGuid();
		Inventory->AddItemByGuid(NewItemGuid, Ingredient.ItemDefinitionId, ToRefund);

		UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Refunded %dx %s"),
			ToRefund, *Ingredient.ItemDefinitionId.ToString());
	}
}

float UMOCraftingQueueComponent::CalculateProgressFromTime(const FMOCraftingQueueEntry& Entry, float CraftDuration) const
{
	if (CraftDuration <= 0.0f)
	{
		return 1.0f;
	}

	FDateTime Now = FDateTime::UtcNow();
	FTimespan Elapsed = Now - CurrentCraftStartTime;
	float ElapsedSeconds = static_cast<float>(Elapsed.GetTotalSeconds());

	return FMath::Clamp(ElapsedSeconds / CraftDuration, 0.0f, 1.0f);
}

float UMOCraftingQueueComponent::GetEffectiveCraftDuration(FName RecipeId) const
{
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		return 0.0f;
	}

	// Use crafting subsystem's tool quality calculation if available
	UMOCraftingSubsystem* CraftingSubsystem = CachedCraftingSubsystem.Get();
	UMOInventoryComponent* Inventory = CachedInventory.Get();

	if (CraftingSubsystem && Inventory)
	{
		// GetAdjustedCraftTime applies tool quality modifiers
		return CraftingSubsystem->GetAdjustedCraftTime(RecipeId, Inventory);
	}

	// Fallback to base duration if subsystem or inventory unavailable
	return Recipe->CraftTime;
}

void UMOCraftingQueueComponent::AdvanceQueueByTime(float ElapsedSeconds)
{
	while (ElapsedSeconds > 0.0f && Queue.Entries.Num() > 0)
	{
		FMOCraftingQueueEntry& Entry = Queue.Entries[0];
		float CraftDuration = GetEffectiveCraftDuration(Entry.RecipeId);

		if (CraftDuration <= 0.0f)
		{
			// Instant craft
			CompletCurrentCraft();
			continue;
		}

		// Calculate how much time is left on current craft
		float TimeIntoCurrentCraft = Entry.Progress * CraftDuration;
		float TimeRemainingOnCurrent = CraftDuration - TimeIntoCurrentCraft;

		if (ElapsedSeconds >= TimeRemainingOnCurrent)
		{
			// This craft completes
			ElapsedSeconds -= TimeRemainingOnCurrent;
			Entry.Progress = 1.0f;
			CompletCurrentCraft();
		}
		else
		{
			// Partial progress
			Entry.Progress = (TimeIntoCurrentCraft + ElapsedSeconds) / CraftDuration;
			Queue.MarkItemDirty(Entry);
			ElapsedSeconds = 0.0f;
		}
	}

	// Reset start time for resumed crafting
	CurrentCraftStartTime = FDateTime::UtcNow();
}
