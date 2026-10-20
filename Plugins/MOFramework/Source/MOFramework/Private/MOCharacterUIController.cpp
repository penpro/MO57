#include "MOCharacterUIController.h"
#include "MOHUDMode.h"
#include "MOMedicalSubsystem.h"
#include "MOAnatomyComponent.h"
#include "MOTimedActionComponent.h"
#include "MOUITokens.h"
#include "MOFramework.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"

#include "MOUIManagerComponent.h"
#include "MOSkillsPanel.h"
#include "MOStatusPanel.h"
#include "MOInspectionActions.h"
#include "MOPlayerController.h"
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
#include "MOHUDRootWidget.h"
#include "MOStatusEffectStripWidget.h"
#include "MOStatusMoodleTypes.h"
#include "MOSurvivalStatsComponent.h"

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
	// Drop the vitals subscription first — otherwise a stale delegate
	// pointer could fire into us during shutdown if the vitals component
	// outlives us by a frame.
	UnbindFromPawnVitalsForMoodles();
	UnbindFromPawnSurvivalStatsForMoodles();
	UnbindFromPawnTimedAction();
	UnbindFromPawnConditionsForMoodles();

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

	// Frame-based debounce: one toggle per frame even if the key reaches this controller twice (added when menus ran in ECommonInputMode::All
	// and both the game action and the menu fired; menus use Menu mode since the May-June rework -- kept as a cheap guard)
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
	CloseCachedMenu(SkillsPanelWidget);

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
	CloseCachedMenu(SkillsPanelWidget);

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
	// DEBUG (MOCharUI, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] CreateStatusPanel called - status panel is now created on-demand."));
}

void UMOCharacterUIController::TogglePlayerStatus()
{
	// Frame-based debounce: one toggle per frame even if the key reaches this controller twice (added when menus ran in ECommonInputMode::All
	// and both the game action and the menu fired; menus use Menu mode since the May-June rework -- kept as a cheap guard)
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
		CloseCachedMenu(StatusPanelWidget);

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
		CloseCachedMenu(StatusPanelWidget);

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

	// Vitals-derived moodles (wet, etc.) — rebind to the new pawn's
	// VitalsComponent so the HUD strip tracks the live pawn.
	UnbindFromPawnVitalsForMoodles();
	BindToPawnVitalsForMoodles();

	// Survival-stat-derived moodles (hunger / thirst / energy).
	UnbindFromPawnSurvivalStatsForMoodles();
	BindToPawnSurvivalStatsForMoodles();

	// The new pawn's timed actions and illnesses.
	UnbindFromPawnTimedAction();
	BindToPawnTimedAction();
	UnbindFromPawnConditionsForMoodles();
	BindToPawnConditionsForMoodles();
}

// =============================================================================
// Timed body actions (UMOTimedActionComponent)
// =============================================================================

void UMOCharacterUIController::BindToPawnTimedAction()
{
	APlayerController* PC = ResolveOwningPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UMOTimedActionComponent* Action = Pawn ? Pawn->FindComponentByClass<UMOTimedActionComponent>() : nullptr;
	if (!Action)
	{
		return;
	}
	TimedActionChangedHandle = Action->OnStateChanged.AddUObject(this, &UMOCharacterUIController::HandleTimedActionChanged);
	BoundTimedAction = Action;
	if (Action->IsActive())
	{
		HandleTimedActionChanged(Action->GetState());
	}
}

void UMOCharacterUIController::UnbindFromPawnTimedAction()
{
	if (UMOTimedActionComponent* Action = BoundTimedAction.Get())
	{
		Action->OnStateChanged.Remove(TimedActionChangedHandle);
	}
	TimedActionChangedHandle.Reset();
	BoundTimedAction.Reset();
	TearDownTimedActionProgressWidget();
}

