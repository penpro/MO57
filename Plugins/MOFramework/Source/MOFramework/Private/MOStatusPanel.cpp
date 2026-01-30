#include "MOStatusPanel.h"
#include "MOFramework.h"
#include "MOStatusField.h"
#include "MOCommonButton.h"
#include "MOVitalsComponent.h"
#include "MOMetabolismComponent.h"
#include "MOMentalStateComponent.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"

void UMOStatusPanel::NativeConstruct()
{
	Super::NativeConstruct();

	BindTabButtons();

	// Populate field configs from Blueprint or C++
	PopulateFieldConfigs();

	// Create field widgets from configs
	CreateFieldsFromConfigs();

	// Start on vitals tab
	SwitchToCategory(EMOStatusCategory::Vitals);
}

void UMOStatusPanel::NativeDestruct()
{
	UnbindFromMedicalComponents();
	Super::NativeDestruct();
}

UWidget* UMOStatusPanel::NativeGetDesiredFocusTarget() const
{
	// Focus the first tab button
	if (VitalsTabButton)
	{
		return VitalsTabButton;
	}
	return nullptr;
}

FReply UMOStatusPanel::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();

	// Tab or Escape closes the panel
	if (PressedKey == EKeys::Tab || PressedKey == EKeys::Escape)
	{
		OnRequestClose.Broadcast();
		return FReply::Handled();
	}

	// Q/E or Left/Right to cycle categories
	if (PressedKey == EKeys::Q || PressedKey == EKeys::Left)
	{
		int32 NewIndex = static_cast<int32>(CurrentCategory) - 1;
		if (NewIndex < 0)
		{
			NewIndex = static_cast<int32>(EMOStatusCategory::MAX) - 1;
		}
		SwitchToCategory(static_cast<EMOStatusCategory>(NewIndex));
		return FReply::Handled();
	}

	if (PressedKey == EKeys::E || PressedKey == EKeys::Right)
	{
		int32 NewIndex = static_cast<int32>(CurrentCategory) + 1;
		if (NewIndex >= static_cast<int32>(EMOStatusCategory::MAX))
		{
			NewIndex = 0;
		}
		SwitchToCategory(static_cast<EMOStatusCategory>(NewIndex));
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMOStatusPanel::SwitchToCategory(EMOStatusCategory Category)
{
	if (Category == EMOStatusCategory::MAX)
	{
		return;
	}

	CurrentCategory = Category;

	// Update widget switcher if available
	if (CategorySwitcher)
	{
		// Get the actual scroll box widget for this category and set it active
		UScrollBox* TargetScrollBox = nullptr;
		switch (Category)
		{
		case EMOStatusCategory::Vitals:		TargetScrollBox = VitalsScrollBox; break;
		case EMOStatusCategory::Nutrition:	TargetScrollBox = NutritionScrollBox; break;
		case EMOStatusCategory::Nutrients:	TargetScrollBox = NutrientsScrollBox; break;
		case EMOStatusCategory::Fitness:	TargetScrollBox = FitnessScrollBox; break;
		case EMOStatusCategory::Mental:		TargetScrollBox = MentalScrollBox; break;
		case EMOStatusCategory::Wounds:		TargetScrollBox = WoundsScrollBox; break;
		case EMOStatusCategory::Conditions:	TargetScrollBox = ConditionsScrollBox; break;
		default: break;
		}

		if (TargetScrollBox)
		{
			CategorySwitcher->SetActiveWidget(TargetScrollBox);
		}
	}
	else
	{
		// Fallback: manually show/hide scroll boxes if CategorySwitcher isn't bound
		UE_LOG(LogMOFramework, Log, TEXT("[MOStatusPanel] No CategorySwitcher bound - using manual scroll box visibility"));
		UpdateScrollBoxVisibility(Category);
	}

	UpdateTabButtonStates();
	OnCategoryChanged.Broadcast(Category);
	OnCategoryChangedBP(Category);

	UE_LOG(LogMOFramework, Log, TEXT("[MOStatusPanel] Switched to category: %d"), static_cast<int32>(Category));
}

void UMOStatusPanel::UpdateFieldValue(FName FieldId, const FText& Value, float NormalizedValue)
{
	if (UMOStatusField* Field = GetFieldById(FieldId))
	{
		Field->SetValue(Value, NormalizedValue);
	}
}

void UMOStatusPanel::UpdateFieldValueFloat(FName FieldId, float Value, float NormalizedValue)
{
	if (const FMOStatusFieldConfig* Config = FieldConfigMap.Find(FieldId))
	{
		FText FormattedValue = FText::Format(FText::FromString(Config->ValueFormat), FText::AsNumber(Value));
		UpdateFieldValue(FieldId, FormattedValue, NormalizedValue);
	}
	else
	{
		// No config found, just display the raw number
		UpdateFieldValue(FieldId, FText::AsNumber(Value), NormalizedValue);
	}
}

UMOStatusField* UMOStatusPanel::GetFieldById(FName FieldId) const
{
	if (const TObjectPtr<UMOStatusField>* FoundField = FieldWidgets.Find(FieldId))
	{
		return *FoundField;
	}
	return nullptr;
}

void UMOStatusPanel::RefreshAllFields()
{
	// Override in Blueprint to update all fields from data source
	UE_LOG(LogMOFramework, Log, TEXT("[MOStatusPanel] RefreshAllFields - override in Blueprint to bind to data"));
}

UMOStatusField* UMOStatusPanel::AddField(const FMOStatusFieldConfig& Config)
{
	if (!StatusFieldClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOStatusPanel] StatusFieldClass not set - cannot create field"));
		return nullptr;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return nullptr;
	}

	// Get the container for this category
	UVerticalBox* Container = GetCategoryContainer(Config.Category);
	if (!Container)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOStatusPanel] No container found for category %d"), static_cast<int32>(Config.Category));
		return nullptr;
	}

	// Create the field widget
	UMOStatusField* Field = CreateWidget<UMOStatusField>(PC, StatusFieldClass);
	if (!Field)
	{
		return nullptr;
	}

	// Configure the field
	Field->SetFieldId(Config.FieldId);
	Field->SetFieldData(Config.Title, FText::GetEmpty(), -1.0f);

	if (Config.WarningThreshold >= 0.0f)
	{
		// TODO: Expose threshold setters on UMOStatusField
	}

	Field->SetProgressBarVisible(Config.bShowProgressBar);

	// Add to container
	Container->AddChild(Field);

	// Store references
	FieldWidgets.Add(Config.FieldId, Field);
	FieldConfigMap.Add(Config.FieldId, Config);

	UE_LOG(LogMOFramework, Log, TEXT("[MOStatusPanel] Added field: %s to category %d"), *Config.FieldId.ToString(), static_cast<int32>(Config.Category));

	return Field;
}

