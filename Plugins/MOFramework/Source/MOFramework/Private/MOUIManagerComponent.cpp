#include "MOUIManagerComponent.h"

#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"

#include "MOInventoryComponent.h"
#include "MOInventoryMenu.h"

UMOUIManagerComponent::UMOUIManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMOUIManagerComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UMOUIManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Ensure we restore input mode on teardown if this component dies while menu is open.
	CloseInventoryMenu();

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

bool UMOUIManagerComponent::IsInventoryMenuOpen() const
{
	const UMOInventoryMenu* MenuWidget = InventoryMenuWidget.Get();
	return IsValid(MenuWidget) && MenuWidget->IsInViewport();
}

UMOInventoryComponent* UMOUIManagerComponent::ResolveCurrentPawnInventoryComponent() const
{
	APlayerController* PlayerController = ResolveOwningPlayerController();
	if (!IsValid(PlayerController))
	{
		return nullptr;
	}

	APawn* CurrentPawn = PlayerController->GetPawn();
	if (!IsValid(CurrentPawn))
	{
		return nullptr;
	}

	return CurrentPawn->FindComponentByClass<UMOInventoryComponent>();
}

void UMOUIManagerComponent::ToggleInventoryMenu()
{
	if (!IsLocalOwningPlayerController())
	{
		return;
	}

	if (IsInventoryMenuOpen())
	{
		CloseInventoryMenu();
	}
	else
	{
		OpenInventoryMenu();
	}
}

void UMOUIManagerComponent::OpenInventoryMenu()
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

	if (!InventoryMenuClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOUI] InventoryMenuClass not set on UI manager component."));
		return;
	}

	UMOInventoryComponent* InventoryComponent = ResolveCurrentPawnInventoryComponent();
	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOUI] No UMOInventoryComponent found on current pawn."));
		return;
	}

	UMOInventoryMenu* MenuWidget = InventoryMenuWidget.Get();
	if (!IsValid(MenuWidget))
	{
		MenuWidget = CreateWidget<UMOInventoryMenu>(PlayerController, InventoryMenuClass);
		InventoryMenuWidget = MenuWidget;

		if (!IsValid(MenuWidget))
		{
			return;
		}

		// Bind Tab close (widget broadcasts, manager closes).
		MenuWidget->OnRequestClose.AddDynamic(this, &UMOUIManagerComponent::HandleInventoryMenuRequestClose);
	}

	// Always re-initialize on open in case pawn changed.
	MenuWidget->InitializeMenu(InventoryComponent);

	if (!MenuWidget->IsInViewport())
	{
		MenuWidget->AddToViewport(InventoryMenuZOrder);
	}

	ApplyInputModeForMenuOpen(PlayerController, MenuWidget);
}

void UMOUIManagerComponent::CloseInventoryMenu()
{
	APlayerController* PlayerController = ResolveOwningPlayerController();

	UMOInventoryMenu* MenuWidget = InventoryMenuWidget.Get();
	if (IsValid(MenuWidget))
	{
		if (MenuWidget->IsInViewport())
		{
			MenuWidget->RemoveFromParent();
		}
	}

	if (IsValid(PlayerController) && PlayerController->IsLocalController())
	{
		ApplyInputModeForMenuClosed(PlayerController);
	}
}

void UMOUIManagerComponent::ApplyInputModeForMenuOpen(APlayerController* PlayerController, UMOInventoryMenu* MenuWidget) const
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

void UMOUIManagerComponent::HandleInventoryMenuRequestClose()
{
	CloseInventoryMenu();
}
