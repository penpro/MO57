/**
 * =============================================================================
 * MOCharacterInfoEntry.h - Character Info Field Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Widget displaying a single character info field (name, age, gender) with
 * optional inline editing. Used in character sheets and possession menus.
 * Shows label + value in display mode, switches to text input in edit mode.
 *
 * DISPLAY MODE:
 * - LabelText: Field name (e.g., "Name:")
 * - ValueText: Current value (e.g., "John Smith")
 * - ChangeButton: Enters edit mode (hidden if !bCanChange)
 *
 * EDIT MODE:
 * - EditTextBox: Text input for new value
 * - ConfirmButton: Saves changes, broadcasts OnValueChanged
 * - CancelButton: Reverts to display mode without saving
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] OPTIONAL WIDGETS: All widgets are BindWidgetOptional. Check null
 *   before use. Minimal Blueprint can have just LabelText + ValueText.
 *
 * [2024-02] TEXT COMMIT: HandleTextCommitted fires on Enter key or focus loss.
 *   OnEnter commits, OnClear cancels. Other commit types are ignored.
 *
 * [2024-02] ABSTRACT CLASS: Must create Blueprint subclass with widget layout.
 *
 * =============================================================================
 * RELATED FILES: MOCharacterUIController.h, MOPossessionMenu.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOCharacterInfoEntry.generated.h"

class UTextBlock;
class UEditableTextBox;
class UMOCommonButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOInfoEntryChangeRequestedSignature, FName, FieldId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOInfoEntryValueChangedSignature, FName, FieldId, const FString&, NewValue);

/**
 * Widget for displaying a single character info field (name, age, gender, etc.)
 * with an optional "Change" button for editable fields.
 *
 * When "Change" is clicked, switches to edit mode showing a text input.
 * When confirmed, broadcasts the new value.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOCharacterInfoEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Initialize the entry with field data. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|CharacterInfo")
	void InitializeEntry(FName InFieldId, const FText& Label, const FText& Value, bool bCanChange);

	/** Update just the value (display mode). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|CharacterInfo")
	void SetValue(const FText& Value);

	/** Get the field ID. */
	UFUNCTION(BlueprintPure, Category="MO|UI|CharacterInfo")
	FName GetFieldId() const { return FieldId; }

	/** Check if this field is editable. */
	UFUNCTION(BlueprintPure, Category="MO|UI|CharacterInfo")
	bool CanChange() const { return bIsEditable; }

	/** Check if currently in edit mode. */
	UFUNCTION(BlueprintPure, Category="MO|UI|CharacterInfo")
	bool IsEditing() const { return bIsInEditMode; }

	/** Enter edit mode (show text input). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|CharacterInfo")
	void EnterEditMode();

	/** Exit edit mode without saving. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|CharacterInfo")
	void CancelEdit();

	/** Confirm the edit and broadcast the new value. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|CharacterInfo")
	void ConfirmEdit();

	/** Called when user clicks the Change button (enters edit mode). */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|CharacterInfo")
	FMOInfoEntryChangeRequestedSignature OnChangeRequested;

	/** Called when user confirms a new value. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|CharacterInfo")
	FMOInfoEntryValueChangedSignature OnValueChanged;

protected:
	virtual void NativeConstruct() override;

	/** Blueprint event when entry is initialized. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|CharacterInfo")
	void OnEntryInitialized(FName InFieldId, const FText& Label, const FText& Value, bool bCanChange);

	/** Blueprint event when entering edit mode. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|CharacterInfo")
	void OnEditModeEntered();

	/** Blueprint event when exiting edit mode. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|CharacterInfo")
	void OnEditModeExited();

	// Bound widgets - Display mode
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> ChangeButton;

	// Bound widgets - Edit mode
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> EditTextBox;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> ConfirmButton;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CancelButton;

private:
	UFUNCTION()
	void HandleChangeClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	/** Update widget visibility based on edit mode and editability. */
	void UpdateWidgetVisibility();

	FName FieldId;
	FString CurrentValue;
	bool bIsEditable = false;
	bool bIsInEditMode = false;
};
