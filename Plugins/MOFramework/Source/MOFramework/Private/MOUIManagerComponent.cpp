#include "MOUIManagerComponent.h"
#include "MOFramework.h"

#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

#include "MOInventoryComponent.h"
#include "MOInventoryMenu.h"
#include "MOReticleWidget.h"
#include "MOFPSCounterWidget.h"
#include "MOGameSettings.h"
#include "MOInGameMenu.h"
#include "MOItemContextMenu.h"
#include "MOConfirmationDialog.h"
#include "MOPersistenceSubsystem.h"
#include "MOSavePanel.h"
#include "MOLoadPanel.h"
#include "MOSurvivalStatsComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOWorldItem.h"
#include "MOStatusPanel.h"
#include "MOModalBackground.h"
#include "MOVitalsComponent.h"
#include "MOMetabolismComponent.h"
#include "MOMentalStateComponent.h"
#include "MONotificationWidget.h"
#include "MOPossessionMenu.h"
#include "MOPawnEntryWidget.h"
#include "MOPossessionSubsystem.h"
#include "MOIdentityRegistrySubsystem.h"
#include "MOIdentityComponent.h"
#include "MOCraftingMenu.h"
#include "MOSkillsPanel.h"
#include "MOSkillsComponent.h"
#include "MOKnowledgeComponent.h"
#include "MOCraftingQueueComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOInventoryHolderInterface.h"
#include "MOIdentifiableInterface.h"
#include "MOInspectionProgressWidget.h"
#include "MORecipeDatabaseSettings.h"
#include "MONotificationComponent.h"
#include "MOKeepOnHarvestContextMenu.h"
#include "MOHarvestProgressWidget.h"
#include "MOHarvestSubsystem.h"
#include "MOCraftingSubsystem.h"
#include "MOBuildingMenu.h"
#include "MOBuildWidget.h"
#include "MOGhostContextMenu.h"
#include "MOStationContextMenu.h"
#include "MOBuildableActor.h"
#include "MOCraftingStationActor.h"
#include "MOCraftingCapableInterface.h"
#include "MOBuildingComponent.h"
#include "MOBuildProgressComponent.h"
#include "MOPlayerController.h"
#include "MOUnifiedInventoryMenu.h"
#include "MOItemComponent.h"
#include "MOModeIndicatorWidget.h"
#include "MOToolHintWidget.h"
#include "MOInteractorComponent.h"
#include "MOPCGInteractionSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"

// UI Controllers
#include "MOInventoryUIController.h"
#include "MOCraftingUIController.h"
#include "MOBuildingUIController.h"
#include "MOCharacterUIController.h"
#include "MOSystemMenuUIController.h"
#include "MOQuestUIController.h"

UMOUIManagerComponent::UMOUIManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CurrentGameplayMode = EMOGameplayMode::Explore;
}

void UMOUIManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalOwningPlayerController())
	{
		if (bCreateReticleOnBeginPlay)
		{
			CreateReticle();
		}

		if (bCreateModeIndicatorOnBeginPlay)
		{
			CreateModeIndicator();
		}

		if (bCreateToolHintOnBeginPlay)
		{
			CreateToolHint();
		}

		// Always create FPS counter (visibility controlled by settings)
		CreateFPSCounter();

		// Start focus hint timer if enabled
		if (bFocusHintEnabled)
		{
			StartFocusHintTimer();
		}
	}
}

void UMOUIManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Ensure we restore input mode on teardown if this component dies while menu is open.
	CloseAllMenus();

	// Clean up reticle widget
	if (UMOReticleWidget* Reticle = ReticleWidget.Get())
	{
		Reticle->RemoveFromParent();
	}
	ReticleWidget.Reset();

	// Clean up FPS counter widget
	if (UMOFPSCounterWidget* FPSCounter = FPSCounterWidget.Get())
	{
		FPSCounter->RemoveFromParent();
	}
	FPSCounterWidget.Reset();

	// Clean up modal background
	if (UMOModalBackground* Background = ModalBackgroundWidget.Get())
	{
		Background->RemoveFromParent();
	}
	ModalBackgroundWidget.Reset();

	// Clean up no-pawn notification
	HideNoPawnNotification();

	// Stop focus hint timer
	StopFocusHintTimer();

	Super::EndPlay(EndPlayReason);
}

APlayerController* UMOUIManagerComponent::ResolveOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

bool UMOUIManagerComponent::IsLocalOwningPlayerController() const
{
	const APlayerController* PlayerController = ResolveOwningPlayerController();
	return IsValid(PlayerController) && PlayerController->IsLocalController();
}

bool UMOUIManagerComponent::HasValidPawn() const
{
	const APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return false;
	}
	return IsValid(PlayerController->GetPawn());
}

// =============================================================================
// Inventory Menu (Delegated to MOInventoryUIController)
// =============================================================================

bool UMOUIManagerComponent::IsInventoryMenuOpen() const
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		return InvController->IsInventoryMenuOpen();
	}
	return false;
}

UMOInventoryComponent* UMOUIManagerComponent::ResolveCurrentPawnInventoryComponent() const
{
	// Use cached inventory component (populated in CachePawnComponents on possession)
	return GetCachedInventory();
}

void UMOUIManagerComponent::ToggleInventoryMenu()
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		InvController->ToggleInventoryMenu();
	}
}

void UMOUIManagerComponent::OpenInventoryMenu()
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		InvController->OpenInventoryMenu();
	}
}

void UMOUIManagerComponent::CloseInventoryMenu()
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		InvController->CloseInventoryMenu();
	}
}

// =============================================================================
// Unified Inventory Menu / Container Support (Delegated to MOInventoryUIController)
// =============================================================================

void UMOUIManagerComponent::OpenInventoryWithContainer(AActor* ContainerActor)
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		InvController->OpenInventoryWithContainer(ContainerActor);
	}
}

