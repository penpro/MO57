#include "MOCharacterUIController.h"
#include "MOFramework.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"

#include "MOUIManagerComponent.h"
#include "MOSkillsPanel.h"
#include "MOStatusPanel.h"
#include "MOInspectionProgressWidget.h"
#include "MOSkillsComponent.h"
#include "MOKnowledgeComponent.h"
#include "MOVitalsComponent.h"
#include "MOMetabolismComponent.h"
#include "MOMentalStateComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MONotificationComponent.h"
#include "MOQuestSubsystem.h"
#include "MOPrimaryGameLayout.h"
#include "MOCharacter.h"
#include "MOTerraformingComponent.h"
#include "MOProgressWidgetBase.h"

UMOCharacterUIController::UMOCharacterUIController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMOCharacterUIController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalOwningPlayerController())
	{
		if (bCreateStatusPanelOnBeginPlay)
		{
			CreateStatusPanel();
		}

		// Initial bind attempt — usually no-ops because pawn is null this early.
		// The systematic OnPossessedPawnChanged hook on the base class runs the
		// real rebind once possession completes.
		BindToPawnTerraformingComponent();
	}
}

void UMOCharacterUIController::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	UE_LOG(LogMOFramework, Log,
		TEXT("[MOCharUI] PossessedPawnChanged: old=%s new=%s — rebinding terraform delegates and status panel"),
		OldPawn ? *OldPawn->GetName() : TEXT("<null>"),
		NewPawn ? *NewPawn->GetName() : TEXT("<null>"));
	OnPawnChanged();
}

void UMOCharacterUIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up skills panel widget - unbind delegates first
	if (UMOSkillsPanel* SkillsWidget = SkillsPanelWidget.Get())
	{
		SkillsWidget->OnRequestClose.RemoveDynamic(this, &UMOCharacterUIController::HandleSkillsPanelRequestClose);
		if (SkillsWidget->IsInViewport())
		{
			SkillsWidget->RemoveFromParent();
		}
	}
	SkillsPanelWidget.Reset();

	// Clean up status panel widget - unbind delegates first
	if (UMOStatusPanel* StatusWidget = StatusPanelWidget.Get())
	{
		StatusWidget->OnRequestClose.RemoveDynamic(this, &UMOCharacterUIController::HandleStatusPanelRequestClose);
		StatusWidget->RemoveFromParent();
	}
	StatusPanelWidget.Reset();

	// Clean up inspection widget - unbind delegates first
	if (UMOInspectionProgressWidget* InspectionWidget = InspectionProgressWidget.Get())
	{
		InspectionWidget->OnInspectionCompleted.RemoveDynamic(this, &UMOCharacterUIController::HandleInspectionCompleted);
		InspectionWidget->OnInspectionCancelled.RemoveDynamic(this, &UMOCharacterUIController::HandleInspectionCancelled);
		InspectionWidget->RemoveFromParent();
	}
	InspectionProgressWidget.Reset();

	// Don't leave a stale entry on the character's movement-interrupt list.
	UnregisterFromInspectionInterrupts();

	// Tear down any in-flight terraform progress widget and unbind from the
	// component so we don't get callbacks during world teardown.
	TearDownTerraformProgressWidget();
	UnbindFromPawnTerraformingComponent();

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// Skills Panel
// =============================================================================

void UMOCharacterUIController::ToggleSkillsPanel()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	// Frame-based debounce: prevent double-toggle from ECommonInputMode::All
	const uint64 CurrentFrame = GFrameCounter;
	if (CurrentFrame == LastToggleFrame)
	{
		return;
	}
	LastToggleFrame = CurrentFrame;

	// Query UIManager for in-game menu state
	UMOUIManagerComponent* UIManager = GetUIManager();
	if (UIManager && UIManager->IsInGameMenuOpen())
	{
		return;
	}

	// If skills panel is already open, just close it
	if (IsSkillsPanelOpen())
	{
		CloseSkillsPanel();
		return;
	}

	// Close all switchable menus and open skills panel
	if (UIManager)
	{
		UIManager->CloseAllSwitchableMenus();
	}
	OpenSkillsPanel();
}

