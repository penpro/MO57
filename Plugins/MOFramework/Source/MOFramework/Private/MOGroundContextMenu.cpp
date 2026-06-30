#include "MOGroundContextMenu.h"
#include "MOForagingSubsystem.h"
#include "MOCommonButton.h"
#include "MOCharacter.h"
#include "MOInterruptTypes.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogMOGroundMenu, Log, All);

UMOGroundContextMenu::UMOGroundContextMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOGroundContextMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button clicks
	BindButtonClick(SearchNearbyButton, this, &UMOGroundContextMenu::HandleSearchNearbyClicked);
	BindButtonClick(DigForSuppliesButton, this, &UMOGroundContextMenu::HandleDigForSuppliesClicked);

	// Reset timer and start mouse position checking
	MouseOutsideTimer = 0.0f;
	StartMouseCheckTimer();

	UE_LOG(LogMOGroundMenu, Verbose, TEXT("[MOGroundContextMenu] NativeConstruct"));
}

void UMOGroundContextMenu::NativeDestruct()
{
	// (H22) If the menu is torn down mid-action, cancel cleanly: stop the timer
	// and drop the interrupt registration so we don't leave a dangling listener
	// on the character. No items/XP are granted for an incomplete action.
	CancelForagingAction();

	StopMouseCheckTimer();
	Super::NativeDestruct();
}

void UMOGroundContextMenu::InitializeForLocation(FVector WorldLocation, APawn* InForagingPawn)
{
	TargetLocation = WorldLocation;
	ForagingPawn = InForagingPawn;

	// Get foraging subsystem
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMOGroundMenu, Warning, TEXT("[MOGroundContextMenu] No world available"));
		return;
	}

	UMOForagingSubsystem* ForagingSubsystem = World->GetSubsystem<UMOForagingSubsystem>();
	if (!ForagingSubsystem)
	{
		UE_LOG(LogMOGroundMenu, Warning, TEXT("[MOGroundContextMenu] No foraging subsystem available"));
		return;
	}

	// Get foraging level and calculate radius
	ForagingLevel = ForagingSubsystem->GetForagingLevel(InForagingPawn);
	SearchRadius = ForagingSubsystem->CalculateSearchRadius(ForagingLevel);

	// Update display
	UpdateDisplayText();

	UE_LOG(LogMOGroundMenu, Log, TEXT("[MOGroundContextMenu] Initialized at %s with radius %.0f (skill %d)"),
		*WorldLocation.ToString(), SearchRadius, ForagingLevel);
}

void UMOGroundContextMenu::HandleSearchNearbyClicked()
{
	UE_LOG(LogMOGroundMenu, Log, TEXT("[MOGroundContextMenu] Search Nearby clicked"));

	// (H22) Don't forage instantly. Start a timed, interruptible action; the
	// foraging primitive (which spawns items + grants XP) runs only on completion.
	StartForagingAction(EMOGroundForageAction::SearchNearby);
}

void UMOGroundContextMenu::HandleDigForSuppliesClicked()
{
	UE_LOG(LogMOGroundMenu, Log, TEXT("[MOGroundContextMenu] Dig for Supplies clicked"));

	// (H22) Don't dig instantly. Start a timed, interruptible action.
	StartForagingAction(EMOGroundForageAction::DigForSupplies);
}

// ============================================================================
// (H22) TIMED FORAGING ACTION
// ============================================================================

void UMOGroundContextMenu::StartForagingAction(EMOGroundForageAction Action)
{
	if (Action == EMOGroundForageAction::None)
	{
		return;
	}

	// Ignore a second click while an action is already running.
	if (IsForagingActionInProgress())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		RequestClose();
		return;
	}

	// Validate the subsystem exists up front so we fail fast (same as the old
	// instant path) rather than waiting a full duration only to no-op.
	if (!World->GetSubsystem<UMOForagingSubsystem>())
	{
		UE_LOG(LogMOGroundMenu, Warning, TEXT("[MOGroundContextMenu] No foraging subsystem"));
		RequestClose();
		return;
	}

	PendingForageAction = Action;
	ForageActionElapsed = 0.0f;

	// Disable the buttons so the action reads as "in progress" and can't be
	// re-triggered or switched mid-action.
	if (SearchNearbyButton)
	{
		SearchNearbyButton->SetIsEnabled(false);
	}
	if (DigForSuppliesButton)
	{
		DigForSuppliesButton->SetIsEnabled(false);
	}

	// Movement / damage / death etc. cancel the action with no reward.
	RegisterForagingInterrupts();

	// Drive the action via a repeating timer (cheap, ~20Hz like the mouse check).
	World->GetTimerManager().SetTimer(
		ForageActionTimerHandle,
		this,
		&UMOGroundContextMenu::TickForagingAction,
		0.05f,
		true);

	UE_LOG(LogMOGroundMenu, Log, TEXT("[MOGroundContextMenu] Started timed foraging action (%d) for %.1fs"),
		static_cast<int32>(Action), ForagingActionDuration);
}

