#include "MOBuildingUIController.h"
#include "MOFramework.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"

#include "MOUIManagerComponent.h"
#include "MOBuildingMenu.h"
#include "MOGhostContextMenu.h"
#include "MOBuildableActor.h"
#include "MOBuildingComponent.h"
#include "MOPlayerController.h"
#include "MOKnowledgeComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MOInventoryHolderInterface.h"
#include "MOModalBackground.h"
#include "MOPrimaryGameLayout.h"

UMOBuildingUIController::UMOBuildingUIController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMOBuildingUIController::BeginPlay()
{
	Super::BeginPlay();
}

void UMOBuildingUIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up building menu widget - unbind delegates first
	if (UMOBuildingMenu* MenuWidget = BuildingMenuWidget.Get())
	{
		MenuWidget->OnRequestClose.RemoveDynamic(this, &UMOBuildingUIController::HandleBuildingMenuRequestClose);
		MenuWidget->OnBuildingSelected.RemoveDynamic(this, &UMOBuildingUIController::HandleBuildingSelected);
		if (MenuWidget->IsInViewport())
		{
			MenuWidget->RemoveFromParent();
		}
	}
	BuildingMenuWidget.Reset();

	// Clean up ghost context menu widget - unbind delegates first
	if (UMOGhostContextMenu* GhostWidget = GhostContextMenuWidget.Get())
	{
		GhostWidget->OnRequestClose.RemoveDynamic(this, &UMOBuildingUIController::HandleGhostContextMenuRequestClose);
		GhostWidget->OnBuildStarted.RemoveDynamic(this, &UMOBuildingUIController::HandleGhostContextMenuBuildStarted);
		GhostWidget->OnCancelled.RemoveDynamic(this, &UMOBuildingUIController::HandleGhostContextMenuCancelled);
		if (GhostWidget->IsInViewport())
		{
			GhostWidget->RemoveFromParent();
		}
	}
	GhostContextMenuWidget.Reset();

	CurrentBuildTarget.Reset();

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// Building Menu
// =============================================================================

void UMOBuildingUIController::ToggleBuildingMenu()
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

	// If building menu is already open, just close it
	if (IsBuildingMenuOpen())
	{
		CloseBuildingMenu();
		return;
	}

	// Close all switchable menus and open building menu
	if (UIManager)
	{
		UIManager->CloseAllSwitchableMenus();
	}
	OpenBuildingMenu();
}

void UMOBuildingUIController::OpenBuildingMenu()
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
		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildUI] OpenBuildingMenu - No valid pawn, showing notification"));
		ShowNoPawnNotification();
		return;
	}

	if (!BuildingMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildUI] BuildingMenuClass not set on BuildingUIController."));
		return;
	}

	// Create widget if needed
	UMOBuildingMenu* MenuWidget = BuildingMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		MenuWidget = CreateWidget<UMOBuildingMenu>(PlayerController, BuildingMenuClass);
		if (!IsValid(MenuWidget))
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOBuildUI] Failed to create Building Menu widget"));
			return;
		}

		BuildingMenuWidget = MenuWidget;

		// Bind delegates
		MenuWidget->OnRequestClose.RemoveDynamic(this, &UMOBuildingUIController::HandleBuildingMenuRequestClose);
		MenuWidget->OnBuildingSelected.RemoveDynamic(this, &UMOBuildingUIController::HandleBuildingSelected);
		MenuWidget->OnRequestClose.AddDynamic(this, &UMOBuildingUIController::HandleBuildingMenuRequestClose);
		MenuWidget->OnBuildingSelected.AddDynamic(this, &UMOBuildingUIController::HandleBuildingSelected);
	}

	// Initialize menu with cached pawn component data
	UMOKnowledgeComponent* Knowledge = GetCachedKnowledge();
	UMORecipeDiscoveryComponent* Discovery = GetCachedRecipeDiscovery();
	UMOInventoryComponent* Inventory = GetCachedInventory();
	UMOSkillsComponent* Skills = GetCachedSkills();
	MenuWidget->InitializeMenu(Knowledge, Discovery, Inventory, Skills);

	// Show modal background and menu via layer system
	ShowModalBackground();
	PushWidgetInstanceToLayer(MOUILayerTags::Layer_Menu, MenuWidget, BuildingMenuZOrder);

	// Set input mode
	ApplyInputModeForMenuOpen(MenuWidget);

	// Set focus for keyboard navigation
	MenuWidget->SetFocus();

	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildUI] Building Menu opened"));
}