void UMOStatusPanel::RemoveField(FName FieldId)
{
	if (UMOStatusField* Field = GetFieldById(FieldId))
	{
		Field->RemoveFromParent();
		FieldWidgets.Remove(FieldId);
		FieldConfigMap.Remove(FieldId);
	}
}

void UMOStatusPanel::ClearCategory(EMOStatusCategory Category)
{
	UVerticalBox* Container = GetCategoryContainer(Category);
	if (Container)
	{
		// Find and remove all fields in this category
		TArray<FName> FieldsToRemove;
		for (const auto& Pair : FieldConfigMap)
		{
			if (Pair.Value.Category == Category)
			{
				FieldsToRemove.Add(Pair.Key);
			}
		}

		for (const FName& FieldId : FieldsToRemove)
		{
			RemoveField(FieldId);
		}

		Container->ClearChildren();
	}
}

UVerticalBox* UMOStatusPanel::GetCategoryContainer(EMOStatusCategory Category) const
{
	// First try to return the explicitly bound VerticalBox container
	UVerticalBox* Container = nullptr;
	UScrollBox* ScrollBox = nullptr;

	switch (Category)
	{
	case EMOStatusCategory::Vitals:
		Container = VitalsContainer;
		ScrollBox = VitalsScrollBox;
		break;
	case EMOStatusCategory::Nutrition:
		Container = NutritionContainer;
		ScrollBox = NutritionScrollBox;
		break;
	case EMOStatusCategory::Nutrients:
		Container = NutrientsContainer;
		ScrollBox = NutrientsScrollBox;
		break;
	case EMOStatusCategory::Fitness:
		Container = FitnessContainer;
		ScrollBox = FitnessScrollBox;
		break;
	case EMOStatusCategory::Mental:
		Container = MentalContainer;
		ScrollBox = MentalScrollBox;
		break;
	case EMOStatusCategory::Wounds:
		Container = WoundsContainer;
		ScrollBox = WoundsScrollBox;
		break;
	case EMOStatusCategory::Conditions:
		Container = ConditionsContainer;
		ScrollBox = ConditionsScrollBox;
		break;
	default:
		return nullptr;
	}

	// If we have an explicit container, use it
	if (Container)
	{
		return Container;
	}

	// If we only have a ScrollBox, look for a VerticalBox child or create one
	if (ScrollBox)
	{
		// Check if there's already a VerticalBox child
		for (int32 i = 0; i < ScrollBox->GetChildrenCount(); ++i)
		{
			if (UVerticalBox* ExistingBox = Cast<UVerticalBox>(ScrollBox->GetChildAt(i)))
			{
				// Cache it for next time (const_cast needed since we're in a const method)
				UMOStatusPanel* MutableThis = const_cast<UMOStatusPanel*>(this);
				switch (Category)
				{
				case EMOStatusCategory::Vitals:		MutableThis->VitalsContainer = ExistingBox; break;
				case EMOStatusCategory::Nutrition:	MutableThis->NutritionContainer = ExistingBox; break;
				case EMOStatusCategory::Nutrients:	MutableThis->NutrientsContainer = ExistingBox; break;
				case EMOStatusCategory::Fitness:	MutableThis->FitnessContainer = ExistingBox; break;
				case EMOStatusCategory::Mental:		MutableThis->MentalContainer = ExistingBox; break;
				case EMOStatusCategory::Wounds:		MutableThis->WoundsContainer = ExistingBox; break;
				case EMOStatusCategory::Conditions:	MutableThis->ConditionsContainer = ExistingBox; break;
				default: break;
				}
				return ExistingBox;
			}
		}

		// No VerticalBox found - create one and add it to the ScrollBox
		UVerticalBox* NewBox = NewObject<UVerticalBox>(ScrollBox);
		ScrollBox->AddChild(NewBox);

		// Cache it
		UMOStatusPanel* MutableThis = const_cast<UMOStatusPanel*>(this);
		switch (Category)
		{
		case EMOStatusCategory::Vitals:		MutableThis->VitalsContainer = NewBox; break;
		case EMOStatusCategory::Nutrition:	MutableThis->NutritionContainer = NewBox; break;
		case EMOStatusCategory::Nutrients:	MutableThis->NutrientsContainer = NewBox; break;
		case EMOStatusCategory::Fitness:	MutableThis->FitnessContainer = NewBox; break;
		case EMOStatusCategory::Mental:		MutableThis->MentalContainer = NewBox; break;
		case EMOStatusCategory::Wounds:		MutableThis->WoundsContainer = NewBox; break;
		case EMOStatusCategory::Conditions:	MutableThis->ConditionsContainer = NewBox; break;
		default: break;
		}

		UE_LOG(LogMOFramework, Log, TEXT("[MOStatusPanel] Auto-created VerticalBox container for category %d"), static_cast<int32>(Category));
		return NewBox;
	}

	return nullptr;
}

