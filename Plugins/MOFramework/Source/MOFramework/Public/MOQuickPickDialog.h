/**
 * =============================================================================
 * MOQuickPickDialog.h - A small modal that offers a handful of choices as buttons
 * =============================================================================
 *
 * PURPOSE:
 * "Pick one of these": the dev `weather` and `time` popups today, any short list of choices tomorrow. The caller hands over a list of options (a stable Id and a
 * label each); the dialog lays them out as buttons in a grid, broadcasts the Id of the one that is clicked, and closes. Cancel / Escape closes it with OnCancelled.
 * Adding another picker is one more option list at the call site -- no new widget, no new Blueprint.
 *
 * BLUEPRINT SETUP: WBP_QuickPickModal (built by Content/Python/ui_specs/quick_pick.py) with parent UMOQuickPickDialog. Required names: TitleText, OptionsGrid,
 * CancelButton. Optional: MessageText. OptionButtonClass must be set to WBP_MOCommonButton (the spec's class_defaults do it).
 *
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * - The option buttons are created at run time from OptionButtonClass, a WIDGET BLUEPRINT. A native UMOCommonButton has no widget tree: it would be an empty,
 *   hit-test-invisible spacer (see Docs/UI_TOOLING.md).
 * - Results are one-shot per open. PushModalWidget recycles pooled instances across unrelated consumers, so every delegate is cleared when the dialog deactivates.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOQuickPickDialog.generated.h"

class UTextBlock;
class UUniformGridPanel;
class UMOCommonButton;

/** One choice in a quick-pick dialog. */
USTRUCT(BlueprintType)
struct FMOQuickPickOption
{
	GENERATED_BODY()

	/** What the caller gets back when this option is clicked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|QuickPick")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|QuickPick")
	FText Label;

	/** Shown when the pointer rests on the button (optional). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|QuickPick")
	FText Tooltip;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FMOQuickPickChosen, FName /*OptionId*/);
DECLARE_MULTICAST_DELEGATE(FMOQuickPickCancelled);

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOQuickPickDialog : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOQuickPickDialog(const FObjectInitializer& ObjectInitializer);

	/**
	 * Fill the dialog: title, an optional line under it, one button per option laid out row by row in `Columns` columns. Replaces whatever a previous open left behind.
	 */
	void Configure(const FText& Title, const FText& Message, const TArray<FMOQuickPickOption>& Options, int32 Columns = 2);

	/** Broadcast with the clicked option's Id, just before the dialog closes. */
	FMOQuickPickChosen OnChosen;

	/** Broadcast when the dialog is cancelled (Cancel button, Escape/Tab). */
	FMOQuickPickCancelled OnCancelled;

	/** The buttons currently shown (for tests and UI tooling). */
	const TArray<TObjectPtr<UMOCommonButton>>& GetOptionButtons() const { return OptionButtons; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnCloseKeyRequested(const FKeyEvent& InKeyEvent) override;

	/** Menu input mode: a modal blocks gameplay input until it is answered. */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	/** The Widget Blueprint used for every option button (WBP_MOCommonButton). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MO|UI|QuickPick")
	TSubclassOf<UMOCommonButton> OptionButtonClass;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> OptionsGrid;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMOCommonButton> CancelButton;

private:
	void ClearOptions();
	void HandleOptionClicked(FName OptionId);
	void HandleCancelClicked();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMOCommonButton>> OptionButtons;
};
