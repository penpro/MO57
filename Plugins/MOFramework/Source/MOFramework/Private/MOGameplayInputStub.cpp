/**
 * MOGameplayInputStub.cpp - RootContentWidgetClass for input restoration
 */

#include "MOGameplayInputStub.h"
#include "MOFramework.h"
#include "MOUIDebugSubsystem.h"
#include "MOUIManagerComponent.h"
#include "GameFramework/PlayerController.h"

UMOGameplayInputStub::UMOGameplayInputStub(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOGameplayInputStub::NativeOnActivated()
{
	MOUI_LOG(this, "Stub", "GameplayInputStub ACTIVATED (stack now empty — restoring game mode)");
	Super::NativeOnActivated();

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
	// Restore full gameplay input when no menus are open
	// Game mode = input goes to game only
	// CaptureDuringMouseDown = normal FPS mouse behavior
	// DoNotLock = mouse can leave window (standard for windowed mode)
	// true = hide cursor during viewport capture
	return FUIInputConfig(
		ECommonInputMode::Game,
		EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown,
		EMouseLockMode::DoNotLock,
		true  // bHideCursorDuringViewportCapture - cursor hidden in gameplay
	);
}
