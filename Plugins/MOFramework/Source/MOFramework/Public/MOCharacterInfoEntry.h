#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOCharacterInfoEntry.generated.h"

class UTextBlock;
class UMOCommonButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOInfoEntryChangeRequestedSignature, FName, FieldId);

/**
 * Widget for displaying a single character info field (name, age, gender, etc.)
 * with an optional "Change" button for editable fields.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOCharacterInfoEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Initialize the entry with field data. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|CharacterInfo")
	void InitializeEntry(FName InFieldId, const FText& Label, const FText& Value, bool bCanChange);

	/** Update just the value. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|CharacterInfo")
	void SetValue(const FText& Value);

	/** Get the field ID. */
	UFUNCTION(BlueprintPure, Category="MO|UI|CharacterInfo")
	FName GetFieldId() const { return FieldId; }

	/** Check if this field is editable. */
	UFUNCTION(BlueprintPure, Category="MO|UI|CharacterInfo")
	bool CanChange() const { return bIsEditable; }

	/** Called when user clicks the Change button. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|CharacterInfo")
	FMOInfoEntryChangeRequestedSignature OnChangeRequested;

protected:
	virtual void NativeConstruct() override;

	/** Blueprint event when entry is initialized. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|CharacterInfo")
	void OnEntryInitialized(FName InFieldId, const FText& Label, const FText& Value, bool bCanChange);

	// Bound widgets
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> ChangeButton;

private:
	UFUNCTION()
	void HandleChangeClicked();

	FName FieldId;
	bool bIsEditable = false;
};
