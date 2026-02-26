/**
 * =============================================================================
 * MOSkillsPanel.h - Skills & Knowledge Display Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Displays player skills with XP progress, or learned knowledge/discoveries.
 * Toggleable between Skills and Knowledge modes.
 *
 * SKILLS MODE:
 * - List of all skills with current level
 * - XP progress bar to next level
 * - Category grouping (Survival, Crafting, Combat, etc.)
 *
 * KNOWLEDGE MODE:
 * - List of learned recipes/techniques
 * - Discovery method (Manual, Inspection, Experimentation)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] MODE TOGGLE: Skills vs Knowledge are separate tabs. Use
 *   SetDisplayMode() to switch. PopulateSkillContainer() vs
 *   PopulateKnowledgeContainer() handle the different data sources.
 *
 * [2024-02] XP BINDING: InitializePanel() binds to SkillsComponent's
 *   OnSkillLevelUp and OnExperienceGained delegates. Unbinds in NativeDestruct().
 *   Uses RemoveAll(this) pattern for safe rebinding.
 *
 * [2024-02] ENTRY CREATION: Creates MOSkillEntryWidget per skill. For 50+
 *   skills, consider widget pooling (not currently implemented).
 *
 * [2024-02] COMPONENT REFERENCES: SkillsComponent and KnowledgeComponent are
 *   TWeakObjectPtr. Handlers check .IsValid() before access. Safe if component
 *   is destroyed while panel is open.
 *
 * [2024-02] DISPLAY DATA STRUCT: FMOSkillDisplayData (in MOSkillEntryWidget.h)
 *   contains: SkillId, DisplayName, Description, Level, CurrentXP, XPToNext,
 *   Progress (0-1), Category, Icon. Used by both skills and knowledge modes.
 *
 * [2024-02] CATEGORY FILTER: EMOSkillCategory enum (in MOSkillDefinitionRow.h).
 *   Values: None (show all), Survival, Crafting, Combat, Social, Knowledge.
 *   SetCategoryFilter() + RefreshSkillList() to apply.
 *
 * [2024-02] WIDGET CLASS FALLBACK: If SkillEntryWidgetClass not set, panel
 *   uses CreateSimpleSkillText() to show basic text entries (UTextBlock).
 *
 * [2024-02] INITIALIZATION ORDER: InitializePanel() or InitializePanelWithKnowledge()
 *   first, then SetDisplayMode() if not default. RefreshSkillList() called
 *   automatically by initialization.
 *
 * [2024-02] DOUBLE INITIALIZATION: Calling InitializePanel() twice is SAFE.
 *   Uses RemoveAll(this) before rebinding delegates. Use case: possession change
 *   to different pawn requires reinitializing with new SkillsComponent.
 *   SYMPTOM if not reinitialized: Panel shows old pawn's skills.
 *
 * [2024-02] NULL COMPONENT HANDLING: If SkillsComponent is nullptr:
 *   - PopulateSkillContainer() shows empty list (no crash)
 *   - XP/level delegates not bound (nothing to bind to)
 *   - Panel is functional but shows no skills
 *   If KnowledgeComponent is nullptr in Knowledge mode: empty list, no crash.
 *   SYMPTOM: Empty skills panel with no error. Check component validity first.
 *
 * [2024-02] PERFORMANCE AT SCALE: Creates one UMOSkillEntryWidget per skill.
 *   At 50+ skills, initial population causes frame hitch (~50-100ms).
 *   MITIGATION: Consider lazy loading (populate visible only) or widget pooling.
 *   Currently no pooling implemented - widgets destroyed on mode switch.
 *
 * [2024-02] FILTER + SHOW ALL INTERACTION: bShowAllSkills=true shows level 0
 *   skills. SetCategoryFilter() applies ON TOP of this. Both conditions must
 *   pass: (bShowAllSkills OR Level>0) AND (CategoryFilter==None OR matches).
 *
 * =============================================================================
 * RELATED FILES: MOSkillsComponent.h, MOKnowledgeComponent.h, MOSkillEntryWidget.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOSkillDefinitionRow.h"
#include "MOSkillEntryWidget.h"
#include "MOSkillsPanel.generated.h"

class UMOSkillsComponent;
class UMOKnowledgeComponent;
class UMOSkillEntryWidget;
class UScrollBox;
class UVerticalBox;
class UPanelWidget;
class UTextBlock;
class UMOCommonButton;

/**
 * Display mode for the skills/knowledge panel.
 */
UENUM(BlueprintType)
enum class EMOSkillsPanelMode : uint8
{
	Skills,		// Show skills with XP progress
	Knowledge	// Show learned knowledge/discoveries
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOSkillsPanelRequestCloseSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOSkillsPanelSkillSelectedSignature, FName, SkillId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOSkillsPanelModeChangedSignature, EMOSkillsPanelMode, NewMode);

