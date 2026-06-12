#include "MOGhostContextMenu.h"
#include "MOBuildableActor.h"
#include "MOBuildProgressComponent.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOWorldItem.h"
#include "MOItemComponent.h"
#include "MOContainerActor.h"
#include "MOBuildingTypes.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOUIUtils.h"
#include "MOMaterialSourceInterface.h"
#include "Components/CheckBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"

const FName UMOGhostContextMenu::BuildingSkillId = FName("Construction");

UMOGhostContextMenu::UMOGhostContextMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Match the base default — close on mouse leave AND click-outside.
	// ShouldCloseOnMouseLeave() still guards against closing while a build
	// timer is active, so a player who walks away mid-build doesn't lose
	// their queued construction.
	bCloseOnMouseLeave = true;
}

void UMOGhostContextMenu::RequestClose()
{
	// Broadcast legacy delegate for backward compatibility
	OnRequestClose.Broadcast();
	// Also call base which broadcasts OnCloseRequested
	Super::RequestClose();
}

bool UMOGhostContextMenu::ShouldCloseOnMouseLeave() const
{
	// Don't close on mouse leave while build timer is active
	return bCloseOnMouseLeave && !IsBuildTimerActive();
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UMOGhostContextMenu::InitializeForGhost(AMOBuildableActor* Target, UMOInventoryComponent* InBuilderInventory)
{
	if (!IsValid(Target))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] InitializeForGhost called with invalid target"));
		return;
	}

	// If we're re-initialising for a different target, drop the previous
	// subscription before we point BuildProgress at a new component.
	UnbindBuildProgressEvents();

	TargetBuilding = Target;
	BuilderInventory = InBuilderInventory;
	BuildProgress = Target->BuildProgressComponent;

	// Subscribe to the new BuildProgress component's state-change broadcast
	// so this menu closes itself when construction transitions away from
	// Constructing (movement interrupt, completion, external cancel).
	BindBuildProgressEvents();

	// Get skills component from the builder's owner
	if (InBuilderInventory)
	{
		AActor* BuilderOwner = InBuilderInventory->GetOwner();
		if (BuilderOwner)
		{
			BuilderSkills = BuilderOwner->FindComponentByClass<UMOSkillsComponent>();
		}
	}

	// Clear any previous deposited materials tracking
	DepositedMaterialsForRefund.Empty();

	// Initialize checkboxes to default state
	if (InventoryCheckbox)
	{
		InventoryCheckbox->SetIsChecked(true);
	}
	if (ContainersCheckbox)
	{
		ContainersCheckbox->SetIsChecked(true);
	}
	if (SurroundingCheckbox)
	{
		SurroundingCheckbox->SetIsChecked(true);
	}

	// Refresh the material list
	RefreshMaterialList();
	UpdateButtonState();

	// Reflect preserved build progress immediately. If this is a re-open on a
	// Paused build, the bar should show the saved % the moment the menu opens
	// — not 0% until the player clicks Build to resume. UpdateButtonState
	// already calls RefreshBuildProgressDisplay, but call it explicitly here
	// too so the contract ("InitializeForGhost shows current state") is
	// obvious without tracing through the helper.
	RefreshBuildProgressDisplay();

	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Initialized for ghost: %s"), *Target->GetName());
}

void UMOGhostContextMenu::RefreshMaterialList()
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	if (!Progress)
	{
		return;
	}

	// Get required materials
	TArray<FName> RequiredItemIds;
	Progress->GetRequiredMaterials(RequiredItemIds);

	// Build material entries
	MaterialEntries.Empty();
	for (const FName& ItemId : RequiredItemIds)
	{
		FMOGhostMaterialEntry Entry;
		Entry.ItemId = ItemId;
		Entry.Deposited = Progress->GetDepositedCount(ItemId);
		Entry.Required = Progress->GetRequiredCount(ItemId);

		// Get display name from item database
		FMOItemDefinitionRow ItemDef;
		if (UMOItemDatabaseSettings::GetItemDefinition(ItemId, ItemDef))
		{
			Entry.DisplayName = ItemDef.DisplayName;
		}
		else
		{
			Entry.DisplayName = FText::FromName(ItemId);
		}

		MaterialEntries.Add(Entry);
	}

	// Update UI
	PopulateMaterialList();
	OnMaterialListUpdated(MaterialEntries);
	UpdateButtonState();
}