void UMOStatusPanel::BindTabButtons()
{
	if (VitalsTabButton)
	{
		VitalsTabButton->OnClicked().RemoveAll(this);
		VitalsTabButton->OnClicked().AddUObject(this, &UMOStatusPanel::HandleVitalsTabClicked);
	}
	if (NutritionTabButton)
	{
		NutritionTabButton->OnClicked().RemoveAll(this);
		NutritionTabButton->OnClicked().AddUObject(this, &UMOStatusPanel::HandleNutritionTabClicked);
	}
	if (NutrientsTabButton)
	{
		NutrientsTabButton->OnClicked().RemoveAll(this);
		NutrientsTabButton->OnClicked().AddUObject(this, &UMOStatusPanel::HandleNutrientsTabClicked);
	}
	if (FitnessTabButton)
	{
		FitnessTabButton->OnClicked().RemoveAll(this);
		FitnessTabButton->OnClicked().AddUObject(this, &UMOStatusPanel::HandleFitnessTabClicked);
	}
	if (MentalTabButton)
	{
		MentalTabButton->OnClicked().RemoveAll(this);
		MentalTabButton->OnClicked().AddUObject(this, &UMOStatusPanel::HandleMentalTabClicked);
	}
	if (WoundsTabButton)
	{
		WoundsTabButton->OnClicked().RemoveAll(this);
		WoundsTabButton->OnClicked().AddUObject(this, &UMOStatusPanel::HandleWoundsTabClicked);
	}
	if (ConditionsTabButton)
	{
		ConditionsTabButton->OnClicked().RemoveAll(this);
		ConditionsTabButton->OnClicked().AddUObject(this, &UMOStatusPanel::HandleConditionsTabClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
		BackButton->OnClicked().AddUObject(this, &UMOStatusPanel::HandleBackClicked);
	}
}

void UMOStatusPanel::HandleVitalsTabClicked()
{
	SwitchToCategory(EMOStatusCategory::Vitals);
}

void UMOStatusPanel::HandleNutritionTabClicked()
{
	SwitchToCategory(EMOStatusCategory::Nutrition);
}

void UMOStatusPanel::HandleNutrientsTabClicked()
{
	SwitchToCategory(EMOStatusCategory::Nutrients);
}

void UMOStatusPanel::HandleFitnessTabClicked()
{
	SwitchToCategory(EMOStatusCategory::Fitness);
}

void UMOStatusPanel::HandleMentalTabClicked()
{
	SwitchToCategory(EMOStatusCategory::Mental);
}

void UMOStatusPanel::HandleWoundsTabClicked()
{
	SwitchToCategory(EMOStatusCategory::Wounds);
}

void UMOStatusPanel::HandleConditionsTabClicked()
{
	SwitchToCategory(EMOStatusCategory::Conditions);
}

void UMOStatusPanel::HandleBackClicked()
{
	OnRequestClose.Broadcast();
}

void UMOStatusPanel::UpdateTabButtonStates()
{
	// TODO: Add visual selected state to buttons
	// This could use SetIsSelected() if using CommonUI toggle buttons
	// For now, buttons don't show selected state visually
}

void UMOStatusPanel::UpdateScrollBoxVisibility(EMOStatusCategory ActiveCategory)
{
	// Helper to set visibility
	auto SetScrollBoxVisible = [](UScrollBox* ScrollBox, bool bVisible)
	{
		if (ScrollBox)
		{
			ScrollBox->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	};

	SetScrollBoxVisible(VitalsScrollBox, ActiveCategory == EMOStatusCategory::Vitals);
	SetScrollBoxVisible(NutritionScrollBox, ActiveCategory == EMOStatusCategory::Nutrition);
	SetScrollBoxVisible(NutrientsScrollBox, ActiveCategory == EMOStatusCategory::Nutrients);
	SetScrollBoxVisible(FitnessScrollBox, ActiveCategory == EMOStatusCategory::Fitness);
	SetScrollBoxVisible(MentalScrollBox, ActiveCategory == EMOStatusCategory::Mental);
	SetScrollBoxVisible(WoundsScrollBox, ActiveCategory == EMOStatusCategory::Wounds);
	SetScrollBoxVisible(ConditionsScrollBox, ActiveCategory == EMOStatusCategory::Conditions);
}

void UMOStatusPanel::PopulateFieldConfigs_Implementation()
{
	// If configs already set in Blueprint defaults, use those
	if (FieldConfigs.Num() > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOStatusPanel] Using %d field configs from Blueprint defaults"), FieldConfigs.Num());
		return;
	}

	// Auto-populate with all medical system fields
	UE_LOG(LogMOFramework, Log, TEXT("[MOStatusPanel] Auto-populating default medical field configs"));

	// ============== VITALS ==============
	FieldConfigs.Add({ FName("HeartRate"), NSLOCTEXT("MOStatus", "HeartRate", "Heart Rate"), EMOStatusCategory::Vitals, TEXT("{0} BPM"), true, false, 0.6f, 0.4f, 0 });
	FieldConfigs.Add({ FName("BloodPressureSystolic"), NSLOCTEXT("MOStatus", "BPSystolic", "Blood Pressure (Sys)"), EMOStatusCategory::Vitals, TEXT("{0} mmHg"), false, false, -1.0f, -1.0f, 1 });
	FieldConfigs.Add({ FName("BloodPressureDiastolic"), NSLOCTEXT("MOStatus", "BPDiastolic", "Blood Pressure (Dia)"), EMOStatusCategory::Vitals, TEXT("{0} mmHg"), false, false, -1.0f, -1.0f, 2 });
	FieldConfigs.Add({ FName("SpO2"), NSLOCTEXT("MOStatus", "SpO2", "Oxygen Saturation"), EMOStatusCategory::Vitals, TEXT("{0}%"), true, false, 0.95f, 0.90f, 3 });
	FieldConfigs.Add({ FName("BodyTemperature"), NSLOCTEXT("MOStatus", "Temperature", "Body Temperature"), EMOStatusCategory::Vitals, TEXT("{0}\u00B0C"), false, false, -1.0f, -1.0f, 4 });
	FieldConfigs.Add({ FName("BloodVolume"), NSLOCTEXT("MOStatus", "BloodVolume", "Blood Volume"), EMOStatusCategory::Vitals, TEXT("{0} mL"), true, false, 0.7f, 0.5f, 5 });
	FieldConfigs.Add({ FName("RespiratoryRate"), NSLOCTEXT("MOStatus", "RespRate", "Respiratory Rate"), EMOStatusCategory::Vitals, TEXT("{0} /min"), true, false, -1.0f, -1.0f, 6 });
	FieldConfigs.Add({ FName("BloodGlucose"), NSLOCTEXT("MOStatus", "Glucose", "Blood Glucose"), EMOStatusCategory::Vitals, TEXT("{0} mg/dL"), true, false, 0.5f, 0.3f, 7 });

	// ============== NUTRITION ==============
	FieldConfigs.Add({ FName("Hunger"), NSLOCTEXT("MOStatus", "Hunger", "Hunger"), EMOStatusCategory::Nutrition, TEXT("{0}%"), true, false, 0.5f, 0.25f, 0 });
	FieldConfigs.Add({ FName("Thirst"), NSLOCTEXT("MOStatus", "Thirst", "Thirst"), EMOStatusCategory::Nutrition, TEXT("{0}%"), true, false, 0.5f, 0.25f, 1 });
	FieldConfigs.Add({ FName("GlycogenStores"), NSLOCTEXT("MOStatus", "Glycogen", "Glycogen Stores"), EMOStatusCategory::Nutrition, TEXT("{0} g"), true, false, 0.4f, 0.2f, 2 });
	FieldConfigs.Add({ FName("HydrationLevel"), NSLOCTEXT("MOStatus", "Hydration", "Hydration"), EMOStatusCategory::Nutrition, TEXT("{0}%"), true, false, 0.5f, 0.25f, 3 });
	FieldConfigs.Add({ FName("ProteinBalance"), NSLOCTEXT("MOStatus", "Protein", "Protein Balance"), EMOStatusCategory::Nutrition, TEXT("{0} g"), true, false, -1.0f, -1.0f, 4 });
	FieldConfigs.Add({ FName("CalorieBalance"), NSLOCTEXT("MOStatus", "Calories", "Calorie Balance"), EMOStatusCategory::Nutrition, TEXT("{0} kcal"), false, false, -1.0f, -1.0f, 5 });

	// ============== NUTRIENTS ==============
	FieldConfigs.Add({ FName("VitaminA"), NSLOCTEXT("MOStatus", "VitaminA", "Vitamin A"), EMOStatusCategory::Nutrients, TEXT("{0}%"), true, false, 0.5f, 0.25f, 0 });
	FieldConfigs.Add({ FName("VitaminB"), NSLOCTEXT("MOStatus", "VitaminB", "Vitamin B"), EMOStatusCategory::Nutrients, TEXT("{0}%"), true, false, 0.5f, 0.25f, 1 });
	FieldConfigs.Add({ FName("VitaminC"), NSLOCTEXT("MOStatus", "VitaminC", "Vitamin C"), EMOStatusCategory::Nutrients, TEXT("{0}%"), true, false, 0.5f, 0.25f, 2 });
	FieldConfigs.Add({ FName("VitaminD"), NSLOCTEXT("MOStatus", "VitaminD", "Vitamin D"), EMOStatusCategory::Nutrients, TEXT("{0}%"), true, false, 0.5f, 0.25f, 3 });
	FieldConfigs.Add({ FName("Iron"), NSLOCTEXT("MOStatus", "Iron", "Iron"), EMOStatusCategory::Nutrients, TEXT("{0}%"), true, false, 0.5f, 0.25f, 4 });
	FieldConfigs.Add({ FName("Calcium"), NSLOCTEXT("MOStatus", "Calcium", "Calcium"), EMOStatusCategory::Nutrients, TEXT("{0}%"), true, false, 0.5f, 0.25f, 5 });
	FieldConfigs.Add({ FName("Potassium"), NSLOCTEXT("MOStatus", "Potassium", "Potassium"), EMOStatusCategory::Nutrients, TEXT("{0}%"), true, false, 0.5f, 0.25f, 6 });
	FieldConfigs.Add({ FName("Sodium"), NSLOCTEXT("MOStatus", "Sodium", "Sodium"), EMOStatusCategory::Nutrients, TEXT("{0}%"), true, false, 0.5f, 0.25f, 7 });

	// ============== FITNESS ==============
	FieldConfigs.Add({ FName("MuscleMass"), NSLOCTEXT("MOStatus", "MuscleMass", "Muscle Mass"), EMOStatusCategory::Fitness, TEXT("{0} kg"), true, false, -1.0f, -1.0f, 0 });
	FieldConfigs.Add({ FName("BodyFatPercent"), NSLOCTEXT("MOStatus", "BodyFat", "Body Fat"), EMOStatusCategory::Fitness, TEXT("{0}%"), true, false, -1.0f, -1.0f, 1 });
	FieldConfigs.Add({ FName("CardiovascularFitness"), NSLOCTEXT("MOStatus", "CardioFitness", "Cardio Fitness"), EMOStatusCategory::Fitness, TEXT("{0}"), true, false, 0.5f, 0.25f, 2 });
	FieldConfigs.Add({ FName("StrengthLevel"), NSLOCTEXT("MOStatus", "Strength", "Strength"), EMOStatusCategory::Fitness, TEXT("{0}"), true, false, 0.5f, 0.25f, 3 });
	FieldConfigs.Add({ FName("TotalWeight"), NSLOCTEXT("MOStatus", "Weight", "Body Weight"), EMOStatusCategory::Fitness, TEXT("{0} kg"), false, false, -1.0f, -1.0f, 4 });
	FieldConfigs.Add({ FName("Stamina"), NSLOCTEXT("MOStatus", "Stamina", "Stamina"), EMOStatusCategory::Fitness, TEXT("{0}%"), true, false, 0.5f, 0.25f, 5 });

	// ============== MENTAL ==============
	FieldConfigs.Add({ FName("Consciousness"), NSLOCTEXT("MOStatus", "Consciousness", "Consciousness"), EMOStatusCategory::Mental, TEXT("{0}"), false, false, -1.0f, -1.0f, 0 });
	FieldConfigs.Add({ FName("ShockLevel"), NSLOCTEXT("MOStatus", "Shock", "Shock"), EMOStatusCategory::Mental, TEXT("{0}%"), true, true, 0.5f, 0.75f, 1 });
	FieldConfigs.Add({ FName("TraumaticStress"), NSLOCTEXT("MOStatus", "Trauma", "Traumatic Stress"), EMOStatusCategory::Mental, TEXT("{0}%"), true, true, 0.5f, 0.75f, 2 });
	FieldConfigs.Add({ FName("MoraleFatigue"), NSLOCTEXT("MOStatus", "Morale", "Morale/Fatigue"), EMOStatusCategory::Mental, TEXT("{0}%"), true, false, 0.5f, 0.25f, 3 });
	FieldConfigs.Add({ FName("Energy"), NSLOCTEXT("MOStatus", "Energy", "Energy"), EMOStatusCategory::Mental, TEXT("{0}%"), true, false, 0.5f, 0.25f, 4 });

	// Note: Wounds and Conditions are dynamic - they get added/removed at runtime
	// based on actual injuries. Use AddField() and RemoveField() for those.

	UE_LOG(LogMOFramework, Log, TEXT("[MOStatusPanel] Auto-populated %d field configs"), FieldConfigs.Num());
}

void UMOStatusPanel::CreateFieldsFromConfigs()
{
	// Sort configs by category and priority
	FieldConfigs.Sort([](const FMOStatusFieldConfig& A, const FMOStatusFieldConfig& B)
	{
		if (A.Category != B.Category)
		{
			return static_cast<int32>(A.Category) < static_cast<int32>(B.Category);
		}
		return A.SortPriority < B.SortPriority;
	});

	// Create fields from configs
	for (const FMOStatusFieldConfig& Config : FieldConfigs)
	{
		AddField(Config);
	}

	// Add "None" placeholders for empty dynamic categories
	UpdateEmptyPlaceholders();

	UE_LOG(LogMOFramework, Log, TEXT("[MOStatusPanel] Created %d fields from configs"), FieldConfigs.Num());
}

void UMOStatusPanel::UpdateEmptyPlaceholders()
{
	// Check Wounds category
	bool bHasWounds = false;
	bool bHasConditions = false;

	for (const auto& Pair : FieldConfigMap)
	{
		if (Pair.Value.Category == EMOStatusCategory::Wounds)
		{
			bHasWounds = true;
		}
		if (Pair.Value.Category == EMOStatusCategory::Conditions)
		{
			bHasConditions = true;
		}
	}

	// Add placeholder text if no wounds
	UVerticalBox* WoundsBox = GetCategoryContainer(EMOStatusCategory::Wounds);
	if (WoundsBox)
	{
		// Remove existing placeholder TextBlocks (they won't be in FieldWidgets)
		for (int32 i = WoundsBox->GetChildrenCount() - 1; i >= 0; --i)
		{
			if (UTextBlock* TextBlock = Cast<UTextBlock>(WoundsBox->GetChildAt(i)))
			{
				TextBlock->RemoveFromParent();
			}
		}

		if (!bHasWounds)
		{
			UTextBlock* PlaceholderText = NewObject<UTextBlock>(WoundsBox);
			PlaceholderText->SetText(NSLOCTEXT("MOStatus", "NoWounds", "No wounds"));
			PlaceholderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
			PlaceholderText->SetRenderOpacity(0.7f);
			WoundsBox->AddChild(PlaceholderText);
		}
	}

	// Add placeholder text if no conditions
	UVerticalBox* ConditionsBox = GetCategoryContainer(EMOStatusCategory::Conditions);
	if (ConditionsBox)
	{
		// Remove existing placeholder TextBlocks
		for (int32 i = ConditionsBox->GetChildrenCount() - 1; i >= 0; --i)
		{
			if (UTextBlock* TextBlock = Cast<UTextBlock>(ConditionsBox->GetChildAt(i)))
			{
				TextBlock->RemoveFromParent();
			}
		}

		if (!bHasConditions)
		{
			UTextBlock* PlaceholderText = NewObject<UTextBlock>(ConditionsBox);
			PlaceholderText->SetText(NSLOCTEXT("MOStatus", "NoConditions", "No conditions"));
			PlaceholderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
			PlaceholderText->SetRenderOpacity(0.7f);
			ConditionsBox->AddChild(PlaceholderText);
		}
	}
}

void UMOStatusPanel::BindToMedicalComponents(UMOVitalsComponent* Vitals, UMOMetabolismComponent* Metabolism, UMOMentalStateComponent* MentalState)
{
	// Unbind from any previous components first
	UnbindFromMedicalComponents();

	// Bind to vitals
	if (IsValid(Vitals))
	{
		BoundVitals = Vitals;
		Vitals->OnVitalsChanged.AddDynamic(this, &UMOStatusPanel::HandleVitalsChanged);
		UpdateVitalsFields();
		UE_LOG(LogMOFramework, Log, TEXT("[MOStatusPanel] Bound to VitalsComponent"));
	}

	// Bind to metabolism
	if (IsValid(Metabolism))
	{
		BoundMetabolism = Metabolism;
		Metabolism->OnMetabolismChanged.AddDynamic(this, &UMOStatusPanel::HandleMetabolismChanged);
		UpdateMetabolismFields();
		UE_LOG(LogMOFramework, Log, TEXT("[MOStatusPanel] Bound to MetabolismComponent"));
	}

	// Bind to mental state
	if (IsValid(MentalState))
	{
		BoundMentalState = MentalState;
		MentalState->OnMentalStateChanged.AddDynamic(this, &UMOStatusPanel::HandleMentalStateChanged);
		UpdateMentalStateFields();
		UE_LOG(LogMOFramework, Log, TEXT("[MOStatusPanel] Bound to MentalStateComponent"));
	}
}

void UMOStatusPanel::UnbindFromMedicalComponents()
{
	if (UMOVitalsComponent* Vitals = BoundVitals.Get())
	{
		Vitals->OnVitalsChanged.RemoveDynamic(this, &UMOStatusPanel::HandleVitalsChanged);
	}
	BoundVitals.Reset();

	if (UMOMetabolismComponent* Metabolism = BoundMetabolism.Get())
	{
		Metabolism->OnMetabolismChanged.RemoveDynamic(this, &UMOStatusPanel::HandleMetabolismChanged);
	}
	BoundMetabolism.Reset();

	if (UMOMentalStateComponent* MentalState = BoundMentalState.Get())
	{
		MentalState->OnMentalStateChanged.RemoveDynamic(this, &UMOStatusPanel::HandleMentalStateChanged);
	}
	BoundMentalState.Reset();
}

void UMOStatusPanel::HandleVitalsChanged()
{
	UpdateVitalsFields();
}

void UMOStatusPanel::HandleMetabolismChanged()
{
	UpdateMetabolismFields();
}

void UMOStatusPanel::HandleMentalStateChanged()
{
	UpdateMentalStateFields();
}

void UMOStatusPanel::UpdateVitalsFields()
{
	UMOVitalsComponent* Vitals = BoundVitals.Get();
	if (!Vitals)
	{
		return;
	}

	const FMOVitalSigns& Signs = Vitals->GetVitalSigns();

	// Heart Rate - normalized based on resting range (60-100 normal, above/below is concerning)
	float HRNorm = FMath::GetMappedRangeValueClamped(FVector2D(40.0f, 120.0f), FVector2D(0.0f, 1.0f), Signs.HeartRate);
	// Invert so middle range is "good"
	HRNorm = 1.0f - FMath::Abs(HRNorm - 0.5f) * 2.0f;
	UpdateFieldValueFloat(FName("HeartRate"), Signs.HeartRate, HRNorm);

	// Blood Pressure
	UpdateFieldValueFloat(FName("BloodPressureSystolic"), Signs.SystolicBP, -1.0f);
	UpdateFieldValueFloat(FName("BloodPressureDiastolic"), Signs.DiastolicBP, -1.0f);

	// SpO2 - normalized 90-100
	float SpO2Norm = FMath::GetMappedRangeValueClamped(FVector2D(80.0f, 100.0f), FVector2D(0.0f, 1.0f), Signs.SpO2);
	UpdateFieldValueFloat(FName("SpO2"), Signs.SpO2, SpO2Norm);

	// Temperature - normalized around 37C
	float TempNorm = 1.0f - FMath::Abs(Signs.BodyTemperature - 37.0f) / 5.0f;
	TempNorm = FMath::Clamp(TempNorm, 0.0f, 1.0f);
	UpdateFieldValueFloat(FName("BodyTemperature"), Signs.BodyTemperature, TempNorm);

	// Blood Volume - normalized to max
	float BloodNorm = Signs.BloodVolume / Signs.MaxBloodVolume;
	UpdateFieldValueFloat(FName("BloodVolume"), Signs.BloodVolume, BloodNorm);

	// Respiratory Rate
	float RRNorm = FMath::GetMappedRangeValueClamped(FVector2D(8.0f, 30.0f), FVector2D(0.0f, 1.0f), Signs.RespiratoryRate);
	RRNorm = 1.0f - FMath::Abs(RRNorm - 0.4f) * 1.5f; // ~16 is optimal
	RRNorm = FMath::Clamp(RRNorm, 0.0f, 1.0f);
	UpdateFieldValueFloat(FName("RespiratoryRate"), Signs.RespiratoryRate, RRNorm);

	// Blood Glucose - normalized 70-140 range
	float GlucoseNorm = FMath::GetMappedRangeValueClamped(FVector2D(40.0f, 180.0f), FVector2D(0.0f, 1.0f), Signs.BloodGlucose);
	GlucoseNorm = 1.0f - FMath::Abs(GlucoseNorm - 0.5f) * 2.0f;
	UpdateFieldValueFloat(FName("BloodGlucose"), Signs.BloodGlucose, GlucoseNorm);
}

void UMOStatusPanel::UpdateMetabolismFields()
{
	UMOMetabolismComponent* Metabolism = BoundMetabolism.Get();
	if (!Metabolism)
	{
		return;
	}

	const FMONutrientLevels& Nutrients = Metabolism->GetNutrientLevels();
	const FMOBodyComposition& Body = Metabolism->GetBodyComposition();

	// Nutrition - Hunger is based on glycogen stores (energy reserves)
	float HungerPercent = (Nutrients.MaxGlycogen > 0.0f) ? (Nutrients.GlycogenStores / Nutrients.MaxGlycogen) : 0.0f;
	UpdateFieldValueFloat(FName("Hunger"), HungerPercent * 100.0f, HungerPercent);
	UpdateFieldValueFloat(FName("Thirst"), Nutrients.HydrationLevel, Nutrients.HydrationLevel / 100.0f);
	UpdateFieldValueFloat(FName("GlycogenStores"), Nutrients.GlycogenStores, Nutrients.GlycogenStores / Nutrients.MaxGlycogen);
	UpdateFieldValueFloat(FName("HydrationLevel"), Nutrients.HydrationLevel, Nutrients.HydrationLevel / 100.0f);
	UpdateFieldValueFloat(FName("ProteinBalance"), Nutrients.ProteinBalance, -1.0f);
	UpdateFieldValueFloat(FName("CalorieBalance"), Metabolism->GetDailyCalorieBalance(), -1.0f);

	// Nutrients (vitamins/minerals as % daily value)
	UpdateFieldValueFloat(FName("VitaminA"), Nutrients.VitaminA, Nutrients.VitaminA / 100.0f);
	UpdateFieldValueFloat(FName("VitaminB"), Nutrients.VitaminB, Nutrients.VitaminB / 100.0f);
	UpdateFieldValueFloat(FName("VitaminC"), Nutrients.VitaminC, Nutrients.VitaminC / 100.0f);
	UpdateFieldValueFloat(FName("VitaminD"), Nutrients.VitaminD, Nutrients.VitaminD / 100.0f);
	UpdateFieldValueFloat(FName("Iron"), Nutrients.Iron, Nutrients.Iron / 100.0f);
	UpdateFieldValueFloat(FName("Calcium"), Nutrients.Calcium, Nutrients.Calcium / 100.0f);
	UpdateFieldValueFloat(FName("Potassium"), Nutrients.Potassium, Nutrients.Potassium / 100.0f);
	UpdateFieldValueFloat(FName("Sodium"), Nutrients.Sodium, Nutrients.Sodium / 100.0f);

	// Fitness
	UpdateFieldValueFloat(FName("MuscleMass"), Body.MuscleMass, Body.MuscleMass / 50.0f);
	UpdateFieldValueFloat(FName("BodyFatPercent"), Body.BodyFatPercent, -1.0f);
	UpdateFieldValueFloat(FName("CardiovascularFitness"), Body.CardiovascularFitness, Body.CardiovascularFitness / 100.0f);
	UpdateFieldValueFloat(FName("StrengthLevel"), Body.StrengthLevel, Body.StrengthLevel / 100.0f);
	UpdateFieldValueFloat(FName("TotalWeight"), Body.TotalWeight, -1.0f);
	UpdateFieldValueFloat(FName("Stamina"), Metabolism->GetCurrentStamina() * 100.0f, Metabolism->GetCurrentStamina());
}

void UMOStatusPanel::UpdateMentalStateFields()
{
	UMOMentalStateComponent* MentalState = BoundMentalState.Get();
	if (!MentalState)
	{
		return;
	}

	const FMOMentalState& State = MentalState->GetMentalState();

	// Consciousness level as text
	FText ConsciousnessText;
	switch (State.Consciousness)
	{
	case EMOConsciousnessLevel::Alert:			ConsciousnessText = NSLOCTEXT("MOStatus", "Alert", "Alert"); break;
	case EMOConsciousnessLevel::Confused:		ConsciousnessText = NSLOCTEXT("MOStatus", "Confused", "Confused"); break;
	case EMOConsciousnessLevel::Drowsy:			ConsciousnessText = NSLOCTEXT("MOStatus", "Drowsy", "Drowsy"); break;
	case EMOConsciousnessLevel::Unconscious:	ConsciousnessText = NSLOCTEXT("MOStatus", "Unconscious", "Unconscious"); break;
	case EMOConsciousnessLevel::Comatose:		ConsciousnessText = NSLOCTEXT("MOStatus", "Comatose", "Comatose"); break;
	default:									ConsciousnessText = NSLOCTEXT("MOStatus", "Unknown", "Unknown"); break;
	}
	UpdateFieldValue(FName("Consciousness"), ConsciousnessText, -1.0f);

	// Shock (inverted - higher is worse)
	UpdateFieldValueFloat(FName("ShockLevel"), State.ShockAccumulation, State.ShockAccumulation / 100.0f);

	// Traumatic Stress (inverted - higher is worse)
	UpdateFieldValueFloat(FName("TraumaticStress"), State.TraumaticStress, State.TraumaticStress / 100.0f);

	// Morale/Fatigue (inverted for fatigue)
	UpdateFieldValueFloat(FName("MoraleFatigue"), 100.0f - State.MoraleFatigue, (100.0f - State.MoraleFatigue) / 100.0f);

	// Energy
	UpdateFieldValueFloat(FName("Energy"), MentalState->GetEnergyLevel() * 100.0f, MentalState->GetEnergyLevel());
}
