#include "MORecipeEntryWidget.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"

UMORecipeEntryWidget::UMORecipeEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMORecipeEntryWidget::SetupEntry(const FMORecipeListEntryData& InData)
{
	EntryData = InData;
	UpdateVisuals();
}

void UMORecipeEntryWidget::SetSelected(bool bInSelected)
{
	if (EntryData.bIsSelected != bInSelected)
	{
		EntryData.bIsSelected = bInSelected;
		UpdateVisuals();
	}
}

void UMORecipeEntryWidget::SetCanCraft(bool bInCanCraft)
{
	if (EntryData.bCanCraft != bInCanCraft)
	{
		EntryData.bCanCraft = bInCanCraft;
		UpdateVisuals();
	}
}

void UMORecipeEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (EntryButton)
	{
		EntryButton->OnClicked().AddUObject(this, &UMORecipeEntryWidget::HandleButtonClicked);
	}
}

void UMORecipeEntryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Apply default visuals in editor preview
	UpdateVisuals();
}

void UMORecipeEntryWidget::UpdateVisuals()
{
	// Update name text
	if (RecipeNameText)
	{
		RecipeNameText->SetText(EntryData.DisplayName);

		// Set text color based on craftability
		FSlateColor TextColor = EntryData.bCanCraft ? TextColorCraftable : TextColorUncraftable;
		RecipeNameText->SetColorAndOpacity(TextColor);
	}

	// Update icon
	if (RecipeIcon && !EntryData.Icon.IsNull())
	{
		UTexture2D* IconTexture = EntryData.Icon.LoadSynchronous();
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
	else if (RecipeIcon)
	{
		RecipeIcon->SetVisibility(ESlateVisibility::Hidden);
	}

	// Update background color
	if (BackgroundBorder)
	{
		FLinearColor BackgroundColor;
		if (EntryData.bIsSelected)
		{
			BackgroundColor = SelectedColor;
		}
		else if (EntryData.bCanCraft)
		{
			BackgroundColor = CraftableColor;
		}
		else
		{
			BackgroundColor = UncraftableColor;
		}
		BackgroundBorder->SetBrushColor(BackgroundColor);
	}

	// Notify Blueprint
	OnVisualsUpdated(EntryData);
}

void UMORecipeEntryWidget::HandleButtonClicked()
{
	OnEntryClicked.Broadcast(EntryData.RecipeId);
}