// ============================================================================
// ACTIONS
// ============================================================================

void UMOGhostContextMenu::AddMaterials()
{
	if (!BuildProgress.IsValid())
	{
		return;
	}

	// Try to gather materials for each requirement that isn't complete
	bool bGatheredAny = false;
	for (FMOGhostMaterialEntry& Entry : MaterialEntries)
	{
		while (!Entry.IsComplete())
		{
			if (TryGatherMaterial(Entry.ItemId))
			{
				Entry.Deposited++;
				bGatheredAny = true;
			}
			else
			{
				// Couldn't find more of this material
				break;
			}
		}
	}

	if (bGatheredAny)
	{
		RefreshMaterialList();
	}
}

void UMOGhostContextMenu::StartBuild()
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	if (!Progress)
	{
		return;
	}

	if (!AreAllMaterialsDeposited())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] Cannot start build - materials not deposited"));
		return;
	}

	// Build options from checkbox states
	FMOBuildProgress Options;
	Options.bDrawFromInventory = InventoryCheckbox ? InventoryCheckbox->IsChecked() : true;
	Options.bDrawFromNearbyContainers = ContainersCheckbox ? ContainersCheckbox->IsChecked() : true;
	Options.bDrawFromSurroundingArea = SurroundingCheckbox ? SurroundingCheckbox->IsChecked() : true;

	// Start construction
	Progress->StartConstruction(Options, BuilderInventory.Get());

	OnBuildStarted.Broadcast();

	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Build started!"));
}

void UMOGhostContextMenu::CancelBuild()
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	AMOBuildableActor* Building = TargetBuilding.Get();

	if (Progress && Building)
	{
		// Determine if build has started (construction in progress)
		const bool bBuildStarted = (Progress->GetState() == EMOBuildState::Constructing ||
			Progress->GetState() == EMOBuildState::Paused);

		// Rebuild the refund list from the COMPONENT's ledger — the single
		// owner. The widget-local list emptied on every menu reopen
		// (InitializeForGhost), so cancel-after-reopen refunded nothing and
		// destroyed every deposited material.
		DepositedMaterialsForRefund.Empty();
		TMap<FName, int32> ComponentLedger;
		Progress->GetDepositedMaterials(ComponentLedger);
		for (const auto& Pair : ComponentLedger)
		{
			FMODepositedMaterial DepositedMat;
			DepositedMat.ItemId = Pair.Key;
			FMOItemDefinitionRow ItemDef;
			if (UMOItemDatabaseSettings::GetItemDefinition(Pair.Key, ItemDef))
			{
				DepositedMat.Rarity = ItemDef.Rarity;
			}
			for (int32 i = 0; i < Pair.Value; ++i)
			{
				DepositedMaterialsForRefund.Add(DepositedMat);
			}
		}

		// Calculate refund based on build state and skill
		const int32 TotalDeposited = DepositedMaterialsForRefund.Num();
		const int32 RefundCount = CalculateRefundAmount(TotalDeposited, bBuildStarted);

		UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] CancelBuild: TotalDeposited=%d, RefundCount=%d, bBuildStarted=%s"),
			TotalDeposited, RefundCount, bBuildStarted ? TEXT("yes") : TEXT("no"));

		// Return refunded materials to builder's inventory (fallback to world drop if no inventory)
		RefundMaterialsToInventory(RefundCount);

		UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Cancel refund: %d/%d materials returned (build started: %s)"),
			RefundCount, TotalDeposited, bBuildStarted ? TEXT("yes") : TEXT("no"));

		// Cancel the construction
		Progress->CancelConstruction(false); // Materials already handled

		// Destroy the ghost building
		Building->Destroy();
	}

	// Clear tracking
	DepositedMaterialsForRefund.Empty();

	OnCancelled.Broadcast();
	RequestClose();
}

