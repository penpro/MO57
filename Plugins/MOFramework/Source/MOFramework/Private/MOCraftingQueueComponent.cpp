#include "MOCraftingQueueComponent.h"
#include "MOFramework.h"
#include "MOGameClockSubsystem.h"
#include "MOCraftingSubsystem.h"
#include "MOInventoryComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOSkillsComponent.h"
#include "MOKnowledgeComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOItemDatabaseSettings.h"
#include "MOIdentifiableInterface.h"
#include "MOIdentityRegistrySubsystem.h"
#include "MOCharacter.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"

void FMOCraftingQueueList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	// Run after removals are applied, so client widgets rebuild from the final array.
	if (OwnerComponent) OwnerComponent->OnQueueChanged.Broadcast();
}

UMOCraftingQueueComponent::UMOCraftingQueueComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UMOCraftingQueueComponent::BeginPlay()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UMOCraftingQueueComponent_BeginPlay);   // spawn-cost breakdown in traces (-statnamedevents)
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

	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!PendingReturns.IsEmpty() || !PendingProducts.IsEmpty())
	{
		MaterialRetryElapsed += DeltaTime;
		if (MaterialRetryElapsed >= 1.0f)
		{
			MaterialRetryElapsed = 0.0f;
			DeliverPendingMaterials();
		}
	}
	if (bAwaitingContext && !IsQueueEmpty())
	{
		ContextRetryElapsed += DeltaTime;
		if (ContextRetryElapsed >= ProgressUpdateInterval)
		{
			ContextRetryElapsed = 0.0f;
			FText Reason;
			if (ValidateEntryContext(Queue.Entries[0], Reason)) StartCrafting();
			else WaitForContext(Reason);
		}
	}
	if (bIsCraftingActive && !IsQueueEmpty()) ProcessCraftingTick(DeltaTime);
}

void UMOCraftingQueueComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMOCraftingQueueComponent, Queue);
	DOREPLIFETIME(UMOCraftingQueueComponent, bIsCraftingActive);
	DOREPLIFETIME(UMOCraftingQueueComponent, bAwaitingContext);
	DOREPLIFETIME(UMOCraftingQueueComponent, CraftingBlockReason);
	DOREPLIFETIME_CONDITION(UMOCraftingQueueComponent, PendingMaterialLotCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMOCraftingQueueComponent, UnresolvedLegacyRefundCount, COND_OwnerOnly);
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
	// Kept for Blueprint/C++ compatibility. The enum is a display hint, never proof of a usable station.
	return EnqueueCraftAtStation(RecipeId, Count, ActiveStation.Get());
}

bool UMOCraftingQueueComponent::EnqueueCraftAtStation(FName RecipeId, int32 Count, AActor* StationActor)
{
	FGuid EntryId;
	return EnqueueCraftAtStationWithId(RecipeId, Count, StationActor, EntryId);
}