void UMOUIManagerComponent::SetActiveContainer(AActor* ContainerActor)
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		InvController->SetActiveContainer(ContainerActor);
	}
}

void UMOUIManagerComponent::ClearActiveContainer()
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		InvController->ClearActiveContainer();
	}
}

AActor* UMOUIManagerComponent::GetActiveContainer() const
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		return InvController->GetActiveContainer();
	}
	return nullptr;
}

bool UMOUIManagerComponent::HasActiveContainer() const
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		return InvController->HasActiveContainer();
	}
	return false;
}


// =============================================================================
// Nearby World Items (Delegated to MOInventoryUIController)
// =============================================================================

TArray<AMOWorldItem*> UMOUIManagerComponent::QueryNearbyWorldItems() const
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		return InvController->QueryNearbyWorldItems();
	}
	return TArray<AMOWorldItem*>();
}

int32 UMOUIManagerComponent::LootAllNearbyItems()
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		return InvController->LootAllNearbyItems();
	}
	return 0;
}

float UMOUIManagerComponent::GetNearbyItemsQueryRadius() const
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		return InvController->GetNearbyItemsQueryRadius();
	}
	return 400.0f; // Default value
}

void UMOUIManagerComponent::SetNearbyItemsQueryRadius(float NewRadius)
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		InvController->SetNearbyItemsQueryRadius(NewRadius);
	}
}

void UMOUIManagerComponent::ApplyInputModeForMenuOpen(APlayerController* PlayerController, UUserWidget* MenuWidget) const
{
	if (!IsValid(PlayerController) || !IsValid(MenuWidget))
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = bShowMouseCursorWhileMenuOpen;

	if (bLockMovementWhileMenuOpen)
	{
		PlayerController->SetIgnoreMoveInput(true);
	}

	if (bLockLookWhileMenuOpen)
	{
		PlayerController->SetIgnoreLookInput(true);
	}
}

void UMOUIManagerComponent::ApplyInputModeForMenuClosed(APlayerController* PlayerController) const
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	FInputModeGameOnly InputMode;
	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = false;

	PlayerController->SetIgnoreMoveInput(false);
	PlayerController->SetIgnoreLookInput(false);
}

void UMOUIManagerComponent::CreateReticle()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	// Use the configured class or default to UMOReticleWidget
	TSubclassOf<UMOReticleWidget> ClassToUse = ReticleWidgetClass;
	if (!ClassToUse)
	{
		ClassToUse = UMOReticleWidget::StaticClass();
	}

	UMOReticleWidget* NewReticle = CreateWidget<UMOReticleWidget>(PlayerController, ClassToUse);
	if (!IsValid(NewReticle))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to create reticle widget."));
		return;
	}

	ReticleWidget = NewReticle;
	NewReticle->AddToViewport(ReticleZOrder);

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Reticle widget created and added to viewport."));
}

void UMOUIManagerComponent::CreateFPSCounter()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (!FPSCounterWidgetClass)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] FPSCounterWidgetClass not set, FPS counter will not be displayed."));
		return;
	}

	UMOFPSCounterWidget* NewFPSCounter = CreateWidget<UMOFPSCounterWidget>(PlayerController, FPSCounterWidgetClass);
	if (!IsValid(NewFPSCounter))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to create FPS counter widget."));
		return;
	}

	FPSCounterWidget = NewFPSCounter;
	NewFPSCounter->AddToViewport(FPSCounterZOrder);

	// Set initial visibility based on settings
	RefreshFPSCounterVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] FPS counter widget created and added to viewport."));
}

void UMOUIManagerComponent::RefreshFPSCounter()
{
	// Create if needed
	if (!FPSCounterWidget.IsValid() && FPSCounterWidgetClass)
	{
		CreateFPSCounter();
		return;
	}

	RefreshFPSCounterVisibility();
}

void UMOUIManagerComponent::RefreshFPSCounterVisibility()
{
	UMOFPSCounterWidget* FPSCounter = FPSCounterWidget.Get();
	if (!FPSCounter)
	{
		return;
	}

	// Visibility is handled by the widget itself via RefreshVisibility()
	FPSCounter->RefreshVisibility();
}

// =============================================================================
// Status Panel (Delegated to MOCharacterUIController)
// =============================================================================

void UMOUIManagerComponent::TogglePlayerStatus()
{
	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		CharController->TogglePlayerStatus();
	}
}

UMOStatusPanel* UMOUIManagerComponent::GetStatusPanel() const
{
	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		return CharController->GetStatusPanel();
	}
	return nullptr;
}

void UMOUIManagerComponent::SetPlayerStatusVisible(bool bVisible)
{
	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		CharController->SetPlayerStatusVisible(bVisible);
	}
}

bool UMOUIManagerComponent::IsPlayerStatusVisible() const
{
	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		return CharController->IsPlayerStatusVisible();
	}
	return false;
}

