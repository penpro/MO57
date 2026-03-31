/**
 * MOBuildingRecipeEntryWidget.cpp - Building Recipe Entry Implementation
 */

#include "MOBuildingRecipeEntryWidget.h"
#include "MOFramework.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"

UMOBuildingRecipeEntryWidget::UMOBuildingRecipeEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOBuildingRecipeEntryWidget::SetupEntry(const FMOBuildRecipeListEntryData& InData)
{
	EntryData = InData;

	// Sync base class state
	UMOListEntryBase::SetSelected(InData.bIsSelected);
	SetEntryEnabled(InData.bCanBuild);

	UpdateVisuals();
}

void UMOBuildingRecipeEntryWidget::SetSelected(bool bInSelected)
{
	if (EntryData.bIsSelected != bInSelected)
	{
		EntryData.bIsSelected = bInSelected;
		Super::SetSelected(bInSelected);
	}
}

void UMOBuildingRecipeEntryWidget::SetCanBuild(bool bInCanBuild)
{
	if (EntryData.bCanBuild != bInCanBuild)
	{
		EntryData.bCanBuild = bInCanBuild;
		SetEntryEnabled(bInCanBuild);
	}
}

void UMOBuildingRecipeEntryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Apply default visuals in editor preview
	UpdateVisuals();
}

void UMOBuildingRecipeEntryWidget::UpdateVisuals_Implementation()
{
	// Update name text
	if (RecipeNameText)
	{
		RecipeNameText->SetText(EntryData.DisplayName);

		// Set text color based on buildability
		FSlateColor TextColor = EntryData.bCanBuild ? TextColorBuildable : TextColorUnbuildable;
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

	// Update background color (3-state: selected, buildable, unbuildable)
	if (BackgroundBorder)
	{
		FLinearColor BackgroundColor;
		if (EntryData.bIsSelected)
		{
			BackgroundColor = SelectedColor;
		}
		else if (EntryData.bCanBuild)
		{
			BackgroundColor = BuildableColor;
		}
		else
		{
			BackgroundColor = UnbuildableColor;
		}
		BackgroundBorder->SetBrushColor(BackgroundColor);
	}

	// Notify Blueprint
	OnVisualsUpdated(EntryData);
}

void UMOBuildingRecipeEntryWidget::HandleButtonClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingRecipeEntryWidget] HandleButtonClicked - Recipe: %s"), *EntryData.RecipeId.ToString());

	// Call base to broadcast generic OnEntrySelected
	Super::HandleButtonClicked();

	// Broadcast building-specific delegate
	OnEntryClicked.Broadcast(EntryData.RecipeId);
}
