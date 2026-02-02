#include "MOBuildingEntryWidget.h"
#include "MORecipeDefinitionRow.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"

void UMOBuildingEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (EntryButton)
	{
		EntryButton->OnClicked.RemoveAll(this);
		EntryButton->OnClicked.AddDynamic(this, &UMOBuildingEntryWidget::HandleButtonClicked);
	}
}

void UMOBuildingEntryWidget::InitializeEntry(FName InRecipeId, const FMORecipeDefinitionRow& Recipe)
{
	RecipeId = InRecipeId;

	// Set name
	if (NameText)
	{
		NameText->SetText(Recipe.DisplayName);
	}

	// Set preview image
	if (PreviewImage && !Recipe.Icon.IsNull())
	{
		if (UTexture2D* IconTexture = Recipe.Icon.LoadSynchronous())
		{
			PreviewImage->SetBrushFromTexture(IconTexture);
		}
	}

	// Set build time
	if (BuildTimeText)
	{
		int32 Minutes = FMath::FloorToInt(Recipe.TotalBuildTime / 60.0f);
		int32 Seconds = FMath::FloorToInt(FMath::Fmod(Recipe.TotalBuildTime, 60.0f));

		FText TimeText;
		if (Minutes > 0)
		{
			TimeText = FText::Format(NSLOCTEXT("MO", "BuildTimeMinSec", "{0}m {1}s"), Minutes, Seconds);
		}
		else
		{
			TimeText = FText::Format(NSLOCTEXT("MO", "BuildTimeSec", "{0}s"), Seconds);
		}
		BuildTimeText->SetText(TimeText);
	}

	// Set materials summary
	if (MaterialsText)
	{
		FString MaterialsSummary;
		for (const FMOBuildPart& Part : Recipe.BuildParts)
		{
			if (Part.IsItemPart())
			{
				if (!MaterialsSummary.IsEmpty())
				{
					MaterialsSummary += TEXT(", ");
				}
				MaterialsSummary += FString::Printf(TEXT("%s x%d"), *Part.ItemDefinitionId.ToString(), Part.Quantity);
			}
		}

		if (MaterialsSummary.IsEmpty())
		{
			MaterialsSummary = TEXT("No materials required");
		}

		MaterialsText->SetText(FText::FromString(MaterialsSummary));
	}
}

void UMOBuildingEntryWidget::HandleButtonClicked()
{
	OnEntryClicked.Broadcast(RecipeId);
}
