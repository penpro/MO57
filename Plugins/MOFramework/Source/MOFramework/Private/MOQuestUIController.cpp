#include "MOQuestUIController.h"
#include "MOFramework.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#include "MOUIManagerComponent.h"
#include "MOQuestLogPanel.h"
#include "MOQuestHUDWidget.h"
#include "MOQuestSubsystem.h"
#include "MOPrimaryGameLayout.h"
#include "MOTutorialHintWidget.h"

UMOQuestUIController::UMOQuestUIController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMOQuestUIController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	// Event-driven: wait for UMOQuestSubsystem::OnQuestSystemReady before
	// spawning the HUD/tutorial widgets. Even if the GameInstance subsystem
	// happens to be ready by the time BeginPlay fires today (synchronous
	// DataTable load), depending on that ordering is exactly the kind of
	// implicit race we just stopped doing for voxel readiness. If the
	// definitions ever move to async load, the BeginPlay path silently breaks.
	//
	// Same-frame fallback: if the subsystem already finished loading before
	// we got here, IsReady() returns true and we skip the wait entirely.
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UMOQuestSubsystem* QuestSub = GI ? GI->GetSubsystem<UMOQuestSubsystem>() : nullptr;
	if (!IsValid(QuestSub))
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOQuestUI] Quest subsystem unavailable at BeginPlay — quest widgets will not be created"));
		return;
	}

	if (QuestSub->IsReady())
	{
		SpawnReadyTimeWidgets();
		return;
	}

	QuestSub->OnQuestSystemReady.RemoveDynamic(this, &UMOQuestUIController::HandleQuestSystemReady);
	QuestSub->OnQuestSystemReady.AddDynamic(this, &UMOQuestUIController::HandleQuestSystemReady);
}

void UMOQuestUIController::HandleQuestSystemReady()
{
	SpawnReadyTimeWidgets();
}

void UMOQuestUIController::SpawnReadyTimeWidgets()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	if (bCreateQuestHUDOnBeginPlay && QuestHUDWidgetClass && !QuestHUDWidget.IsValid())
	{
		CreateQuestHUD();
	}
	if (bCreateTutorialHintOnBeginPlay && TutorialHintWidgetClass && !TutorialHintWidget.IsValid())
	{
		CreateTutorialHintWidget();
	}
}

void UMOQuestUIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unbind from quest subsystem readiness signal. Safe even if BeginPlay
	// took the IsReady() short-circuit path and never subscribed.
	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UMOQuestSubsystem* QuestSub = GI->GetSubsystem<UMOQuestSubsystem>())
		{
			QuestSub->OnQuestSystemReady.RemoveDynamic(this, &UMOQuestUIController::HandleQuestSystemReady);
		}
	}

	// Clean up quest log panel widget - unbind delegates first
	if (UMOQuestLogPanel* QuestWidget = QuestLogPanelWidget.Get())
	{
		QuestWidget->OnCloseRequested.RemoveDynamic(this, &UMOQuestUIController::HandleQuestLogRequestClose);
		if (QuestWidget->IsInViewport())
		{
			QuestWidget->RemoveFromParent();
		}
	}
	QuestLogPanelWidget.Reset();

	// Clean up quest HUD widget
	if (UMOQuestHUDWidget* HUDWidget = QuestHUDWidget.Get())
	{
		if (HUDWidget->IsInViewport())
		{
			HUDWidget->RemoveFromParent();
		}
	}
	QuestHUDWidget.Reset();

	// Clean up tutorial hint widget
	if (UMOTutorialHintWidget* HintWidget = TutorialHintWidget.Get())
	{
		if (HintWidget->IsInViewport())
		{
			HintWidget->RemoveFromParent();
		}
	}
	TutorialHintWidget.Reset();

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// Quest Log Panel
// =============================================================================

void UMOQuestUIController::ToggleQuestLog()
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

	// If quest log is already open, just close it
	if (IsQuestLogOpen())
	{
		CloseQuestLog();
		return;
	}

	// Close all switchable menus and open quest log
	if (UIManager)
	{
		UIManager->CloseAllSwitchableMenus();
	}
	OpenQuestLog();
}

void UMOQuestUIController::OpenQuestLog()
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

	if (!QuestLogPanelClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQuestUI] QuestLogPanelClass not set on QuestUIController."));
		return;
	}

	// Close any existing panel first
	UMOQuestLogPanel* ExistingPanel = QuestLogPanelWidget.Get();
	if (IsValid(ExistingPanel) && ExistingPanel->IsActivated())
	{
		PopWidgetFromLayer(ExistingPanel);
		QuestLogPanelWidget.Reset();
	}

	// Create new widget via CommonUI layer stack
	ShowModalBackground();
	UCommonActivatableWidget* CreatedWidget = PushWidgetToLayer(MOUILayerTags::Layer_Menu, QuestLogPanelClass);
	UMOQuestLogPanel* PanelWidget = Cast<UMOQuestLogPanel>(CreatedWidget);

	if (!IsValid(PanelWidget))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOQuestUI] Failed to create quest log via layer stack"));
		HideModalBackground();
		return;
	}

	// Cache reference + auto-clear-on-deactivate (any close path). See base class.
	RegisterCachedMenu(PanelWidget, QuestLogPanelWidget);
	PanelWidget->OnCloseRequested.RemoveAll(this);
	PanelWidget->OnCloseRequested.AddDynamic(this, &UMOQuestUIController::HandleQuestLogRequestClose);

	// Refresh the quest list
	PanelWidget->RefreshQuestList();

	// Widget handles input state via NativeOnActivated/GetDesiredInputConfig
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestUI] Quest Log opened"));
}

