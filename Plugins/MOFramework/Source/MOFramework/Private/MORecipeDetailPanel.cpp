#include "MORecipeDetailPanel.h"
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

UMORecipeDetailPanel::UMORecipeDetailPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMORecipeDetailPanel::InitializePanel(
	UMOInventoryComponent* InInventory,
	UMOSkillsComponent* InSkills,
	UMORecipeDiscoveryComponent* InDiscovery)
{
	InventoryComponent = InInventory;
	SkillsComponent = InSkills;
	DiscoveryComponent = InDiscovery;
}

void UMORecipeDetailPanel::DisplayRecipe(FName RecipeId)
{
	DisplayedRecipeId = RecipeId;
	CraftAmount = 1;

	if (RecipeId.IsNone())
	{
		ClearDisplay();
		return;
	}

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		ClearDisplay();
		return;
	}

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
	if (RecipeIcon && !Recipe->Icon.IsNull())
	{
		UTexture2D* IconTexture = Recipe->Icon.LoadSynchronous();
		if (IconTexture)
		{
			RecipeIcon->SetBrushFromTexture(IconTexture);
			RecipeIcon->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else if (RecipeIcon)
	{
		RecipeIcon->SetVisibility(ESlateVisibility::Hidden);
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
				NSLOCTEXT("MOCrafting", "SkillReq", "{0}: {1}/{2}"),
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

	// Update craft time text
	if (CraftTimeText)
	{
		if (Recipe->CraftTime > 0.0f)
		{
			FText TimeText;
			if (Recipe->CraftTime >= 3600.0f)
			{
				float Hours = Recipe->CraftTime / 3600.0f;
				TimeText = FText::Format(NSLOCTEXT("MOCrafting", "TimeHours", "{0}h"), FText::AsNumber(FMath::RoundToInt(Hours * 10) / 10.0f));
			}
			else if (Recipe->CraftTime >= 60.0f)
			{
				float Minutes = Recipe->CraftTime / 60.0f;
				TimeText = FText::Format(NSLOCTEXT("MOCrafting", "TimeMinutes", "{0}m"), FText::AsNumber(FMath::RoundToInt(Minutes)));
			}
			else
			{
				TimeText = FText::Format(NSLOCTEXT("MOCrafting", "TimeSeconds", "{0}s"), FText::AsNumber(FMath::RoundToInt(Recipe->CraftTime)));
			}
			CraftTimeText->SetText(TimeText);
			CraftTimeText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			CraftTimeText->SetText(NSLOCTEXT("MOCrafting", "Instant", "Instant"));
			CraftTimeText->SetVisibility(ESlateVisibility::Visible);
		}
	}

	// Build ingredient data
	CachedIngredients.Empty();
	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		CachedIngredients.Add(BuildIngredientData(Ingredient));
	}

	// Build output data
	CachedOutputs.Empty();
	for (const FMORecipeOutput& Output : Recipe->Outputs)
	{
		CachedOutputs.Add(BuildOutputData(Output));
	}

	// Populate containers with text widgets
	PopulateIngredientsContainer();
	PopulateOutputsContainer();

	// Update craft amount controls
	if (CraftAmountSpinBox)
	{
		CraftAmountSpinBox->SetValue(CraftAmount);
	}
	if (CraftAmountSlider)
	{
		int32 MaxAmount = GetMaxCraftableAmount();
		CraftAmountSlider->SetMaxValue(FMath::Max(1.0f, static_cast<float>(MaxAmount)));
		CraftAmountSlider->SetValue(static_cast<float>(CraftAmount));
	}
	if (CraftAmountText)
	{
		CraftAmountText->SetText(FText::AsNumber(CraftAmount));
	}

	UpdateCraftButtonState();

	// Notify Blueprint
	OnRecipeDisplayed(RecipeId, Recipe->DisplayName, Recipe->Description);
	OnIngredientsUpdated(CachedIngredients);
	OnOutputsUpdated(CachedOutputs);
}

void UMORecipeDetailPanel::ClearDisplay()
{
	DisplayedRecipeId = NAME_None;
	CachedIngredients.Empty();
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

void UMORecipeDetailPanel::RefreshDisplay()
{
	if (!DisplayedRecipeId.IsNone())
	{
		// Rebuild ingredient data with updated counts
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(DisplayedRecipeId);
		if (Recipe)
		{
			CachedIngredients.Empty();
			for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
			{
				CachedIngredients.Add(BuildIngredientData(Ingredient));
			}

			// Repopulate the container
			PopulateIngredientsContainer();

			OnIngredientsUpdated(CachedIngredients);
		}

		UpdateCraftButtonState();
	}
}

void UMORecipeDetailPanel::SetCraftAmount(int32 Amount)
{
	CraftAmount = FMath::Max(1, Amount);

	if (CraftAmountSpinBox)
	{
		CraftAmountSpinBox->SetValue(CraftAmount);
	}
	if (CraftAmountSlider)
	{
		CraftAmountSlider->SetValue(static_cast<float>(CraftAmount));
	}
	if (CraftAmountText)
	{
		CraftAmountText->SetText(FText::AsNumber(CraftAmount));
	}

	UpdateCraftButtonState();
}

int32 UMORecipeDetailPanel::GetMaxCraftableAmount() const
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

	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		int32 Available = Inventory->GetItemCountByDefinitionId(Ingredient.ItemDefinitionId);
		int32 CanMake = Available / Ingredient.Quantity;
		MaxAmount = FMath::Min(MaxAmount, CanMake);
	}

	return MaxAmount == INT_MAX ? 0 : MaxAmount;
}

