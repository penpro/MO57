/**
 * =============================================================================
 * MOKeyBindingEntryWidget.h - Single Key Binding Row Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Widget representing a single rebindable key entry. Shows action name,
 * current key with capture selector, and reset button. Used in options panel.
 *
 * WIDGET BINDINGS (required in Blueprint):
 * - ActionNameText (UTextBlock) - Display name of the action
 * - KeySelector (UInputKeySelector) - Captures new key input
 * - ResetButton (UCommonButtonBase) - Reverts to default key
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] KEY CAPTURE: UInputKeySelector captures ALL input during capture
 *   mode. Escape cancels capture, any other key sets the binding.
 *
 * [2024-02] CONFLICT CHECK: OnKeyChanged fires BEFORE conflict checking.
 *   Parent panel should validate for duplicate bindings.
 *
 * [2024-02] DEFAULT KEY: ResetToDefault() uses cached default, not from
 *   InputMappingContext. Ensure defaults are cached at setup time.
 *
 * =============================================================================
 * RELATED FILES: MOKeyBindingTypes.h, MOOptionsPanel.h, MOKeyBindingManager.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOKeyBindingTypes.h"
#include "MOKeyBindingEntryWidget.generated.h"

class UTextBlock;
class UInputKeySelector;
class UCommonButtonBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOKeyBindingChangedSignature, FName, ActionId, FKey, NewKey);

/**
 * Entry widget for a single key binding row.
 * See file header for widget bindings and pitfalls.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOKeyBindingEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Setup the entry with binding data. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|KeyBinding")
	void SetupEntry(const FMOKeyBindingVisualData& InData);

	/** Reset this binding to default key. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|KeyBinding")
	void ResetToDefault();

	/** Get the action ID for this entry. */
	UFUNCTION(BlueprintPure, Category="MO|UI|KeyBinding")
	FName GetActionId() const { return EntryData.ActionId; }

	/** Get the current key for this entry. */
	UFUNCTION(BlueprintPure, Category="MO|UI|KeyBinding")
	FKey GetCurrentKey() const { return EntryData.CurrentKey; }

	/** Delegate fired when key binding changes. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|KeyBinding")
	FMOKeyBindingChangedSignature OnKeyChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Update visual state from entry data. */
	void UpdateVisuals();

	/** Called when visuals are updated. Blueprint can extend. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|KeyBinding")
	void OnVisualsUpdated(const FMOKeyBindingVisualData& Data);

private:
	UFUNCTION()
	void HandleKeySelected(FInputChord SelectedKey);

	UFUNCTION()
	void HandleResetClicked();

protected:
	// ============================================================================
	// WIDGET BINDINGS
	// ============================================================================

	/** Text displaying the action name. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ActionNameText;

	/** Key selector widget for capturing key input. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UInputKeySelector> KeySelector;

	/** Button to reset binding to default. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ResetButton;

private:
	/** Cached entry data. */
	FMOKeyBindingVisualData EntryData;
};