// ============================================================================
// STATE
// ============================================================================

bool UMOGhostContextMenu::AreAllMaterialsDeposited() const
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	return Progress ? Progress->AreAllMaterialsDeposited() : false;
}

bool UMOGhostContextMenu::IsBuildTimerActive() const
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	if (!Progress)
	{
		return false;
	}
	return Progress->GetState() == EMOBuildState::Constructing;
}

float UMOGhostContextMenu::GetBuildProgress() const
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	return Progress ? Progress->GetProgress() : 0.0f;
}

void UMOGhostContextMenu::GetMaterialEntries(TArray<FMOGhostMaterialEntry>& OutEntries) const
{
	OutEntries = MaterialEntries;
}

// ============================================================================
// OVERRIDES
// ============================================================================

void UMOGhostContextMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button handlers
	if (AddMaterialsButton)
	{
		AddMaterialsButton->OnClicked().RemoveAll(this);
		AddMaterialsButton->OnClicked().AddUObject(this, &UMOGhostContextMenu::HandleAddMaterialsClicked);
	}
	if (BuildButton)
	{
		BuildButton->OnClicked().RemoveAll(this);
		BuildButton->OnClicked().AddUObject(this, &UMOGhostContextMenu::HandleBuildClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOGhostContextMenu::HandleCancelClicked);
	}
}

void UMOGhostContextMenu::NativeDestruct()
{
	// Clean up button bindings
	if (AddMaterialsButton)
	{
		AddMaterialsButton->OnClicked().RemoveAll(this);
	}
	if (BuildButton)
	{
		BuildButton->OnClicked().RemoveAll(this);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
	}

	// Stop listening to BuildProgress state — otherwise we'd hold a dynamic
	// binding to a freed widget if the component outlives us.
	UnbindBuildProgressEvents();

	Super::NativeDestruct();
}

void UMOGhostContextMenu::BindBuildProgressEvents()
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	if (!IsValid(Progress))
	{
		return;
	}

	// RemoveDynamic-before-AddDynamic protects against re-init for the same target
	// (no leaked duplicate bindings).
	Progress->OnConstructionStateChanged.RemoveDynamic(this, &UMOGhostContextMenu::HandleBuildStateChanged);
	Progress->OnConstructionStateChanged.AddDynamic(this, &UMOGhostContextMenu::HandleBuildStateChanged);
}

void UMOGhostContextMenu::UnbindBuildProgressEvents()
{
	if (UMOBuildProgressComponent* Progress = BuildProgress.Get())
	{
		Progress->OnConstructionStateChanged.RemoveDynamic(this, &UMOGhostContextMenu::HandleBuildStateChanged);
	}
}

void UMOGhostContextMenu::HandleBuildStateChanged(EMOBuildState NewState)
{
	// Close the menu whenever construction leaves the actively-building state.
	// Paused = something interrupted us (most commonly the movement interrupt).
	// Complete = build finished; menu has nothing left to do.
	// Ghost = build was cancelled with refund.
	//
	// Constructing is the only state where the menu is genuinely useful, so
	// we use a positive test: close on anything else, but only when leaving
	// Constructing (avoids closing on the initial Ghost->Constructing
	// transition which fires during normal startup).
	if (NewState == EMOBuildState::Constructing)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOGhostContextMenu] Build state changed to %d (non-Constructing) — closing menu"),
		(int32)NewState);

	// Route through the standard close path so the UI controller's
	// HandleGhostContextMenuRequestClose runs (which tears down the modal
	// background, restores input mode, etc.).
	RequestClose();
}

void UMOGhostContextMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	// Base class handles mouse leave timer
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Check for build completion
	if (IsBuildComplete())
	{
		// Build finished - close and update ghost visual
		RequestClose();
		return;
	}

	// Live progress animation only matters while actively constructing.
	// (Paused-state display is set once on menu open and doesn't move.)
	if (IsBuildTimerActive())
	{
		RefreshBuildProgressDisplay();
	}
}