void UMORecipeDetailPanel::GetIngredients(TArray<FMOIngredientDisplayData>& OutIngredients) const
{
	OutIngredients = CachedIngredients;
}

void UMORecipeDetailPanel::GetOutputs(TArray<FMOOutputDisplayData>& OutOutputs) const
{
	OutOutputs = CachedOutputs;
}

bool UMORecipeDetailPanel::CanCraftCurrentRecipe() const
{
	if (DisplayedRecipeId.IsNone())
	{
		return false;
	}

	// Check all ingredients for requested amount
	for (const FMOIngredientDisplayData& Ingredient : CachedIngredients)
	{
		int32 TotalRequired = Ingredient.RequiredQuantity * CraftAmount;
		if (Ingredient.AvailableQuantity < TotalRequired)
		{
			return false;
		}
	}

	return true;
}

void UMORecipeDetailPanel::NativeConstruct()
{
	Super::NativeConstruct();

	// Remove first to avoid duplicate bindings when widget is re-added to viewport
	if (CraftButton)
	{
		CraftButton->OnClicked().RemoveAll(this);
		CraftButton->OnClicked().AddUObject(this, &UMORecipeDetailPanel::HandleCraftButtonClicked);
	}

	if (CraftAmountSpinBox)
	{
		CraftAmountSpinBox->OnValueChanged.RemoveDynamic(this, &UMORecipeDetailPanel::HandleCraftAmountChanged);
		CraftAmountSpinBox->OnValueChanged.AddDynamic(this, &UMORecipeDetailPanel::HandleCraftAmountChanged);
	}

	if (CraftAmountSlider)
	{
		CraftAmountSlider->OnValueChanged.RemoveDynamic(this, &UMORecipeDetailPanel::HandleCraftAmountChanged);
		CraftAmountSlider->OnValueChanged.AddDynamic(this, &UMORecipeDetailPanel::HandleCraftAmountChanged);
	}
}

void UMORecipeDetailPanel::HandleCraftButtonClicked()
{
	if (!DisplayedRecipeId.IsNone() && CanCraftCurrentRecipe())
	{
		OnCraftRequested.Broadcast(DisplayedRecipeId, CraftAmount);
	}
}