void UMOCharacterUIController::HandleTimedActionChanged(const FMOTimedActionState& State)
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}
	if (!State.bActive)
	{
		TearDownTimedActionProgressWidget();
		return;
	}
	TSubclassOf<UMOProgressWidgetBase> WidgetClass = TimedActionProgressWidgetClass ? TimedActionProgressWidgetClass : TerraformProgressWidgetClass;
	if (!WidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharUI] No progress widget class for timed actions (TimedActionProgressWidgetClass / TerraformProgressWidgetClass): '%s' runs without a bar"),
			*State.Label.ToString());
		return;
	}
	TearDownTimedActionProgressWidget();
	UMOProgressWidgetBase* Widget = Cast<UMOProgressWidgetBase>(PushWidgetToLayer(MOUILayerTags::Layer_GameOverlay, WidgetClass));
	if (!Widget)
	{
		return;
	}
	TimedActionProgressWidget = Widget;
	UMOUIManagerComponent::RegisterHUDElement(Widget, EMOHUDElementKind::Feedback);
	Widget->OnCancelled.RemoveDynamic(this, &UMOCharacterUIController::HandleTimedActionWidgetCancelled);
	Widget->OnCancelled.AddDynamic(this, &UMOCharacterUIController::HandleTimedActionWidgetCancelled);
	// The label and length, then the component's own clock drives the values (SetProgress turns the widget's own timer off).
	Widget->StartProgress(State.Label, State.RealSeconds);
	UpdateTimedActionProgress();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimedActionProgressTimer, this, &UMOCharacterUIController::UpdateTimedActionProgress, 0.05f, true);
	}
}

void UMOCharacterUIController::UpdateTimedActionProgress()
{
	UMOProgressWidgetBase* Widget = TimedActionProgressWidget.Get();
	const UMOTimedActionComponent* Action = BoundTimedAction.Get();
	if (!Widget || !Action || !Action->IsActive())
	{
		return;
	}
	Widget->SetProgress(Action->GetProgress01(), Action->GetRemainingRealSeconds());
}

void UMOCharacterUIController::HandleTimedActionWidgetCancelled()
{
	if (UMOTimedActionComponent* Action = BoundTimedAction.Get())
	{
		Action->ServerCancel();
	}
}

void UMOCharacterUIController::TearDownTimedActionProgressWidget()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimedActionProgressTimer);
	}
	UMOProgressWidgetBase* Widget = TimedActionProgressWidget.Get();
	TimedActionProgressWidget.Reset();
	if (!IsValid(Widget))
	{
		return;
	}
	Widget->OnCancelled.RemoveDynamic(this, &UMOCharacterUIController::HandleTimedActionWidgetCancelled);
	Widget->DeactivateWidget();
	if (Widget->IsInViewport())
	{
		Widget->RemoveFromParent();
	}
}

// =============================================================================
// Illness moodles (symptomatic conditions)
// =============================================================================

void UMOCharacterUIController::BindToPawnConditionsForMoodles()
{
	APlayerController* PC = ResolveOwningPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UMOAnatomyComponent* Anatomy = Pawn ? Pawn->FindComponentByClass<UMOAnatomyComponent>() : nullptr;
	if (!Anatomy)
	{
		return;
	}
	ConditionsChangedHandle = Anatomy->OnConditionsChanged.AddUObject(this, &UMOCharacterUIController::RefreshConditionMoodles);
	BoundAnatomy = Anatomy;
	RefreshConditionMoodles();
}

void UMOCharacterUIController::UnbindFromPawnConditionsForMoodles()
{
	if (UMOAnatomyComponent* Anatomy = BoundAnatomy.Get())
	{
		Anatomy->OnConditionsChanged.Remove(ConditionsChangedHandle);
	}
	ConditionsChangedHandle.Reset();
	BoundAnatomy.Reset();
	RefreshConditionMoodles();   // a new pawn starts with none of the old one's
}

