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

UMOBuildingRecipeListWidget::UMOBuildingRecipeListWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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

	CurrentRecipeIds = RecipeIds;

	// Clear existing entries
	ClearRecipes();

	// Debug: Log widget binding status
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingRecipeListWidget] RecipeScrollBox bound: %s"), RecipeScrollBox ? TEXT("yes") : TEXT("no"));
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingRecipeListWidget] RecipeContainer bound: %s"), RecipeContainer ? TEXT("yes") : TEXT("no"));

	// Get the container to add entries to
	UPanelWidget* Container = RecipeScrollBox ? Cast<UPanelWidget>(RecipeScrollBox) : Cast<UPanelWidget>(RecipeContainer);
	if (!Container)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingRecipeListWidget] No container widget bound (need RecipeScrollBox or RecipeContainer)"));
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingRecipeListWidget] Using container: %s"), *Container->GetName());

	// Check if we have a valid entry widget class
	if (!RecipeEntryWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingRecipeListWidget] No RecipeEntryWidgetClass set - check Blueprint defaults"));
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingRecipeListWidget] RecipeEntryWidgetClass: %s"), *RecipeEntryWidgetClass->GetName());

	// Create entry widgets
	int32 CreatedCount = 0;
	for (const FName& RecipeId : RecipeIds)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingRecipeListWidget] Creating entry for recipe: %s"), *RecipeId.ToString());

		UMOBuildingRecipeEntryWidget* EntryWidget = CreateWidget<UMOBuildingRecipeEntryWidget>(this, RecipeEntryWidgetClass);
		if (!EntryWidget)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingRecipeListWidget] Failed to create widget for recipe: %s"), *RecipeId.ToString());
			continue;
		}

		// Build and set data
		FMOBuildRecipeListEntryData EntryData = BuildEntryData(RecipeId);
		EntryWidget->SetupEntry(EntryData);

		// Bind click handler
		EntryWidget->OnEntryClicked.AddDynamic(this, &UMOBuildingRecipeListWidget::HandleEntryClicked);

		// Add to container
		Container->AddChild(EntryWidget);
		EntryWidgets.Add(EntryWidget);
		CreatedCount++;

		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingRecipeListWidget] Entry widget created for: %s (DisplayName: %s)"),
			*RecipeId.ToString(), *EntryData.DisplayName.ToString());
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingRecipeListWidget] Populated %d building recipes (created %d widgets)"), RecipeIds.Num(), CreatedCount);
}

void UMOBuildingRecipeListWidget::ClearRecipes()
{
	// Remove all entry widgets
	for (UMOBuildingRecipeEntryWidget* Entry : EntryWidgets)
	{
		if (Entry)
		{
			Entry->RemoveFromParent();
		}
	}
	EntryWidgets.Empty();
	SelectedRecipeId = NAME_None;
}

void UMOBuildingRecipeListWidget::RefreshEntryStates()
{
	for (UMOBuildingRecipeEntryWidget* Entry : EntryWidgets)
	{
		if (!Entry)
		{
			continue;
		}

		FName RecipeId = Entry->GetRecipeId();
		bool bCanBuild = CanBuildRecipe(RecipeId);
		bool bIsSelected = (RecipeId == SelectedRecipeId);

		Entry->SetCanBuild(bCanBuild);
		Entry->SetSelected(bIsSelected);
	}
}

void UMOBuildingRecipeListWidget::SelectRecipe(FName RecipeId)
{
	FName OldSelection = SelectedRecipeId;
	SelectedRecipeId = RecipeId;

	// Update visual state of affected entries
	for (UMOBuildingRecipeEntryWidget* Entry : EntryWidgets)
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
		OnRecipeSelected.Broadcast(SelectedRecipeId);
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

void UMOBuildingRecipeListWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UMOBuildingRecipeListWidget::HandleEntryClicked(FName RecipeId)
{
	SelectRecipe(RecipeId);
}

FMOBuildRecipeListEntryData UMOBuildingRecipeListWidget::BuildEntryData(FName RecipeId) const
{
	FMOBuildRecipeListEntryData Data;
	Data.RecipeId = RecipeId;
	Data.bIsSelected = (RecipeId == SelectedRecipeId);
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