void UMOUIManagerComponent::SetReticleVisible(bool bVisible)
{
	UMOReticleWidget* Reticle = ReticleWidget.Get();
	if (!IsValid(Reticle))
	{
		return;
	}

	if (bVisible)
	{
		Reticle->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Reticle->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UMOUIManagerComponent::IsReticleVisible() const
{
	const UMOReticleWidget* Reticle = ReticleWidget.Get();
	if (!IsValid(Reticle))
	{
		return false;
	}

	return Reticle->GetVisibility() != ESlateVisibility::Collapsed && Reticle->GetVisibility() != ESlateVisibility::Hidden;
}

UMOReticleWidget* UMOUIManagerComponent::GetReticleWidget() const
{
	return ReticleWidget.Get();
}

// =============================================================================
// Possession Menu (Delegated to MOSystemMenuUIController)
// =============================================================================

void UMOUIManagerComponent::TogglePossessionMenu()
{
	if (UMOSystemMenuUIController* SysController = GetSystemMenuController())
	{
		SysController->TogglePossessionMenu();
	}
}

void UMOUIManagerComponent::OpenPossessionMenu()
{
	if (UMOSystemMenuUIController* SysController = GetSystemMenuController())
	{
		SysController->OpenPossessionMenu();
	}
}

void UMOUIManagerComponent::ClosePossessionMenu()
{
	if (UMOSystemMenuUIController* SysController = GetSystemMenuController())
	{
		SysController->ClosePossessionMenu();
	}
}

bool UMOUIManagerComponent::IsPossessionMenuOpen() const
{
	if (UMOSystemMenuUIController* SysController = GetSystemMenuController())
	{
		return SysController->IsPossessionMenuOpen();
	}
	return false;
}

void UMOUIManagerComponent::RefreshPossessionMenu()
{
	if (UMOSystemMenuUIController* SysController = GetSystemMenuController())
	{
		SysController->RefreshPossessionMenu();
	}
}

// =============================================================================
// Crafting Menu (Delegated to MOCraftingUIController)
// =============================================================================

void UMOUIManagerComponent::ToggleCraftingMenu()
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		CraftController->ToggleCraftingMenu();
	}
}

void UMOUIManagerComponent::OpenCraftingMenu()
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		CraftController->OpenCraftingMenu();
	}
}

void UMOUIManagerComponent::CloseCraftingMenu()
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		CraftController->CloseCraftingMenu();
	}
}

bool UMOUIManagerComponent::IsCraftingMenuOpen() const
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		return CraftController->IsCraftingMenuOpen();
	}
	return false;
}

UMOCraftingMenu* UMOUIManagerComponent::GetCraftingMenu() const
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		return CraftController->GetCraftingMenu();
	}
	return nullptr;
}

// =============================================================================
// Skills Panel (Delegated to MOCharacterUIController)
// =============================================================================

void UMOUIManagerComponent::ToggleSkillsPanel()
{
	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		CharController->ToggleSkillsPanel();
	}
}

void UMOUIManagerComponent::OpenSkillsPanel()
{
	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		CharController->OpenSkillsPanel();
	}
}

void UMOUIManagerComponent::CloseSkillsPanel()
{
	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		CharController->CloseSkillsPanel();
	}
}

bool UMOUIManagerComponent::IsSkillsPanelOpen() const
{
	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		return CharController->IsSkillsPanelOpen();
	}
	return false;
}

UMOSkillsPanel* UMOUIManagerComponent::GetSkillsPanel() const
{
	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		return CharController->GetSkillsPanel();
	}
	return nullptr;
}

// =============================================================================
// Quest Log (Delegated to MOQuestUIController)
// =============================================================================

void UMOUIManagerComponent::ToggleQuestLog()
{
	if (UMOQuestUIController* QuestController = GetQuestController())
	{
		QuestController->ToggleQuestLog();
	}
}

void UMOUIManagerComponent::OpenQuestLog()
{
	if (UMOQuestUIController* QuestController = GetQuestController())
	{
		QuestController->OpenQuestLog();
	}
}

void UMOUIManagerComponent::CloseQuestLog()
{
	if (UMOQuestUIController* QuestController = GetQuestController())
	{
		QuestController->CloseQuestLog();
	}
}

bool UMOUIManagerComponent::IsQuestLogOpen() const
{
	if (UMOQuestUIController* QuestController = GetQuestController())
	{
		return QuestController->IsQuestLogOpen();
	}
	return false;
}

// =============================================================================
// In-Game Menu (Delegated to MOSystemMenuUIController)
// =============================================================================

void UMOUIManagerComponent::ToggleInGameMenu()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	// If any other menu is open, close it first (orchestration stays in UIManager)
	if (IsInventoryMenuOpen())
	{
		CloseInventoryMenu();
		return;
	}

	if (IsItemContextMenuOpen())
	{
		CloseItemContextMenu();
		return;
	}

	if (IsPlayerStatusVisible())
	{
		SetPlayerStatusVisible(false);
		return;
	}

	if (IsCraftingMenuOpen())
	{
		CloseCraftingMenu();
		return;
	}

	if (IsQuestLogOpen())
	{
		CloseQuestLog();
		return;
	}

	// Delegate the actual toggle to the controller
	if (UMOSystemMenuUIController* SysController = GetSystemMenuController())
	{
		SysController->ToggleInGameMenu();
	}
}

void UMOUIManagerComponent::OpenInGameMenu()
{
	if (UMOSystemMenuUIController* SysController = GetSystemMenuController())
	{
		SysController->OpenInGameMenu();
	}
}

void UMOUIManagerComponent::CloseInGameMenu()
{
	if (UMOSystemMenuUIController* SysController = GetSystemMenuController())
	{
		SysController->CloseInGameMenu();
	}
}

bool UMOUIManagerComponent::IsInGameMenuOpen() const
{
	if (UMOSystemMenuUIController* SysController = GetSystemMenuController())
	{
		return SysController->IsInGameMenuOpen();
	}
	return false;
}

// =============================================================================
// Item Context Menu (Delegated to MOInventoryUIController)
// =============================================================================

void UMOUIManagerComponent::ShowItemContextMenu(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid, int32 SlotIndex, FVector2D ScreenPosition)
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		InvController->ShowItemContextMenu(InventoryComponent, ItemGuid, SlotIndex, ScreenPosition);
	}
}

void UMOUIManagerComponent::CloseItemContextMenu()
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		InvController->CloseItemContextMenu();
	}
}

bool UMOUIManagerComponent::IsItemContextMenuOpen() const
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		return InvController->IsItemContextMenuOpen();
	}
	return false;
}