void UMOCharacterUIController::RefreshConditionMoodles()
{
	UMOUIManagerComponent* UIManager = GetUIManager();
	UMOHUDRootWidget* HUDRoot = UIManager ? UIManager->GetHUDRoot() : nullptr;
	UMOStatusEffectStripWidget* Strip = HUDRoot ? HUDRoot->GetStatusStrip() : nullptr;
	if (!Strip)
	{
		return;
	}
	const UMOAnatomyComponent* Anatomy = BoundAnatomy.Get();
	const UMOMedicalSubsystem* Medical = GetWorld() && GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<UMOMedicalSubsystem>() : nullptr;
	TSet<FName> Wanted;
	if (Anatomy && Medical)
	{
		for (const FMOCondition& Condition : Anatomy->GetConditions())
		{
			FMOConditionDefinitionRow Def;
			// Nothing shows while an illness incubates: the player has no way to know yet.
			if (!Condition.IsSymptomatic() || Condition.Severity < 1.0f || !Medical->GetConditionDefinition(Condition.ConditionType, Def) || !Def.bShowsMoodle)
			{
				continue;
			}
			FMOStatusMoodle M;
			M.Id = FName(*FString::Printf(TEXT("Condition_%s"), *UEnum::GetValueAsString(Condition.ConditionType).RightChop(FCString::Strlen(TEXT("EMOConditionType::")))));
			M.Label = Def.MoodleLabel.IsEmpty() ? Def.DisplayName : Def.MoodleLabel;
			M.Tooltip = Def.MoodleTooltip.IsEmpty() ? Def.Description : Def.MoodleTooltip;
			M.Icon = Def.Icon.LoadSynchronous();   // the condition's own glyph when one is authored; none otherwise (never another moodle's)
			M.Level = Condition.Severity >= 45.0f ? 1 : 0;
			M.Severity = Condition.Severity >= 45.0f ? EMOStatusMoodleSeverity::Critical : EMOStatusMoodleSeverity::Warning;
			Strip->AddOrUpdateMoodle(M);
			Wanted.Add(M.Id);
		}
	}
	for (const FName& Id : ShownConditionMoodles)
	{
		if (!Wanted.Contains(Id))
		{
			Strip->RemoveMoodle(Id);
		}
	}
	ShownConditionMoodles = MoveTemp(Wanted);
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
	StartItemInspectionAt(ItemDefinitionId, ItemGuid, FMOInstanceRef());
}

void UMOCharacterUIController::StartItemInspectionAt(FName ItemDefinitionId, const FGuid& ItemGuid, const FMOInstanceRef& Instance)
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}
	AMOPlayerController* PlayerController = Cast<AMOPlayerController>(ResolveOwningPlayerController());
	if (!IsValid(PlayerController))
	{
		return;
	}
	if (!IsValid(PlayerController->GetPawn()))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharUI] StartItemInspection - No pawn to inspect with"));
		ShowNoPawnNotification();
		return;
	}
	// The server owns the study, from the host and a joiner alike (FMOInspectionActions): it checks the item is at hand, times it on the pawn (the
	// timed-action bar shows it here; moving or being hurt ends it) and inspects on its own copy, so what the player learns is the server's and is
	// saved. (Until Oct 18 2026 a progress widget's UI timer inspected on THIS machine: a joiner's learning lived only in the joiner's copy.)
	// Inspection does not close the inventory: the player can keep looking at other items while it runs.
	PlayerController->ServerInspectItem(ItemDefinitionId, ItemGuid, Instance);
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Asked the server to inspect '%s' (GUID: %s, at: %s)"), *ItemDefinitionId.ToString(),
		*ItemGuid.ToString(EGuidFormats::DigitsWithHyphens), *Instance.ToString());
}

void UMOCharacterUIController::CancelItemInspection()
{
	const APlayerController* PC = ResolveOwningPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UMOTimedActionComponent* Timed = Pawn ? Pawn->FindComponentByClass<UMOTimedActionComponent>() : nullptr;
	if (Timed && Timed->IsActive() && Timed->GetState().ActionId == FMOInspectionActions::TimedActionId)
	{
		Timed->ServerCancel();
	}
}

