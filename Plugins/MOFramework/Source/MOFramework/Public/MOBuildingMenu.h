#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOBuildingMenu.generated.h"

class UMOKnowledgeComponent;
class UMORecipeDiscoveryComponent;
class UMOInventoryComponent;
class UMOSkillsComponent;
class UMOBuildingRecipeListWidget;
class UMOBuildingDetailPanel;
class UMOCommonButton;

/**
 * Delegate for when the building menu requests close.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOBuildingMenuRequestCloseSignature);

/**
 * Delegate for when a building is selected to be placed (after clicking Build in detail panel).
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
	 * @param InInventory - Inventory component for material checks
	 * @param InSkills - Skills component for skill requirements
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void InitializeMenu(
		UMOKnowledgeComponent* InKnowledge,
		UMORecipeDiscoveryComponent* InDiscovery,
		UMOInventoryComponent* InInventory = nullptr,
		UMOSkillsComponent* InSkills = nullptr
	);

	/**
	 * Refresh the building list.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void RefreshBuildingList();

protected:
	// ============================================================================
	// WIDGETS (Set in Blueprint)
	// ============================================================================

	/** Recipe list widget containing building entries. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UMOBuildingRecipeListWidget> RecipeList;

	/** Detail panel showing selected recipe info and Build button. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOBuildingDetailPanel> DetailPanel;

	/** Close button. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CloseButton;

	// ============================================================================
	// OVERRIDES
	// ============================================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
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

	/** Cached inventory component. */
	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> InventoryComponent;

	/** Cached skills component. */
	UPROPERTY()
	TWeakObjectPtr<UMOSkillsComponent> SkillsComponent;

	// ============================================================================
	// HANDLERS
	// ============================================================================

	UFUNCTION()
	void HandleCloseClicked();

	/** Called when a recipe is selected in the list - shows it in detail panel. */
	UFUNCTION()
	void HandleRecipeSelected(FName RecipeId);

	/** Called when Build is clicked in the detail panel - broadcasts OnBuildingSelected. */
	UFUNCTION()
	void HandleBuildRequested(FName RecipeId, int32 Count);
};
