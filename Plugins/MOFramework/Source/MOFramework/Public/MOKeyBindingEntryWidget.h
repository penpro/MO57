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
 * Displays action name, current key selector, and reset button.
 *
 * Widget Setup in Blueprint:
 * 1. Create WBP_KeyBindingEntry based on this class
 * 2. Add widgets with matching names:
 *    - ActionNameText (UTextBlock) - displays the action display name
 *    - KeySelector (UInputKeySelector) - handles key capture
 *    - ResetButton (UCommonButtonBase) - resets to default
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
