#include "MOUIControllerBase.h"
#include "MOFramework.h"
#include "MOUIManagerComponent.h"
#include "MONotificationComponent.h"
#include "MOGameUIManagerSubsystem.h"
#include "MOPrimaryGameLayout.h"
#include "CommonActivatableWidget.h"
#include "GameFramework/PlayerController.h"

UMOUIControllerBase::UMOUIControllerBase()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMOUIControllerBase::BeginPlay()
{
	Super::BeginPlay();

	// Single systematic possession-change hook for all UI controllers.
	// Each subclass overrides OnPossessedPawnChanged() to react.
	// Without this, BeginPlay-time pawn binding silently bails (pawn is null
	// before possession completes) and no later hook re-runs the binding.
	if (APlayerController* PC = ResolveOwningPlayerController())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &UMOUIControllerBase::HandlePossessedPawnChanged);
		PC->OnPossessedPawnChanged.AddDynamic(this, &UMOUIControllerBase::HandlePossessedPawnChanged);
	}
}

void UMOUIControllerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerController* PC = ResolveOwningPlayerController())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &UMOUIControllerBase::HandlePossessedPawnChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void UMOUIControllerBase::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	// Glue: route the UFUNCTION-bound delegate call to the virtual hook
	// subclasses override. Keeping these as separate methods means subclasses
	// don't need their own UFUNCTION dispatcher — just override the virtual.
	OnPossessedPawnChanged(OldPawn, NewPawn);
}

APlayerController* UMOUIControllerBase::ResolveOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

bool UMOUIControllerBase::IsLocalOwningPlayerController() const
{
	const APlayerController* PC = ResolveOwningPlayerController();
	return IsValid(PC) && PC->IsLocalController();
}

UMOUIManagerComponent* UMOUIControllerBase::GetUIManager() const
{
	// Check cache first
	if (UMOUIManagerComponent* Cached = CachedUIManager.Get())
	{
		return Cached;
	}

	// Find sibling component on same owner
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		UMOUIManagerComponent* Found = Owner->FindComponentByClass<UMOUIManagerComponent>();
		if (IsValid(Found))
		{
			CachedUIManager = Found;
			return Found;
		}
	}

	return nullptr;
}

// =============================================================================
// UI MANAGER DELEGATION
// =============================================================================
// NOTE: Input mode is now handled automatically by CommonUI via GetDesiredInputConfig()
// ApplyInputModeForMenuOpen/Closed have been removed.

void UMOUIControllerBase::ShowModalBackground()
{
	// Delegate to UIManager for centralized modal background handling
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		UIManager->RequestShowModalBackground();
	}
}

void UMOUIControllerBase::HideModalBackground()
{
	// Delegate to UIManager for centralized modal background handling
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		UIManager->RequestHideModalBackground();
	}
}

bool UMOUIControllerBase::HasValidPawn() const
{
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		return UIManager->HasValidPawn();
	}
	return false;
}

void UMOUIControllerBase::ShowNoPawnNotification()
{
	// Delegate to UIManager's notification system
	if (UMONotificationComponent* NotificationComp = GetNotificationComponent())
	{
		// Use the standard notification pattern
		NotificationComp->ShowWarningNotification(
			NSLOCTEXT("MO", "NoPawnMessage", "Please select a character to view their information"),
			3.0f
		);
	}
}

void UMOUIControllerBase::UpdateReticleVisibility()
{
	// Delegate to UIManager for centralized reticle handling
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		UIManager->RequestUpdateReticleVisibility();
	}
}

bool UMOUIControllerBase::IsAnyMenuOpen() const
{
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		return UIManager->IsAnyMenuOpen();
	}
	return false;
}

// =============================================================================
// COMMONUI LAYER SYSTEM
// =============================================================================

