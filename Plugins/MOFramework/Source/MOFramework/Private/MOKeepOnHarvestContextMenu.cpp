#include "MOKeepOnHarvestContextMenu.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOHarvestSubsystem.h"
#include "MORecipeDatabaseSettings.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "MOInventoryComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/InstancedStaticMeshComponent.h"

UMOKeepOnHarvestContextMenu::UMOKeepOnHarvestContextMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOKeepOnHarvestContextMenu::RequestClose()
{
	// Broadcast legacy delegate for backward compatibility
	RequestClose();
	// Also call base which broadcasts OnCloseRequested
	Super::RequestClose();
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UMOKeepOnHarvestContextMenu::InitializeForTarget(
	const FMOInteractionTarget& Target,
	UMOKnowledgeComponent* Knowledge,
	UMOSkillsComponent* Skills,
	UMOInventoryComponent* Inventory
)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] InitializeForTarget called. ButtonClass=%s, ButtonContainer=%s"),
		ButtonClass ? *ButtonClass->GetName() : TEXT("NULL"),
		ButtonContainer ? TEXT("bound") : TEXT("NULL"));

	if (!Target.IsValid() || !Target.bIsInstancedMeshTarget)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] InitializeForTarget: Invalid or non-ISM target"));
		return;
	}

	CurrentTarget = Target;
	CachedKnowledge = Knowledge;
	CachedSkills = Skills;
	CachedInventory = Inventory;

	// Get harvest subsystem
	UMOHarvestSubsystem* HarvestSubsystem = GetWorld()->GetSubsystem<UMOHarvestSubsystem>();
	if (!HarvestSubsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] No harvest subsystem found"));
		return;
	}

	// Collect target tags
	TargetTags = HarvestSubsystem->CollectTargetTags(Target.ISMComponent.Get());

	// Get smart inspect item
	SmartInspectItemId = HarvestSubsystem->GetSmartInspectItemId(TargetTags, Knowledge, Skills);

	// Get available harvest recipes
	HarvestSubsystem->GetHarvestRecipesForTags(TargetTags, Knowledge, Skills, Inventory, AvailableHarvestRecipes);

	// Find chop down recipe (even if we don't have the tool)
	ChopDownRecipeId = HarvestSubsystem->FindDestroyRecipeForTags(TargetTags);
	bCanExecuteChopDown = HarvestSubsystem->CanExecuteDestroyRecipe(ChopDownRecipeId, Inventory);

	UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] ChopDown: Recipe=%s, CanExecute=%s"),
		*ChopDownRecipeId.ToString(), bCanExecuteChopDown ? TEXT("yes") : TEXT("no"));

	// Refresh display
	RefreshDisplay();

	UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Initialized with %d tags, %d harvest recipes, inspect='%s', chopdown='%s'"),
		TargetTags.Num(),
		AvailableHarvestRecipes.Num(),
		*SmartInspectItemId.ToString(),
		*ChopDownRecipeId.ToString());
}

void UMOKeepOnHarvestContextMenu::RefreshDisplay()
{
	// Update target name
	if (TargetNameText)
	{
		TargetNameText->SetText(GetTargetDisplayName());
	}

	// Populate all buttons
	PopulateButtons();

	// Notify Blueprint
	OnButtonStatesUpdated(CanInspect(), CanChopDown(), AvailableHarvestRecipes.Num());
}

// ============================================================================
// STATE QUERIES
// ============================================================================

FText UMOKeepOnHarvestContextMenu::GetTargetDisplayName() const
{
	// Look for "Name " tag prefix
	for (const FName& Tag : TargetTags)
	{
		FString TagString = Tag.ToString();
		if (TagString.StartsWith(TEXT("Name ")))
		{
			return FText::FromString(TagString.RightChop(5));
		}
	}

	// Fallback to "Unknown Object"
	return NSLOCTEXT("MO", "UnknownObject", "Unknown Object");
}

// SetPopupPosition is inherited from UMOContextMenuBase

// ============================================================================
// OVERRIDES
// ============================================================================

void UMOKeepOnHarvestContextMenu::NativeConstruct()
{
	Super::NativeConstruct();
}

