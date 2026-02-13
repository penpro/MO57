#include "MOBuildingMenu.h"
#include "MOFramework.h"
#include "MOKnowledgeComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOBuildingRecipeListWidget.h"
#include "MOBuildingDetailPanel.h"
#include "MOCommonButton.h"

UMOBuildingMenu::UMOBuildingMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UMOBuildingMenu::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
		CloseButton->OnClicked().AddUObject(this, &UMOBuildingMenu::HandleCloseClicked);
	}

	// Bind to recipe list selection - shows recipe in detail panel
	if (RecipeList)
	{
		RecipeList->OnRecipeSelected.RemoveDynamic(this, &UMOBuildingMenu::HandleRecipeSelected);
		RecipeList->OnRecipeSelected.AddDynamic(this, &UMOBuildingMenu::HandleRecipeSelected);
	}

	// Bind to detail panel Build button - triggers placement mode
	if (DetailPanel)
	{
		DetailPanel->OnBuildRequested.RemoveDynamic(this, &UMOBuildingMenu::HandleBuildRequested);
		DetailPanel->OnBuildRequested.AddDynamic(this, &UMOBuildingMenu::HandleBuildRequested);
	}
}

void UMOBuildingMenu::NativeDestruct()
{
	// Clean up button bindings
	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
	}

	// Clean up child widget bindings
	if (RecipeList)
	{
		RecipeList->OnRecipeSelected.RemoveDynamic(this, &UMOBuildingMenu::HandleRecipeSelected);
	}
	if (DetailPanel)
	{
		DetailPanel->OnBuildRequested.RemoveDynamic(this, &UMOBuildingMenu::HandleBuildRequested);
	}

	Super::NativeDestruct();
}

FReply UMOBuildingMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();

	// Close on Tab, Escape, or B (the open key)
	if (PressedKey == EKeys::Tab || PressedKey == EKeys::Escape || PressedKey == EKeys::B)
	{
		OnRequestClose.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMOBuildingMenu::InitializeMenu(
	UMOKnowledgeComponent* InKnowledge,
	UMORecipeDiscoveryComponent* InDiscovery,
	UMOInventoryComponent* InInventory,
	UMOSkillsComponent* InSkills)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] InitializeMenu called"));
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] KnowledgeComponent valid: %s"), IsValid(InKnowledge) ? TEXT("yes") : TEXT("no"));
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] DiscoveryComponent valid: %s"), IsValid(InDiscovery) ? TEXT("yes") : TEXT("no"));
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] InventoryComponent valid: %s"), IsValid(InInventory) ? TEXT("yes") : TEXT("no"));
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] SkillsComponent valid: %s"), IsValid(InSkills) ? TEXT("yes") : TEXT("no"));

	KnowledgeComponent = InKnowledge;
	DiscoveryComponent = InDiscovery;
	InventoryComponent = InInventory;
	SkillsComponent = InSkills;

	// Initialize the recipe list widget
	if (RecipeList)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Initializing RecipeList widget"));
		RecipeList->InitializeList(InInventory, InSkills, InDiscovery);
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingMenu] RecipeList is null during InitializeMenu"));
	}

	// Initialize the detail panel
	if (DetailPanel)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Initializing DetailPanel widget"));
		DetailPanel->InitializePanel(InInventory, InSkills, InDiscovery);
		DetailPanel->ClearDisplay();
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingMenu] DetailPanel is null during InitializeMenu"));
	}

	RefreshBuildingList();
}

void UMOBuildingMenu::RefreshBuildingList()
{
	if (!RecipeList)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingMenu] RecipeList widget not bound"));
		return;
	}

	// Get pre-cached building recipes (O(1) instead of O(n) filtering)
	TArray<FName> AllBuildingRecipes;
	UMORecipeDatabaseSettings::GetBuildingRecipes(AllBuildingRecipes);

	// Filter by discovery requirements
	TArray<FName> DiscoveredBuildingRecipes;
	DiscoveredBuildingRecipes.Reserve(AllBuildingRecipes.Num());

	UMORecipeDiscoveryComponent* Discovery = DiscoveryComponent.Get();

	for (const FName& RecipeId : AllBuildingRecipes)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (!Recipe)
		{
			continue;
		}

		// Check discovery if required
		if (Recipe->bRequiresDiscovery && IsValid(Discovery))
		{
			if (!Discovery->IsRecipeDiscovered(RecipeId))
			{
				continue;
			}
		}

		DiscoveredBuildingRecipes.Add(RecipeId);
	}

	// Populate the recipe list widget
	RecipeList->PopulateRecipes(DiscoveredBuildingRecipes);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Populated with %d/%d buildings (discovered/total)"),
		DiscoveredBuildingRecipes.Num(), AllBuildingRecipes.Num());
}

void UMOBuildingMenu::HandleCloseClicked()
{
	OnRequestClose.Broadcast();
}

void UMOBuildingMenu::HandleRecipeSelected(FName RecipeId)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Recipe selected in list: %s"), *RecipeId.ToString());

	// Show the recipe in the detail panel instead of immediately entering build mode
	if (DetailPanel)
	{
		DetailPanel->DisplayRecipe(RecipeId);
		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Displaying recipe in detail panel"));
	}
	else
	{
		// No detail panel - fall back to immediate build mode (old behavior)
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingMenu] No DetailPanel bound - entering placement mode directly"));
		OnBuildingSelected.Broadcast(RecipeId);
	}
}

void UMOBuildingMenu::HandleBuildRequested(FName RecipeId, int32 Count)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Build requested for recipe: %s (count: %d)"), *RecipeId.ToString(), Count);

	// Broadcast to trigger placement mode
	OnBuildingSelected.Broadcast(RecipeId);
}