// =============================================================================
// Confirmation Dialog (Delegated to MOSystemMenuUIController)
// =============================================================================

void UMOUIManagerComponent::ShowConfirmationDialog(const FText& Title, const FText& Message, const FText& ConfirmText, const FText& CancelText)
{
	if (UMOSystemMenuUIController* SysController = GetSystemMenuController())
	{
		SysController->ShowConfirmationDialog(Title, Message, ConfirmText, CancelText);
	}
}

// =============================================================================
// Menu Stack Helpers
// =============================================================================

bool UMOUIManagerComponent::IsAnyMenuOpen() const
{
	// Delegate to controllers for their respective menus
	return IsInventoryMenuOpen() || IsInGameMenuOpen() || IsItemContextMenuOpen() ||
	       IsPlayerStatusVisible() || IsPossessionMenuOpen() || IsCraftingMenuOpen() ||
	       IsSkillsPanelOpen() || IsQuestLogOpen() || IsBuildingMenuOpen() || IsBuildWidgetOpen() ||
	       IsStationContextMenuOpen() || IsKeepOnHarvestContextMenuOpen() ||
	       IsInspectionInProgress() || IsHarvestInProgress();
}

void UMOUIManagerComponent::CloseAllMenus()
{
	// Delegate to each controller to close its menus
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		InvController->CloseInventoryMenu();
		InvController->CloseItemContextMenu();
	}

	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		CharController->CloseSkillsPanel();
		CharController->SetPlayerStatusVisible(false);
		CharController->CancelItemInspection();
	}

	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		CraftController->CloseCraftingMenu();
		CraftController->HideStationContextMenu();
		CraftController->HideKeepOnHarvestContextMenu();
		CraftController->CancelHarvestOperation();
	}

	if (UMOBuildingUIController* BuildController = GetBuildingController())
	{
		BuildController->CloseBuildingMenu();
		BuildController->HideBuildWidget();
	}

	if (UMOSystemMenuUIController* SysController = GetSystemMenuController())
	{
		SysController->CloseInGameMenu();
		SysController->ClosePossessionMenu();
	}

	if (UMOQuestUIController* QuestController = GetQuestController())
	{
		QuestController->CloseQuestLog();
	}

	// Hide modal background
	HideModalBackground();

	// Restore input mode
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (IsValid(PlayerController) && PlayerController->IsLocalController())
	{
		ApplyInputModeForMenuClosed(PlayerController);
	}

	UpdateReticleVisibility();
}

void UMOUIManagerComponent::CloseAllSwitchableMenus()
{
	// Close all menus that participate in menu switching.
	// These are the main gameplay menus that toggle between each other.
	// NOT included: In-game menu (pause), confirmation dialogs, context menus, inspection.

	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		InvController->CloseInventoryMenu();
	}

	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		CraftController->CloseCraftingMenu();
	}

	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		CharController->CloseSkillsPanel();
		CharController->SetPlayerStatusVisible(false);
	}

	if (UMOSystemMenuUIController* SysController = GetSystemMenuController())
	{
		SysController->ClosePossessionMenu();
	}

	if (UMOBuildingUIController* BuildController = GetBuildingController())
	{
		BuildController->CloseBuildingMenu();
	}

	if (UMOQuestUIController* QuestController = GetQuestController())
	{
		QuestController->CloseQuestLog();
	}

	// Hide modal background if no menus remain open
	if (!IsAnyMenuOpen())
	{
		HideModalBackground();

		APlayerController* PlayerController = ResolveOwningPlayerController();
		if (IsValid(PlayerController) && PlayerController->IsLocalController())
		{
			ApplyInputModeForMenuClosed(PlayerController);
		}
	}
}

void UMOUIManagerComponent::UpdateReticleVisibility()
{
	const bool bMenuOpen = IsAnyMenuOpen();

	if (bHideReticleWhenMenuOpen)
	{
		SetReticleVisible(!bMenuOpen);
	}

	// Note: bHideStatusPanelWhenMenuOpen is handled differently - the status panel
	// IS a menu, so it shouldn't hide itself. This flag would be for hiding a
	// persistent HUD-style status display, which we don't currently have.
}

void UMOUIManagerComponent::ShowModalBackground()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	UMOModalBackground* Background = ModalBackgroundWidget.Get();
	if (!IsValid(Background))
	{
		Background = CreateWidget<UMOModalBackground>(PlayerController, UMOModalBackground::StaticClass());
		if (!IsValid(Background))
		{
			return;
		}

		ModalBackgroundWidget = Background;
		Background->OnBackgroundClicked.AddDynamic(this, &UMOUIManagerComponent::HandleModalBackgroundClicked);
	}

	if (!Background->IsInViewport())
	{
		Background->AddToViewport(ModalBackgroundZOrder);
	}
}

void UMOUIManagerComponent::HideModalBackground()
{
	UMOModalBackground* Background = ModalBackgroundWidget.Get();
	if (IsValid(Background) && Background->IsInViewport())
	{
		Background->RemoveFromParent();
	}
}

void UMOUIManagerComponent::HandleModalBackgroundClicked()
{
	// Close all open menus when clicking outside
	CloseAllMenus();
}

// =============================================================================
// UI Controller Support (Public wrappers for controller access)
// =============================================================================

void UMOUIManagerComponent::RequestShowModalBackground()
{
	ShowModalBackground();
}

void UMOUIManagerComponent::RequestHideModalBackground()
{
	HideModalBackground();
}

void UMOUIManagerComponent::RequestInputModeForMenuOpen(UUserWidget* MenuWidget)
{
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (IsValid(PlayerController) && IsValid(MenuWidget))
	{
		ApplyInputModeForMenuOpen(PlayerController, MenuWidget);
	}
}

void UMOUIManagerComponent::RequestInputModeForMenuClosed()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (IsValid(PlayerController))
	{
		ApplyInputModeForMenuClosed(PlayerController);
	}
}

