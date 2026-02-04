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
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] RefreshBuildingList called"));

	if (!RecipeList)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingMenu] RecipeList widget not bound"));
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] RecipeList is valid: %s"), *RecipeList->GetName());

	// Get all recipes
	TArray<FName> AllRecipeIds;
	UMORecipeDatabaseSettings::GetAllRecipeIds(AllRecipeIds);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Total recipes in database: %d"), AllRecipeIds.Num());

	// Filter to building recipes that are discovered
	TArray<FName> BuildingRecipeIds;
	UMORecipeDiscoveryComponent* Discovery = DiscoveryComponent.Get();

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Discovery component valid: %s"), IsValid(Discovery) ? TEXT("yes") : TEXT("no"));

	// Debug: Check first few recipes for bIsBuilding value
	int32 DebugCount = 0;
	for (const FName& RecipeId : AllRecipeIds)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (!Recipe)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingMenu] Recipe %s - definition not found"), *RecipeId.ToString());
			continue;
		}

		// Log first 5 recipes and any that contain "Build" in the name for debugging
		if (DebugCount < 5 || RecipeId.ToString().Contains(TEXT("Build")))
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Recipe %s: bIsBuilding=%s"),
				*RecipeId.ToString(), Recipe->bIsBuilding ? TEXT("true") : TEXT("false"));
			DebugCount++;
		}

		if (!Recipe->bIsBuilding)
		{
			continue;
		}

		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Found building recipe: %s (RequiresDiscovery: %s)"),
			*RecipeId.ToString(), Recipe->bRequiresDiscovery ? TEXT("yes") : TEXT("no"));

		// Check discovery if required
		if (Recipe->bRequiresDiscovery && IsValid(Discovery))
		{
			if (!Discovery->IsRecipeDiscovered(RecipeId))
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Recipe %s skipped - not discovered"), *RecipeId.ToString());
				continue;
			}
		}

		BuildingRecipeIds.Add(RecipeId);
		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Recipe %s added to list"), *RecipeId.ToString());
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Calling PopulateRecipes with %d building recipes"), BuildingRecipeIds.Num());

	// Populate the recipe list widget
	RecipeList->PopulateRecipes(BuildingRecipeIds);

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Populated with %d buildings"), BuildingRecipeIds.Num());
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
