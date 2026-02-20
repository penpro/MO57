#include "MOCraftingMenu.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MOKnowledgeComponent.h"
#include "MOCraftingQueueComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOCraftingSubsystem.h"
#include "MORecipeDatabaseSettings.h"
#include "MORecipeListWidget.h"
#include "MORecipeDetailPanel.h"
#include "MOCraftingQueueWidget.h"
#include "MOCommonButton.h"
#include "MOCraftingStationActor.h"
#include "MOUIUtils.h"
#include "Components/WidgetSwitcher.h"
#include "Components/TextBlock.h"
#include "Components/CheckBox.h"

UMOCraftingMenu::UMOCraftingMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Enable keyboard input for this widget (needed for Tab/Escape to close)
	SetIsFocusable(true);
}

void UMOCraftingMenu::InitializeMenu(
	UMOInventoryComponent* InInventory,
	UMOSkillsComponent* InSkills,
	UMOKnowledgeComponent* InKnowledge,
	UMOCraftingQueueComponent* InCraftingQueue,
	UMORecipeDiscoveryComponent* InDiscovery)
{
	// Unbind from previous inventory if different
	if (UMOInventoryComponent* OldInventory = InventoryComponent.Get())
	{
		if (OldInventory != InInventory)
		{
			OldInventory->OnInventoryChanged.RemoveDynamic(this, &UMOCraftingMenu::HandleInventoryChanged);
		}
	}

	InventoryComponent = InInventory;
	SkillsComponent = InSkills;
	KnowledgeComponent = InKnowledge;
	CraftingQueueComponent = InCraftingQueue;
	DiscoveryComponent = InDiscovery;

	// Get crafting subsystem
	if (UWorld* World = GetWorld())
	{
		CraftingSubsystem = World->GetSubsystem<UMOCraftingSubsystem>();
	}

	// Initialize child widgets (remove then add to avoid duplicate bindings)
	if (RecipeList)
	{
		RecipeList->OnRecipeSelected.RemoveDynamic(this, &UMOCraftingMenu::HandleRecipeSelected);
		RecipeList->InitializeList(InInventory, InSkills, InDiscovery);
		RecipeList->OnRecipeSelected.AddDynamic(this, &UMOCraftingMenu::HandleRecipeSelected);
	}

	if (DetailPanel)
	{
		DetailPanel->OnCraftRequested.RemoveDynamic(this, &UMOCraftingMenu::HandleCraftRequested);
		DetailPanel->InitializePanel(InInventory, InSkills, InDiscovery);
		DetailPanel->OnCraftRequested.AddDynamic(this, &UMOCraftingMenu::HandleCraftRequested);
	}

	if (QueueWidget && InCraftingQueue)
	{
		QueueWidget->InitializeQueue(InCraftingQueue);
	}

	// Bind to inventory changes (only if not already bound to same inventory)
	if (bAutoRefreshOnInventoryChange && InInventory)
	{
		// Remove first to ensure no duplicates, then add
		InInventory->OnInventoryChanged.RemoveDynamic(this, &UMOCraftingMenu::HandleInventoryChanged);
		InInventory->OnInventoryChanged.AddDynamic(this, &UMOCraftingMenu::HandleInventoryChanged);
	}

	// Initial refresh
	RefreshRecipeList();
}

void UMOCraftingMenu::SetCraftingStation(EMOCraftingStation InStation)
{
	if (CurrentStation != InStation)
	{
		CurrentStation = InStation;
		UpdateStationDisplay();
		RefreshRecipeList();
	}
}

void UMOCraftingMenu::SetActiveStationActor(AMOCraftingStationActor* InStation)
{
	ActiveStationActor = InStation;

	// Update station type based on whether we have a valid station
	if (InStation)
	{
		SetCraftingStation(InStation->GetStationType());
	}
	else
	{
		// Clear to hand crafting when station is removed
		SetCraftingStation(EMOCraftingStation::None);
	}

	UpdateStationDisplay();
}