bool UMOCraftingQueueComponent::EnqueueCraftAtStationWithId(FName RecipeId, int32 Count, AActor* StationActor, FGuid& OutEntryId,
	TFunction<void(const FGuid&)> OnInstalled)
{
	OutEntryId.Invalidate();
	if (!GetOwner()) return false;
	if (RecipeId.IsNone() || Count <= 0)
	{
		return false;
	}

	// Server-authoritative clamp. EnqueueCraft is the single chokepoint the co-op
	// ServerRequestEnqueueCraft RPC also funnels through, so bound the (client-supplied)
	// repeat count here. Without it a huge Count overflowed the int32 `Ingredient.Quantity
	// * Count` below: Required wrapped negative, passed the availability check while
	// consuming almost nothing -> item duplication. Also caps absurd offline catch-up. (audit)
	Count = FMath::Min(Count, 1000);
	// Retained receipts are opt-in, one per assignment iteration, and bounded even if an executor disappears.
	if (OnInstalled && (Count != 1 || CompletionReceipts.Num() + Queue.Entries.Num() >= 64)) return false;

	// Server-authoritative: only the server mutates the replicated queue and consumes
	// ingredients. On a remote client this no-ops (matches UMOSurvivorJobQueueComponent).
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		// Co-op: a remote client that locally controls this pawn forwards the craft to the
		// host; the replicated Queue then updates its UI. A non-locally-controlled owner
		// (e.g. a simulated proxy) has no business enqueuing, so it stays a no-op.
		const APawn* OwnerPawn = Cast<APawn>(GetOwner());
		if (OwnerPawn && OwnerPawn->IsLocallyControlled())
		{
			ServerRequestEnqueueCraft(RecipeId, Count, StationActor);
			return true; // dispatched; authoritative result arrives via Queue replication
		}
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

	// Validate at the authoritative mutation boundary. UI state is advisory and
	// remote clients can call the RPC directly, so station, unlock, skill, tool,
	// and ingredient requirements must all be checked here before consumption.
	if (!CachedCraftingSubsystem.IsValid() || !CachedInventory.IsValid())
	{
		CacheComponents();
	}
	UMOCraftingSubsystem* Crafting = CachedCraftingSubsystem.Get();
	AActor* OwnerActor = GetOwner();
	UMOKnowledgeComponent* Knowledge = OwnerActor ? OwnerActor->FindComponentByClass<UMOKnowledgeComponent>() : nullptr;
	UMOSkillsComponent* Skills = OwnerActor ? OwnerActor->FindComponentByClass<UMOSkillsComponent>() : nullptr;
	const FMOCraftingValidation Validation = Crafting
		? Crafting->CanCraftRecipeAtStation(RecipeId, Knowledge, Skills, CachedInventory.Get(), StationActor)
		: FMOCraftingValidation();
	if (!Validation.bCanCraft)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingQueue] Validation rejected %s: %s"),
			*RecipeId.ToString(), *Validation.FailureReason.ToString());
		return false;
	}

	// Create queue entry
	FMOCraftingQueueEntry NewEntry;
	NewEntry.EntryId = FGuid::NewGuid();
	NewEntry.bRetainCompletionReceipt = bool(OnInstalled);
	NewEntry.RecipeId = RecipeId;
	NewEntry.Count = Count;
	NewEntry.CompletedCount = 0;
	NewEntry.Progress = 0.0f;
	const auto Context = Crafting->ResolveExecutionContext(RecipeId, OwnerActor, StationActor);
	NewEntry.Station = Context.StationType;
	NewEntry.StationGuid = Context.StationGuid;
	if (Context.StationGuid.IsValid()) KnownStations.Add(Context.StationGuid, StationActor);
	NewEntry.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	FMOInventoryMutationScope InventoryMutation(CachedInventory.Get());
	if (!ReserveIngredientsForCraft(NewEntry))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingQueue] Failed to reserve ingredients for recipe: %s"), *RecipeId.ToString());
		return false;
	}

	const int32 NewIndex = Queue.Entries.Add(NewEntry);
	Queue.MarkItemDirty(Queue.Entries[NewIndex]);
	OutEntryId = NewEntry.EntryId;
	if (OnInstalled) OnInstalled(NewEntry.EntryId);

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

void UMOCraftingQueueComponent::ServerRequestEnqueueCraft_Implementation(FName RecipeId, int32 Count, AActor* StationActor)
{
	// Normally executes on the host, where HasAuthority() is true, so EnqueueCraft runs
	// the real authoritative path (ingredient consume + queue mutation + replication).
	// The guard is load-bearing: a Server RPC resolves to LOCAL callspace when the
	// owner's world has no NetDriver (e.g. a pawn lingering in a torn-down world after
	// travel/disconnect). Without it, EnqueueCraft re-forwards here on !HasAuthority and
	// the pair recurses to EXCEPTION_STACK_OVERFLOW (crashed the editor 2026-07-04).
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOCraftingQueue] ServerRequestEnqueueCraft executed without authority (dead-world local callspace?) - dropped"));
		return;
	}
	EnqueueCraftAtStation(RecipeId, Count, StationActor);
}

void UMOCraftingQueueComponent::ServerRequestCancelCraft_Implementation(FGuid EntryId)
{
	// Server RPCs can resolve to local callspace after disconnect; never bounce back to the forwarding wrapper.
	if (GetOwner() && GetOwner()->HasAuthority()) CancelCraft(EntryId, true);
}

