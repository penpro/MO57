#include "MORecipeListWidget.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MORecipeEntryWidget.h"
#include "MORecipeDatabaseSettings.h"
#include "MOItemDatabaseSettings.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/PanelWidget.h"

UMORecipeListWidget::UMORecipeListWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPanelWidget* UMORecipeListWidget::GetContainer() const
{
	// Use domain-specific bindings instead of base class ContentScrollBox/ContentContainer
	if (RecipeScrollBox)
	{
		return RecipeScrollBox;
	}
	return RecipeContainer;
}

void UMORecipeListWidget::InitializeList(
	UMOInventoryComponent* InInventory,
	UMOSkillsComponent* InSkills,
	UMORecipeDiscoveryComponent* InDiscovery)
{
	InventoryComponent = InInventory;
	SkillsComponent = InSkills;
	DiscoveryComponent = InDiscovery;
}

void UMORecipeListWidget::PopulateRecipes(const TArray<FName>& RecipeIds)
{
	CurrentRecipeIds = RecipeIds;

	// Clear existing entries
	ClearRecipes();

	// Get the container to add entries to
	UPanelWidget* Container = GetContainer();
	if (!Container)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MORecipeListWidget] No container widget bound"));
		return;
	}

	// Check if we have a valid entry widget class
	if (!RecipeEntryWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MORecipeListWidget] No RecipeEntryWidgetClass set"));
		return;
	}

	// Create entry widgets
	for (const FName& RecipeId : RecipeIds)
	{
		UMORecipeEntryWidget* EntryWidget = CreateWidget<UMORecipeEntryWidget>(this, RecipeEntryWidgetClass);
		if (!EntryWidget)
		{
			continue;
		}

		// Build and set data
		FMORecipeListEntryData EntryData = BuildEntryData(RecipeId);
		EntryWidget->SetupEntry(EntryData);

		// Bind click handler
		EntryWidget->OnEntryClicked.AddDynamic(this, &UMORecipeListWidget::HandleRecipeEntryClicked);

		// Add to container
		Container->AddChild(EntryWidget);
		RecipeEntryWidgets.Add(EntryWidget);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MORecipeListWidget] Populated %d recipes"), RecipeIds.Num());
}

void UMORecipeListWidget::ClearRecipes()
{
	// Remove all entry widgets
	for (UMORecipeEntryWidget* Entry : RecipeEntryWidgets)
	{
		if (Entry)
		{
			Entry->RemoveFromParent();
		}
	}
	RecipeEntryWidgets.Empty();
	SelectedRecipeId = NAME_None;
}

void UMORecipeListWidget::RefreshEntryStates()
{
	for (UMORecipeEntryWidget* Entry : RecipeEntryWidgets)
	{
		if (!Entry)
		{
			continue;
		}

		FName RecipeId = Entry->GetRecipeId();
		bool bCanCraft = CanCraftRecipe(RecipeId);
		bool bIsSelected = (RecipeId == SelectedRecipeId);

		Entry->SetCanCraft(bCanCraft);
		Entry->SetSelected(bIsSelected);
	}
}

void UMORecipeListWidget::SelectRecipe(FName RecipeId)
{
	FName OldSelection = SelectedRecipeId;
	SelectedRecipeId = RecipeId;

	// Update visual state of affected entries
	for (UMORecipeEntryWidget* Entry : RecipeEntryWidgets)
	{
		if (!Entry)
		{
			continue;
		}

		FName EntryRecipeId = Entry->GetRecipeId();
		if (EntryRecipeId == OldSelection || EntryRecipeId == SelectedRecipeId)
		{
			Entry->SetSelected(EntryRecipeId == SelectedRecipeId);
		}
	}

	// Broadcast selection
	if (SelectedRecipeId != OldSelection)
	{
		// Broadcast standard delegate (prefer this for new code)
		OnRecipeSelection.Broadcast(SelectedRecipeId);
		// Broadcast legacy delegate for backward compatibility
		OnRecipeSelected.Broadcast(SelectedRecipeId);
	}
}

void UMORecipeListWidget::SetStationFilter(EMOCraftingStation Station)
{
	StationFilter = Station;
	// Note: Caller should repopulate the list with filtered recipes
}

void UMORecipeListWidget::SetCategoryFilter(FName Category)
{
	CategoryFilter = Category;
	// Note: Caller should repopulate the list with filtered recipes
}

void UMORecipeListWidget::SetShowOnlyCraftable(bool bOnlyCraftable)
{
	bShowOnlyCraftable = bOnlyCraftable;
	// Note: Caller should repopulate the list with filtered recipes
}

void UMORecipeListWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UMORecipeListWidget::HandleRecipeEntryClicked(FName RecipeId)
{
	SelectRecipe(RecipeId);
}

FMORecipeListEntryData UMORecipeListWidget::BuildEntryData(FName RecipeId) const
{
	FMORecipeListEntryData Data;
	Data.RecipeId = RecipeId;
	Data.bIsSelected = (RecipeId == SelectedRecipeId);
	Data.bCanCraft = CanCraftRecipe(RecipeId);

	// Check discovery
	if (UMORecipeDiscoveryComponent* Discovery = DiscoveryComponent.Get())
	{
		Data.bIsDiscovered = Discovery->IsRecipeDiscovered(RecipeId);
	}

	// Get recipe definition for display info
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (Recipe)
	{
		Data.DisplayName = Recipe->DisplayName;
		Data.Category = Recipe->Category;

		// Use recipe icon if set, otherwise fall back to first output item's icon
		if (!Recipe->Icon.IsNull())
		{
			Data.Icon = Recipe->Icon;
		}
		else if (Recipe->Outputs.Num() > 0)
		{
			FMOItemDefinitionRow ItemDef;
			if (UMOItemDatabaseSettings::GetItemDefinition(Recipe->Outputs[0].ItemDefinitionId, ItemDef))
			{
				Data.Icon = ItemDef.UI.IconSmall;
			}
		}
	}
	else
	{
		Data.DisplayName = FText::FromName(RecipeId);
	}

	return Data;
}

bool UMORecipeListWidget::CanCraftRecipe(FName RecipeId) const
{
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		return false;
	}

	UMOInventoryComponent* Inventory = InventoryComponent.Get();
	if (!Inventory)
	{
		return false;
	}

	// Check all ingredients
	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		if (!Inventory->HasItem(Ingredient.ItemDefinitionId, Ingredient.Quantity))
		{
			return false;
		}
	}

	// Check skill requirements
	if (!Recipe->RequiredSkillId.IsNone() && Recipe->RequiredSkillLevel > 0)
	{
		if (UMOSkillsComponent* Skills = SkillsComponent.Get())
		{
			if (!Skills->HasSkillLevel(Recipe->RequiredSkillId, Recipe->RequiredSkillLevel))
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	return true;
}
