#include "MOBuildingMenu.h"
#include "MOFramework.h"
#include "MOKnowledgeComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOBuildingEntryWidget.h"
#include "MOCommonButton.h"
#include "Components/ScrollBox.h"

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

void UMOBuildingMenu::InitializeMenu(UMOKnowledgeComponent* InKnowledge, UMORecipeDiscoveryComponent* InDiscovery)
{
	KnowledgeComponent = InKnowledge;
	DiscoveryComponent = InDiscovery;

	RefreshBuildingList();
}

void UMOBuildingMenu::RefreshBuildingList()
{
	ClearBuildingList();

	if (!BuildingEntryWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingMenu] BuildingEntryWidgetClass not set"));
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	// Get all recipes
	TArray<FName> AllRecipeIds;
	UMORecipeDatabaseSettings::GetAllRecipeIds(AllRecipeIds);

	// Filter to building recipes that are discovered
	UMORecipeDiscoveryComponent* Discovery = DiscoveryComponent.Get();

	for (const FName& RecipeId : AllRecipeIds)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (!Recipe || !Recipe->bIsBuilding)
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

		// Create entry widget
		UMOBuildingEntryWidget* Entry = CreateWidget<UMOBuildingEntryWidget>(PC, BuildingEntryWidgetClass);
		if (Entry)
		{
			Entry->InitializeEntry(RecipeId, *Recipe);
			Entry->OnEntryClicked.AddDynamic(this, &UMOBuildingMenu::HandleBuildingEntryClicked);

			if (BuildingListScrollBox)
			{
				BuildingListScrollBox->AddChild(Entry);
			}
			EntryWidgets.Add(Entry);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Populated with %d buildings"), EntryWidgets.Num());
}

void UMOBuildingMenu::ClearBuildingList()
{
	for (UMOBuildingEntryWidget* Entry : EntryWidgets)
	{
		if (Entry)
		{
			Entry->RemoveFromParent();
		}
	}
	EntryWidgets.Empty();
}

void UMOBuildingMenu::HandleCloseClicked()
{
	OnRequestClose.Broadcast();
}

void UMOBuildingMenu::HandleBuildingEntryClicked(FName RecipeId)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingMenu] Building entry clicked: %s"), *RecipeId.ToString());
	OnBuildingSelected.Broadcast(RecipeId);
}