void UMOCraftingQueueComponent::ServerRequestCancelAllCrafts_Implementation()
{
	if (GetOwner() && GetOwner()->HasAuthority()) CancelAllCrafts(true);
}

bool UMOCraftingQueueComponent::CancelCraft(const FGuid& EntryId, bool bRefundIngredients)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		if (Pawn && Pawn->IsLocallyControlled())
		{
			ServerRequestCancelCraft(EntryId);
			return true;
		}
		return false;
	}

	const int32 Index = Queue.Entries.IndexOfByPredicate([&EntryId](const auto& Entry) { return Entry.EntryId == EntryId; });
	if (Index == INDEX_NONE) return false;
	FMOInventoryMutationScope Mutation(CachedInventory.Get());
	const bool bResumeNext = Index == 0 && (bIsCraftingActive || bAwaitingContext);
	if (Index == 0) PauseCrafting();
	const FMOCraftingQueueEntry Cancelled = Queue.Entries[Index];
	// Remove ownership before refund callbacks: cancelling this id again cannot refund it twice.
	Queue.Entries.RemoveAt(Index);
	Queue.MarkArrayDirty();
	if (bRefundIngredients && Cancelled.bIngredientsConsumed)
	{
		RetainCancelledIngredients(Cancelled);
	}
	DeliverPendingMaterials();
	if (bResumeNext && !Queue.Entries.IsEmpty()) StartCrafting();
	RefreshTickEnabled();
	OnCraftCancelled.Broadcast(Cancelled.EntryId, bRefundIngredients && Cancelled.bHasCapturedReservation);
	OnQueueChanged.Broadcast();
	return true;
}

void UMOCraftingQueueComponent::CancelAllCrafts(bool bRefundIngredients)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		if (Pawn && Pawn->IsLocallyControlled()) ServerRequestCancelAllCrafts();
		return;
	}

	if (Queue.Entries.IsEmpty()) return;
	FMOInventoryMutationScope Mutation(CachedInventory.Get());
	PauseCrafting();
	const TArray<FMOCraftingQueueEntry> Cancelled = MoveTemp(Queue.Entries);
	Queue.Entries.Reset();
	Queue.MarkArrayDirty();
	for (const auto& Entry : Cancelled)
	{
		if (bRefundIngredients && Entry.bIngredientsConsumed)
		{
			RetainCancelledIngredients(Entry);
		}
	}
	// Install ownership for ALL cancelled entries before the first callback can save.
	DeliverPendingMaterials();
	RefreshTickEnabled();
	for (const auto& Entry : Cancelled) OnCraftCancelled.Broadcast(Entry.EntryId, bRefundIngredients && Entry.bHasCapturedReservation);
	OnQueueChanged.Broadcast();
}

bool UMOCraftingQueueComponent::ReorderQueueEntry(const FGuid& EntryId, int32 NewIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
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
	if (CurrentIndex == 0 && (bIsCraftingActive || bAwaitingContext)) return false;

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
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	if (IsQueueEmpty())
	{
		return false;
	}
	DeliverPendingMaterials();
	if (IsQueueEmpty()) return false;

	FText Reason;
	if (!ValidateEntryContext(Queue.Entries[0], Reason))
	{
		WaitForContext(Reason);
		return false;
	}
	if (bIsCraftingActive) return true;

	bAwaitingContext = false;
	CraftingBlockReason = FText::GetEmpty();

	bIsCraftingActive = true;
	// (H25) Back-date the progress anchor by whatever the front entry has already
	// accumulated. ProcessCraftingTick derives Progress from
	// (Now - CurrentCraftStartTime)/Duration, so a bare UtcNow() anchor made the
	// next tick recompute ~0 and OVERWRITE the saved Progress — pause/resume and
	// every save/load silently wiped a partial craft (time/matter vanishing,
	// Principle 11). A fresh craft has Progress==0 and maps to UtcNow() unchanged;
	// a resumed or restored craft continues exactly where it left off.
	{
		const FMOCraftingQueueEntry& Front = Queue.Entries[0];
		const float FrontDuration = GetEffectiveCraftDuration(Front.RecipeId);
		const float ResumeProgress = FMath::Clamp(Front.Progress, 0.0f, 1.0f);
		CurrentCraftStartTime = FDateTime::UtcNow()
			- FTimespan::FromSeconds(static_cast<double>(ResumeProgress) * FrontDuration);
	}

	// Enable ticking
	SetComponentTickEnabled(true);

	// Subscribe to owner interrupts. Now severe damage / knockdown / death /
	// combat / unconsciousness can pause this craft via the centralized bus.
	// Movement is intentionally policy'd out in NotifyInterrupt — the player
	// can walk around the station while a craft progresses on real-time.
	RegisterWithOwnerForInterrupts();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Crafting started"));
	OnQueueChanged.Broadcast();

	return true;
}

