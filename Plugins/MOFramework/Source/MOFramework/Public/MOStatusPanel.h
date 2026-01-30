#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOStatusPanel.generated.h"

class UMOStatusField;
class UMOCommonButton;
class UWidgetSwitcher;
class UScrollBox;
class UVerticalBox;
class UMOVitalsComponent;
class UMOMetabolismComponent;
class UMOMentalStateComponent;

/**
 * Status category for organizing fields into tabs
 */
UENUM(BlueprintType)
enum class EMOStatusCategory : uint8
{
	Vitals		UMETA(DisplayName = "Vitals"),
	Nutrition	UMETA(DisplayName = "Nutrition"),
	Nutrients	UMETA(DisplayName = "Nutrients"),
	Fitness		UMETA(DisplayName = "Fitness"),
	Mental		UMETA(DisplayName = "Mental"),
	Wounds		UMETA(DisplayName = "Wounds"),
	Conditions	UMETA(DisplayName = "Conditions"),

	MAX			UMETA(Hidden)
};

/**
 * Configuration for a status field that will be created dynamically
 */
USTRUCT(BlueprintType)
struct FMOStatusFieldConfig
{
	GENERATED_BODY()

	/** Unique identifier for data binding */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName FieldId;

	/** Display title */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Title;

	/** Which category/tab this field belongs to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMOStatusCategory Category = EMOStatusCategory::Vitals;

	/** Format string for the value (e.g., "{0} BPM", "{0}%", "{0}°C") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ValueFormat = TEXT("{0}");

	/** Whether to show a progress bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bShowProgressBar = true;

	/** Whether higher values are worse (inverts color thresholds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInvertThresholds = false;

	/** Custom warning threshold (0-1), -1 to use default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WarningThreshold = -1.0f;

	/** Custom critical threshold (0-1), -1 to use default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CriticalThreshold = -1.0f;

	/** Sort priority within category (lower = higher in list) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SortPriority = 0;
};

/**
 * Delegate for when a category tab is selected
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOStatusCategoryChangedSignature, EMOStatusCategory, NewCategory);

/**
 * Delegate for panel close request
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOStatusPanelRequestCloseSignature);

/**
 * Tab-based status panel for displaying player stats organized by category.
 * Uses MOStatusField widgets to display individual stats.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOStatusPanel : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Switch to a specific category tab */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void SwitchToCategory(EMOStatusCategory Category);

	/** Get the currently active category */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	EMOStatusCategory GetCurrentCategory() const { return CurrentCategory; }

	/** Update a specific field's value by ID */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void UpdateFieldValue(FName FieldId, const FText& Value, float NormalizedValue = -1.0f);

	/** Update a field using a float value and its configured format string */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void UpdateFieldValueFloat(FName FieldId, float Value, float NormalizedValue = -1.0f);

	/** Get a status field by ID */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	UMOStatusField* GetFieldById(FName FieldId) const;

	/** Refresh all fields from bound data source */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void RefreshAllFields();

	/** Add a field dynamically at runtime */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	UMOStatusField* AddField(const FMOStatusFieldConfig& Config);

	/** Remove a field by ID */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void RemoveField(FName FieldId);

	/** Clear all fields in a category */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void ClearCategory(EMOStatusCategory Category);

	/** Bind to medical components for automatic updates */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void BindToMedicalComponents(UMOVitalsComponent* Vitals, UMOMetabolismComponent* Metabolism, UMOMentalStateComponent* MentalState);

	/** Unbind from medical components */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void UnbindFromMedicalComponents();

	/** Broadcast when category changes */
	UPROPERTY(BlueprintAssignable, Category = "MO|Status")
	FMOStatusCategoryChangedSignature OnCategoryChanged;

	/** Broadcast when panel requests to close */
	UPROPERTY(BlueprintAssignable, Category = "MO|Status")
	FMOStatusPanelRequestCloseSignature OnRequestClose;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Blueprint event when category changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "MO|Status")
	void OnCategoryChangedBP(EMOStatusCategory NewCategory);

	/** Blueprint event to populate fields - override to set up your field configurations */
	UFUNCTION(BlueprintNativeEvent, Category = "MO|Status")
	void PopulateFieldConfigs();

	/** Create field widgets from configs */
	void CreateFieldsFromConfigs();

	/** Get the container for a specific category */
	UVerticalBox* GetCategoryContainer(EMOStatusCategory Category) const;

	/** Bind tab button events */
	void BindTabButtons();

	/** Tab button handlers */
	void HandleVitalsTabClicked();
	void HandleNutritionTabClicked();
	void HandleNutrientsTabClicked();
	void HandleFitnessTabClicked();
	void HandleMentalTabClicked();
	void HandleWoundsTabClicked();
	void HandleConditionsTabClicked();
	void HandleBackClicked();

	/** Update tab button visual states */
	void UpdateTabButtonStates();

	/** Medical component change handlers */
	UFUNCTION()
	void HandleVitalsChanged();

	UFUNCTION()
	void HandleMetabolismChanged();

	UFUNCTION()
	void HandleMentalStateChanged();

	/** Update fields from bound components */
	void UpdateVitalsFields();
	void UpdateMetabolismFields();
	void UpdateMentalStateFields();

protected:
	// Tab buttons - bind in Blueprint
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMOCommonButton> VitalsTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMOCommonButton> NutritionTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMOCommonButton> NutrientsTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMOCommonButton> FitnessTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMOCommonButton> MentalTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMOCommonButton> WoundsTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMOCommonButton> ConditionsTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMOCommonButton> BackButton;

	/** Widget switcher for category content */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> CategorySwitcher;

	/** Scroll boxes for each category (children of CategorySwitcher) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> VitalsScrollBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> NutritionScrollBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> NutrientsScrollBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> FitnessScrollBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> MentalScrollBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> WoundsScrollBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ConditionsScrollBox;

	/** Vertical box containers inside each scroll box */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> VitalsContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> NutritionContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> NutrientsContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> FitnessContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> MentalContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> WoundsContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ConditionsContainer;

	/** Widget class to use for status fields */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Status")
	TSubclassOf<UMOStatusField> StatusFieldClass;

	/** Field configurations - populate in Blueprint or via PopulateFieldConfigs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Status")
	TArray<FMOStatusFieldConfig> FieldConfigs;

	/** Currently active category */
	UPROPERTY(BlueprintReadOnly, Category = "MO|Status")
	EMOStatusCategory CurrentCategory = EMOStatusCategory::Vitals;

	/** Map of field ID to widget for quick lookup */
	UPROPERTY()
	TMap<FName, TObjectPtr<UMOStatusField>> FieldWidgets;

	/** Map of field ID to config for format strings etc. */
	TMap<FName, FMOStatusFieldConfig> FieldConfigMap;

	/** Index mapping for widget switcher */
	static constexpr int32 CategoryIndex_Vitals = 0;
	static constexpr int32 CategoryIndex_Nutrition = 1;
	static constexpr int32 CategoryIndex_Nutrients = 2;
	static constexpr int32 CategoryIndex_Fitness = 3;
	static constexpr int32 CategoryIndex_Mental = 4;
	static constexpr int32 CategoryIndex_Wounds = 5;
	static constexpr int32 CategoryIndex_Conditions = 6;

	/** Bound medical components for auto-updates */
	UPROPERTY()
	TWeakObjectPtr<UMOVitalsComponent> BoundVitals;

	UPROPERTY()
	TWeakObjectPtr<UMOMetabolismComponent> BoundMetabolism;

	UPROPERTY()
	TWeakObjectPtr<UMOMentalStateComponent> BoundMentalState;
};
