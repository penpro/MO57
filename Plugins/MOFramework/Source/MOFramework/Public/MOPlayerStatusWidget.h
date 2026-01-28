#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOPlayerStatusWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UMOSurvivalStatsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOPlayerStatusRequestCloseSignature);

/**
 * HUD widget that displays player survival stats (health, hunger, thirst, etc.).
 * Binds to MOSurvivalStatsComponent on the player's pawn.
 *
 * Widget Setup:
 * 1. Create WBP_PlayerStatus based on this class
 * 2. Add ProgressBars named: HealthBar, StaminaBar, HungerBar, ThirstBar, EnergyBar
 * 3. Optionally add TextBlocks: HealthText, StaminaText, etc. for numeric display
 * 4. Mark all as "Is Variable"
 */
UCLASS()
class MOFRAMEWORK_API UMOPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOPlayerStatusWidget(const FObjectInitializer& ObjectInitializer);

	/** Initialize with a survival stats component. Call after creating widget. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Status")
	void InitializeStatus(UMOSurvivalStatsComponent* InSurvivalStats);

	/** Manually refresh all stat displays. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Status")
	void RefreshAllStats();

	/** Called when Tab/Escape is pressed to request close. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Status")
	FMOPlayerStatusRequestCloseSignature OnRequestClose;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleStatChanged(FName StatName, float OldValue, float NewValue);

	void UpdateStatDisplay(FName StatName);
	void UpdateAllDisplays();

	UProgressBar* GetProgressBarForStat(FName StatName) const;
	UTextBlock* GetTextBlockForStat(FName StatName) const;

private:
	// Progress bars for each stat
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> StaminaBar;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> HungerBar;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> ThirstBar;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> EnergyBar;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> TemperatureBar;

	// Optional text displays for numeric values
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StaminaText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HungerText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ThirstText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EnergyText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TemperatureText;

	UPROPERTY()
	TWeakObjectPtr<UMOSurvivalStatsComponent> SurvivalStats;

	/** Whether to show percentage or current/max values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(AllowPrivateAccess="true"))
	bool bShowPercentage = true;

	/** Colors for stat bars based on value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(AllowPrivateAccess="true"))
	FLinearColor HealthyColor = FLinearColor(0.2f, 0.8f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(AllowPrivateAccess="true"))
	FLinearColor WarningColor = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(AllowPrivateAccess="true"))
	FLinearColor CriticalColor = FLinearColor(0.9f, 0.1f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(AllowPrivateAccess="true"))
	float WarningThreshold = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(AllowPrivateAccess="true"))
	float CriticalThreshold = 0.25f;
};