void UMOCharacterUIController::OpenSkillsPanel()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	// Check for valid pawn first
	if (!HasValidPawn())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] OpenSkillsPanel - No valid pawn, showing notification"));
		ShowNoPawnNotification();
		return;
	}

	if (!SkillsPanelClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharUI] SkillsPanelClass not set on CharacterUIController."));
		return;
	}

	// Get current pawn
	APawn* CurrentPawn = PlayerController->GetPawn();
	if (!CurrentPawn)
	{
		return;
	}

	// Close any existing panel first
	UMOSkillsPanel* ExistingPanel = SkillsPanelWidget.Get();
	if (IsValid(ExistingPanel) && ExistingPanel->IsActivated())
	{
		PopWidgetFromLayer(ExistingPanel);
		SkillsPanelWidget.Reset();
	}

	// Create new widget via CommonUI layer stack
	ShowModalBackground();
	UCommonActivatableWidget* CreatedWidget = PushWidgetToLayer(MOUILayerTags::Layer_Menu, SkillsPanelClass);
	UMOSkillsPanel* PanelWidget = Cast<UMOSkillsPanel>(CreatedWidget);

	if (!IsValid(PanelWidget))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCharUI] Failed to create skills panel via layer stack"));
		HideModalBackground();
		return;
	}

	// Cache reference + auto-clear-on-deactivate (any close path). See base class.
	RegisterCachedMenu(PanelWidget, SkillsPanelWidget);
	PanelWidget->OnRequestClose.RemoveAll(this);
	PanelWidget->OnRequestClose.AddDynamic(this, &UMOCharacterUIController::HandleSkillsPanelRequestClose);

	// Get skills and knowledge components from cache
	UMOSkillsComponent* Skills = GetCachedSkills();
	UMOKnowledgeComponent* Knowledge = GetCachedKnowledge();

	// Initialize with both components
	PanelWidget->InitializePanelWithKnowledge(Skills, Knowledge);

	// Widget handles input state via NativeOnActivated/GetDesiredInputConfig
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Skills Panel opened"));
}

void UMOCharacterUIController::CloseSkillsPanel()
{
	// IMPORTANT: Get reference before clearing cache
	// Reset cache FIRST to ensure IsSkillsPanelOpen() returns false immediately
	// This prevents race conditions with toggle input that fires multiple times per frame
	UMOSkillsPanel* PanelWidget = SkillsPanelWidget.Get();
	SkillsPanelWidget.Reset();

	if (IsValid(PanelWidget) && PanelWidget->IsActivated())
	{
		PopWidgetFromLayer(PanelWidget);
	}

	// Modal background + reticle refresh handled by UMOActivatableWidget::NativeOnDeactivated.

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Skills Panel closed"));
}

bool UMOCharacterUIController::IsSkillsPanelOpen() const
{
	return IsCachedMenuOpen(SkillsPanelWidget);
}

UMOSkillsPanel* UMOCharacterUIController::GetSkillsPanel() const
{
	return SkillsPanelWidget.Get();
}

void UMOCharacterUIController::HandleSkillsPanelRequestClose()
{
	CloseSkillsPanel();
}

// =============================================================================
// Status Panel
// =============================================================================

void UMOCharacterUIController::CreateStatusPanel()
{
	// Status panel is now created on-demand via SetPlayerStatusVisible
	// This function is kept for backwards compatibility but does nothing
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] CreateStatusPanel called - status panel is now created on-demand."));
}

void UMOCharacterUIController::TogglePlayerStatus()
{
	// Frame-based debounce: prevent double-toggle from ECommonInputMode::All
	const uint64 CurrentFrame = GFrameCounter;
	if (CurrentFrame == LastToggleFrame)
	{
		return;
	}
	LastToggleFrame = CurrentFrame;

	// Query UIManager for in-game menu state
	UMOUIManagerComponent* UIManager = GetUIManager();
	if (UIManager && UIManager->IsInGameMenuOpen() && !IsPlayerStatusVisible())
	{
		return;
	}

	// If already visible, just close it
	if (IsPlayerStatusVisible())
	{
		SetPlayerStatusVisible(false);
		return;
	}

	// Close other switchable menus and open this one
	if (UIManager)
	{
		UIManager->CloseAllSwitchableMenus();
	}
	SetPlayerStatusVisible(true);
}

void UMOCharacterUIController::HandleStatusPanelRequestClose()
{
	SetPlayerStatusVisible(false);
}

UMOStatusPanel* UMOCharacterUIController::GetStatusPanel() const
{
	return StatusPanelWidget.Get();
}

