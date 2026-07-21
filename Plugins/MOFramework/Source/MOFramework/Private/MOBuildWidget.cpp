#include "MOBuildWidget.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MOKnowledgeComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOCraftingSubsystem.h"
#include "MORecipeDatabaseSettings.h"
#include "MOBuildingRecipeListWidget.h"
#include "MOBuildingDetailPanel.h"
#include "MOBuildingQueueWidget.h"
#include "MOBuildableActor.h"
#include "MOBuildProgressComponent.h"
#include "MOBuildingTypes.h"
#include "MOCommonButton.h"
#include "Components/WidgetSwitcher.h"

UMOBuildWidget::UMOBuildWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Enable keyboard input for this widget (needed for Tab/Escape to close)
	SetIsFocusable(true);
}

void UMOBuildWidget::InitializeMenu(
	UMOInventoryComponent* InInventory,
	UMOSkillsComponent* InSkills,
	UMOKnowledgeComponent* InKnowledge,
	UMORecipeDiscoveryComponent* InDiscovery)
{
	// Unbind from previous inventory if different
	if (UMOInventoryComponent* OldInventory = InventoryComponent.Get())
	{
		if (OldInventory != InInventory)
		{
			OldInventory->OnInventoryChanged.RemoveDynamic(this, &UMOBuildWidget::HandleInventoryChanged);
		}
	}

	InventoryComponent = InInventory;
	SkillsComponent = InSkills;
	KnowledgeComponent = InKnowledge;
	DiscoveryComponent = InDiscovery;

	// Get crafting subsystem
	if (UWorld* World = GetWorld())
	{
		CraftingSubsystem = World->GetSubsystem<UMOCraftingSubsystem>();
	}

	// Initialize child widgets (remove then add to avoid duplicate bindings)
	if (RecipeList)
	{
		RecipeList->OnRecipeSelected.RemoveDynamic(this, &UMOBuildWidget::HandleRecipeSelected);
		RecipeList->OnSelectionCleared.RemoveDynamic(this, &UMOBuildWidget::HandleRecipeSelectionCleared);
		RecipeList->InitializeList(InInventory, InSkills, InDiscovery);
		RecipeList->OnRecipeSelected.AddDynamic(this, &UMOBuildWidget::HandleRecipeSelected);
		RecipeList->OnSelectionCleared.AddDynamic(this, &UMOBuildWidget::HandleRecipeSelectionCleared);
	}

	if (DetailPanel)
	{
		DetailPanel->OnBuildRequested.RemoveDynamic(this, &UMOBuildWidget::HandleBuildRequested);
		DetailPanel->InitializePanel(InInventory, InSkills, InDiscovery);
		DetailPanel->OnBuildRequested.AddDynamic(this, &UMOBuildWidget::HandleBuildRequested);
	}

	// Bind to inventory changes (only if not already bound to same inventory)
	if (bAutoRefreshOnInventoryChange && InInventory)
	{
		// Remove first to ensure no duplicates, then add
		InInventory->OnInventoryChanged.RemoveDynamic(this, &UMOBuildWidget::HandleInventoryChanged);
		InInventory->OnInventoryChanged.AddDynamic(this, &UMOBuildWidget::HandleInventoryChanged);
	}

	// Initial refresh
	RefreshRecipeList();
}

void UMOBuildWidget::InitializeForBuilding(AMOBuildableActor* Target)
{
	TargetBuilding = Target;

	if (!IsValid(Target))
	{
		return;
	}

	FName TargetRecipeId = Target->GetRecipeId();

	// Initialize the queue widget with the building's progress component
	if (QueueWidget)
	{
		UMOBuildProgressComponent* ProgressComp = Target->FindComponentByClass<UMOBuildProgressComponent>();
		if (ProgressComp)
		{
			QueueWidget->InitializeQueue(ProgressComp);
		}
	}

	// Select the recipe in the list
	if (RecipeList)
	{
		RecipeList->SelectRecipe(TargetRecipeId);
	}
	else if (DetailPanel)
	{
		DetailPanel->DisplayRecipe(TargetRecipeId);
	}
}

FMOBuildProgress UMOBuildWidget::GetBuildOptions() const
{
	// Forward to detail panel if available
	if (DetailPanel)
	{
		return DetailPanel->GetBuildOptions();
	}

	// Return default options
	FMOBuildProgress Options;
	Options.bDrawFromInventory = true;
	Options.bDrawFromNearbyContainers = true;
	Options.bDrawFromSurroundingArea = true;
	Options.GatherRange = 150.0f;
	return Options;
}

