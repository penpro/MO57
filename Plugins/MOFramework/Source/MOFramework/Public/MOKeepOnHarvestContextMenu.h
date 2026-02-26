/**
 * =============================================================================
 * MOKeepOnHarvestContextMenu.h - Resource Node Context Menu
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Context menu for interacting with harvestable resource nodes (trees, bushes,
 * rocks) that use KeepOnHarvest ISM/HISM instances. Dynamically builds button
 * list based on available harvest recipes.
 *
 * OPTIONS:
 * - Inspect: Smart inspect (picks first learnable product)
 * - Harvest [X]: Dynamic buttons per available recipe (e.g., "Harvest Berries")
 * - Chop Down: Destroy the resource (if applicable recipe exists)
 * - Search for Insects: Future expansion feature
 *
 * SMART INSPECT:
 * Examines resource's potential products and picks the first one the player
 * hasn't learned yet. If all known, picks the primary output.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] DYNAMIC BUTTONS: Buttons are created at runtime based on available
 *   recipes. Must set ButtonClass in Blueprint defaults.
 *
 * [2024-02] RECIPE LOOKUP: Uses FMOInteractionTarget tags to find applicable
 *   recipes. Tags come from MOHISMInteractableComponent or mesh data.
 *
 * [2024-02] TOOL REQUIREMENTS: CanChopDown checks if player has required tools.
 *   Uses MOCraftingSubsystem.CanExecuteRecipe() for validation.
 *
 * [2024-02] CLEANUP: ClearButtons() must remove all created widgets to prevent
 *   memory leaks when menu closes.
 *
 * =============================================================================
 * RELATED FILES: MOInteractorComponent.h, MOHarvestSubsystem.h, MOContextMenuBase.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOContextMenuBase.h"
#include "MOInteractorComponent.h"
#include "MOKeepOnHarvestContextMenu.generated.h"

class UMOCommonButton;
class UTextBlock;
class UVerticalBox;
class UMOKnowledgeComponent;
class UMOSkillsComponent;
class UMOInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOKeepOnHarvestMenuRequestClose);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOKeepOnHarvestMenuInspectClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOKeepOnHarvestMenuHarvestClicked, FName, RecipeId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOKeepOnHarvestMenuChopDownClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOKeepOnHarvestMenuSearchInsectsClicked);
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOKeepOnHarvestContextMenu : public UMOContextMenuBase
{
	GENERATED_BODY()

public:
	UMOKeepOnHarvestContextMenu(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize the menu for a specific interaction target.
	 * @param Target - The ISM/HISM target to configure
	 * @param Knowledge - Player's knowledge component
	 * @param Skills - Player's skills component
	 * @param Inventory - Player's inventory component
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest|UI")
	void InitializeForTarget(
		const FMOInteractionTarget& Target,
		UMOKnowledgeComponent* Knowledge,
		UMOSkillsComponent* Skills,
		UMOInventoryComponent* Inventory
	);

	/**
	 * Refresh the display (button states, available options).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest|UI")
	void RefreshDisplay();

	// ============================================================================
	// STATE QUERIES
	// ============================================================================

	/**
	 * Get the current interaction target.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Harvest|UI")
	const FMOInteractionTarget& GetCurrentTarget() const { return CurrentTarget; }

	/**
	 * Get the item ID that will be inspected (smart inspect result).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Harvest|UI")
	FName GetSmartInspectItemId() const { return SmartInspectItemId; }

	/**
	 * Get the display name of the target object.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Harvest|UI")
	FText GetTargetDisplayName() const;

	/**
	 * Check if inspect is available.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Harvest|UI")
	bool CanInspect() const { return !SmartInspectItemId.IsNone(); }

	/**
	 * Check if chop down recipe exists for this target.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Harvest|UI")
	bool HasChopDownRecipe() const { return !ChopDownRecipeId.IsNone(); }

	/**
	 * Check if chop down can be executed (has required tools).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Harvest|UI")
	bool CanChopDown() const { return bCanExecuteChopDown; }

	// SetPopupPosition is inherited from UMOContextMenuBase

	// Override to broadcast legacy OnRequestClose delegate
	virtual void RequestClose() override;

	// ============================================================================
	// DELEGATES
	// ============================================================================

	UPROPERTY(BlueprintAssignable, Category="MO|Harvest|UI")
	FMOKeepOnHarvestMenuRequestClose OnRequestClose;

	UPROPERTY(BlueprintAssignable, Category="MO|Harvest|UI")
	FMOKeepOnHarvestMenuInspectClicked OnInspectClicked;

	UPROPERTY(BlueprintAssignable, Category="MO|Harvest|UI")
	FMOKeepOnHarvestMenuHarvestClicked OnHarvestClicked;

	UPROPERTY(BlueprintAssignable, Category="MO|Harvest|UI")
	FMOKeepOnHarvestMenuChopDownClicked OnChopDownClicked;

	UPROPERTY(BlueprintAssignable, Category="MO|Harvest|UI")
	FMOKeepOnHarvestMenuSearchInsectsClicked OnSearchInsectsClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	// NativeOnKeyDown is inherited from UMOContextMenuBase

	// ============================================================================
	// WIDGET BINDINGS
	// ============================================================================

	/** Target name display */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TargetNameText;

	/** Container for all buttons - populated programmatically */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UVerticalBox> ButtonContainer;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Button class to use for all buttons (WBP_MOCommonButton). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Harvest|UI")
	TSubclassOf<UMOCommonButton> ButtonClass;

	// ============================================================================
	// BLUEPRINT EVENTS
	// ============================================================================

	/** Called when harvest buttons are created/refreshed */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Harvest|UI")
	void OnHarvestButtonsUpdated(int32 ButtonCount);

	/** Called when button states are updated */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Harvest|UI")
	void OnButtonStatesUpdated(bool bCanInspect, bool bCanChopDown, int32 HarvestCount);

private:
	// ============================================================================
	// STATE
	// ============================================================================

	FMOInteractionTarget CurrentTarget;

	UPROPERTY()
	TWeakObjectPtr<UMOKnowledgeComponent> CachedKnowledge;

	UPROPERTY()
	TWeakObjectPtr<UMOSkillsComponent> CachedSkills;

	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> CachedInventory;

	/** Item ID for smart inspect. */
	FName SmartInspectItemId = NAME_None;

	/** Recipe ID for chop down action. */
	FName ChopDownRecipeId = NAME_None;

	/** Whether the chop down recipe can be executed (has required tools). */
	bool bCanExecuteChopDown = false;

	/** Available harvest recipe IDs. */
	TArray<FName> AvailableHarvestRecipes;

	/** Tags collected from the target. */
	TArray<FName> TargetTags;

	/** All created button widgets (for cleanup). */
	UPROPERTY()
	TArray<TObjectPtr<UMOCommonButton>> CreatedButtons;

	/** Populate the ButtonContainer with all context-appropriate buttons. */
	void PopulateButtons();

	/** Clear all created buttons. */
	void ClearButtons();

	/** Helper to create and add a button with a label. */
	UMOCommonButton* CreateButton(const FText& Label);
};