void UMOCharacterUIController::SetPlayerStatusVisible(bool bVisible)
{
	if (bVisible)
	{
		// Check for valid pawn first
		if (!HasValidPawn())
		{
			ShowNoPawnNotification();
			return;
		}

		if (!StatusPanelClass)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCharUI] StatusPanelClass not set on CharacterUIController."));
			return;
		}

		// Close any existing panel first
		UMOStatusPanel* ExistingPanel = StatusPanelWidget.Get();
		if (IsValid(ExistingPanel) && ExistingPanel->IsActivated())
		{
			PopWidgetFromLayer(ExistingPanel);
			StatusPanelWidget.Reset();
		}

		// Create new widget via CommonUI layer stack
		ShowModalBackground();
		UCommonActivatableWidget* CreatedWidget = PushWidgetToLayer(MOUILayerTags::Layer_Menu, StatusPanelClass);
		UMOStatusPanel* Status = Cast<UMOStatusPanel>(CreatedWidget);

		if (!IsValid(Status))
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOCharUI] Failed to create status panel via layer stack"));
			HideModalBackground();
			return;
		}

		// Cache reference + auto-clear-on-deactivate (any close path). See base class.
		RegisterCachedMenu(Status, StatusPanelWidget);
		Status->OnRequestClose.RemoveAll(this);
		Status->OnRequestClose.AddDynamic(this, &UMOCharacterUIController::HandleStatusPanelRequestClose);

		// Bind to current pawn's medical components
		RebindStatusPanelToCurrentPawn();

		UpdateReticleVisibility();

		UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Status panel opened"));
	}
	else
	{
		// IMPORTANT: Get reference before clearing cache
		// Reset cache FIRST to ensure IsPlayerStatusVisible() returns false immediately
		// This prevents race conditions with toggle input that fires multiple times per frame
		UMOStatusPanel* Status = StatusPanelWidget.Get();
		StatusPanelWidget.Reset();

		if (IsValid(Status) && Status->IsActivated())
		{
			PopWidgetFromLayer(Status);
		}

		// Modal background + reticle refresh handled by UMOActivatableWidget::NativeOnDeactivated.

		UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Status panel closed"));
	}
}

bool UMOCharacterUIController::IsPlayerStatusVisible() const
{
	// Check if we have a cached status panel - if so, panel is either open or opening
	// IMPORTANT: Check IsValid() first, not just IsActivated()
	// CommonUI defers activation, so there's a window where the widget is cached but not yet activated
	UMOStatusPanel* Status = StatusPanelWidget.Get();
	return IsValid(Status);
}

void UMOCharacterUIController::GetCurrentPawnMedicalComponents(UMOVitalsComponent*& OutVitals, UMOMetabolismComponent*& OutMetabolism, UMOMentalStateComponent*& OutMental) const
{
	// Use cached components from base class
	OutVitals = GetCachedVitals();
	OutMetabolism = GetCachedMetabolism();
	OutMental = GetCachedMentalState();
}

void UMOCharacterUIController::RebindStatusPanelToCurrentPawn()
{
	UMOStatusPanel* Status = StatusPanelWidget.Get();
	if (!IsValid(Status))
	{
		return;
	}

	UMOVitalsComponent* Vitals = nullptr;
	UMOMetabolismComponent* Metabolism = nullptr;
	UMOMentalStateComponent* Mental = nullptr;

	GetCurrentPawnMedicalComponents(Vitals, Metabolism, Mental);

	// Bind to medical components (null-safe - will unbind if any are null)
	Status->BindToMedicalComponents(Vitals, Metabolism, Mental);

	// Refresh the Info tab with current pawn data
	Status->RefreshCharacterInfo();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Status panel rebound to current pawn (Vitals: %s, Metabolism: %s, Mental: %s)"),
		IsValid(Vitals) ? TEXT("Yes") : TEXT("No"),
		IsValid(Metabolism) ? TEXT("Yes") : TEXT("No"),
		IsValid(Mental) ? TEXT("Yes") : TEXT("No"));
}

void UMOCharacterUIController::OnPawnChanged()
{
	// Rebind status panel if visible
	if (bStatusPanelVisible)
	{
		RebindStatusPanelToCurrentPawn();
	}

	// Possessing a new pawn means a new TerraformingComponent. Any active
	// progress widget belongs to the old pawn — tear it down — and rebind
	// our OnTerraformStarted listener to the new pawn's component.
	TearDownTerraformProgressWidget();
	UnbindFromPawnTerraformingComponent();
	BindToPawnTerraformingComponent();
}