// NativeOnKeyDown, NativeOnMouseEnter, NativeOnMouseLeave, SetPopupPosition
// are now inherited from UMOContextMenuBase

bool UMOGhostContextMenu::IsBuildComplete() const
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	return Progress && Progress->GetState() == EMOBuildState::Complete;
}

// ============================================================================
// HANDLERS
// ============================================================================

void UMOGhostContextMenu::HandleAddMaterialsClicked()
{
	AddMaterials();
}

void UMOGhostContextMenu::HandleBuildClicked()
{
	if (AreAllMaterialsDeposited())
	{
		StartBuild();
	}
}

void UMOGhostContextMenu::HandleCancelClicked()
{
	CancelBuild();
}

// ============================================================================
// INTERNAL
// ============================================================================

void UMOGhostContextMenu::RefreshBuildProgressDisplay()
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	if (!Progress)
	{
		// No component → nothing to display. Clear so stale data from a
		// previous building doesn't leak through when this menu instance is
		// reused.
		if (BuildProgressBar)
		{
			BuildProgressBar->SetPercent(0.0f);
		}
		if (BuildTimeText)
		{
			BuildTimeText->SetText(UMOUIUtils::FormatDurationAsTimeCode(0.0f));
		}
		return;
	}

	// GetProgress() returns the elapsed fraction (0..1) regardless of state.
	// GetTimeRemaining() returns wall-clock seconds left at the current
	// elapsed time. Both are valid in Constructing AND Paused states — the
	// only difference is that in Paused they don't change without a resume.
	//
	// This is the fix for "menu re-opened on a paused build shows 0%":
	// previously the menu zeroed the bar in any non-Constructing state.
	// Now it just displays whatever the component has — the component is
	// the source of truth, the menu is just a view.
	const float ProgressVal = Progress->GetProgress();
	const float TimeRemaining = Progress->GetTimeRemaining();

	if (BuildProgressBar)
	{
		BuildProgressBar->SetPercent(ProgressVal);
	}
	if (BuildTimeText)
	{
		BuildTimeText->SetText(UMOUIUtils::FormatDurationAsTimeCode(TimeRemaining));
	}

	// Blueprint hook — kept here so subclasses still get notified once per
	// tick during construction (was the only call site previously).
	OnBuildProgressUpdated(ProgressVal, TimeRemaining);
}

void UMOGhostContextMenu::UpdateButtonState()
{
	const bool bAllMaterialsReady = AreAllMaterialsDeposited();
	const bool bBuildActive = IsBuildTimerActive();

	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] UpdateButtonState: AllMaterials=%d, BuildActive=%d, HasProgressBar=%d, HasTimeText=%d"),
		bAllMaterialsReady ? 1 : 0, bBuildActive ? 1 : 0,
		BuildProgressBar != nullptr ? 1 : 0, BuildTimeText != nullptr ? 1 : 0);

	// Build button - enabled only when all materials deposited and not already building
	if (BuildButton)
	{
		BuildButton->SetIsEnabled(bAllMaterialsReady && !bBuildActive);
	}

	// Add Materials button - hidden when all materials deposited or when building
	if (AddMaterialsButton)
	{
		AddMaterialsButton->SetVisibility((bAllMaterialsReady || bBuildActive)
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	}

	// Progress bar and time text are always visible. Push the latest value
	// from the BuildProgressComponent so Paused state displays the preserved
	// progress (not a stale 0%) when the menu is re-opened after a movement
	// interrupt. RefreshBuildProgressDisplay handles all states correctly.
	if (BuildProgressBar)
	{
		BuildProgressBar->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] BuildProgressBar not bound! Add a ProgressBar named 'BuildProgressBar' to the widget."));
	}
	RefreshBuildProgressDisplay();

	if (BuildTimeText)
	{
		BuildTimeText->SetVisibility(ESlateVisibility::Visible);
		if (!bBuildActive)
		{
			// Show total build time before starting
			UMOBuildProgressComponent* Progress = BuildProgress.Get();
			if (Progress)
			{
				float TotalTime = Progress->GetProgressData().TotalBuildTime;
				BuildTimeText->SetText(UMOUIUtils::FormatDurationAsTimeCode(TotalTime));
			}
			else
			{
				BuildTimeText->SetText(FText::FromString(TEXT("--:--")));
			}
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] BuildTimeText not bound! Add a TextBlock named 'BuildTimeText' to the widget."));
	}

	// Blueprint callback
	OnButtonStateChanged(bAllMaterialsReady);
}

