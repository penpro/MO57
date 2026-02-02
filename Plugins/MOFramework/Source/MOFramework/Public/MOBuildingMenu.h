#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOBuildingMenu.generated.h"

class UMOKnowledgeComponent;
class UMORecipeDiscoveryComponent;
class UScrollBox;
class UMOBuildingEntryWidget;
class UMOCommonButton;

/**
 * Delegate for when the building menu requests close.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOBuildingMenuRequestCloseSignature);

/**
 * Delegate for when a building is selected from the menu.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnBuildingSelectedSignature, FName, RecipeId);

/**
 * Menu for selecting buildings to place.
 * Shows available building recipes filtered by discovery.
 */
UCLASS()
class MOFRAMEWORK_API UMOBuildingMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UMOBuildingMenu(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when the menu should be closed. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOBuildingMenuRequestCloseSignature OnRequestClose;

	/** Broadcast when a building is selected. */
	UPROPERTY(BlueprintAssignable, Category="MO|Building")
	FMOOnBuildingSelectedSignature OnBuildingSelected;

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize the menu with player components.
	 * @param InKnowledge - Knowledge component for recipe filtering
	 * @param InDiscovery - Discovery component for discovered recipes
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void InitializeMenu(UMOKnowledgeComponent* InKnowledge, UMORecipeDiscoveryComponent* InDiscovery);

	/**
	 * Refresh the building list.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void RefreshBuildingList();

protected:
	// ============================================================================
	// WIDGETS (Set in Blueprint)
	// ============================================================================

	/** Scroll box containing building entries. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UScrollBox> BuildingListScrollBox;

	/** Close button. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CloseButton;

	/** Widget class for building entries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building")
	TSubclassOf<UMOBuildingEntryWidget> BuildingEntryWidgetClass;

	// ============================================================================
	// OVERRIDES
	// ============================================================================

	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// ============================================================================
	// STATE
	// ============================================================================

	/** Cached knowledge component. */
	UPROPERTY()
	TWeakObjectPtr<UMOKnowledgeComponent> KnowledgeComponent;

	/** Cached discovery component. */
	UPROPERTY()
	TWeakObjectPtr<UMORecipeDiscoveryComponent> DiscoveryComponent;

	/** Created entry widgets. */
	UPROPERTY()
	TArray<TObjectPtr<UMOBuildingEntryWidget>> EntryWidgets;

	// ============================================================================
	// HANDLERS
	// ============================================================================

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleBuildingEntryClicked(FName RecipeId);

	/** Clear the building list. */
	void ClearBuildingList();
};
