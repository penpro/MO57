/**
 * =============================================================================
 * MOModalBackground.h - Full-Screen Click Catcher for Modal Menus
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Invisible full-screen widget that catches clicks outside modal menus.
 * When clicked, broadcasts OnBackgroundClicked so menus can close themselves.
 * Used by UIControllers to implement click-outside-to-close behavior.
 *
 * USAGE:
 * 1. UIController creates/adds this widget to viewport
 * 2. UIController binds to OnBackgroundClicked
 * 3. Menu is added on top of background
 * 4. Click outside menu -> background catches it -> menu closes
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] Z-ORDER: Background MUST be added before menu widgets so it's
 *   behind them. If added after, it will catch all clicks.
 *
 * [2024-02] VISIBILITY: Widget is invisible but HitTestInvisible=false.
 *   RebuildWidget sets up a ColorBlock with 0 alpha.
 *
 * =============================================================================
 * RELATED FILES: MOUIControllerBase.h, MOContextMenuBase.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOModalBackground.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOModalBackgroundClickedSignature);

/**
 * Full-screen invisible widget that catches clicks outside menus.
 * When clicked, broadcasts OnBackgroundClicked so menus can close.
 */
UCLASS()
class MOFRAMEWORK_API UMOModalBackground : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOModalBackground(const FObjectInitializer& ObjectInitializer);

	/** Called when the background is clicked (outside any menu content). */
	UPROPERTY(BlueprintAssignable, Category="MO|UI")
	FMOModalBackgroundClickedSignature OnBackgroundClicked;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