FText UMOCraftingMenu::GetStationDisplayName() const
{
	// Try to get name from active station actor
	if (AMOCraftingStationActor* Station = ActiveStationActor.Get())
	{
		FName RecipeId = Station->GetRecipeId();
		if (!RecipeId.IsNone())
		{
			const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
			if (Recipe)
			{
				return Recipe->DisplayName;
			}
		}

		// Fallback to station type name
		return FText::FromString(UEnum::GetValueAsString(Station->GetStationType()));
	}

	// No station = hand crafting
	if (CurrentStation == EMOCraftingStation::None)
	{
		return NSLOCTEXT("MOCrafting", "HandCrafting", "Hand Crafting");
	}

	// Station type set but no actor
	return FText::FromString(UEnum::GetValueAsString(CurrentStation));
}

void UMOCraftingMenu::RefreshRecipeList()
{
	UMOCraftingSubsystem* CraftingSub = CraftingSubsystem.Get();
	if (!CraftingSub)
	{
		return;
	}

	TArray<FName> RecipeIds;

	// When "Show All Known" is checked, ignore station filter
	// This shows all discovered recipes but marks unavailable ones with "Requires: X"
	EMOCraftingStation StationForQuery = bShowAllKnownRecipes ? EMOCraftingStation::None : CurrentStation;

	// Get available recipes based on current settings
	if (bShowOnlyCraftable)
	{
		CraftingSub->GetCraftableRecipes(
			KnowledgeComponent.Get(),
			SkillsComponent.Get(),
			InventoryComponent.Get(),
			StationForQuery,
			RecipeIds
		);
	}
	else
	{
		CraftingSub->GetAvailableRecipes(
			KnowledgeComponent.Get(),
			SkillsComponent.Get(),
			StationForQuery,
			RecipeIds
		);
	}

	// Filter by discovery if we have discovery component
	if (UMORecipeDiscoveryComponent* Discovery = DiscoveryComponent.Get())
	{
		RecipeIds.RemoveAll([Discovery](const FName& RecipeId) {
			return !Discovery->IsRecipeDiscovered(RecipeId);
		});
	}

	// Filter by category if set
	if (!CategoryFilter.IsNone())
	{
		RecipeIds.RemoveAll([this](const FName& RecipeId) {
			const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
			return !Recipe || Recipe->Category != CategoryFilter;
		});
	}

	// Populate the list
	if (RecipeList)
	{
		RecipeList->PopulateRecipes(RecipeIds);

		// Restore selection if still valid
		if (!SelectedRecipeId.IsNone() && RecipeIds.Contains(SelectedRecipeId))
		{
			RecipeList->SelectRecipe(SelectedRecipeId);
		}
	}
}

void UMOCraftingMenu::SelectRecipe(FName RecipeId)
{
	SelectedRecipeId = RecipeId;

	if (RecipeList)
	{
		RecipeList->SelectRecipe(RecipeId);
	}

	if (DetailPanel)
	{
		DetailPanel->DisplayRecipe(RecipeId);
	}
}

bool UMOCraftingMenu::CraftSelectedRecipe(int32 Count)
{
	if (SelectedRecipeId.IsNone() || Count <= 0)
	{
		return false;
	}

	// If we have a queue component, enqueue the craft
	if (UMOCraftingQueueComponent* Queue = CraftingQueueComponent.Get())
	{
		return Queue->EnqueueCraft(SelectedRecipeId, Count, CurrentStation);
	}

	// Otherwise, try immediate craft via subsystem
	if (UMOCraftingSubsystem* CraftingSub = CraftingSubsystem.Get())
	{
		for (int32 i = 0; i < Count; ++i)
		{
			FMOCraftResult Result = CraftingSub->ExecuteCraft(
				SelectedRecipeId,
				InventoryComponent.Get(),
				SkillsComponent.Get()
			);

			if (!Result.bSuccess)
			{
				return i > 0; // Return true if at least one craft succeeded
			}
		}
		return true;
	}

	return false;
}

void UMOCraftingMenu::SetCategoryFilter(FName Category)
{
	if (CategoryFilter != Category)
	{
		CategoryFilter = Category;
		RefreshRecipeList();
	}
}

void UMOCraftingMenu::ClearCategoryFilter()
{
	SetCategoryFilter(NAME_None);
}