void UMOUIManagerComponent::RequestUpdateReticleVisibility()
{
	UpdateReticleVisibility();
}

// =============================================================================
// Specialized UI Controller Getters
// =============================================================================
// These resolve sibling controller components on the same PlayerController owner.
// Controllers are created as default subobjects on the PlayerController.
// Returns nullptr until controllers are actually added to the PlayerController.

UMOInventoryUIController* UMOUIManagerComponent::GetInventoryController() const
{
	// Check cache first
	if (UMOInventoryUIController* Cached = CachedInventoryController.Get())
	{
		return Cached;
	}

	// Find sibling component on same owner
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		UMOInventoryUIController* Found = Owner->FindComponentByClass<UMOInventoryUIController>();
		if (Found)
		{
			CachedInventoryController = Found;
			return Found;
		}
	}

	return nullptr;
}

UMOCraftingUIController* UMOUIManagerComponent::GetCraftingController() const
{
	// Check cache first
	if (UMOCraftingUIController* Cached = CachedCraftingController.Get())
	{
		return Cached;
	}

	// Find sibling component on same owner
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		UMOCraftingUIController* Found = Owner->FindComponentByClass<UMOCraftingUIController>();
		if (Found)
		{
			CachedCraftingController = Found;
			return Found;
		}
	}

	return nullptr;
}

UMOBuildingUIController* UMOUIManagerComponent::GetBuildingController() const
{
	// Check cache first
	if (UMOBuildingUIController* Cached = CachedBuildingController.Get())
	{
		return Cached;
	}

	// Find sibling component on same owner
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		UMOBuildingUIController* Found = Owner->FindComponentByClass<UMOBuildingUIController>();
		if (Found)
		{
			CachedBuildingController = Found;
			return Found;
		}
	}

	return nullptr;
}

UMOCharacterUIController* UMOUIManagerComponent::GetCharacterController() const
{
	// Check cache first
	if (UMOCharacterUIController* Cached = CachedCharacterController.Get())
	{
		return Cached;
	}

	// Find sibling component on same owner
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		UMOCharacterUIController* Found = Owner->FindComponentByClass<UMOCharacterUIController>();
		if (Found)
		{
			CachedCharacterController = Found;
			return Found;
		}
	}

	return nullptr;
}

UMOSystemMenuUIController* UMOUIManagerComponent::GetSystemMenuController() const
{
	// Check cache first
	if (UMOSystemMenuUIController* Cached = CachedSystemMenuController.Get())
	{
		return Cached;
	}

	// Find sibling component on same owner
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		UMOSystemMenuUIController* Found = Owner->FindComponentByClass<UMOSystemMenuUIController>();
		if (Found)
		{
			CachedSystemMenuController = Found;
			return Found;
		}
	}

	return nullptr;
}

UMOQuestUIController* UMOUIManagerComponent::GetQuestController() const
{
	// Check cache first
	if (UMOQuestUIController* Cached = CachedQuestController.Get())
	{
		return Cached;
	}

	// Find sibling component on same owner
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		UMOQuestUIController* Found = Owner->FindComponentByClass<UMOQuestUIController>();
		if (Found)
		{
			CachedQuestController = Found;
			return Found;
		}
	}

	return nullptr;
}

void UMOUIManagerComponent::DropItemToWorldByGuid(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid)
{
	if (UMOInventoryUIController* InvController = GetInventoryController())
	{
		InvController->DropItemToWorldByGuid(InventoryComponent, ItemGuid);
	}
}

void UMOUIManagerComponent::RebindStatusPanelToCurrentPawn()
{
	// Delegated to CharacterUIController
	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		CharController->RebindStatusPanelToCurrentPawn();
	}
}

// =============================================================================
// Pawn Component Caching
// =============================================================================

void UMOUIManagerComponent::CachePawnComponents(APawn* NewPawn)
{
	// Clear any existing cached references
	ClearCachedPawnComponents();

	if (!IsValid(NewPawn))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] CachePawnComponents - No pawn, caches cleared"));
		return;
	}

	CachedPawn = NewPawn;

	// Cache all pawn components we frequently need
	// Use interface if available for inventory, otherwise fall back to FindComponentByClass
	if (NewPawn->Implements<UMOInventoryHolderInterface>())
	{
		CachedInventoryComponent = IMOInventoryHolderInterface::Execute_GetInventory(NewPawn);
	}
	else
	{
		CachedInventoryComponent = NewPawn->FindComponentByClass<UMOInventoryComponent>();
	}

	CachedSkillsComponent = NewPawn->FindComponentByClass<UMOSkillsComponent>();
	CachedKnowledgeComponent = NewPawn->FindComponentByClass<UMOKnowledgeComponent>();
	CachedCraftingQueueComponent = NewPawn->FindComponentByClass<UMOCraftingQueueComponent>();
	CachedRecipeDiscoveryComponent = NewPawn->FindComponentByClass<UMORecipeDiscoveryComponent>();
	CachedVitalsComponent = NewPawn->FindComponentByClass<UMOVitalsComponent>();
	CachedMetabolismComponent = NewPawn->FindComponentByClass<UMOMetabolismComponent>();
	CachedMentalStateComponent = NewPawn->FindComponentByClass<UMOMentalStateComponent>();
	CachedSurvivalStatsComponent = NewPawn->FindComponentByClass<UMOSurvivalStatsComponent>();
	CachedInteractorComponent = NewPawn->FindComponentByClass<UMOInteractorComponent>();

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] CachePawnComponents - Cached %d components for pawn %s"),
		(CachedInventoryComponent.IsValid() ? 1 : 0) +
		(CachedSkillsComponent.IsValid() ? 1 : 0) +
		(CachedKnowledgeComponent.IsValid() ? 1 : 0) +
		(CachedCraftingQueueComponent.IsValid() ? 1 : 0) +
		(CachedRecipeDiscoveryComponent.IsValid() ? 1 : 0) +
		(CachedVitalsComponent.IsValid() ? 1 : 0) +
		(CachedMetabolismComponent.IsValid() ? 1 : 0) +
		(CachedMentalStateComponent.IsValid() ? 1 : 0) +
		(CachedSurvivalStatsComponent.IsValid() ? 1 : 0) +
		(CachedInteractorComponent.IsValid() ? 1 : 0),
		*NewPawn->GetName());

	// Auto-rebind status panel if it exists
	RebindStatusPanelToCurrentPawn();
}

