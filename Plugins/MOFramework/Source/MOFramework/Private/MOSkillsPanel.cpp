#include "MOSkillsPanel.h"
#include "MOFramework.h"
#include "MOSkillsComponent.h"
#include "MOKnowledgeComponent.h"
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
	// Enable keyboard input for this widget (needed for Tab/Escape to close)
	SetIsFocusable(true);
}

void UMOSkillsPanel::InitializePanel(UMOSkillsComponent* InSkillsComponent)
{
	InitializePanelWithKnowledge(InSkillsComponent, nullptr);
}

void UMOSkillsPanel::InitializePanelWithKnowledge(UMOSkillsComponent* InSkillsComponent, UMOKnowledgeComponent* InKnowledgeComponent)
{
	// Unbind from previous skills component
	if (UMOSkillsComponent* OldSkills = SkillsComponent.Get())
	{
		OldSkills->OnSkillLevelUp.RemoveDynamic(this, &UMOSkillsPanel::HandleSkillLevelUp);
		OldSkills->OnExperienceGained.RemoveDynamic(this, &UMOSkillsPanel::HandleExperienceGained);
	}

	// Unbind from previous knowledge component
	if (UMOKnowledgeComponent* OldKnowledge = KnowledgeComponent.Get())
	{
		OldKnowledge->OnKnowledgeLearned.RemoveDynamic(this, &UMOSkillsPanel::HandleKnowledgeLearned);
	}

	SkillsComponent = InSkillsComponent;
	KnowledgeComponent = InKnowledgeComponent;

	// Bind to new skills component
	if (InSkillsComponent)
	{
		InSkillsComponent->OnSkillLevelUp.AddDynamic(this, &UMOSkillsPanel::HandleSkillLevelUp);
		InSkillsComponent->OnExperienceGained.AddDynamic(this, &UMOSkillsPanel::HandleExperienceGained);
	}

	// Bind to new knowledge component
	if (InKnowledgeComponent)
	{
		InKnowledgeComponent->OnKnowledgeLearned.AddDynamic(this, &UMOSkillsPanel::HandleKnowledgeLearned);
	}

	UpdateTabButtonStates();
	RefreshSkillList();
}

void UMOSkillsPanel::SetDisplayMode(EMOSkillsPanelMode NewMode)
{
	if (CurrentDisplayMode == NewMode)
	{
		return;
	}

	CurrentDisplayMode = NewMode;
	SelectedSkillId = NAME_None;
	SelectedKnowledgeId = NAME_None;

	UpdateTabButtonStates();

	if (CurrentDisplayMode == EMOSkillsPanelMode::Skills)
	{
		PopulateSkillContainer();
	}
	else
	{
		PopulateKnowledgeContainer();
	}

	UpdateDetailPanel();
	OnModeChanged.Broadcast(NewMode);
}

void UMOSkillsPanel::ShowSkills()
{
	SetDisplayMode(EMOSkillsPanelMode::Skills);
}

void UMOSkillsPanel::ShowKnowledge()
{
	SetDisplayMode(EMOSkillsPanelMode::Knowledge);
}