UTextBlock* UMOGhostContextMenu::CreateMaterialTextWidget(const FMOGhostMaterialEntry& Entry)
{
	UTextBlock* TextWidget = NewObject<UTextBlock>(this);
	if (TextWidget)
	{
		UpdateMaterialTextWidget(TextWidget, Entry);
	}
	return TextWidget;
}

void UMOGhostContextMenu::UpdateMaterialTextWidget(UTextBlock* Widget, const FMOGhostMaterialEntry& Entry)
{
	if (!Widget)
	{
		return;
	}

	// Format: "Material Name 3/5"
	FString Text = FString::Printf(TEXT("%s %d/%d"),
		*Entry.DisplayName.ToString(),
		Entry.Deposited,
		Entry.Required);

	Widget->SetText(FText::FromString(Text));

	// Green if complete, default otherwise
	if (Entry.IsComplete())
	{
		Widget->SetColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.2f, 1.0f));
	}
	else
	{
		Widget->SetColorAndOpacity(FLinearColor::White);
	}
}

bool UMOGhostContextMenu::TryGatherMaterial(FName ItemId)
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	if (!Progress)
	{
		return false;
	}

	// Check if we still need this material
	int32 Required = Progress->GetRequiredCount(ItemId);
	int32 Deposited = Progress->GetDepositedCount(ItemId);
	if (Deposited >= Required)
	{
		return false;
	}

	// Get gather range from progress
	float GatherRange = Progress->GetProgressData().GatherRange;
	if (GatherRange <= 0.0f)
	{
		GatherRange = 500.0f; // Default range
	}

	// Get all material sources and sort by priority
	TArray<AActor*> Sources = Progress->FindMaterialSources(GatherRange);

	// Add builder (player) as a potential source if checkbox is checked
	if (InventoryCheckbox && InventoryCheckbox->IsChecked())
	{
		UMOInventoryComponent* Inventory = BuilderInventory.Get();
		if (Inventory)
		{
			AActor* OwnerActor = Inventory->GetOwner();
			if (OwnerActor && !Sources.Contains(OwnerActor))
			{
				Sources.Add(OwnerActor);
			}
		}
	}

	// Sort by priority (highest first) using interface
	Sources.Sort([](AActor& A, AActor& B) {
		int32 PriorityA = A.Implements<UMOMaterialSourceInterface>()
			? IMOMaterialSourceInterface::Execute_GetMaterialSourcePriority(&A) : 0;
		int32 PriorityB = B.Implements<UMOMaterialSourceInterface>()
			? IMOMaterialSourceInterface::Execute_GetMaterialSourcePriority(&B) : 0;
		return PriorityA > PriorityB;
	});

	// Filter sources based on checkbox states
	for (AActor* Source : Sources)
	{
		if (!Source->Implements<UMOMaterialSourceInterface>())
		{
			continue;
		}

		// Determine source type and check if enabled
		int32 SourcePriority = IMOMaterialSourceInterface::Execute_GetMaterialSourcePriority(Source);

		// SourcePriority 100 = Inventory (player), 50 = Containers, 25 = World Items
		bool bSourceEnabled = false;
		if (SourcePriority >= 100)
		{
			bSourceEnabled = InventoryCheckbox && InventoryCheckbox->IsChecked();
		}
		else if (SourcePriority >= 50)
		{
			bSourceEnabled = ContainersCheckbox && ContainersCheckbox->IsChecked();
		}
		else
		{
			bSourceEnabled = SurroundingCheckbox && SurroundingCheckbox->IsChecked();
		}

		if (!bSourceEnabled)
		{
			continue;
		}

		// Try to gather from this source via interface
		if (IMOMaterialSourceInterface::Execute_CanProvideMaterial(Source, ItemId, 1))
		{
			int32 Gathered = IMOMaterialSourceInterface::Execute_GatherMaterial(Source, ItemId, 1);
			if (Gathered > 0)
			{
				Progress->DepositMaterial(ItemId);

				// Track deposited material for potential refund
				FMODepositedMaterial DepositedMat;
				DepositedMat.ItemId = ItemId;

				// Get rarity from item definition
				FMOItemDefinitionRow ItemDef;
				if (UMOItemDatabaseSettings::GetItemDefinition(ItemId, ItemDef))
				{
					DepositedMat.Rarity = ItemDef.Rarity;
				}
				DepositedMaterialsForRefund.Add(DepositedMat);

				UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Gathered %s from %s (priority %d)"),
					*ItemId.ToString(), *Source->GetName(), SourcePriority);
				return true;
			}
		}
	}

	return false;
}