void UMOGroundContextMenu::TickForagingAction()
{
	if (!IsForagingActionInProgress())
	{
		return;
	}

	ForageActionElapsed += 0.05f;

	if (ForageActionElapsed >= ForagingActionDuration)
	{
		FinishForagingAction();
	}
}

void UMOGroundContextMenu::FinishForagingAction()
{
	const EMOGroundForageAction Action = PendingForageAction;

	// Tear down action state/timers BEFORE running the payload so the menu can't
	// re-enter or self-cancel (e.g. RequestClose -> NativeDestruct).
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(ForageActionTimerHandle);
	}
	UnregisterForagingInterrupts();
	PendingForageAction = EMOGroundForageAction::None;
	ForageActionElapsed = 0.0f;

	UMOForagingSubsystem* ForagingSubsystem = World ? World->GetSubsystem<UMOForagingSubsystem>() : nullptr;
	if (!ForagingSubsystem)
	{
		UE_LOG(LogMOGroundMenu, Warning, TEXT("[MOGroundContextMenu] FinishForagingAction: No foraging subsystem"));
		RequestClose();
		return;
	}

	// Now the action has taken time — grant the result.
	if (Action == EMOGroundForageAction::SearchNearby)
	{
		TArray<AMOWorldItem*> RevealedItems = ForagingSubsystem->RevealHISMInstancesInRadius(
			TargetLocation, SearchRadius, ForagingPawn.Get());
		OnSearchComplete.Broadcast(RevealedItems);
	}
	else if (Action == EMOGroundForageAction::DigForSupplies)
	{
		TArray<AMOWorldItem*> DugItems = ForagingSubsystem->DigForSupplies(
			TargetLocation, ForagingLevel, ForagingPawn.Get());
		OnDigComplete.Broadcast(DugItems);
	}

	RequestClose();
}

void UMOGroundContextMenu::CancelForagingAction()
{
	if (!IsForagingActionInProgress())
	{
		return;
	}

	UE_LOG(LogMOGroundMenu, Log, TEXT("[MOGroundContextMenu] Foraging action cancelled (no items/XP granted)"));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ForageActionTimerHandle);
	}
	UnregisterForagingInterrupts();
	PendingForageAction = EMOGroundForageAction::None;
	ForageActionElapsed = 0.0f;

	// Re-enable buttons in case the menu somehow stays open.
	if (SearchNearbyButton)
	{
		SearchNearbyButton->SetIsEnabled(true);
	}
	if (DigForSuppliesButton)
	{
		DigForSuppliesButton->SetIsEnabled(true);
	}
}

void UMOGroundContextMenu::NotifyInterrupt_Implementation(const FMOInterruptContext& Context)
{
	if (!IsForagingActionInProgress())
	{
		return;
	}

	// Any meaningful disturbance cancels — foraging has no resumable state.
	// UserCancel is skipped to avoid double-handling with explicit UI close.
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
		CancelForagingAction();
		// The action was interrupted — close the menu too.
		RequestClose();
		break;

	case EMOInterruptReason::UserCancel:
	case EMOInterruptReason::None:
	default:
		break;
	}
}

void UMOGroundContextMenu::RegisterForagingInterrupts()
{
	AMOCharacter* Character = Cast<AMOCharacter>(ForagingPawn.Get());
	if (!IsValid(Character))
	{
		return;
	}

	Character->RegisterInterruptListener(this);
	RegisteredForageCharacter = Character;
}

void UMOGroundContextMenu::UnregisterForagingInterrupts()
{
	if (AMOCharacter* Character = RegisteredForageCharacter.Get())
	{
		Character->UnregisterInterruptListener(this);
	}
	RegisteredForageCharacter.Reset();
}