void UMOSkillsPanel::RefreshSkillList()
{
	if (CurrentDisplayMode == EMOSkillsPanelMode::Skills)
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
	else
	{
		PopulateKnowledgeContainer();
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

	if (SkillsTabButton)
	{
		SkillsTabButton->OnClicked().RemoveAll(this);
		SkillsTabButton->OnClicked().AddUObject(this, &UMOSkillsPanel::HandleSkillsTabClicked);
	}

	if (KnowledgeTabButton)
	{
		KnowledgeTabButton->OnClicked().RemoveAll(this);
		KnowledgeTabButton->OnClicked().AddUObject(this, &UMOSkillsPanel::HandleKnowledgeTabClicked);
	}

	UpdateTabButtonStates();
}

void UMOSkillsPanel::NativeDestruct()
{
	if (UMOSkillsComponent* Skills = SkillsComponent.Get())
	{
		Skills->OnSkillLevelUp.RemoveDynamic(this, &UMOSkillsPanel::HandleSkillLevelUp);
		Skills->OnExperienceGained.RemoveDynamic(this, &UMOSkillsPanel::HandleExperienceGained);
	}

	if (UMOKnowledgeComponent* Knowledge = KnowledgeComponent.Get())
	{
		Knowledge->OnKnowledgeLearned.RemoveDynamic(this, &UMOSkillsPanel::HandleKnowledgeLearned);
	}

	Super::NativeDestruct();
}

FReply UMOSkillsPanel::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();

	// Close on Escape or Tab (same key that opens it)
	if (PressedKey == EKeys::Escape || PressedKey == EKeys::Tab)
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

	// Update the specific entry widget and play flash animation
	for (UMOSkillEntryWidget* Entry : EntryWidgets)
	{
		if (Entry && Entry->GetSkillId() == SkillId)
		{
			Entry->UpdateProgress(Progress.GetLevelProgress(), Progress.CurrentXP, Progress.XPToNextLevel);
			Entry->PlayFlashAnimation(2.0f); // Flash for 2 seconds when XP is gained
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
	if (CurrentDisplayMode == EMOSkillsPanelMode::Skills)
	{
		SelectSkill(SkillId);
	}
	else
	{
		// In knowledge mode, track selected knowledge
		SelectedKnowledgeId = SkillId;
		UpdateDetailPanel();
		OnSkillSelected.Broadcast(SkillId); // Reuse the delegate for knowledge selection
	}
}

void UMOSkillsPanel::HandleCloseClicked()
{
	OnRequestClose.Broadcast();
}

void UMOSkillsPanel::HandleSkillsTabClicked()
{
	ShowSkills();

	// Return focus to the panel so Tab key still works
	SetFocus();
}

void UMOSkillsPanel::HandleKnowledgeTabClicked()
{
	ShowKnowledge();

	// Return focus to the panel so Tab key still works
	SetFocus();
}

void UMOSkillsPanel::HandleKnowledgeLearned(FName KnowledgeId, FName FromItemId)
{
	// Refresh if we're showing knowledge
	if (CurrentDisplayMode == EMOSkillsPanelMode::Knowledge)
	{
		PopulateKnowledgeContainer();

		// Play flash animation on the newly added knowledge entry
		for (UMOSkillEntryWidget* Entry : EntryWidgets)
		{
			if (Entry && Entry->GetSkillId() == KnowledgeId)
			{
				Entry->PlayFlashAnimation(2.0f);
				break;
			}
		}
	}
}

void UMOSkillsPanel::UpdateTabButtonStates()
{
	// Disable knowledge tab if no knowledge component is set
	if (KnowledgeTabButton)
	{
		bool bHasKnowledge = KnowledgeComponent.IsValid();
		KnowledgeTabButton->SetIsEnabled(bHasKnowledge);
	}

	// Call Blueprint event to update button visual states
	// Blueprint should style buttons based on the current mode (highlight active tab, etc.)
	OnTabModeChanged(CurrentDisplayMode);
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

FMOSkillDisplayData UMOSkillsPanel::BuildKnowledgeDisplayData(FName KnowledgeId) const
{
	FMOSkillDisplayData Data;
	Data.SkillId = KnowledgeId; // Reuse SkillId field for KnowledgeId
	Data.DisplayName = FormatKnowledgeName(KnowledgeId);
	Data.Description = NSLOCTEXT("MO", "KnowledgeDesc", "Knowledge gained through study and inspection of items.");
	Data.HowToIncrease = NSLOCTEXT("MO", "KnowledgeHowTo", "Inspect items related to this topic to increase your understanding.");
	Data.Category = EMOSkillCategory::Knowledge;

	// Knowledge now uses the same XP/level system as skills
	// Query actual progress from SkillsComponent
	if (UMOSkillsComponent* Skills = SkillsComponent.Get())
	{
		FMOSkillProgress Progress;
		if (Skills->GetSkillProgress(KnowledgeId, Progress))
		{
			Data.Level = Progress.Level;
			Data.CurrentXP = Progress.CurrentXP;
			Data.XPToNextLevel = Progress.XPToNextLevel;
			Data.LevelProgress = Progress.GetLevelProgress();
			Data.MaxLevel = 100; // Default max level for knowledge (could be looked up per-knowledge)
		}
		else
		{
			// Knowledge learned but not yet tracked in skills (shouldn't happen with new system)
			Data.Level = 1;
			Data.MaxLevel = 100;
			Data.CurrentXP = 0.0f;
			Data.XPToNextLevel = 100.0f;
			Data.LevelProgress = 0.0f;
		}
	}
	else
	{
		// No skills component, default values
		Data.Level = 1;
		Data.MaxLevel = 100;
		Data.CurrentXP = 0.0f;
		Data.XPToNextLevel = 100.0f;
		Data.LevelProgress = 0.0f;
	}

	return Data;
}

FText UMOSkillsPanel::FormatKnowledgeName(FName KnowledgeId) const
{
	// Convert camelCase/PascalCase to "Title Case With Spaces"
	FString KnowledgeString = KnowledgeId.ToString();

	if (KnowledgeString.Len() > 0)
	{
		FString FormattedString;
		for (int32 i = 0; i < KnowledgeString.Len(); i++)
		{
			TCHAR Char = KnowledgeString[i];

			// Insert space before capitals (except first char)
			if (i > 0 && FChar::IsUpper(Char))
			{
				FormattedString.AppendChar(TEXT(' '));
			}

			// Capitalize first character
			if (i == 0)
			{
				Char = FChar::ToUpper(Char);
			}

			FormattedString.AppendChar(Char);
		}
		KnowledgeString = FormattedString;
	}

	return FText::FromString(KnowledgeString);
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

void UMOSkillsPanel::PopulateKnowledgeContainer()
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

	UMOKnowledgeComponent* Knowledge = KnowledgeComponent.Get();
	if (!Knowledge)
	{
		if (EmptyListText)
		{
			EmptyListText->SetVisibility(ESlateVisibility::Visible);
			EmptyListText->SetText(NSLOCTEXT("MO", "NoKnowledgeComponent", "Knowledge tracking unavailable"));
		}
		OnSkillListUpdated(0);
		return;
	}

	// Get all learned knowledge
	TArray<FName> LearnedKnowledge;
	Knowledge->GetAllLearnedKnowledge(LearnedKnowledge);

	// Update empty state
	if (EmptyListText)
	{
		if (LearnedKnowledge.Num() == 0)
		{
			EmptyListText->SetVisibility(ESlateVisibility::Visible);
			EmptyListText->SetText(NSLOCTEXT("MO", "NoKnowledge", "No discoveries yet.\nInspect items to learn about the world."));
		}
		else
		{
			EmptyListText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Sort alphabetically by formatted name
	LearnedKnowledge.Sort([this](const FName& A, const FName& B) {
		return FormatKnowledgeName(A).ToString() < FormatKnowledgeName(B).ToString();
	});

	// Build display list
	TArray<FMOSkillDisplayData> DisplayList;
	for (const FName& KnowledgeId : LearnedKnowledge)
	{
		DisplayList.Add(BuildKnowledgeDisplayData(KnowledgeId));
	}

	// Create widgets
	for (const FMOSkillDisplayData& Data : DisplayList)
	{
		if (SkillEntryWidgetClass)
		{
			// Use custom widget class (same as skills)
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
			UTextBlock* Text = CreateSimpleKnowledgeText(Data);
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

UTextBlock* UMOSkillsPanel::CreateSimpleKnowledgeText(const FMOSkillDisplayData& Data)
{
	UTextBlock* TextWidget = NewObject<UTextBlock>(this);
	if (!TextWidget)
	{
		return nullptr;
	}

	// Knowledge just shows the name (no XP/levels)
	TextWidget->SetText(Data.DisplayName);
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
	FName SelectedId = (CurrentDisplayMode == EMOSkillsPanelMode::Skills) ? SelectedSkillId : SelectedKnowledgeId;

	if (SelectedId.IsNone())
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

	FMOSkillDisplayData Data;
	if (CurrentDisplayMode == EMOSkillsPanelMode::Skills)
	{
		Data = BuildSkillDisplayData(SelectedId);
	}
	else
	{
		Data = BuildKnowledgeDisplayData(SelectedId);
	}

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
		// Both skills and knowledge now have levels
		DetailLevelText->SetText(FText::Format(
			NSLOCTEXT("MOSkills", "DetailLevel", "Level {0} / {1}"),
			FText::AsNumber(Data.Level),
			FText::AsNumber(Data.MaxLevel)
		));
		DetailLevelText->SetVisibility(ESlateVisibility::Visible);
	}

	if (DetailXPText)
	{
		// Both skills and knowledge now have XP
		DetailXPText->SetText(FText::Format(
			NSLOCTEXT("MOSkills", "DetailXP", "{0} / {1} XP"),
			FText::AsNumber(FMath::RoundToInt(Data.CurrentXP)),
			FText::AsNumber(FMath::RoundToInt(Data.XPToNextLevel))
		));
		DetailXPText->SetVisibility(ESlateVisibility::Visible);
	}

	if (DetailXPBar)
	{
		// Both skills and knowledge now have progress bars
		DetailXPBar->SetPercent(Data.LevelProgress);
		DetailXPBar->SetVisibility(ESlateVisibility::Visible);
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