void UMOGhostContextMenu::PopulateMaterialList()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] PopulateMaterialList called, MaterialEntries: %d"), MaterialEntries.Num());

	if (!MaterialListContainer)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] MaterialListContainer is NULL - widget not bound! Make sure widget is named 'MaterialListContainer' and marked as variable."));
		return;
	}

	// Clear existing widgets
	MaterialListContainer->ClearChildren();
	MaterialTextWidgets.Empty();

	// Create text widgets for each material
	for (const FMOGhostMaterialEntry& Entry : MaterialEntries)
	{
		UTextBlock* TextWidget = CreateMaterialTextWidget(Entry);
		if (TextWidget)
		{
			MaterialListContainer->AddChild(TextWidget);
			MaterialTextWidgets.Add(TextWidget);
			UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Added material: %s (%d/%d)"),
				*Entry.ItemId.ToString(), Entry.Deposited, Entry.Required);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] PopulateMaterialList complete, added %d widgets"), MaterialTextWidgets.Num());
}

int32 UMOGhostContextMenu::CalculateRefundAmount(int32 TotalDeposited, bool bBuildStarted) const
{
	if (TotalDeposited <= 0)
	{
		return 0;
	}

	// Before build starts: 100% refund
	if (!bBuildStarted)
	{
		return TotalDeposited;
	}

	// After build starts: 50% base + skill bonus (0-45% for skill 0-100)
	constexpr float BaseRefundPercent = 0.50f;
	constexpr float MaxSkillBonus = 0.45f;

	float SkillBonus = 0.0f;
	if (UMOSkillsComponent* Skills = BuilderSkills.Get())
	{
		const int32 SkillLevel = Skills->GetSkillLevel(BuildingSkillId);
		// Clamp to 0-100 and calculate bonus
		const float NormalizedSkill = FMath::Clamp(static_cast<float>(SkillLevel), 0.0f, 100.0f) / 100.0f;
		SkillBonus = NormalizedSkill * MaxSkillBonus;
	}

	const float TotalRefundPercent = BaseRefundPercent + SkillBonus;
	const int32 RefundAmount = FMath::FloorToInt(TotalDeposited * TotalRefundPercent);

	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Refund calc: %d total, %.1f%% base + %.1f%% skill = %d returned"),
		TotalDeposited, BaseRefundPercent * 100.0f, SkillBonus * 100.0f, RefundAmount);

	return RefundAmount;
}

