#include "MOQuestUIController.h"
#include "MOFramework.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"

#include "MOUIManagerComponent.h"
#include "MOQuestLogPanel.h"
#include "MOQuestHUDWidget.h"
#include "MOQuestSubsystem.h"

UMOQuestUIController::UMOQuestUIController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMOQuestUIController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalOwningPlayerController())
	{
		if (bCreateQuestHUDOnBeginPlay && QuestHUDWidgetClass)
		{
			CreateQuestHUD();
		}
	}
}

void UMOQuestUIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

	// Create widget if needed
	UMOQuestLogPanel* PanelWidget = QuestLogPanelWidget.Get();
	if (!PanelWidget)
	{
		PanelWidget = CreateWidget<UMOQuestLogPanel>(PlayerController, QuestLogPanelClass);
		if (!PanelWidget)
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOQuestUI] Failed to create Quest Log Panel widget."));
			return;
		}

		QuestLogPanelWidget = PanelWidget;
		PanelWidget->OnCloseRequested.AddDynamic(this, &UMOQuestUIController::HandleQuestLogRequestClose);
	}

	// Refresh the quest list
	PanelWidget->RefreshQuestList();

	if (!PanelWidget->IsInViewport())
	{
		ShowModalBackground();
		PanelWidget->AddToViewport(QuestLogPanelZOrder);
	}

	ApplyInputModeForMenuOpen(PanelWidget);
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestUI] Quest Log opened"));
}

void UMOQuestUIController::CloseQuestLog()
{
	UMOQuestLogPanel* PanelWidget = QuestLogPanelWidget.Get();
	if (!PanelWidget)
	{
		return;
	}

	if (PanelWidget->IsInViewport())
	{
		PanelWidget->RemoveFromParent();
		HideModalBackground();
	}

	ApplyInputModeForMenuClosed();
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOQuestUI] Quest Log closed"));
}

bool UMOQuestUIController::IsQuestLogOpen() const
{
	UMOQuestLogPanel* PanelWidget = QuestLogPanelWidget.Get();
	return PanelWidget && PanelWidget->IsInViewport();
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