void UMOBuildWidget::RefreshRecipeList()
{
	UMOCraftingSubsystem* CraftingSub = CraftingSubsystem.Get();
	if (!CraftingSub)
	{
		return;
	}

	TArray<FName> AllRecipeIds;
	TArray<FName> BuildingRecipeIds;

	// Get all recipes first
	CraftingSub->GetAvailableRecipes(
		KnowledgeComponent.Get(),
		SkillsComponent.Get(),
		EMOCraftingStation::None,
		AllRecipeIds
	);

	// Filter to only building recipes
	for (const FName& RecipeId : AllRecipeIds)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (Recipe && Recipe->bIsBuilding)
		{
			BuildingRecipeIds.Add(RecipeId);
		}
	}

	// Filter by discovery if we have discovery component
	if (UMORecipeDiscoveryComponent* Discovery = DiscoveryComponent.Get())
	{
		BuildingRecipeIds.RemoveAll([Discovery](const FName& RecipeId) {
			return !Discovery->IsRecipeDiscovered(RecipeId);
		});
	}

	// Filter by category if set
	if (!CategoryFilter.IsNone())
	{
		BuildingRecipeIds.RemoveAll([this](const FName& RecipeId) {
			const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
			return !Recipe || Recipe->Category != CategoryFilter;
		});
	}

	// Filter by buildability if requested
	if (bShowOnlyBuildable)
	{
		BuildingRecipeIds.RemoveAll([this](const FName& RecipeId) {
			const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
			if (!Recipe)
			{
				return true;
			}

			UMOInventoryComponent* Inventory = InventoryComponent.Get();
			if (!Inventory)
			{
				return true;
			}

			// Check all build parts that are items
			for (const FMOBuildPart& Part : Recipe->BuildParts)
			{
				if (!Part.ItemDefinitionId.IsNone())
				{
					if (!Inventory->HasItem(Part.ItemDefinitionId, Part.Quantity))
					{
						return true;
					}
				}
			}
			return false;
		});
	}

	// Populate the list
	if (RecipeList)
	{
		RecipeList->PopulateRecipes(BuildingRecipeIds);
	}
}

void UMOBuildWidget::SelectRecipe(FName RecipeId)
{
	if (RecipeList)
	{
		RecipeList->SelectRecipe(RecipeId);
	}
	else if (DetailPanel && !RecipeId.IsNone())
	{
		DetailPanel->DisplayRecipe(RecipeId);
	}
}

FName UMOBuildWidget::GetSelectedRecipeId() const
{
	return RecipeList ? RecipeList->GetSelectedRecipeId() : NAME_None;
}

bool UMOBuildWidget::BuildSelectedRecipe(int32 Count)
{
	const FName SelectedRecipeId = GetSelectedRecipeId();
	if (SelectedRecipeId.IsNone() || Count <= 0)
	{
		return false;
	}

	// Broadcast that a building was selected (for entering placement mode)
	OnBuildingSelected.Broadcast(SelectedRecipeId);

	return true;
}

void UMOBuildWidget::SetCategoryFilter(FName Category)
{
	if (CategoryFilter != Category)
	{
		CategoryFilter = Category;
		RefreshRecipeList();
	}
}

void UMOBuildWidget::ClearCategoryFilter()
{
	SetCategoryFilter(NAME_None);
}

void UMOBuildWidget::SetShowOnlyBuildable(bool bOnlyBuildable)
{
	if (bShowOnlyBuildable != bOnlyBuildable)
	{
		bShowOnlyBuildable = bOnlyBuildable;
		RefreshRecipeList();
	}
}

void UMOBuildWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		// Remove first to avoid duplicate bindings when widget is re-added to viewport
		CloseButton->OnClicked().RemoveAll(this);
		CloseButton->OnClicked().AddUObject(this, &UMOBuildWidget::HandleCloseClicked);
	}
}

void UMOBuildWidget::HandleCloseClicked()
{
	OnRequestClose.Broadcast();
}

void UMOBuildWidget::NativeDestruct()
{
	// Unbind from inventory
	if (UMOInventoryComponent* Inventory = InventoryComponent.Get())
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UMOBuildWidget::HandleInventoryChanged);
	}
	if (RecipeList)
	{
		RecipeList->OnRecipeSelected.RemoveDynamic(this, &UMOBuildWidget::HandleRecipeSelected);
		RecipeList->OnSelectionCleared.RemoveDynamic(this, &UMOBuildWidget::HandleRecipeSelectionCleared);
	}
	if (DetailPanel)
	{
		DetailPanel->OnBuildRequested.RemoveDynamic(this, &UMOBuildWidget::HandleBuildRequested);
	}

	Super::NativeDestruct();
}

FReply UMOBuildWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();

	// Close on Tab or Escape
	if (PressedKey == EKeys::Tab || PressedKey == EKeys::Escape)
	{
		OnRequestClose.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMOBuildWidget::HandleRecipeSelected(FName RecipeId)
{
	if (DetailPanel)
	{
		DetailPanel->DisplayRecipe(RecipeId);
	}
}

void UMOBuildWidget::HandleRecipeSelectionCleared()
{
	if (DetailPanel)
	{
		DetailPanel->ClearDisplay();
	}
}

void UMOBuildWidget::HandleBuildRequested(FName RecipeId, int32 Count)
{
	if (RecipeId == GetSelectedRecipeId())
	{
		// If we have a target building (ghost configuration mode), broadcast OnStartBuild
		if (TargetBuilding.IsValid())
		{
			OnStartBuild.Broadcast();
		}
		else
		{
			// Otherwise, enter placement mode for the selected recipe
			BuildSelectedRecipe(Count);
		}
	}
}

void UMOBuildWidget::HandleInventoryChanged()
{
	if (RecipeList)
	{
		RecipeList->RefreshEntryStates();
	}

	if (DetailPanel)
	{
		DetailPanel->RefreshDisplay();
	}
}