void UMOKeepOnHarvestContextMenu::NativeDestruct()
{
	ClearButtons();
	Super::NativeDestruct();
}

// NativeOnKeyDown is inherited from UMOContextMenuBase

// ============================================================================
// BUTTON MANAGEMENT
// ============================================================================

UMOCommonButton* UMOKeepOnHarvestContextMenu::CreateButton(const FText& Label)
{
	if (!ButtonClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] No ButtonClass set"));
		return nullptr;
	}

	UMOCommonButton* Button = CreateWidget<UMOCommonButton>(GetOwningPlayer(), ButtonClass);
	if (!Button)
	{
		return nullptr;
	}

	Button->SetButtonText(Label);
	ButtonContainer->AddChild(Button);
	CreatedButtons.Add(Button);

	return Button;
}

void UMOKeepOnHarvestContextMenu::PopulateButtons()
{
	ClearButtons();

	if (!ButtonContainer)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] No ButtonContainer bound"));
		return;
	}

	if (!ButtonClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] No ButtonClass set"));
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] PopulateButtons: CanInspect=%s, HarvestRecipes=%d, ChopDownRecipe=%s"),
		CanInspect() ? TEXT("yes") : TEXT("no"),
		AvailableHarvestRecipes.Num(),
		*ChopDownRecipeId.ToString());

	// 1. Inspect button (if available)
	if (CanInspect())
	{
		UMOCommonButton* InspectBtn = CreateButton(NSLOCTEXT("MO", "InspectButton", "Inspect"));
		if (InspectBtn)
		{
			InspectBtn->OnClicked().AddWeakLambda(this, [this]()
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Inspect clicked (item: %s)"), *SmartInspectItemId.ToString());
				OnInspectClicked.Broadcast();
				RequestClose();
			});
		}
	}

	// 2. Harvest buttons (dynamic based on available recipes)
	for (const FName& RecipeId : AvailableHarvestRecipes)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (!Recipe)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] Recipe '%s' not found in database"), *RecipeId.ToString());
			continue;
		}

		UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Adding harvest button: %s"), *Recipe->DisplayName.ToString());

		UMOCommonButton* HarvestBtn = CreateButton(Recipe->DisplayName);
		if (HarvestBtn)
		{
			FName CapturedRecipeId = RecipeId;
			HarvestBtn->OnClicked().AddWeakLambda(this, [this, CapturedRecipeId]()
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Harvest clicked (recipe: %s)"), *CapturedRecipeId.ToString());
				OnHarvestClicked.Broadcast(CapturedRecipeId);
				RequestClose();
			});
		}
	}

	// 3. Chop Down button - always show if recipe exists, but disable if no tool
	if (HasChopDownRecipe())
	{
		const FMORecipeDefinitionRow* ChopRecipe = UMORecipeDatabaseSettings::GetRecipeDefinition(ChopDownRecipeId);
		FText ChopLabel = ChopRecipe ? ChopRecipe->DisplayName : NSLOCTEXT("MO", "ChopDownButton", "Chop Down");

		UMOCommonButton* ChopBtn = CreateButton(ChopLabel);
		if (ChopBtn)
		{
			// Disable if requirements not met (CanChopDown checks tool requirements)
			ChopBtn->SetIsEnabled(CanChopDown());

			ChopBtn->OnClicked().AddWeakLambda(this, [this]()
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Chop Down clicked (recipe: %s)"), *ChopDownRecipeId.ToString());
				OnHarvestClicked.Broadcast(ChopDownRecipeId);
				RequestClose();
			});
		}
	}

	// Note: "Search for Insects" removed - future feature

	OnHarvestButtonsUpdated(CreatedButtons.Num());
	UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Created %d buttons"), CreatedButtons.Num());
}

void UMOKeepOnHarvestContextMenu::ClearButtons()
{
	for (UMOCommonButton* Button : CreatedButtons)
	{
		if (IsValid(Button))
		{
			Button->OnClicked().RemoveAll(this);
			Button->RemoveFromParent();
		}
	}
	CreatedButtons.Empty();
}