bool UMOCharacterUIController::IsInspectionInProgress() const
{
	const APlayerController* PC = ResolveOwningPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UMOTimedActionComponent* Timed = Pawn ? Pawn->FindComponentByClass<UMOTimedActionComponent>() : nullptr;
	return Timed && Timed->IsActive() && Timed->GetState().ActionId == FMOInspectionActions::TimedActionId;
}

void UMOCharacterUIController::ShowInspectionResult(FName ItemDefinitionId, const FMOInspectionResult& Result)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Inspection of '%s' finished: Success=%s, XPGrants=%d"), *ItemDefinitionId.ToString(),
		Result.bSuccess ? TEXT("true") : TEXT("false"), Result.XPGrants.Num());
	if (!Result.bSuccess)
	{
		return;
	}
	UMONotificationComponent* NotificationComp = GetNotificationComponent();

	// Cycle through showing popup for each affected entry (both skills and knowledge)
	// Each entry gets its own popup that displays sequentially
	for (const FMOInspectionXPGrant& Grant : Result.XPGrants)
	{
		if (Grant.XPAmount > 0.0f && NotificationComp)
		{
			// One toast per grant, saying what it gave ("Woodworking +2 XP", "New knowledge: Wood Properties"); knowledge as knowledge.
			NotificationComp->ShowSkillToast(Grant.Id, Grant.bIsKnowledge, Grant.XPAmount, Grant.LevelBefore, Grant.LevelAfter, 3.0f, Grant.CurrentXPAfter,
				Grant.XPToNextLevelAfter);

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

	// Fire quest events for the inspection (the quest log is this player's machine's)
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UMOQuestSubsystem* QuestSub = GI->GetSubsystem<UMOQuestSubsystem>())
			{
				// Fire both a generic "ItemInspected" event and an item-specific event
				QuestSub->FireGameEvent(FName(TEXT("ItemInspected")));
				QuestSub->FireGameEvent(FName(*FString::Printf(TEXT("Inspected_%s"), *ItemDefinitionId.ToString())));
			}
		}
	}
}

// =============================================================================
// Vitals-Derived Moodles (Wet, future bleeding/etc.)
// =============================================================================

void UMOCharacterUIController::BindToPawnVitalsForMoodles()
{
	APlayerController* PC = ResolveOwningPlayerController();
	if (!IsValid(PC))
	{
		return;
	}

	APawn* Pawn = PC->GetPawn();
	if (!IsValid(Pawn))
	{
		return;
	}

	UMOVitalsComponent* Vitals = Pawn->FindComponentByClass<UMOVitalsComponent>();
	if (!IsValid(Vitals))
	{
		return;
	}

	// AddUnique pattern via Remove-then-Add for dynamic delegates.
	Vitals->OnWetnessStateChanged.RemoveDynamic(this, &UMOCharacterUIController::HandleWetnessStateChanged);
	Vitals->OnWetnessStateChanged.AddDynamic(this, &UMOCharacterUIController::HandleWetnessStateChanged);

	Vitals->OnThermalComfortChanged.RemoveDynamic(this, &UMOCharacterUIController::HandleThermalComfortChanged);
	Vitals->OnThermalComfortChanged.AddDynamic(this, &UMOCharacterUIController::HandleThermalComfortChanged);

	BoundVitalsComponent = Vitals;

	// Push current state immediately — don't wait for the next transition
	// (player may have respawned soaked + freezing).
	RefreshWetMoodle(Vitals->GetWetnessState());
	RefreshThermalMoodle(Vitals->GetThermalComfort());

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOCharUI] Bound to vitals for moodles on '%s' (wet=%s, thermal=%s)"),
		*Pawn->GetName(),
		*UEnum::GetValueAsString(Vitals->GetWetnessState()),
		*UEnum::GetValueAsString(Vitals->GetThermalComfort()));
}