void UMOCraftingMenu::SetShowOnlyCraftable(bool bOnlyCraftable)
{
	if (bShowOnlyCraftable != bOnlyCraftable)
	{
		bShowOnlyCraftable = bOnlyCraftable;
		RefreshRecipeList();
	}
}

void UMOCraftingMenu::SetShowAllKnown(bool bShowAll)
{
	if (bShowAllKnownRecipes != bShowAll)
	{
		bShowAllKnownRecipes = bShowAll;
		RefreshRecipeList();
	}
}

void UMOCraftingMenu::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		// Remove first to avoid duplicate bindings when widget is re-added to viewport
		CloseButton->OnClicked().RemoveAll(this);
		CloseButton->OnClicked().AddUObject(this, &UMOCraftingMenu::HandleCloseClicked);
	}

	if (ShowAllKnownCheckbox)
	{
		ShowAllKnownCheckbox->OnCheckStateChanged.RemoveDynamic(this, &UMOCraftingMenu::HandleShowAllKnownChanged);
		ShowAllKnownCheckbox->OnCheckStateChanged.AddDynamic(this, &UMOCraftingMenu::HandleShowAllKnownChanged);
		ShowAllKnownCheckbox->SetIsChecked(bShowAllKnownRecipes);
	}

	UpdateStationDisplay();
}

void UMOCraftingMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update fuel time display if we have an active station
	if (FuelTimeText && ActiveStationActor.IsValid())
	{
		AMOCraftingStationActor* Station = ActiveStationActor.Get();
		if (Station && Station->bRequiresFuel)
		{
			float TimeRemaining = Station->GetFuelTimeRemaining();
			FText TimeText = FText::Format(
				NSLOCTEXT("MOCrafting", "FuelTimeRemaining", "{0} remaining"),
				UMOUIUtils::FormatDurationAsTimeCode(TimeRemaining)
			);
			FuelTimeText->SetText(TimeText);
		}
	}
}

void UMOCraftingMenu::HandleCloseClicked()
{
	// Broadcast standard delegate (prefer this for new code)
	OnCloseRequested.Broadcast();
	// Broadcast legacy delegate for backward compatibility
	OnRequestClose.Broadcast();
}

void UMOCraftingMenu::NativeDestruct()
{
	// Unbind from inventory
	if (UMOInventoryComponent* Inventory = InventoryComponent.Get())
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UMOCraftingMenu::HandleInventoryChanged);
	}

	Super::NativeDestruct();
}

FReply UMOCraftingMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();

	// Close on Tab or Escape
	if (PressedKey == EKeys::Tab || PressedKey == EKeys::Escape)
	{
		// Broadcast standard delegate (prefer this for new code)
		OnCloseRequested.Broadcast();
		// Broadcast legacy delegate for backward compatibility
		OnRequestClose.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMOCraftingMenu::HandleRecipeSelected(FName RecipeId)
{
	SelectedRecipeId = RecipeId;

	if (DetailPanel)
	{
		DetailPanel->DisplayRecipe(RecipeId);
	}
}

void UMOCraftingMenu::HandleCraftRequested(FName RecipeId, int32 Count)
{
	if (RecipeId == SelectedRecipeId)
	{
		CraftSelectedRecipe(Count);
	}
}

void UMOCraftingMenu::HandleInventoryChanged()
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

void UMOCraftingMenu::HandleShowAllKnownChanged(bool bIsChecked)
{
	SetShowAllKnown(bIsChecked);
}

void UMOCraftingMenu::UpdateStationDisplay()
{
	// Update station name
	if (StationNameText)
	{
		StationNameText->SetText(GetStationDisplayName());
	}

	// Update fuel time
	if (FuelTimeText)
	{
		AMOCraftingStationActor* Station = ActiveStationActor.Get();
		if (Station && Station->bRequiresFuel)
		{
			float TimeRemaining = Station->GetFuelTimeRemaining();
			FText TimeText = FText::Format(
				NSLOCTEXT("MOCrafting", "FuelTimeRemaining", "{0} remaining"),
				UMOUIUtils::FormatDurationAsTimeCode(TimeRemaining)
			);
			FuelTimeText->SetText(TimeText);
			FuelTimeText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			FuelTimeText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