// =============================================================================
// Terraforming Progress Widget Lifecycle
// =============================================================================

void UMOCharacterUIController::BindToPawnTerraformingComponent()
{
	APlayerController* PC = ResolveOwningPlayerController();
	if (!IsValid(PC))
	{
		return;
	}

	AMOCharacter* Pawn = Cast<AMOCharacter>(PC->GetPawn());
	if (!IsValid(Pawn))
	{
		return;
	}

	UMOTerraformingComponent* Terraforming = Pawn->GetTerraformingComponent();
	if (!IsValid(Terraforming))
	{
		return;
	}

	// Defensive remove-before-add: if BeginPlay and OnPawnChanged both fire
	// for the same pawn, we don't want stacked bindings.
	// Subscribe to ALL the terraform lifecycle events here. The widget is a
	// pure view — the controller is what mediates between the model
	// (component) and the view (widget).
	Terraforming->OnTerraformStarted.RemoveDynamic(this, &UMOCharacterUIController::HandleTerraformStarted);
	Terraforming->OnTerraformProgress.RemoveDynamic(this, &UMOCharacterUIController::HandleTerraformProgress);
	Terraforming->OnTerraformCompleted.RemoveDynamic(this, &UMOCharacterUIController::HandleTerraformCompleted);
	Terraforming->OnTerraformCancelled.RemoveDynamic(this, &UMOCharacterUIController::HandleTerraformCancelled);

	Terraforming->OnTerraformStarted.AddDynamic(this, &UMOCharacterUIController::HandleTerraformStarted);
	Terraforming->OnTerraformProgress.AddDynamic(this, &UMOCharacterUIController::HandleTerraformProgress);
	Terraforming->OnTerraformCompleted.AddDynamic(this, &UMOCharacterUIController::HandleTerraformCompleted);
	Terraforming->OnTerraformCancelled.AddDynamic(this, &UMOCharacterUIController::HandleTerraformCancelled);

	BoundTerraformingComponent = Terraforming;
}

void UMOCharacterUIController::UnbindFromPawnTerraformingComponent()
{
	if (UMOTerraformingComponent* Comp = BoundTerraformingComponent.Get())
	{
		Comp->OnTerraformStarted.RemoveDynamic(this, &UMOCharacterUIController::HandleTerraformStarted);
		Comp->OnTerraformProgress.RemoveDynamic(this, &UMOCharacterUIController::HandleTerraformProgress);
		Comp->OnTerraformCompleted.RemoveDynamic(this, &UMOCharacterUIController::HandleTerraformCompleted);
		Comp->OnTerraformCancelled.RemoveDynamic(this, &UMOCharacterUIController::HandleTerraformCancelled);
	}
	BoundTerraformingComponent.Reset();
}

void UMOCharacterUIController::HandleTerraformStarted(EMOTerraformMode Mode, float DurationSeconds)
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	APlayerController* PC = ResolveOwningPlayerController();
	if (!IsValid(PC))
	{
		return;
	}

	UMOTerraformingComponent* Comp = BoundTerraformingComponent.Get();
	if (!IsValid(Comp))
	{
		return;
	}

	if (!TerraformProgressWidgetClass)
	{
		// The action will still complete — the component drives its own
		// timer and applies the sculpt on completion regardless of UI. The
		// player just won't see a progress bar. Soft failure so the game
		// stays playable while the Blueprint is being set up.
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOCharUI] TerraformProgressWidgetClass not set — terraform still applies, but no progress bar. Point this at any UMOProgressWidgetBase BP (the existing harvest progress widget works) in the UMOCharacterUIController Blueprint defaults."));
		return;
	}

	// Tear down any previous widget. BeginTerraform already cancels its own
	// previous pending action, but the UI side might still have a stale
	// widget if the previous one didn't get cancelled cleanly.
	TearDownTerraformProgressWidget();

	// Push to the GameOverlay layer so the bar appears over the HUD.
	UCommonActivatableWidget* ActualWidget = PushWidgetToLayer(
		MOUILayerTags::Layer_GameOverlay, TerraformProgressWidgetClass);

	UMOProgressWidgetBase* Widget = Cast<UMOProgressWidgetBase>(ActualWidget);
	if (!IsValid(Widget))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCharUI] Failed to create terraform progress widget"));
		return;
	}

	TerraformProgressWidget = Widget;

	// Wire up the widget's Cancel button (base-class OnCancelled delegate).
	// The user clicking Cancel maps to "cancel my terraform action."
	Widget->OnCancelled.RemoveDynamic(this, &UMOCharacterUIController::HandleTerraformWidgetCancelled);
	Widget->OnCancelled.AddDynamic(this, &UMOCharacterUIController::HandleTerraformWidgetCancelled);

	// Initialize the widget's label + duration. StartProgress sets ActionName
	// and starts the base class's own timed-tick mode. The very next component
	// tick will call Widget->SetProgress, which switches the widget to
	// non-timed mode — so the widget's tick only "runs" for one frame at
	// most before the component takes over driving values.
	Widget->StartProgress(Comp->GetModeDisplayName(), DurationSeconds);

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Started terraform progress widget for mode '%s' (%.1fs)"),
		*Comp->GetModeDisplayName().ToString(), DurationSeconds);
}

