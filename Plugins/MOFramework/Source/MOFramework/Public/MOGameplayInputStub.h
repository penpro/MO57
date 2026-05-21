/**
 * =============================================================================
 * MOGameplayInputStub.h - RootContentWidgetClass for input restoration
 * =============================================================================
 *
 * PURPOSE:
 * This widget is set as the RootContentWidgetClass on each UCommonActivatableWidgetStack.
 * When the last menu is removed from a stack, CommonUI asks this widget for its
 * GetDesiredInputConfig() to restore input state.
 *
 * Without this, input state becomes undefined when all menus close.
 *
 * USAGE:
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

/**
 * Stub widget for restoring gameplay input when all menus close.
 *
 * This widget is never directly visible - it exists only to provide
 * the correct input configuration when widget stacks are empty.
 */
UCLASS()
class MOFRAMEWORK_API UMOGameplayInputStub : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UMOGameplayInputStub(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnActivated() override;

	/**
	 * Configure input mode for gameplay.
	 * Returns Game mode with mouse capture and hidden cursor.
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
};
