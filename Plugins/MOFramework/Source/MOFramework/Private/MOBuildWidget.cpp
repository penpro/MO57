#include "MOBuildWidget.h"
#include "MOBuildableActor.h"
#include "MORecipeDatabaseSettings.h"
#include "MOFramework.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CheckBox.h"
#include "MOCommonButton.h"

UMOBuildWidget::UMOBuildWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UMOBuildWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (StartButton)
	{
		StartButton->OnClicked().RemoveAll(this);
		StartButton->OnClicked().AddUObject(this, &UMOBuildWidget::HandleStartClicked);
	}

	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOBuildWidget::HandleCancelClicked);
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
}

FReply UMOBuildWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();

	// Close on Escape
	if (PressedKey == EKeys::Escape)
	{
		OnRequestClose.Broadcast();
		return FReply::Handled();
	}

	// Start build on Enter
	if (PressedKey == EKeys::Enter)
	{
		OnStartBuild.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMOBuildWidget::InitializeForBuilding(AMOBuildableActor* Target)
{
	TargetBuilding = Target;

	if (!IsValid(Target))
	{
		return;
	}

	TargetRecipeId = Target->GetRecipeId();

	// Get recipe definition
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(TargetRecipeId);
	if (!Recipe)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildWidget] Recipe not found: %s"), *TargetRecipeId.ToString());
		return;
	}

	GatherRange = Recipe->BuildRange;

	// Set building name
	if (BuildingNameText)
	{
		BuildingNameText->SetText(Recipe->DisplayName);
	}

	// Set icon
	if (BuildingIcon && !Recipe->Icon.IsNull())
	{
		if (UTexture2D* IconTexture = Recipe->Icon.LoadSynchronous())
		{
			BuildingIcon->SetBrushFromTexture(IconTexture);
		}
	}

	// Set build time
	if (BuildTimeText)
	{
		int32 Minutes = FMath::FloorToInt(Recipe->TotalBuildTime / 60.0f);
		int32 Seconds = FMath::FloorToInt(FMath::Fmod(Recipe->TotalBuildTime, 60.0f));

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
}

FMOBuildProgress UMOBuildWidget::GetBuildOptions() const
{
	FMOBuildProgress Options;

	Options.bDrawFromInventory = InventoryCheckbox ? InventoryCheckbox->IsChecked() : true;
	Options.bDrawFromNearbyContainers = ContainersCheckbox ? ContainersCheckbox->IsChecked() : true;
	Options.bDrawFromSurroundingArea = SurroundingCheckbox ? SurroundingCheckbox->IsChecked() : true;
	Options.GatherRange = GatherRange;

	return Options;
}

void UMOBuildWidget::HandleStartClicked()
{
	OnStartBuild.Broadcast();
}

void UMOBuildWidget::HandleCancelClicked()
{
	OnRequestClose.Broadcast();
}