void UMOCraftingQueueComponent::PauseCrafting()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const bool bWasActive = bIsCraftingActive;
	bAwaitingContext = false;
	CraftingBlockReason = FText::GetEmpty();
	if (!Queue.Entries.IsEmpty() && bIsCraftingActive)
	{
		Queue.Entries[0].Progress = CalculateProgressFromTime(Queue.Entries[0], GetEffectiveCraftDuration(Queue.Entries[0].RecipeId));
		Queue.MarkItemDirty(Queue.Entries[0]);
	}

	// Pending material delivery continues even when timed work is paused.
	bIsCraftingActive = false;
	RefreshTickEnabled();

	// Crafting is no longer actively progressing; drop the interrupt
	// subscription. StartCrafting() will re-register on resume.
	UnregisterFromOwnerInterrupts();

	if (bWasActive) UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Crafting paused"));
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

AActor* UMOCraftingQueueComponent::ResolveEntryStation(const FMOCraftingQueueEntry& Entry) const
{
	if (!Entry.StationGuid.IsValid()) return nullptr;
	if (const auto* Known = KnownStations.Find(Entry.StationGuid); Known && Known->IsValid()) return Known->Get();
	if (UWorld* World = GetWorld())
	{
		if (const auto* Registry = World->GetSubsystem<UMOIdentityRegistrySubsystem>()) return Registry->ResolveActorOrNull(Entry.StationGuid);
	}
	return nullptr;
}

bool UMOCraftingQueueComponent::ValidateEntryContext(const FMOCraftingQueueEntry& Entry, FText& OutReason) const
{
	if (!PendingProducts.IsEmpty())
	{
		OutReason = FText::FromString(TEXT("Finished goods await inventory room or a compatible liquid vessel."));
		return false;
	}
	if (Entry.Count <= 0 || Entry.CompletedCount < 0 || Entry.CompletedCount >= Entry.Count || !Entry.bIngredientsConsumed)
	{
		OutReason = FText::FromString(TEXT("Saved craft has invalid counts or unpaid ingredients. Materials are retained."));
		return false;
	}
	if (Entry.bHasCapturedReservation)
	{
		auto Check = Entry.ReservedIngredients;
		if (!MOIngredientPlans::Consume(Check, Entry.IngredientsPerCraft).IsComplete())
		{
			OutReason = FText::FromString(TEXT("Reserved ingredients are incomplete. Materials are retained."));
			return false;
		}
	}
	const auto* Crafting = CachedCraftingSubsystem.Get();
	if (!Crafting && GetWorld()) Crafting = GetWorld()->GetSubsystem<UMOCraftingSubsystem>();
	if (!Crafting)
	{
		OutReason = FText::FromString(TEXT("Crafting service is unavailable."));
		return false;
	}
	const auto Context = Crafting->ResolveExecutionContext(Entry.RecipeId, GetOwner(), ResolveEntryStation(Entry));
	if (!Context.IsUsable())
	{
		OutReason = Context.FailureReason;
		return false;
	}
	const auto* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(Entry.RecipeId);
	if (!Crafting->HasRequiredTools(Recipe, CachedInventory.Get()))
	{
		OutReason = FText::FromString(TEXT("Required crafting tools are missing."));
		return false;
	}
	return true;
}