void UMOCharacterUIController::UnbindFromPawnVitalsForMoodles()
{
	if (UMOVitalsComponent* Vitals = BoundVitalsComponent.Get())
	{
		Vitals->OnWetnessStateChanged.RemoveDynamic(this, &UMOCharacterUIController::HandleWetnessStateChanged);
		Vitals->OnThermalComfortChanged.RemoveDynamic(this, &UMOCharacterUIController::HandleThermalComfortChanged);
	}
	BoundVitalsComponent.Reset();

	// Remove any previously-pushed moodles on pawn-swap so the new pawn's
	// state can replace them cleanly.
	UMOUIManagerComponent* UIManager = GetUIManager();
	UMOHUDRootWidget* HUDRoot = UIManager ? UIManager->GetHUDRoot() : nullptr;
	if (UMOStatusEffectStripWidget* Strip = HUDRoot ? HUDRoot->GetStatusStrip() : nullptr)
	{
		Strip->RemoveMoodle(FName(TEXT("Wet")));
		Strip->RemoveMoodle(FName(TEXT("Thermal")));
	}
}

void UMOCharacterUIController::HandleWetnessStateChanged(EMOWetnessState OldState, EMOWetnessState NewState)
{
	UE_LOG(LogMOFramework, Log,
		TEXT("[MOCharUI] Wetness %s -> %s"),
		*UEnum::GetValueAsString(OldState),
		*UEnum::GetValueAsString(NewState));
	RefreshWetMoodle(NewState);
}

void UMOCharacterUIController::HandleThermalComfortChanged(EMOThermalComfort OldComfort, EMOThermalComfort NewComfort)
{
	UE_LOG(LogMOFramework, Log,
		TEXT("[MOCharUI] ThermalComfort %s -> %s"),
		*UEnum::GetValueAsString(OldComfort),
		*UEnum::GetValueAsString(NewComfort));
	RefreshThermalMoodle(NewComfort);
}

void UMOCharacterUIController::RefreshThermalMoodle(EMOThermalComfort Comfort)
{
	EnsureDefaultMoodleIcons();
	UMOUIManagerComponent* UIManager = GetUIManager();
	UMOHUDRootWidget* HUDRoot = UIManager ? UIManager->GetHUDRoot() : nullptr;
	UMOStatusEffectStripWidget* Strip = HUDRoot ? HUDRoot->GetStatusStrip() : nullptr;
	if (!Strip) return;

	const FName ThermalMoodleId(TEXT("Thermal"));

	// Comfortable = no moodle. The strip is for "things to react to," and
	// being comfortable isn't one of them. Player can infer from the
	// absence of cold/hot moodles that they're fine.
	if (Comfort == EMOThermalComfort::Comfortable)
	{
		Strip->RemoveMoodle(ThermalMoodleId);
		return;
	}

	FMOStatusMoodle M;
	M.Id = ThermalMoodleId;
	// Use enum value directly as the icon index — designers author 5
	// textures, ThermalMoodleIcons[0]..[4] = VeryCold..VeryHot. Index 2
	// (Comfortable) is unused but kept in the array so the indexing stays
	// 1:1 with the enum.
	M.Level = static_cast<int32>(Comfort);
	M.Icon = PickIconForLevel(ThermalMoodleIcons, M.Level);

	switch (Comfort)
	{
	case EMOThermalComfort::VeryCold:
		M.Label = NSLOCTEXT("MO", "Moodle.VeryCold", "Freezing");
		M.Tooltip = NSLOCTEXT("MO", "Moodle.VeryCold.Tip",
			"Severe hypothermia. Find heat now — performance is degrading, danger of death.");
		M.Severity = EMOStatusMoodleSeverity::Critical;
		break;

	case EMOThermalComfort::Cold:
		M.Label = NSLOCTEXT("MO", "Moodle.Cold", "Cold");
		M.Tooltip = NSLOCTEXT("MO", "Moodle.Cold.Tip",
			"You're cold. Build a fire, find shelter, or get drier clothing.");
		M.Severity = EMOStatusMoodleSeverity::Warning;
		break;

	case EMOThermalComfort::Hot:
		M.Label = NSLOCTEXT("MO", "Moodle.Hot", "Hot");
		M.Tooltip = NSLOCTEXT("MO", "Moodle.Hot.Tip",
			"You're overheating. Seek shade, water, and rest.");
		M.Severity = EMOStatusMoodleSeverity::Warning;
		break;

	case EMOThermalComfort::VeryHot:
		M.Label = NSLOCTEXT("MO", "Moodle.VeryHot", "Heatstroke");
		M.Tooltip = NSLOCTEXT("MO", "Moodle.VeryHot.Tip",
			"Severe hyperthermia. Cool down now — performance is degrading, danger of death.");
		M.Severity = EMOStatusMoodleSeverity::Critical;
		break;

	default:
		// Comfortable handled above; new enum values would fall through here.
		return;
	}

	Strip->AddOrUpdateMoodle(M);
}