void UMOBuildingUIController::CloseBuildingMenu()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();

	UMOBuildingMenu* MenuWidget = BuildingMenuWidget.Get();
	if (IsValid(MenuWidget))
	{
		if (MenuWidget->IsInViewport())
		{
			MenuWidget->RemoveFromParent();
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildUI] Building Menu closed"));
}

bool UMOBuildingUIController::IsBuildingMenuOpen() const
{
	UMOBuildingMenu* MenuWidget = BuildingMenuWidget.Get();
	return IsValid(MenuWidget) && MenuWidget->IsInViewport();
}

UMOBuildingMenu* UMOBuildingUIController::GetBuildingMenu() const
{
	return BuildingMenuWidget.Get();
}

void UMOBuildingUIController::HandleBuildingMenuRequestClose()
{
	CloseBuildingMenu();
}

void UMOBuildingUIController::HandleBuildingSelected(FName RecipeId)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildUI] Building selected: %s"), *RecipeId.ToString());

	// Close the building menu
	CloseBuildingMenu();

	// Enter placement mode via building component
	AMOPlayerController* PC = Cast<AMOPlayerController>(ResolveOwningPlayerController());
	if (IsValid(PC))
	{
		UMOBuildingComponent* BuildingComp = PC->GetBuildingComponent();
		if (IsValid(BuildingComp))
		{
			BuildingComp->EnterPlacementMode(RecipeId);
		}
	}
}

// =============================================================================
// Ghost Context Menu (Build Widget)
// =============================================================================