UCommonActivatableWidget* UMOUIControllerBase::PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	UMOGameUIManagerSubsystem* UISubsystem = UMOGameUIManagerSubsystem::Get(this);
	if (!UISubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[MOUIControllerBase] PushWidgetToLayer: UISubsystem not available"));
		return nullptr;
	}

	UMOPrimaryGameLayout* Layout = UISubsystem->GetRootLayout();
	if (!Layout)
	{
		UE_LOG(LogTemp, Error, TEXT("[MOUIControllerBase] PushWidgetToLayer: No root layout. Is WBP_PrimaryGameLayout configured?"));
		return nullptr;
	}

	return Layout->PushWidgetToLayer(LayerTag, WidgetClass);
}

UCommonActivatableWidget* UMOUIControllerBase::PushWidgetInstanceToLayer(FGameplayTag LayerTag, UCommonActivatableWidget* Widget)
{
	if (!Widget)
	{
		return nullptr;
	}

	UMOGameUIManagerSubsystem* UISubsystem = UMOGameUIManagerSubsystem::Get(this);
	if (!UISubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[MOUIControllerBase] PushWidgetInstanceToLayer: UISubsystem not available"));
		return nullptr;
	}

	UMOPrimaryGameLayout* Layout = UISubsystem->GetRootLayout();
	if (!Layout)
	{
		UE_LOG(LogTemp, Error, TEXT("[MOUIControllerBase] PushWidgetInstanceToLayer: No root layout. Is WBP_PrimaryGameLayout configured?"));
		return nullptr;
	}

	return Layout->PushWidgetToLayerInstance(LayerTag, Widget);
}

void UMOUIControllerBase::PopWidgetFromLayer(UCommonActivatableWidget* Widget)
{
	if (!Widget)
	{
		return;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOUIControllerBase] PopWidgetFromLayer: %s (activated=%s)"),
		*Widget->GetName(), Widget->IsActivated() ? TEXT("yes") : TEXT("no"));

	// The idiomatic CommonUI close pattern: DeactivateWidget().
	// When a widget on a CommonActivatableWidgetStack is deactivated, the stack
	// automatically removes it and activates the next widget down (or root content).
	// Do NOT use RemoveWidgetFromLayer — it removes from the stack data structure
	// but deactivation is deferred, leaving the widget visually present.
	if (Widget->IsActivated())
	{
		Widget->DeactivateWidget();
	}
	else if (Widget->IsInViewport())
	{
		// Widget was added via AddToViewport (context menus, fallback path)
		Widget->RemoveFromParent();
	}
}

// =============================================================================
// PAWN COMPONENT ACCESS
// =============================================================================

UMOInventoryComponent* UMOUIControllerBase::GetCachedInventory() const
{
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		return UIManager->GetCachedInventory();
	}
	return nullptr;
}

UMOSkillsComponent* UMOUIControllerBase::GetCachedSkills() const
{
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		return UIManager->GetCachedSkills();
	}
	return nullptr;
}

UMOKnowledgeComponent* UMOUIControllerBase::GetCachedKnowledge() const
{
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		return UIManager->GetCachedKnowledge();
	}
	return nullptr;
}

UMOCraftingQueueComponent* UMOUIControllerBase::GetCachedCraftingQueue() const
{
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		return UIManager->GetCachedCraftingQueue();
	}
	return nullptr;
}

UMORecipeDiscoveryComponent* UMOUIControllerBase::GetCachedRecipeDiscovery() const
{
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		return UIManager->GetCachedRecipeDiscovery();
	}
	return nullptr;
}

UMOVitalsComponent* UMOUIControllerBase::GetCachedVitals() const
{
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		return UIManager->GetCachedVitals();
	}
	return nullptr;
}

UMOMetabolismComponent* UMOUIControllerBase::GetCachedMetabolism() const
{
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		return UIManager->GetCachedMetabolism();
	}
	return nullptr;
}

UMOMentalStateComponent* UMOUIControllerBase::GetCachedMentalState() const
{
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		return UIManager->GetCachedMentalState();
	}
	return nullptr;
}

UMOSurvivalStatsComponent* UMOUIControllerBase::GetCachedSurvivalStats() const
{
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		return UIManager->GetCachedSurvivalStats();
	}
	return nullptr;
}

UMONotificationComponent* UMOUIControllerBase::GetNotificationComponent() const
{
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		return UIManager->GetNotificationComponent();
	}
	return nullptr;
}
