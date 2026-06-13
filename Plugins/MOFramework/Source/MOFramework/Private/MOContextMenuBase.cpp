#include "MOContextMenuBase.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOGameUIManagerSubsystem.h"
#include "MOUIDebugSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"
#include "Input/Events.h"

// ============================================================================
// FMOContextMenuClickOutsideHandler — Slate input pre-processor
// ============================================================================
//
// Captures every mouse-button-down event globally and routes "outside the menu"
// clicks to UMOContextMenuBase::HandleClickOutside(). Designed to be registered
// in NativeConstruct and unregistered in NativeDestruct, so its lifetime is
// strictly the menu's lifetime — there's no risk of leaking a stale handler.
//
// We never RETURN true (consume) from HandleMouseButtonDownEvent because:
//   1. The click that opens menu A might end up registering this handler before
//      Slate finishes processing the same click — we'd swallow our own opener.
//      A short opening grace period (TimeSinceConstruct) avoids that race.
//   2. Letting the click flow through to the world means right-clicking a new
//      tree closes the old menu AND opens the new one in a single click,
//      instead of forcing the player to click twice.

FMOContextMenuClickOutsideHandler::FMOContextMenuClickOutsideHandler(UMOContextMenuBase* InMenu)
	: MenuWeak(InMenu)
{
}

bool FMOContextMenuClickOutsideHandler::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	UMOContextMenuBase* Menu = MenuWeak.Get();
	if (!Menu)
	{
		return false;
	}

	if (!Menu->IsReadyForClickOutsideClose())
	{
		return false;
	}

	if (Menu->IsScreenPositionInsideMenu(MouseEvent.GetScreenSpacePosition()))
	{
		return false;
	}

	Menu->HandleClickOutside();
	return false;
}

UMOContextMenuBase::UMOContextMenuBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Enable keyboard input for this widget (needed for Tab/Escape to close)
	SetIsFocusable(true);

	// Note: Context menus are NOT activatable widgets (UCommonUserWidget)
	// They use AddToViewport() and handle their own key events via NativeOnKeyDown
}

void UMOContextMenuBase::NativeConstruct()
{
	MOUI_LOG(this, "ContextMenu", "CONSTRUCT  %s", *GetName());
	Super::NativeConstruct();

	TimeSinceConstruct = 0.0f;

	// Single source of truth: cursor + Game-and-UI input lives on the
	// subsystem. See UMOGameUIManagerSubsystem::ApplyContextMenuInputState.
	if (APlayerController* PC = GetOwningPlayer())
	{
		bWasCursorVisible = PC->bShowMouseCursor;
		MOUI_LOG(this, "ContextMenu", "  %s: cursor was %s, applying context-menu input state",
			*GetName(), bWasCursorVisible ? TEXT("VISIBLE") : TEXT("hidden"));
		UMOGameUIManagerSubsystem::ApplyContextMenuInputState(PC);
	}

	// Hook into Slate's input pipeline so we see clicks that land outside this
	// widget's bounds. Without this, a click anywhere outside the menu just
	// hits the world (or another widget) and the menu stays open forever.
	if (bCloseOnClickOutside && FSlateApplication::IsInitialized())
	{
		ClickOutsideHandler = MakeShared<FMOContextMenuClickOutsideHandler>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(ClickOutsideHandler);
		MOUI_LOG(this, "ContextMenu", "  %s: registered click-outside handler", *GetName());
	}

	// H45: claim keyboard focus so the advertised Tab/Escape close (NativeOnKeyDown)
	// works immediately — without it, key events only reach this menu after the
	// player clicks it, and a stray Escape under GameAndUI opens the in-game menu
	// on top of a fresh context menu. Centralized here so EVERY context menu gets
	// it (previously only the ghost menu claimed focus, at its own spawn site).
	//
	// Deferred one tick: at NativeConstruct the Slate tree isn't laid out and the
	// right-click that spawned us is still settling — a synchronous claim can be
	// clobbered. Next tick is strictly more reliable for focus.
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UMOContextMenuBase> WeakThis(this);
		World->GetTimerManager().SetTimerForNextTick([WeakThis]()
		{
			UMOContextMenuBase* Self = WeakThis.Get();
			if (Self && Self->IsInViewport())
			{
				Self->SetKeyboardFocus();
				MOUI_LOG(Self, "ContextMenu", "  %s: claimed keyboard focus (deferred)", *Self->GetName());
			}
		});
	}
}