void UMOUIManagerComponent::ClearCachedPawnComponents()
{
	CachedPawn.Reset();
	CachedInventoryComponent.Reset();
	CachedSkillsComponent.Reset();
	CachedKnowledgeComponent.Reset();
	CachedCraftingQueueComponent.Reset();
	CachedRecipeDiscoveryComponent.Reset();
	CachedVitalsComponent.Reset();
	CachedMetabolismComponent.Reset();
	CachedMentalStateComponent.Reset();
	CachedSurvivalStatsComponent.Reset();
	CachedInteractorComponent.Reset();

	// Clear focus hint when pawn is lost
	CurrentFocusHintText = FText::GetEmpty();
}

UMOInventoryComponent* UMOUIManagerComponent::GetCachedInventory() const
{
	return CachedInventoryComponent.Get();
}

UMOSkillsComponent* UMOUIManagerComponent::GetCachedSkills() const
{
	return CachedSkillsComponent.Get();
}

UMOKnowledgeComponent* UMOUIManagerComponent::GetCachedKnowledge() const
{
	return CachedKnowledgeComponent.Get();
}

UMOCraftingQueueComponent* UMOUIManagerComponent::GetCachedCraftingQueue() const
{
	return CachedCraftingQueueComponent.Get();
}

UMORecipeDiscoveryComponent* UMOUIManagerComponent::GetCachedRecipeDiscovery() const
{
	return CachedRecipeDiscoveryComponent.Get();
}

UMOVitalsComponent* UMOUIManagerComponent::GetCachedVitals() const
{
	return CachedVitalsComponent.Get();
}

UMOMetabolismComponent* UMOUIManagerComponent::GetCachedMetabolism() const
{
	return CachedMetabolismComponent.Get();
}

UMOMentalStateComponent* UMOUIManagerComponent::GetCachedMentalState() const
{
	return CachedMentalStateComponent.Get();
}

UMOSurvivalStatsComponent* UMOUIManagerComponent::GetCachedSurvivalStats() const
{
	return CachedSurvivalStatsComponent.Get();
}

// =============================================================================
// No Pawn Notification
// =============================================================================

void UMOUIManagerComponent::ShowNoPawnNotification()
{
	// Use the consolidated notification system
	UMONotificationComponent* NotificationComp = ResolveNotificationComponent();
	if (NotificationComp)
	{
		NotificationComp->ShowWarningNotification(NoPawnMessage, NoPawnNotificationDuration);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] ShowNoPawnNotification - No notification component found"));
	}

	// Broadcast delegate so possession menu can hook in
	OnNoPawnForMenu.Broadcast();

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Showing no-pawn notification: %s"), *NoPawnMessage.ToString());
}

void UMOUIManagerComponent::HideNoPawnNotification()
{
	// The notification component now handles its own timing
	// This method kept for backwards compatibility but is no longer needed
	// The notification will auto-hide based on the duration passed to ShowWarningNotification
}

// =============================================================================
// Item Inspection (Delegated to MOCharacterUIController)
// =============================================================================

void UMOUIManagerComponent::StartItemInspection(FName ItemDefinitionId, const FGuid& ItemGuid)
{
	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		CharController->StartItemInspection(ItemDefinitionId, ItemGuid);
	}
}

void UMOUIManagerComponent::CancelItemInspection()
{
	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		CharController->CancelItemInspection();
	}
}

bool UMOUIManagerComponent::IsInspectionInProgress() const
{
	if (UMOCharacterUIController* CharController = GetCharacterController())
	{
		return CharController->IsInspectionInProgress();
	}
	return false;
}

// =============================================================================
// Notifications (Delegated to UMONotificationComponent)
// =============================================================================

UMONotificationComponent* UMOUIManagerComponent::ResolveNotificationComponent() const
{
	// Check cache first
	UMONotificationComponent* Cached = CachedNotificationComponent.Get();
	if (IsValid(Cached))
	{
		return Cached;
	}

	// Find on owner
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		UMONotificationComponent* Found = Owner->FindComponentByClass<UMONotificationComponent>();
		if (IsValid(Found))
		{
			// Cache it (member is mutable for caching in const method)
			CachedNotificationComponent = Found;
			return Found;
		}
	}

	return nullptr;
}

UMONotificationComponent* UMOUIManagerComponent::GetNotificationComponent() const
{
	return ResolveNotificationComponent();
}

void UMOUIManagerComponent::ShowNotification(const FText& Message, float Duration)
{
	UMONotificationComponent* NotificationComp = ResolveNotificationComponent();
	if (IsValid(NotificationComp))
	{
		NotificationComp->ShowNotification(Message, Duration);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] ShowNotification called but no UMONotificationComponent found on owner. Add UMONotificationComponent to your PlayerController."));
	}
}

void UMOUIManagerComponent::ShowSkillIncreaseNotification(FName SkillId, float XPAmount)
{
	UMONotificationComponent* NotificationComp = ResolveNotificationComponent();
	if (IsValid(NotificationComp))
	{
		NotificationComp->ShowSkillIncreaseNotification(SkillId, XPAmount);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] ShowSkillIncreaseNotification called but no UMONotificationComponent found on owner."));
	}
}

