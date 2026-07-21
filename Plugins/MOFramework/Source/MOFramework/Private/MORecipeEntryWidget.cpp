/**
 * MORecipeEntryWidget.cpp - Recipe List Entry Implementation
 */

#include "MORecipeEntryWidget.h"
#include "MOFramework.h"
#include "MOUIInteractionState.h"
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

	// A known recipe remains selectable even when it cannot be crafted now.
	// Selection opens the details that explain missing requirements; action
	// availability belongs to the detail action, not the row button.
	const FMOInspectableEntryState InteractionState =
		MOUIInteractionState::MakeInspectableEntry(InData.bCanCraft);
	SetEntryId(InData.RecipeId);
	UMOListEntryBase::SetSelected(InData.bIsSelected);
	SetEntryEnabled(InteractionState.bSelectable);

	UpdateVisuals();
}

void UMORecipeEntryWidget::SetSelected(bool bInSelected)
{
	if (EntryData.bIsSelected != bInSelected)
	{
		EntryData.bIsSelected = bInSelected;
		Super::SetSelected(bInSelected);
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

void UMORecipeEntryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Apply default visuals in editor preview
	UpdateVisuals();
}

void UMORecipeEntryWidget::UpdateVisuals_Implementation()
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

	// Update background color (3-state: selected, craftable, uncraftable)
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
	if (!IsEntryEnabled())
	{
		return;
	}

	// Call base to broadcast generic OnEntrySelected
	Super::HandleButtonClicked();

	// Broadcast recipe-specific delegates
	OnRecipeSelected.Broadcast(EntryData.RecipeId);

	// Broadcast legacy delegate for backward compatibility
	OnEntryClicked.Broadcast(EntryData.RecipeId);
}
