#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOHarvestProgressWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UCommonButtonBase;
class UMOKnowledgeComponent;
class UMOSkillsComponent;
class UMOInventoryComponent;
class UInstancedStaticMeshComponent;
struct FMOCraftResult;

/**
 * Widget displaying harvest progress with a countdown timer.
 * Shows action name, progress bar, and time remaining.
 * Works with the MOHarvestSubsystem to execute harvest recipes.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOHarvestProgressWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Called when harvest is completed (either success or cancelled). */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOHarvestCompletedSignature, bool, bCompleted, const FMOCraftResult&, Result);
	UPROPERTY(BlueprintAssignable, Category="MO|Harvest")
	FMOHarvestCompletedSignature OnHarvestCompleted;

	/** Called when player requests to close/cancel the harvest. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOHarvestCancelledSignature);
	UPROPERTY(BlueprintAssignable, Category="MO|Harvest")
	FMOHarvestCancelledSignature OnHarvestCancelled;

	/**
	 * Start a harvest operation with progress display.
	 * @param InISMComponent The ISM/HISM component to harvest from
	 * @param InInstanceIndex The instance index to harvest
	 * @param InRecipeId The harvest recipe to execute
	 * @param InActionDisplayName Display name for the action (e.g., "Harvesting Bark")
	 * @param InInventory Player's inventory (for time calculation and output)
	 * @param InSkills Player's skills (for XP)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	void StartHarvest(
		UInstancedStaticMeshComponent* InISMComponent,
		int32 InInstanceIndex,
		FName InRecipeId,
		const FText& InActionDisplayName,
		UMOInventoryComponent* InInventory,
		UMOSkillsComponent* InSkills
	);

	/** Cancel the harvest in progress. */
	UFUNCTION(BlueprintCallable, Category="MO|Harvest")
	void CancelHarvest();

	/** Check if harvest is currently in progress. */
	UFUNCTION(BlueprintPure, Category="MO|Harvest")
	bool IsHarvestInProgress() const { return bIsHarvesting; }

	/** Get current progress (0-1). */
	UFUNCTION(BlueprintPure, Category="MO|Harvest")
	float GetProgress() const { return CurrentProgress; }

	/** Get the recipe being harvested. */
	UFUNCTION(BlueprintPure, Category="MO|Harvest")
	FName GetCurrentRecipeId() const { return RecipeId; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Update the visual display. Override in BP for custom visuals. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|Harvest")
	void UpdateDisplay(float Progress, float TimeRemaining, const FText& ActionName);

	/** Called when harvest completes successfully. Override in BP for custom behavior. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|Harvest")
	void OnHarvestSuccess(const FMOCraftResult& Result);

	// --- Widget Bindings ---

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="MO|Harvest")
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="MO|Harvest")
	TObjectPtr<UTextBlock> ActionNameText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="MO|Harvest")
	TObjectPtr<UTextBlock> TimeRemainingText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="MO|Harvest")
	TObjectPtr<UTextBlock> InstructionText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="MO|Harvest")
	TObjectPtr<UCommonButtonBase> CancelButton;

private:
	UFUNCTION()
	void HandleCancelClicked();

	/** Timer callback for real-time progress tracking. */
	UFUNCTION()
	void TickHarvest();

	void CompleteHarvest();

	// Current harvest state
	FName RecipeId = NAME_None;
	FText ActionDisplayName;
	TWeakObjectPtr<UMOInventoryComponent> InventoryComponent;
	TWeakObjectPtr<UMOSkillsComponent> SkillsComponent;

	float HarvestDuration = 5.0f;
	float ElapsedTime = 0.0f;
	float CurrentProgress = 0.0f;
	bool bIsHarvesting = false;

	/** Wall-clock time when harvest started (for real-time tracking). */
	double HarvestStartTime = 0.0;

	/** Timer handle for progress updates. */
	FTimerHandle HarvestTimerHandle;
};
