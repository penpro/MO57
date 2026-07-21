/**
 * =============================================================================
 * MORecipeListWidget.h - Scrollable Recipe List
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Scrollable list of recipe entries. Used in crafting menu and building menu.
 * Filters and displays recipes based on knowledge, skills, and station.
 *
 * INHERITS FROM: UMOScrollListBase (provides container management, selection)
 *
 * ENTRY DATA:
 * Each entry shows:
 * - Recipe icon
 * - Display name
 * - Craftability status (has ingredients, meets requirements)
 * - Lock status (unknown recipe greyed out)
 *
 * FILTERING:
 * - By station type (hand craft, workbench, forge, etc.)
 * - By discovery state (known vs unknown)
 * - By search text (optional)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] LEGACY DELEGATE: FMORecipeSelectedSignature is legacy. New code
 *   should use FMOUIRecipeSelected from MOUIDelegates.h.
 *
 * [2024-02] ENTRY POOLING: List creates MORecipeEntryWidget per recipe.
 *   For 100+ recipes, consider widget pooling to reduce allocation.
 *
 * [2024-02] REFRESH TIMING: Call RefreshEntryStates() when inventory changes
 *   or skills level up. Ingredient availability affects bCanCraft state.
 *
 * [2026-07] FILTER APPLICATION: Station context refreshes entry availability.
 *   Category/show-only setters still store query policy; the owning menu
 *   repopulates the visible recipe IDs after changing those filters.
 *
 * [2024-02] NULL COMPONENTS: InventoryComponent, SkillsComponent, and
 *   DiscoveryComponent are TWeakObjectPtr. BuildEntryData() and
 *   CanCraftRecipe() handle null gracefully (assume can't craft, discovered).
 *
 * [2024-02] DELEGATE BINDING: NativeConstruct() does NOT autobind to
 *   component events. Caller must call RefreshEntryStates() when inventory
 *   or skills change. This is intentional for performance control.
 *
 * [2024-02] ENTRY CLICK HANDLING: Entry widgets call HandleEntryClicked()
 *   which broadcasts both OnRecipeSelection (new) and OnRecipeSelected
 *   (legacy) for backward compatibility. Both receive same RecipeId.
 *
 * [2024-02] ENTRY DATA STRUCT: FMORecipeListEntryData contains:
 *   RecipeId, DisplayName, Category, Icon (TSoftObjectPtr), bCanCraft,
 *   bIsDiscovered, bIsSelected. Use for custom entry widget display.
 *
 * [2026-03] WIDGET BINDINGS: Uses RecipeScrollBox/RecipeContainer (not base's
 *   ContentScrollBox/ContentContainer) for backward compatibility. GetContainer()
 *   is overridden to use these.
 *
 * =============================================================================
 * RELATED FILES: MOScrollListBase.h, MORecipeEntryWidget.h, MOCraftingMenu.h
 * LAST UPDATED: 2026-03-29
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOScrollListBase.h"
#include "MORecipeDefinitionRow.h"
#include "MOUIDelegates.h"
#include "MORecipeListWidget.generated.h"

class UMOInventoryComponent;
class UMOSkillsComponent;
class UMOKnowledgeComponent;
class UMORecipeDiscoveryComponent;
class UMORecipeEntryWidget;

// Legacy delegate - prefer FMOUIRecipeSelected from MOUIDelegates.h for new code
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMORecipeSelectedSignature, FName, RecipeId);

/**
 * Visual data for a recipe entry in the list.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMORecipeListEntryData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FName RecipeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	FName Category = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** True if the player can craft this recipe right now. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	bool bCanCraft = false;

	/** True if this recipe has been discovered. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	bool bIsDiscovered = true;

	/** True if currently selected. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Crafting|UI")
	bool bIsSelected = false;
};

/**
 * Widget that displays a scrollable list of recipes.
 * Inherits container/selection management from UMOScrollListBase.
 *
 * Requires a Blueprint implementation with:
 * - RecipeScrollBox (UScrollBox) or RecipeContainer (UVerticalBox)
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMORecipeListWidget : public UMOScrollListBase
{
	GENERATED_BODY()

public:
	UMORecipeListWidget(const FObjectInitializer& ObjectInitializer);

	// --- Initialization ---

	/**
	 * Initialize the list with data sources.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void InitializeList(
		UMOInventoryComponent* InInventory,
		UMOSkillsComponent* InSkills,
		UMORecipeDiscoveryComponent* InDiscovery
	);

	/** Supply knowledge/station context for authoritative action availability. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetCraftingContext(UMOKnowledgeComponent* InKnowledge, EMOCraftingStation InStation);

	// --- Recipe Population ---

	/**
	 * Populate the list with the given recipe IDs.
	 * @param RecipeIds Array of recipe IDs to display
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void PopulateRecipes(const TArray<FName>& RecipeIds);

	/** Clear all recipes from the list. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void ClearRecipes();

	/** Refresh the visual state of all entries (craftability, selection). */
	virtual void RefreshEntryStates() override;
	virtual void SelectEntry(FName EntryId) override;

	// --- Selection ---

	/** Select a recipe by ID. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SelectRecipe(FName RecipeId);

	/** Get the currently selected recipe ID. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|UI")
	FName GetSelectedRecipeId() const { return GetSelectedEntryId(); }

	// --- Filtering ---

	/** Set the station filter for recipe visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetStationFilter(EMOCraftingStation Station);

	/** Set the category filter. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetCategoryFilter(FName Category);

	/** Set whether to only show craftable recipes. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|UI")
	void SetShowOnlyCraftable(bool bOnlyCraftable);

	// --- Delegates ---

	/** Broadcast when a recipe is selected. Uses standard delegate signature. */
	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|UI")
	FMOUIRecipeSelected OnRecipeSelection;

	/** @deprecated Use OnRecipeSelection instead. Broadcast for backward compatibility. */
	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|UI")
	FMORecipeSelectedSignature OnRecipeSelected;

	// --- Configuration ---

	/** Widget class to use for recipe entries. Must be a subclass of UMORecipeEntryWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|UI")
	TSubclassOf<UMORecipeEntryWidget> RecipeEntryWidgetClass;

protected:
	/** Override to use our specific widget bindings (RecipeScrollBox/RecipeContainer). */
	virtual UPanelWidget* GetContainer() const;

	virtual void ConfigureEntry_Implementation(UMOListEntryBase* Entry, FName EntryId) override;
	virtual void RefreshEntryState_Implementation(UMOListEntryBase* Entry, FName EntryId) override;

	/** Build visual data for a recipe. */
	FMORecipeListEntryData BuildEntryData(FName RecipeId) const;

	/** Check if a recipe can be crafted with current resources. */
	bool CanCraftRecipe(FName RecipeId) const;

	// --- Widget Bindings (domain-specific, override base) ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UScrollBox> RecipeScrollBox;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> RecipeContainer;

private:
	// Cached component references
	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOSkillsComponent> SkillsComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOKnowledgeComponent> KnowledgeComponent;

	UPROPERTY()
	TWeakObjectPtr<UMORecipeDiscoveryComponent> DiscoveryComponent;

	// Current state
	EMOCraftingStation StationFilter = EMOCraftingStation::None;
	FName CategoryFilter = NAME_None;
	bool bShowOnlyCraftable = false;
};