void UMOCraftingQueueComponent::WaitForContext(const FText& Reason)
{
	const bool bChanged = !bAwaitingContext || !CraftingBlockReason.EqualTo(Reason);
	PauseCrafting();
	bAwaitingContext = true;
	CraftingBlockReason = Reason;
	SetComponentTickEnabled(true);
	if (bChanged) OnQueueChanged.Broadcast();
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
		if (StationGuid.IsValid()) KnownStations.Add(StationGuid, Station);
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
	// The station the pawn last opened is "active" only while the pawn is at it. Nothing cleared it when the pawn walked off, so the crafting menu
	// showed a campfire and its fuel from a mile away (Wes, Oct 18 2026); the crafting service refuses beyond the same reach anyway.
	AActor* Station = ActiveStation.Get();
	const AActor* Owner = GetOwner();
	if (Station && Owner && FVector::DistSquared(Owner->GetActorLocation(), Station->GetActorLocation()) > FMath::Square(UMOCraftingSubsystem::StationReachCm))
	{
		return nullptr;
	}
	return Station;
}

// =============================================================================
// Save/Load
// =============================================================================

void UMOCraftingQueueComponent::BuildSaveData(FMOCraftingQueueSaveData& OutSaveData) const
{
	OutSaveData.QueuedCrafts = Queue.Entries;
	OutSaveData.CompletionReceipts = CompletionReceipts;
	if (bIsCraftingActive && !OutSaveData.QueuedCrafts.IsEmpty())
	{
		auto& Front = OutSaveData.QueuedCrafts[0];
		Front.Progress = CalculateProgressFromTime(Front, GetEffectiveCraftDuration(Front.RecipeId));
	}
	OutSaveData.PendingReturns = PendingReturns;
	OutSaveData.PendingProducts = PendingProducts;
	OutSaveData.UnresolvedLegacyRefunds = UnresolvedLegacyRefunds;
	OutSaveData.PausedAt = FDateTime::UtcNow();
	OutSaveData.bWasActive = bIsCraftingActive;
	OutSaveData.bAwaitingContext = bAwaitingContext;
	OutSaveData.ActiveStationGuid = GetActiveStationGuid();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Built save data: %d entries, active: %s, station: %s"),
		OutSaveData.QueuedCrafts.Num(),
		OutSaveData.bWasActive ? TEXT("yes") : TEXT("no"),
		OutSaveData.ActiveStationGuid.IsValid() ? *OutSaveData.ActiveStationGuid.ToString(EGuidFormats::DigitsWithHyphens) : TEXT("none"));
}

bool UMOCraftingQueueComponent::ApplySaveData(const FMOCraftingQueueSaveData& InSaveData, bool bCalculateOfflineProgress)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
	PauseCrafting();
	CacheComponents();
	KnownStations.Reset();
	// Clear current queue
	Queue.Entries = InSaveData.QueuedCrafts;
	CompletionReceipts = InSaveData.CompletionReceipts;
	PendingReturns = InSaveData.PendingReturns;
	PendingProducts = InSaveData.PendingProducts;
	UnresolvedLegacyRefunds = InSaveData.UnresolvedLegacyRefunds;
	for (auto& Entry : Queue.Entries)
	{
		// Legacy saves had one station binding for the whole queue. Preserve that best available identity, never invent a station from its enum.
		if (!Entry.StationGuid.IsValid() && Entry.Station != EMOCraftingStation::None) Entry.StationGuid = InSaveData.ActiveStationGuid;
		Queue.MarkItemDirty(Entry);
	}
	Queue.MarkArrayDirty();
	DeliverPendingMaterials();

	FText ContextReason;
	const bool bContextValid = !Queue.Entries.IsEmpty() && ValidateEntryContext(Queue.Entries[0], ContextReason);
	if (bCalculateOfflineProgress && InSaveData.bWasActive && bContextValid)
	{
		// Calculate time elapsed since save
		FDateTime Now = FDateTime::UtcNow();
		FTimespan Elapsed = Now - InSaveData.PausedAt;
		float ElapsedSeconds = static_cast<float>(Elapsed.GetTotalSeconds());

		// Cap offline progress at the SAME bound the game clock uses for its
		// offline advance (default 4h) — was 7 days, which let a queue bank a
		// year of work while the rest of the sim stayed frozen (Principle 11 /
		// H36). Queued crafts advance up to the cap; nothing runs a year ahead.
		float MaxOfflineSeconds = 4.0f * 60.0f * 60.0f;
		if (const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this))
		{
			MaxOfflineSeconds = static_cast<float>(Clock->GetOfflineAdvanceMaxHours() * 3600.0);
		}
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

	if (!Queue.Entries.IsEmpty() && (InSaveData.bWasActive || InSaveData.bAwaitingContext))
	{
		if (bContextValid) StartCrafting();
		else WaitForContext(ContextReason);
	}
	RefreshTickEnabled();
	OnQueueChanged.Broadcast();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingQueue] Applied save data: %d entries remaining"),
		Queue.Entries.Num());

	return true;
}

