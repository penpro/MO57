#include "MOBuildingRecipeListWidget.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOBuildingRecipeEntryWidget.h"
#include "MORecipeDatabaseSettings.h"
#include "MOItemDatabaseSettings.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/PanelWidget.h"

UMOBuildingRecipeListWidget::UMOBuildingRecipeListWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPanelWidget* UMOBuildingRecipeListWidget::GetContainer() const
{
	// Use domain-specific bindings instead of base class ContentScrollBox/ContentContainer
	if (RecipeScrollBox)
	{
		return RecipeScrollBox;
	}
	return RecipeContainer;
}

void UMOBuildingRecipeListWidget::InitializeList(
	UMOInventoryComponent* InInventory,
	UMOSkillsComponent* InSkills,
	UMORecipeDiscoveryComponent* InDiscovery)
{
	InventoryComponent = InInventory;
	SkillsComponent = InSkills;
	DiscoveryComponent = InDiscovery;
}

void UMOBuildingRecipeListWidget::PopulateRecipes(const TArray<FName>& RecipeIds)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingRecipeListWidget] PopulateRecipes called with %d recipes"), RecipeIds.Num());

	if (!RecipeEntryWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingRecipeListWidget] No RecipeEntryWidgetClass set - check Blueprint defaults"));
		return;
	}

	EntryWidgetClass = RecipeEntryWidgetClass;
	Super::PopulateList(RecipeIds);
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingRecipeListWidget] Populated %d building recipes"), GetEntryCount());
}

void UMOBuildingRecipeListWidget::ClearRecipes()
{
	Super::ClearList();
}

void UMOBuildingRecipeListWidget::RefreshEntryStates()
{
	Super::RefreshEntryStates();
}

void UMOBuildingRecipeListWidget::SelectRecipe(FName RecipeId)
{
	SelectEntry(RecipeId);
}

void UMOBuildingRecipeListWidget::SelectEntry(FName EntryId)
{
	const FName PreviousId = GetSelectedEntryId();
	Super::SelectEntry(EntryId);
	const FName CurrentId = GetSelectedEntryId();
	if (CurrentId != PreviousId && !CurrentId.IsNone())
	{
		OnRecipeSelected.Broadcast(CurrentId);
	}
}

void UMOBuildingRecipeListWidget::SetCategoryFilter(FName Category)
{
	CategoryFilter = Category;
	// Note: Caller should repopulate the list with filtered recipes
}

void UMOBuildingRecipeListWidget::SetShowOnlyBuildable(bool bOnlyBuildable)
{
	bShowOnlyBuildable = bOnlyBuildable;
	// Note: Caller should repopulate the list with filtered recipes
}

void UMOBuildingRecipeListWidget::ConfigureEntry_Implementation(UMOListEntryBase* Entry, FName EntryId)
{
	if (UMOBuildingRecipeEntryWidget* BuildingEntry = Cast<UMOBuildingRecipeEntryWidget>(Entry))
	{
		BuildingEntry->SetupEntry(BuildEntryData(EntryId));
	}
}

void UMOBuildingRecipeListWidget::RefreshEntryState_Implementation(UMOListEntryBase* Entry, FName EntryId)
{
	if (UMOBuildingRecipeEntryWidget* BuildingEntry = Cast<UMOBuildingRecipeEntryWidget>(Entry))
	{
		BuildingEntry->SetCanBuild(CanBuildRecipe(EntryId));
		BuildingEntry->SetSelected(EntryId == GetSelectedEntryId());
	}
}

FMOBuildRecipeListEntryData UMOBuildingRecipeListWidget::BuildEntryData(FName RecipeId) const
{
	FMOBuildRecipeListEntryData Data;
	Data.RecipeId = RecipeId;
	Data.bIsSelected = (RecipeId == GetSelectedEntryId());
	Data.bCanBuild = CanBuildRecipe(RecipeId);

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
		Data.Icon = Recipe->Icon;
	}
	else
	{
		Data.DisplayName = FText::FromName(RecipeId);
	}

	return Data;
}

bool UMOBuildingRecipeListWidget::CanBuildRecipe(FName RecipeId) const
{
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe || !Recipe->bIsBuilding)
	{
		return false;
	}

	UMOInventoryComponent* Inventory = InventoryComponent.Get();
	if (!Inventory)
	{
		return false;
	}

	// Check all build parts that are items (not actions)
	for (const FMOBuildPart& Part : Recipe->BuildParts)
	{
		if (!Part.ItemDefinitionId.IsNone())
		{
			if (!Inventory->HasItem(Part.ItemDefinitionId, Part.Quantity))
			{
				return false;
			}
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