void UMORecipeDetailPanel::HandleCraftAmountChanged(float Value)
{
	SetCraftAmount(FMath::RoundToInt(Value));
}

void UMORecipeDetailPanel::UpdateCraftButtonState()
{
	if (CraftButton)
	{
		CraftButton->SetIsEnabled(CanCraftCurrentRecipe());
	}
}

FMOIngredientDisplayData UMORecipeDetailPanel::BuildIngredientData(const FMORecipeIngredient& Ingredient) const
{
	FMOIngredientDisplayData Data;
	Data.ItemDefinitionId = Ingredient.ItemDefinitionId;
	Data.RequiredQuantity = Ingredient.Quantity;

	// Get available count from inventory
	if (UMOInventoryComponent* Inventory = InventoryComponent.Get())
	{
		Data.AvailableQuantity = Inventory->GetItemCountByDefinitionId(Ingredient.ItemDefinitionId);
	}

	Data.bHasEnough = Data.AvailableQuantity >= Data.RequiredQuantity;

	// Get item definition for display info
	FMOItemDefinitionRow ItemDef;
	if (UMOItemDatabaseSettings::GetItemDefinition(Ingredient.ItemDefinitionId, ItemDef))
	{
		Data.DisplayName = ItemDef.DisplayName;
		Data.Icon = ItemDef.UI.IconSmall;
	}
	else
	{
		Data.DisplayName = FText::FromName(Ingredient.ItemDefinitionId);
	}

	return Data;
}

FMOOutputDisplayData UMORecipeDetailPanel::BuildOutputData(const FMORecipeOutput& Output) const
{
	FMOOutputDisplayData Data;
	Data.ItemDefinitionId = Output.ItemDefinitionId;
	Data.Quantity = Output.Quantity;
	Data.Chance = Output.Chance;

	// Get item definition for display info
	FMOItemDefinitionRow ItemDef;
	if (UMOItemDatabaseSettings::GetItemDefinition(Output.ItemDefinitionId, ItemDef))
	{
		Data.DisplayName = ItemDef.DisplayName;
		Data.Icon = ItemDef.UI.IconSmall;
	}
	else
	{
		Data.DisplayName = FText::FromName(Output.ItemDefinitionId);
	}

	return Data;
}

void UMORecipeDetailPanel::PopulateIngredientsContainer()
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

	// Create text widget for each ingredient
	for (const FMOIngredientDisplayData& Ingredient : CachedIngredients)
	{
		// Format: "Item Name (have/need)"
		FText DisplayText = FText::Format(
			NSLOCTEXT("MOCrafting", "IngredientFormat", "{0} ({1}/{2})"),
			Ingredient.DisplayName,
			FText::AsNumber(Ingredient.AvailableQuantity),
			FText::AsNumber(Ingredient.RequiredQuantity)
		);

		UTextBlock* TextWidget = CreateSimpleTextWidget(DisplayText, Ingredient.bHasEnough);
		if (TextWidget)
		{
			IngredientsContainer->AddChild(TextWidget);
			IngredientTextWidgets.Add(TextWidget);
		}
	}
}

void UMORecipeDetailPanel::PopulateOutputsContainer()
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
	for (const FMOOutputDisplayData& Output : CachedOutputs)
	{
		FText DisplayText;
		if (Output.Chance < 1.0f)
		{
			// Show chance if not 100%
			DisplayText = FText::Format(
				NSLOCTEXT("MOCrafting", "OutputFormatChance", "{0} x{1} ({2}%)"),
				Output.DisplayName,
				FText::AsNumber(Output.Quantity),
				FText::AsNumber(FMath::RoundToInt(Output.Chance * 100))
			);
		}
		else if (Output.Quantity > 1)
		{
			DisplayText = FText::Format(
				NSLOCTEXT("MOCrafting", "OutputFormatQty", "{0} x{1}"),
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

UTextBlock* UMORecipeDetailPanel::CreateSimpleTextWidget(const FText& Text, bool bHasEnough)
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
