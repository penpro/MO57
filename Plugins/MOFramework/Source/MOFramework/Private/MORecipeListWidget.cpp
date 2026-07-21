#include "MORecipeListWidget.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MOKnowledgeComponent.h"
#include "MOCraftingSubsystem.h"
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

void UMORecipeListWidget::SetCraftingContext(UMOKnowledgeComponent* InKnowledge, EMOCraftingStation InStation)
{
	KnowledgeComponent = InKnowledge;
	StationFilter = InStation;
	RefreshEntryStates();
}

void UMORecipeListWidget::PopulateRecipes(const TArray<FName>& RecipeIds)
{
	if (!RecipeEntryWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MORecipeListWidget] No RecipeEntryWidgetClass set"));
		return;
	}

	EntryWidgetClass = RecipeEntryWidgetClass;
	Super::PopulateList(RecipeIds);
	UE_LOG(LogMOFramework, Log, TEXT("[MORecipeListWidget] Populated %d recipes"), GetEntryCount());
}

void UMORecipeListWidget::ClearRecipes()
{
	Super::ClearList();
}

void UMORecipeListWidget::RefreshEntryStates()
{
	Super::RefreshEntryStates();
}

void UMORecipeListWidget::SelectRecipe(FName RecipeId)
{
	SelectEntry(RecipeId);
}

void UMORecipeListWidget::SelectEntry(FName EntryId)
{
	const FName PreviousId = GetSelectedEntryId();
	Super::SelectEntry(EntryId);
	const FName CurrentId = GetSelectedEntryId();
	if (CurrentId != PreviousId && !CurrentId.IsNone())
	{
		OnRecipeSelection.Broadcast(CurrentId);
		OnRecipeSelected.Broadcast(CurrentId);
	}
}

void UMORecipeListWidget::SetStationFilter(EMOCraftingStation Station)
{
	StationFilter = Station;
	RefreshEntryStates();
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

void UMORecipeListWidget::ConfigureEntry_Implementation(UMOListEntryBase* Entry, FName EntryId)
{
	if (UMORecipeEntryWidget* RecipeEntry = Cast<UMORecipeEntryWidget>(Entry))
	{
		RecipeEntry->SetupEntry(BuildEntryData(EntryId));
	}
}

void UMORecipeListWidget::RefreshEntryState_Implementation(UMOListEntryBase* Entry, FName EntryId)
{
	if (UMORecipeEntryWidget* RecipeEntry = Cast<UMORecipeEntryWidget>(Entry))
	{
		RecipeEntry->SetCanCraft(CanCraftRecipe(EntryId));
		RecipeEntry->SetSelected(EntryId == GetSelectedEntryId());
	}
}

FMORecipeListEntryData UMORecipeListWidget::BuildEntryData(FName RecipeId) const
{
	FMORecipeListEntryData Data;
	Data.RecipeId = RecipeId;
	Data.bIsSelected = (RecipeId == GetSelectedEntryId());
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
	if (UWorld* World = GetWorld())
	{
		if (const UMOCraftingSubsystem* Crafting = World->GetSubsystem<UMOCraftingSubsystem>())
		{
			return Crafting->CanCraftRecipe(
				RecipeId,
				KnowledgeComponent.Get(),
				SkillsComponent.Get(),
				InventoryComponent.Get(),
				StationFilter).bCanCraft;
		}
	}

	return false;
}