void UMOCharacterUIController::HandleTerraformProgress(float Progress01, float TimeRemainingSeconds)
{
	UMOProgressWidgetBase* Widget = TerraformProgressWidget.Get();
	if (!IsValid(Widget))
	{
		return;
	}

	// SetProgress switches bIsTimedProgress=false so the widget's own
	// NativeTick stops advancing — values come from the component now.
	Widget->SetProgress(Progress01, TimeRemainingSeconds);
}

void UMOCharacterUIController::HandleTerraformCompleted(bool bSuccess)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Terraform completed (success=%s)"),
		bSuccess ? TEXT("true") : TEXT("false"));
	TearDownTerraformProgressWidget();
}

void UMOCharacterUIController::HandleTerraformCancelled()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Terraform cancelled"));
	TearDownTerraformProgressWidget();
}

void UMOCharacterUIController::HandleTerraformWidgetCancelled()
{
	// User clicked the widget's Cancel button. Tell the component to cancel —
	// the component's OnTerraformCancelled will then fire HandleTerraformCancelled
	// which tears down the widget.
	if (UMOTerraformingComponent* Comp = BoundTerraformingComponent.Get())
	{
		Comp->CancelTerraform();
	}
}

void UMOCharacterUIController::TearDownTerraformProgressWidget()
{
	UMOProgressWidgetBase* Widget = TerraformProgressWidget.Get();
	if (!IsValid(Widget))
	{
		TerraformProgressWidget.Reset();
		return;
	}

	Widget->OnCancelled.RemoveDynamic(this, &UMOCharacterUIController::HandleTerraformWidgetCancelled);

	// DeactivateWidget triggers UMOActivatableWidget::NativeOnDeactivated,
	// which now systematically refreshes the modal background and reticle
	// for every widget close — no need for per-caller HideModalBackground /
	// UpdateReticleVisibility calls.
	Widget->DeactivateWidget();
	if (Widget->IsInViewport())
	{
		Widget->RemoveFromParent();
	}
	TerraformProgressWidget.Reset();
}

// =============================================================================
// Item Inspection
// =============================================================================