void UMOBuildingUIController::ShowBuildWidget(AMOBuildableActor* Target)
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	if (!IsValid(Target))
	{
		return;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (!GhostContextMenuClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildUI] GhostContextMenuClass not set on BuildingUIController."));
		return;
	}

	// Get the player's inventory for material sourcing via interface
	UMOInventoryComponent* BuilderInventory = nullptr;
	APawn* Pawn = PlayerController->GetPawn();
	if (IsValid(Pawn) && Pawn->Implements<UMOInventoryHolderInterface>())
	{
		BuilderInventory = IMOInventoryHolderInterface::Execute_GetInventory(Pawn);
	}

	// Create widget if needed
	UMOGhostContextMenu* WidgetInst = GhostContextMenuWidget.Get();
	if (!IsValid(WidgetInst))
	{
		WidgetInst = CreateWidget<UMOGhostContextMenu>(PlayerController, GhostContextMenuClass);
		if (!IsValid(WidgetInst))
		{
			UE_LOG(LogMOFramework, Error, TEXT("[MOBuildUI] Failed to create Ghost Context Menu"));
			return;
		}

		GhostContextMenuWidget = WidgetInst;

		// Bind delegates
		WidgetInst->OnRequestClose.RemoveDynamic(this, &UMOBuildingUIController::HandleGhostContextMenuRequestClose);
		WidgetInst->OnBuildStarted.RemoveDynamic(this, &UMOBuildingUIController::HandleGhostContextMenuBuildStarted);
		WidgetInst->OnCancelled.RemoveDynamic(this, &UMOBuildingUIController::HandleGhostContextMenuCancelled);
		WidgetInst->OnRequestClose.AddDynamic(this, &UMOBuildingUIController::HandleGhostContextMenuRequestClose);
		WidgetInst->OnBuildStarted.AddDynamic(this, &UMOBuildingUIController::HandleGhostContextMenuBuildStarted);
		WidgetInst->OnCancelled.AddDynamic(this, &UMOBuildingUIController::HandleGhostContextMenuCancelled);
	}

	CurrentBuildTarget = Target;

	// Initialize widget with target building and player inventory
	WidgetInst->InitializeForGhost(Target, BuilderInventory);

	// Show clickable modal background first (clicking it will close the menu)
	ShowModalBackground();

	// Bind modal click to close ghost menu via UIManager
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		// Note: Modal background binding is handled by UIManager
		// We just need to show it and let UIManager's CloseAllMenus handle clicks
	}

	// Add menu to layer system above background
	PushWidgetInstanceToLayer(MOUILayerTags::Layer_GameOverlay, WidgetInst, GhostContextMenuZOrder);

	// Get viewport size
	int32 ViewportX, ViewportY;
	PlayerController->GetViewportSize(ViewportX, ViewportY);

	// Try to position near the target building by projecting world position to screen
	FVector BoundsOrigin;
	FVector BoundsExtent;
	Target->GetActorBounds(true, BoundsOrigin, BoundsExtent);

	// Use center of object for positioning
	FVector WorldPosition = BoundsOrigin;

	FVector2D ScreenPosition;
	bool bProjected = PlayerController->ProjectWorldLocationToScreen(WorldPosition, ScreenPosition, false);

	UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildUI] Projection: World(%.0f, %.0f, %.0f) -> Screen(%.0f, %.0f), success=%d, viewport=(%d, %d)"),
		WorldPosition.X, WorldPosition.Y, WorldPosition.Z,
		ScreenPosition.X, ScreenPosition.Y, bProjected ? 1 : 0,
		ViewportX, ViewportY);

	// Check if the projected position is valid (on screen)
	bool bValidPosition = bProjected &&
		ScreenPosition.X >= 0 && ScreenPosition.X <= ViewportX &&
		ScreenPosition.Y >= 0 && ScreenPosition.Y <= ViewportY;

	if (bValidPosition)
	{
		// Offset to the right so menu doesn't obscure object
		ScreenPosition.X = FMath::Min(ScreenPosition.X + 50.0f, (float)ViewportX - 320.0f);
		ScreenPosition.Y = FMath::Clamp(ScreenPosition.Y - 100.0f, 10.0f, (float)ViewportY - 420.0f);
	}
	else
	{
		// Fallback: center of screen
		ScreenPosition.X = (ViewportX - 300.0f) * 0.5f;
		ScreenPosition.Y = (ViewportY - 400.0f) * 0.5f;
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildUI] Using center fallback position"));
	}

	WidgetInst->SetPopupPosition(ScreenPosition);
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildUI] Final menu position: (%.0f, %.0f)"), ScreenPosition.X, ScreenPosition.Y);

	// Set input mode - use Game and UI so keyboard still works
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(WidgetInst->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(true);

	WidgetInst->SetKeyboardFocus();

	UpdateReticleVisibility();

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildUI] Ghost Context Menu opened for: %s"), *Target->GetName());
}

void UMOBuildingUIController::HideBuildWidget()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();

	UMOGhostContextMenu* WidgetInst = GhostContextMenuWidget.Get();
	if (IsValid(WidgetInst))
	{
		if (WidgetInst->IsInViewport())
		{
			WidgetInst->RemoveFromParent();
		}
	}

	CurrentBuildTarget.Reset();

	UpdateReticleVisibility();

	// Hide modal and restore input mode
	HideModalBackground();
	if (IsValid(PlayerController) && PlayerController->IsLocalController())
	{
		ApplyInputModeForMenuClosed();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildUI] Ghost Context Menu closed"));
}

bool UMOBuildingUIController::IsBuildWidgetOpen() const
{
	UMOGhostContextMenu* WidgetInst = GhostContextMenuWidget.Get();
	return IsValid(WidgetInst) && WidgetInst->IsInViewport();
}

void UMOBuildingUIController::HandleGhostContextMenuRequestClose()
{
	HideBuildWidget();
}

void UMOBuildingUIController::HandleGhostContextMenuBuildStarted()
{
	// Don't close the menu when build starts - let user watch progress
	// The menu will auto-close when build completes or user moves mouse away
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildUI] Build started from Ghost Context Menu"));
}

void UMOBuildingUIController::HandleGhostContextMenuCancelled()
{
	// The context menu handles cancellation and material dropping internally
	// Just close the widget
	HideBuildWidget();

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildUI] Build cancelled from Ghost Context Menu"));
}
