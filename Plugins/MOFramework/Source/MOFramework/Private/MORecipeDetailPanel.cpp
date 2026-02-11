#include "MORecipeDetailPanel.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOItemDatabaseSettings.h"
#include "MOUIUtils.h"
#include "Components/TextBlock.h"

UMORecipeDetailPanel::UMORecipeDetailPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMORecipeDetailPanel::GetIngredients(TArray<FMOIngredientDisplayData>& OutIngredients) const
{
	OutIngredients = CachedIngredients;
}

void UMORecipeDetailPanel::GetOutputs(TArray<FMOOutputDisplayData>& OutOutputs) const
{
	OutOutputs = CachedOutputs;
}

void UMORecipeDetailPanel::RefreshDisplay()
{
	if (!DisplayedRecipeId.IsNone())
	{
		// Rebuild ingredient data with updated counts
		const FMORecipeDefinitionRow* Recipe = GetDisplayedRecipe();
		if (Recipe)
		{
			CachedIngredients.Empty();
			for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
			{
				CachedIngredients.Add(BuildIngredientData(Ingredient));
			}

			OnIngredientsUpdated(CachedIngredients);
		}
	}

	// Call base class to populate container and update button state
	Super::RefreshDisplay();
}

int32 UMORecipeDetailPanel::GetMaxPerformableAmount() const
{
	if (DisplayedRecipeId.IsNone())
	{
		return 0;
	}

	const FMORecipeDefinitionRow* Recipe = GetDisplayedRecipe();
	if (!Recipe)
	{
		return 0;
	}

	UMOInventoryComponent* Inventory = InventoryComponent.Get();
	if (!Inventory)
	{
		return 0;
	}

	int32 MaxAmount = INT_MAX;

	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		int32 Available = Inventory->GetItemCountByDefinitionId(Ingredient.ItemDefinitionId);
		int32 CanMake = Available / Ingredient.Quantity;
		MaxAmount = FMath::Min(MaxAmount, CanMake);
	}

	return MaxAmount == INT_MAX ? 0 : MaxAmount;
}

bool UMORecipeDetailPanel::CanPerformAction() const
{
	if (DisplayedRecipeId.IsNone())
	{
		return false;
	}

	// Check all ingredients for requested amount
	for (const FMOIngredientDisplayData& Ingredient : CachedIngredients)
	{
		int32 TotalRequired = Ingredient.RequiredQuantity * ActionAmount;
		if (Ingredient.AvailableQuantity < TotalRequired)
		{
			return false;
		}
	}

	return true;
}

void UMORecipeDetailPanel::OnDisplayRecipe(const FMORecipeDefinitionRow* Recipe)
{
	// Update required station text (crafting-specific)
	if (RequiredStationText)
	{
		if (Recipe->RequiredStation != EMOCraftingStation::None)
		{
			// Get station name from enum
			FString StationName = UEnum::GetDisplayValueAsText(Recipe->RequiredStation).ToString();
			RequiredStationText->SetText(FText::Format(
				NSLOCTEXT("MOCrafting", "RequiresStation", "Requires: {0}"),
				FText::FromString(StationName)
			));
			RequiredStationText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			RequiredStationText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Build ingredient data
	CachedIngredients.Empty();
	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		CachedIngredients.Add(BuildIngredientData(Ingredient));
	}

	// Build output data
	CachedOutputs.Empty();
	for (const FMORecipeOutput& Output : Recipe->Outputs)
	{
		CachedOutputs.Add(BuildOutputData(Output));
	}

	// Notify Blueprint
	OnRecipeDisplayed(DisplayedRecipeId, Recipe->DisplayName, Recipe->Description);
	OnIngredientsUpdated(CachedIngredients);
	OnOutputsUpdated(CachedOutputs);
}

void UMORecipeDetailPanel::PopulateRequirementsContainer()
{
	// Clear existing widgets using base class helper
	ClearTextWidgets(IngredientTextWidgets);

	if (!IngredientsContainer)
	{
		return;
	}

	// Create text widget for each ingredient
	for (const FMOIngredientDisplayData& Ingredient : CachedIngredients)
	{
		FText DisplayText = UMOUIUtils::FormatQuantityDisplay(
			Ingredient.DisplayName,
			Ingredient.AvailableQuantity,
			Ingredient.RequiredQuantity
		);

		AddTextWidget(IngredientsContainer, IngredientTextWidgets, DisplayText, Ingredient.bHasEnough);
	}
}

void UMORecipeDetailPanel::PopulateOutputsContainer()
{
	// Clear existing widgets using base class helper
	ClearTextWidgets(OutputTextWidgets);

	if (!OutputsContainer)
	{
		return;
	}

	// Create text widget for each output
	for (const FMOOutputDisplayData& Output : CachedOutputs)
	{
		FText DisplayText = UMOUIUtils::FormatOutputDisplay(
			Output.DisplayName,
			Output.Quantity,
			Output.Chance
		);

		AddTextWidget(OutputsContainer, OutputTextWidgets, DisplayText, true);
	}
}

void UMORecipeDetailPanel::HandleActionButtonClicked()
{
	if (!DisplayedRecipeId.IsNone() && CanPerformAction())
	{
		// Broadcast standard delegate (prefer this for new code)
		OnCraftAction.Broadcast(DisplayedRecipeId, ActionAmount);
		// Broadcast legacy delegate for backward compatibility
		OnCraftRequested.Broadcast(DisplayedRecipeId, ActionAmount);
	}
}

float UMORecipeDetailPanel::GetRecipeTime(const FMORecipeDefinitionRow* Recipe) const
{
	// Crafting uses CraftTime
	return Recipe ? Recipe->CraftTime : 0.0f;
}

FMOIngredientDisplayData UMORecipeDetailPanel::BuildIngredientData(const FMORecipeIngredient& Ingredient) const
{
	FMOIngredientDisplayData Data;
	Data.ItemDefinitionId = Ingredient.ItemDefinitionId;
	Data.RequiredQuantity = Ingredient.Quantity;

	// Get available count from inventory
	if (UMOInventoryComponent* Inventory = InventoryComponent.Get())
	{
		Data.AvailableQuantity = Inventory->GetItemCountByDefinitionId(Ingredient.ItemDefinitionId);
	}

	Data.bHasEnough = Data.AvailableQuantity >= Data.RequiredQuantity;

	// Get item definition for display info
	FMOItemDefinitionRow ItemDef;
	if (UMOItemDatabaseSettings::GetItemDefinition(Ingredient.ItemDefinitionId, ItemDef))
	{
		Data.DisplayName = ItemDef.DisplayName;
		Data.Icon = ItemDef.UI.IconSmall;
	}
	else
	{
		Data.DisplayName = FText::FromName(Ingredient.ItemDefinitionId);
	}

	return Data;
}

FMOOutputDisplayData UMORecipeDetailPanel::BuildOutputData(const FMORecipeOutput& Output) const
{
	FMOOutputDisplayData Data;
	Data.ItemDefinitionId = Output.ItemDefinitionId;
	Data.Quantity = Output.Quantity;
	Data.Chance = Output.Chance;

	// Get item definition for display info
	FMOItemDefinitionRow ItemDef;
	if (UMOItemDatabaseSettings::GetItemDefinition(Output.ItemDefinitionId, ItemDef))
	{
		Data.DisplayName = ItemDef.DisplayName;
		Data.Icon = ItemDef.UI.IconSmall;
	}
	else
	{
		Data.DisplayName = FText::FromName(Output.ItemDefinitionId);
	}

	return Data;
}