bool UMOGroundContextMenu::ShouldCloseOnMouseLeave() const
{
	// (H22) Keep the menu open while a timed action runs, mirroring the ghost
	// menu's build-timer guard, so cursor drift doesn't cancel a multi-second dig.
	return Super::ShouldCloseOnMouseLeave() && !IsForagingActionInProgress();
}

void UMOGroundContextMenu::UpdateDisplayText()
{
	if (RadiusText)
	{
		RadiusText->SetText(FText::Format(
			NSLOCTEXT("MOForaging", "RadiusFormat", "Range: {0}m"),
			FText::AsNumber(FMath::RoundToInt(SearchRadius / 100.0f)) // Convert to meters
		));
	}

	if (SkillLevelText)
	{
		SkillLevelText->SetText(FText::Format(
			NSLOCTEXT("MOForaging", "SkillFormat", "Foraging: {0}"),
			FText::AsNumber(ForagingLevel)
		));
	}
}

void UMOGroundContextMenu::SetMenuPosition(FVector2D ScreenPosition)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	// Get viewport size and place menu at center (where the reticle is)
	int32 ViewportX, ViewportY;
	PC->GetViewportSize(ViewportX, ViewportY);

	// Center of screen, offset slightly so cursor is inside the menu
	const float CenterX = ViewportX / 2.0f;
	const float CenterY = ViewportY / 2.0f;
	const float OffsetX = -50.0f;  // Offset left so menu appears to the right of center
	const float OffsetY = -30.0f;  // Offset up so menu appears below center

	// Get the DPI scale to convert viewport coords to widget coords
	const float DPIScale = UWidgetLayoutLibrary::GetViewportScale(PC);

	if (DPIScale > 0.0f)
	{
		const FVector2D AdjustedPosition = FVector2D(CenterX + OffsetX, CenterY + OffsetY) / DPIScale;
		SetPositionInViewport(AdjustedPosition, false);
	}
	else
	{
		SetPositionInViewport(FVector2D(CenterX + OffsetX, CenterY + OffsetY), false);
	}
}

bool UMOGroundContextMenu::IsMouseOverMenu() const
{
	if (!ButtonContainer)
	{
		return false;
	}

	// Get absolute cursor position (screen space)
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	const FVector2D AbsoluteMousePos = FSlateApplication::Get().GetCursorPos();

	// Get the widget's geometry and check if mouse is inside
	const FGeometry& Geometry = ButtonContainer->GetCachedGeometry();
	const FVector2D LocalMousePos = Geometry.AbsoluteToLocal(AbsoluteMousePos);
	const FVector2D LocalSize = Geometry.GetLocalSize();

	return LocalMousePos.X >= 0.0f && LocalMousePos.X <= LocalSize.X &&
	       LocalMousePos.Y >= 0.0f && LocalMousePos.Y <= LocalSize.Y;
}

void UMOGroundContextMenu::StartMouseCheckTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Check mouse position every 0.05 seconds (20 times per second)
	World->GetTimerManager().SetTimer(
		MouseCheckTimerHandle,
		this,
		&UMOGroundContextMenu::CheckMousePosition,
		0.05f,
		true  // Looping
	);

	UE_LOG(LogMOGroundMenu, Verbose, TEXT("[MOGroundContextMenu] Started mouse check timer"));
}

void UMOGroundContextMenu::StopMouseCheckTimer()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(MouseCheckTimerHandle);
	}
}

void UMOGroundContextMenu::CheckMousePosition()
{
	// (H22) Never auto-close (and thus cancel) while a timed foraging action is
	// running — the player committed to the action by clicking. Mirrors the
	// ShouldCloseOnMouseLeave() guard for this menu's own timer-based close path.
	if (IsForagingActionInProgress())
	{
		MouseOutsideTimer = 0.0f;
		return;
	}

	const bool bMouseOver = IsMouseOverMenu();

	if (!bMouseOver)
	{
		MouseOutsideTimer += 0.05f;  // Timer interval
		if (MouseOutsideTimer >= AutoCloseDelay)
		{
			UE_LOG(LogMOGroundMenu, Log, TEXT("[MOGroundContextMenu] Auto-closing - mouse outside for %.2f seconds"), MouseOutsideTimer);
			RequestClose();
		}
	}
	else
	{
		MouseOutsideTimer = 0.0f;
	}
}