void UMOCraftingQueueComponent::ClearQueue()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	Queue.Entries.Empty();
	Queue.MarkArrayDirty();
	bIsCraftingActive = false;
	bAwaitingContext = false;
	CraftingBlockReason = FText::GetEmpty();
	UnregisterFromOwnerInterrupts();
	RefreshTickEnabled();
	OnQueueChanged.Broadcast();
}

// =============================================================================
// Internal Methods
// =============================================================================

void UMOCraftingQueueComponent::ProcessCraftingTick(float DeltaTime)
{
	// Tick is only enabled by StartCrafting (authority-gated), but guard the body
	// directly — progress mutation is the one thing the queue must never do on a client.
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (Queue.Entries.Num() == 0)
	{
		PauseCrafting();
		return;
	}

	if (AccumulatedDeltaTime + DeltaTime >= ProgressUpdateInterval)
	{
		FText Reason;
		if (!ValidateEntryContext(Queue.Entries[0], Reason))
		{
			WaitForContext(Reason);
			return;
		}
	}

	FMOCraftingQueueEntry& CurrentEntry = Queue.Entries[0];
	const FGuid CurrentEntryId = CurrentEntry.EntryId;
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
		// Progress observers may cancel/reorder/pause. Elapsed time belongs only to this entry.
		if (bIsCraftingActive && !Queue.Entries.IsEmpty() && Queue.Entries[0].EntryId == CurrentEntryId) CompletCurrentCraft();
	}
}

