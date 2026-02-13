#include "MOBuildingDetailPanel.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOItemDatabaseSettings.h"
#include "MOUIUtils.h"
#include "Components/CheckBox.h"

UMOBuildingDetailPanel::UMOBuildingDetailPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOBuildingDetailPanel::NativeConstruct()
{
	Super::NativeConstruct();

	// Set default checkbox states
	if (InventoryCheckbox)
	{
		InventoryCheckbox->SetIsChecked(true);
	}
	if (ContainersCheckbox)
	{
		ContainersCheckbox->SetIsChecked(true);
	}
	if (SurroundingCheckbox)
	{
		SurroundingCheckbox->SetIsChecked(true);
	}
}

FMOBuildProgress UMOBuildingDetailPanel::GetBuildOptions() const
{
	FMOBuildProgress Options;

	Options.bDrawFromInventory = InventoryCheckbox ? InventoryCheckbox->IsChecked() : true;
	Options.bDrawFromNearbyContainers = ContainersCheckbox ? ContainersCheckbox->IsChecked() : true;
	Options.bDrawFromSurroundingArea = SurroundingCheckbox ? SurroundingCheckbox->IsChecked() : true;
	Options.GatherRange = GatherRange;

	return Options;
}

void UMOBuildingDetailPanel::GetBuildParts(TArray<FMOBuildPartDisplayData>& OutBuildParts) const
{
	OutBuildParts = CachedBuildParts;
}

void UMOBuildingDetailPanel::GetOutputs(TArray<FMOBuildOutputDisplayData>& OutOutputs) const
{
	OutOutputs = CachedOutputs;
}

void UMOBuildingDetailPanel::RefreshDisplay()
{
	if (!DisplayedRecipeId.IsNone())
	{
		// Rebuild build part data with updated counts
		const FMORecipeDefinitionRow* Recipe = GetDisplayedRecipe();
		if (Recipe)
		{
			CachedBuildParts.Empty();
			for (const FMOBuildPart& Part : Recipe->BuildParts)
			{
				CachedBuildParts.Add(BuildPartData(Part));
			}

			OnBuildPartsUpdated(CachedBuildParts);
		}
	}

	// Call base class to populate container and update button state
	Super::RefreshDisplay();
}

int32 UMOBuildingDetailPanel::GetMaxPerformableAmount() const
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

	// Check all build parts that require items
	for (const FMOBuildPart& Part : Recipe->BuildParts)
	{
		if (!Part.ItemDefinitionId.IsNone())
		{
			int32 Available = Inventory->GetItemCountByDefinitionId(Part.ItemDefinitionId);
			int32 CanMake = Available / Part.Quantity;
			MaxAmount = FMath::Min(MaxAmount, CanMake);
		}
	}

	return MaxAmount == INT_MAX ? 0 : MaxAmount;
}

bool UMOBuildingDetailPanel::CanPerformAction() const
{
	if (DisplayedRecipeId.IsNone())
	{
		return false;
	}

	// For buildings, placement mode comes BEFORE material gathering.
	// The ghost is placed first, then materials are deposited into it.
	// So we don't check materials here - that's done in the ghost context menu.
	// We could optionally check skill requirements here if desired.

	return true;
}

void UMOBuildingDetailPanel::OnDisplayRecipe(const FMORecipeDefinitionRow* Recipe)
{
	// Only accept building recipes
	if (!Recipe->bIsBuilding)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingDetailPanel] Recipe %s is not a building recipe"), *DisplayedRecipeId.ToString());
		ClearDisplay();
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingDetailPanel] OnDisplayRecipe: %s, BuildParts.Num()=%d"),
		*DisplayedRecipeId.ToString(), Recipe->BuildParts.Num());

	GatherRange = Recipe->BuildRange;

	// Build part data (ingredients)
	CachedBuildParts.Empty();
	for (const FMOBuildPart& Part : Recipe->BuildParts)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingDetailPanel]   BuildPart: ItemId=%s, ActionId=%s, Qty=%d"),
			*Part.ItemDefinitionId.ToString(), *Part.ActionId.ToString(), Part.Quantity);
		CachedBuildParts.Add(BuildPartData(Part));
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingDetailPanel] CachedBuildParts.Num()=%d after processing"), CachedBuildParts.Num());

	// Build output data
	CachedOutputs.Empty();
	CachedOutputs.Add(BuildOutputData(Recipe));

	// Notify Blueprint
	OnRecipeDisplayed(DisplayedRecipeId, Recipe->DisplayName, Recipe->Description);
	OnBuildPartsUpdated(CachedBuildParts);
	OnOutputsUpdated(CachedOutputs);
}