void UMOCharacterUIController::StartItemInspection(FName ItemDefinitionId, const FGuid& ItemGuid)
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	// Cancel any existing inspection
	if (IsInspectionInProgress())
	{
		CancelItemInspection();
	}

	// Inspection used to close the inventory menu here. Per design — users want to
	// keep the inventory open while an inspection runs in the background (so they
	// can look at other items or queue up more inspections). The progress widget
	// lives on the GameOverlay layer; the inventory is on the Menu layer, so they
	// coexist visually.

	// Get item display name from database
	FText ItemDisplayName = UMOItemDatabaseSettings::GetItemDisplayName(ItemDefinitionId);
	if (ItemDisplayName.IsEmpty())
	{
		ItemDisplayName = FText::FromName(ItemDefinitionId);
	}

	// Get knowledge and skills components from pawn
	APawn* CurrentPawn = PlayerController->GetPawn();
	if (!IsValid(CurrentPawn))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharUI] StartItemInspection - No pawn to inspect with"));
		ShowNoPawnNotification();
		return;
	}

	UMOKnowledgeComponent* KnowledgeComp = GetCachedKnowledge();
	UMOSkillsComponent* SkillsComp = GetCachedSkills();

	if (!IsValid(KnowledgeComp))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharUI] StartItemInspection - Pawn has no KnowledgeComponent"));
		return;
	}

	// Create inspection widget if needed
	if (!InspectionProgressWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharUI] InspectionProgressWidgetClass not set on CharacterUIController"));
		return;
	}

	UMOInspectionProgressWidget* InspectionWidget = InspectionProgressWidget.Get();
	bool bNeedsBindDelegates = false;

	if (IsValid(InspectionWidget))
	{
		// Try to reuse existing widget - push it to the layer stack
		UCommonActivatableWidget* ActualWidget = PushWidgetInstanceToLayer(MOUILayerTags::Layer_GameOverlay, InspectionWidget);

		// CommonUI may have created a new widget (can't add existing instances to stacks)
		if (ActualWidget && ActualWidget != InspectionWidget)
		{
			InspectionWidget = Cast<UMOInspectionProgressWidget>(ActualWidget);
			InspectionProgressWidget = InspectionWidget;
			bNeedsBindDelegates = true;
		}
	}
	else
	{
		// Create new widget via layer stack
		UCommonActivatableWidget* ActualWidget = PushWidgetToLayer(MOUILayerTags::Layer_GameOverlay, InspectionProgressWidgetClass);
		InspectionWidget = Cast<UMOInspectionProgressWidget>(ActualWidget);
		bNeedsBindDelegates = true;
	}

	if (!IsValid(InspectionWidget))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCharUI] Failed to create inspection widget"));
		return;
	}

	// Cache and bind delegates if this is a new widget
	if (bNeedsBindDelegates)
	{
		InspectionProgressWidget = InspectionWidget;
		InspectionWidget->OnInspectionCompleted.RemoveDynamic(this, &UMOCharacterUIController::HandleInspectionCompleted);
		InspectionWidget->OnInspectionCancelled.RemoveDynamic(this, &UMOCharacterUIController::HandleInspectionCancelled);
		InspectionWidget->OnInspectionCompleted.AddDynamic(this, &UMOCharacterUIController::HandleInspectionCompleted);
		InspectionWidget->OnInspectionCancelled.AddDynamic(this, &UMOCharacterUIController::HandleInspectionCancelled);
	}

	// Store the item being inspected
	InspectingItemGuid = ItemGuid;

	// Start the inspection (widget is now in layer stack, CommonUI handles input mode)
	InspectionWidget->StartInspection(ItemDefinitionId, ItemDisplayName, KnowledgeComp, SkillsComp, InspectionDuration);

	// Subscribe to movement-interrupt — inspection is a "stand still and study"
	// action, so any motion cancels it (no partial progress saved).
	RegisterForInspectionInterrupts();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Started inspection of item '%s' (GUID: %s)"),
		*ItemDefinitionId.ToString(), *ItemGuid.ToString(EGuidFormats::DigitsWithHyphens));
}

void UMOCharacterUIController::CancelItemInspection()
{
	UMOInspectionProgressWidget* InspectionWidget = InspectionProgressWidget.Get();
	if (IsValid(InspectionWidget) && InspectionWidget->IsInspectionInProgress())
	{
		InspectionWidget->CancelInspection();
	}
}

bool UMOCharacterUIController::IsInspectionInProgress() const
{
	const UMOInspectionProgressWidget* InspectionWidget = InspectionProgressWidget.Get();
	return IsValid(InspectionWidget) && InspectionWidget->IsInspectionInProgress();
}

