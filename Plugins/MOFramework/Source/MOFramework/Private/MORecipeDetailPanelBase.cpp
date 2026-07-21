#include "MORecipeDetailPanelBase.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOUIUtils.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SpinBox.h"
#include "Components/Slider.h"

UMORecipeDetailPanelBase::UMORecipeDetailPanelBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMORecipeDetailPanelBase::InitializePanel(
	UMOInventoryComponent* InInventory,
	UMOSkillsComponent* InSkills,
	UMORecipeDiscoveryComponent* InDiscovery)
{
	InventoryComponent = InInventory;
	SkillsComponent = InSkills;
	DiscoveryComponent = InDiscovery;
}

void UMORecipeDetailPanelBase::DisplayRecipe(FName RecipeId)
{
	DisplayedRecipeId = RecipeId;
	ActionAmount = 1;

	if (RecipeId.IsNone())
	{
		ClearDisplay();
		return;
	}

	const FMORecipeDefinitionRow* Recipe = GetDisplayedRecipe();
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
	if (RecipeIcon)
	{
		UTexture2D* IconTexture = UMOUIUtils::LoadRecipeIcon(Recipe);
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

			FText SkillText = UMOUIUtils::FormatSkillRequirement(
				Recipe->RequiredSkillId,
				CurrentLevel,
				Recipe->RequiredSkillLevel
			);
			SkillRequirementText->SetText(SkillText);
			SkillRequirementText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			SkillRequirementText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update time text
	if (CraftTimeText)
	{
		float TimeValue = GetRecipeTime(Recipe);
		FText TimeText = UMOUIUtils::FormatDurationAsText(TimeValue);
		CraftTimeText->SetText(TimeText);
		CraftTimeText->SetVisibility(ESlateVisibility::Visible);
	}

	// Let subclass handle specific display logic
	OnDisplayRecipe(Recipe);

	// Populate containers
	PopulateRequirementsContainer();
	PopulateOutputsContainer();

	// Update amount controls
	int32 MaxAmount = FMath::Max(1, GetMaxPerformableAmount());

	if (CraftAmountSpinBox)
	{
		CraftAmountSpinBox->SetMaxValue(static_cast<float>(MaxAmount));
		CraftAmountSpinBox->SetMaxSliderValue(static_cast<float>(MaxAmount));
		CraftAmountSpinBox->SetValue(ActionAmount);
	}
	if (CraftAmountSlider)
	{
		CraftAmountSlider->SetMaxValue(static_cast<float>(MaxAmount));
		CraftAmountSlider->SetValue(static_cast<float>(ActionAmount));
	}
	if (CraftAmountText)
	{
		CraftAmountText->SetText(FText::AsNumber(ActionAmount));
	}

	UpdateActionButtonState();
}

void UMORecipeDetailPanelBase::ClearDisplay()
{
	DisplayedRecipeId = NAME_None;

	// Clear text widgets
	ClearTextWidgets(IngredientTextWidgets);
	ClearTextWidgets(OutputTextWidgets);

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
	if (ActionUnavailableText)
	{
		ActionUnavailableText->SetText(FText::GetEmpty());
		ActionUnavailableText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMORecipeDetailPanelBase::RefreshDisplay()
{
	if (!DisplayedRecipeId.IsNone())
	{
		// Repopulate requirements with updated counts
		PopulateRequirementsContainer();
		UpdateActionButtonState();
	}
}

void UMORecipeDetailPanelBase::SetActionAmount(int32 Amount)
{
	int32 MaxAmount = FMath::Max(1, GetMaxPerformableAmount());
	ActionAmount = FMath::Clamp(Amount, 1, MaxAmount);

	if (CraftAmountSpinBox)
	{
		CraftAmountSpinBox->SetValue(ActionAmount);
	}
	if (CraftAmountSlider)
	{
		CraftAmountSlider->SetValue(static_cast<float>(ActionAmount));
	}
	if (CraftAmountText)
	{
		CraftAmountText->SetText(FText::AsNumber(ActionAmount));
	}

	UpdateActionButtonState();
}

void UMORecipeDetailPanelBase::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind action button
	if (CraftButton)
	{
		CraftButton->OnClicked().RemoveAll(this);
		CraftButton->OnClicked().AddUObject(this, &UMORecipeDetailPanelBase::HandleActionButtonClicked);
	}

	// Bind max button
	if (CraftMaxButton)
	{
		CraftMaxButton->OnClicked().RemoveAll(this);
		CraftMaxButton->OnClicked().AddUObject(this, &UMORecipeDetailPanelBase::HandleMaxButtonClicked);
	}

	// Bind spin box
	if (CraftAmountSpinBox)
	{
		CraftAmountSpinBox->OnValueChanged.RemoveDynamic(this, &UMORecipeDetailPanelBase::HandleAmountChanged);
		CraftAmountSpinBox->OnValueChanged.AddDynamic(this, &UMORecipeDetailPanelBase::HandleAmountChanged);
		CraftAmountSpinBox->SetMinValue(1.0f);
		CraftAmountSpinBox->SetMinSliderValue(1.0f);
	}

	// Bind slider
	if (CraftAmountSlider)
	{
		CraftAmountSlider->OnValueChanged.RemoveDynamic(this, &UMORecipeDetailPanelBase::HandleAmountChanged);
		CraftAmountSlider->OnValueChanged.AddDynamic(this, &UMORecipeDetailPanelBase::HandleAmountChanged);
		CraftAmountSlider->SetMinValue(1.0f);
	}
}

const FMORecipeDefinitionRow* UMORecipeDetailPanelBase::GetDisplayedRecipe() const
{
	if (DisplayedRecipeId.IsNone())
	{
		return nullptr;
	}
	return UMORecipeDatabaseSettings::GetRecipeDefinition(DisplayedRecipeId);
}

void UMORecipeDetailPanelBase::OnDisplayRecipe(const FMORecipeDefinitionRow* Recipe)
{
	// Base implementation does nothing - subclasses override
}

void UMORecipeDetailPanelBase::PopulateRequirementsContainer()
{
	// Base implementation clears - subclasses populate
	ClearTextWidgets(IngredientTextWidgets);
}

void UMORecipeDetailPanelBase::PopulateOutputsContainer()
{
	// Base implementation clears - subclasses populate
	ClearTextWidgets(OutputTextWidgets);
}

void UMORecipeDetailPanelBase::HandleActionButtonClicked()
{
	// Base implementation - subclasses override
}

void UMORecipeDetailPanelBase::HandleMaxButtonClicked()
{
	int32 MaxAmount = GetMaxPerformableAmount();
	if (MaxAmount > 0)
	{
		SetActionAmount(MaxAmount);
	}
}

void UMORecipeDetailPanelBase::HandleAmountChanged(float Value)
{
	SetActionAmount(FMath::RoundToInt(Value));
}

void UMORecipeDetailPanelBase::UpdateActionButtonState()
{
	const bool bCanPerform = CanPerformAction();
	if (CraftButton)
	{
		CraftButton->SetIsEnabled(bCanPerform);
	}

	if (ActionUnavailableText)
	{
		const FText Reason = bCanPerform ? FText::GetEmpty() : GetActionUnavailableReason();
		ActionUnavailableText->SetText(Reason);
		ActionUnavailableText->SetVisibility(
			Reason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

FText UMORecipeDetailPanelBase::GetActionUnavailableReason() const
{
	return DisplayedRecipeId.IsNone()
		? NSLOCTEXT("MORecipeUI", "NoRecipeSelected", "Select a recipe to see its requirements.")
		: FText::GetEmpty();
}

float UMORecipeDetailPanelBase::GetRecipeTime(const FMORecipeDefinitionRow* Recipe) const
{
	if (!Recipe)
	{
		return 0.0f;
	}

	// Use TotalBuildTime for buildings, CraftTime for regular recipes
	if (Recipe->bIsBuilding && Recipe->TotalBuildTime > 0.0f)
	{
		return Recipe->TotalBuildTime;
	}
	return Recipe->CraftTime;
}

void UMORecipeDetailPanelBase::ClearTextWidgets(TArray<TObjectPtr<UTextBlock>>& WidgetArray)
{
	for (UTextBlock* TextWidget : WidgetArray)
	{
		if (TextWidget)
		{
			TextWidget->RemoveFromParent();
		}
	}
	WidgetArray.Empty();
}

void UMORecipeDetailPanelBase::AddTextWidget(UPanelWidget* Container, TArray<TObjectPtr<UTextBlock>>& WidgetArray, const FText& Text, bool bHasEnough)
{
	if (!Container)
	{
		return;
	}

	UTextBlock* TextWidget = UMOUIUtils::CreateSimpleTextWidget(this, Text, bHasEnough, 12);
	if (TextWidget)
	{
		Container->AddChild(TextWidget);
		WidgetArray.Add(TextWidget);
	}
}
