/**
 * =============================================================================
 * MOGameplayInputStub.h - RootContentWidgetClass for layer stacks
 * =============================================================================
 *
 * PURPOSE:
 * Sentinel widget assigned as the RootContentWidgetClass on each
 * UCommonActivatableWidgetStack in WBP_PrimaryGameLayout. When a stack is
 * empty (no menus pushed), CommonUI activates this stub as the stack's
 * default "active" widget.
 *
 * The stub's job is to claim Game-mode input config (cursor hidden, mouse
 * captured, no UI input) **but only when no other UI is active anywhere**.
 * That nuance is the load-bearing detail — see SYSTEMIC NOTE below.
 *
 * =============================================================================
 * SYSTEMIC NOTE - why GetDesiredInputConfig defers to other layers
 * =============================================================================
 *
 * CommonUI's action router picks the "leaf-most node" across all layer
 * stacks and applies its input config. Stacks are evaluated in z-order;
 * the topmost active widget wins.
 *
 * Each layer has its own stub instance. If every stub unconditionally
 * claimed Game-mode input, the topmost EMPTY layer's stub (e.g. Modal at
 * z=200) would trump a populated lower layer (e.g. Menu at z=150 hosting
 * the main menu). The leaf-most picker doesn't care that the lower layer
 * has a real widget — it picks whatever's highest with a focusable target.
 *
 * That's exactly what happened during the main-menu → layer-stack
 * migration: every empty layer's stub kept hiding the cursor and capturing
 * mouse while the main menu sat below trying to be interactive.
 *
 * Fix: stub.GetDesiredInputConfig() queries UMOPrimaryGameLayout::
 * GetActiveMenuCount() (which excludes stubs) and returns an empty
 * TOptional if anything real is active. CommonUI then defers to that real
 * widget. Stubs only claim input when truly nothing else is showing — the
 * "all menus closed in-game" case the stub was designed for.
 *
 * APPLIES TO EVERY STUB ON EVERY LAYER:
 * The check lives in this base class. All stub instances inherit it
 * automatically. No per-layer customization needed.
 *
 * ALTERNATIVE ARCHITECTURE (consider later):
 * The cleanest design would be to have a stub on ONLY ONE layer (the
 * bottommost, e.g. Game at z=50) and leave higher layers without a
 * RootContentWidget. Empty stacks would have nothing to claim leaf-most
 * priority, the bottom stub would always win when nothing's pushed
 * elsewhere, and the GetActiveMenuCount query wouldn't be needed.
 *
 * That requires editing WBP_PrimaryGameLayout to remove the stub from the
 * Modal/Menu/GameOverlay/HUD layers' RootContentWidget slot. Left as a
 * BP-level cleanup item; the runtime fix here works without the BP edit.
 *
 * =============================================================================
 * USAGE (Blueprint):
 * Set WBP_GameplayInputStub (Blueprint child of this class) as the
 * RootContentWidgetClass on each UCommonActivatableWidgetStack in
 * WBP_PrimaryGameLayout.
 *
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CommonInputModeTypes.h"
#include "MOGameplayInputStub.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOGameplayInputStub : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UMOGameplayInputStub(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnActivated() override;

	/**
	 * Returns Game-mode config ONLY when no other real UI is active across
	 * any layer. Otherwise returns empty TOptional so CommonUI defers to
	 * the real active widget's config. See SYSTEMIC NOTE in file header.
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
};