void UMOQuestUIController::CloseQuestLog()
{
	// IMPORTANT: Get reference before clearing cache
	// Reset cache FIRST to ensure IsQuestLogOpen() returns false immediately
	// This prevents race conditions with toggle input that fires multiple times per frame
	UMOQuestLogPanel* PanelWidget = QuestLogPanelWidget.Get();
	QuestLogPanelWidget.Reset();

	if (IsValid(PanelWidget) && PanelWidget->IsActivated())
	{
		PopWidgetFromLayer(PanelWidget);
	}

	UpdateReticleVisibility();

	// Manage modal background visibility
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestUI] Quest Log closed"));
}

bool UMOQuestUIController::IsQuestLogOpen() const
{
	// Check if we have a cached quest log - if so, panel is either open or opening
	// IMPORTANT: Check IsValid() first, not just IsActivated()
	// CommonUI defers activation, so there's a window where the widget is cached but not yet activated
	UMOQuestLogPanel* PanelWidget = QuestLogPanelWidget.Get();
	return IsValid(PanelWidget);
}

UMOQuestLogPanel* UMOQuestUIController::GetQuestLog() const
{
	return QuestLogPanelWidget.Get();
}

void UMOQuestUIController::HandleQuestLogRequestClose()
{
	CloseQuestLog();
}

// =============================================================================
// Quest HUD Widget
// =============================================================================

void UMOQuestUIController::CreateQuestHUD()
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

	if (!QuestHUDWidgetClass)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOQuestUI] QuestHUDWidgetClass not set, skipping HUD creation."));
		return;
	}

	if (QuestHUDWidget.IsValid())
	{
		return;
	}

	UMOQuestHUDWidget* NewHUD = CreateWidget<UMOQuestHUDWidget>(PlayerController, QuestHUDWidgetClass);
	if (!IsValid(NewHUD))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOQuestUI] Failed to create Quest HUD widget."));
		return;
	}

	QuestHUDWidget = NewHUD;
	NewHUD->AddToViewport(QuestHUDZOrder);
	NewHUD->SetVisibility(ESlateVisibility::HitTestInvisible);

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestUI] Quest HUD widget created."));
}

void UMOQuestUIController::ShowQuestHUD()
{
	if (UMOQuestHUDWidget* HUDWidget = QuestHUDWidget.Get())
	{
		if (!HUDWidget->IsInViewport())
		{
			HUDWidget->AddToViewport(QuestHUDZOrder);
		}
		HUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else if (QuestHUDWidgetClass)
	{
		CreateQuestHUD();
	}
}

void UMOQuestUIController::HideQuestHUD()
{
	if (UMOQuestHUDWidget* HUDWidget = QuestHUDWidget.Get())
	{
		HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UMOQuestUIController::IsQuestHUDVisible() const
{
	const UMOQuestHUDWidget* HUDWidget = QuestHUDWidget.Get();
	if (!IsValid(HUDWidget))
	{
		return false;
	}
	return HUDWidget->GetVisibility() != ESlateVisibility::Collapsed &&
	       HUDWidget->GetVisibility() != ESlateVisibility::Hidden;
}

UMOQuestHUDWidget* UMOQuestUIController::GetQuestHUD() const
{
	return QuestHUDWidget.Get();
}

// =============================================================================
// Tutorial Hint Widget
// =============================================================================

void UMOQuestUIController::CreateTutorialHintWidget()
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

	if (!TutorialHintWidgetClass)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOQuestUI] TutorialHintWidgetClass not set, skipping tutorial hint creation."));
		return;
	}

	if (TutorialHintWidget.IsValid())
	{
		return;
	}

	UMOTutorialHintWidget* NewHint = CreateWidget<UMOTutorialHintWidget>(PlayerController, TutorialHintWidgetClass);
	if (!IsValid(NewHint))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOQuestUI] Failed to create Tutorial Hint widget."));
		return;
	}

	TutorialHintWidget = NewHint;
	NewHint->AddToViewport(TutorialHintZOrder);
	// CommonActivatableWidget controls its own visibility via Activate/Deactivate;
	// we do NOT call SetVisibility here — the widget starts deactivated and
	// activates itself when there's a hint to show.

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestUI] Tutorial hint widget created."));
}
