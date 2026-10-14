#include "MOGhostContextMenu.h"
#include "MOBuildableActor.h"
#include "MOBuildProgressComponent.h"
#include "MOInventoryComponent.h"
#include "MOBuildingComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOBuildingTypes.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOUIUtils.h"
#include "Components/CheckBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

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
	// The server gathers (the deposits and the inventories they come from are its state); the list refreshes when the ledger changes.
	if (BuildProgress.IsValid())
	{
		RequestBuild(EMOBuildRequest::AddMaterials);
	}
}

void UMOGhostContextMenu::StartBuild()
{
	if (!BuildProgress.IsValid())
	{
		return;
	}
	if (!AreAllMaterialsDeposited())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] Cannot start build - materials not deposited"));
		return;
	}
	RequestBuild(EMOBuildRequest::Start);
	OnBuildStarted.Broadcast();
	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Build requested"));
}

void UMOGhostContextMenu::CancelBuild()
{
	// The server refunds (Construction skill, rarest kept) and removes the building (UMOBuildProgressComponent::CancelAndRemove).
	if (TargetBuilding.IsValid())
	{
		RequestBuild(EMOBuildRequest::CancelAndRemove);
	}

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
	Progress->OnDepositsChanged.RemoveDynamic(this, &UMOGhostContextMenu::HandleDepositsChanged);
	Progress->OnDepositsChanged.AddDynamic(this, &UMOGhostContextMenu::HandleDepositsChanged);
}

void UMOGhostContextMenu::UnbindBuildProgressEvents()
{
	if (UMOBuildProgressComponent* Progress = BuildProgress.Get())
	{
		Progress->OnConstructionStateChanged.RemoveDynamic(this, &UMOGhostContextMenu::HandleBuildStateChanged);
		Progress->OnDepositsChanged.RemoveDynamic(this, &UMOGhostContextMenu::HandleDepositsChanged);
	}
}

void UMOGhostContextMenu::HandleDepositsChanged()
{
	RefreshMaterialList();
}

uint8 UMOGhostContextMenu::GetSourceFlags() const
{
	uint8 Flags = 0;
	Flags |= (!InventoryCheckbox || InventoryCheckbox->IsChecked()) ? MOMaterialSourceBit(EMOMaterialSource::Inventory) : 0;
	Flags |= (!ContainersCheckbox || ContainersCheckbox->IsChecked()) ? MOMaterialSourceBit(EMOMaterialSource::NearbyContainers) : 0;
	Flags |= (!SurroundingCheckbox || SurroundingCheckbox->IsChecked()) ? MOMaterialSourceBit(EMOMaterialSource::SurroundingArea) : 0;
	return Flags;
}

void UMOGhostContextMenu::RequestBuild(EMOBuildRequest Request)
{
	APlayerController* PC = GetOwningPlayer();
	UMOBuildingComponent* Building = PC ? PC->FindComponentByClass<UMOBuildingComponent>() : nullptr;
	if (!Building)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] Build request %d dropped: the owning player has no UMOBuildingComponent"), static_cast<int32>(Request));
		return;
	}
	Building->RequestBuildAction(TargetBuilding.Get(), Request, GetSourceFlags());
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