void UMOBuildingDetailPanel::PopulateRequirementsContainer()
{
	// Clear existing widgets using base class helper
	ClearTextWidgets(IngredientTextWidgets);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingDetailPanel] PopulateRequirementsContainer: IngredientsContainer=%s, CachedBuildParts.Num()=%d"),
		IngredientsContainer ? TEXT("valid") : TEXT("NULL"), CachedBuildParts.Num());

	if (!IngredientsContainer)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingDetailPanel] IngredientsContainer is NULL - widget not bound!"));
		return;
	}

	// Create text widget for each build part
	for (const FMOBuildPartDisplayData& Part : CachedBuildParts)
	{
		FText DisplayText;

		if (Part.bIsAction)
		{
			// Action: "Dig x1"
			DisplayText = UMOUIUtils::FormatActionDisplay(Part.DisplayName, Part.RequiredQuantity);
		}
		else
		{
			// Item: "Stone (have/need)"
			DisplayText = UMOUIUtils::FormatQuantityDisplay(
				Part.DisplayName,
				Part.AvailableQuantity,
				Part.RequiredQuantity
			);
		}

		AddTextWidget(IngredientsContainer, IngredientTextWidgets, DisplayText, Part.bHasEnough);
	}
}

void UMOBuildingDetailPanel::PopulateOutputsContainer()
{
	// Clear existing widgets using base class helper
	ClearTextWidgets(OutputTextWidgets);

	if (!OutputsContainer)
	{
		return;
	}

	// Create text widget for each output
	for (const FMOBuildOutputDisplayData& Output : CachedOutputs)
	{
		FText DisplayText;
		if (Output.Quantity > 1)
		{
			DisplayText = FText::Format(
				NSLOCTEXT("MOBuilding", "OutputFormatQty", "{0} x{1}"),
				Output.DisplayName,
				FText::AsNumber(Output.Quantity)
			);
		}
		else
		{
			DisplayText = Output.DisplayName;
		}

		AddTextWidget(OutputsContainer, OutputTextWidgets, DisplayText, true);
	}
}

void UMOBuildingDetailPanel::HandleActionButtonClicked()
{
	if (!DisplayedRecipeId.IsNone() && CanPerformAction())
	{
		// Broadcast standard delegate (prefer this for new code)
		OnBuildAction.Broadcast(DisplayedRecipeId, ActionAmount);
		// Broadcast legacy delegate for backward compatibility
		OnBuildRequested.Broadcast(DisplayedRecipeId, ActionAmount);
	}
}

FMOBuildPartDisplayData UMOBuildingDetailPanel::BuildPartData(const FMOBuildPart& Part) const
{
	FMOBuildPartDisplayData Data;
	Data.ItemDefinitionId = Part.ItemDefinitionId;
	Data.ActionId = Part.ActionId;
	Data.RequiredQuantity = Part.Quantity;
	Data.bIsAction = !Part.ActionId.IsNone();

	if (Data.bIsAction)
	{
		// Action-based part
		Data.DisplayName = FText::FromName(Part.ActionId);
		Data.bHasEnough = true; // Actions are always "available"
		Data.AvailableQuantity = Part.Quantity;
	}
	else
	{
		// Item-based part
		// Get available count from inventory
		if (UMOInventoryComponent* Inventory = InventoryComponent.Get())
		{
			Data.AvailableQuantity = Inventory->GetItemCountByDefinitionId(Part.ItemDefinitionId);
		}

		Data.bHasEnough = Data.AvailableQuantity >= Data.RequiredQuantity;

		// Get item definition for display info
		FMOItemDefinitionRow ItemDef;
		if (UMOItemDatabaseSettings::GetItemDefinition(Part.ItemDefinitionId, ItemDef))
		{
			Data.DisplayName = ItemDef.DisplayName;
			Data.Icon = ItemDef.UI.IconSmall;
		}
		else
		{
			Data.DisplayName = FText::FromName(Part.ItemDefinitionId);
		}
	}

	return Data;
}

FMOBuildOutputDisplayData UMOBuildingDetailPanel::BuildOutputData(const FMORecipeDefinitionRow* Recipe) const
{
	FMOBuildOutputDisplayData Data;

	if (Recipe)
	{
		Data.DisplayName = Recipe->DisplayName;
		Data.Quantity = 1;
		Data.Icon = Recipe->Icon;
	}

	return Data;
}