void UMOCharacterUIController::HandleInspectionCompleted(bool bCompleted, const FMOInspectionResult& Result)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Inspection completed: Success=%s, XPGrants=%d"),
		bCompleted ? TEXT("true") : TEXT("false"),
		Result.XPGrants.Num());

	// Inspection ended one way or another — stop listening for movement.
	UnregisterFromInspectionInterrupts();

	// Deactivate widget (CommonUI pops it from the layer stack and restores input)
	UMOInspectionProgressWidget* InspectionWidget = InspectionProgressWidget.Get();
	if (IsValid(InspectionWidget))
	{
		InspectionWidget->DeactivateWidget();
	}

	// Clear inspecting item
	InspectingItemGuid.Invalidate();

	// Modal background + reticle refresh handled by UMOActivatableWidget::NativeOnDeactivated.

	// Show notifications for inspection results
	if (bCompleted && Result.bSuccess)
	{
		UMONotificationComponent* NotificationComp = GetNotificationComponent();

		// Cycle through showing popup for each affected entry (both skills and knowledge)
		// Each entry gets its own popup that displays sequentially
		for (const FMOInspectionXPGrant& Grant : Result.XPGrants)
		{
			if (Grant.XPAmount > 0.0f && NotificationComp)
			{
				// Both skills and knowledge use the same popup system
				// The bIsKnowledge flag can be used by the widget to style differently if desired
				NotificationComp->ShowSkillPopup(Grant.Id, 3.0f);

				UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI]   Showing popup for %s '%s': +%.0f XP, Level %d -> %d"),
					Grant.bIsKnowledge ? TEXT("knowledge") : TEXT("skill"),
					*Grant.Id.ToString(),
					Grant.XPAmount,
					Grant.LevelBefore,
					Grant.LevelAfter);
			}
		}

		// Show feedback message about learning potential
		if (!Result.FeedbackMessage.IsEmpty() && NotificationComp)
		{
			NotificationComp->ShowNotification(Result.FeedbackMessage, 4.0f);
		}

		// Fire quest event for item inspection
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UMOQuestSubsystem* QuestSub = GI->GetSubsystem<UMOQuestSubsystem>())
				{
					// Get the item ID from the widget if still valid
					if (IsValid(InspectionWidget))
					{
						const FName InspectedItemId = InspectionWidget->GetInspectingItemId();
						if (!InspectedItemId.IsNone())
						{
							// Fire both a generic "ItemInspected" event and an item-specific event
							QuestSub->FireGameEvent(FName(TEXT("ItemInspected")));
							QuestSub->FireGameEvent(FName(*FString::Printf(TEXT("Inspected_%s"), *InspectedItemId.ToString())));
						}
					}
				}
			}
		}
	}
}

void UMOCharacterUIController::HandleInspectionCancelled()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Inspection cancelled"));

	// Deactivate widget (CommonUI pops it from the layer stack and restores input)
	UMOInspectionProgressWidget* InspectionWidget = InspectionProgressWidget.Get();
	if (IsValid(InspectionWidget))
	{
		InspectionWidget->DeactivateWidget();
	}

	// Clear inspecting item
	InspectingItemGuid.Invalidate();

	UnregisterFromInspectionInterrupts();

	// Modal background + reticle refresh handled by UMOActivatableWidget::NativeOnDeactivated.
}

// =============================================================================
// INTERRUPT HANDLING (IMOInterruptibleInterface)
// =============================================================================

void UMOCharacterUIController::NotifyInterrupt_Implementation(const FMOInterruptContext& Context)
{
	if (!IsInspectionInProgress())
	{
		// Defensive — should already be unregistered, but a no-op is safer
		// than a crash if the unregister somehow lagged.
		return;
	}

	// Inspection is a "stand still and concentrate" activity. Any meaningful
	// disturbance discards progress. UserCancel is intentionally skipped —
	// the inventory UI calls CancelItemInspection directly on that path, so
	// reacting here would double-fire.
	switch (Context.Reason)
	{
	case EMOInterruptReason::Movement:
	case EMOInterruptReason::Damage:
	case EMOInterruptReason::Knockdown:
	case EMOInterruptReason::Unconscious:
	case EMOInterruptReason::Death:
	case EMOInterruptReason::EnteredCombat:
	case EMOInterruptReason::LostControl:
	case EMOInterruptReason::External:
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOCharUI] Interrupt reason=%d — cancelling item inspection (no progress saved)"),
			(int32)Context.Reason);
		// CancelItemInspection -> widget cancels -> HandleInspectionCancelled
		// runs UnregisterFromInspectionInterrupts. Clean self-removal.
		CancelItemInspection();
		break;

	case EMOInterruptReason::UserCancel:
	case EMOInterruptReason::None:
	default:
		break;
	}
}

void UMOCharacterUIController::RegisterForInspectionInterrupts()
{
	APlayerController* PC = ResolveOwningPlayerController();
	if (!IsValid(PC))
	{
		return;
	}
	AMOCharacter* Pawn = Cast<AMOCharacter>(PC->GetPawn());
	if (!IsValid(Pawn))
	{
		return;
	}

	Pawn->RegisterInterruptListener(this);
	RegisteredInspectionCharacter = Pawn;
}

void UMOCharacterUIController::UnregisterFromInspectionInterrupts()
{
	if (AMOCharacter* Pawn = RegisteredInspectionCharacter.Get())
	{
		Pawn->UnregisterInterruptListener(this);
	}
	RegisteredInspectionCharacter.Reset();
}
