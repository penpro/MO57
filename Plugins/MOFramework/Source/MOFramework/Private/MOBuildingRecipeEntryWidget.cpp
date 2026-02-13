#include "MOBuildingRecipeEntryWidget.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
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
	UpdateVisuals();
}

void UMOBuildingRecipeEntryWidget::SetSelected(bool bInSelected)
{
	if (EntryData.bIsSelected != bInSelected)
	{
		EntryData.bIsSelected = bInSelected;
		UpdateVisuals();
	}
}

void UMOBuildingRecipeEntryWidget::SetCanBuild(bool bInCanBuild)
{
	if (EntryData.bCanBuild != bInCanBuild)
	{
		EntryData.bCanBuild = bInCanBuild;
		UpdateVisuals();
	}
}

void UMOBuildingRecipeEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingRecipeEntryWidget] NativeConstruct for recipe: %s, EntryButton=%s"),
		*EntryData.RecipeId.ToString(),
		EntryButton ? TEXT("BOUND") : TEXT("NULL - widget not bound in Blueprint!"));

	if (EntryButton)
	{
		// Remove first to avoid duplicate bindings when widget is re-added to viewport
		EntryButton->OnClicked().RemoveAll(this);
		EntryButton->OnClicked().AddUObject(this, &UMOBuildingRecipeEntryWidget::HandleButtonClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOBuildingRecipeEntryWidget] Click handler bound for recipe: %s"), *EntryData.RecipeId.ToString());
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildingRecipeEntryWidget] No EntryButton bound! Clicks will not work. Check that the Blueprint has a UMOCommonButton named 'EntryButton'."));
	}
}

void UMOBuildingRecipeEntryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Apply default visuals in editor preview
	UpdateVisuals();
}

void UMOBuildingRecipeEntryWidget::UpdateVisuals()
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

	// Update background color
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
	OnEntryClicked.Broadcast(EntryData.RecipeId);
}
