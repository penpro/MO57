/**
 * =============================================================================
 * MOBugReportPanel.h - the in-game bug report form
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Focus panel behind the in-game menu's "Bug Report" button (a child of its FocusWindowSwitcher, found by widget). The player describes the problem; the panel hands a draft to
 * UMOBugReportSubsystem, which adds the game's state (build, system, session, clock, weather, player ...), the log tail and a screenshot, and uploads
 * the lot to the crash endpoint (see MOBugReport.h). This class only collects input and shows status -- all report logic lives in the subsystem.
 *
 * BLUEPRINT SETUP (WBP_BugReportPanel, parent UMOBugReportPanel; built by Content/Python/ui_specs/bug_report_panel.py, which also adds it to the
 * in-game menu's FocusWindowSwitcher):
 *
 *   Widget name           Type                          Required?
 *   -------------------   ---------------------------   ---------
 *   TitleInput            UEditableTextBox              YES   one line: what is wrong
 *   DescriptionInput      UMultiLineEditableTextBox     YES   what happened / what was expected
 *   StepsInput            UMultiLineEditableTextBox     no    how to make it happen again
 *   CategoryCombo         UComboBoxString               no    filled from UMOBugReportSubsystem::GetCategories()
 *   ContactInput          UEditableTextBox              no    optional way to reach the player
 *   IncludeLogCheck       UCheckBox                     no    attach the end of the session log (default on)
 *   IncludeScreenshotCheck UCheckBox                    no    attach the frame the panel was opened on (default on)
 *   PreviewButton         UMOCommonButton               no    "Preview": shows what will be sent
 *   PreviewText           UTextBlock                    no    the exact report text (UMOBugReportSubsystem::BuildReportText)
 *   StatusText            UTextBlock                    no    progress / result
 *   SendButton            UMOCommonButton               YES
 *   BackButton            UMOCommonButton               no
 *   DiscordButton         UMOCommonButton               no    opens the community link (UMOCommunitySettings::OpenBugReportLink)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES ARISE
 * =============================================================================
 *
 * [2026-10] DRAFT SURVIVES CLOSING: PrepareForDisplay() does NOT clear the fields -- a player who closes the panel by accident keeps what they typed.
 *   The form is cleared only after a report was accepted by the endpoint.
 *
 * [2026-10] SCREENSHOT IS TAKEN AT OPEN, NOT AT SEND: by the time Send is pressed the menu covers the game. PrepareForDisplay() asks the subsystem for a
 *   UI-less frame capture; it arrives one frame later, long before the player has typed anything.
 *
 * [2026-10] TEXT STYLE: the boxes go through UMOUIUtils::ApplyReadable(MultiLine)TextInputStyle like every other text field (the engine default is
 *   grey-on-grey); never UEditableTextBox::SetWidgetStyle (dangling style pointer).
 *
 * =============================================================================
 * RELATED FILES: MOBugReport.h (subsystem), MOBugReportBundle.h (bundle format), MOInGameMenu.h (host), MOCommunitySettings.h (Discord link)
 * LAST UPDATED: 2026-10-09
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOBugReport.h"
#include "MOUIDelegates.h"
#include "MOBugReportPanel.generated.h"

class UMOCommonButton;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UComboBoxString;
class UCheckBox;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOBugReportPanel : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	UMOBugReportPanel(const FObjectInitializer& ObjectInitializer);

	/** Fired when the panel wants to close (Back button / close key). */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|BugReport")
	FMOUIRequestClose OnRequestClose;

	/** Called by the host menu every time the panel is opened: fresh status, buttons enabled, a new screenshot. The typed text is kept. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|BugReport")
	void PrepareForDisplay();

	/** The form as a draft. */
	FMOBugReportDraft ReadDraft() const;

	/** Rebuild PreviewText from the form's current content. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|BugReport")
	void RefreshPreview();

	/** What clicking Send does (public so tests and the UI toolset can drive it). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|BugReport")
	void SendReport();

	/** Empty the form (after a successful send). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|BugReport")
	void ClearForm();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
	UFUNCTION() void HandleSendClicked();
	UFUNCTION() void HandleBackClicked();
	UFUNCTION() void HandleDiscordClicked();
	UFUNCTION() void HandlePreviewClicked();

	void HandleSubmitFinished(const FMOBugReportResult& Result);
	void SetStatus(const FString& Message) const;
	void SetBusy(bool bBusy);

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableTextBox> TitleInput;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMultiLineEditableTextBox> DescriptionInput;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMultiLineEditableTextBox> StepsInput;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UComboBoxString> CategoryCombo;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> ContactInput;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> IncludeLogCheck;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> IncludeScreenshotCheck;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> PreviewButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PreviewText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> SendButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> BackButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> DiscordButton;
};
