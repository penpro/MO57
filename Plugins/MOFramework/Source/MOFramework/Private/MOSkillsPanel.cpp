#include "MOSkillsPanel.h"
#include "MOFramework.h"
#include "MOSkillsComponent.h"
#include "MOSkillDatabaseSettings.h"
#include "MOSkillEntryWidget.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"

UMOSkillsPanel::UMOSkillsPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOSkillsPanel::InitializePanel(UMOSkillsComponent* InSkillsComponent)
{
	// Unbind from previous component
	if (UMOSkillsComponent* OldSkills = SkillsComponent.Get())
	{
		OldSkills->OnSkillLevelUp.RemoveDynamic(this, &UMOSkillsPanel::HandleSkillLevelUp);
		OldSkills->OnExperienceGained.RemoveDynamic(this, &UMOSkillsPanel::HandleExperienceGained);
	}

	SkillsComponent = InSkillsComponent;

	// Bind to new component
	if (InSkillsComponent)
	{
		InSkillsComponent->OnSkillLevelUp.AddDynamic(this, &UMOSkillsPanel::HandleSkillLevelUp);
		InSkillsComponent->OnExperienceGained.AddDynamic(this, &UMOSkillsPanel::HandleExperienceGained);
	}

	RefreshSkillList();
}

void UMOSkillsPanel::RefreshSkillList()
{
	PopulateSkillContainer();

	// Select first skill if none selected
	if (SelectedSkillId.IsNone() && (EntryWidgets.Num() > 0 || SimpleTextWidgets.Num() > 0))
	{
		TArray<FName> AllSkillIds;
		UMOSkillDatabaseSettings::GetAllSkillIds(AllSkillIds);
		if (AllSkillIds.Num() > 0)
		{
			SelectSkill(AllSkillIds[0]);
		}
	}
	else
	{
		UpdateDetailPanel();
	}
}

void UMOSkillsPanel::RefreshProgress()
{
	UMOSkillsComponent* Skills = SkillsComponent.Get();
	if (!Skills)
	{
		return;
	}

	// Update entry widgets
	for (UMOSkillEntryWidget* Entry : EntryWidgets)
	{
		if (!Entry)
		{
			continue;
		}

		FMOSkillProgress Progress;
		if (Skills->GetSkillProgress(Entry->GetSkillId(), Progress))
		{
			Entry->UpdateProgress(Progress.GetLevelProgress(), Progress.CurrentXP, Progress.XPToNextLevel);
		}
	}

	// Update detail panel
	UpdateDetailPanel();
}

void UMOSkillsPanel::SetCategoryFilter(EMOSkillCategory Category)
{
	if (CategoryFilter != Category)
	{
		CategoryFilter = Category;
		RefreshSkillList();
	}
}

void UMOSkillsPanel::ClearCategoryFilter()
{
	SetCategoryFilter(EMOSkillCategory::None);
}

void UMOSkillsPanel::SelectSkill(FName SkillId)
{
	SelectedSkillId = SkillId;
	UpdateDetailPanel();
	OnSkillSelected.Broadcast(SkillId);
}

void UMOSkillsPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
		CloseButton->OnClicked().AddUObject(this, &UMOSkillsPanel::HandleCloseClicked);
	}
}

void UMOSkillsPanel::NativeDestruct()
{
	if (UMOSkillsComponent* Skills = SkillsComponent.Get())
	{
		Skills->OnSkillLevelUp.RemoveDynamic(this, &UMOSkillsPanel::HandleSkillLevelUp);
		Skills->OnExperienceGained.RemoveDynamic(this, &UMOSkillsPanel::HandleExperienceGained);
	}

	Super::NativeDestruct();
}

FReply UMOSkillsPanel::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnRequestClose.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMOSkillsPanel::HandleSkillLevelUp(FName SkillId, int32 OldLevel, int32 NewLevel)
{
	// Refresh the specific skill entry
	for (UMOSkillEntryWidget* Entry : EntryWidgets)
	{
		if (Entry && Entry->GetSkillId() == SkillId)
		{
			Entry->SetupEntry(BuildSkillDisplayData(SkillId));
			break;
		}
	}

	// If the leveled skill is selected, update detail panel
	if (SelectedSkillId == SkillId)
	{
		UpdateDetailPanel();
	}
}

