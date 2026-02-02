#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOBuildingTypes.h"
#include "MOBuildWidget.generated.h"

class AMOBuildableActor;
class UTextBlock;
class UImage;
class UCheckBox;
class UMOCommonButton;
class UProgressBar;

/**
 * Delegate for when the build widget requests close.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOBuildWidgetRequestCloseSignature);

/**
 * Delegate for when the start build button is clicked.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOnStartBuildSignature);

/**
 * Widget shown when interacting with a ghost building.
 * Allows configuration of material sources and starting construction.
 */
UCLASS()
class MOFRAMEWORK_API UMOBuildWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UMOBuildWidget(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when the widget should be closed. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOBuildWidgetRequestCloseSignature OnRequestClose;

	/** Broadcast when the start build button is clicked. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOOnStartBuildSignature OnStartBuild;

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize the widget for a specific building.
	 * @param Target - The buildable actor to configure
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void InitializeForBuilding(AMOBuildableActor* Target);

	/**
	 * Get the current build options based on checkbox states.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	FMOBuildProgress GetBuildOptions() const;

protected:
	// ============================================================================
	// WIDGETS (Set in Blueprint)
	// ============================================================================

	/** Building name text. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> BuildingNameText;

	/** Building icon. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> BuildingIcon;

	/** Build time text. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> BuildTimeText;

	/** Checkbox for drawing from inventory. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> InventoryCheckbox;

	/** Checkbox for drawing from nearby containers. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> ContainersCheckbox;

	/** Checkbox for drawing from surrounding area. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> SurroundingCheckbox;

	/** Start build button. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> StartButton;

	/** Cancel button. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CancelButton;

	// ============================================================================
	// OVERRIDES
	// ============================================================================

	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// ============================================================================
	// STATE
	// ============================================================================

	/** The building being configured. */
	UPROPERTY()
	TWeakObjectPtr<AMOBuildableActor> TargetBuilding;

	/** Recipe ID of the target building. */
	FName TargetRecipeId = NAME_None;

	/** Cached gather range from recipe. */
	float GatherRange = 150.0f;

	// ============================================================================
	// HANDLERS
	// ============================================================================

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleCancelClicked();
};