void UMOCharacterUIController::EnsureDefaultMoodleIcons()
{
	if (bDefaultMoodleIconsEnsured)
	{
		return;
	}
	bDefaultMoodleIconsEnsured = true;
	auto Fill = [](TArray<TObjectPtr<UTexture2D>>& Icons, std::initializer_list<const TCHAR*> Names)
	{
		if (Icons.Num() > 0)
		{
			return;   // a designer authored this one
		}
		for (const TCHAR* Name : Names)
		{
			Icons.Add(UMOUITokens::Icon(FName(*FString::Printf(TEXT("ic_moodle_%s"), Name))));   // the kit glyph set (white on transparent); null if missing: that level stays label-only
		}
	};
	Fill(ThermalMoodleIcons, { TEXT("very_cold"), TEXT("cold"), TEXT("cold"), TEXT("hot"), TEXT("very_hot") });   // [2] Comfortable is unused (no moodle then)
	Fill(WetMoodleIcons, { TEXT("damp"), TEXT("wet"), TEXT("soaked") });
	Fill(HungerMoodleIcons, { TEXT("hungry"), TEXT("starving") });
	Fill(ThirstMoodleIcons, { TEXT("thirsty"), TEXT("dehydrated") });
	Fill(EnergyMoodleIcons, { TEXT("tired"), TEXT("exhausted") });
}

UTexture2D* UMOCharacterUIController::PickIconForLevel(const TArray<TObjectPtr<UTexture2D>>& Icons, int32 Level)
{
	if (Icons.Num() == 0)
	{
		return nullptr;
	}
	const int32 Clamped = FMath::Clamp(Level, 0, Icons.Num() - 1);
	return Icons[Clamped];
}

void UMOCharacterUIController::RefreshWetMoodle(EMOWetnessState State)
{
	EnsureDefaultMoodleIcons();
	UMOUIManagerComponent* UIManager = GetUIManager();
	UMOHUDRootWidget* HUDRoot = UIManager ? UIManager->GetHUDRoot() : nullptr;
	UMOStatusEffectStripWidget* Strip = HUDRoot ? HUDRoot->GetStatusStrip() : nullptr;
	if (!Strip)
	{
		// HUD strip not present in WBP — silent no-op. The diagnostic dump
		// in MOHUDRootWidget::NativeOnInitialized already complains if the
		// designer forgot to add it.
		return;
	}

	const FName WetMoodleId(TEXT("Wet"));

	if (State == EMOWetnessState::Dry)
	{
		Strip->RemoveMoodle(WetMoodleId);
		return;
	}

	// Damp / Wet / Soaked — show or update the same moodle slot. Label +
	// severity + icon ratchet up as the player gets wetter; designers
	// author one texture per Level in WetMoodleIcons.
	FMOStatusMoodle M;
	M.Id = WetMoodleId;

	switch (State)
	{
	case EMOWetnessState::Damp:
		M.Level = 0;
		M.Label = NSLOCTEXT("MO", "Moodle.Damp", "Damp");
		M.Tooltip = NSLOCTEXT("MO", "Moodle.Damp.Tooltip",
			"You're slightly damp. Your body will lose heat faster in cold weather.");
		M.Severity = EMOStatusMoodleSeverity::Info;
		break;

	case EMOWetnessState::Wet:
		M.Level = 1;
		M.Label = NSLOCTEXT("MO", "Moodle.Wet", "Wet");
		M.Tooltip = NSLOCTEXT("MO", "Moodle.Wet.Tooltip",
			"You're wet. Significant heat loss in cold; fire-starting is harder.");
		M.Severity = EMOStatusMoodleSeverity::Warning;
		break;

	case EMOWetnessState::Soaked:
		M.Level = 2;
		M.Label = NSLOCTEXT("MO", "Moodle.Soaked", "Soaked");
		M.Tooltip = NSLOCTEXT("MO", "Moodle.Soaked.Tooltip",
			"You're soaked through. Severe heat loss in cold; find shelter and a fire.");
		M.Severity = EMOStatusMoodleSeverity::Critical;
		break;

	default:
		// Dry handled above
		break;
	}

	M.Icon = PickIconForLevel(WetMoodleIcons, M.Level);
	Strip->AddOrUpdateMoodle(M);
}