void UMOGhostContextMenu::RefundMaterialsToInventory(int32 RefundCount)
{
	if (RefundCount <= 0 || DepositedMaterialsForRefund.Num() == 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] RefundMaterialsToInventory: Nothing to refund (RefundCount=%d, Deposited=%d)"),
			RefundCount, DepositedMaterialsForRefund.Num());
		return;
	}

	UMOInventoryComponent* Inventory = BuilderInventory.Get();
	if (!Inventory)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] RefundMaterialsToInventory: No builder inventory, cannot refund!"));
		return;
	}

	// Sort by rarity (lowest first) so we lose common items first, keep rarer items
	DepositedMaterialsForRefund.Sort([](const FMODepositedMaterial& A, const FMODepositedMaterial& B) {
		return static_cast<uint8>(A.Rarity) < static_cast<uint8>(B.Rarity);
	});

	// Materials to lose are at the front (lowest rarity)
	// Materials to refund are at the back (highest rarity)
	const int32 TotalDeposited = DepositedMaterialsForRefund.Num();
	const int32 LoseCount = TotalDeposited - RefundCount;

	// Consolidate materials to refund by ItemId (so stackable items stack properly)
	TMap<FName, int32> ItemsToRefund;
	for (int32 i = LoseCount; i < TotalDeposited; ++i)
	{
		const FMODepositedMaterial& Mat = DepositedMaterialsForRefund[i];
		int32& Count = ItemsToRefund.FindOrAdd(Mat.ItemId);
		Count++;
	}

	// Return consolidated materials to inventory
	int32 RefundedCount = 0;
	for (const auto& Pair : ItemsToRefund)
	{
		const FName& ItemId = Pair.Key;
		const int32 Quantity = Pair.Value;

		// Add stacked items with a single GUID
		FGuid NewItemGuid = FGuid::NewGuid();
		if (Inventory->AddItemByGuid(NewItemGuid, ItemId, Quantity))
		{
			RefundedCount += Quantity;
			UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Refunded %d x %s to inventory"), Quantity, *ItemId.ToString());
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] Failed to add %d x %s to inventory (full?)"), Quantity, *ItemId.ToString());
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Refunded %d/%d materials to inventory"), RefundedCount, RefundCount);

	// Log what was lost
	if (LoseCount > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Lost %d materials (lowest rarity first)"), LoseCount);
	}
}

void UMOGhostContextMenu::DropRefundedMaterials(int32 RefundCount, const FVector& DropLocation)
{
	if (RefundCount <= 0 || DepositedMaterialsForRefund.Num() == 0)
	{
		return;
	}

	// Sort by rarity (lowest first) so we lose common items first, keep rarer items
	DepositedMaterialsForRefund.Sort([](const FMODepositedMaterial& A, const FMODepositedMaterial& B) {
		return static_cast<uint8>(A.Rarity) < static_cast<uint8>(B.Rarity);
	});

	// Materials to lose are at the front (lowest rarity)
	// Materials to refund are at the back (highest rarity)
	const int32 TotalDeposited = DepositedMaterialsForRefund.Num();
	const int32 LoseCount = TotalDeposited - RefundCount;

	// Spawn refunded materials (skip the first LoseCount items)
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 i = LoseCount; i < TotalDeposited; ++i)
	{
		const FMODepositedMaterial& Mat = DepositedMaterialsForRefund[i];

		FMOItemDefinitionRow ItemDef;
		if (!UMOItemDatabaseSettings::GetItemDefinition(Mat.ItemId, ItemDef))
		{
			continue;
		}

		UClass* WorldActorClass = ItemDef.WorldVisual.WorldActorClass.LoadSynchronous();
		if (!WorldActorClass)
		{
			continue;
		}

		// Scatter spawn locations slightly
		FVector SpawnLocation = DropLocation + FVector(
			FMath::RandRange(-50.0f, 50.0f),
			FMath::RandRange(-50.0f, 50.0f),
			FMath::RandRange(0.0f, 30.0f)
		);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AActor* SpawnedActor = World->SpawnActor<AActor>(WorldActorClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

		if (SpawnedActor)
		{
			// Apply item definition visuals if it's a world item
			if (AMOWorldItem* WorldItem = Cast<AMOWorldItem>(SpawnedActor))
			{
				WorldItem->ApplyItemDefinitionToWorldMesh();
				WorldItem->EnableDropPhysics();
			}

			UE_LOG(LogMOFramework, Verbose, TEXT("[MOGhostContextMenu] Refunded %s (%s rarity)"),
				*Mat.ItemId.ToString(),
				*UEnum::GetValueAsString(Mat.Rarity));
		}
	}

	// Log what was lost
	if (LoseCount > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Lost %d materials (lowest rarity first)"), LoseCount);
	}
}