void UMOSkillsPanel::HandleExperienceGained(FName SkillId, float XPGained, float TotalXP)
{
	UMOSkillsComponent* Skills = SkillsComponent.Get();
	if (!Skills)
	{
		return;
	}

	FMOSkillProgress Progress;
	if (!Skills->GetSkillProgress(SkillId, Progress))
	{
		return;
	}

	// Update the specific entry widget
	for (UMOSkillEntryWidget* Entry : EntryWidgets)
	{
		if (Entry && Entry->GetSkillId() == SkillId)
		{
			Entry->UpdateProgress(Progress.GetLevelProgress(), Progress.CurrentXP, Progress.XPToNextLevel);
			break;
		}
	}

	// If selected skill gained XP, update detail panel
	if (SelectedSkillId == SkillId)
	{
		UpdateDetailPanel();
	}
}

void UMOSkillsPanel::HandleSkillEntrySelected(FName SkillId)
{
	SelectSkill(SkillId);
}

void UMOSkillsPanel::HandleCloseClicked()
{
	OnRequestClose.Broadcast();
}

FMOSkillDisplayData UMOSkillsPanel::BuildSkillDisplayData(FName SkillId) const
{
	FMOSkillDisplayData Data;
	Data.SkillId = SkillId;

	// Get skill definition
	const FMOSkillDefinitionRow* SkillDef = UMOSkillDatabaseSettings::GetSkillDefinition(SkillId);
	if (SkillDef)
	{
		Data.DisplayName = SkillDef->DisplayName;
		Data.Description = SkillDef->Description;
		Data.HowToIncrease = SkillDef->HowToIncrease;
		Data.Category = SkillDef->Category;
		Data.MaxLevel = SkillDef->MaxLevel;
		Data.Icon = SkillDef->Icon;
	}
	else
	{
		Data.DisplayName = FText::FromName(SkillId);
	}

	// Get player progress
	if (UMOSkillsComponent* Skills = SkillsComponent.Get())
	{
		FMOSkillProgress Progress;
		if (Skills->GetSkillProgress(SkillId, Progress))
		{
			Data.Level = Progress.Level;
			Data.CurrentXP = Progress.CurrentXP;
			Data.XPToNextLevel = Progress.XPToNextLevel;
			Data.LevelProgress = Progress.GetLevelProgress();
		}
	}

	return Data;
}