void UMOUIManagerComponent::ShowRecipeUnlockedNotification(FName RecipeId)
{
	UMONotificationComponent* NotificationComp = ResolveNotificationComponent();
	if (IsValid(NotificationComp))
	{
		NotificationComp->ShowRecipeUnlockedNotification(RecipeId);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] ShowRecipeUnlockedNotification called but no UMONotificationComponent found on owner."));
	}
}

// =============================================================================
// Building Menu (Delegated to MOBuildingUIController)
// =============================================================================

void UMOUIManagerComponent::ToggleBuildingMenu()
{
	if (UMOBuildingUIController* BuildController = GetBuildingController())
	{
		BuildController->ToggleBuildingMenu();
	}
}

void UMOUIManagerComponent::OpenBuildingMenu()
{
	if (UMOBuildingUIController* BuildController = GetBuildingController())
	{
		BuildController->OpenBuildingMenu();
	}
}

void UMOUIManagerComponent::CloseBuildingMenu()
{
	if (UMOBuildingUIController* BuildController = GetBuildingController())
	{
		BuildController->CloseBuildingMenu();
	}
}

bool UMOUIManagerComponent::IsBuildingMenuOpen() const
{
	if (UMOBuildingUIController* BuildController = GetBuildingController())
	{
		return BuildController->IsBuildingMenuOpen();
	}
	return false;
}

UMOBuildingMenu* UMOUIManagerComponent::GetBuildingMenu() const
{
	if (UMOBuildingUIController* BuildController = GetBuildingController())
	{
		return BuildController->GetBuildingMenu();
	}
	return nullptr;
}

// =============================================================================
// Ghost Context Menu (Delegated to MOBuildingUIController)
// =============================================================================

void UMOUIManagerComponent::ShowBuildWidget(AMOBuildableActor* Target)
{
	if (UMOBuildingUIController* BuildController = GetBuildingController())
	{
		BuildController->ShowBuildWidget(Target);
	}
}

void UMOUIManagerComponent::HideBuildWidget()
{
	if (UMOBuildingUIController* BuildController = GetBuildingController())
	{
		BuildController->HideBuildWidget();
	}
}

bool UMOUIManagerComponent::IsBuildWidgetOpen() const
{
	if (UMOBuildingUIController* BuildController = GetBuildingController())
	{
		return BuildController->IsBuildWidgetOpen();
	}
	return false;
}

// ============================================================================
// STATION CONTEXT MENU (Delegated to MOCraftingUIController)
// ============================================================================

void UMOUIManagerComponent::ShowStationContextMenu(AActor* StationActor, FVector WorldPosition)
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		CraftController->ShowStationContextMenu(StationActor, WorldPosition);
	}
}

void UMOUIManagerComponent::HideStationContextMenu()
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		CraftController->HideStationContextMenu();
	}
}

bool UMOUIManagerComponent::IsStationContextMenuOpen() const
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		return CraftController->IsStationContextMenuOpen();
	}
	return false;
}

// =============================================================================
// KEEPONHARVEST CONTEXT MENU (Delegated to MOCraftingUIController)
// =============================================================================

void UMOUIManagerComponent::ShowKeepOnHarvestContextMenu(const FMOInteractionTarget& Target)
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		CraftController->ShowKeepOnHarvestContextMenu(Target);
	}
}

void UMOUIManagerComponent::HideKeepOnHarvestContextMenu()
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		CraftController->HideKeepOnHarvestContextMenu();
	}
}

bool UMOUIManagerComponent::IsKeepOnHarvestContextMenuOpen() const
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		return CraftController->IsKeepOnHarvestContextMenuOpen();
	}
	return false;
}

void UMOUIManagerComponent::StartHarvestOperation(FName RecipeId)
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		CraftController->StartHarvestOperation(RecipeId);
	}
}

void UMOUIManagerComponent::CancelHarvestOperation()
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		CraftController->CancelHarvestOperation();
	}
}

bool UMOUIManagerComponent::IsHarvestInProgress() const
{
	if (UMOCraftingUIController* CraftController = GetCraftingController())
	{
		return CraftController->IsHarvestInProgress();
	}
	return false;
}

// =============================================================================
// Mode Indicator
// =============================================================================

void UMOUIManagerComponent::CreateModeIndicator()
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

	// Use configured class or default
	TSubclassOf<UMOModeIndicatorWidget> WidgetClass = ModeIndicatorClass;
	if (!WidgetClass)
	{
		WidgetClass = UMOModeIndicatorWidget::StaticClass();
	}

	UMOModeIndicatorWidget* WidgetInst = CreateWidget<UMOModeIndicatorWidget>(PlayerController, WidgetClass);
	if (!IsValid(WidgetInst))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to create Mode Indicator widget"));
		return;
	}

	ModeIndicatorWidget = WidgetInst;
	WidgetInst->AddToViewport(ModeIndicatorZOrder);

	// Set initial mode
	WidgetInst->SetMode(CurrentGameplayMode);

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Mode Indicator created"));
}

void UMOUIManagerComponent::SetGameplayMode(EMOGameplayMode NewMode)
{
	CurrentGameplayMode = NewMode;

	UMOModeIndicatorWidget* WidgetInst = ModeIndicatorWidget.Get();
	if (IsValid(WidgetInst))
	{
		WidgetInst->SetMode(NewMode);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Gameplay mode set to: %s"),
		*UMOModeIndicatorWidget::GetModeDisplayName(NewMode).ToString());
}

EMOGameplayMode UMOUIManagerComponent::GetGameplayMode() const
{
	return CurrentGameplayMode;
}

UMOModeIndicatorWidget* UMOUIManagerComponent::GetModeIndicator() const
{
	return ModeIndicatorWidget.Get();
}

