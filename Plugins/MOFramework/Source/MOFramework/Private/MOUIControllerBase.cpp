#include "MOUIControllerBase.h"
#include "MOUIManagerComponent.h"
#include "MONotificationComponent.h"
#include "GameFramework/PlayerController.h"

UMOUIControllerBase::UMOUIControllerBase()
{
	PrimaryComponentTick.bCanEverTick = false;
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

void UMOUIControllerBase::ApplyInputModeForMenuOpen(UUserWidget* MenuWidget)
{
	// Delegate to UIManager for centralized input mode handling
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		UIManager->RequestInputModeForMenuOpen(MenuWidget);
	}
}

void UMOUIControllerBase::ApplyInputModeForMenuClosed()
{
	// Delegate to UIManager for centralized input mode handling
	if (UMOUIManagerComponent* UIManager = GetUIManager())
	{
		UIManager->RequestInputModeForMenuClosed();
	}
}

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
