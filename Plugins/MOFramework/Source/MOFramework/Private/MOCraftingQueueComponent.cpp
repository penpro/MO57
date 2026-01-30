#include "MOCraftingQueueComponent.h"
#include "MOFramework.h"
#include "MOCraftingSubsystem.h"
#include "MOInventoryComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOItemDatabaseSettings.h"
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
	TArray<FGuid> EntryIds;
	for (const FMOCraftingQueueEntry& Entry : Queue.Entries)
	{
		EntryIds.Add(Entry.EntryId);
	}

	for (const FGuid& EntryId : EntryIds)
	{
		CancelCraft(EntryId, bRefundIngredients);
	}
}

bool UMOCraftingQueueComponent::ReorderQueueEntry(const FGuid& EntryId, int32 NewIndex)
{
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
	if (IsQueueEmpty())
	{
		return false;
	}

	bIsCraftingActive = true;
	CurrentCraftStartTime = FDateTime::UtcNow();

	// Enable ticking
	SetComponentTickEnabled(true);

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Crafting started"));

	return true;
}

void UMOCraftingQueueComponent::PauseCrafting()
{
	if (!bIsCraftingActive)
	{
		return;
	}

	bIsCraftingActive = false;

	// Disable ticking
	SetComponentTickEnabled(false);

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Crafting paused"));
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

// =============================================================================
// Save/Load
// =============================================================================

void UMOCraftingQueueComponent::BuildSaveData(FMOCraftingQueueSaveData& OutSaveData) const
{
	OutSaveData.QueuedCrafts = Queue.Entries;
	OutSaveData.PausedAt = FDateTime::UtcNow();
	OutSaveData.bWasActive = bIsCraftingActive;

	// TODO: Store active station GUID when AMOCraftingStationActor is implemented (Phase 3)
	OutSaveData.ActiveStationGuid = FGuid();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Built save data: %d entries, active: %s"),
		OutSaveData.QueuedCrafts.Num(), OutSaveData.bWasActive ? TEXT("yes") : TEXT("no"));
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
	FMOCraftResult Result;
	if (UMOCraftingSubsystem* CraftingSub = CachedCraftingSubsystem.Get())
	{
		Result = CraftingSub->ExecuteCraft(CurrentEntry.RecipeId, CachedInventory.Get(), nullptr);
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

	// First verify we have enough of all ingredients
	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		int32 Required = Ingredient.Quantity * Count;
		int32 Available = 0;

		TArray<FMOInventoryEntry> Entries;
		Inventory->GetInventoryEntries(Entries);

		for (const FMOInventoryEntry& Entry : Entries)
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

	// Now consume the ingredients
	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		int32 ToConsume = Ingredient.Quantity * Count;

		TArray<FMOInventoryEntry> Entries;
		Inventory->GetInventoryEntries(Entries);

		for (const FMOInventoryEntry& Entry : Entries)
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

	float BaseDuration = Recipe->CraftTime;

	// TODO: Apply tool quality modifiers here when tool system is integrated
	// For now, return base duration
	return BaseDuration;
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