void UMOContextMenuBase::NativeDestruct()
{
	MOUI_LOG(this, "ContextMenu", "DESTRUCT   %s", *GetName());

	// Always unregister the click-outside handler before tearing down. Leaving
	// it registered after destruction would still be safe (it holds a weak
	// pointer), but Slate would keep dispatching pointless callbacks every click.
	if (ClickOutsideHandler.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(ClickOutsideHandler);
		MOUI_LOG(this, "ContextMenu", "  %s: unregistered click-outside handler", *GetName());
	}
	ClickOutsideHandler.Reset();

	// Single source of truth for context-menu teardown input handling:
	// - No other menu open → restore game mode + viewport focus
	// - Menu still open  → refocus the topmost active widget (so the
	//   underlying menu's Tab/Escape keys come alive again)
	// See UMOGameUIManagerSubsystem::RestoreContextMenuInputState.
	if (APlayerController* PC = GetOwningPlayer())
	{
		UMOGameUIManagerSubsystem::RestoreContextMenuInputState(this, PC);
		MOUI_LOG(this, "ContextMenu", "  %s: invoked RestoreContextMenuInputState", *GetName());
	}

	Super::NativeDestruct();
}

void UMOContextMenuBase::SetPopupPosition(FVector2D ScreenPosition)
{
	// Use SetPositionInViewport for reliable screen positioning
	// Pass true to remove DPI scale since our coordinates are in screen pixels
	UE_LOG(LogMOFramework, Log, TEXT("[MOContextMenuBase] SetPopupPosition called: (%f, %f), IsInViewport: %s"),
		ScreenPosition.X, ScreenPosition.Y, IsInViewport() ? TEXT("true") : TEXT("false"));

	SetPositionInViewport(ScreenPosition, true);

	// Warp the mouse cursor to a point just inside the menu's top-left so
	// mouse-enter fires immediately on open. Without this, the cursor stays
	// wherever the player right-clicked (often outside the menu) and the
	// "close on mouse-leave" timer never gets the enter-event it needs to
	// be primed — the menu would behave inconsistently with no clear "you
	// were over the menu" state.
	//
	// SetMouseLocation takes viewport pixels (same coordinate space as the
	// ScreenPosition we just used for positioning). A small inset (5px)
	// keeps us solidly inside, immune to float rounding at the boundary.
	if (bWarpCursorOnOpen)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			const FVector2D CursorPos = ScreenPosition + FVector2D(CursorWarpInsetPx, CursorWarpInsetPx);
			PC->SetMouseLocation(FMath::RoundToInt(CursorPos.X), FMath::RoundToInt(CursorPos.Y));
		}
	}
}

void UMOContextMenuBase::RequestClose()
{
	OnCloseRequested.Broadcast();
}

FReply UMOContextMenuBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Close on Escape or Tab
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Tab)
	{
		RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMOContextMenuBase::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	bIsMouseOver = true;
	MouseLeaveTimer = 0.0f; // Cancel any pending close
}

void UMOContextMenuBase::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	bIsMouseOver = false;

	// Start grace timer if close-on-mouse-leave is enabled
	if (ShouldCloseOnMouseLeave())
	{
		MouseLeaveTimer = MouseLeaveGraceTime;
	}
}

void UMOContextMenuBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TimeSinceConstruct += InDeltaTime;

	// Handle mouse leave grace timer
	if (MouseLeaveTimer > 0.0f && !bIsMouseOver)
	{
		MouseLeaveTimer -= InDeltaTime;
		if (MouseLeaveTimer <= 0.0f)
		{
			MouseLeaveTimer = 0.0f;
			RequestClose();
		}
	}
}

bool UMOContextMenuBase::ShouldCloseOnMouseLeave() const
{
	return bCloseOnMouseLeave;
}

bool UMOContextMenuBase::IsScreenPositionInsideMenu(const FVector2D& ScreenPosition) const
{
	// Use cached geometry — this is the same data IsHovered() uses internally,
	// and reflects the layout produced by the most recent paint pass.
	const FGeometry& Geo = GetCachedGeometry();

	// IsUnderLocation expects ABSOLUTE coordinates (FSlateApplication's screen
	// space). FPointerEvent::GetScreenSpacePosition already returns absolute,
	// so this is the right test.
	return Geo.IsUnderLocation(ScreenPosition);
}

bool UMOContextMenuBase::IsReadyForClickOutsideClose() const
{
	// The same right-click that opens the menu is still being processed by Slate
	// when we get registered. If we react to that mouse-button-down event, we'd
	// immediately close the menu on the click that just spawned it. A small
	// grace window — long enough for one full input-frame to elapse — is the
	// simplest, frame-rate-agnostic way to dodge that race.
	constexpr float OpeningClickGracePeriod = 0.10f;
	return TimeSinceConstruct >= OpeningClickGracePeriod;
}

void UMOContextMenuBase::HandleClickOutside()
{
	MOUI_LOG(this, "ContextMenu", "%s: click-outside detected -> RequestClose", *GetName());
	RequestClose();
}