void UMOSkillsPanel::PopulateSkillContainer()
{
	// Clear existing widgets
	for (UMOSkillEntryWidget* Entry : EntryWidgets)
	{
		if (Entry)
		{
			Entry->OnSkillSelected.RemoveDynamic(this, &UMOSkillsPanel::HandleSkillEntrySelected);
			Entry->RemoveFromParent();
		}
	}
	EntryWidgets.Empty();

	for (UTextBlock* Text : SimpleTextWidgets)
	{
		if (Text)
		{
			Text->RemoveFromParent();
		}
	}
	SimpleTextWidgets.Empty();

	if (!SkillsContainer)
	{
		return;
	}

	// Get all skill IDs
	TArray<FName> AllSkillIds;
	if (bShowAllSkills)
	{
		UMOSkillDatabaseSettings::GetAllSkillIds(AllSkillIds);
	}
	else if (UMOSkillsComponent* Skills = SkillsComponent.Get())
	{
		Skills->GetAllSkillIds(AllSkillIds);
	}

	// Build display data and filter
	TArray<FMOSkillDisplayData> DisplayList;
	for (const FName& SkillId : AllSkillIds)
	{
		FMOSkillDisplayData Data = BuildSkillDisplayData(SkillId);

		// Filter by category
		if (CategoryFilter != EMOSkillCategory::None && Data.Category != CategoryFilter)
		{
			continue;
		}

		DisplayList.Add(Data);
	}

	// Sort by level if enabled
	if (bSortByLevel)
	{
		DisplayList.Sort([](const FMOSkillDisplayData& A, const FMOSkillDisplayData& B) {
			if (A.Level != B.Level)
			{
				return A.Level > B.Level; // Higher level first
			}
			return A.DisplayName.CompareTo(B.DisplayName) < 0; // Alphabetical as tiebreaker
		});
	}

	// Update empty state visibility
	if (EmptyListText)
	{
		EmptyListText->SetVisibility(DisplayList.Num() == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Create widgets
	for (const FMOSkillDisplayData& Data : DisplayList)
	{
		if (SkillEntryWidgetClass)
		{
			// Use custom widget class
			UMOSkillEntryWidget* Entry = CreateWidget<UMOSkillEntryWidget>(this, SkillEntryWidgetClass);
			if (Entry)
			{
				Entry->SetupEntry(Data);
				Entry->OnSkillSelected.AddDynamic(this, &UMOSkillsPanel::HandleSkillEntrySelected);
				SkillsContainer->AddChild(Entry);
				EntryWidgets.Add(Entry);
			}
		}
		else
		{
			// Use simple text
			UTextBlock* Text = CreateSimpleSkillText(Data);
			if (Text)
			{
				SkillsContainer->AddChild(Text);
				SimpleTextWidgets.Add(Text);
			}
		}
	}

	OnSkillListUpdated(DisplayList.Num());
}

UTextBlock* UMOSkillsPanel::CreateSimpleSkillText(const FMOSkillDisplayData& Data)
{
	UTextBlock* TextWidget = NewObject<UTextBlock>(this);
	if (!TextWidget)
	{
		return nullptr;
	}

	// Format: "Cooking - Lv. 5 (250/500 XP)"
	FText DisplayText = FText::Format(
		NSLOCTEXT("MOSkills", "SimpleSkillFormat", "{0} - Lv. {1} ({2}/{3} XP)"),
		Data.DisplayName,
		FText::AsNumber(Data.Level),
		FText::AsNumber(FMath::RoundToInt(Data.CurrentXP)),
		FText::AsNumber(FMath::RoundToInt(Data.XPToNextLevel))
	);

	TextWidget->SetText(DisplayText);
	TextWidget->SetAutoWrapText(true);

	// Set font size to 12pt
	FSlateFontInfo FontInfo = TextWidget->GetFont();
	FontInfo.Size = 12;
	TextWidget->SetFont(FontInfo);

	TextWidget->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	return TextWidget;
}

void UMOSkillsPanel::UpdateDetailPanel()
{
	if (SelectedSkillId.IsNone())
	{
		// Clear detail panel
		if (DetailNameText) DetailNameText->SetText(FText::GetEmpty());
		if (DetailDescriptionText) DetailDescriptionText->SetText(FText::GetEmpty());
		if (DetailHowToIncreaseText) DetailHowToIncreaseText->SetText(FText::GetEmpty());
		if (DetailLevelText) DetailLevelText->SetText(FText::GetEmpty());
		if (DetailXPText) DetailXPText->SetText(FText::GetEmpty());
		if (DetailXPBar) DetailXPBar->SetPercent(0.0f);
		if (DetailIcon) DetailIcon->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	FMOSkillDisplayData Data = BuildSkillDisplayData(SelectedSkillId);

	if (DetailNameText)
	{
		DetailNameText->SetText(Data.DisplayName);
	}

	if (DetailDescriptionText)
	{
		DetailDescriptionText->SetText(Data.Description);
	}

	if (DetailHowToIncreaseText)
	{
		DetailHowToIncreaseText->SetText(Data.HowToIncrease);
	}

	if (DetailLevelText)
	{
		DetailLevelText->SetText(FText::Format(
			NSLOCTEXT("MOSkills", "DetailLevel", "Level {0} / {1}"),
			FText::AsNumber(Data.Level),
			FText::AsNumber(Data.MaxLevel)
		));
	}

	if (DetailXPText)
	{
		DetailXPText->SetText(FText::Format(
			NSLOCTEXT("MOSkills", "DetailXP", "{0} / {1} XP"),
			FText::AsNumber(FMath::RoundToInt(Data.CurrentXP)),
			FText::AsNumber(FMath::RoundToInt(Data.XPToNextLevel))
		));
	}

	if (DetailXPBar)
	{
		DetailXPBar->SetPercent(Data.LevelProgress);
	}

	if (DetailIcon && !Data.Icon.IsNull())
	{
		UTexture2D* IconTexture = Data.Icon.LoadSynchronous();
		if (IconTexture)
		{
			DetailIcon->SetBrushFromTexture(IconTexture);
			DetailIcon->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else if (DetailIcon)
	{
		DetailIcon->SetVisibility(ESlateVisibility::Hidden);
	}

	OnSelectedSkillChanged(Data);
}
