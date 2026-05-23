/**
 * MOGameplayInputStub.cpp - RootContentWidgetClass for input restoration
 */

#include "MOGameplayInputStub.h"
#include "MOFramework.h"
#include "MOActivatableWidget.h"
#include "MOUIDebugSubsystem.h"
#include "MOUIManagerComponent.h"
#include "MOGameUIManagerSubsystem.h"
#include "MOPrimaryGameLayout.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"

UMOGameplayInputStub::UMOGameplayInputStub(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The stub is a placeholder, not a real UI surface. UCommonActivatableWidget
	// would otherwise cache the currently-focused widget on deactivate and
	// restore it on re-activate — but the stub was first activated at game
	// start when the cached "focus" was the viewport, so every later
	// re-activation (every time a stack empties) re-applies viewport focus
	// even when another layer still has an active menu underneath. Disable
	// the restoration so this stub never touches focus state.
	bAutoRestoreFocus = false;
}

void UMOGameplayInputStub::NativeOnActivated()
{
	MOUI_LOG(this, "Stub", "GameplayInputStub ACTIVATED on layer (stack now empty)");

	// Capture the pre-Super focus so we can detect/log what Super::NativeOnActivated
	// does to focus. UCommonActivatableWidget activation in an empty container
	// calls SetUserFocus on the (focus-target-less) stub, which Slate decays to
	// the game viewport — exactly the focus-loss symptom we then have to undo.
	TSharedPtr<SWidget> PreSuperFocus;
	if (FSlateApplication::IsInitialized())
	{
		PreSuperFocus = FSlateApplication::Get().GetUserFocusedWidget(0);
	}
	MOUI_LOG(this, "Stub", "  pre-Super focus: %s",
		PreSuperFocus.IsValid() ? *PreSuperFocus->GetTypeAsString() : TEXT("<none>"));

	Super::NativeOnActivated();

	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<SWidget> PostSuperFocus = FSlateApplication::Get().GetUserFocusedWidget(0);
		MOUI_LOG(this, "Stub", "  post-Super focus: %s",
			PostSuperFocus.IsValid() ? *PostSuperFocus->GetTypeAsString() : TEXT("<none>"));
	}

	// SYMMETRY WITH GetDesiredInputConfig: stubs sit on every layer as
	// RootContentWidget. Whenever one layer's stack becomes empty (e.g. the
	// Modal layer after the rename modal pops), its stub re-activates.
	//
	// Two problems Super::NativeOnActivated created if other layers still
	// have a real menu active:
	//   1. Cursor + reticle stomp — we used to unconditionally hide cursor
	//      and reset reticle here. Now gated on GetActiveMenuCount.
	//   2. Slate focus drift — Super calls SetUserFocus on the stub (a
	//      widget with no focus target), and Slate falls back to the game
	//      viewport. Symptom: player has to click the underlying menu to
	//      restore keyboard focus.
	//
	// Both must be addressed. Only the truly-empty case (no menus open
	// anywhere) gets the gameplay-mode reset the stub was designed for.
	const UMOGameUIManagerSubsystem* Subsystem = UMOGameUIManagerSubsystem::Get(GetWorld());
	const UMOPrimaryGameLayout* Layout = Subsystem ? Subsystem->GetRootLayout() : nullptr;
	if (Subsystem && Layout && Layout->GetActiveMenuCount() > 0)
	{
		// Undo Super's focus theft: walk the topmost active widget's
		// desired-focus-target chain and place focus back on the menu's
		// real leaf (a button, text input, etc). Without this, focus is
		// stuck on the viewport even though the menu is still showing.
		if (UMOActivatableWidget* TopActive = Subsystem->GetTopmostActiveWidget())
		{
			MOUI_LOG(this, "Stub", "  reclaiming focus for active menu %s", *TopActive->GetName());
			TopActive->ClaimFocusForReactivation();
		}
		else
		{
			MOUI_LOG(this, "Stub", "  GetTopmostActiveWidget returned null despite ActiveMenuCount=%d",
				Layout->GetActiveMenuCount());
		}

		MOUI_LOG(this, "Stub", "  deferring cursor/reticle reset — %d menu(s) still active",
			Layout->GetActiveMenuCount());
		MOUI_DUMP_STATE_LITERAL(this, "After GameplayInputStub activate (deferred)");
		return;
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		MOUI_LOG(this, "Stub", "  setting bShowMouseCursor=false (was %s)",
			PC->bShowMouseCursor ? TEXT("true") : TEXT("false"));
		PC->bShowMouseCursor = false;

		// Restore reticle visibility — progress widgets and menus may have hidden it.
		if (UMOUIManagerComponent* UIMgr = PC->FindComponentByClass<UMOUIManagerComponent>())
		{
			UIMgr->RequestUpdateReticleVisibility();
			MOUI_LOG(this, "Stub", "  called RequestUpdateReticleVisibility");
		}
	}

	MOUI_DUMP_STATE_LITERAL(this, "After GameplayInputStub activate");
}

TOptional<FUIInputConfig> UMOGameplayInputStub::GetDesiredInputConfig() const
{
	// One stub sits on every layer of the primary game layout as the
	// RootContentWidget — they activate whenever their stack is empty. If
	// every stub claimed Game-mode input config unconditionally, then the
	// stub on the topmost empty layer (e.g. Modal at z=200) would beat any
	// active menu on a lower layer (e.g. Main Menu at Menu, z=150) for
	// "leaf-most node" in CommonUI's action router — applying Game mode and
	// hiding the cursor. That's why the main menu had no input after the
	// primary-layout migration: empty Modal/etc. layer stubs were trumping
	// the populated Menu layer's main menu.
	//
	// Fix: only claim Game-mode config when there's literally nothing else
	// active across all layers. Querying GetActiveMenuCount (which excludes
	// stubs themselves) tells us if a real widget is showing somewhere.
	// If yes → return empty so CommonUI defers to that widget's config.
	// If no → return Game mode (the original intent, for in-game when all
	// menus close).
	const UWorld* World = GetWorld();
	const UMOGameUIManagerSubsystem* Subsystem = UMOGameUIManagerSubsystem::Get(World);
	const UMOPrimaryGameLayout* Layout = Subsystem ? Subsystem->GetRootLayout() : nullptr;
	if (Layout && Layout->GetActiveMenuCount() > 0)
	{
		// Defer to the active widget. Returning empty TOptional means
		// CommonUI's action router skips this node when picking a leaf.
		return TOptional<FUIInputConfig>();
	}

	// No other UI active — apply Game mode (this is the in-game "all menus
	// closed, return to gameplay" case the stub was designed for).
	return FUIInputConfig(
		ECommonInputMode::Game,
		EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown,
		EMouseLockMode::DoNotLock,
		true  // bHideCursorDuringViewportCapture - cursor hidden in gameplay
	);
}