bool UMOCraftingQueueComponent::CompletCurrentCraft()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Queue.Entries.IsEmpty() || bProcessingCompletion) return false;
	TGuardValue<bool> Completing(bProcessingCompletion, true);
	FText Reason;
	if (!ValidateEntryContext(Queue.Entries[0], Reason))
	{
		WaitForContext(Reason);
		return false;
	}
	auto* Crafting = CachedCraftingSubsystem.Get();
	auto* Inventory = CachedInventory.Get();
	if (!Crafting || !Inventory) return false;
	const FGuid EntryId = Queue.Entries[0].EntryId;
	const FName RecipeId = Queue.Entries[0].RecipeId;
	FMOIngredientReservation Products;
	FMOCraftResult Result;
	if (!Crafting->PrepareCraftOutputs(RecipeId, Products, Result))
	{
		WaitForContext(FText::FromString(TEXT("Recipe outputs cannot be resolved. Reserved materials are retained.")));
		return false;
	}

	// No observer runs until inputs, completed count and ALL outputs have durable owners.
	// A callback that cancels or saves can never refund this completed repeat or lose an output.
	FMOInventoryMutationScope Mutation(Inventory);
	auto& Entry = Queue.Entries[0];
	if (Entry.bHasCapturedReservation)
	{
		const auto Consumed = MOIngredientPlans::Consume(Entry.ReservedIngredients, Entry.IngredientsPerCraft);
		if (!Consumed.IsComplete())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingQueue] Reservation failure %d for %s (%s)"),
				int32(Consumed.Failure), *RecipeId.ToString(), *Consumed.IngredientId.ToString());
			WaitForContext(FText::FromString(TEXT("Reserved ingredients are incomplete. Work is suspended without producing goods.")));
			return false;
		}
	}
	else if (!Entry.bIngredientsConsumed)
	{
		WaitForContext(FText::FromString(TEXT("Saved craft has no paid ingredients. Cancel and enqueue it again.")));
		return false;
	}
	// Legacy entries already paid at enqueue, but lack provenance. They may finish their
	// original timed work; cancellation retains a diagnostic record instead of inventing a refund.
	Entry.CompletedCount++;
	Entry.Progress = 0.0f;
	if (Entry.bRetainCompletionReceipt)
	{
		FMOCraftingCompletionReceipt Receipt;
		Receipt.EntryId = EntryId;
		Receipt.ProducedItems = Result.ProducedItems;
		CompletionReceipts.Add(MoveTemp(Receipt));
	}
	PendingProducts.Lots.Append(MoveTemp(Products.Lots));
	if (Entry.CompletedCount >= Entry.Count)
	{
		// Float-tolerance remnants still exist. Return them instead of destroying them.
		PendingReturns.Lots.Append(MoveTemp(Entry.ReservedIngredients.Lots));
		Queue.Entries.RemoveAt(0);
		Queue.MarkArrayDirty();
	}
	else Queue.MarkItemDirty(Entry);
	CurrentCraftStartTime = FDateTime::UtcNow();
	DeliverPendingMaterials();
	for (const auto& Lot : PendingProducts.Lots)
	{
		if (!Result.ProducedItems.Contains(Lot.IngredientId)) continue;
		const int32 Units = Lot.IsLiquid() ? FMath::CeilToInt(Lot.State.Liquid.Millilitres / Lot.MeasureMl) : Lot.Quantity;
		Result.PendingDeliveryItems.FindOrAdd(Lot.IngredientId) += Units;
	}
	// Pending may include older output of the same type; this result reports only this craft.
	for (auto& Pair : Result.PendingDeliveryItems) Pair.Value = FMath::Min(Pair.Value, Result.ProducedItems.FindRef(Pair.Key));
	if (Queue.Entries.IsEmpty()) PauseCrafting();
	else
	{
		FText NextReason;
		if (!ValidateEntryContext(Queue.Entries[0], NextReason)) WaitForContext(NextReason);
	}
	RefreshTickEnabled();
	Crafting->FinalizeReservedCraft(RecipeId, Inventory, GetOwner()->FindComponentByClass<UMOSkillsComponent>(), Result);
	OnCraftCompleted.Broadcast(EntryId, Result);
	OnQueueChanged.Broadcast();
	return true;
}

bool UMOCraftingQueueComponent::GetCompletionReceipt(const FGuid& EntryId, FMOCraftingCompletionReceipt& OutReceipt) const
{
	const auto* Found = CompletionReceipts.FindByPredicate([&](const auto& Receipt) { return Receipt.EntryId == EntryId; });
	if (!Found) return false;
	OutReceipt = *Found;
	return true;
}

void UMOCraftingQueueComponent::AcknowledgeCompletionReceipt(const FGuid& EntryId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	CompletionReceipts.RemoveAll([&](const auto& Receipt) { return Receipt.EntryId == EntryId; });
}

bool UMOCraftingQueueComponent::ReserveIngredientsForCraft(FMOCraftingQueueEntry& Entry)
{
	auto* Inventory = CachedInventory.Get();
	const auto* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(Entry.RecipeId);
	if (!Inventory || !Recipe || Entry.Count <= 0) return false;
	TMap<FName, int32> Totals;
	for (const auto& Ingredient : Recipe->Ingredients)
	{
		if (Ingredient.ItemDefinitionId.IsNone() || Ingredient.Quantity <= 0) return false;
		const int64 PerCraft = int64(Entry.IngredientsPerCraft.FindRef(Ingredient.ItemDefinitionId)) + Ingredient.Quantity;
		if (PerCraft > MAX_int32 || PerCraft * Entry.Count > MAX_int32) return false;
		Entry.IngredientsPerCraft.Add(Ingredient.ItemDefinitionId, int32(PerCraft));
		Totals.Add(Ingredient.ItemDefinitionId, int32(PerCraft * Entry.Count));
	}
	const auto Result = Inventory->ReserveIngredients(Totals, Entry.ReservedIngredients);
	if (!Result.IsComplete())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingQueue] Reserve refused %s: failure=%d ingredient=%s missing=%d"),
			*Entry.RecipeId.ToString(), int32(Result.Failure), *Result.IngredientId.ToString(), Result.MissingUnits);
		return false;
	}
	Entry.bHasCapturedReservation = true;
	Entry.bIngredientsConsumed = true;
	return true;
}

