/**
 * =============================================================================
 * MOTutorialHintWidget.h - Tutorial Popup Banner
 * =============================================================================
 *
 * PURPOSE:
 * On-screen banner that surfaces the current tutorial step. Subscribes to
 * UMOQuestSubsystem::OnTutorialHintChanged; when an active tutorial quest has
 * a current objective with bShowAsTutorialPopup=true, the widget activates,
 * formats the hint text through FMOInputHintFormatter (so {key:...} tokens
 * resolve to live key glyphs), and shows the banner. When the objective is
 * completed (or the player presses F1 to skip / F3 to disable popups), the
 * widget deactivates and CommonUI fades it out.
 *
 * BLUEPRINT REQUIREMENTS (WBP_TutorialHint):
 * - Inherit this class
 * - Add a UCommonTextBlock named "TitleText"  (BindWidget)
 * - Add a UCommonTextBlock named "BodyText"   (BindWidget)
 * - Anchor/align at top-center of the canvas
 * - Drive Activated/Deactivated visibility via CommonUI fade transitions
 *
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOTutorialHintWidget.generated.h"

class UCommonTextBlock;
class UMOQuestSubsystem;

UCLASS(Abstract, BlueprintType)
class MOFRAMEWORK_API UMOTutorialHintWidget : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOTutorialHintWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Yield to menus: while true the banner is hidden even if a hint is active, and it comes back (still the current hint) when this goes false. Driven by
	 * UMOQuestUIController::SetHUDYieldedToMenus from the UI manager's menu open/close chokepoint. The state lives HERE because HandleTutorialHintChanged runs on its own
	 * (a quest objective completing while a menu is open must not pop the banner back over the menu).
	 */
	void SetSuppressedByMenu(bool bSuppress);
	bool IsSuppressedByMenu() const { return bSuppressedByMenu; }

	/** True while the quest subsystem has a tutorial hint to show (whether or not a menu is hiding it right now). */
	bool HasActiveHint() const { return bHaveHint; }

	/** The visibility rule in one pure function (unit-tested): nothing to show or a menu open -> Collapsed; otherwise the passive overlay visibility. */
	static ESlateVisibility ComputeVisibility(bool bHaveHint, bool bSuppressedByMenu);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Resolved title for the current hint. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Tutorial", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> TitleText;

	/** Resolved body for the current hint. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Tutorial", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> BodyText;

	/**
	 * Override to act as a passive overlay — no input mode change, no focus
	 * grab. The popup should never steal input from gameplay.
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

private:
	/** Re-pull the current hint from the quest subsystem and update widgets. */
	UFUNCTION()
	void HandleTutorialHintChanged();

	/** Cached subsystem we subscribe to. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UMOQuestSubsystem> BoundSubsystem;

	/** ID of the objective currently displayed — used to detect "the hint changed". */
	FName CurrentObjectiveId;

	/** True while the quest subsystem has a tutorial hint to show (independent of whether a menu is hiding it). */
	bool bHaveHint = false;

	/** True while a menu is open (SetSuppressedByMenu). */
	bool bSuppressedByMenu = false;
};