// =============================================================================
// Survival-Stat Moodles (Hunger / Thirst / Energy)
// =============================================================================
//
// Sources of truth: UMOSurvivalStatsComponent::OnStatChanged. The handler
// reroutes by stat name to a per-stat refresh helper so each moodle has its
// own bucket logic + label text. Energy is a placeholder for now (added
// to test the pattern; the stat exists but doesn't drive interesting
// gameplay yet).

void UMOCharacterUIController::BindToPawnSurvivalStatsForMoodles()
{
	APlayerController* PC = ResolveOwningPlayerController();
	if (!IsValid(PC))
	{
		return;
	}

	APawn* Pawn = PC->GetPawn();
	if (!IsValid(Pawn))
	{
		return;
	}

	UMOSurvivalStatsComponent* Stats = Pawn->FindComponentByClass<UMOSurvivalStatsComponent>();
	if (!IsValid(Stats))
	{
		return;
	}

	Stats->OnStatChanged.RemoveDynamic(this, &UMOCharacterUIController::HandleSurvivalStatChanged);
	Stats->OnStatChanged.AddDynamic(this, &UMOCharacterUIController::HandleSurvivalStatChanged);
	BoundSurvivalStats = Stats;

	// Push current state immediately — same reasoning as the wet moodle:
	// a respawned-hungry pawn should show the moodle without waiting for
	// the stat to tick down further.
	RefreshSurvivalStatMoodle(FName("Hunger"));
	RefreshSurvivalStatMoodle(FName("Thirst"));
	RefreshSurvivalStatMoodle(FName("Energy"));

	UE_LOG(LogMOFramework, Log,
		TEXT("[MOCharUI] Bound survival-stat moodles on '%s'"), *Pawn->GetName());
}

void UMOCharacterUIController::UnbindFromPawnSurvivalStatsForMoodles()
{
	if (UMOSurvivalStatsComponent* Stats = BoundSurvivalStats.Get())
	{
		Stats->OnStatChanged.RemoveDynamic(this, &UMOCharacterUIController::HandleSurvivalStatChanged);
	}
	BoundSurvivalStats.Reset();

	// Drop any moodles we placed so the next pawn's state replaces them cleanly.
	UMOUIManagerComponent* UIManager = GetUIManager();
	UMOHUDRootWidget* HUDRoot = UIManager ? UIManager->GetHUDRoot() : nullptr;
	if (UMOStatusEffectStripWidget* Strip = HUDRoot ? HUDRoot->GetStatusStrip() : nullptr)
	{
		Strip->RemoveMoodle(FName(TEXT("Hunger")));
		Strip->RemoveMoodle(FName(TEXT("Thirst")));
		Strip->RemoveMoodle(FName(TEXT("Energy")));
	}
}