// =============================================================================
// Tool Hint
// =============================================================================

void UMOUIManagerComponent::CreateToolHint()
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

	// Use configured class or default
	TSubclassOf<UMOToolHintWidget> WidgetClass = ToolHintClass;
	if (!WidgetClass)
	{
		WidgetClass = UMOToolHintWidget::StaticClass();
	}

	UMOToolHintWidget* WidgetInst = CreateWidget<UMOToolHintWidget>(PlayerController, WidgetClass);
	if (!IsValid(WidgetInst))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOUI] Failed to create Tool Hint widget"));
		return;
	}

	ToolHintWidget = WidgetInst;
	WidgetInst->AddToViewport(ToolHintZOrder);

	UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Tool Hint created"));
}

void UMOUIManagerComponent::ShowToolHint(const FText& HintText, float Duration)
{
	UMOToolHintWidget* WidgetInst = ToolHintWidget.Get();
	if (IsValid(WidgetInst))
	{
		WidgetInst->ShowHint(HintText, Duration);
	}
}

void UMOUIManagerComponent::HideToolHint()
{
	UMOToolHintWidget* WidgetInst = ToolHintWidget.Get();
	if (IsValid(WidgetInst))
	{
		WidgetInst->HideHint();
	}
}

UMOToolHintWidget* UMOUIManagerComponent::GetToolHintWidget() const
{
	return ToolHintWidget.Get();
}

// =============================================================================
// Focus Hint (shows item name when looking at interactable objects)
// =============================================================================

void UMOUIManagerComponent::SetFocusHintEnabled(bool bEnabled)
{
	if (bFocusHintEnabled == bEnabled)
	{
		return;
	}

	bFocusHintEnabled = bEnabled;

	if (bFocusHintEnabled)
	{
		StartFocusHintTimer();
	}
	else
	{
		StopFocusHintTimer();
		CurrentFocusHintText = FText::GetEmpty();

		// Hide the tool hint when disabled
		HideToolHint();
	}
}

void UMOUIManagerComponent::StartFocusHintTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FocusHintTimerHandle,
			this,
			&UMOUIManagerComponent::UpdateFocusHint,
			FocusHintUpdateInterval,
			true  // Looping
		);

		UE_LOG(LogMOFramework, Log, TEXT("[MOUI] Focus hint timer started (%.0fms interval)"), FocusHintUpdateInterval * 1000.0f);
	}
}

void UMOUIManagerComponent::StopFocusHintTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FocusHintTimerHandle);
	}
}

void UMOUIManagerComponent::UpdateFocusHint()
{
	// Don't update if any menu is open
	if (IsAnyMenuOpen())
	{
		if (!CurrentFocusHintText.IsEmpty())
		{
			CurrentFocusHintText = FText::GetEmpty();
		}
		return;
	}

	// Get the interactor component from the possessed pawn
	UMOInteractorComponent* Interactor = CachedInteractorComponent.Get();
	if (!Interactor)
	{
		if (!CurrentFocusHintText.IsEmpty())
		{
			CurrentFocusHintText = FText::GetEmpty();
		}
		return;
	}

	// Do the line trace to find what we're looking at
	FMOInteractionTarget Target;
	const bool bFoundTarget = Interactor->FindInteractionTarget(Target);

	FText NewHintText = FText::GetEmpty();

	if (bFoundTarget && Target.IsValid())
	{
		// For instanced meshes, look for "Name " prefixed tag for display name
		if (Target.bIsInstancedMeshTarget)
		{
			UInstancedStaticMeshComponent* ISMComp = Target.ISMComponent.Get();
			if (ISMComp)
			{
				// Look for tag with "Name " prefix (e.g., "Name Black Alder")
				for (const FName& Tag : ISMComp->ComponentTags)
				{
					FString TagString = Tag.ToString();
					if (TagString.StartsWith(TEXT("Name ")))
					{
						NewHintText = FText::FromString(TagString.RightChop(5)); // Remove "Name " prefix
						break;
					}
				}

				// Fall back to item lookup if no "Name " tag found
				if (NewHintText.IsEmpty())
				{
					UWorld* World = GetWorld();
					if (World)
					{
						UMOPCGInteractionSubsystem* PCGSubsystem = World->GetSubsystem<UMOPCGInteractionSubsystem>();
						if (PCGSubsystem)
						{
							FName ItemId = PCGSubsystem->GetItemIdForComponentTags(Cast<UActorComponent>(ISMComp));
							if (ItemId.IsNone())
							{
								ItemId = PCGSubsystem->GetItemIdForMesh(ISMComp->GetStaticMesh());
							}

							if (!ItemId.IsNone())
							{
								NewHintText = UMOItemDatabaseSettings::GetItemDisplayName(ItemId);
							}
						}
					}
				}
			}
		}
		else if (Target.TargetActor.IsValid())
		{
			// For regular actors, try to get a display name
			// Check if it has an item component with an item definition
			AActor* HitActor = Target.TargetActor.Get();
			if (UMOItemComponent* ItemComp = HitActor->FindComponentByClass<UMOItemComponent>())
			{
				FName ItemId = ItemComp->ItemDefinitionId;
				if (!ItemId.IsNone())
				{
					NewHintText = UMOItemDatabaseSettings::GetItemDisplayName(ItemId);
				}
			}
		}
	}

	// Only update if the text changed
	if (!NewHintText.EqualTo(CurrentFocusHintText))
	{
		CurrentFocusHintText = NewHintText;

		// Update the tool hint widget with what we're looking at
		if (!CurrentFocusHintText.IsEmpty())
		{
			// Show with 0 duration = persistent until hidden or replaced
			ShowToolHint(CurrentFocusHintText, 0.0f);
		}
		else
		{
			HideToolHint();
		}
	}
}