/**
 * Panel widget that displays all skills with their levels and XP progress.
 * Can filter by category and shows skill details.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOSkillsPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOSkillsPanel(const FObjectInitializer& ObjectInitializer);

	// --- Initialization ---

	/** Initialize with the player's skills component. */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void InitializePanel(UMOSkillsComponent* InSkillsComponent);

	/** Initialize with both skills and knowledge components. */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void InitializePanelWithKnowledge(UMOSkillsComponent* InSkillsComponent, UMOKnowledgeComponent* InKnowledgeComponent);

	// --- Display Mode ---

	/** Set the display mode (Skills or Knowledge). */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void SetDisplayMode(EMOSkillsPanelMode NewMode);

	/** Get current display mode. */
	UFUNCTION(BlueprintPure, Category="MO|Skills|UI")
	EMOSkillsPanelMode GetDisplayMode() const { return CurrentDisplayMode; }

	/** Switch to Skills mode. */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void ShowSkills();

	/** Switch to Knowledge mode. */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void ShowKnowledge();

	// --- Refresh ---

	/** Rebuild the entire skill list. */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void RefreshSkillList();

	/** Update progress displays without rebuilding. */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void RefreshProgress();

	// --- Filtering ---

	/** Set category filter (None = show all). */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void SetCategoryFilter(EMOSkillCategory Category);

	/** Clear category filter to show all skills. */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void ClearCategoryFilter();

	/** Get current category filter. */
	UFUNCTION(BlueprintPure, Category="MO|Skills|UI")
	EMOSkillCategory GetCategoryFilter() const { return CategoryFilter; }

	// --- Selection ---

	/** Select a skill to show details. */
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void SelectSkill(FName SkillId);

	/** Get currently selected skill. */
	UFUNCTION(BlueprintPure, Category="MO|Skills|UI")
	FName GetSelectedSkillId() const { return SelectedSkillId; }

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category="MO|Skills|UI")
	FMOSkillsPanelRequestCloseSignature OnRequestClose;

	UPROPERTY(BlueprintAssignable, Category="MO|Skills|UI")
	FMOSkillsPanelSkillSelectedSignature OnSkillSelected;

	UPROPERTY(BlueprintAssignable, Category="MO|Skills|UI")
	FMOSkillsPanelModeChangedSignature OnModeChanged;

	// --- Configuration ---

	/** Widget class to use for skill entries. If not set, uses simple text entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Skills|UI")
	TSubclassOf<UMOSkillEntryWidget> SkillEntryWidgetClass;

	/** Whether to show skills with no progress (level 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Skills|UI")
	bool bShowAllSkills = true;

	/** Whether to sort skills by level (highest first). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Skills|UI")
	bool bSortByLevel = true;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Handle skill level up event. */
	UFUNCTION()
	void HandleSkillLevelUp(FName SkillId, int32 OldLevel, int32 NewLevel);

	/** Handle XP gained event. */
	UFUNCTION()
	void HandleExperienceGained(FName SkillId, float XPGained, float TotalXP);

	/** Handle skill entry selected. */
	UFUNCTION()
	void HandleSkillEntrySelected(FName SkillId);

	/** Handle close button clicked. */
	UFUNCTION()
	void HandleCloseClicked();

	/** Handle skills tab button clicked. */
	UFUNCTION()
	void HandleSkillsTabClicked();

	/** Handle knowledge tab button clicked. */
	UFUNCTION()
	void HandleKnowledgeTabClicked();

	/** Handle new knowledge learned. */
	UFUNCTION()
	void HandleKnowledgeLearned(FName KnowledgeId, FName FromItemId);

	/** Build display data for a skill. */
	FMOSkillDisplayData BuildSkillDisplayData(FName SkillId) const;

	/** Build display data for knowledge (reuses FMOSkillDisplayData structure). */
	FMOSkillDisplayData BuildKnowledgeDisplayData(FName KnowledgeId) const;

	/** Format a knowledge ID into a readable display name. */
	FText FormatKnowledgeName(FName KnowledgeId) const;

	/** Populate the container with skill entries. */
	void PopulateSkillContainer();

	/** Populate the container with knowledge entries. */
	void PopulateKnowledgeContainer();

	/** Create a simple text entry for a skill (when no widget class set). */
	UTextBlock* CreateSimpleSkillText(const FMOSkillDisplayData& Data);

	/** Create a simple text entry for knowledge. */
	UTextBlock* CreateSimpleKnowledgeText(const FMOSkillDisplayData& Data);

	/** Update the detail panel for selected skill or knowledge. */
	void UpdateDetailPanel();

	/** Update tab button visual states based on current mode. */
	void UpdateTabButtonStates();

	/** Blueprint event when skill list is updated. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Skills|UI")
	void OnSkillListUpdated(int32 SkillCount);

	/** Blueprint event when selected skill changes. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Skills|UI")
	void OnSelectedSkillChanged(const FMOSkillDisplayData& SkillData);

	/** Blueprint event when tab mode changes. Use this to update button visual styles. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Skills|UI")
	void OnTabModeChanged(EMOSkillsPanelMode NewMode);

	// --- Widget Bindings ---

	/** Container for skill entries (ScrollBox or VerticalBox). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> SkillsContainer;

	/** Text shown when no skills match filter. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyListText;

	/** Close button. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> CloseButton;

	/** Tab button to switch to Skills view. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> SkillsTabButton;

	/** Tab button to switch to Knowledge view. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> KnowledgeTabButton;

	// --- Detail Panel Bindings (for selected skill/knowledge) ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailDescriptionText;

	/** Text showing how to increase skill XP. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailHowToIncreaseText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailLevelText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailXPText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> DetailXPBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> DetailIcon;

private:
	UPROPERTY()
	TWeakObjectPtr<UMOSkillsComponent> SkillsComponent;

	UPROPERTY()
	TWeakObjectPtr<UMOKnowledgeComponent> KnowledgeComponent;

	UPROPERTY()
	TArray<TObjectPtr<UMOSkillEntryWidget>> EntryWidgets;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SimpleTextWidgets;

	FName SelectedSkillId = NAME_None;
	FName SelectedKnowledgeId = NAME_None;
	EMOSkillCategory CategoryFilter = EMOSkillCategory::None;
	EMOSkillsPanelMode CurrentDisplayMode = EMOSkillsPanelMode::Skills;
};