void UMOCharacterUIController::HandleSurvivalStatChanged(FName StatName, float /*OldValue*/, float /*NewValue*/)
{
	// Only the three stats we drive moodles for. Other stats (Temperature etc.)
	// are handled by their own dedicated indicators.
	if (StatName == FName("Hunger") ||
	    StatName == FName("Thirst") ||
	    StatName == FName("Energy"))
	{
		RefreshSurvivalStatMoodle(StatName);
	}
}

void UMOCharacterUIController::RefreshSurvivalStatMoodle(FName StatName)
{
	EnsureDefaultMoodleIcons();
	UMOSurvivalStatsComponent* Stats = BoundSurvivalStats.Get();
	if (!Stats) return;

	UMOUIManagerComponent* UIManager = GetUIManager();
	UMOHUDRootWidget* HUDRoot = UIManager ? UIManager->GetHUDRoot() : nullptr;
	UMOStatusEffectStripWidget* Strip = HUDRoot ? HUDRoot->GetStatusStrip() : nullptr;
	if (!Strip) return;

	const float Percent = Stats->GetStatPercent(StatName);

	// Above the warning threshold → no moodle (player is fine on this stat).
	if (Percent >= SurvivalMoodleWarningPercent)
	{
		Strip->RemoveMoodle(StatName);
		return;
	}

	// Below warning → push a moodle. Severity + label + icon level depend
	// on which stat AND how deep into the warning band we are.
	FMOStatusMoodle M;
	M.Id = StatName;

	const bool bCritical = Percent < SurvivalMoodleCriticalPercent;
	M.Level = bCritical ? 1 : 0;  // 0 = warning, 1 = critical
	M.Severity = bCritical ? EMOStatusMoodleSeverity::Critical : EMOStatusMoodleSeverity::Warning;

	if (StatName == FName("Hunger"))
	{
		M.Icon = PickIconForLevel(HungerMoodleIcons, M.Level);
		M.Label = bCritical
			? NSLOCTEXT("MO", "Moodle.Starving",      "Starving")
			: NSLOCTEXT("MO", "Moodle.Hungry",        "Hungry");
		M.Tooltip = bCritical
			? NSLOCTEXT("MO", "Moodle.Starving.Tip",  "You're starving. Find food immediately — extended hunger causes serious damage.")
			: NSLOCTEXT("MO", "Moodle.Hungry.Tip",    "You're hungry. Eat soon to maintain strength and stamina.");
	}
	else if (StatName == FName("Thirst"))
	{
		M.Icon = PickIconForLevel(ThirstMoodleIcons, M.Level);
		M.Label = bCritical
			? NSLOCTEXT("MO", "Moodle.Dehydrated",    "Dehydrated")
			: NSLOCTEXT("MO", "Moodle.Thirsty",       "Thirsty");
		M.Tooltip = bCritical
			? NSLOCTEXT("MO", "Moodle.Dehydrated.Tip","Severe dehydration. Drink water now — fatal within hours.")
			: NSLOCTEXT("MO", "Moodle.Thirsty.Tip",   "You're thirsty. Find water soon.");
	}
	else if (StatName == FName("Energy"))
	{
		M.Icon = PickIconForLevel(EnergyMoodleIcons, M.Level);
		M.Label = bCritical
			? NSLOCTEXT("MO", "Moodle.Exhausted",     "Exhausted")
			: NSLOCTEXT("MO", "Moodle.Tired",         "Tired");
		M.Tooltip = bCritical
			? NSLOCTEXT("MO", "Moodle.Exhausted.Tip", "Exhausted — sleep, your performance is suffering.")
			: NSLOCTEXT("MO", "Moodle.Tired.Tip",     "You're tired. Rest soon.");
	}
	else
	{
		// Unknown stat — shouldn't happen given the gate in HandleSurvivalStatChanged,
		// but bail rather than push a garbage moodle.
		return;
	}

	Strip->AddOrUpdateMoodle(M);
}
