#include "MOBuildingDetailPanel.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOItemDatabaseSettings.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/PanelWidget.h"
#include "Components/SpinBox.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"

UMOBuildingDetailPanel::UMOBuildingDetailPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOBuildingDetailPanel::InitializePanel(
	UMOInventoryComponent* InInventory,
	UMOSkillsComponent* InSkills,
	UMORecipeDiscoveryComponent* InDiscovery)
{
	InventoryComponent = InInventory;
	SkillsComponent = InSkills;
	DiscoveryComponent = InDiscovery;
}

void UMOBuildingDetailPanel::DisplayRecipe(FName RecipeId)
{
	DisplayedRecipeId = RecipeId;
	BuildAmount = 1;

	if (RecipeId.IsNone())
	{
		ClearDisplay();
		return;
	}

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe || !Recipe->bIsBuilding)
	{
		ClearDisplay();
		return;
	}

	GatherRange = Recipe->BuildRange;

	// Update name
	if (RecipeNameText)
	{
		RecipeNameText->SetText(Recipe->DisplayName);
	}

	// Update description
	if (RecipeDescriptionText)
	{
		RecipeDescriptionText->SetText(Recipe->Description);
	}

	// Update icon
	if (RecipeIcon)
	{
		UTexture2D* IconTexture = nullptr;

		if (!Recipe->Icon.IsNull())
		{
			IconTexture = Recipe->Icon.LoadSynchronous();
		}

		if (IconTexture)
		{
			RecipeIcon->SetBrushFromTexture(IconTexture);
			RecipeIcon->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			RecipeIcon->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	// Update skill requirement text
	if (SkillRequirementText)
	{
		if (!Recipe->RequiredSkillId.IsNone() && Recipe->RequiredSkillLevel > 0)
		{
			int32 CurrentLevel = 0;
			if (UMOSkillsComponent* Skills = SkillsComponent.Get())
			{
				CurrentLevel = Skills->GetSkillLevel(Recipe->RequiredSkillId);
			}

			FText SkillText = FText::Format(
				NSLOCTEXT("MOBuilding", "SkillReq", "{0}: {1}/{2}"),
				FText::FromName(Recipe->RequiredSkillId),
				FText::AsNumber(CurrentLevel),
				FText::AsNumber(Recipe->RequiredSkillLevel)
			);
			SkillRequirementText->SetText(SkillText);
			SkillRequirementText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			SkillRequirementText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update build time text (uses CraftTimeText binding for compatibility)
	if (CraftTimeText)
	{
		if (Recipe->TotalBuildTime > 0.0f)
		{
			FText TimeText;
			if (Recipe->TotalBuildTime >= 3600.0f)
			{
				float Hours = Recipe->TotalBuildTime / 3600.0f;
				TimeText = FText::Format(NSLOCTEXT("MOBuilding", "TimeHours", "{0}h"), FText::AsNumber(FMath::RoundToInt(Hours * 10) / 10.0f));
			}
			else if (Recipe->TotalBuildTime >= 60.0f)
			{
				float Minutes = Recipe->TotalBuildTime / 60.0f;
				TimeText = FText::Format(NSLOCTEXT("MOBuilding", "TimeMinutes", "{0}m"), FText::AsNumber(FMath::RoundToInt(Minutes)));
			}
			else
			{
				TimeText = FText::Format(NSLOCTEXT("MOBuilding", "TimeSeconds", "{0}s"), FText::AsNumber(FMath::RoundToInt(Recipe->TotalBuildTime)));
			}
			CraftTimeText->SetText(TimeText);
			CraftTimeText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			CraftTimeText->SetText(NSLOCTEXT("MOBuilding", "Instant", "Instant"));
			CraftTimeText->SetVisibility(ESlateVisibility::Visible);
		}
	}

	// Build part data (ingredients)
	CachedBuildParts.Empty();
	for (const FMOBuildPart& Part : Recipe->BuildParts)
	{
		CachedBuildParts.Add(BuildPartData(Part));
	}

	// Build output data
	CachedOutputs.Empty();
	CachedOutputs.Add(BuildOutputData(Recipe));

	// Populate containers with text widgets
	PopulateIngredientsContainer();
	PopulateOutputsContainer();

	// Update build amount controls
	int32 MaxAmount = FMath::Max(1, GetMaxBuildableAmount());

	if (CraftAmountSpinBox)
	{
		CraftAmountSpinBox->SetMaxValue(static_cast<float>(MaxAmount));
		CraftAmountSpinBox->SetMaxSliderValue(static_cast<float>(MaxAmount));
		CraftAmountSpinBox->SetValue(BuildAmount);
	}
	if (CraftAmountSlider)
	{
		CraftAmountSlider->SetMaxValue(static_cast<float>(MaxAmount));
		CraftAmountSlider->SetValue(static_cast<float>(BuildAmount));
	}
	if (CraftAmountText)
	{
		CraftAmountText->SetText(FText::AsNumber(BuildAmount));
	}

	// Set default checkbox states
	if (InventoryCheckbox)
	{
		InventoryCheckbox->SetIsChecked(true);
	}
	if (ContainersCheckbox)
	{
		ContainersCheckbox->SetIsChecked(true);
	}
	if (SurroundingCheckbox)
	{
		SurroundingCheckbox->SetIsChecked(true);
	}

	UpdateBuildButtonState();

	// Notify Blueprint
	OnRecipeDisplayed(RecipeId, Recipe->DisplayName, Recipe->Description);
	OnBuildPartsUpdated(CachedBuildParts);
	OnOutputsUpdated(CachedOutputs);
}

void UMOBuildingDetailPanel::ClearDisplay()
{
	DisplayedRecipeId = NAME_None;
	CachedBuildParts.Empty();
	CachedOutputs.Empty();

	// Clear ingredient widgets
	for (UTextBlock* TextWidget : IngredientTextWidgets)
	{
		if (TextWidget)
		{
			TextWidget->RemoveFromParent();
		}
	}
	IngredientTextWidgets.Empty();

	// Clear output widgets
	for (UTextBlock* TextWidget : OutputTextWidgets)
	{
		if (TextWidget)
		{
			TextWidget->RemoveFromParent();
		}
	}
	OutputTextWidgets.Empty();

	if (RecipeNameText)
	{
		RecipeNameText->SetText(FText::GetEmpty());
	}
	if (RecipeDescriptionText)
	{
		RecipeDescriptionText->SetText(FText::GetEmpty());
	}
	if (RecipeIcon)
	{
		RecipeIcon->SetVisibility(ESlateVisibility::Hidden);
	}
	if (SkillRequirementText)
	{
		SkillRequirementText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (CraftTimeText)
	{
		CraftTimeText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (CraftButton)
	{
		CraftButton->SetIsEnabled(false);
	}
}

void UMOBuildingDetailPanel::RefreshDisplay()
{
	if (!DisplayedRecipeId.IsNone())
	{
		// Rebuild part data with updated counts
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(DisplayedRecipeId);
		if (Recipe)
		{
			CachedBuildParts.Empty();
			for (const FMOBuildPart& Part : Recipe->BuildParts)
			{
				CachedBuildParts.Add(BuildPartData(Part));
			}

			// Repopulate the container
			PopulateIngredientsContainer();

			OnBuildPartsUpdated(CachedBuildParts);
		}

		UpdateBuildButtonState();
	}
}

void UMOBuildingDetailPanel::SetBuildAmount(int32 Amount)
{
	int32 MaxAmount = FMath::Max(1, GetMaxBuildableAmount());
	BuildAmount = FMath::Clamp(Amount, 1, MaxAmount);

	if (CraftAmountSpinBox)
	{
		CraftAmountSpinBox->SetValue(BuildAmount);
	}
	if (CraftAmountSlider)
	{
		CraftAmountSlider->SetValue(static_cast<float>(BuildAmount));
	}
	if (CraftAmountText)
	{
		CraftAmountText->SetText(FText::AsNumber(BuildAmount));
	}

	UpdateBuildButtonState();
}

int32 UMOBuildingDetailPanel::GetMaxBuildableAmount() const
{
	if (DisplayedRecipeId.IsNone())
	{
		return 0;
	}

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(DisplayedRecipeId);
	if (!Recipe)
	{
		return 0;
	}

	UMOInventoryComponent* Inventory = InventoryComponent.Get();
	if (!Inventory)
	{
		return 0;
	}

	int32 MaxAmount = INT_MAX;

	// Check all build parts that require items
	for (const FMOBuildPart& Part : Recipe->BuildParts)
	{
		if (!Part.ItemDefinitionId.IsNone())
		{
			int32 Available = Inventory->GetItemCountByDefinitionId(Part.ItemDefinitionId);
			int32 CanMake = Available / Part.Quantity;
			MaxAmount = FMath::Min(MaxAmount, CanMake);
		}
	}

	return MaxAmount == INT_MAX ? 0 : MaxAmount;
}

FMOBuildProgress UMOBuildingDetailPanel::GetBuildOptions() const
{
	FMOBuildProgress Options;

	Options.bDrawFromInventory = InventoryCheckbox ? InventoryCheckbox->IsChecked() : true;
	Options.bDrawFromNearbyContainers = ContainersCheckbox ? ContainersCheckbox->IsChecked() : true;
	Options.bDrawFromSurroundingArea = SurroundingCheckbox ? SurroundingCheckbox->IsChecked() : true;
	Options.GatherRange = GatherRange;

	return Options;
}

void UMOBuildingDetailPanel::GetBuildParts(TArray<FMOBuildPartDisplayData>& OutBuildParts) const
{
	OutBuildParts = CachedBuildParts;
}

void UMOBuildingDetailPanel::GetOutputs(TArray<FMOBuildOutputDisplayData>& OutOutputs) const
{
	OutOutputs = CachedOutputs;
}

bool UMOBuildingDetailPanel::CanBuildCurrentRecipe() const
{
	if (DisplayedRecipeId.IsNone())
	{
		return false;
	}

	// Check all build parts for requested amount
	for (const FMOBuildPartDisplayData& Part : CachedBuildParts)
	{
		// Only check items, not actions
		if (!Part.bIsAction)
		{
			int32 TotalRequired = Part.RequiredQuantity * BuildAmount;
			if (Part.AvailableQuantity < TotalRequired)
			{
				return false;
			}
		}
	}

	return true;
}

void UMOBuildingDetailPanel::NativeConstruct()
{
	Super::NativeConstruct();

	// Remove first to avoid duplicate bindings when widget is re-added to viewport
	if (CraftButton)
	{
		CraftButton->OnClicked().RemoveAll(this);
		CraftButton->OnClicked().AddUObject(this, &UMOBuildingDetailPanel::HandleBuildButtonClicked);
	}

	if (CraftMaxButton)
	{
		CraftMaxButton->OnClicked().RemoveAll(this);
		CraftMaxButton->OnClicked().AddUObject(this, &UMOBuildingDetailPanel::HandleBuildMaxButtonClicked);
	}

	if (CraftAmountSpinBox)
	{
		CraftAmountSpinBox->OnValueChanged.RemoveDynamic(this, &UMOBuildingDetailPanel::HandleBuildAmountChanged);
		CraftAmountSpinBox->OnValueChanged.AddDynamic(this, &UMOBuildingDetailPanel::HandleBuildAmountChanged);
		// Set minimum to 1
		CraftAmountSpinBox->SetMinValue(1.0f);
		CraftAmountSpinBox->SetMinSliderValue(1.0f);
	}

	if (CraftAmountSlider)
	{
		CraftAmountSlider->OnValueChanged.RemoveDynamic(this, &UMOBuildingDetailPanel::HandleBuildAmountChanged);
		CraftAmountSlider->OnValueChanged.AddDynamic(this, &UMOBuildingDetailPanel::HandleBuildAmountChanged);
		CraftAmountSlider->SetMinValue(1.0f);
	}
}

void UMOBuildingDetailPanel::HandleBuildButtonClicked()
{
	if (!DisplayedRecipeId.IsNone() && CanBuildCurrentRecipe())
	{
		OnBuildRequested.Broadcast(DisplayedRecipeId, BuildAmount);
	}
}

void UMOBuildingDetailPanel::HandleBuildMaxButtonClicked()
{
	int32 MaxAmount = GetMaxBuildableAmount();
	if (MaxAmount > 0)
	{
		SetBuildAmount(MaxAmount);
	}
}

void UMOBuildingDetailPanel::HandleBuildAmountChanged(float Value)
{
	SetBuildAmount(FMath::RoundToInt(Value));
}

void UMOBuildingDetailPanel::UpdateBuildButtonState()
{
	if (CraftButton)
	{
		CraftButton->SetIsEnabled(CanBuildCurrentRecipe());
	}
}

FMOBuildPartDisplayData UMOBuildingDetailPanel::BuildPartData(const FMOBuildPart& Part) const
{
	FMOBuildPartDisplayData Data;
	Data.ItemDefinitionId = Part.ItemDefinitionId;
	Data.ActionId = Part.ActionId;
	Data.RequiredQuantity = Part.Quantity;
	Data.bIsAction = !Part.ActionId.IsNone();

	if (Data.bIsAction)
	{
		// Action-based part
		Data.DisplayName = FText::FromName(Part.ActionId);
		Data.bHasEnough = true; // Actions are always "available"
		Data.AvailableQuantity = Part.Quantity;
	}
	else
	{
		// Item-based part
		// Get available count from inventory
		if (UMOInventoryComponent* Inventory = InventoryComponent.Get())
		{
			Data.AvailableQuantity = Inventory->GetItemCountByDefinitionId(Part.ItemDefinitionId);
		}

		Data.bHasEnough = Data.AvailableQuantity >= Data.RequiredQuantity;

		// Get item definition for display info
		FMOItemDefinitionRow ItemDef;
		if (UMOItemDatabaseSettings::GetItemDefinition(Part.ItemDefinitionId, ItemDef))
		{
			Data.DisplayName = ItemDef.DisplayName;
			Data.Icon = ItemDef.UI.IconSmall;
		}
		else
		{
			Data.DisplayName = FText::FromName(Part.ItemDefinitionId);
		}
	}

	return Data;
}

FMOBuildOutputDisplayData UMOBuildingDetailPanel::BuildOutputData(const FMORecipeDefinitionRow* Recipe) const
{
	FMOBuildOutputDisplayData Data;

	if (Recipe)
	{
		Data.DisplayName = Recipe->DisplayName;
		Data.Quantity = 1;
		Data.Icon = Recipe->Icon;
	}

	return Data;
}

void UMOBuildingDetailPanel::PopulateIngredientsContainer()
{
	// Clear existing widgets
	for (UTextBlock* TextWidget : IngredientTextWidgets)
	{
		if (TextWidget)
		{
			TextWidget->RemoveFromParent();
		}
	}
	IngredientTextWidgets.Empty();

	if (!IngredientsContainer)
	{
		return;
	}

	// Create text widget for each build part
	for (const FMOBuildPartDisplayData& Part : CachedBuildParts)
	{
		FText DisplayText;

		if (Part.bIsAction)
		{
			// Action: "Dig x1"
			DisplayText = FText::Format(
				NSLOCTEXT("MOBuilding", "ActionFormat", "{0} x{1}"),
				Part.DisplayName,
				FText::AsNumber(Part.RequiredQuantity)
			);
		}
		else
		{
			// Item: "Stone (have/need)"
			DisplayText = FText::Format(
				NSLOCTEXT("MOBuilding", "PartFormat", "{0} ({1}/{2})"),
				Part.DisplayName,
				FText::AsNumber(Part.AvailableQuantity),
				FText::AsNumber(Part.RequiredQuantity)
			);
		}

		UTextBlock* TextWidget = CreateSimpleTextWidget(DisplayText, Part.bHasEnough);
		if (TextWidget)
		{
			IngredientsContainer->AddChild(TextWidget);
			IngredientTextWidgets.Add(TextWidget);
		}
	}
}

void UMOBuildingDetailPanel::PopulateOutputsContainer()
{
	// Clear existing widgets
	for (UTextBlock* TextWidget : OutputTextWidgets)
	{
		if (TextWidget)
		{
			TextWidget->RemoveFromParent();
		}
	}
	OutputTextWidgets.Empty();

	if (!OutputsContainer)
	{
		return;
	}

	// Create text widget for each output
	for (const FMOBuildOutputDisplayData& Output : CachedOutputs)
	{
		FText DisplayText;
		if (Output.Quantity > 1)
		{
			DisplayText = FText::Format(
				NSLOCTEXT("MOBuilding", "OutputFormatQty", "{0} x{1}"),
				Output.DisplayName,
				FText::AsNumber(Output.Quantity)
			);
		}
		else
		{
			DisplayText = Output.DisplayName;
		}

		UTextBlock* TextWidget = CreateSimpleTextWidget(DisplayText, true);
		if (TextWidget)
		{
			OutputsContainer->AddChild(TextWidget);
			OutputTextWidgets.Add(TextWidget);
		}
	}
}

UTextBlock* UMOBuildingDetailPanel::CreateSimpleTextWidget(const FText& Text, bool bHasEnough)
{
	UTextBlock* TextWidget = NewObject<UTextBlock>(this);
	if (!TextWidget)
	{
		return nullptr;
	}

	TextWidget->SetText(Text);
	TextWidget->SetAutoWrapText(true);

	// Set font size to 12pt
	FSlateFontInfo FontInfo = TextWidget->GetFont();
	FontInfo.Size = 12;
	TextWidget->SetFont(FontInfo);

	// Set color based on whether we have enough
	if (bHasEnough)
	{
		TextWidget->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	else
	{
		TextWidget->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f, 1.0f))); // Red-ish
	}

	return TextWidget;
}