bool UMOCraftingQueueComponent::RetainCancelledIngredients(const FMOCraftingQueueEntry& Entry)
{
	if (Entry.bHasCapturedReservation)
	{
		PendingReturns.Lots.Append(Entry.ReservedIngredients.Lots);
		return true;
	}
	if (Entry.Count > Entry.CompletedCount)
	{
		UnresolvedLegacyRefunds.Add(Entry);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingQueue] Legacy refund for %s retained unresolved: original item/liquid state was not saved"), *Entry.RecipeId.ToString());
	}
	return false;
}

void UMOCraftingQueueComponent::RefreshTickEnabled()
{
	const int32 Lots = PendingReturns.Lots.Num() + PendingProducts.Lots.Num();
	const int32 Legacy = UnresolvedLegacyRefunds.Num();
	const bool bChanged = Lots != PendingMaterialLotCount || Legacy != UnresolvedLegacyRefundCount;
	PendingMaterialLotCount = Lots;
	UnresolvedLegacyRefundCount = Legacy;
	SetComponentTickEnabled(bIsCraftingActive || bAwaitingContext || !PendingReturns.IsEmpty() || !PendingProducts.IsEmpty());
	if (bChanged) OnQueueChanged.Broadcast();
}

void UMOCraftingQueueComponent::OnRep_MaterialStatus()
{
	OnQueueChanged.Broadcast();
}

void UMOCraftingQueueComponent::DeliverPendingMaterials()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bDeliveringMaterials) return;
	if (!CachedInventory.IsValid()) CacheComponents();
	auto* Inventory = CachedInventory.Get();
	if (!Inventory) return;
	TGuardValue<bool> Delivering(bDeliveringMaterials, true);
	FMOInventoryMutationScope Mutation(Inventory);
	// Each adapter updates the persistent remainder BEFORE inventory callbacks flush.
	if (!PendingReturns.IsEmpty()) Inventory->ReturnReservedIngredients(PendingReturns);
	if (!PendingProducts.IsEmpty()) Inventory->ReturnReservedIngredients(PendingProducts);
	RefreshTickEnabled();
}

float UMOCraftingQueueComponent::CalculateProgressFromTime(const FMOCraftingQueueEntry& Entry, float CraftDuration) const
{
	if (!FMath::IsFinite(CraftDuration) || CraftDuration <= 0.0f)
	{
		return FMath::IsFinite(Entry.Progress) ? FMath::Clamp(Entry.Progress, 0.0f, 1.0f) : 0.0f;
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
		const float Adjusted = CraftingSubsystem->GetAdjustedCraftTime(RecipeId, Inventory);
		return FMath::IsFinite(Adjusted) ? FMath::Max(0.1f, Adjusted) : FMath::Max(0.1f, Recipe->CraftTime);
	}

	// Fallback to base duration if subsystem or inventory unavailable
	return Recipe->CraftTime;
}

void UMOCraftingQueueComponent::AdvanceQueueByTime(float ElapsedSeconds)
{
	while (ElapsedSeconds > 0.0f && Queue.Entries.Num() > 0)
	{
		FText Reason;
		if (!ValidateEntryContext(Queue.Entries[0], Reason))
		{
			WaitForContext(Reason);
			break;
		}
		FMOCraftingQueueEntry& Entry = Queue.Entries[0];
		float CraftDuration = GetEffectiveCraftDuration(Entry.RecipeId);

		if (CraftDuration <= 0.0f)
		{
			// Instant craft
			if (!CompletCurrentCraft()) break;
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
			if (!CompletCurrentCraft()) break;
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
