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
};
