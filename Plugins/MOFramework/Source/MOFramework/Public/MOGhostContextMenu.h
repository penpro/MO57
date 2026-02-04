#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOGhostContextMenu.generated.h"

class AMOBuildableActor;
class UMOInventoryComponent;
class UMOBuildProgressComponent;
class UCheckBox;
class UButton;
class UVerticalBox;
class UTextBlock;
class UProgressBar;

/**
 * Data for displaying a material requirement in the ghost context menu.
 */
USTRUCT(BlueprintType)
struct FMOGhostMaterialEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly)
	int32 Deposited = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Required = 0;

	bool IsComplete() const { return Deposited >= Required; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOGhostMenuRequestCloseSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOGhostMenuCancelledSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOGhostMenuBuildStartedSignature);

/**
 * Simple context menu for interacting with ghost buildings.
 *
 * Flow:
 * 1. Player interacts with ghost -> This menu opens
 * 2. Player selects material sources (checkboxes)
 * 3. Player clicks "Add" to deposit materials from sources
 * 4. Material list shows "Cordage 3/5" style progress
 * 5. Once all materials deposited, "Add" becomes "Build"
 * 6. "Build" starts the construction timer
 * 7. "Cancel" destroys ghost and drops deposited materials
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOGhostContextMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOGhostContextMenu(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize the menu for a specific ghost building.
	 * @param Target - The ghost building to configure
	 * @param BuilderInventory - The player's inventory for material sourcing
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void InitializeForGhost(AMOBuildableActor* Target, UMOInventoryComponent* BuilderInventory);

	/**
	 * Refresh the material list display.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void RefreshMaterialList();

	// ============================================================================
	// ACTIONS
	// ============================================================================

	/**
	 * Attempt to add/deposit materials from selected sources.
	 * Called when Add button is clicked.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void AddMaterials();

	/**
	 * Start the build timer (only available when all materials deposited).
	 * Called when Build button is clicked.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void StartBuild();

	/**
	 * Cancel the build - destroys ghost and drops deposited materials.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void CancelBuild();

	// ============================================================================
	// STATE
	// ============================================================================

	/**
	 * Check if all materials have been deposited.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	bool AreAllMaterialsDeposited() const;

	/**
	 * Check if the build timer is currently active.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	bool IsBuildTimerActive() const;

	/**
	 * Get the current build progress (0.0 - 1.0).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Building|UI")
	float GetBuildProgress() const;

	/**
	 * Get material entries for display.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Building|UI")
	void GetMaterialEntries(TArray<FMOGhostMaterialEntry>& OutEntries) const;

	// ============================================================================
	// DELEGATES
	// ============================================================================

	UPROPERTY(BlueprintAssignable, Category="MO|Building|UI")
	FMOGhostMenuRequestCloseSignature OnRequestClose;

	UPROPERTY(BlueprintAssignable, Category="MO|Building|UI")
	FMOGhostMenuCancelledSignature OnCancelled;

	UPROPERTY(BlueprintAssignable, Category="MO|Building|UI")
	FMOGhostMenuBuildStartedSignature OnBuildStarted;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// ============================================================================
	// WIDGET BINDINGS
	// ============================================================================

	/** Checkbox: Pull from player inventory */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> InventoryCheckbox;

	/** Checkbox: Pull from nearby containers */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> ContainersCheckbox;

	/** Checkbox: Pull from surrounding area (world items) */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UCheckBox> SurroundingCheckbox;

	/** Container for material requirement text entries */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> MaterialListContainer;

	/** Add/Build button - changes based on state */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UButton> AddBuildButton;

	/** Text on the Add/Build button */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> AddBuildButtonText;

	/** Cancel button */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UButton> CancelButton;

	/** Progress bar for build timer */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> BuildProgressBar;

	/** Text showing build time remaining */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> BuildTimeText;

	// ============================================================================
	// BLUEPRINT EVENTS
	// ============================================================================

	/** Called when material list needs to be updated in Blueprint */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Building|UI")
	void OnMaterialListUpdated(const TArray<FMOGhostMaterialEntry>& Materials);

	/** Called when button state changes (Add vs Build) */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Building|UI")
	void OnButtonStateChanged(bool bShowBuildButton);

	/** Called when build progress updates */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Building|UI")
	void OnBuildProgressUpdated(float Progress, float TimeRemaining);

private:
	// ============================================================================
	// HANDLERS
	// ============================================================================

	UFUNCTION()
	void HandleAddBuildClicked();

	UFUNCTION()
	void HandleCancelClicked();

	// ============================================================================
	// STATE
	// ============================================================================

	UPROPERTY()
	TWeakObjectPtr<AMOBuildableActor> TargetBuilding;

	UPROPERTY()
	TWeakObjectPtr<UMOInventoryComponent> BuilderInventory;

	UPROPERTY()
	TWeakObjectPtr<UMOBuildProgressComponent> BuildProgress;

	/** Cached material entries */
	UPROPERTY()
	TArray<FMOGhostMaterialEntry> MaterialEntries;

	/** Dynamically created text widgets for material list */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> MaterialTextWidgets;

	// ============================================================================
	// INTERNAL
	// ============================================================================

	/** Update the Add/Build button state */
	void UpdateButtonState();

	/** Create text widget for a material entry */
	UTextBlock* CreateMaterialTextWidget(const FMOGhostMaterialEntry& Entry);

	/** Update material text widget */
	void UpdateMaterialTextWidget(UTextBlock* Widget, const FMOGhostMaterialEntry& Entry);

	/** Try to gather one unit of a material from sources */
	bool TryGatherMaterial(FName ItemId);

	/** Populate material list container with text widgets */
	void PopulateMaterialList();
};
