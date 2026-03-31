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
	}
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

	// Create widget if needed
	UMOSkillsPanel* PanelWidget = SkillsPanelWidget.Get();
	if (!PanelWidget)
	{
		PanelWidget = CreateWidget<UMOSkillsPanel>(PlayerController, SkillsPanelClass);
		if (!PanelWidget)
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOCharUI] Failed to create Skills Panel widget."));
			return;
		}

		SkillsPanelWidget = PanelWidget;
		PanelWidget->OnRequestClose.AddDynamic(this, &UMOCharacterUIController::HandleSkillsPanelRequestClose);
	}

	// Get skills and knowledge components from cache
	UMOSkillsComponent* Skills = GetCachedSkills();
	UMOKnowledgeComponent* Knowledge = GetCachedKnowledge();

	// Initialize with both components
	PanelWidget->InitializePanelWithKnowledge(Skills, Knowledge);

	if (!PanelWidget->IsInViewport())
	{
		ShowModalBackground();
		PushWidgetInstanceToLayer(MOUILayerTags::Layer_Menu, PanelWidget, SkillsPanelZOrder);
	}

	ApplyInputModeForMenuOpen(PanelWidget);
	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Skills Panel opened"));
}

void UMOCharacterUIController::CloseSkillsPanel()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();

	UMOSkillsPanel* PanelWidget = SkillsPanelWidget.Get();
	if (IsValid(PanelWidget))
	{
		if (PanelWidget->IsInViewport())
		{
			PanelWidget->RemoveFromParent();
		}
	}

	UpdateReticleVisibility();

	// Only restore input mode if no other menus are open
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed();
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Skills Panel closed"));
}

bool UMOCharacterUIController::IsSkillsPanelOpen() const
{
	UMOSkillsPanel* PanelWidget = SkillsPanelWidget.Get();
	return PanelWidget && PanelWidget->IsInViewport();
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
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	if (!StatusPanelClass)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] StatusPanelClass not set, skipping status panel creation."));
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (StatusPanelWidget.IsValid())
	{
		return;
	}

	UMOStatusPanel* NewStatus = CreateWidget<UMOStatusPanel>(PlayerController, StatusPanelClass);
	if (!IsValid(NewStatus))
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOCharUI] Failed to create status panel widget."));
		return;
	}

	StatusPanelWidget = NewStatus;

	// Add to viewport but hidden
	NewStatus->AddToViewport(StatusPanelZOrder);
	NewStatus->SetVisibility(ESlateVisibility::Collapsed);

	// Bind close request delegate
	NewStatus->OnRequestClose.AddDynamic(this, &UMOCharacterUIController::HandleStatusPanelRequestClose);

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharUI] Status panel widget created (hidden by default)."));
}

void UMOCharacterUIController::TogglePlayerStatus()
{
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
	UMOStatusPanel* Status = StatusPanelWidget.Get();
	if (!IsValid(Status))
	{
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();

	if (bVisible)
	{
		// Check for valid pawn first
		if (!HasValidPawn())
		{
			ShowNoPawnNotification();
			return;
		}

		bStatusPanelVisible = true;

		// Bind to current pawn's medical components before showing
		RebindStatusPanelToCurrentPawn();

		ShowModalBackground();
		Status->SetVisibility(ESlateVisibility::Visible);

		// Set input mode for menu interaction
		if (IsValid(PlayerController))
		{
			ApplyInputModeForMenuOpen(Status);
		}

		UpdateReticleVisibility();
	}
	else
	{
		bStatusPanelVisible = false;

		Status->SetVisibility(ESlateVisibility::Collapsed);

		UpdateReticleVisibility();

		// Restore game input mode if no other menus open
		if (!IsAnyMenuOpen())
		{
			HideModalBackground();
			if (IsValid(PlayerController))
			{
				ApplyInputModeForMenuClosed();
			}
		}
	}
}

bool UMOCharacterUIController::IsPlayerStatusVisible() const
{
	return bStatusPanelVisible;
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

	// Close inventory menu while inspecting (via UIManager)
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		UIManager->CloseInventoryMenu();
	}

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
	if (!IsValid(InspectionWidget))
	{
		InspectionWidget = CreateWidget<UMOInspectionProgressWidget>(PlayerController, InspectionProgressWidgetClass);
		if (!IsValid(InspectionWidget))
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOCharUI] Failed to create inspection widget"));
			return;
		}

		InspectionProgressWidget = InspectionWidget;

		// Bind delegates (remove first to avoid duplicates)
		InspectionWidget->OnInspectionCompleted.RemoveDynamic(this, &UMOCharacterUIController::HandleInspectionCompleted);
		InspectionWidget->OnInspectionCancelled.RemoveDynamic(this, &UMOCharacterUIController::HandleInspectionCancelled);
		InspectionWidget->OnInspectionCompleted.AddDynamic(this, &UMOCharacterUIController::HandleInspectionCompleted);
		InspectionWidget->OnInspectionCancelled.AddDynamic(this, &UMOCharacterUIController::HandleInspectionCancelled);
	}

	// Store the item being inspected
	InspectingItemGuid = ItemGuid;

	// Show the widget via layer system
	PushWidgetInstanceToLayer(MOUILayerTags::Layer_GameOverlay, InspectionWidget, InspectionProgressZOrder);

	// Set up input mode for inspection (game and UI to allow ESC to cancel)
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(InspectionWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(true);

	// Start the inspection
	InspectionWidget->StartInspection(ItemDefinitionId, ItemDisplayName, KnowledgeComp, SkillsComp, InspectionDuration);

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

	// Remove widget from viewport
	UMOInspectionProgressWidget* InspectionWidget = InspectionProgressWidget.Get();
	if (IsValid(InspectionWidget))
	{
		InspectionWidget->RemoveFromParent();
	}

	// Clear inspecting item
	InspectingItemGuid.Invalidate();

	// Only restore input mode if no other menus are open
	if (!IsAnyMenuOpen())
	{
		APlayerController* PlayerController = ResolveOwningPlayerController();
		if (IsValid(PlayerController))
		{
			ApplyInputModeForMenuClosed();
		}
	}

	UpdateReticleVisibility();

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

	// Remove widget from viewport
	UMOInspectionProgressWidget* InspectionWidget = InspectionProgressWidget.Get();
	if (IsValid(InspectionWidget))
	{
		InspectionWidget->RemoveFromParent();
	}

	// Clear inspecting item
	InspectingItemGuid.Invalidate();

	// Only restore input mode if no other menus are open
	if (!IsAnyMenuOpen())
	{
		APlayerController* PlayerController = ResolveOwningPlayerController();
		if (IsValid(PlayerController))
		{
			ApplyInputModeForMenuClosed();
		}
	}

	UpdateReticleVisibility();
}
